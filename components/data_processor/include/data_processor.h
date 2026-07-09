#ifndef DATA_PROCESSOR_H
#define DATA_PROCESSOR_H

#include "global_types.h"
#include <stdbool.h>

/* Cấu trúc lưu trữ trạng thái của bộ lọc cho từng kênh đo */
typedef struct {
    float past_value;
    uint8_t spike_counter;
    float delta_max;
    bool is_initialized;
} FilterState_t;

/* Khởi tạo giới hạn nhiễu cho từng kênh */
void filter_init_states(void);

/* Thuật toán lọc kết hợp EMA và khử nhiễu gai */
float filter_process_value(FilterState_t *state, float new_value);

/* Các hàm tính toán AQI */
uint16_t aqi_calc_vn_pm25(float pm25_val);
uint16_t aqi_calc_vn_pm10(float pm10_val);
uint16_t aqi_calc_total(uint16_t aqi_pm25, uint16_t aqi_pm10);

/* Các hàm hiệu chuẩn */
float calib_apply(float raw_val, float gain, float offset);
bool calib_check_drift(uint32_t active_days, float current_drift_percent);

#endif // DATA_PROCESSOR_H