#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "driver/gpio.h"

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
/* NGẮT PHẦN CỨNG MQ-135 (D0 -> D32)                                          */
/* ========================================================================== */
static void IRAM_ATTR mq135_isr_handler(void* arg) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    // Đặt lại thời gian về 1s để đo thường xuyên hơn
    measure_interval_ms = 1000;
    
    // Xóa cờ nhàn rỗi (nếu đang bật)
    xEventGroupClearBitsFromISR(SystemEventGroup, BIT_IDLE);
    
    // Đánh thức Task_Sensor ngay lập tức và báo cho Task_Power
    xEventGroupSetBitsFromISR(SystemEventGroup, BIT_WAKEUP_ISR | BIT_GAS_INTR_POWER, &xHigherPriorityTaskWoken);
    
    if (xHigherPriorityTaskWoken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

/* ========================================================================== */
/* KHỞI TẠO VÙNG NHỚ CHO CÁC BIẾN EXTERN (Đã khai báo ở global_types.h)       */
/* ========================================================================== */
QueueHandle_t Q1_SensorQueue;
QueueHandle_t Q_Display;
QueueHandle_t Q_Alert;
QueueHandle_t Q_Storage;
QueueHandle_t Q_Comms;
QueueHandle_t Q_Power;
EventGroupHandle_t SystemEventGroup;

// Chu kỳ đo mặc định ban đầu là 1000ms (1s) [cite: 39]
volatile uint32_t measure_interval_ms = 1000; 

/* ========================================================================== */
/* KHỞI TẠO VÙNG NHỚ TĨNH CHO IPC & TASKS                                     */
/* ========================================================================== */
// Queues & EventGroup
static StaticEventGroup_t xSystemEventGroupStruct;

static uint8_t ucSensorQueueStorage[QUEUE_LENGTH * sizeof(SensorData_t)];
static StaticQueue_t xSensorQueueStruct;

static uint8_t ucQueueDisplayStorage[5 * sizeof(ProcessedData_t)];
static StaticQueue_t xQueueDisplayStruct;

static uint8_t ucQueueAlertStorage[5 * sizeof(ProcessedData_t)];
static StaticQueue_t xQueueAlertStruct;

static uint8_t ucQueueStorageStorage[10 * sizeof(ProcessedData_t)];
static StaticQueue_t xQueueStorageStruct;

static uint8_t ucQueueCommsStorage[10 * sizeof(ProcessedData_t)];
static StaticQueue_t xQueueCommsStruct;

static uint8_t ucQueuePowerStorage[2 * sizeof(ProcessedData_t)];
static StaticQueue_t xQueuePowerStruct;

// Tasks Stack & TCB
static StackType_t xTaskAlertStack[4096];
static StaticTask_t xTaskAlertTCB;

static StackType_t xTaskSensorStack[4096];
static StaticTask_t xTaskSensorTCB;

static StackType_t xTaskProcessStack[8192];
static StaticTask_t xTaskProcessTCB;

static StackType_t xTaskDisplayStack[4096];
static StaticTask_t xTaskDisplayTCB;

static StackType_t xTaskStorageStack[4096];
static StaticTask_t xTaskStorageTCB;

static StackType_t xTaskCommsStack[4096];
static StaticTask_t xTaskCommsTCB;

static StackType_t xTaskPowerStack[2048];
static StaticTask_t xTaskPowerTCB;

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

    // 2. Khởi tạo các đối tượng IPC (Inter-Process Communication) cho FreeRTOS (Cấp phát tĩnh)
    Q1_SensorQueue = xQueueCreateStatic(QUEUE_LENGTH, sizeof(SensorData_t), ucSensorQueueStorage, &xSensorQueueStruct);
    Q_Display = xQueueCreateStatic(5, sizeof(ProcessedData_t), ucQueueDisplayStorage, &xQueueDisplayStruct);
    Q_Alert = xQueueCreateStatic(5, sizeof(ProcessedData_t), ucQueueAlertStorage, &xQueueAlertStruct);
    Q_Storage = xQueueCreateStatic(10, sizeof(ProcessedData_t), ucQueueStorageStorage, &xQueueStorageStruct);
    Q_Comms   = xQueueCreateStatic(10, sizeof(ProcessedData_t), ucQueueCommsStorage, &xQueueCommsStruct);
    Q_Power   = xQueueCreateStatic(2, sizeof(ProcessedData_t), ucQueuePowerStorage, &xQueuePowerStruct);
    SystemEventGroup = xEventGroupCreateStatic(&xSystemEventGroupStruct);
    
    // (Bỏ qua việc kiểm tra NULL vì cấp phát tĩnh luôn thành công với bộ nhớ đã cấp phát sẵn)

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

    // Cấu hình ngắt phần cứng cho MQ-135 (D32)
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_ANYEDGE; // Kích hoạt khi có thay đổi trạng thái
    io_conf.pin_bit_mask = (1ULL << MQ135_INTR_PIN);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);
    
    gpio_install_isr_service(0);
    gpio_isr_handler_add(MQ135_INTR_PIN, mq135_isr_handler, NULL);

    // I2C bus initialization is handled by i2cdev library when the first device (BME680) is created.
    // Explicit bsp_i2c_master_init() call is not needed here to avoid driver conflicts.

    // 4. Kích hoạt các FreeRTOS Tasks theo đúng Priority trong tài liệu [cite: 38, 39]
    ESP_LOGI(TAG, "Đang cấp phát luồng cho 7 Task FreeRTOS...");

    // Cú pháp: xTaskCreate(Task_Func, "Name", StackSize, Params, Priority, TaskHandle)
    // Priority cao nhất = 6, thấp nhất = 1
    
    // T1_Alert (Priority 6) [cite: 39]
    xTaskCreateStatic(task_alert_entry, "Task_Alert", 4096, NULL, 6, xTaskAlertStack, &xTaskAlertTCB);
    
    // T2_Sensor (Priority 5) [cite: 39]
    xTaskCreateStatic(task_sensor_entry, "Task_Sensor", 4096, NULL, 5, xTaskSensorStack, &xTaskSensorTCB);
    
    // T3_Process (Priority 4) [cite: 39]
    xTaskCreateStatic(task_process_entry, "Task_Process", 8192, NULL, 4, xTaskProcessStack, &xTaskProcessTCB); // Stack lớn hơn vì chứa mảng/toán học
    
    // T4_Display (Priority 3) [cite: 39]
    xTaskCreateStatic(task_display_entry, "Task_Display", 4096, NULL, 3, xTaskDisplayStack, &xTaskDisplayTCB);
    
    // T5_Storage (Priority 2) [cite: 39]
    xTaskCreateStatic(task_storage_entry, "Task_Storage", 4096, NULL, 2, xTaskStorageStack, &xTaskStorageTCB);
    
    // T6_Comms (Priority 2) [cite: 39]
    xTaskCreateStatic(task_comms_entry, "Task_Comms", 4096, NULL, 2, xTaskCommsStack, &xTaskCommsTCB);
    
    // T7_Power (Priority 1) [cite: 39]
    xTaskCreateStatic(task_power_entry, "Task_Power", 2048, NULL, 1, xTaskPowerStack, &xTaskPowerTCB);

    ESP_LOGI(TAG, "Khởi tạo thành công! Hệ thống đã đi vào hoạt động đo đạc.");
    
    // Trả lại tài nguyên của hàm app_main (FreeRTOS tự động chạy các luồng ngầm)
}