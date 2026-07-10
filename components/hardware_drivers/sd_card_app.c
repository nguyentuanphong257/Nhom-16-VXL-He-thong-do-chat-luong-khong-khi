#include "sd_card_app.h"
#include "app_config.h"
#include "global_types.h"
#include "driver/spi_master.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "esp_log.h"
#include <stdbool.h>
#include <string.h>
#include <errno.h>

static const char *TAG = "SD_CARD_APP";
#define MOUNT_POINT "/sdcard"

static sdmmc_card_t *s_card = NULL;


/* ============================================================
   Cấu trúc hàm này bám sát 1:1 theo sd_writer_init() của dự án
   ECG-PCG (đang chạy ổn định trên cùng loại module SD).
   Đã bỏ toàn bộ: pull-up thủ công, vTaskDelay, 80-clock wakeup —
   những thứ KHÔNG có trong bản tham chiếu, tức là nhiều khả năng
   không phải nguyên nhân thật của lỗi CMD52/0x106.
   Khác biệt còn lại duy nhất được test ở đây: max_freq_khz.
   ============================================================ */
esp_err_t sd_card_init(void) {
    if (s_card != NULL) return ESP_OK;

    const esp_vfs_fat_sdmmc_mount_config_t mount_cfg = {
        .format_if_mount_failed = false,
        .max_files              = 8,
        .allocation_unit_size   = 16U * 1024U,
    };
    
    const spi_bus_config_t buscfg = {
        .mosi_io_num     = SPI_MOSI_PIN,
        .miso_io_num     = SPI_MISO_PIN,
        .sclk_io_num     = SPI_SCK_PIN,
        .quadwp_io_num   = -1,
        .quadhd_io_num   = -1,
        .max_transfer_sz = 4000,
    };

    esp_err_t ret = spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "spi_bus_initialize thất bại: %s", esp_err_to_name(ret));
        xEventGroupClearBits(SystemEventGroup, BIT_SD_CARD_READY);
        xEventGroupSetBits(SystemEventGroup, BIT_SD_CARD_ERROR);
        return ret;
    }

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot         = SPI2_HOST;
    host.max_freq_khz = 10000;   // [THAY ĐỔI] 10 MHz — giống hệ thống tham chiếu, KHÔNG còn ép 400kHz

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = SPI_CS_PIN;
    slot_config.host_id = SPI2_HOST;

    

    ESP_LOGI(TAG, "Mount SD @ %ukHz (MISO=%d MOSI=%d SCK=%d CS=%d)...",
             (unsigned)host.max_freq_khz, SPI_MISO_PIN, SPI_MOSI_PIN, SPI_SCK_PIN, SPI_CS_PIN);

    ret = esp_vfs_fat_sdspi_mount(MOUNT_POINT, &host, &slot_config, &mount_cfg, &s_card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Mount thất bại: %s", esp_err_to_name(ret));
        s_card = NULL;
        spi_bus_free(SPI2_HOST); // [THÊM] Dọn bus khi thất bại — thiếu bước này ở bản trước
        xEventGroupClearBits(SystemEventGroup, BIT_SD_CARD_READY);
        xEventGroupSetBits(SystemEventGroup, BIT_SD_CARD_ERROR);
        return ret;
    }

    sdmmc_card_print_info(stdout, s_card);
    
    ESP_LOGI(TAG, "Thẻ SD sẵn sàng!");

    // [SỬA LỖI] Trước đây hàm này không hề set BIT_SD_CARD_READY sau khi mount
    // thành công, khiến các task khác (task_comms, task_storage) luôn đọc
    // sd_ready = false dù thẻ SD đã sẵn sàng thực sự.
    xEventGroupClearBits(SystemEventGroup, BIT_SD_CARD_ERROR);
    xEventGroupSetBits(SystemEventGroup, BIT_SD_CARD_READY);

    return ESP_OK;
}

esp_err_t sd_card_write_line(const char *filepath, const char *data) {
    if (s_card == NULL) {
        ESP_LOGE(TAG, "SD chưa mount, không thể ghi: %s", filepath);
        return ESP_ERR_INVALID_STATE;
    }

    FILE *f = fopen(filepath, "a");
    if (f == NULL) {
        ESP_LOGE(TAG, "Không mở được file: %s, errno=%d (%s)", filepath, errno, strerror(errno));
        return ESP_FAIL;
    }
    fprintf(f, "%s\n", data);
    fclose(f);
    return ESP_OK;
}

void sd_card_deinit(void) {
    if (s_card == NULL) return;
    esp_vfs_fat_sdcard_unmount(MOUNT_POINT, s_card);
    s_card = NULL;
    spi_bus_free(SPI2_HOST);
    xEventGroupClearBits(SystemEventGroup, BIT_SD_CARD_READY);
    ESP_LOGI(TAG, "Đã unmount thẻ SD.");
}