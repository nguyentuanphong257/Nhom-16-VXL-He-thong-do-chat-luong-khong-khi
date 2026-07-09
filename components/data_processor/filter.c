#include "data_processor.h"
#include <math.h>

// Hệ số lọc EMA (alpha)
#define ALPHA_EMA 0.5f 

FilterState_t filter_pm25;
FilterState_t filter_pm10;
FilterState_t filter_temp;
FilterState_t filter_hum;

void filter_init_states(void) {
    // Thiết lập ngưỡng delta_max tùy theo đặc thù cảm biến
    filter_pm25 = (FilterState_t){.delta_max = 20.0f, .spike_counter = 0, .is_initialized = false};
    filter_pm10 = (FilterState_t){.delta_max = 30.0f, .spike_counter = 0, .is_initialized = false};
    filter_temp = (FilterState_t){.delta_max = 2.0f, .spike_counter = 0, .is_initialized = false};
    filter_hum  = (FilterState_t){.delta_max = 5.0f, .spike_counter = 0, .is_initialized = false};
}

float filter_process_value(FilterState_t *state, float value) {
    if (!state->is_initialized) {
        state->past_value = value;
        state->is_initialized = true;
        return value;
    }

    float diff = fabs(value - state->past_value);

    // TH2: Xử lý dữ liệu bị biến động lớn (Nhiễu gai)
    if (diff >= state->delta_max) {
        if (state->spike_counter < 3) {
            state->spike_counter++;
            // Xuất đầu ra ở giá trị past_value trong 3 chu kỳ
            return state->past_value; 
        } else {
            // Sau 3 chu kỳ vẫn biến động mạnh -> Đây không phải nhiễu
            // Loại bỏ EMA, gán đầu ra bằng value mới nhất
            state->spike_counter = 0;
            state->past_value = value;
            return value;
        }
    } 
    // TH1: Xử lý dữ liệu bị biến động nhỏ (Bộ lọc EMA)
    else {
        state->spike_counter = 0;
        // Áp dụng phương trình EMA: yi = alpha * xi + (1 - alpha) * yi-1
        state->past_value = (ALPHA_EMA * value) + ((1.0f - ALPHA_EMA) * state->past_value);
        return state->past_value;
    }
}