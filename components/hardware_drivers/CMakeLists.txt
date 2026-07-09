idf_component_register(
    SRCS 
        "bme680_app.c"
        "pms5003.c"
        "mq135.c"
        "ds3231.c"
        "sd_card_app.c"
    INCLUDE_DIRS 
        "include"
    REQUIRES 
        log
        global_defines
        driver           # Chứa I2C, SPI, UART, GPIO chỉnh của ESP32
        esp_adc          # Thư viện quản lý ADC Oneshot/Continuous mới từ v5.0+
        fatfs            # Định dạng file hệ thống cho thẻ nhớ SD
        vfs              # Hệ thống file ảo để dùng các hàm fopen, fprintf chuẩn C
        esp-idf-lib__bme680
        esp-idf-lib__i2cdev # Thư viện quản lý bus I2C cho esp-idf-lib
)