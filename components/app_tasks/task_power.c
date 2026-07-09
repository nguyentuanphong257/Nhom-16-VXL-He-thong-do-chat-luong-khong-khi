#include "app_tasks.h"
#include "global_types.h"

#define IDLE_TIME_THRESHOLD_SEC 600 // 10 phút


void task_power_entry(void *pvParameters) {
    ProcessedData_t data;
    uint32_t good_aqi_seconds_counter = 0;

    while (1) {
        // Nhận ké kết quả từ Q_Display (dùng peek để không làm mất dữ liệu cho Task Display)
        if (xQueuePeek(Q_Display, &data, pdMS_TO_TICKS(1000)) == pdPASS) {
            
            // Theo dõi trạng thái: nếu AQI <= 50 trong suốt 10 phút 
            if (data.aqi_total <= 50) {
                good_aqi_seconds_counter += (measure_interval_ms / 1000);
            } else {
                good_aqi_seconds_counter = 0;
                measure_interval_ms = 1000; // Hoạt động: đặt lại measure_interval_ms = 1s 
            }

            // Kích hoạt nhàn rỗi: tăng measure_interval_ms lên 10s [cite: 41, 96]
            if (good_aqi_seconds_counter >= IDLE_TIME_THRESHOLD_SEC) {
                measure_interval_ms = 10000; 
                xEventGroupSetBits(SystemEventGroup, BIT_IDLE);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}