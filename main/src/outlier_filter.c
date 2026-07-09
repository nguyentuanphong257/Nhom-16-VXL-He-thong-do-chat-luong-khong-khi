#include "outlier_filter.h"
#include <math.h>

#define SPIKE_HOLD_CYCLES  3

void outlier_filter_init(outlier_filter_t *f, float alpha, float delta_max)
{
    f->alpha       = alpha;
    f->delta_max   = delta_max;
    f->hold_count  = 0;
    f->initialised = false;
    f->ema_output  = 0.0f;
    f->past_value  = 0.0f;
    f->spike_value = 0.0f;
}

float outlier_filter_update(outlier_filter_t *f, float value)
{
    /* First sample: bootstrap */
    if (!f->initialised) {
        f->ema_output  = value;
        f->past_value  = value;
        f->initialised = true;
        return value;
    }

    float delta = fabsf(value - f->past_value);

    if (f->hold_count > 0) {
        /*
         * We are in spike-hold mode.
         * The incoming value may still be high or may have settled.
         */
        f->hold_count--;

        if (delta < f->delta_max) {
            /*
             * Variation settled => was just noise.
             * Continue EMA with the original past_value.
             */
            f->ema_output = f->alpha * value + (1.0f - f->alpha) * f->ema_output;
            f->past_value = f->ema_output;
            f->hold_count = 0;
        } else {
            /* Still spiking */
            if (f->hold_count == 0) {
                /*
                 * Persisted for SPIKE_HOLD_CYCLES => real change, not noise.
                 * Immediately adopt the new level.
                 */
                f->ema_output = value;
                f->past_value = value;
            }
            /* else: keep outputting past_value for remaining hold cycles */
        }
        return f->past_value;
    }

    /* Normal operation */
    if (delta < f->delta_max) {
        /* Small variation => EMA smoothing */
        f->ema_output = f->alpha * value + (1.0f - f->alpha) * f->ema_output;
        f->past_value = f->ema_output;
        return f->ema_output;
    } else {
        /* Large variation => enter spike-hold */
        f->spike_value = value;
        f->hold_count  = SPIKE_HOLD_CYCLES;
        /* Output stays at past_value this cycle */
        return f->past_value;
    }
}

void outlier_filter_reset(outlier_filter_t *f)
{
    f->initialised = false;
    f->hold_count  = 0;
}