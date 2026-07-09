#include "mqtt_manager.h"
#include "config.h"
#include "mqtt_client.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

#define TAG "MQTT"

static esp_mqtt_client_handle_t s_client = NULL;

static void mqtt_event_handler(void *arg, esp_event_base_t base,
                               int32_t id, void *data)
{
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)data;
    switch (id) {
        case MQTT_EVENT_CONNECTED:
            xEventGroupSetBits(g_event_group, BIT_MQTT_CONNECTED);
            ESP_LOGI(TAG, "MQTT connected to %s", MQTT_BROKER_URI);
            /* Subscribe to command topic */
            esp_mqtt_client_subscribe(s_client, MQTT_TOPIC_CMD, 1);
            break;
        case MQTT_EVENT_DISCONNECTED:
            xEventGroupClearBits(g_event_group, BIT_MQTT_CONNECTED);
            ESP_LOGW(TAG, "MQTT disconnected");
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "CMD: topic=%.*s data=%.*s",
                     event->topic_len, event->topic,
                     event->data_len, event->data);
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT error");
            break;
        default:
            break;
    }
}

void mqtt_init(void)
{
    esp_mqtt_client_config_t cfg = {
        .broker.address.uri = MQTT_BROKER_URI,
        .credentials.client_id = MQTT_CLIENT_ID,
        .session.keepalive = 60,
        .network.reconnect_timeout_ms = 10000,
    };
    s_client = esp_mqtt_client_init(&cfg);
    esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID,
                                   mqtt_event_handler, NULL);
    esp_mqtt_client_start(s_client);
    ESP_LOGI(TAG, "MQTT client started (broker=%s)", MQTT_BROKER_URI);
}

bool mqtt_is_connected(void)
{
    return (xEventGroupGetBits(g_event_group) & BIT_MQTT_CONNECTED) != 0;
}

esp_err_t mqtt_publish_data(const result_data_t *r)
{
    if (!mqtt_is_connected()) return ESP_ERR_INVALID_STATE;

    char payload[256];
    snprintf(payload, sizeof(payload),
        "{\"ts\":\"%s\","
        "\"aqi\":%d,\"aqi_cat\":\"%s\","
        "\"T\":%.2f,\"RH\":%.1f,\"P_hpa\":%.1f,"
        "\"PM2_5\":%d,\"PM10\":%d,\"CO2\":%.1f}",
        r->timestamp_str, r->aqi, r->aqi_label,
        r->temperature, r->humidity, r->pressure_hpa,
        r->pm2_5, r->pm10, r->co2_ppm);

    int msg_id = esp_mqtt_client_publish(s_client, MQTT_TOPIC_DATA,
                                         payload, 0, 0, 0);
    return (msg_id >= 0) ? ESP_OK : ESP_FAIL;
}

esp_err_t mqtt_publish_alert(const result_data_t *r)
{
    if (!mqtt_is_connected()) return ESP_ERR_INVALID_STATE;

    char payload[128];
    snprintf(payload, sizeof(payload),
        "{\"ts\":\"%s\",\"aqi\":%d,\"alert\":true}",
        r->timestamp_str, r->aqi);

    int msg_id = esp_mqtt_client_publish(s_client, MQTT_TOPIC_ALERT,
                                         payload, 0, 1, 0); /* QoS 1 */
    return (msg_id >= 0) ? ESP_OK : ESP_FAIL;
}

esp_err_t mqtt_publish_status(const char *msg)
{
    if (!mqtt_is_connected()) return ESP_ERR_INVALID_STATE;
    int msg_id = esp_mqtt_client_publish(s_client, MQTT_TOPIC_STATUS,
                                         msg, 0, 0, 0);
    return (msg_id >= 0) ? ESP_OK : ESP_FAIL;
}