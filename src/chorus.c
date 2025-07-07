/* August 24, 1998
 * Copyright (C) 1998 Juergen Mueller And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained.
 * Juergen Mueller And Sundry Contributors are not responsible for
 * the consequences of using this software.
 *
 * minor updates: Martin W. Guy, Dr. Thomas Tensi, 2025-01
 */

/*
 * Chorus effect.
 *
 * See the flow diagram in the usage message at the end of the file.
 *
 * In reality, it only has one delay buffer, whose size is the longest version
 * of (delay + depth) and each of the apparent delays reads from that,
 *
 * The delay i is controlled by a sine or triangle modulation i ( 1 <= i <= n).
 */

/*
 * libSoX chorus effect
 */

#include "sox_i.h"

/** the number of parameter for a chorus stage */
#define PARAM_COUNT_PER_STAGE 5

/** the number of global chorus parameters */
#define FIXED_PARAM_COUNT    2

/** the downscaling factor for the samples in the delay line
 * to prevent overflow; best if it's a power of 2 */
#define SCALING_FACTOR 256

/** the maximum number of stages in a chorus, because of SCALING_FACTOR */
#define MAX_STAGE_COUNT SCALING_FACTOR

/** the function for checking for a clipped sample in the resolution
 * after downscaling */
#define CLIP_COUNT_PROC SOX_24BIT_CLIP_COUNT
 
/** the allowed options for the modulation kind mapped onto a wave
 * type */
static lsx_enum_item modulation_kind_map[] ={
    { "-s", SOX_WAVE_SINE },
    { "-t", SOX_WAVE_TRIANGLE },
    { NULL, 0 }
};

/** the type used in the delay lines */
typedef sox_sample_t chorus_delay_sample_t;

/** an auxiliary macro for doing a modular increment */
#define MODULAR_INCREMENT(a, b)     a = ((a) + 1) % (b)

/*--------------------*/

/** a single stage in the chorus effect */
typedef struct {
        /* command line parameters */
        float                  delay;       /* in seconds */
        float                  decay;
        float                  speed;       /* in Hz */
        float                  depth;       /* in seconds */
        lsx_wave_t             wave_type;
        /* sample tables, table counts and indices */
        sox_uint32_t           delay_line_index;
        sox_uint32_t           delay_line_length;
        chorus_delay_sample_t  *delay_line;
        sox_uint32_t           depth_sample_count;
        sox_uint32_t           wave_index;
        sox_uint32_t           wave_length;
        int                    *wave_table;
} chorus_stage_t;

/*--------------------*/

/** the chorus effect */
typedef struct {
        /* command line parameters */
        float           gain_in;
        float           gain_out;

        /* all stages of that effect */
        sox_uint32_t    stage_count;
        chorus_stage_t  *stage;

        /* remaining samples for drain phase */
        sox_uint32_t    remaining_samples;
} chorus_priv_t;

/*--------------------*/

/**
 * Process command line options for this effect <C>effp</C> given by
 * <C>argc</C> and <C>argv</C>.
 *
 * @param effp  chorus effect affected by parameters
 * @param argc  count of parameters (including effect name)
 * @param argv  list of string parameters
 * @return  information whether parameter collection has been
 *          successful
 */
static int sox_chorus_getopts (sox_effect_t *effp,
                               int argc,
                               char **argv)
{
        chorus_priv_t *chorus = (chorus_priv_t *) effp->priv;
        sox_uint32_t i;
        float total_volume;

        /* skip over effect name */
        argc--;
        argv++;

        /* there must be at least one stage
         * and all stages must have parameters */
        if ( argc < FIXED_PARAM_COUNT + PARAM_COUNT_PER_STAGE
            || (argc - FIXED_PARAM_COUNT) % PARAM_COUNT_PER_STAGE != 0) {
            return lsx_usage(effp);
        }

        /* read the global parameters gain_in and gain_out */
        do {
            chorus_priv_t* p = chorus;
            NUMERIC_PARAMETER(gain_in, -1.0, 1.0);
            NUMERIC_PARAMETER(gain_out,-1.0, 1.0);
        } while (0);

        /* read all stages */
        chorus->stage_count = 0;

        do {
            chorus_stage_t *p;

	    lsx_revalloc(chorus->stage, chorus->stage_count + 1);
	    p = &chorus->stage[chorus->stage_count];
            memset(p, 0, sizeof(*p));

            NUMERIC_PARAMETER(delay,  0, 86400000);
            NUMERIC_PARAMETER(decay, -1, 1);
            NUMERIC_PARAMETER(speed,  0, 192000);
            NUMERIC_PARAMETER(depth,  0, 86400000);
            TEXTUAL_PARAMETER(wave_type, modulation_kind_map);

            /* normalize time parameters to seconds */
            p->delay /= 1000.0;
            p->depth /= 1000.0;
            chorus->stage_count++;
        } while (argc > 0 && chorus->stage_count < MAX_STAGE_COUNT);

	if (argc > 0) {
	    lsx_fail("there is a maximum of %d chorus stages", MAX_STAGE_COUNT);
	    return SOX_EOF;
	}

        /* issue warning about possible clipping when parameters are
         * above some threshold */
        total_volume = chorus->gain_in;

        for (i = 0; i < chorus->stage_count; i++) {
            total_volume += chorus->stage[i].decay;
        }

        if (total_volume * chorus->gain_out > 1.0) {
            lsx_warn("the output may saturate; a safe gain-out is %g",
                     fabsf(1.0f / total_volume));
        }

        return (SOX_SUCCESS);
}

/*--------------------*/

/**
 * Prepare effect <C>effp</C> for processing.
 *
 * @param effp  chorus effect about to be started
 * @return  information whether start was successful
  */
static int sox_chorus_start (sox_effect_t *effp)
{
        chorus_priv_t *chorus = (chorus_priv_t *) effp->priv;
        sox_uint32_t i;

        /* start is called once per channel, but each channel gets a copy
         * of the "stage" pointer, pointing to the same array of stages
         * which are common data for all channels except for the delay line
         * and its offsets, which need to be separate for every channel.
         *
         * A hacky solution is to copy the stage array for every channel.
         * This means that everyone gets their own copy of the same wavetable
         * but at least they all get separate delay lines.
         */

        if (effp->flow != 0) {
                chorus_stage_t *stages = chorus->stage;
                chorus_stage_t *newstages;

                lsx_valloc(newstages, chorus->stage_count);
                memcpy(newstages, stages, chorus->stage_count * sizeof(chorus_stage_t));
                chorus->stage = newstages;
        }

        for (i = 0;  i < chorus->stage_count;  i++) {
                chorus_stage_t *stage = &chorus->stage[i];
		double dll;

                stage->depth_sample_count =
                    stage->depth * effp->in_signal.rate;

                /* delay line */
                dll = ceil((stage->delay + stage->depth) * effp->in_signal.rate);
                if (dll > SOX_UINT_MAX(32)) {
		    lsx_fail("delay + depth can't be more than %.0f ms at a sample rate of %.0fHz",
			     SOX_UINT_MAX(32) / effp->in_signal.rate * 1000,
			     effp->in_signal.rate);
		    return SOX_EOF;
		}
                stage->delay_line_length = dll;
		if (stage->delay_line_length < 1) {
		    lsx_fail("delay can't be less than %g milliseconds",
			     1000 / effp->in_signal.rate);
		    return SOX_EOF;
		}
                stage->delay_line =
                    lsx_calloc(stage->delay_line_length,
                               sizeof(chorus_delay_sample_t));

                /* modulation wave table */
                stage->wave_length = effp->in_signal.rate / stage->speed;
		if (stage->wave_length < 1) {
		    lsx_fail("speed can't be more than the sample rate");
		    return SOX_EOF;
		}
                lsx_valloc(stage->wave_table, stage->wave_length);
                lsx_generate_wave_table(stage->wave_type, SOX_INT,
                                        stage->wave_table,
                                        stage->wave_length,
                                        0., stage->depth_sample_count,
                                        M_PI_2);
 
                /* find maximum delay line length across all stages */
                chorus->remaining_samples =
                        max(chorus->remaining_samples,
                             stage->delay_line_length);
        }

        effp->out_signal.length = SOX_UNKNOWN_LEN;
        /* TODO: calculate actual length */

        return (SOX_SUCCESS);
}

/*--------------------*/

/**
 * Apply effect <C>effp</C> on samples from input sample buffer
 * <C>ibuf</C> to output sample buffer <C>obuf</C> with sample counts
 * given by <C>isamp</C> and <C>osamp</C>.  When <C>ibuf</C> is NULL,
 * the drain phase must be processed and input is assumed to be 0.
 *
 * @param effp   chorus effect processing the samples
 * @param ibuf   sample buffer containing input samples (may be NULL)
 * @param obuf   target sample buffer for the output samples
 * @param isamp  count of samples in the input buffer or, when ibuf is
 *               NULL, the count of samples to be drained (to be updated)
 * @param osamp  count of sample slots in the output buffer (to be
 *               updated)
 * @return  information whether process has been successful
  */
static int sox_chorus_flow_or_drain (sox_effect_t *effp,
                                     const sox_sample_t *ibuf,
                                     sox_sample_t *obuf,
                                     size_t *isamp,
                                     size_t *osamp)
{
        chorus_priv_t *chorus = (chorus_priv_t *) effp->priv;
        const sox_bool is_drain = (ibuf == NULL);
        size_t len = min(*isamp, *osamp);
        int result = (!is_drain
                      ? SOX_SUCCESS
                      : (*isamp > *osamp ? SOX_SUCCESS : SOX_EOF));

        *isamp = *osamp = len;

        while (len--) {
                sox_uint32_t i;
                sox_sample_t output_sample;

                /* Scale samples down to prevent arithmetic overflow
                 * when adding up many delay lines */
                const chorus_delay_sample_t d_in =
                    (is_drain
                     ? 0
                     : (chorus_delay_sample_t) *ibuf++ / SCALING_FACTOR);

                /* Compute output */
                chorus_delay_sample_t d_out = d_in * chorus->gain_in;

                for (i = 0; i < chorus->stage_count; i++) {
                    chorus_stage_t *stage = &chorus->stage[i];
                    sox_uint32_t wave_index = stage->wave_index;
                    sox_uint32_t offset = stage->wave_table[wave_index];
                    sox_uint32_t delay_line_index =
                        ((stage->delay_line_index + offset)
                         % stage->delay_line_length);
                    chorus_delay_sample_t sample =
                        stage->delay_line[delay_line_index];
                    d_out += sample * stage->decay;
                    stage->delay_line[stage->delay_line_index] = d_in;
                    MODULAR_INCREMENT(stage->delay_line_index,
                                      stage->delay_line_length);
                    MODULAR_INCREMENT(stage->wave_index, stage->wave_length);
                }

                /* Adjust the output volume by gain_out, check for
                 * clipping and scale output up again */
                d_out = d_out * chorus->gain_out;
                output_sample = CLIP_COUNT_PROC((sox_sample_t) d_out,
                                                effp->clips);
                *obuf++ = output_sample * SCALING_FACTOR;
        }

        return result;
}

/*--------------------*/

/**
 * Apply effect <C>effp</C> on samples from input sample buffer
 * <C>ibuf</C> to output sample buffer <C>obuf</C> with sample counts
 * given by <C>isamp</C> and <C>osamp</C>.
 *
 * @param effp   chorus effect processing the samples
 * @param ibuf   sample buffer containing input samples
 * @param obuf   target sample buffer for the output samples
 * @param isamp  count of samples in the input buffer (to be updated)
 * @param osamp  count of sample slots in the output buffer (to be
 *               updated)
 * @return number of samples processed
 */
static int sox_chorus_flow (sox_effect_t *effp,
                            const sox_sample_t *ibuf,
                            sox_sample_t *obuf,
                            size_t *isamp,
                            size_t *osamp)
{
    return sox_chorus_flow_or_drain(effp, ibuf, obuf, isamp, osamp);
}

/*--------------------*/

/**
 * Drain out effect delay lines for effect <C>effp</C> to output
 * sample buffer <C>obuf</C> with sample count given by <C>osamp</C>.
 *
 * @param effp   chorus effect processing the samples
 * @param obuf   target sample buffer for the output samples
 * @param osamp  count of sample slots in the output buffer (to be
 *               updated)
 * @return number of samples processed
 */
static int sox_chorus_drain (sox_effect_t * effp,
                             sox_sample_t *obuf,
                             size_t *osamp)
{
    chorus_priv_t *chorus = (chorus_priv_t *) effp->priv;
    int result = SOX_EOF;

    if (chorus->remaining_samples > 0) {
        size_t dummy = chorus->remaining_samples;
        result = sox_chorus_flow_or_drain(effp, NULL, obuf,
                                          &dummy, osamp);
        chorus->remaining_samples -= dummy;
    }

    return result;
}

/*--------------------*/

/**
 * Clean up chorus effect <C>effp</C>.
 *
 * @param effp   chorus effect to be cleaned up
 */
static int sox_chorus_stop (sox_effect_t * effp)
{
        chorus_priv_t * chorus = (chorus_priv_t *) effp->priv;
        sox_uint32_t i;

        for (i = 0;  i < chorus->stage_count;  i++) {
                chorus_stage_t *stage = &chorus->stage[i];
                free(stage->wave_table);
                free(stage->delay_line);
        }
        free(chorus->stage);

        return (SOX_SUCCESS);
}

const sox_effect_handler_t *lsx_chorus_effect_fn(void)
{
  static char const usage[] =
"gain-in gain-out <delay decay speed depth -s|-t>";
  static char const * const extra_usage[] = {
"                                              ___",
"In---+-------------------------------------->|   |",
"     |     _________              * gain-in  |   |",
"     |    |         |                        |   |",
"     +--->| delay 1 |----------------------->|   |",
"     |    |_________|             * decay 1  |   |",
"     |         ^                             |   |",
"     :         | * depth 1                   |   |",
"     : +---------------+                     | + |------------>Out",
"     : | sine/triangle |<--speed 1           |   | * gain-out",
"     : +---------------+                     |   |",
"     |     _________                         |   |",
"     |    |         |                        |   |",
"     +--->| delay n |----------------------->|   |",
"          |_________|              * decay n |   |",
"               ^                             |___|",
"               | * depth n",
"       +---------------+",
"       | sine/triangle |<--speed n",
"       +---------------+",
"",
"         RANGE TYPICAL DESCRIPTION",
"gain-in  -1-1    0.5   Proportion of input delivered clean to the adder",
"gain-out -1-1     1    Final volume adjustment",
"delay   0-1000  40-60  Fixed delay in milliseconds",
"decay    -1-1    0.5   Proportion of delay's output delivered to the adder",
"speed   0-192k   0.25  Modulation frequency (no more than the sample rate)",
"depth   0-1000    2    Additional variable delay in milliseconds",
"-s                     Modulate sinusoidally",
"-t                     Modulate triangularly",
"Hint: gain-out <= 1 / ( gain-in + decay 1 + ... + decay n )",
          NULL
	};

        static sox_effect_handler_t sox_chorus_effect = {
                "chorus",
                usage, extra_usage,
                SOX_EFF_LENGTH | SOX_EFF_GAIN,
                sox_chorus_getopts,
                sox_chorus_start,
                sox_chorus_flow,
                sox_chorus_drain,
                sox_chorus_stop,
                NULL,
                sizeof(chorus_priv_t)
        };

        return &sox_chorus_effect;
}
