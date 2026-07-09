#include "app_tasks.h"
#include "global_types.h"
#include "app_config.h"
#include "comms.h"
#include "sd_card_app.h"
#include "esp_log.h"
#include <stdio.h>

static const char *TAG = "T6_COMMS";

extern QueueHandle_t Q_Comms;

void task_comms_entry(void *pvParameters) {
    ProcessedData_t data;
    char payload_buffer[256];

    while (1) {
        if (xQueueReceive(Q_Comms, &data, portMAX_DELAY) == pdPASS) {
            snprintf(payload_buffer, sizeof(payload_buffer), 
                     "{\"timestamp\":\"%04d-%02d-%02d %02d:%02d:%02d\","
                     "\"aqi\":%d,\"temp\":%.1f,\"hum\":%.1f,\"pm25\":%d,\"pm10\":%d,\"co2\":%.0f}",
                     data.timestamp.year, data.timestamp.month, data.timestamp.day,
                     data.timestamp.hour, data.timestamp.minute, data.timestamp.second,
                     data.aqi_total, data.temperature, data.humidity,
                     data.pm2_5, data.pm10, data.co2_ppm);

            EventBits_t bits = xEventGroupGetBits(SystemEventGroup);
            int qos = (bits & BIT_ALERT) ? 1 : 0;

            if (wifi_manager_is_connected() && mqtt_manager_is_connected()) {
                if (mqtt_manager_publish(MQTT_TOPIC_DATA, payload_buffer, qos)) {
                    ESP_LOGD(TAG, "Đã gửi MQTT thành công.");
                } else {
                    ESP_LOGW(TAG, "Không gửi được MQTT, dữ liệu vẫn được lưu bình thường.");
                }
            } else {
                ESP_LOGW(TAG, "Mất kết nối mạng, dữ liệu sẽ được lưu bình thường.");
            }
        }
    }
}