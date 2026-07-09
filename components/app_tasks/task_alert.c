#include "app_tasks.h"
#include "global_types.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "comms.h"

#define BUZZER_PIN GPIO_NUM_4

void task_alert_entry(void *pvParameters) {
    // Cấu hình chân D4 cho Buzzer [cite: 53]
    gpio_reset_pin(BUZZER_PIN);
    gpio_set_direction(BUZZER_PIN, GPIO_MODE_OUTPUT);

    while (1) {
        // Chờ tín hiệu BIT_ALERT được set (Blocking) 
        EventBits_t bits = xEventGroupWaitBits(
            SystemEventGroup,
            BIT_ALERT,
            pdFALSE,  // Clear bit sau khi đọc
            pdFALSE, // Chỉ cần 1 bit
            portMAX_DELAY
        );

        if (bits & BIT_ALERT) {
            ESP_LOGW("T1_ALERT", "Phát hiện vượt ngưỡng! Kích hoạt cảnh báo.");
            
            // Bật còi cảnh báo (GPIO4) 
            //gpio_set_level(BUZZER_PIN, 1);
            //vTaskDelay(pdMS_TO_TICKS(1000)); 
            //gpio_set_level(BUZZER_PIN, 0);

            // Đẩy MQTT sự kiện vượt ngưỡng lên Webserver 
            if (mqtt_manager_is_connected()) {
                mqtt_manager_publish(MQTT_TOPIC_ALERT, "{\"alert\": \"AQI Threshold Exceeded\"}", 1);
            }

            // Tránh lặp liên tục khi BIT_ALERT vẫn còn set
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}