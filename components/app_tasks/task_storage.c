#include "app_tasks.h"
#include "global_types.h"
#include "app_config.h"
#include "sd_card_app.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "T5_STORAGE";
static bool calib_log_handled = false;

void task_storage_entry(void *pvParameters) {
    
    EventBits_t bits = xEventGroupWaitBits(SystemEventGroup,
    BIT_SD_CARD_READY | BIT_SD_CARD_ERROR, pdFALSE, pdFALSE, pdMS_TO_TICKS(5000));

if (bits & BIT_SD_CARD_ERROR) {
    ESP_LOGW("Task_Storage", "SD lỗi, task chạy chế độ no-op.");
    vTaskDelete(NULL); // Hoặc loop rỗng tùy thiết kế
    return;
}
    
    ProcessedData_t data;
    char csv_buffer[128];
    
    
    ESP_LOGI(TAG, "Khởi động: Đang tải tham số hiệu chuẩn từ Flash...");

    while (1) {
        if (xQueueReceive(Q_Storage, &data, portMAX_DELAY) == pdPASS) {
            snprintf(csv_buffer, sizeof(csv_buffer), 
                     "%04d-%02d-%02d %02d:%02d:%02d,%.1f,%.1f,%d,%d,%.0f,%d\n",
                     data.timestamp.year, data.timestamp.month, data.timestamp.day,
                     data.timestamp.hour, data.timestamp.minute, data.timestamp.second,
                     data.temperature, data.humidity ,
                     data.pm2_5, data.pm10, data.co2_ppm, data.aqi_total);
            
            sd_card_write_line("/sdcard/data.csv", csv_buffer);
            ESP_LOGD(TAG, "Đã ghi CSV: %s", csv_buffer);

            // 2. Kiểm tra và ghi nhật ký sự kiện
            EventBits_t bits = xEventGroupGetBits(SystemEventGroup);
            
            
            if (bits & BIT_CALREG) {
                if (!calib_log_handled) {
                    char log_buffer[128];
                    snprintf(log_buffer, sizeof(log_buffer),
                             "[%04d-%02d-%02d %02d:%02d:%02d] YEU CAU: Tai hieu chuan.\n",
                             data.timestamp.year, data.timestamp.month, data.timestamp.day,
                             data.timestamp.hour, data.timestamp.minute, data.timestamp.second);
                    sd_card_write_line("/sdcard/event.log", log_buffer);
                    ESP_LOGI(TAG, "Ghi nhat ky: %s", log_buffer);
                    calib_log_handled = true;
                }
            } else {
                calib_log_handled = false;
            }
        }
    }
}