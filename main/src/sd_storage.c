#include "sd_storage.h"
#include "config.h"
#include "driver/spi_common.h"
#include "driver/sdspi_host.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

#define TAG      "SD"
#define SPI_HOST SPI2_HOST

static sdmmc_card_t *s_card = NULL;
static SemaphoreHandle_t s_sd_mutex = NULL;

esp_err_t sd_init(void)
{
    s_sd_mutex = xSemaphoreCreateMutex();

    esp_vfs_fat_sdmmc_mount_config_t mount_cfg = {
        .format_if_mount_failed = true,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024,
    };

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SPI_HOST;

    spi_bus_config_t bus_cfg = {
        .mosi_io_num   = PIN_SPI_MOSI,
        .miso_io_num   = PIN_SPI_MISO,
        .sclk_io_num   = PIN_SPI_SCK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI_HOST, &bus_cfg, SDSPI_DEFAULT_DMA));

    sdspi_device_config_t dev_cfg = SDSPI_DEVICE_CONFIG_DEFAULT();
    dev_cfg.gpio_cs = PIN_SPI_CS;
    dev_cfg.host_id = SPI_HOST;

    esp_err_t r = esp_vfs_fat_sdspi_mount(SD_MOUNT_POINT, &host,
                                           &dev_cfg, &mount_cfg, &s_card);
    if (r != ESP_OK) {
        ESP_LOGE(TAG, "SD mount failed: %s", esp_err_to_name(r));
        return r;
    }

    /* Write CSV header if file is new */
    FILE *f = fopen(SD_DATA_FILE, "a");
    if (f) {
        fseek(f, 0, SEEK_END);
        if (ftell(f) == 0) {
            fprintf(f, "timestamp,aqi,aqi_label,temperature,humidity,"
                       "pressure_hpa,pm2_5,pm10,co2_ppm\n");
        }
        fclose(f);
    }

    ESP_LOGI(TAG, "MicroSD mounted at %s (%.1f MB)",
             SD_MOUNT_POINT,
             (float)s_card->csd.capacity * s_card->csd.sector_size / (1024 * 1024));
    return ESP_OK;
}

esp_err_t sd_write_data(const result_data_t *r)
{
    xSemaphoreTake(s_sd_mutex, portMAX_DELAY);
    FILE *f = fopen(SD_DATA_FILE, "a");
    if (!f) { xSemaphoreGive(s_sd_mutex); return ESP_FAIL; }

    fprintf(f, "%s,%d,%s,%.2f,%.1f,%.1f,%d,%d,%.1f\n",
            r->timestamp_str, r->aqi, r->aqi_label,
            r->temperature, r->humidity, r->pressure_hpa,
            r->pm2_5, r->pm10, r->co2_ppm);
    fclose(f);
    xSemaphoreGive(s_sd_mutex);
    return ESP_OK;
}

esp_err_t sd_write_log(const char *category, const char *message)
{
    xSemaphoreTake(s_sd_mutex, portMAX_DELAY);
    FILE *f = fopen(SD_LOG_FILE, "a");
    if (!f) { xSemaphoreGive(s_sd_mutex); return ESP_FAIL; }

    fprintf(f, "[%s] %s\n", category, message);
    fclose(f);
    xSemaphoreGive(s_sd_mutex);
    return ESP_OK;
}

esp_err_t sd_buffer_offline(const result_data_t *r)
{
    xSemaphoreTake(s_sd_mutex, portMAX_DELAY);
    FILE *f = fopen(SD_BUFFER_FILE, "a");
    if (!f) { xSemaphoreGive(s_sd_mutex); return ESP_FAIL; }

    fprintf(f, "%s,%d,%s,%.2f,%.1f,%.1f,%d,%d,%.1f\n",
            r->timestamp_str, r->aqi, r->aqi_label,
            r->temperature, r->humidity, r->pressure_hpa,
            r->pm2_5, r->pm10, r->co2_ppm);
    fclose(f);
    xSemaphoreGive(s_sd_mutex);
    return ESP_OK;
}

bool sd_has_offline_data(void)
{
    FILE *f = fopen(SD_BUFFER_FILE, "r");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    bool has = ftell(f) > 0;
    fclose(f);
    return has;
}

esp_err_t sd_flush_offline(void (*publish_cb)(const char *line))
{
    xSemaphoreTake(s_sd_mutex, portMAX_DELAY);
    FILE *f = fopen(SD_BUFFER_FILE, "r");
    if (!f) { xSemaphoreGive(s_sd_mutex); return ESP_OK; }

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        /* Strip newline */
        line[strcspn(line, "\r\n")] = '\0';
        if (publish_cb) publish_cb(line);
    }
    fclose(f);

    /* Clear buffer file after successful flush */
    remove(SD_BUFFER_FILE);
    xSemaphoreGive(s_sd_mutex);
    ESP_LOGI(TAG, "Offline buffer flushed");
    return ESP_OK;
}