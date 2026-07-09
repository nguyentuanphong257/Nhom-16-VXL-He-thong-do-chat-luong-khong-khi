#include "mq135.h"
#include "app_config.h"       // Nguồn khai báo chân chung của toàn hệ thống (ADC_MQ135_PIN)
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"
#include <math.h> // Cần thiết để tính hàm powf() cho đồ thị Logarit

#define RL_VALUE            10.0f  // Điện trở tải trên mạch (KOhms)
#define RO_CLEAN_AIR_FACTOR 3.6f   // Tỷ lệ Rs/Ro trong không khí sạch của MQ135

static adc_oneshot_unit_handle_t adc_handle = NULL;
static adc_channel_t mq135_channel;   // Được suy ra tự động từ ADC_MQ135_PIN (app_config.h)
static adc_unit_t    mq135_unit;
static const char *TAG = "MQ135_DRV";

esp_err_t mq135_init(void) {
    // Chống khởi tạo trùng lặp phá hủy bộ nhớ
    if (adc_handle != NULL) {
        return ESP_OK; 
    }

    // Suy ra ADC unit + channel trực tiếp từ số GPIO khai báo trong app_config.h,
    // tránh tình trạng lệch pha giữa chân vật lý và channel hardcode như trước đây.
    esp_err_t err = adc_oneshot_io_to_channel(ADC_MQ135_PIN, &mq135_unit, &mq135_channel);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "GPIO%d khong phai chan ADC hop le", ADC_MQ135_PIN);
        return err;
    }

    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = mq135_unit,
    };
    err = adc_oneshot_new_unit(&init_config, &adc_handle);
    if (err != ESP_OK) return err;

    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12, // Dải đo đầy đủ lên tới ~3.1-3.3V tùy chip
    };
    return adc_oneshot_config_channel(adc_handle, mq135_channel, &config);
}

esp_err_t mq135_read_co2(float *co2_ppm) {
    if (adc_handle == NULL) return ESP_ERR_INVALID_STATE;

    int raw_val;
    esp_err_t err = adc_oneshot_read(adc_handle, mq135_channel, &raw_val);
    if (err == ESP_OK) {
        // 1. Chuyển đổi giá trị thô ADC sang Điện áp (V)
        float voltage = (float)raw_val * 3.3f / 4095.0f;
        if (voltage < 0.1f) voltage = 0.1f; // Tránh chia cho 0

        // 2. Tính điện trở cảm biến hiện tại (Rs)
        float rs_gas = ((3.3f - voltage) * RL_VALUE) / voltage;

        // 3. Quy đổi theo đồ thị logarit chuẩn của MQ-135 (Đường cong CO2)
        // Công thức tiệm cận phổ biến: PPM = a * (Rs/Ro)^b
        // Với Ro mặc định ước lượng khoảng 20-30 KOhms ở không khí sạch, hoặc hiệu chuẩn ở Task_Process
        float ro_default = 25.0f; 
        float rs_ro_ratio = rs_gas / ro_default;

        // Thông số a, b lấy từ datasheet đường cong CO2: a = 110.47, b = -2.862
        *co2_ppm = 110.47f * powf(rs_ro_ratio, -2.862f);

        // --- DEBUG: log toàn bộ chuỗi tính toán để xác định lỗi phần cứng hay công thức ---
        // Nếu raw_val luôn ~0 hoặc ~4095 -> nghi ngờ phần cứng (dây AO, nguồn cấp, chân GPIO).
        // Nếu raw_val hợp lý nhưng co2_ppm vẫn ra 0 -> nghi ngờ công thức/ép kiểu ở nơi gọi hàm.
        ESP_LOGI(TAG, "raw=%d  V=%.3f  Rs=%.1fK  Rs/Ro=%.3f  CO2=%.2f ppm",
                 raw_val, voltage, rs_gas, rs_ro_ratio, *co2_ppm);
    } else {
        ESP_LOGE(TAG, "Doc ADC that bai: %s", esp_err_to_name(err));
    }
    return err;
}