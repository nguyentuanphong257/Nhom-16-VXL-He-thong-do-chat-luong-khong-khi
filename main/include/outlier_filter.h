#pragma once
#include <stdbool.h>

/**
 * @brief Outlier / noise filter combining EMA (small variation)
 *        and a spike-rejection hold (large variation).
 *
 * Algorithm (per mô tả hệ thống):
 *  - |value - past_value| < delta_max  => EMA (α = 0.5)
 *  - |value - past_value| >= delta_max =>
 *      Hold past_value for up to 3 cycles.
 *      If still high after 3 cycles => accept new value (not noise).
 */
typedef struct {
    float  ema_output;       /**< current EMA output                      */
    float  past_value;       /**< last accepted / emitted value            */
    float  alpha;            /**< EMA coefficient (0 < α ≤ 1)             */
    float  delta_max;        /**< spike threshold                          */
    int    hold_count;       /**< cycles remaining in spike-hold mode      */
    float  spike_value;      /**< value that triggered the spike hold      */
    bool   initialised;
} outlier_filter_t;

/**
 * @brief Initialise a filter instance.
 * @param f          Filter handle.
 * @param alpha      EMA smoothing factor (recommended 0.5).
 * @param delta_max  Spike detection threshold (same unit as data).
 */
void outlier_filter_init(outlier_filter_t *f, float alpha, float delta_max);

/**
 * @brief Feed a new sample and get the filtered output.
 * @param f      Filter handle.
 * @param value  New raw sample.
 * @return       Filtered output value.
 */
float outlier_filter_update(outlier_filter_t *f, float value);

/**
 * @brief Reset filter state (e.g. after a sensor error).
 */
void outlier_filter_reset(outlier_filter_t *f);