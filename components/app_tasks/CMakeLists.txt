idf_component_register(
    SRCS 
        "task_alert.c"
        "task_sensor.c"
        "task_process.c"
        "task_display.c"
        "task_storage.c"
        "task_comms.c"
        "task_power.c"
    INCLUDE_DIRS 
        "include"
    REQUIRES 
        global_defines   # Đọc cấu trúc Queue handles, EventBits
        hardware_drivers # Gọi hàm đọc từ cảm biến
        data_processor   # Gọi thuật toán lọc nhiễu, tính toán AQI
        display_oled     # Gọi hàm xuất dữ liệu ra màn hình
        comms            # Gọi hàm gửi dữ liệu MQTT
        driver           # Sử dụng GPIO điều khiển còi báo Buzzer (Task Alert)
        nvs_flash
)