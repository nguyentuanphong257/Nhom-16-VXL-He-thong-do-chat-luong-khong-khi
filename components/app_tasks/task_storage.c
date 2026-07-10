#include "app_tasks.h"
#include "global_types.h"
#include "app_config.h"
#include "sd_card_app.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "T5_STORAGE";

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
                     "%04d-%02d-%02d %02d:%02d:%02d,%.1f,%.1f,%.1f,%d,%d,%.0f,%d\n",
                     data.timestamp.year, data.timestamp.month, data.timestamp.day,
                     data.timestamp.hour, data.timestamp.minute, data.timestamp.second,
                     data.temperature, data.humidity, data.pressure,
                     data.pm2_5, data.pm10, data.co2_ppm, data.aqi_total);
            
            sd_card_write_line("/sdcard/data.csv", csv_buffer);
            ESP_LOGD(TAG, "Đã ghi CSV: %s", csv_buffer);

            // 2. Kiểm tra và ghi nhật ký sự kiện
            EventBits_t bits = xEventGroupGetBits(SystemEventGroup);
            
            
            if (bits & BIT_CALREG) {
                // sd_card_write_log("/sdcard/event.log", "YÊU CẦU: Tái hiệu chuẩn do drift > 10% hoặc quá 30 ngày.");
                ESP_LOGI(TAG, "Ghi nhật ký: Yêu cầu tái hiệu chuẩn");
                
                // Lưu tham số mới khi có BIT_CAL_REG
                // CalibrationParams_t new_calib = {...};
                // flash_save_calib(&new_calib);
                
                // Xóa cờ sau khi xử lý xong
                xEventGroupClearBits(SystemEventGroup, BIT_CALREG);
            }
        }
    }
}