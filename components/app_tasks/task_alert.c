#include "app_tasks.h"
#include "global_types.h"
#include "app_config.h"
#include "esp_log.h"
#include "sd_card_app.h"
#include "driver/gpio.h"
#include "comms.h"
#include <stdio.h>
#include "esp_timer.h"


#define BUZZER_PIN GPIO_NUM_4
static const char *TAG = "T1_ALERT";
static const char *MQTT_ALERT_OUTBOX = "/sdcard/mqalt.log";

void task_alert_entry(void *pvParameters) {
    // Cấu hình chân D4 cho Buzzer
    gpio_reset_pin(BUZZER_PIN);
    gpio_set_direction(BUZZER_PIN, GPIO_MODE_OUTPUT);

    ProcessedData_t data;
    char event_buffer[128];
    char mqtt_payload[128];
    char timestamp[32];

    while (1) {
        // Chờ tín hiệu BIT_ALERT được set (blocking)
        EventBits_t bits = xEventGroupWaitBits(
            SystemEventGroup,
            BIT_ALERT,
            pdFALSE,  // don't clear here; we'll clear after processing
            pdFALSE,  // wait for any bit
            portMAX_DELAY
        );

        if (bits & BIT_ALERT) {
            ESP_LOGW(TAG, "Phát hiện vượt ngưỡng! Kích hoạt cảnh báo.");

            EventBits_t sd_bits = xEventGroupGetBits(SystemEventGroup);
            if (xQueueReceive(Q_Alert, &data, pdMS_TO_TICKS(1000)) == pdPASS) {
                // Đo thời gian và in log
                uint64_t alert_time_us = esp_timer_get_time();
                uint32_t total_time_us = (uint32_t)(alert_time_us - data.start_time_us);
                ESP_LOGI("PERFORMANCE", "Thời gian từ lúc đo tới lúc phát cảnh báo: %lu us", total_time_us);

                // Bật còi cảnh báo
                gpio_set_level(BUZZER_PIN, 1);

                snprintf(event_buffer, sizeof(event_buffer),
                         "%04d-%02d-%02d %02d:%02d:%02d - CẢNH BÁO: Vượt ngưỡng AQI an toàn!",
                         data.timestamp.year, data.timestamp.month, data.timestamp.day,
                         data.timestamp.hour, data.timestamp.minute, data.timestamp.second);
                if (sd_bits & BIT_SD_CARD_READY) {
                    sd_card_write_line("/sdcard/event.log", event_buffer);
                    ESP_LOGI(TAG, "Đã ghi nhật ký sự kiện vào thẻ SD");
                } else {
                    ESP_LOGW(TAG, "SD không sẵn sàng, bỏ qua ghi nhật ký sự kiện");
                }
            }

            snprintf(timestamp, sizeof(timestamp),
                     "%04d-%02d-%02d %02d:%02d:%02d",
                     data.timestamp.year, data.timestamp.month, data.timestamp.day,
                     data.timestamp.hour, data.timestamp.minute, data.timestamp.second);

            snprintf(mqtt_payload, sizeof(mqtt_payload),
                     "{\"timestamp\": \"%s\", \"alert\": \"AQI Threshold Exceeded\"}",
                     timestamp);

            if (mqtt_manager_is_connected()) {
                if (!mqtt_manager_publish(MQTT_TOPIC_ALERT, mqtt_payload, 1)) {
                    ESP_LOGW(TAG, "Không gửi MQTT alert, lưu vào outbox.");
                    if (sd_bits & BIT_SD_CARD_READY) {
                        sd_card_write_line(MQTT_ALERT_OUTBOX, mqtt_payload);
                    } else {
                        ESP_LOGW(TAG, "SD không sẵn sàng, bỏ qua lưu alert outbox");
                    }
                }
            } else {
                ESP_LOGW(TAG, "MQTT offline, lưu alert vào outbox.");
                if (sd_bits & BIT_SD_CARD_READY) {
                    sd_card_write_line(MQTT_ALERT_OUTBOX, mqtt_payload);
                } else {
                    ESP_LOGW(TAG, "SD không sẵn sàng, bỏ qua lưu alert outbox");
                }
            }

            // Tắt còi sau 1 giây
            vTaskDelay(pdMS_TO_TICKS(200));
            gpio_set_level(BUZZER_PIN, 0);

            // Xóa cờ alert sau khi xử lý
            xEventGroupClearBits(SystemEventGroup, BIT_ALERT);

            // Tránh lặp quá nhanh
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}
