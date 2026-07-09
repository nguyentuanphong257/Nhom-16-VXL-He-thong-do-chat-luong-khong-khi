#pragma once
#include <stdint.h>

/**
 * @brief AQI category indices (VN_AQI – Quyết định 1459/QĐ-TCMT)
 */
typedef enum {
    AQI_GOOD        = 0,   /* 0 – 50   (Tốt / Xanh)      */
    AQI_MODERATE    = 1,   /* 51 – 100 (Trung bình / Vàng)*/
    AQI_POOR        = 2,   /* 101–150  (Kém / Da cam)     */
    AQI_BAD         = 3,   /* 151–200  (Xấu / Đỏ)        */
    AQI_VERY_BAD    = 4,   /* 201–300  (Rất xấu / Tím)   */
    AQI_HAZARDOUS   = 5,   /* 301–500  (Nguy hại / Nâu)  */
} aqi_category_t;

/**
 * @brief Calculate AQI sub-index for PM2.5 (µg/m³, 1-hour average).
 */
uint16_t aqi_from_pm25(float pm25_ug_m3);

/**
 * @brief Calculate AQI sub-index for PM10 (µg/m³, 1-hour average).
 */
uint16_t aqi_from_pm10(float pm10_ug_m3);

/**
 * @brief Final AQI = max(AQI_PM2.5, AQI_PM10).
 */
uint16_t aqi_calculate(float pm25_ug_m3, float pm10_ug_m3);

/**
 * @brief Return the category enum for a given AQI value.
 */
aqi_category_t aqi_category(uint16_t aqi);

/**
 * @brief Return a Vietnamese label string for a category.
 */
const char *aqi_label(aqi_category_t cat);