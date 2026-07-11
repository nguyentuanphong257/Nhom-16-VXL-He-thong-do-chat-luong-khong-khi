#ifndef DISPLAY_OLED_H
#define DISPLAY_OLED_H

#include "global_types.h"
#include <stdbool.h>

/* Khởi tạo màn hình OLED 1.3 inch qua I2C */
void display_oled_init(void);

/* Cập nhật các thông số AQI, T, RH, PM2.5, PM10, CO2 lên màn hình */
void display_oled_update(const ProcessedData_t *data, bool has_alert, bool is_idle);

/* Xóa màn hình */
void display_oled_clear(void);

#endif // DISPLAY_OLED_H