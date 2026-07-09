#ifndef MQ135_H
#define MQ135_H

#include "esp_err.h"
#include <stdint.h>

/**
 * @brief Khởi tạo bộ ADC (OneShot) cho MQ-135
 * Sử dụng kênh ADC tương ứng với chân D25
 */
esp_err_t mq135_init(void);

/**
 * @brief Đọc giá trị điện áp và quy đổi ra nồng độ CO2 tương đối
 * * @param co2_ppm Con trỏ lưu nồng độ CO2 (ppm)
 * @return esp_err_t ESP_OK nếu đọc thành công
 */
esp_err_t mq135_read_co2(float *co2_ppm);

#endif // MQ135_H