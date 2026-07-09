idf_component_register(
    SRCS 
        "filter.c"
        "aqi_calc.c"
        "calibration.c"
    INCLUDE_DIRS 
        "include"
    REQUIRES 
        global_defines
        nvs_flash        # Sử dụng NVS Flash để lưu vĩnh viễn Gain/Offset hiệu chuẩn
)