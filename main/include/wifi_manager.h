#pragma once
#include "esp_err.h"
#include <stdbool.h>

void      wifi_init(void);
bool      wifi_is_connected(void);
esp_err_t wifi_reconnect(uint32_t timeout_ms);