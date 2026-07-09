#include "display_oled.h"
#include "app_config.h"        // Nguồn cấu hình chân và ngoại vi tập trung
#include "u8g2.h"
#include "i2cdev.h"            // SỬ DỤNG THƯ VIỆN MỚI (Thay thế driver/i2c.h cũ để tránh lỗi xung đột driver_ng)
#include "esp_log.h"
#include "freertos/FreeRTOS.h" // THÊM THƯ VIỆN NÀY: Sửa lỗi báo đỏ vTaskDelay và pdMS_TO_TICKS
#include "freertos/task.h"     // THÊM THƯ VIỆN NÀY
#include <stdio.h>
#include <string.h>

static const char *TAG = "OLED_APP";
static u8g2_t u8g2;

#define OLED_I2C_ADDRESS   0x3C

// Định nghĩa cấu trúc descriptor quản lý thiết bị I2C cho OLED theo chuẩn thư viện mới
static i2c_dev_t oled_device;

/**
 * @brief Callback truyền dữ liệu I2C từ U8g2 xuống phần cứng thông qua thư viện i2cdev mới.
 */
static uint8_t u8x8_byte_esp32_i2c(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr) {
    static uint8_t buffer[136]; // Đủ cho 1 page 128 bytes + control byte
    static uint16_t buf_idx;

    switch (msg) {
        case U8X8_MSG_BYTE_INIT:
            // Không khởi tạo thô I2C ở đây - bus đã được quản lý an toàn thông qua cấu trúc i2cdev
            break;

        case U8X8_MSG_BYTE_START_TRANSFER:
            buf_idx = 0;
            break;

        case U8X8_MSG_BYTE_SEND: {
            uint8_t *data = (uint8_t *)arg_ptr;
            while (arg_int > 0) {
                if (buf_idx < sizeof(buffer)) {
                    buffer[buf_idx++] = *data;
                }
                data++;
                arg_int--;
            }
            break;
        }

        case U8X8_MSG_BYTE_END_TRANSFER: {
            // SỬ DỤNG HÀM GHI MỚI: i2c_dev_write thay cho i2c_master_write_to_device cũ
            // Vì U8g2 đã tự đóng gói control byte vào đầu buffer, ta truyền reg_addr = NULL và reg_len = 0
            esp_err_t err = i2c_dev_write(&oled_device, NULL, 0, buffer, buf_idx);
            if (err != ESP_OK) {
                ESP_LOGW(TAG, "I2C write failed: %d", err);
                return 0;
            }
            break;
        }

        default:
            return 0;
    }
    return 1;
}

/**
 * @brief Callback xử lý delay của U8g2 (không block FreeRTOS scheduler).
 * Đã hết báo đỏ nhờ thêm các header FreeRTOS phía trên đầu file.
 */
static uint8_t u8x8_gpio_and_delay_esp32(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr) {
    switch (msg) {
        case U8X8_MSG_DELAY_MILLI:
            vTaskDelay(pdMS_TO_TICKS(arg_int)); // Hết báo đỏ
            break;
        default:
            break;
    }
    return 1;
}

void display_oled_init(void) {
    ESP_LOGI(TAG, "Khởi tạo OLED qua i2cdev (Cổng I2C_NUM_%d)...", I2C_PORT);

    // 1. Gán cấu hình phần cứng từ app_config.h vào cổng quản lý i2cdev
    oled_device.port = I2C_PORT;
    oled_device.addr = OLED_I2C_ADDRESS;
    oled_device.cfg.sda_io_num = I2C_SDA_PIN;
    oled_device.cfg.scl_io_num = I2C_SCL_PIN;
    oled_device.cfg.master.clk_speed = I2C_MASTER_FREQ_HZ;

    // 2. Khởi tạo khóa Mutex để tránh tranh chấp Bus I2C khi đọc ghi đồng thời với BME680 / DS3231
    i2c_dev_create_mutex(&oled_device);

    // 3. Thiết lập U8g2 liên kết với các bộ Callback mới (Hỗ trợ cả SSD1306 và SH1106 tùy linh kiện của bạn)
    u8g2_Setup_sh1106_i2c_128x64_noname_f(
        &u8g2, U8G2_R0,
        u8x8_byte_esp32_i2c,
        u8x8_gpio_and_delay_esp32
    );

    // U8g2 yêu cầu địa chỉ dịch trái 1 bit: 0x3C << 1 = 0x78
    u8g2_SetI2CAddress(&u8g2, OLED_I2C_ADDRESS << 1);

    u8g2_InitDisplay(&u8g2);
    u8g2_SetPowerSave(&u8g2, 0); // 0 = Bật màn hình

    display_oled_clear();
    ESP_LOGI(TAG, "OLED sẵn sàng.");
}

void display_oled_clear(void) {
    u8g2_ClearDisplay(&u8g2);
}

void display_oled_update(const ProcessedData_t *data, bool has_alert) {
    u8g2_ClearBuffer(&u8g2);

    char buffer[32];

    // Dòng 1: VN-AQI tổng hợp và trạng thái Alert
    u8g2_SetFont(&u8g2, u8g2_font_7x14_tf);
    snprintf(buffer, sizeof(buffer), "VN-AQI: %d", data->aqi_total);
    u8g2_DrawStr(&u8g2, 0, 14, buffer);

    if (has_alert) {
        u8g2_DrawStr(&u8g2, 82, 14, "[ALERT]");
    } else {
        u8g2_DrawStr(&u8g2, 90, 14, "[OK]");
    }

    // Dòng 2: Nhiệt độ và Độ ẩm
    u8g2_SetFont(&u8g2, u8g2_font_6x10_tf);
    snprintf(buffer, sizeof(buffer), "T: %.1fC  |  H: %.1f%%", data->temperature, data->humidity);
    u8g2_DrawStr(&u8g2, 0, 30, buffer);

    // Dòng 3: Bụi mịn PM2.5 và PM10
    snprintf(buffer, sizeof(buffer), "PM2.5: %d | PM10: %d", data->pm2_5, data->pm10);
    u8g2_DrawStr(&u8g2, 0, 46, buffer);

    // Dòng 4: CO2
    snprintf(buffer, sizeof(buffer), "CO2: %.1f ppm", data->co2_ppm);
    u8g2_DrawStr(&u8g2, 0, 62, buffer);

    u8g2_SendBuffer(&u8g2);
}