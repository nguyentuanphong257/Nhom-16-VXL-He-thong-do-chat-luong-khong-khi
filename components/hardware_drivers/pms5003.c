#include "pms5003.h"
#include "driver/uart.h"
#include "esp_log.h"


static const char *TAG = "PMS5003";

esp_err_t pms5003_init(void) {
    uart_config_t uart_config = {
        .baud_rate = PMS_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    
    ESP_ERROR_CHECK(uart_param_config(PMS_UART_PORT, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(PMS_UART_PORT, PMS_TX_PIN, PMS_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(PMS_UART_PORT, PMS_BUF_SIZE * 2, 0, 0, NULL, 0));
    
    ESP_LOGI(TAG, "Khoi tao UART cho PMS5003 hoan tat.");
    return ESP_OK;
}

esp_err_t pms5003_read(uint16_t *pm25, uint16_t *pm10) {
    uint8_t data[64];

    //xóa dữ liệu cũ trong bộ đệm
    uart_flush_input(PMS_UART_PORT);

    // Đọc gói dữ liệu 32 bytes từ bộ đệm UART
    int len = uart_read_bytes(PMS_UART_PORT, data, sizeof(data), pdMS_TO_TICKS(1200));
    
    if (len < 32) {
        return ESP_ERR_TIMEOUT;
    }

    // Tìm Byte Start (0x42 và 0x4D) để đồng bộ khung dữ liệu
    for (int i = 0; i <= len - 32; i++) {
        if (data[i] == 0x42 && data[i+1] == 0x4D) {
            // Kiểm tra Checksum
            uint16_t checksum = 0;
            for (int j = 0; j < 30; j++) {
                checksum += data[i + j];
            }
            uint16_t expected_checksum = (data[i + 30] << 8) | data[i + 31];
            
            if (checksum == expected_checksum) {
                // Parse dữ liệu theo chuẩn nhà sản xuất (Môi trường - Nhà máy thường lấy trường Data 2)
                *pm25 = (data[i + 12] << 8) | data[i + 13]; // PM2.5 SP_Env
                *pm10 = (data[i + 14] << 8) | data[i + 15]; // PM10 SP_Env
                return ESP_OK;
            } else {
                return ESP_ERR_INVALID_CRC;
            }
        }
    }
    return ESP_ERR_NOT_FOUND;
}