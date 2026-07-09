#pragma once
#include "config.h"
#include <stdbool.h>

void      mqtt_init(void);
bool      mqtt_is_connected(void);
esp_err_t mqtt_publish_data(const result_data_t *r);
esp_err_t mqtt_publish_alert(const result_data_t *r);
esp_err_t mqtt_publish_status(const char *msg);