#include "comms.h"
#include "esp_wifi.h"
#include "esp_event.h" // Thư viện chứa esp_event_loop_create_default
#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>

static const char *TAG = "WIFI_MGR";
static bool s_wifi_connected = false;
static int s_retry_num = 0;
static esp_timer_handle_t s_retry_timer = NULL;

/* Event Handler xử lý sự kiện Wi-Fi và IP */
static void retry_timer_callback(void* arg) {
    if (!s_wifi_connected) {
        s_retry_num = 0;
        ESP_LOGI(TAG, "Wi-Fi retry count đã được reset sau 5 giây. Thử kết nối lại...");
        esp_wifi_connect();
    }
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data) 
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        s_wifi_connected = false;
        if (s_retry_num < WIFI_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGW(TAG, "Mất kết nối Wi-Fi. Đang thử kết nối lại (%d/%d)...", s_retry_num, WIFI_MAXIMUM_RETRY);
            if (s_retry_num == WIFI_MAXIMUM_RETRY) {
                ESP_LOGI(TAG, "Đã đạt giới hạn retry. Sẽ reset retry count sau 5 giây.");
                esp_timer_start_once(s_retry_timer, 5000000);
            }
        } else {
            ESP_LOGE(TAG, "Không thể kết nối tới Wi-Fi sau %d lần thử.", WIFI_MAXIMUM_RETRY);
            if (!esp_timer_is_active(s_retry_timer)) {
                ESP_LOGI(TAG, "Bắt đầu timer reset retry count 5 giây.");
                esp_timer_start_once(s_retry_timer, 5000000);
            }
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Đã kết nối thành công! Cấp IP: " IPSTR, IP2STR(&event->ip_info.ip));
        s_wifi_connected = true;
        s_retry_num = 0;
        if (esp_timer_is_active(s_retry_timer)) {
            esp_timer_stop(s_retry_timer);
        }
    }
}

void wifi_manager_init(void) {
    s_wifi_connected = false;
    s_retry_num = 0;

    // 1. Khởi tạo Network Interface
    ESP_ERROR_CHECK(esp_netif_init());

    // 2. KHẮC PHỤC LỖI 0x103: Khởi tạo Vòng lặp sự kiện hệ thống mặc định (BẮT BUỘC)
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // 3. Khởi tạo timer để reset retry count sau 5 giây nếu Wi-Fi tiếp tục thất bại
    const esp_timer_create_args_t retry_timer_args = {
        .callback = &retry_timer_callback,
        .arg = NULL,
        .name = "wifi_retry_reset"
    };
    ESP_ERROR_CHECK(esp_timer_create(&retry_timer_args, &s_retry_timer));

    // 4. Tạo thực thể Wi-Fi Station (Bây giờ đã an toàn)
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif); // Đảm bảo interface được tạo thành công

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Khởi động Wi-Fi Station hoàn tất.");
}

bool wifi_manager_is_connected(void) {
    return s_wifi_connected;
}