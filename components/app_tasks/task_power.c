#include "app_tasks.h"
#include "global_types.h"
#include "esp_log.h"

#define IDLE_TIME_THRESHOLD_SEC 600 // 10 phút

static const char *TAG = "POWER_TASK";

void task_power_entry(void *pvParameters) {
    ProcessedData_t data;
    uint32_t good_aqi_seconds_counter = 0;

    while (1) {
        if (xQueueReceive(Q_Power, &data, portMAX_DELAY) == pdPASS) {
            
            // Kiểm tra xem có ngắt phần cứng từ chân D33 (khí gas) không
            EventBits_t uxBits = xEventGroupClearBits(SystemEventGroup, BIT_GAS_INTR_POWER);
            if ((uxBits & BIT_GAS_INTR_POWER) != 0) {
                good_aqi_seconds_counter = 0;
                ESP_LOGI(TAG, "Ngắt phần cứng (Gas): Reset good_aqi_seconds_counter");
            }
            
            // Theo dõi trạng thái: nếu AQI <= 200
            if (data.aqi_total <= 50) {
                good_aqi_seconds_counter += (measure_interval_ms / 1000);
            } else {
                good_aqi_seconds_counter = 0;
                if (measure_interval_ms != 1000) {
                    measure_interval_ms = 1000; // Hoạt động: đặt lại measure_interval_ms = 1s 
                    xEventGroupClearBits(SystemEventGroup, BIT_IDLE);
                    ESP_LOGI(TAG, "Thoát khỏi chế độ nhàn rỗi");
                }
            }

            // Kích hoạt nhàn rỗi: tăng measure_interval_ms lên 10s
            if (good_aqi_seconds_counter >= IDLE_TIME_THRESHOLD_SEC) {
                if (measure_interval_ms != 10000) {
                    measure_interval_ms = 10000; 
                    xEventGroupSetBits(SystemEventGroup, BIT_IDLE);
                    ESP_LOGI(TAG, "Chuyển sang chế độ nhàn rỗi");
                }
            }
        }
    }
}