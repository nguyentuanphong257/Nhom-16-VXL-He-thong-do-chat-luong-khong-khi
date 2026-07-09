#pragma once
#include "config.h"

/** Load calibration from NVS flash into g_cal. */
void cal_load_from_nvs(void);

/** Save current g_cal to NVS flash. */
void cal_save_to_nvs(void);

/** Apply gain/offset calibration: corrected = raw * gain + offset */
float cal_apply(float raw, float gain, float offset);

/** Return pointer to current calibration parameters (read-only). */
const cal_params_t *cal_get(void);

/** Update calibration parameters (also saves to NVS). */
void cal_set(const cal_params_t *new_params);

/**
 * @brief Check if recalibration is needed.
 * @return true if drift > CAL_DRIFT_THRESHOLD_PCT or > CAL_PERIOD_DAYS.
 */
bool cal_self_check(float raw_temp, float raw_hum, uint32_t now_epoch);