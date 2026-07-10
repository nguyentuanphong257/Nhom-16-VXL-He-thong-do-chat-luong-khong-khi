#ifndef COMMS_H
#define COMMS_H

#include <stdbool.h>

/* --- WIFI CONFIG --- */
#define WIFI_SSID       "Apple meow meow"
#define WIFI_PASS       "13251971"
#define WIFI_MAXIMUM_RETRY  5

/* --- MQTT CONFIG --- */
#define MQTT_BROKER_URI     "mqtt://broker.hivemq.com:1883"
#define MQTT_TOPIC_DATA     "vias/air_monitor/data"
#define MQTT_TOPIC_ALERT    "vias/air_monitor/alert"

/* Khởi tạo hệ thống mạng Wi-Fi Station Mode */
void wifi_manager_init(void);

/* Kiểm tra xem Wi-Fi đã được cấp IP thành công chưa */
bool wifi_manager_is_connected(void);

/* Khởi tạo và kết nối tới MQTT Broker */
void mqtt_manager_init(void);

/* Kiểm tra xem đã kết nối thành công tới MQTT Broker chưa */
bool mqtt_manager_is_connected(void);

/* Hàm đẩy dữ liệu lên một Topic xác định */
bool mqtt_manager_publish(const char *topic, const char *payload, int qos);

#endif // COMMS_H