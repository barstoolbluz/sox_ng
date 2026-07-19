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
#include <ctype.h>   /* for isdigit() */

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
        unsigned num_delays;
        float  **delay_buf;
        float    gain_in, gain_out;
        float   *delay, *decay;
        ptrdiff_t *samples;
        size_t   sumsamples;
} priv_t;

/*
 * Process options
 */
static int echos_getopts(sox_effect_t * effp, int argc, char **argv)
{
        priv_t * echos = (priv_t *) effp->priv;
        char *endptr;
        int i;

        echos->num_delays = 0;
        echos->delay = echos->decay = NULL;

        --argc, ++argv;
        if (argc < 4) {
	  lsx_fail("gain_in, gain_out and one delay decay pair are required");
          return SOX_EOF;
	}
	if (argc % 2) {
	  lsx_fail("each delay requires a decay");
          return SOX_EOF;
	}

        i = 0;
        echos->gain_in = lsx_strtod(endptr = argv[i], &endptr);
        if (endptr == argv[i] || *endptr) {
          lsx_fail("gain-in `%s' is not a number", argv[i]);
          return SOX_EOF;
        }
        i++;
        echos->gain_out = lsx_strtod(endptr = argv[i], &endptr);
        if (endptr == argv[i] || *endptr) {
          lsx_fail("gain-out `%s' is not a number", argv[i]);
          return SOX_EOF;
        }
        i++;
        while (i < argc) {
		float delay, decay;

                delay = lsx_strtod(endptr = argv[i], &endptr);
                if (endptr == argv[i] || *endptr) {
                        lsx_fail("delay `%s' is not a number", argv[i]);
                        return (SOX_EOF);
                }
                if (delay < 0) {
                        lsx_fail("delays can't be negative");
                        return (SOX_EOF);
                }
		i++;

                decay = lsx_strtod(endptr = argv[i], &endptr);
                if (endptr == argv[i] || *endptr) {
                        lsx_fail("decay `%s' is not a number", argv[i]);
                        return (SOX_EOF);
                }
                if (decay < 0 || decay > 1) {
                        lsx_fail("decays must be from 0 to 1");
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

static char *
get_echos(sox_effect_t *effp, char *name)
{
  priv_t *p = (priv_t *)effp->priv;
  char *s = NULL;

  if (!strcmp(name, "gain_in")) {
    s = lsx_malloc(16);
    sprintf(s, "%g", p->gain_in);
  }
  if (!strcmp(name, "gain_out")) {
    s = lsx_malloc(16);
    sprintf(s, "%g", p->gain_out);
  }

  /* An array-based parameter */
  if (!strncmp(name, "decay", 5)) {
    unsigned i;
    unsigned nth = 0; /* 0 for "decay", non-zero for "decay1" etc. */

    if (isdigit((unsigned char)name[5])) {
      nth = atoi(name + 5);
      if (nth == 0) {
        lsx_warn("keymaps for individual decays start at 1");
        return NULL;
      }
    }

    for (i=0; i < p->num_delays; i++) {
      if (nth == 0 || nth == i+1) {
        /* If they ask for "decay" and there are several,
         * return the first one */
        s = lsx_malloc(16);
        sprintf(s, "%g", p->decay[i]);
        return s;
      }
    }
  }

  return s;
}

static char *
set_echos(sox_effect_t *effp, char *name, char *value)
{
  priv_t *p = (priv_t *)effp->priv;
  char *s = NULL;
  char *endptr = value;
  double v = lsx_strtod(value, &endptr);

  if (endptr == value || *endptr != '\0') return NULL;

  if (!strcmp(name, "gain_in")) {
    p->gain_in = v;
    s = lsx_malloc(16);
    sprintf(s, "%g", v);
  }
  if (!strcmp(name, "gain_out")) {
    p->gain_out = v;
    s = lsx_malloc(16);
    sprintf(s, "%g", v);
  }

  /* An array-based parameter */
  if (!strncmp(name, "decay", strlen("decay"))) {
    unsigned i;
    unsigned nth = 0; /* 0 for "decay", non-zero for "decay1" etc. */

    if (isdigit((unsigned char)name[5])) {
      nth = atoi(name + 5);
      if (nth == 0) {
        lsx_warn("keymaps for individual decays start at 1");
        return NULL;
      }
    }
    for (i=0; i < p->num_delays; i++) {
      if (nth == 0 || nth == i+1) {
        p->decay[i] = v;

        /* If we adjust several, return the last one,
         * after all, they'll all be the same */
        if (!s) s = lsx_malloc(16);
        sprintf(s, "%g", p->decay[i]);
      }
    }
  }
  return s;
}

/*
 * Prepare for processing.
 */
static int echos_start(sox_effect_t * effp)
{
        priv_t * echos = (priv_t *) effp->priv;
        unsigned i;
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

        if (effp->in_signal.length == SOX_UNKNOWN_LEN)
                effp->out_signal.length = SOX_UNKNOWN_LEN;
        else
                effp->out_signal.length =
                        effp->in_signal.length + echos->sumsamples;

        return (SOX_SUCCESS);
}

/*
 * Processed signed long samples from ibuf to obuf.
 * Return number of samples processed.
 */
static int echos_flow(sox_effect_t * effp, const sox_sample_t *ibuf, sox_sample_t *obuf,
                size_t *isamp, size_t *osamp)
{
        priv_t * echos = (priv_t *) effp->priv;
        unsigned j;
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
static int echos_drain(sox_effect_t * effp, sox_sample_t *obuf, size_t *osamp)
{
        priv_t * echos = (priv_t *) effp->priv;
        float d_out;
        unsigned j;
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
 * Clean up echos effect per-flow.
 */
static int echos_stop(sox_effect_t * effp)
{
        priv_t * echos = (priv_t *) effp->priv;
	unsigned i;

        free(echos->counter);
        free(echos->samples);
	for (i=0; i<echos->num_delays; i++)
	    free(echos->delay_buf[i]);
        free(echos->delay_buf);
        echos->delay_buf = NULL;
        return (SOX_SUCCESS);
}

/*
 * Clean up echos effect per-effect.
 */
static int echos_kill(sox_effect_t * effp)
{
        priv_t * echos = (priv_t *) effp->priv;

        free(echos->delay);
        free(echos->decay);
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
"           RANGE   DESCRIPTION",
"gain-in  -inf-inf  Proportion of input signal delivered clean to adder",
"gain-out -inf-inf  Final volume adjustment",
"delay       0-     Delay in milliseconds",
"decay       0-1    Proportion of delayed signal delivered to adder",
"",
"When decay is close to 1.0, samples can clip and the output can saturate.",
"Hint: gain-out < 1 / (gain-in + decay1 + ... + decayN)",
"Keymaps: echos.(gain_in|gain_out)",
    NULL
  };

  static sox_effect_handler_t handler = {
    "echos", usage, SOX_EFF_LENGTH | SOX_EFF_GAIN,
    echos_getopts,
    echos_start, echos_flow, echos_drain, echos_stop, echos_kill,
    sizeof(priv_t), extra_usage, get_echos, set_echos,
  };

  return &handler;
}
