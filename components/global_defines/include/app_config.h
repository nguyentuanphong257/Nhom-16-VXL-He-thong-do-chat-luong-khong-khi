#ifndef APP_CONFIG_H
#define APP_CONFIG_H

/* ========================================================================== */
/* CẤU HÌNH CHÂN PIN (Theo Bảng kết nối phần cứng)               */
/* ========================================================================== */

// --- 1. Bus I2C chung (BME680, DS3231, OLED 1.3") ---
#define I2C_SDA_PIN         21
#define I2C_SCL_PIN         22
#define I2C_PORT            I2C_NUM_0
#define I2C_MASTER_FREQ_HZ  400000


// --- 2. Bus SPI chung (MicroSD Adapter) ---
#define SPI_CS_PIN          5
#define SPI_SCK_PIN         18
#define SPI_MOSI_PIN        23
#define SPI_MISO_PIN        19

// Event bits cho SD card phải khác biệt với các bit khác ở global_types.h
#define BIT_SD_CARD_READY   (1 << 3)
#define BIT_SD_CARD_ERROR   (1 << 4)

// --- 3. Bus UART (Cảm biến bụi mịn PMS5003) ---
#define UART_TX_PIN         17
#define UART_RX_PIN         16

// --- 4. Chân Analog ADC (Cảm biến khí MQ-135) ---
#define ADC_MQ135_PIN       33

// --- 5. Chân Digital Output ---
#define BUZZER_PIN          4

/* ========================================================================== */
/* CẤU HÌNH THÔNG SỐ HỆ THỐNG CƠ BẢN                                          */
/* ========================================================================== */
#define QUEUE_LENGTH        10  // Độ dài hàng đợi dữ liệu

#endif // APP_CONFIG_H