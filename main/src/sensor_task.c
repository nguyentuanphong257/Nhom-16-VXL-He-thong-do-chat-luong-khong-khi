/**
 * @file sensor_task.c
 * @brief T2_Sensor (Priority 5)
 *
 * Kích hoạt: vTaskDelay theo g_measure_interval_ms (1s hoặc 10s khi nhàn rỗi).
 * Deadline: < 100ms toàn bộ chu kỳ đọc.
 *
 * Luồng:
 *   1. Đọc timestamp từ DS3231.
 *   2. Đọc BME680, PMS5003, MQ-135.
 *   3. Validate range + lọc checksum PMS5003.
 *   4. Áp dụng bộ lọc outlier (EMA / spike-hold) cho từng kênh.
 *   5. Gửi sensor_data_t vào Q1_SensorQueue.
 */

#include "sensor_task.h"
#include "config.h"
#include "ds3231_driver.h"
#include "bme680_driver.h"
#include "pms5003_driver.h"
#include "mq135_driver.h"
#include "outlier_filter.h"
#include "sd_storage.h"
#include "esp_log.h"
#include <string.h>

#define TAG "T2_SENSOR"

/* ---- Per-channel outlier filters ---- */
static outlier_filter_t s_flt_temp;
static outlier_filter_t s_flt_hum;
static outlier_filter_t s_flt_pres;
static outlier_filter_t s_flt_co2;
static outlier_filter_t s_flt_pm25;
static outlier_filter_t s_flt_pm10;

/* ---- Range validation ---- */
static bool validate_bme680(const bme680_data_t *d)
{
    return (d->temperature >= TEMP_MIN_C     && d->temperature <= TEMP_MAX_C     &&
            d->humidity    >= HUMIDITY_MIN_PCT && d->humidity    <= HUMIDITY_MAX_PCT &&
            d->pressure    >= PRESSURE_MIN_PA && d->pressure    <= PRESSURE_MAX_PA);
}

static bool validate_pms(const pms5003_data_t *d)
{
    return (d->pm2_5 >= PM_MIN && d->pm2_5 <= PM_MAX &&
            d->pm10  >= PM_MIN && d->pm10  <= PM_MAX);
}

static bool validate_co2(float ppm)
{
    return (ppm >= CO2_MIN_PPM && ppm <= CO2_MAX_PPM);
}

/* ---- Error bitmasks ---- */
#define ERR_BME680  (1 << 0)
#define ERR_PMS5003 (1 << 1)
#define ERR_MQ135   (1 << 2)

void sensor_task(void *pv)
{
    /* Initialise outlier filters with measured δmax values */
    outlier_filter_init(&s_flt_temp,  0.5f,  5.0f);    /* °C   */
    outlier_filter_init(&s_flt_hum,   0.5f, 10.0f);    /* %RH  */
    outlier_filter_init(&s_flt_pres,  0.5f, 500.0f);   /* Pa   */
    outlier_filter_init(&s_flt_co2,   0.5f, 100.0f);   /* ppm  */
    outlier_filter_init(&s_flt_pm25,  0.5f, 50.0f);    /* µg   */
    outlier_filter_init(&s_flt_pm10,  0.5f, 80.0f);    /* µg   */

    ESP_LOGI(TAG, "T2_Sensor started");

    while (1) {
        TickType_t cycle_start = xTaskGetTickCount();

        sensor_data_t sd;
        memset(&sd, 0, sizeof(sd));
        sd.valid = true;

        /* 1. Timestamp from DS3231 */
        ds3231_time_t rtc_time;
        if (ds3231_get_time(&rtc_time) == ESP_OK) {
            sd.epoch_s = ds3231_to_epoch(&rtc_time);
            ds3231_format(&rtc_time, sd.timestamp_str);
        } else {
            ESP_LOGW(TAG, "DS3231 read failed");
            snprintf(sd.timestamp_str, sizeof(sd.timestamp_str), "0000-00-00 00:00:00");
        }

        /* 2. BME680 */
        bme680_data_t bme;
        if (bme680_read(&bme) == ESP_OK && validate_bme680(&bme)) {
            sd.temperature_raw    = outlier_filter_update(&s_flt_temp, bme.temperature);
            sd.humidity_raw       = outlier_filter_update(&s_flt_hum,  bme.humidity);
            sd.pressure_raw       = outlier_filter_update(&s_flt_pres, bme.pressure);
            sd.gas_resistance_ohm = bme.gas_resistance;
        } else {
            sd.error_mask |= ERR_BME680;
            sd.valid = false;
            ESP_LOGE(TAG, "BME680 error or out of range");
        }

        /* 3. PMS5003 */
        pms5003_data_t pms;
        if (pms5003_read(&pms) == ESP_OK && validate_pms(&pms)) {
            sd.pm1_0  = pms.pm1_0;
            sd.pm2_5  = (uint16_t)outlier_filter_update(&s_flt_pm25, (float)pms.pm2_5);
            sd.pm10   = (uint16_t)outlier_filter_update(&s_flt_pm10, (float)pms.pm10);
        } else {
            sd.error_mask |= ERR_PMS5003;
            sd.valid = false;
            ESP_LOGE(TAG, "PMS5003 error or out of range");
        }

        /* 4. MQ-135 */
        float co2_ppm = 0.0f;
        if (mq135_read_ppm(&co2_ppm) == ESP_OK && validate_co2(co2_ppm)) {
            sd.co2_ppm_raw = outlier_filter_update(&s_flt_co2, co2_ppm);
        } else {
            sd.error_mask |= ERR_MQ135;
            /* MQ-135 error is non-fatal; still send data */
            sd.co2_ppm_raw = 0.0f;
            ESP_LOGW(TAG, "MQ-135 error or out of range");
        }

        /* 5. Log errors to SD if any */
        if (sd.error_mask) {
            char msg[64];
            snprintf(msg, sizeof(msg), "ts=%s err_mask=0x%02X",
                     sd.timestamp_str, sd.error_mask);
            sd_write_log("SENSOR_ERR", msg);
        }

        /* 6. Enqueue (non-blocking; drop if queue full) */
        if (xQueueSend(g_q1_sensor, &sd, 0) != pdTRUE) {
            ESP_LOGW(TAG, "Q1 full, dropping sample");
        }

        /* 7. Wait for next cycle */
        uint32_t interval = g_measure_interval_ms;
        TickType_t elapsed = xTaskGetTickCount() - cycle_start;
        TickType_t wait    = pdMS_TO_TICKS(interval);
        if (elapsed < wait) {
            vTaskDelay(wait - elapsed);
        }
    }
}