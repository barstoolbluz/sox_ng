/* libSoX Echo effect             August 24, 1998
 *
 * Copyright (C) 1998 Juergen Mueller And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained.
 * Juergen Mueller And Sundry Contributors are not responsible for
 * the consequences of using this software.
 *
 *
 */

#include "sox_i.h"

/* Private data */

/* Instead of having a separate buffer for each delay, one huge buffer is
 * allocated and the individual delay lines are in it one after the other.
 * For delay i:
 * delay_buf[i] is the start of the echo's buffer
 * samples[i] is the size of its buffer, proportional to the delay time
 * counter[i] is the offset in delay_buf[i] of the next sample to output,
 *            that was written its delay time ago, and is also
 *            where to write new incoming data to be regurgitated
 *            <delay> microseconds (== samples[i] samples) in the future.
 */
typedef struct {
        int     *counter;
        int     num_delays;
        float   **delay_buf;
        float   gain_in, gain_out;
        float   *delay, *decay;
        ptrdiff_t *samples;
        size_t sumsamples;
} priv_t;

/*
 * Process options
 */
static int sox_echos_getopts(sox_effect_t * effp, int argc, char **argv)
{
        priv_t * echos = (priv_t *) effp->priv;
        int i;

        echos->num_delays = 0;
        echos->delay = echos->decay = NULL;

        --argc, ++argv;
        if ((argc < 4) || (argc % 2))
          return lsx_usage(effp);

        i = 0;
        sscanf(argv[i++], "%f", &echos->gain_in);
        sscanf(argv[i++], "%f", &echos->gain_out);
        while (i < argc) {
		float delay, decay;

                if (sscanf(argv[i], "%f", &delay) != 1) {
                        lsx_fail("delay `%s' is not a number", argv[i]);
                        return (SOX_EOF);
                }
                if (delay < 0 || !isfinite(delay)) {
                        lsx_fail("delays must be positive");
                        return (SOX_EOF);
                }
		i++;
                if (sscanf(argv[i], "%f", &decay) != 1) {
                        lsx_fail("decay `%s' is not a number", argv[i]);
                        return (SOX_EOF);
                }
		i++;

                echos->num_delays++;
		lsx_revalloc(echos->delay, echos->num_delays);
		lsx_revalloc(echos->decay, echos->num_delays);
                echos->delay[echos->num_delays - 1] = delay;
                echos->decay[echos->num_delays - 1] = decay;
        }
        return (SOX_SUCCESS);
}

/*
 * Prepare for processing.
 */
static int sox_echos_start(sox_effect_t * effp)
{
        priv_t * echos = (priv_t *) effp->priv;
        int i;
        float sum_in_volume;

	lsx_vcalloc(echos->counter, echos->num_delays);
	lsx_vcalloc(echos->samples, echos->num_delays);
	lsx_vcalloc(echos->delay_buf, echos->num_delays);
        echos->sumsamples = 0;
        for ( i = 0; i < echos->num_delays; i++ ) {
                echos->samples[i] = echos->delay[i] * effp->in_signal.rate / 1000.0;
                if ( echos->samples[i] < 1 ) {
                    lsx_fail("delays can't be less than %g milliseconds",
		             1000 / effp->in_signal.rate);
                    return (SOX_EOF);
                }
		echos->delay_buf[i] = lsx_calloc(echos->samples[i],
		                                 sizeof(*echos->delay_buf[i]));
	        /* calloc() returns the memory already zeroed */
                echos->counter[i] = 0;
                echos->sumsamples += echos->samples[i];
        }
        sum_in_volume = echos->gain_in;
        for ( i = 0; i < echos->num_delays; i++ )
                sum_in_volume += echos->decay[i];
        if ( fabsf(sum_in_volume * echos->gain_out) > 1.0 )
                lsx_warn("the output may saturate; a safe gain-out is %g",
                         1.0 / fabsf(sum_in_volume));

        effp->out_signal.length = SOX_UNKNOWN_LEN; /* TODO: calculate actual length */

        return (SOX_SUCCESS);
}

/*
 * Processed signed long samples from ibuf to obuf.
 * Return number of samples processed.
 */
static int sox_echos_flow(sox_effect_t * effp, const sox_sample_t *ibuf, sox_sample_t *obuf,
                size_t *isamp, size_t *osamp)
{
        priv_t * echos = (priv_t *) effp->priv;
        int j;
        float d_in, d_out;
        size_t len = min(*isamp, *osamp);
        *isamp = *osamp = len;

        while (len--) {
                /* Store delays as 24-bit signed longs */
                d_in = (float) *ibuf++;
                /* Compute output first */
                d_out = d_in * echos->gain_in;
                for ( j = 0; j < echos->num_delays; j++ ) {
                        d_out += echos->delay_buf[j][echos->counter[j]] * echos->decay[j];
                }
                /* Adjust the output volume and size to 24 bit */
                d_out = d_out * echos->gain_out;
                *obuf++ = SOX_ROUND_CLIP_COUNT(d_out, effp->clips);
                /* Mix decay of delays and input */
                for ( j = echos->num_delays - 1; j > 0; j-- ) {
		    echos->delay_buf[j][echos->counter[j]] =
		    echos->delay_buf[j-1][echos->counter[j-1]] + d_in;
                }
                echos->delay_buf[0][echos->counter[0]] = d_in;
                /* Adjust the counters */
                for ( j = 0; j < echos->num_delays; j++ )
                        echos->counter[j] =
                           ( echos->counter[j] + 1 ) % echos->samples[j];
        }
        /* processed all samples */
        return (SOX_SUCCESS);
}

/*
 * Drain out reverb lines.
 */
static int sox_echos_drain(sox_effect_t * effp, sox_sample_t *obuf, size_t *osamp)
{
        priv_t * echos = (priv_t *) effp->priv;
        float d_out;
        int j;
        size_t done;

        done = 0;
        /* drain out delay samples */
        while ( ( done < *osamp ) && ( done < echos->sumsamples ) ) {
                d_out = 0;
                for ( j = 0; j < echos->num_delays; j++ ) {
                        d_out += echos->delay_buf[j][echos->counter[j]] * echos->decay[j];
                }
                /* Adjust the output volume and size to 24 bit */
                d_out = d_out * echos->gain_out;
                *obuf++ = SOX_ROUND_CLIP_COUNT(d_out, effp->clips);
                /* Mix decay of delays and input */
                for ( j = echos->num_delays - 1; j > 0; j-- ) {
                        echos->delay_buf[j][echos->counter[j]] =
                        echos->delay_buf[j-1][echos->counter[j-1]];
                }
                echos->delay_buf[0][echos->counter[0]] = 0;
                /* Adjust the counters */
                for ( j = 0; j < echos->num_delays; j++ )
                        echos->counter[j] =
                           ( echos->counter[j] + 1 ) % echos->samples[j];
                done++;
                echos->sumsamples--;
        };
        /* samples played, it remains */
        *osamp = done;
        if (echos->sumsamples == 0)
            return SOX_EOF;
        else
            return SOX_SUCCESS;
}

/*
 * Clean up echos effect.
 */
static int sox_echos_stop(sox_effect_t * effp)
{
        priv_t * echos = (priv_t *) effp->priv;
	int i;

        free(echos->counter);
        free(echos->samples);
	for (i=0; i<echos->num_delays; i++)
	    free(echos->delay_buf[i]);
        free(echos->delay_buf);
        echos->delay_buf = NULL;
        return (SOX_SUCCESS);
}

const sox_effect_handler_t *lsx_echos_effect_fn(void)
{
  static const char usage[] = "gain-in gain-out <delay decay>";
  static char const * const extra_usage[] = {
"                                                         ___",
" In--+--------+-------------------+-------------------->|   |",
"     |        |                   |           * gain-in |   |",
" ____v___    _v_     ________    _v_     ________       |   |",
"|        |  |   |   |        |  |   |   |        |      |   |",
"| delay1 |  | + |-->| delay2 |  | + |-->| delayN |      |   |",
"|________|  |___|   |________|  |___|   |________|      |   | * gain-out",
"     |        ^          |        ^          |          | + |------------>",
"     |        |          |        |          |          |   |         Out",
"     |        |          |        |          +--------->|   |",
"     |        |          |        |           * decay N |   |",
"     |        |          +--------+-------------------->|   |",
"     |        |                               * decay 2 |   |",
"     +--------+---------------------------------------->|   |",
"                                              * decay 1 |___|",
"         RANGE  DESCRIPTION",
"gain-in   0-1   Proportion of input signal delivered clean to adder",
"gain-out  0-    Final volume adjustment",
"delay     0-    Delay in milliseconds",
"decay     0-1   Proportion of delayed signal delivered to adder",
"",
"When decay is close to 1.0, samples can clip and the output can saturate.",
"Hint: gain-out < 1 / (gain-in + decay1 + ... + decayN)",
    NULL
  };

  static sox_effect_handler_t handler = {
    "echos", usage, extra_usage, SOX_EFF_LENGTH | SOX_EFF_GAIN,
    sox_echos_getopts,
    sox_echos_start, sox_echos_flow, sox_echos_drain, sox_echos_stop,
    NULL, sizeof(priv_t)
  };

  return &handler;
}
