#include "bme680_app.h"
#include "app_config.h"   // Nguồn sự thật duy nhất cho I2C_PORT, I2C_SDA_PIN, I2C_SCL_PIN
#include "bme680.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

#define BME680_I2C_ADDR   0x77

static const char *TAG = "BME680_DRIVER";
static bme680_t sensor;

esp_err_t bme680_app_init(void) {
    memset(&sensor, 0, sizeof(bme680_t));

    // Khởi tạo descriptor: dùng macro từ app_config.h, không hardcode pin
    esp_err_t res = bme680_init_desc(&sensor, BME680_I2C_ADDR,
                                     I2C_PORT,    // I2C_NUM_0
                                     I2C_SDA_PIN, // GPIO 21
                                     I2C_SCL_PIN  // GPIO 22
                                    );
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Lỗi khởi tạo Descriptor: %d", res);
        return ESP_FAIL;
    }

    res = bme680_init_sensor(&sensor);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Lỗi kết nối cảm biến BME680: %d", res);
        return ESP_FAIL;
    }

    // Cấu hình Oversampling và Filter
    bme680_set_oversampling_rates(&sensor, BME680_OSR_2X, BME680_OSR_16X, BME680_OSR_16X);
    bme680_set_filter_size(&sensor, BME680_IIR_SIZE_3);

    // Cấu hình bộ nung khí Gas: 320°C trong 150ms
    bme680_set_heater_profile(&sensor, 0, 320, 150);
    bme680_use_heater_profile(&sensor, 0);

    ESP_LOGI(TAG, "BME680 đã khởi tạo thành công.");
    return ESP_OK;
}

esp_err_t bme680_app_read(float *temperature, float *humidity,
                           float *pressure, float *gas_resistance) {
    bme680_values_float_t values;
    uint32_t duration;

    // Lấy thời gian đo — thư viện esp-idf-bme680 trả về đơn vị MILLISECONDS
    bme680_get_measurement_duration(&sensor, &duration);

    // [FIX 1] Nếu chip đang bận từ lần trước (do delay quá ngắn),
    // chờ thêm một chu kỳ đo đầy đủ rồi mới force lại.
    esp_err_t err = bme680_force_measurement(&sensor);
    if (err == ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "Chip đang bận, chờ thêm %"PRIu32"ms rồi thử lại...", duration + 50);
        vTaskDelay(pdMS_TO_TICKS(duration + 250));
        err = bme680_force_measurement(&sensor);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "bme680_force_measurement thất bại sau retry: %d", err);
        return ESP_FAIL;
    }

    // [FIX 2] duration đã là ms — KHÔNG chia thêm cho 1000
    // Cộng thêm 50ms buffer (gas heater cần thêm thời gian ổn định)
    uint32_t delay_ms = duration + 250;
    ESP_LOGD(TAG, "Chờ đo xong: %"PRIu32" ms", delay_ms);
    vTaskDelay(pdMS_TO_TICKS(delay_ms));

    if (bme680_get_results_float(&sensor, &values) == ESP_OK) {
        *temperature    = values.temperature;
        *humidity       = values.humidity;
        *pressure       = values.pressure;
        *gas_resistance = values.gas_resistance;
        return ESP_OK;
    }

    ESP_LOGE(TAG, "Đọc kết quả BME680 thất bại");
    return ESP_FAIL;
}