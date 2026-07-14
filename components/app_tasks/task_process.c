#include "app_tasks.h"
#include "global_types.h"
#include "data_processor.h"
#include "esp_log.h"
#include <time.h>
#include <math.h>

#define AQI_THRESHOLD_ALERT 150 // Ngưỡng bắt đầu mức Xấu

extern FilterState_t filter_pm25, filter_pm10, filter_temp, filter_hum;

static const CalibrationParams_t s_fixed_calib = {
    .temp_offset = 0.8268f,
    .temp_gain = 0.9214f,
    .hum_offset = -38.0f,
    .hum_gain = 1.5f,
};

static time_t get_unix_time(Timestamp_t ts) {
    struct tm t = {0};
    t.tm_year = ts.year - 1900;
    t.tm_mon = ts.month - 1;
    t.tm_mday = ts.day;
    t.tm_hour = ts.hour;
    t.tm_min = ts.minute;
    t.tm_sec = ts.second;
    return mktime(&t);
}

static time_t last_cal_timestamp = 0;
static float calibrated_baseline_co2 = 400.0f; // Baseline CO2 = 400 ppm
static float current_baseline_co2 = 99999.0f;
static time_t cycle_start_time = 0;
static bool has_triggered_calib = false;

static void Self_Check_Drift(Timestamp_t current_ts, float co2_ppm) {
    time_t current_time = get_unix_time(current_ts);

    if (last_cal_timestamp == 0) last_cal_timestamp = current_time;
    if (cycle_start_time == 0) cycle_start_time = current_time;

    // Cập nhật giá trị sạch nhất (Minimum CO2)
    if (co2_ppm < current_baseline_co2) {
        current_baseline_co2 = co2_ppm;
    }

    bool need_calibration = false;

    // Lớp 1: Kiểm tra quá hạn thời gian cứng (Hard Time-Check) -> 30 ngày
    if ((current_time - last_cal_timestamp) >= (30 * 24 * 3600)) {
        need_calibration = true;
    }

    // Lớp 2: Kiểm tra độ trôi bằng Baseline (Dynamic Drift-Check) -> chu kỳ 7 ngày
    if ((current_time - cycle_start_time) >= (10)) {
        float drift = 0.0f;
        if (calibrated_baseline_co2 > 0) {
            drift = fabs((current_baseline_co2 - calibrated_baseline_co2) / calibrated_baseline_co2) * 100.0f;
        }

        if (drift > 10.0f) {
            need_calibration = true;
        }

        // Reset chu kỳ
        cycle_start_time = current_time;
        current_baseline_co2 = 99999.0f;
    }

    if (need_calibration && !has_triggered_calib) {
        xEventGroupSetBits(SystemEventGroup, BIT_CALREG);
        has_triggered_calib = true; // Chỉ kích hoạt 1 lần
    }
}

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

            // Gọi hàm Self_Check_Drift
            Self_Check_Drift(result.timestamp, result.co2_ppm);

            // Phân phối dữ liệu tới các Task tiêu thụ
            xQueueSend(Q_Display, &result, 0);
            xQueueSend(Q_Storage, &result, 0);
            xQueueSend(Q_Comms,   &result, 0);
            xQueueSend(Q_Power,   &result, 0);
        }
    }
}