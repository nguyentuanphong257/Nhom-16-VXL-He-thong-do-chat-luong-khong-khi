#pragma once
#include "config.h"
#include "esp_err.h"
#include <stdbool.h>

esp_err_t sd_init(void);
esp_err_t sd_write_data(const result_data_t *r);
esp_err_t sd_write_log(const char *category, const char *message);
esp_err_t sd_buffer_offline(const result_data_t *r);
bool      sd_has_offline_data(void);
esp_err_t sd_flush_offline(void (*publish_cb)(const char *line));