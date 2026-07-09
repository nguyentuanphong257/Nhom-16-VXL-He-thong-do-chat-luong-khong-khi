#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"

/* ============================================================
 * HARDWARE PIN DEFINITIONS
 * ============================================================ */
/* I2C Bus (BME680, DS3231, OLED) */
#define PIN_I2C_SDA             21
#define PIN_I2C_SCL             22
#define I2C_PORT                I2C_NUM_0
#define I2C_FREQ_HZ             400000

/* SPI Bus (MicroSD) */
#define PIN_SPI_CS              5
#define PIN_SPI_SCK             18
#define PIN_SPI_MOSI            23
#define PIN_SPI_MISO            19

/* UART2 (PMS5003) */
#define PIN_UART_TX             17
#define PIN_UART_RX             16
#define UART_PORT               UART_NUM_2
#define UART_BAUD               9600

/* ADC (MQ-135) */
#define PIN_MQ135_AO            25
#define ADC_CHANNEL             ADC1_CHANNEL_8   /* GPIO25 = ADC1_CH8 */
#define ADC_ATTEN               ADC_ATTEN_DB_11
#define ADC_WIDTH               ADC_WIDTH_BIT_12

/* GPIO */
#define PIN_BUZZER              4

/* ============================================================
 * I2C DEVICE ADDRESSES
 * ============================================================ */
#define BME680_I2C_ADDR         0x77
#define DS3231_I2C_ADDR         0x68
#define OLED_I2C_ADDR           0x3C

/* ============================================================
 * TASK PRIORITIES (higher number = higher priority)
 * ============================================================ */
#define TASK_PRIO_ALERT         6
#define TASK_PRIO_SENSOR        5
#define TASK_PRIO_PROCESS       4
#define TASK_PRIO_DISPLAY       3
#define TASK_PRIO_STORAGE       2
#define TASK_PRIO_COMMS         2
#define TASK_PRIO_POWER         1

/* ============================================================
 * TASK STACK SIZES (bytes)
 * ============================================================ */
#define STACK_ALERT             3072
#define STACK_SENSOR            4096
#define STACK_PROCESS           4096
#define STACK_DISPLAY           3072
#define STACK_STORAGE           4096
#define STACK_COMMS             6144
#define STACK_POWER             2048

/* ============================================================
 * EVENT GROUP BITS
 * ============================================================ */
#define BIT_ALERT               (1 << 0)
#define BIT_IDLE                (1 << 1)
#define BIT_CAL_REQ             (1 << 2)
#define BIT_WIFI_CONNECTED      (1 << 3)
#define BIT_MQTT_CONNECTED      (1 << 4)

/* ============================================================
 * TIMING PARAMETERS
 * ============================================================ */
#define MEASURE_INTERVAL_NORMAL_MS    1000
#define MEASURE_INTERVAL_IDLE_MS      10000
#define IDLE_AQI_THRESHOLD            50
#define IDLE_DURATION_MS              (10 * 60 * 1000)  /* 10 minutes */
#define CAL_DRIFT_THRESHOLD_PCT       10
#define CAL_PERIOD_DAYS               30
#define ALERT_DEADLINE_MS             3000
#define RECONNECT_TIMEOUT_MS          30000

/* ============================================================
 * SENSOR VALID RANGES
 * ============================================================ */
#define TEMP_MIN_C              (-40.0f)
#define TEMP_MAX_C              (85.0f)
#define PRESSURE_MIN_PA         (30000.0f)
#define PRESSURE_MAX_PA         (110000.0f)
#define HUMIDITY_MIN_PCT        (0.0f)
#define HUMIDITY_MAX_PCT        (100.0f)
#define PM_MIN                  (0.0f)
#define PM_MAX                  (1000.0f)
#define CO2_MIN_PPM             (10.0f)
#define CO2_MAX_PPM             (1000.0f)

/* ============================================================
 * ALERT THRESHOLDS
 * ============================================================ */
#define ALERT_AQI_THRESHOLD     100
#define ALERT_CO2_PPM           500
#define ALERT_PM25_UG_M3        35.4f

/* ============================================================
 * QUEUE DEPTHS
 * ============================================================ */
#define SENSOR_QUEUE_DEPTH      4
#define RESULT_QUEUE_DEPTH      4

/* ============================================================
 * MQTT / WiFi CONFIG
 * ============================================================ */
#define WIFI_SSID               "YOUR_WIFI_SSID"
#define WIFI_PASSWORD           "YOUR_WIFI_PASSWORD"
#define MQTT_BROKER_URI         "mqtt://broker.hivemq.com:1883"
#define MQTT_CLIENT_ID          "esp32_aqm_001"
#define MQTT_TOPIC_DATA         "aqm/001/data"
#define MQTT_TOPIC_ALERT        "aqm/001/alert"
#define MQTT_TOPIC_STATUS       "aqm/001/status"
#define MQTT_TOPIC_CMD          "aqm/001/cmd"

/* ============================================================
 * SD CARD
 * ============================================================ */
#define SD_MOUNT_POINT          "/sdcard"
#define SD_DATA_FILE            "/sdcard/data.csv"
#define SD_LOG_FILE             "/sdcard/events.log"
#define SD_BUFFER_FILE          "/sdcard/offline_buffer.csv"
#define SD_CAL_FILE             "/sdcard/calibration.json"

/* ============================================================
 * NVS KEYS
 * ============================================================ */
#define NVS_NAMESPACE           "aqm_cal"
#define NVS_KEY_TEMP_OFFSET     "t_off"
#define NVS_KEY_TEMP_GAIN       "t_gain"
#define NVS_KEY_HUM_OFFSET      "h_off"
#define NVS_KEY_HUM_GAIN        "h_gain"
#define NVS_KEY_CO2_OFFSET      "co2_off"
#define NVS_KEY_CO2_GAIN        "co2_gain"
#define NVS_KEY_PM25_OFFSET     "pm25_off"
#define NVS_KEY_PM25_GAIN       "pm25_gain"
#define NVS_KEY_PM10_OFFSET     "pm10_off"
#define NVS_KEY_PM10_GAIN       "pm10_gain"
#define NVS_KEY_LAST_CAL_EPOCH  "last_cal"

/* ============================================================
 * DATA STRUCTURES
 * ============================================================ */

/** Raw sensor readings (output of T2_Sensor) */
typedef struct {
    /* Timestamp */
    uint32_t epoch_s;
    char     timestamp_str[20];   /* "YYYY-MM-DD HH:MM:SS" */

    /* BME680 */
    float    temperature_raw;     /* °C */
    float    humidity_raw;        /* %RH */
    float    pressure_raw;        /* Pa */
    float    gas_resistance_ohm;  /* Ω */

    /* PMS5003 */
    uint16_t pm1_0;               /* µg/m³ */
    uint16_t pm2_5;               /* µg/m³ */
    uint16_t pm10;                /* µg/m³ */

    /* MQ-135 */
    float    co2_ppm_raw;         /* ppm */

    bool     valid;               /* data passed validation */
    uint8_t  error_mask;          /* bitmask of sensor errors */
} sensor_data_t;

/** Processed / calibrated results (output of T3_Process, fed to T4/T5/T6) */
typedef struct {
    uint32_t epoch_s;
    char     timestamp_str[20];

    /* Calibrated values */
    float    temperature;         /* °C */
    float    humidity;            /* %RH */
    float    pressure_hpa;        /* hPa */
    float    co2_ppm;             /* ppm */
    uint16_t pm2_5;               /* µg/m³ */
    uint16_t pm10;                /* µg/m³ */

    /* Computed indices */
    uint16_t aqi;
    uint8_t  aqi_category;        /* 0=Good … 5=Hazardous */
    char     aqi_label[16];

    bool     alert_active;
    uint8_t  error_mask;
} result_data_t;

/** Calibration parameters (per-channel gain + offset) */
typedef struct {
    float temp_offset;   float temp_gain;
    float hum_offset;    float hum_gain;
    float co2_offset;    float co2_gain;
    float pm25_offset;   float pm25_gain;
    float pm10_offset;   float pm10_gain;
    uint32_t last_cal_epoch;
} cal_params_t;

/* ============================================================
 * GLOBAL SHARED HANDLES (defined in main.c)
 * ============================================================ */
extern QueueHandle_t     g_q1_sensor;
extern QueueHandle_t     g_q2_result;
extern EventGroupHandle_t g_event_group;
extern SemaphoreHandle_t g_i2c_mutex;
extern volatile uint32_t g_measure_interval_ms;