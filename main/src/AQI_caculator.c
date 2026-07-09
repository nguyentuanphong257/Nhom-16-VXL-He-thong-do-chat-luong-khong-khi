#include "AQI_caculator.h"
#include <stddef.h>

/* ----------------------------------------------------------------
 * VN_AQI breakpoint tables (Quyết định 1459/QĐ-TCMT)
 * Each row: { C_lo, C_hi, I_lo, I_hi }
 * ---------------------------------------------------------------- */
typedef struct { float c_lo, c_hi; uint16_t i_lo, i_hi; } bp_t;

/* PM2.5 µg/m³  (1-hour) */
static const bp_t PM25_BP[] = {
    {  0.0f,  25.0f,   0,  50 },
    { 25.1f,  50.0f,  51, 100 },
    { 50.1f, 150.0f, 101, 150 },
    {150.1f, 250.0f, 151, 200 },
    {250.1f, 350.0f, 201, 300 },
    {350.1f, 500.0f, 301, 500 },
};

/* PM10 µg/m³  (1-hour) */
static const bp_t PM10_BP[] = {
    {  0.0f,  50.0f,   0,  50 },
    { 50.1f, 100.0f,  51, 100 },
    {100.1f, 265.0f, 101, 150 },
    {265.1f, 430.0f, 151, 200 },
    {430.1f, 600.0f, 201, 300 },
    {600.1f, 604.0f, 301, 500 },
};

#define BP_COUNT(arr) (sizeof(arr)/sizeof(arr[0]))

/**
 * @brief Linear interpolation between two AQI breakpoints.
 *        Formula: I = (I_hi - I_lo)/(C_hi - C_lo) * (C - C_lo) + I_lo
 */
static uint16_t interpolate(float c, const bp_t *table, size_t n)
{
    if (c < 0.0f) return 0;

    for (size_t i = 0; i < n; i++) {
        if (c <= table[i].c_hi) {
            float ratio = (float)(table[i].i_hi - table[i].i_lo)
                        / (table[i].c_hi - table[i].c_lo);
            float idx = ratio * (c - table[i].c_lo) + (float)table[i].i_lo;
            return (uint16_t)(idx + 0.5f);   /* round */
        }
    }
    return 500;   /* beyond highest breakpoint */
}

uint16_t aqi_from_pm25(float pm25)
{
    return interpolate(pm25, PM25_BP, BP_COUNT(PM25_BP));
}

uint16_t aqi_from_pm10(float pm10)
{
    return interpolate(pm10, PM10_BP, BP_COUNT(PM10_BP));
}

uint16_t aqi_calculate(float pm25, float pm10)
{
    uint16_t a25 = aqi_from_pm25(pm25);
    uint16_t a10 = aqi_from_pm10(pm10);
    return (a25 > a10) ? a25 : a10;
}

aqi_category_t aqi_category(uint16_t aqi)
{
    if (aqi <=  50) return AQI_GOOD;
    if (aqi <= 100) return AQI_MODERATE;
    if (aqi <= 150) return AQI_POOR;
    if (aqi <= 200) return AQI_BAD;
    if (aqi <= 300) return AQI_VERY_BAD;
    return AQI_HAZARDOUS;
}

const char *aqi_label(aqi_category_t cat)
{
    switch (cat) {
        case AQI_GOOD:      return "Tot";
        case AQI_MODERATE:  return "Trung binh";
        case AQI_POOR:      return "Kem";
        case AQI_BAD:       return "Xau";
        case AQI_VERY_BAD:  return "Rat xau";
        case AQI_HAZARDOUS: return "Nguy hai";
        default:            return "N/A";
    }
}