#include "data_processor.h"

// Cấu trúc bảng Breakpoint (QĐ 1459/QĐ-TCMT)
typedef struct {
    float bp_low;   // BP_i
    float bp_high;  // BP_i+1
    float i_low;    // I_i
    float i_high;   // I_i+1
} Breakpoint_t;

// Bảng tiêu chuẩn VN_AQI cho PM2.5
static const Breakpoint_t pm25_table[] = {
    {0.0, 25.0, 0, 50},       // Tốt
    {25.1, 50.0, 51, 100},    // Trung bình
    {50.1, 80.0, 101, 150},   // Kém
    {80.1, 150.0, 151, 200},  // Xấu
    {150.1, 250.0, 201, 300}, // Rất xấu
    {250.1, 350.0, 301, 400}, // Nguy hại
    {350.1, 500.0, 401, 500}  // Nguy hại
};

// Bảng tiêu chuẩn VN_AQI cho PM10
static const Breakpoint_t pm10_table[] = {
    {0.0, 50.0, 0, 50},
    {50.1, 150.0, 51, 100},
    {150.1, 250.0, 101, 150},
    {250.1, 350.0, 151, 200},
    {350.1, 420.0, 201, 300},
    {420.1, 500.0, 301, 400},
    {500.1, 600.0, 401, 500}
};

static uint16_t calculate_linear_interp(float C_x, const Breakpoint_t* table, uint8_t size) {
    for (uint8_t i = 0; i < size; i++) {
        if (C_x >= table[i].bp_low && C_x <= table[i].bp_high) {
            float BP_i = table[i].bp_low;
            float BP_next = table[i].bp_high;
            float I_i = table[i].i_low;
            float I_next = table[i].i_high;
            
            // Công thức nội suy tuyến tính tính AQIx
            float aqi = ((I_next - I_i) / (BP_next - BP_i)) * (C_x - BP_i) + I_i;
            return (uint16_t)aqi;
        }
    }
    // Nếu vượt quá bảng, trả về mức kịch trần
    return 500; 
}

uint16_t aqi_calc_vn_pm25(float pm25_val) {
    uint8_t size = sizeof(pm25_table) / sizeof(pm25_table[0]);
    return calculate_linear_interp(pm25_val, pm25_table, size);
}

uint16_t aqi_calc_vn_pm10(float pm10_val) {
    uint8_t size = sizeof(pm10_table) / sizeof(pm10_table[0]);
    return calculate_linear_interp(pm10_val, pm10_table, size);
}

uint16_t aqi_calc_total(uint16_t aqi_pm25, uint16_t aqi_pm10) {
    // Chỉ số AQI cuối cùng: AQI = max(AQI_PM2.5, AQI_PM10)
    return (aqi_pm25 > aqi_pm10) ? aqi_pm25 : aqi_pm10;
}