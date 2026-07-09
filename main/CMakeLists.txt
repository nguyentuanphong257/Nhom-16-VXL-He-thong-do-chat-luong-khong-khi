# File: main/CMakeLists.txt
idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES 
    app_tasks 
    global_defines 
    hardware_drivers 
    data_processor 
    display_oled 
    comms
)