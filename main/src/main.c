/**
 * @file main.c
 * @brief Trạm Quan Trắc Chất Lượng Không Khí Đa Thông Số
 *        Khởi tạo hệ thống, ngoại vi, hàng đợi, semaphore,
 *        event group và tạo toàn bộ 7 FreeRTOS Task.
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/spi_master.h"
#include "driver/uart.h"
#include "driver/adc.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"

#include "config.h"
#include "bme680_driver.h"
#include "pms5003_driver.h"
#include "mq135_driver.h"
#include "ds3231_driver.h"
#include "oled_driver.h"
#include "sd_storage.h"
#include "wifi_manager.h"
#include "mqtt_manager.h"
#include "calibration.h"

/* Task function declarations */
void alert_task(void *pv);
void sensor_task(void *pv);
void process_task(void *pv);
void display_task(void *pv);
void storage_task(void *pv);
void comms_task(void *pv);
void power_task(void *pv);

/* ============================================================
 * GLOBAL SHARED HANDLES
 * ============================================================ */
QueueHandle_t      g_q1_sensor;
QueueHandle_t      g_q2_result;
EventGroupHandle_t g_event_group;
SemaphoreHandle_t  g_i2c_mutex;
volatile uint32_t  g_measure_interval_ms = MEASURE_INTERVAL_NORMAL_MS;

static const char *TAG = "MAIN";

/* ============================================================
 * PERIPHERAL INITIALISATION
 * ============================================================ */

static esp_err_t init_i2c(void)
{
    i2c_config_t conf = {
        .mode             = I2C_MODE_MASTER,
        .sda_io_num       = PIN_I2C_SDA,
        .scl_io_num       = PIN_I2C_SCL,
        .sda_pullup_en    = GPIO_PULLUP_ENABLE,
        .scl_pullup_en    = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_FREQ_HZ,
    };
    ESP_ERROR_CHECK(i2c_param_config(I2C_PORT, &conf));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_PORT, I2C_MODE_MASTER, 0, 0, 0));
    ESP_LOGI(TAG, "I2C bus initialised (SDA=%d, SCL=%d)", PIN_I2C_SDA, PIN_I2C_SCL);
    return ESP_OK;
}

static esp_err_t init_uart(void)
{
    uart_config_t uart_conf = {
        .baud_rate  = UART_BAUD,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
    };
    ESP_ERROR_CHECK(uart_param_config(UART_PORT, &uart_conf));
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT, PIN_UART_TX, PIN_UART_RX,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(UART_PORT, 256, 0, 0, NULL, 0));
    ESP_LOGI(TAG, "UART%d initialised (TX=%d RX=%d @ %d baud)",
             UART_PORT, PIN_UART_TX, PIN_UART_RX, UART_BAUD);
    return ESP_OK;
}

static esp_err_t init_adc(void)
{
    adc1_config_width(ADC_WIDTH);
    adc1_config_channel_atten(ADC_CHANNEL, ADC_ATTEN);
    ESP_LOGI(TAG, "ADC1 channel %d initialised (GPIO%d)", ADC_CHANNEL, PIN_MQ135_AO);
    return ESP_OK;
}

static esp_err_t init_gpio(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << PIN_BUZZER),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));
    gpio_set_level(PIN_BUZZER, 0);
    ESP_LOGI(TAG, "Buzzer GPIO%d configured", PIN_BUZZER);
    return ESP_OK;
}

/* ============================================================
 * APP MAIN
 * ============================================================ */

void app_main(void)
{
    ESP_LOGI(TAG, "=== Air Quality Monitor starting ===");

    /* ----- NVS Flash (needed by WiFi, MQTT, calibration) ----- */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* ----- Event loop (WiFi) ----- */
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* ----- Peripheral init ----- */
    ESP_ERROR_CHECK(init_gpio());
    ESP_ERROR_CHECK(init_i2c());
    ESP_ERROR_CHECK(init_uart());
    ESP_ERROR_CHECK(init_adc());

    /* ----- Device drivers ----- */
    ESP_ERROR_CHECK(bme680_init());
    ESP_ERROR_CHECK(ds3231_init());
    ESP_ERROR_CHECK(oled_init());
    pms5003_init();
    mq135_init();

    /* ----- SD Card & calibration load ----- */
    ESP_ERROR_CHECK(sd_init());
    cal_load_from_nvs();          /* load gain/offset from flash */

    /* ----- WiFi + MQTT ----- */
    wifi_init();
    mqtt_init();

    /* ----- FreeRTOS shared objects ----- */
    g_q1_sensor   = xQueueCreate(SENSOR_QUEUE_DEPTH,  sizeof(sensor_data_t));
    g_q2_result   = xQueueCreate(RESULT_QUEUE_DEPTH,  sizeof(result_data_t));
    g_event_group = xEventGroupCreate();
    g_i2c_mutex   = xSemaphoreCreateMutex();

    configASSERT(g_q1_sensor);
    configASSERT(g_q2_result);
    configASSERT(g_event_group);
    configASSERT(g_i2c_mutex);

    /* ----- Create tasks ----- */
    xTaskCreate(alert_task,   "T1_Alert",   STACK_ALERT,   NULL, TASK_PRIO_ALERT,   NULL);
    xTaskCreate(sensor_task,  "T2_Sensor",  STACK_SENSOR,  NULL, TASK_PRIO_SENSOR,  NULL);
    xTaskCreate(process_task, "T3_Process", STACK_PROCESS, NULL, TASK_PRIO_PROCESS, NULL);
    xTaskCreate(display_task, "T4_Display", STACK_DISPLAY, NULL, TASK_PRIO_DISPLAY, NULL);
    xTaskCreate(storage_task, "T5_Storage", STACK_STORAGE, NULL, TASK_PRIO_STORAGE, NULL);
    xTaskCreate(comms_task,   "T6_Comms",   STACK_COMMS,   NULL, TASK_PRIO_COMMS,   NULL);
    xTaskCreate(power_task,   "T7_Power",   STACK_POWER,   NULL, TASK_PRIO_POWER,   NULL);

    ESP_LOGI(TAG, "All tasks created. System running.");
}