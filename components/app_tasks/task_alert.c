#include "app_tasks.h"
#include "global_types.h"
#include "esp_log.h"
#include "sd_card_app.h"
#include "driver/gpio.h"
#include "comms.h"

#define BUZZER_PIN GPIO_NUM_4
static const char *TAG = "T1_ALERT";

void task_alert_entry(void *pvParameters) {
    // Cấu hình chân D4 cho Buzzer
    gpio_reset_pin(BUZZER_PIN);
    gpio_set_direction(BUZZER_PIN, GPIO_MODE_OUTPUT);

    ProcessedData_t data;
    char event_buffer[128];

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

            // Bật còi cảnh báo (tùy muốn)
            // gpio_set_level(BUZZER_PIN, 1);
            // vTaskDelay(pdMS_TO_TICKS(1000));
            // gpio_set_level(BUZZER_PIN, 0);

            // Lấy dữ liệu kèm theo từ queue alert (gửi bởi task_process)
            if (xQueueReceive(Q_Alert, &data, pdMS_TO_TICKS(1000)) == pdPASS) {
                snprintf(event_buffer, sizeof(event_buffer),
                         "%04d-%02d-%02d %02d:%02d:%02d - CẢNH BÁO: Vượt ngưỡng AQI an toàn!",
                         data.timestamp.year, data.timestamp.month, data.timestamp.day,
                         data.timestamp.hour, data.timestamp.minute, data.timestamp.second);
                sd_card_write_line("/sdcard/event.log", event_buffer);
                ESP_LOGI(TAG, "Đã ghi nhật ký sự kiện vào thẻ SD");
            } else {
                snprintf(event_buffer, sizeof(event_buffer),
                         "0000-00-00 00:00:00 - CẢNH BÁO: Vượt ngưỡng AQI (dữ liệu không có)");
                sd_card_write_line("/sdcard/event.log", event_buffer);
                ESP_LOGI(TAG, "Đã ghi nhật ký sự kiện (không có dữ liệu kèm theo)");
            }

            // Đẩy MQTT sự kiện vượt ngưỡng lên Webserver
            if (mqtt_manager_is_connected()) {
                mqtt_manager_publish(MQTT_TOPIC_ALERT, "{\"alert\": \"AQI Threshold Exceeded\"}", 1);
            }

            // Xóa cờ alert sau khi xử lý
            xEventGroupClearBits(SystemEventGroup, BIT_ALERT);

            // Tránh lặp quá nhanh
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}
