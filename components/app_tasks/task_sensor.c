#include "app_tasks.h"
#include "global_types.h"
#include "bme680_app.h"
#include "mq135.h"
#include "ds3231.h"
#include "pms5003.h"



void task_sensor_entry(void *pvParameters) {
    SensorData_t raw_data;
    
    while (1) {
        // 1. Đọc DS3231 lấy timestamp TRỰC TIẾP vào biến của hệ thống
        if (ds3231_get_time(&raw_data.timestamp) != ESP_OK) {
            raw_data.timestamp.year = 0; // Đánh dấu lỗi nếu ngắt kết nối I2C
        }
        
        // 2. Đọc cảm biến BME680 (I2C) 
        bme680_app_read(&raw_data.temperature, &raw_data.humidity, 
                        &raw_data.pressure, &raw_data.gas_resistance);
                    
        // 3. Đọc PMS5003 (UART) 
        pms5003_read(&raw_data.pm2_5, &raw_data.pm10);
        raw_data.pm1_0 = 0; // Trạm không sử dụng PM1.0
        
        // 4. Đọc MQ-135 (ADC) 
        float co2_value = 0.0f;
        mq135_read_co2(&co2_value); // Vẫn giữ nguyên hàm đọc MQ-135

        // Sử dụng gas_resistance từ BME680 để ước lượng eCO2 do MQ-135 bị lỗi (D33 = 0)
        if (raw_data.gas_resistance > 0) {
            // Công thức ước lượng eCO2 cơ bản: Baseline 400 ppm + nghịch đảo điện trở khí
            co2_value = 400.0f + (500000.0f / raw_data.gas_resistance); 
        }

        raw_data.mq135_adc_val = (uint16_t)co2_value;
        
        // 5. Validate dải giá trị đo
        raw_data.is_valid = true;
        if (raw_data.temperature < -40.0f || raw_data.temperature > 85.0f) {
            raw_data.is_valid = false;
        }
        
        // 6. Gửi gói dữ liệu thô vào hàng đợi Q1_SensorQueue 
        if (raw_data.is_valid) {
            xQueueSend(Q1_SensorQueue, &raw_data, 0);
        }

        // vTaskDelay theo chu kỳ measure_interval_ms (được cập nhật bởi T7) 
        vTaskDelay(pdMS_TO_TICKS(measure_interval_ms));
    }
}