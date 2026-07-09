idf_component_register(
    SRCS 
        "wifi_manager.c"
        "mqtt_manager.c"
    INCLUDE_DIRS 
        "include"
    REQUIRES 
        global_defines
        esp_wifi         # Thành phần Wi-Fi gốc của ESP-IDF
        esp_event        # Hệ thống quản lý Event Loop (Sự kiện kết nối/ngắt kết nối)
        esp_netif        # Lớp mạng Network Interface
        mqtt             # Giao thức MQTT Client hệ thống
        nvs_flash        # Wi-Fi Stack bắt buộc phải có NVS để lưu thông tin Wi-Fi cũ
)