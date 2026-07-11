#include "app_tasks.h"
#include "global_types.h"
#include "app_config.h"
#include "comms.h"
#include "sd_card_app.h"
#include "esp_log.h"
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

static const char *TAG = "T6_COMMS";
static const char *MQTT_DATA_OUTBOX = "/sdcard/mqdata.log";
static const char *MQTT_ALERT_OUTBOX = "/sdcard/mqalt.log";
static const char *MQTT_DATA_TMP = "/sdcard/mqdata.tmp";
static const char *MQTT_ALERT_TMP = "/sdcard/mqalt.tmp";

extern QueueHandle_t Q_Comms;

static bool mqtt_publish_outbox_line(const char *line, bool is_alert)
{
    if (!mqtt_manager_is_connected()) {
        return false;
    }

    if (is_alert) {
        return mqtt_manager_publish(MQTT_TOPIC_ALERT, line, 1);
    }

    return mqtt_manager_publish(MQTT_TOPIC_DATA, line, 0);
}

static bool flush_mqtt_outbox(const char *path, const char *tmp_path, bool is_alert)
{
    FILE *f = fopen(path, "r");
    if (f == NULL) {
        return false;
    }

    FILE *tmp = fopen(tmp_path, "w");
    if (tmp == NULL) {
        fclose(f);
        return false;
    }

    char line[512];
    bool all_sent = true;
    while (fgets(line, sizeof(line), f) != NULL) {
        line[strcspn(line, "\r\n")] = '\0';

        if (all_sent) {
            if (!mqtt_publish_outbox_line(line, is_alert)) {
                all_sent = false;
            }
        }

        if (!all_sent) {
            fprintf(tmp, "%s\n", line);
        }
    }

    fclose(f);
    fclose(tmp);

    if (all_sent) {
        remove(path);
        remove(tmp_path);
        ESP_LOGI(TAG, "Đã flush outbox MQTT: %s", path);
        return true;
    }

    remove(path);
    rename(tmp_path, path);
    return false;
}

static void try_flush_mqtt_outboxes(void)
{
    if (!wifi_manager_is_connected() || !mqtt_manager_is_connected()) {
        return;
    }

    flush_mqtt_outbox(MQTT_DATA_OUTBOX, MQTT_DATA_TMP, false);
    flush_mqtt_outbox(MQTT_ALERT_OUTBOX, MQTT_ALERT_TMP, true);
}

static bool calib_alert_handled = false;

void task_comms_entry(void *pvParameters) {
    ProcessedData_t data;
    char payload_buffer[256];

    while (1) {
        bool is_connected = wifi_manager_is_connected() && mqtt_manager_is_connected();
        if (is_connected) {
            try_flush_mqtt_outboxes();
        }

        if (xQueueReceive(Q_Comms, &data, pdMS_TO_TICKS(2000)) == pdPASS) {
            snprintf(payload_buffer, sizeof(payload_buffer), 
                     "{\"timestamp\":\"%04d-%02d-%02d %02d:%02d:%02d\","
                     "\"aqi\":%d,\"temp\":%.1f,\"hum\":%.1f,\"pm25\":%d,\"pm10\":%d,\"co2\":%.0f}",
                     data.timestamp.year, data.timestamp.month, data.timestamp.day,
                     data.timestamp.hour, data.timestamp.minute, data.timestamp.second,
                     data.aqi_total, data.temperature, data.humidity,
                     data.pm2_5, data.pm10, data.co2_ppm);

            EventBits_t bits = xEventGroupGetBits(SystemEventGroup);
            int qos = (bits & BIT_ALERT) ? 1 : 0;

            if (bits & BIT_CALREG) {
                if (!calib_alert_handled) {
                    char alert_msg[192];
                    snprintf(alert_msg, sizeof(alert_msg),
                             "{\"timestamp\":\"%04d-%02d-%02d %02d:%02d:%02d\",\"alert\":\"CALIBRATION_REQUIRED\",\"reason\":\"Drift > 10%%\"}",
                             data.timestamp.year, data.timestamp.month, data.timestamp.day,
                             data.timestamp.hour, data.timestamp.minute, data.timestamp.second);
                    if (is_connected) {
                        mqtt_manager_publish(MQTT_TOPIC_ALERT, alert_msg, 1);
                    } else {
                        if (xEventGroupGetBits(SystemEventGroup) & BIT_SD_CARD_READY) {
                            sd_card_write_line(MQTT_ALERT_OUTBOX, alert_msg);
                        }
                    }
                    calib_alert_handled = true;
                }
            } else {
                calib_alert_handled = false;
            }

            if (is_connected) {
                if (mqtt_manager_publish(MQTT_TOPIC_DATA, payload_buffer, qos)) {
                    ESP_LOGD(TAG, "Đã gửi MQTT thành công.");
                } else {
                    ESP_LOGW(TAG, "Không gửi được MQTT, lưu vào outbox.");
                    if (xEventGroupGetBits(SystemEventGroup) & BIT_SD_CARD_READY) {
                        sd_card_write_line(MQTT_DATA_OUTBOX, payload_buffer);
                    } else {
                        ESP_LOGW(TAG, "SD không sẵn sàng, bỏ qua lưu data outbox");
                    }
                }
            } else {
                ESP_LOGW(TAG, "Mất kết nối mạng, lưu MQTT data vào outbox.");
                if (xEventGroupGetBits(SystemEventGroup) & BIT_SD_CARD_READY) {
                    sd_card_write_line(MQTT_DATA_OUTBOX, payload_buffer);
                } else {
                    ESP_LOGW(TAG, "SD không sẵn sàng, bỏ qua lưu data outbox");
                }
            }
        }
    }
}