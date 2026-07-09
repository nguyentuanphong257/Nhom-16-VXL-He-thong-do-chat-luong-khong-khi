#ifndef DS3231_H
#define DS3231_H

#include "esp_err.h"
#include "global_types.h" // Gọi thư viện chứa struct Timestamp_t

/**
 * @brief Khởi tạo giao tiếp I2C cho DS3231
 */
esp_err_t ds3231_init(void);

/**
 * @brief Lấy thời gian từ RTC và lưu trực tiếp vào Timestamp_t
 */
esp_err_t ds3231_get_time(Timestamp_t *timeinfo);

/**
 * @brief Cài đặt thời gian cho RTC từ Timestamp_t
 */
esp_err_t ds3231_set_time(Timestamp_t *timeinfo);

#endif // DS3231_H