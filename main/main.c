#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "nvs_flash.h"
#include "esp_log.h"

// Gọi các Header nội bộ
#include "app_config.h"
#include "global_types.h"
#include "app_tasks.h"
#include "comms.h"
#include "bme680_app.h"
#include "ds3231.h"
#include "i2cdev.h"
#include "pms5003.h"
#include "mq135.h"
#include "sd_card_app.h"



static const char *TAG = "APP_MAIN";

/* ========================================================================== */
/* KHỞI TẠO VÙNG NHỚ CHO CÁC BIẾN EXTERN (Đã khai báo ở global_types.h)       */
/* ========================================================================== */
QueueHandle_t Q1_SensorQueue;
QueueHandle_t Q_Display;
QueueHandle_t Q_Alert;
QueueHandle_t Q_Storage;
QueueHandle_t Q_Comms;
EventGroupHandle_t SystemEventGroup;

// Chu kỳ đo mặc định ban đầu là 1000ms (1s) [cite: 39]
volatile uint32_t measure_interval_ms = 1000; 

/* ========================================================================== */
/* HÀM MAIN - ĐIỂM BẮT ĐẦU CỦA HỆ THỐNG ESP-IDF                               */
/* ========================================================================== */
void app_main(void) {
    ESP_LOGI(TAG, "--- Bắt đầu khởi động Trạm Quan Trắc Không Khí ---");

    // 1. Khởi tạo NVS (Cần thiết để lưu Wi-Fi và tham số hiệu chuẩn lâu dài)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 2. Khởi tạo các đối tượng IPC (Inter-Process Communication) cho FreeRTOS
    Q1_SensorQueue = xQueueCreate(QUEUE_LENGTH, sizeof(SensorData_t));
    Q_Display = xQueueCreate(5, sizeof(ProcessedData_t));
    Q_Alert = xQueueCreate(5, sizeof(ProcessedData_t));
    Q_Storage = xQueueCreate(10, sizeof(ProcessedData_t));
    Q_Comms   = xQueueCreate(10, sizeof(ProcessedData_t));
    SystemEventGroup = xEventGroupCreate();

    if (Q1_SensorQueue == NULL || Q_Display == NULL || Q_Alert == NULL || Q_Storage == NULL || Q_Comms == NULL || SystemEventGroup == NULL) {
        ESP_LOGE(TAG, "Không đủ bộ nhớ RAM để cấp phát Queue/EventGroup!");
        return; // Dừng hệ thống nếu không đủ RAM
    }

    //Khởi tạo subsytem i2cdev

    ESP_ERROR_CHECK(i2cdev_init());
    ESP_ERROR_CHECK(bme680_app_init());
    ESP_ERROR_CHECK(pms5003_init());  
    ESP_ERROR_CHECK(ds3231_init());  
    ESP_ERROR_CHECK(mq135_init());       // Sửa lỗi 'null pointer handle' của ADC
    esp_err_t sd_ret = sd_card_init();
    if (sd_ret != ESP_OK) {
        ESP_LOGW(TAG, "Không có thẻ SD — Task_Storage sẽ tự bỏ qua ghi file.");
        xEventGroupSetBits(SystemEventGroup, BIT_SD_CARD_ERROR); // [FIX 5]
    } else {
        xEventGroupSetBits(SystemEventGroup, BIT_SD_CARD_READY);
    }

    // 3. Khởi tạo kết nối mạng (Wi-Fi và MQTT) [cite: 54]
    wifi_manager_init();
    mqtt_manager_init();

    // I2C bus initialization is handled by i2cdev library when the first device (BME680) is created.
    // Explicit bsp_i2c_master_init() call is not needed here to avoid driver conflicts.

    // 4. Kích hoạt các FreeRTOS Tasks theo đúng Priority trong tài liệu [cite: 38, 39]
    ESP_LOGI(TAG, "Đang cấp phát luồng cho 7 Task FreeRTOS...");

    // Cú pháp: xTaskCreate(Task_Func, "Name", StackSize, Params, Priority, TaskHandle)
    // Priority cao nhất = 6, thấp nhất = 1
    
    // T1_Alert (Priority 6) [cite: 39]
    xTaskCreate(task_alert_entry, "Task_Alert", 4096, NULL, 6, NULL);
    
    // T2_Sensor (Priority 5) [cite: 39]
    xTaskCreate(task_sensor_entry, "Task_Sensor", 4096, NULL, 5, NULL);
    
    // T3_Process (Priority 4) [cite: 39]
    xTaskCreate(task_process_entry, "Task_Process", 8192, NULL, 4, NULL); // Stack lớn hơn vì chứa mảng/toán học
    
    // T4_Display (Priority 3) [cite: 39]
    xTaskCreate(task_display_entry, "Task_Display", 4096, NULL, 3, NULL);
    
    // T5_Storage (Priority 2) [cite: 39]
    xTaskCreate(task_storage_entry, "Task_Storage", 4096, NULL, 2, NULL);
    
    // T6_Comms (Priority 2) [cite: 39]
    xTaskCreate(task_comms_entry, "Task_Comms", 4096, NULL, 2, NULL);
    
    // T7_Power (Priority 1) [cite: 39]
    xTaskCreate(task_power_entry, "Task_Power", 2048, NULL, 1, NULL);

    ESP_LOGI(TAG, "Khởi tạo thành công! Hệ thống đã đi vào hoạt động đo đạc.");
    
    // Trả lại tài nguyên của hàm app_main (FreeRTOS tự động chạy các luồng ngầm)
}