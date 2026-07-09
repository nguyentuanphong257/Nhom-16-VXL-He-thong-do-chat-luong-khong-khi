idf_component_register(
    SRCS 
        "ssd1306_app.c"
    INCLUDE_DIRS 
        "include"
    REQUIRES 
        global_defines
        hardware_drivers # Phụ thuộc vào I2C Bus dùng chung từ BSP
        driver           # Cần driver GPIO/I2C để thao tác màn hình
        u8g2

)