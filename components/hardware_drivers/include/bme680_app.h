#ifndef BME680_APP_H
#define BME680_APP_H

#include "esp_err.h"

/**
 * @brief Khởi tạo cảm biến BME680 trên I2C Bus đã tạo
 */
esp_err_t bme680_app_init(void);

/**
 * @brief Đọc dữ liệu từ cảm biến BME680
 * * @param temperature Con trỏ lưu nhiệt độ (°C)
 * @param humidity Con trỏ lưu độ ẩm (%RH)
 * @param pressure Con trỏ lưu áp suất (hPa)
 * @param gas_resistance Con trỏ lưu điện trở khí (Ohms) - dùng để tính TVOC
 * @return esp_err_t ESP_OK nếu thành công
 */
esp_err_t bme680_app_read(float *temperature, float *humidity, float *pressure, float *gas_resistance);

#endif // BME680_APP_H