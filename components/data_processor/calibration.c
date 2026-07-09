#include "data_processor.h"

float calib_apply(float raw_val, float gain, float offset) {
    // Công thức hiệu chỉnh
    return (raw_val * gain) + offset;
}

bool calib_check_drift(uint32_t active_days, float current_drift_percent) {
    // Self-check drift >10% hoặc >30 ngày => yêu cầu tái hiệu chuẩn
    if (active_days > 30 || current_drift_percent > 10.0f) {
        return true;
    }
    return false;
}