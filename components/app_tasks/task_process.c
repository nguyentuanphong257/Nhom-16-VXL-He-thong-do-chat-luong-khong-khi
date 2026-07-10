#include "app_tasks.h"
#include "global_types.h"
#include "data_processor.h"
#include "esp_log.h"

#define AQI_THRESHOLD_ALERT 10 // Ngưỡng bắt đầu mức Xấu

extern FilterState_t filter_pm25, filter_pm10, filter_temp, filter_hum;

static const CalibrationParams_t s_fixed_calib = {
    .temp_offset = 0.3f,
    .temp_gain = 1.00f,
    .hum_offset = 1.5f,
    .hum_gain = 1.00f,
};

void task_process_entry(void *pvParameters) {
    SensorData_t raw_data;
    ProcessedData_t result;
    
    // Khởi tạo các state của bộ lọc
    filter_init_states();

    // Hiệu chuẩn cố định cho nhiệt độ và độ ẩm để bắt đầu dùng chức năng này.
    const CalibrationParams_t calib = s_fixed_calib;
    ESP_LOGI("TASK_PROCESS", "Using fixed calibration: T gain=%.2f offset=%.2f, H gain=%.2f offset=%.2f",
             calib.temp_gain, calib.temp_offset, calib.hum_gain, calib.hum_offset);

    while (1) {
        // Đợi dữ liệu mới từ Q1 (blocking receive) 
        if (xQueueReceive(Q1_SensorQueue, &raw_data, portMAX_DELAY) == pdPASS) {
            
            result.timestamp = raw_data.timestamp;
            
            // Lọc nhiễu và áp dụng hệ số hiệu chuẩn cố định
            float filtered_t = filter_process_value(&filter_temp, raw_data.temperature);
            result.temperature = calib_apply(filtered_t, calib.temp_gain, calib.temp_offset);

            float filtered_h = filter_process_value(&filter_hum, raw_data.humidity);
            result.humidity = calib_apply(filtered_h, calib.hum_gain, calib.hum_offset);
            result.pm2_5 = (uint16_t)filter_process_value(&filter_pm25, (float)raw_data.pm2_5);
            result.pm10 = (uint16_t)filter_process_value(&filter_pm10, (float)raw_data.pm10);

            result.co2_ppm = raw_data.mq135_adc_val;
            
            // Tính toán AQI 
            result.aqi_pm2_5 = aqi_calc_vn_pm25((float)result.pm2_5);
            result.aqi_pm10 = aqi_calc_vn_pm10((float)result.pm10);
            result.aqi_total = aqi_calc_total(result.aqi_pm2_5, result.aqi_pm10);
            
            // Kiểm tra vượt ngưỡng [cite: 41, 81]
            if (result.aqi_total >= AQI_THRESHOLD_ALERT) {
                xEventGroupSetBits(SystemEventGroup, BIT_ALERT);
                // Send a copy of the processed data to the alert task for logging/notification
                xQueueSend(Q_Alert, &result, 0);
            }

            // Phân phối dữ liệu tới các Task tiêu thụ
            xQueueSend(Q_Display, &result, 0);
            xQueueSend(Q_Storage, &result, 0);
            xQueueSend(Q_Comms,   &result, 0);
        }
    }
}