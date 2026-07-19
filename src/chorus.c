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

/** the downscaling factor for the samples in the delay line
 * to prevent overflow; best if it's a power of 2 */
#define SCALING_FACTOR 256

/** the maximum number of stages in a chorus, because of SCALING_FACTOR */
#define MAX_STAGE_COUNT SCALING_FACTOR

/** the allowed options for the modulation kind mapped onto a wave
 * type */
static lsx_enum_item modulation_kind_map[] ={
    { "-sine",     SOX_WAVE_SINE },
    { "-triangle", SOX_WAVE_TRIANGLE },
    { NULL, 0 }
};



/** the allowed interpolation types, mirroring those of flanger */
typedef enum {INTERP_NONE, INTERP_LINEAR, INTERP_QUADRATIC} interp_t;

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
        int                    *wave_table_i;
        float                  *wave_table_f;
} chorus_stage_t;

/*--------------------*/

/** the chorus effect */
typedef struct {
        /* command line parameters */
        interp_t        interpolation;
        float           gain_in;
        float           gain_out;

        /* all stages of that effect */
        sox_uint32_t    stage_count;
        chorus_stage_t  *stage;

        /* remaining samples for drain phase */
        sox_uint32_t    remaining_samples;
        lsx_wave_t      default_wave;  /* If -t was given */
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

        while (argc > 0 && argv[0][0] == '-') {
          switch (argv[0][1]) {
          case 'n':
              chorus->interpolation = INTERP_NONE;
              argc--; argv++;
              break;
          case 'l':
              chorus->interpolation = INTERP_LINEAR;
              argc--; argv++;
              break;
          case 'q':
              chorus->interpolation = INTERP_QUADRATIC;
              argc--; argv++;
              break;
          case 's':
              chorus->default_wave = SOX_WAVE_SINE;
              argc--; argv++;
              break;
          case 't':
              chorus->default_wave = SOX_WAVE_TRIANGLE;
              argc--; argv++;
              break;
          default:
              lsx_fail("invalid option  '%s'", argv[0]);
              return lsx_usage(effp);
          }
        }

        /* read the global parameters gain_in and gain_out */
        chorus->gain_in = 0.5;
        chorus->gain_out = 1;
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

            p->delay = 50;
            p->decay = 0.5;
            p->speed = 0.25;
            p->depth = 2;
            p->wave_type = chorus->default_wave;
            if (argc > 0) NUMERIC_PARAMETER(delay,  0, 86400000);
            if (argc > 0) NUMERIC_PARAMETER(decay, -1, 1);
            if (argc > 0) NUMERIC_PARAMETER(speed,  0, 192000);
            if (argc > 0) NUMERIC_PARAMETER(depth,  0, 86400000);
            if (argc > 0) TEXTUAL_PARAMETER(wave_type, modulation_kind_map);

            /* normalize time parameters to seconds */
            p->delay /= 1000.0;
            p->depth /= 1000.0;
            chorus->stage_count++;
        } while (argc > 0 && chorus->stage_count < MAX_STAGE_COUNT);

        if (argc > 0) {
            if (chorus->stage_count == MAX_STAGE_COUNT)
                lsx_fail("there is a maximum of %d stages", MAX_STAGE_COUNT);
            else
                lsx_fail("invalid argument `%s'", *argv);
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

static char * get_chorus(sox_effect_t *effp, char *name)
{
  chorus_priv_t *chorus = (chorus_priv_t *)effp->priv;
  char *s = NULL;

  if (!strcmp(name, "gain_in")) {
    s = lsx_malloc(32);
    sprintf(s, "%g", chorus->gain_in);
  }
  if (!strcmp(name, "gain_out")) {
    s = lsx_malloc(32);
    sprintf(s, "%g", chorus->gain_out);
  }

  return s;
}

static char *
set_chorus(sox_effect_t *effp, char *name, char *value)
{
  chorus_priv_t *chorus = (chorus_priv_t *)effp->priv;
  char *s = NULL;
  char *endptr = value;

  if (!strcmp(name, "gain-in")) {
    double gain = lsx_strtod(value, &endptr);
    if (endptr == value || *endptr != '\0') return NULL;
    chorus->gain_in = gain;
    s = malloc(32);
    sprintf(s, "%g", gain);
  }
  if (!strcmp(name, "gain-out")) {
    double gain = lsx_strtod(value, &endptr);
    if (endptr == value || *endptr != '\0') return NULL;
    chorus->gain_out = gain;
    s = malloc(32);
    sprintf(s, "%g", gain);
  }
  return s;
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
		    lsx_fail("delay + depth can't be more than %.0f ms at sample rate %.0fHz",
			     SOX_UINT_MAX(32) / effp->in_signal.rate * 1000,
			     effp->in_signal.rate);
		    lsx_fail("lower sr to increase the maximum depth of the delay");
		    return SOX_EOF;
		}
                stage->delay_line_length = dll;
		if (stage->delay_line_length < 1) {
		    lsx_fail("delay can't be less than %g milliseconds",
			     1000 / effp->in_signal.rate);
		    return SOX_EOF;
		}
                switch (chorus->interpolation) {
                case INTERP_NONE:
                  break;
                case INTERP_LINEAR:
                  stage->delay_line_length += 1;  /* Need 0 to n, i.e. n + 1 */
                  break;
                case INTERP_QUADRATIC:
                  stage->delay_line_length += 2;  /* Quadratic needs one more */
                  break;
                }
                stage->delay_line =
                    lsx_calloc(stage->delay_line_length,
                               sizeof(chorus_delay_sample_t));

                /* modulation wave table */
                stage->wave_length = effp->in_signal.rate / stage->speed + 0.5;
                if (stage->wave_length < 1) {
                    lsx_fail("speed can't be more than the sample rate");
                    return SOX_EOF;
                }
                switch (chorus->interpolation) {
                case INTERP_NONE:
                    lsx_valloc(stage->wave_table_i, stage->wave_length);
                    lsx_generate_wave_table(stage->wave_type, SOX_INT,
                        stage->wave_table_i,
                        stage->wave_length,
                        stage->delay * effp->in_signal.rate,
                        (stage->delay + stage->depth) * effp->in_signal.rate,
                        3 * M_PI_2);
                    break;
                case INTERP_LINEAR:
                case INTERP_QUADRATIC:
                    lsx_valloc(stage->wave_table_f, stage->wave_length);
                    lsx_generate_wave_table(stage->wave_type, SOX_FLOAT,
                        stage->wave_table_f,
                        stage->wave_length,
                        stage->delay * effp->in_signal.rate,
                        (stage->delay + stage->depth) * effp->in_signal.rate,
                        3 * M_PI_2);
                    break;
                }

                /* find maximum delay line length across all stages */
                chorus->remaining_samples =
                        max(chorus->remaining_samples,
                             stage->delay_line_length);
        }

        effp->out_signal.length = effp->in_signal.length;
        if (effp->out_signal.length != SOX_UNKNOWN_LEN)
          effp->out_signal.length += chorus->remaining_samples;

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

	switch (chorus->interpolation) {
	case INTERP_NONE:
            while (len--) {
                sox_uint32_t i;

                /* Scale samples down to prevent arithmetic overflow
                 * when adding up many delay lines.
                 *
                 * Dividing by scale_factor rounds the positive and
                 * negative halves of the wave towards zero, thereby
                 * crushing the wave towards zero by an average of half
                 * a sample value at the scaled resolution.
                 * The correction is *ibuf + (*ibuf < 0 ? -128 : +128) but
                 * this can overflow, and the scaled resolution is 24-bit
                 * so the error is negligable.
                 */
                const chorus_delay_sample_t d_in =
                    (is_drain
                     ? 0
                     : (chorus_delay_sample_t) *ibuf++ / SCALING_FACTOR);

                /* Compute output */
                chorus_delay_sample_t d_out = d_in * chorus->gain_in;

		for (i = 0; i < chorus->stage_count; i++) {
		    chorus_stage_t *stage = &chorus->stage[i];
		    sox_uint32_t wave_index = stage->wave_index;
		    sox_uint32_t offset_i = stage->wave_table_i[wave_index];
		    sox_uint32_t delay_line_index =
			((stage->delay_line_index + stage->delay_line_length - offset_i)
			 % stage->delay_line_length);
		    chorus_delay_sample_t sample;

		    stage->delay_line[stage->delay_line_index] = d_in;
		    sample = stage->delay_line[delay_line_index];
		    d_out += sample * stage->decay;
		    MODULAR_INCREMENT(stage->delay_line_index,
				      stage->delay_line_length);
		    MODULAR_INCREMENT(stage->wave_index, stage->wave_length);
	       }

                /* Adjust the output volume by gain_out, scale output up again
		 * and check for clipping */
                *obuf++ = SOX_ROUND_CLIP_COUNT(d_out * chorus->gain_out * SCALING_FACTOR, effp->clips);
             }
	     break;
         case INTERP_LINEAR:
            while (len--) {
                sox_uint32_t i;

                /* Scale samples down to prevent arithmetic overflow
                 * when adding up many delay lines.
                 */
                const chorus_delay_sample_t d_in =
                    (is_drain
                     ? 0
                     : (chorus_delay_sample_t) *ibuf++ / SCALING_FACTOR);

                /* Compute output */
                chorus_delay_sample_t d_out = d_in * chorus->gain_in;

		for (i = 0; i < chorus->stage_count; i++) {
		    chorus_stage_t *stage = &chorus->stage[i];
		    sox_uint32_t wave_index = stage->wave_index;
		    double       offset_f = stage->wave_table_f[wave_index];
		    sox_uint32_t offset_i = offset_f;
		    double       frac     = offset_f - offset_i;
		    sox_uint32_t delay_line_index =
			((stage->delay_line_index + stage->delay_line_length - offset_i)
			 % stage->delay_line_length);
		    chorus_delay_sample_t delayed_0, delayed_1;
		    chorus_delay_sample_t sample;

		    stage->delay_line[stage->delay_line_index] = d_in;
		    delayed_0 = stage->delay_line[delay_line_index];
		    delayed_1 = stage->delay_line[(delay_line_index + stage->delay_line_length - 1) % stage->delay_line_length];
		    sample = delayed_0 * (1 - frac) + delayed_1 * frac;

		    d_out += sample * stage->decay;
		    MODULAR_INCREMENT(stage->delay_line_index,
				      stage->delay_line_length);
		    MODULAR_INCREMENT(stage->wave_index, stage->wave_length);
		}

                /* Adjust the output volume by gain_out, scale output up again
		 * and check for clipping */
                *obuf++ = SOX_ROUND_CLIP_COUNT(d_out * chorus->gain_out * SCALING_FACTOR, effp->clips);
            }
	    break;
         case INTERP_QUADRATIC:
            while (len--) {
                sox_uint32_t i;

                /* Scale samples down to prevent arithmetic overflow
                 * when adding up many delay lines.
                 */
                const chorus_delay_sample_t d_in =
                    (is_drain
                     ? 0
                     : (chorus_delay_sample_t) *ibuf++ / SCALING_FACTOR);

                /* Compute output */
                chorus_delay_sample_t d_out = d_in * chorus->gain_in;

		for (i = 0; i < chorus->stage_count; i++) {
		    chorus_stage_t *stage = &chorus->stage[i];
		    sox_uint32_t wave_index = stage->wave_index;
		    double       offset_f = stage->wave_table_f[wave_index];
		    sox_uint32_t offset_i = offset_f;
		    double       frac     = offset_f - offset_i;
		    sox_uint32_t delay_line_index =
			((stage->delay_line_index + stage->delay_line_length - offset_i)
			 % stage->delay_line_length);
		    chorus_delay_sample_t delayed_0, delayed_1, delayed_2;
		    chorus_delay_sample_t sample;

		    stage->delay_line[stage->delay_line_index] = d_in;
		    delayed_0 = stage->delay_line[delay_line_index];
		    delayed_1 = stage->delay_line[(delay_line_index + stage->delay_line_length - 1) % stage->delay_line_length];
		    delayed_2 = stage->delay_line[(delay_line_index + stage->delay_line_length - 2) % stage->delay_line_length];

		    {
		      double a, b;
		      delayed_2 -= delayed_0;
		      delayed_1 -= delayed_0;
		      a = delayed_2 *.5 - delayed_1;
		      b = delayed_1 * 2 - delayed_2 *.5;
		      sample = delayed_0 + (a * frac + b) * frac;
		    }

		    d_out += sample * stage->decay;
		    MODULAR_INCREMENT(stage->delay_line_index,
				      stage->delay_line_length);
		    MODULAR_INCREMENT(stage->wave_index, stage->wave_length);
		}

                /* Adjust the output volume by gain_out, scale output up again
		 * and check for clipping */
                *obuf++ = SOX_ROUND_CLIP_COUNT(d_out * chorus->gain_out * SCALING_FACTOR, effp->clips);
            }
	    break;
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
		switch (chorus->interpolation) {
		case INTERP_NONE:      free(stage->wave_table_i); break;
		case INTERP_LINEAR:
		case INTERP_QUADRATIC: free(stage->wave_table_f); break;
		}
                free(stage->delay_line);
        }
        free(chorus->stage);

        return (SOX_SUCCESS);
}

const sox_effect_handler_t *lsx_chorus_effect_fn(void)
{
  static char const usage[] =
"[-n|-l|-q] [-s|-t] [gain-in [gain-out {delay [decay [speed [depth [-s|-t]]]]}]]";
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
"OPTION   RANGE DEFAULT DESCRIPTION",
"interp -n|-l|-q  -n    Interpolation type: none, linear or quadratic",
"gain-in  -1-1    0.5   Proportion of input delivered clean to the adder",
"gain-out -1-1     1    Final volume adjustment",
"delay   0-1000   50    Fixed delay in milliseconds",
"decay    -1-1    0.5   Proportion of delay's output delivered to the adder",
"speed   0-192k   0.25  Modulation frequency (no more than the sample rate)",
"depth   0-1000    2    Additional variable delay in milliseconds",
"wave     -s|-t   -s    Modulate with a sinusoidal or a triangular wave",
"Hint: gain-out <= 1 / ( gain-in + decay 1 + ... + decay n )",
"Keymaps: chorus.(gain_in|gain_out)",
          NULL
	};

        static sox_effect_handler_t sox_chorus_effect = {
                "chorus",
                usage,
                SOX_EFF_LENGTH | SOX_EFF_GAIN,
                sox_chorus_getopts,
                sox_chorus_start,
                sox_chorus_flow,
                sox_chorus_drain,
                sox_chorus_stop,
                NULL,
                sizeof(chorus_priv_t),
                extra_usage,
                get_chorus,
                set_chorus,
        };

        return &sox_chorus_effect;
}
