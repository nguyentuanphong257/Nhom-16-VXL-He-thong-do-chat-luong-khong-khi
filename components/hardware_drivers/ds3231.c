#include "ds3231.h"
#include "app_config.h"   
#include "i2cdev.h"       

#define DS3231_ADDR   0x68

static i2c_dev_t ds3231_device;

// Hàm phụ trợ chuyển đổi BCD <-> Decimal
static uint8_t bcd2dec(uint8_t val) { return ((val / 16 * 10) + (val % 16)); }
static uint8_t dec2bcd(uint8_t val) { return ((val / 10 * 16) + (val % 10)); }

esp_err_t ds3231_init(void) {
    ds3231_device.port = I2C_PORT;
    ds3231_device.addr = DS3231_ADDR;
    ds3231_device.cfg.sda_io_num = I2C_SDA_PIN;
    ds3231_device.cfg.scl_io_num = I2C_SCL_PIN;
    ds3231_device.cfg.master.clk_speed = I2C_MASTER_FREQ_HZ;

    return i2c_dev_create_mutex(&ds3231_device);
}

// 🌟 SỬA ĐỔI: Dùng Timestamp_t trực tiếp
esp_err_t ds3231_get_time(Timestamp_t *timeinfo) {
    uint8_t data[7];
    uint8_t reg_addr = 0x00;

    esp_err_t err = i2c_dev_read(&ds3231_device, &reg_addr, 1, data, 7);

    if (err == ESP_OK) {
        timeinfo->second = bcd2dec(data[0]);
        timeinfo->minute = bcd2dec(data[1]);
        timeinfo->hour   = bcd2dec(data[2] & 0x3F);
        // Bỏ qua ngày trong tuần ở data[3]
        timeinfo->day    = bcd2dec(data[4]);
        timeinfo->month  = bcd2dec(data[5] & 0x1F); // Giữ nguyên mốc 1-12
        timeinfo->year   = bcd2dec(data[6]) + 2000; // Cộng thêm 2000 (Ví dụ: 26 -> 2026)
    }
    return err;
}

// 🌟 SỬA ĐỔI: Dùng Timestamp_t trực tiếp
esp_err_t ds3231_set_time(Timestamp_t *timeinfo) {
    uint8_t data[7];
    uint8_t reg_addr = 0x00;
    
    data[0] = dec2bcd(timeinfo->second);
    data[1] = dec2bcd(timeinfo->minute);
    data[2] = dec2bcd(timeinfo->hour);
    data[3] = 1; // Giá trị thứ trong tuần không quan trọng với bài toán này
    data[4] = dec2bcd(timeinfo->day);
    data[5] = dec2bcd(timeinfo->month);  
    data[6] = dec2bcd(timeinfo->year - 2000); // Lấy 2 số cuối của năm

    return i2c_dev_write(&ds3231_device, &reg_addr, 1, data, sizeof(data));
}