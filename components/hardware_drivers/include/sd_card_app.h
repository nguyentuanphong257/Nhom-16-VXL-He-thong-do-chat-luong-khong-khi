#ifndef SD_CARD_APP_H
#define SD_CARD_APP_H

#include <stdbool.h>
#include "esp_err.h"

/**
 * @brief Khởi tạo Bus SPI và Gắn kết thẻ MicroSD (Tương đương SD.begin() của Arduino)
 */
esp_err_t sd_card_init(void);

/**
 * @brief Hủy kết nối an toàn với thẻ nhớ
 */
void sd_card_deinit(void);

/**
 * @brief Ghi một dòng dữ liệu vào thẻ SD (Tương đương myFile.println())
 * Chống sập nguồn tuyệt đối nếu thẻ bị rút ra hoặc khởi tạo lỗi.
 */
esp_err_t sd_card_write_line(const char *filepath, const char *data);


#endif // SD_CARD_APP_H