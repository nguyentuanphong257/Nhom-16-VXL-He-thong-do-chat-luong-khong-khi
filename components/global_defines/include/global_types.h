#ifndef GLOBAL_TYPES_H
#define GLOBAL_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

/* ========================================================================== */
/* 1. ĐỊNH NGHĨA CÁC BIT CHO EVENT GROUP (ĐIỀU PHỐI TASK)                     */
/* ========================================================================== */
// Yêu cầu từ T3, T7 và T5
#define BIT_ALERT    (1 << 0)  // Set bởi T3: Kích hoạt khi có cảnh báo vượt ngưỡng
#define BIT_IDLE     (1 << 1)  // Set bởi T7: Hệ thống nhàn rỗi (AQI <= 50 trong 10 phút)
#define BIT_CALREG   (1 << 2)  // Set bởi T3: Cần tái hiệu chuẩn (drift > 10% hoặc > 30 ngày)

/* ========================================================================== */
/* 2. CẤU TRÚC DỮ LIỆU THỜI GIAN THỰC (DS3231)                                */
/* ========================================================================== */
typedef struct {
    uint16_t year;
    uint8_t  month;
    uint8_t  day;
    uint8_t  hour;
    uint8_t  minute;
    uint8_t  second;
} Timestamp_t;

/* ========================================================================== */
/* 3. CẤU TRÚC DỮ LIỆU THÔ (Truyền từ T2_Sensor -> T3_Process qua Q1)         */
/* ========================================================================== */
typedef struct {
    Timestamp_t timestamp;
    
    // Cảm biến BME680
    float    temperature;     // °C
    float    humidity;        // %RH
    float    pressure;        // hPa
    float    gas_resistance;  // Ohms
    
    // Cảm biến PMS5003
    uint16_t pm1_0;           // µg/m3
    uint16_t pm2_5;           // µg/m3
    uint16_t pm10;            // µg/m3
    
    // Cảm biến MQ-135
    uint16_t mq135_adc_val;   // Giá trị điện áp/ADC thô
    
    bool     is_valid;        // Cờ đánh dấu dữ liệu nằm trong dải đo hợp lý
} SensorData_t;

/* ========================================================================== */
/* 4. CẤU TRÚC DỮ LIỆU ĐÃ XỬ LÝ (Truyền từ T3 -> T4, T5, T6 qua Q2)           */
/* ========================================================================== */
typedef struct {
    Timestamp_t timestamp;
    
    // Dữ liệu đã qua lọc EMA/Nhiễu gai và áp dụng Gain/Offset
    float    temperature;
    float    humidity;
    float    pressure;
    uint16_t pm2_5;
    uint16_t pm10;
    float    co2_ppm;         // ppm (Đã suy diễn từ ADC)
    float    tvoc;
    
    // Chỉ số tổng hợp
    uint16_t aqi_pm2_5;
    uint16_t aqi_pm10;
    uint16_t aqi_total;       // max(AQI_PM2.5, AQI_PM10)
    uint8_t  comfort_index;   
} ProcessedData_t;

/* ========================================================================== */
/* 5. CẤU TRÚC THAM SỐ HIỆU CHUẨN (Lưu bền vững trên NVS Flash/SD)            */
/* ========================================================================== */
typedef struct {
    float temp_offset;
    float temp_gain;
    float hum_offset;
    float hum_gain;
} CalibrationParams_t;

/* ========================================================================== */
/* 6. KHAI BÁO EXTERN CÁC ĐỐI TƯỢNG IPC (Khởi tạo thực tế ở main.c)           */
/* ========================================================================== */
// Hàng đợi dữ liệu
extern QueueHandle_t Q1_SensorQueue;
extern QueueHandle_t Q_Display;
extern QueueHandle_t Q_Alert;
extern QueueHandle_t Q_Storage;
extern QueueHandle_t Q_Comms;

// Nhóm sự kiện
extern EventGroupHandle_t SystemEventGroup;

/* ========================================================================== */
/* 7. BIẾN CHIA SẺ TRẠNG THÁI HỆ THỐNG                                        */
/* ========================================================================== */
// Chu kỳ đo: 1000ms (Bình thường) hoặc 10000ms (Nhàn rỗi)
// Khai báo volatile vì biến này được cập nhật bởi T7 và đọc bởi T2
extern volatile uint32_t measure_interval_ms; 

#endif // GLOBAL_TYPES_H