#include "app_tasks.h"
#include "global_types.h"
#include "display_oled.h"

void task_display_entry(void *pvParameters) {
    ProcessedData_t data;
    
    display_oled_init();

    while (1) {
        // Đợi dữ liệu từ Q_Display
        if (xQueueReceive(Q_Display, &data, portMAX_DELAY) == pdPASS) {
            
            // Kiểm tra xem hệ thống có đang ở trạng thái cảnh báo không
            EventBits_t bits = xEventGroupGetBits(SystemEventGroup);
            bool has_alert = (bits & BIT_ALERT) ? true : false;
            bool is_idle = (bits & BIT_IDLE) ? true : false;
            
            // Cập nhật màn hình mỗi chu kỳ nhận kết quả 
            display_oled_update(&data, has_alert, is_idle);
        }
    }
}