#include "calibration.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include <math.h>
#include <string.h>

static const char *TAG = "CAL";

/* Default calibration (identity): corrected = raw * 1.0 + 0.0 */
static cal_params_t g_cal = {
    .temp_offset  = 0.0f, .temp_gain   = 1.0f,
    .hum_offset   = 0.0f, .hum_gain    = 1.0f,
    .co2_offset   = 0.0f, .co2_gain    = 1.0f,
    .pm25_offset  = 0.0f, .pm25_gain   = 1.0f,
    .pm10_offset  = 0.0f, .pm10_gain   = 1.0f,
    .last_cal_epoch = 0,
};

/* Snapshot used by self-check drift detection */
static float  s_ref_temp = 0.0f;
static float  s_ref_hum  = 0.0f;
static bool   s_ref_valid = false;

/* ---------------------------------------------------------------- */

static esp_err_t nvs_read_float(nvs_handle_t h, const char *key, float *out, float def)
{
    uint32_t raw = 0;
    esp_err_t r = nvs_get_u32(h, key, &raw);
    if (r == ESP_OK) {
        memcpy(out, &raw, sizeof(float));
    } else {
        *out = def;
    }
    return r;
}

static void nvs_write_float(nvs_handle_t h, const char *key, float val)
{
    uint32_t raw = 0;
    memcpy(&raw, &val, sizeof(float));
    nvs_set_u32(h, key, raw);
}

/* ---------------------------------------------------------------- */

void cal_load_from_nvs(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &h);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS open failed (%s), using defaults", esp_err_to_name(err));
        return;
    }

    nvs_read_float(h, NVS_KEY_TEMP_OFFSET,  &g_cal.temp_offset,  0.0f);
    nvs_read_float(h, NVS_KEY_TEMP_GAIN,    &g_cal.temp_gain,    1.0f);
    nvs_read_float(h, NVS_KEY_HUM_OFFSET,   &g_cal.hum_offset,   0.0f);
    nvs_read_float(h, NVS_KEY_HUM_GAIN,     &g_cal.hum_gain,     1.0f);
    nvs_read_float(h, NVS_KEY_CO2_OFFSET,   &g_cal.co2_offset,   0.0f);
    nvs_read_float(h, NVS_KEY_CO2_GAIN,     &g_cal.co2_gain,     1.0f);
    nvs_read_float(h, NVS_KEY_PM25_OFFSET,  &g_cal.pm25_offset,  0.0f);
    nvs_read_float(h, NVS_KEY_PM25_GAIN,    &g_cal.pm25_gain,    1.0f);
    nvs_read_float(h, NVS_KEY_PM10_OFFSET,  &g_cal.pm10_offset,  0.0f);
    nvs_read_float(h, NVS_KEY_PM10_GAIN,    &g_cal.pm10_gain,    1.0f);
    nvs_get_u32(h, NVS_KEY_LAST_CAL_EPOCH, &g_cal.last_cal_epoch);

    nvs_close(h);
    ESP_LOGI(TAG, "Calibration loaded from NVS (last_cal epoch=%lu)", (unsigned long)g_cal.last_cal_epoch);
}

void cal_save_to_nvs(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS open for write failed: %s", esp_err_to_name(err));
        return;
    }

    nvs_write_float(h, NVS_KEY_TEMP_OFFSET,  g_cal.temp_offset);
    nvs_write_float(h, NVS_KEY_TEMP_GAIN,    g_cal.temp_gain);
    nvs_write_float(h, NVS_KEY_HUM_OFFSET,   g_cal.hum_offset);
    nvs_write_float(h, NVS_KEY_HUM_GAIN,     g_cal.hum_gain);
    nvs_write_float(h, NVS_KEY_CO2_OFFSET,   g_cal.co2_offset);
    nvs_write_float(h, NVS_KEY_CO2_GAIN,     g_cal.co2_gain);
    nvs_write_float(h, NVS_KEY_PM25_OFFSET,  g_cal.pm25_offset);
    nvs_write_float(h, NVS_KEY_PM25_GAIN,    g_cal.pm25_gain);
    nvs_write_float(h, NVS_KEY_PM10_OFFSET,  g_cal.pm10_offset);
    nvs_write_float(h, NVS_KEY_PM10_GAIN,    g_cal.pm10_gain);
    nvs_set_u32(h, NVS_KEY_LAST_CAL_EPOCH, g_cal.last_cal_epoch);

    nvs_commit(h);
    nvs_close(h);
    ESP_LOGI(TAG, "Calibration saved to NVS");
}

float cal_apply(float raw, float gain, float offset)
{
    return raw * gain + offset;
}

const cal_params_t *cal_get(void)
{
    return &g_cal;
}

void cal_set(const cal_params_t *new_params)
{
    g_cal = *new_params;
    cal_save_to_nvs();
    s_ref_valid = false;   /* reset drift reference */
}

bool cal_self_check(float raw_temp, float raw_hum, uint32_t now_epoch)
{
    /* Time-based check: > CAL_PERIOD_DAYS since last calibration */
    if (g_cal.last_cal_epoch > 0) {
        uint32_t days = (now_epoch - g_cal.last_cal_epoch) / 86400u;
        if (days >= CAL_PERIOD_DAYS) {
            ESP_LOGW(TAG, "Self-check: %lu days since last calibration", (unsigned long)days);
            return true;
        }
    }

    /* Drift-based check against reference snapshot */
    if (!s_ref_valid) {
        s_ref_temp  = raw_temp;
        s_ref_hum   = raw_hum;
        s_ref_valid = true;
        return false;
    }

    float drift_t = (s_ref_temp != 0.0f)
                  ? fabsf((raw_temp - s_ref_temp) / s_ref_temp) * 100.0f
                  : 0.0f;
    float drift_h = (s_ref_hum != 0.0f)
                  ? fabsf((raw_hum - s_ref_hum) / s_ref_hum) * 100.0f
                  : 0.0f;

    if (drift_t > CAL_DRIFT_THRESHOLD_PCT || drift_h > CAL_DRIFT_THRESHOLD_PCT) {
        ESP_LOGW(TAG, "Self-check drift: T=%.1f%% H=%.1f%%", drift_t, drift_h);
        return true;
    }
    return false;
}