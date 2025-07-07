/* libSoX Echo effect             August 24, 1998
 *
 * Copyright (C) 1998 Juergen Mueller And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained.
 * Juergen Mueller And Sundry Contributors are not responsible for
 * the consequences of using this software.
 */

#include "sox_i.h"

/*
 * It's faster to use floats that sox_sample_t because the
 * conversion to/from float to apply decay at every sample
 * outweighs the advantage of the fixed-point arithmetic.
 */

/* Private data */
typedef struct {
        int     counter;
        int     num_delays;
        float   *delay_buf;
        float   gain_in, gain_out;
        float   *delay, *decay;
        ptrdiff_t *samples, maxsamples;
        size_t fade_out;
} priv_t;

/*
 * Process options
 */
static int sox_echo_getopts(sox_effect_t * effp, int argc, char **argv)
{
        priv_t * echo = (priv_t *) effp->priv;
        int i;

        echo->num_delays = 0;
        echo->delay = echo->decay = NULL;

        --argc, ++argv;
        if ((argc < 4) || (argc % 2))
          return lsx_usage(effp);

        i = 0;
        if (sscanf(argv[i], "%f", &echo->gain_in) != 1) {
                lsx_fail("gain-in `%s` is not a number", argv[i]);
		return (SOX_EOF);
	}
	i++;
        if (sscanf(argv[i], "%f", &echo->gain_out) != 1) {
                lsx_fail("gain-out `%s` is not a number", argv[i]);
		return (SOX_EOF);
	}
	i++;
        while (i < argc - 1) {
		float delay, decay;

                if (sscanf(argv[i], "%f", &delay) != 1) {
			lsx_fail("delay `%s` is not a number", argv[i]);
			return (SOX_EOF);
		}
                if (delay < 0 || !isfinite(delay)) {
			lsx_fail("delays must be positive");
			return (SOX_EOF);
		}
		i++;
                if (sscanf(argv[i], "%f", &decay) != 1) {
			lsx_fail("decay `%s` is not a number", argv[i]);
			return (SOX_EOF);
		}
		i++;

                echo->num_delays++;
		lsx_revalloc(echo->delay, echo->num_delays);
		lsx_revalloc(echo->decay, echo->num_delays);
		echo->delay[echo->num_delays - 1] = delay;
		echo->decay[echo->num_delays - 1] = decay;
        }
        return (SOX_SUCCESS);
}

/*
 * Prepare for processing.
 */
static int sox_echo_start(sox_effect_t * effp)
{
        priv_t * echo = (priv_t *) effp->priv;
        int i;
        float sum_in_volume;

        echo->maxsamples = 0;
	lsx_vcalloc(echo->samples, echo->num_delays);
        for ( i = 0; i < echo->num_delays; i++ ) {
                echo->samples[i] = echo->delay[i] * effp->in_signal.rate / 1000.0;
                if ( echo->samples[i] > echo->maxsamples )
                        echo->maxsamples = echo->samples[i];
        }
	echo->delay_buf = NULL;	/* Provoke segfault if accessed */
	if (echo->maxsamples > 0) {
                echo->delay_buf = lsx_calloc(echo->maxsamples,
                                             sizeof(*(echo->delay_buf)));
		/* calloc() sets the memory to zero */
	}
        sum_in_volume = echo->gain_in;
        for ( i = 0; i < echo->num_delays; i++ )
                sum_in_volume += echo->decay[i];
        if ( fabsf(sum_in_volume * echo->gain_out) > 1.0 )
                lsx_warn("the output may saturate; a safe gain-out is %g",
		         fabsf(1.0f / sum_in_volume));
        echo->counter = 0;
        echo->fade_out = echo->maxsamples;

  effp->out_signal.length = SOX_UNKNOWN_LEN; /* TODO: calculate actual length */

        return (SOX_SUCCESS);
}

/*
 * Processed signed long samples from ibuf to obuf.
 * Return number of samples processed.
 */
static int sox_echo_flow(sox_effect_t * effp, const sox_sample_t *ibuf, sox_sample_t *obuf,
                 size_t *isamp, size_t *osamp)
{
        priv_t * echo = (priv_t *) effp->priv;
        int j;
        float d_in, d_out;
        size_t len = min(*isamp, *osamp);
        *isamp = *osamp = len;

        while (len--) {
                d_in = (float) *ibuf++;
                /* Compute output first */
                d_out = d_in * echo->gain_in;
		if (echo->maxsamples == 0) {
			for ( j = 0; j < echo->num_delays; j++ ) {
				d_out += d_in * echo->decay[j];
			}
		} else {
			for ( j = 0; j < echo->num_delays; j++ ) {
				d_out += echo->delay_buf[
	(echo->counter + echo->maxsamples - echo->samples[j]) % echo->maxsamples]
				* echo->decay[j];
			}
		}
                /* Adjust the output volume and size to 24 bit */
                d_out = d_out * echo->gain_out;
                *obuf++ = SOX_ROUND_CLIP_COUNT(d_out, effp->clips);
                /* Store input in delay buffer */
                if (echo->maxsamples > 0) {
		        echo->delay_buf[echo->counter] = d_in;
			/* Adjust the counter */
			echo->counter = ( echo->counter + 1 ) % echo->maxsamples;
		}
        }
        /* processed all samples */
        return (SOX_SUCCESS);
}

/*
 * Drain out reverb lines.
 */
static int sox_echo_drain(sox_effect_t * effp, sox_sample_t *obuf, size_t *osamp)
{
        priv_t * echo = (priv_t *) effp->priv;
        float d_in, d_out;
        int j;
        size_t done;

        done = 0;
        /* drain out delay samples */
        while ( ( done < *osamp ) && ( done < echo->fade_out ) ) {
                d_in = 0;
                d_out = 0;
		if (echo->maxsamples > 0) {
		    for ( j = 0; j < echo->num_delays; j++ ) {
			    d_out += echo->delay_buf[
    (echo->counter + echo->maxsamples - echo->samples[j]) % echo->maxsamples]
			    * echo->decay[j];
		    }
		}
                /* Adjust the output volume and size to 24 bit */
                d_out = d_out * echo->gain_out;
                *obuf++ = SOX_ROUND_CLIP_COUNT(d_out, effp->clips);
                /* Store input in delay buffer */
                echo->delay_buf[echo->counter] = d_in;
                /* Adjust the counters */
                echo->counter = ( echo->counter + 1 ) % echo->maxsamples;
                done++;
                echo->fade_out--;
        };
        /* samples played, it remains */
        *osamp = done;
        if (echo->fade_out == 0)
            return SOX_EOF;
        else
            return SOX_SUCCESS;
}

static int sox_echo_stop(sox_effect_t * effp)
{
        priv_t * echo = (priv_t *) effp->priv;

        /* free per-channel data */
        free(echo->samples);
        free(echo->delay_buf);
        return (SOX_SUCCESS);
}

static int sox_echo_kill(sox_effect_t * effp)
{
        priv_t * echo = (priv_t *) effp->priv;

        /* free per-effect data */
        free(echo->delay);
        free(echo->decay);
        return (SOX_SUCCESS);
}

const sox_effect_handler_t *lsx_echo_effect_fn(void)
{
  static const char usage[] = "gain-in gain-out <delay decay>";
  static char const * const extra_usage[] = {
"                               ___",
"In--+------------------------>|   |",
"    |    _________  * gain-in |   |",
"    |   |         |           |   |",
"    +-->| delay 1 |---------->|   |",
"    |   |_________| * decay 1 |   |",
"    |    _________            | + |------------>Out",
"    |   |         |           |   | * gain-out",
"    +-->| delay 2 |---------->|   |",
"    |   |_________| * decay 2 |   |",
"    :    _________            |   |",
"    |   |         |           |   |",
"    +-->| delay n |---------->|___|",
"        |_________| * decay n",
"",
"           RANGE   DESCRIPTION",
"gain-in  -inf-inf  Proportion of input signal delivered clean to adder",
"gain-out -inf-inf  Final volume adjustment",
"delay       0-inf  Delay in milliseconds",
"decay    -inf-inf  Proportion of delayed signal delivered to adder",
    NULL
  };

  static sox_effect_handler_t handler = {
    "echo", usage, extra_usage, SOX_EFF_LENGTH | SOX_EFF_GAIN,
    sox_echo_getopts,
    sox_echo_start,
    sox_echo_flow,
    sox_echo_drain,
    sox_echo_stop,
    sox_echo_kill, sizeof(priv_t)
  };

  return &handler;
}
