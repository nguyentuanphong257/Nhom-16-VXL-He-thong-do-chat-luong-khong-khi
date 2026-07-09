#ifndef PMS5003_H
#define PMS5003_H

#include "esp_err.h"
#include <stdint.h>

#include "global_types.h" // Nhận struct định nghĩa dữ liệu từ global component

#define PMS_UART_PORT       UART_NUM_2
#define PMS_TX_PIN          17
#define PMS_RX_PIN          16
#define PMS_BAUD_RATE       9600
#define PMS_BUF_SIZE        256

/**
 * @brief Khởi tạo cấu hình UART2 cho cảm biến PMS5003
 */
esp_err_t pms5003_init(void);

/**
 * @brief Đọc và parse gói dữ liệu từ PMS5003
 * @param pm25 Con trỏ trả về giá trị PM2.5 (ug/m3)
 * @param pm10 Con trỏ trả về giá trị PM10 (ug/m3)
 * @return ESP_OK nếu đọc thành công và đúng checksum
 */
esp_err_t pms5003_read(uint16_t *pm25, uint16_t *pm10);

#endif // PMS5003_H