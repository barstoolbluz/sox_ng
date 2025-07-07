/* libSoX effect: Soft volume control
 *
 * Copyright 2025 Martin Guy <martinwguy@gmail.com>
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or (at
 * your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "sox_i.h"

/* Private data for effect */
typedef struct {
  float softvol;
  float double_time;	  /* How long we take to double the volume */
  float mult_per_sample;  /* How much to multiply softvol by per sample frame
  			   * to make it double in double_time seconds
			   */
  sox_sample_t max_amp;	  /* Don't go beyond this value */
} priv_t;

/* Remember if there is a softvol in action so that volume keys can change it */
static priv_t * softvol_priv = NULL;

/* Make 'v' and 'V' keys adjust the softvol if one is active */
int
lsx_adjust_softvol(int delta)
{
  if (!softvol_priv) return SOX_EOF;
  softvol_priv->softvol *= (100 + delta) / 100.0;
  return SOX_SUCCESS;
}

/*
 * Process command-line options but don't do other
 * initialization now: effp->in_signal & effp->out_signal are not
 * yet filled in.
 */
static int getopts(sox_effect_t * effp, int argc, char UNUSED **argv)
{
  priv_t *p = (priv_t *)effp->priv;
  float headroom = 0.0;

  p->softvol = 1.0;
  p->double_time = 0.0;

  /* Initial value */
  if (argc > 1) {
    if (sscanf(argv[1], "%f", &p->softvol) != 1 ||
        p->softvol < 0.0) {
      lsx_fail("invalid volume multiplier `%s'", argv[1]);
      return SOX_EOF;
    }
  }
  argv++; argc--;

  /* The time over which to double the volume in seconds */
  if (argc > 1) {
    if (sscanf(argv[1], "%f", &p->double_time) != 1 ||
	p->double_time < 0.0) {
      lsx_fail("invalid doubling time `%s'", argv[1]);
      return SOX_EOF;
    }
    argv++; argc--;
  }

  /* Headroom to allow below SOX_SAMPLE_MAX in dB */
  if (argc > 1) {
    if (sscanf(argv[1], "%f", &headroom) != 1 ||
        headroom < 0.0f) {
      lsx_fail("invalid headroom `%s'", argv[1]);
      return SOX_EOF;
    }
    argv++; argc--;
  }
  p->max_amp = SOX_SAMPLE_MAX * dB_to_linear(-headroom);

  if (argc > 1) return lsx_usage(effp);

  softvol_priv = p;

  return SOX_SUCCESS;
}

/*
 * Prepare processing.
 * Do all initializations.
 */
static int start(sox_effect_t * effp)
{
  priv_t *p = (priv_t *)effp->priv;

  if (p->double_time != 0)
    p->mult_per_sample = pow(2.0, 1.0 /
                             (p->double_time * effp->in_signal.rate));

  return SOX_SUCCESS;
}

/*
 * Process up to *isamp samples from ibuf and produce up to *osamp samples
 * in obuf.  Write back the actual numbers of samples to *isamp and *osamp.
 * Return SOX_SUCCESS or, if error occurs, SOX_EOF.
 */
static int flow(sox_effect_t * effp, const sox_sample_t *ibuf, sox_sample_t *obuf,
                           size_t *isamp, size_t *osamp)
{
  priv_t *p = (priv_t *)effp->priv;
  size_t len;		/* Number of sample frames to process */
  size_t done;
  sox_sample_t const *iptr = ibuf;
  sox_sample_t       *optr = obuf;
  unsigned chans = effp->in_signal.channels;
  SOX_SAMPLE_LOCALS;

  len = min(*isamp, *osamp) / chans;
  for (done = 0; done < len; done++)
  {
    unsigned chan;
    sox_sample_t maxamp;

    /* What is the maximum amplitude over all channels? */
    maxamp = 0;
    for (chan = 0; chan < chans; chan++) {
      sox_sample_t amp = abs(iptr[chan]);
      if (amp > maxamp) maxamp = amp;
    }

    /* If it would exceed maximum volume, lower softvol so that it doesn't. */
    if (maxamp * p->softvol > p->max_amp) {
      p->softvol = p->max_amp / maxamp;
    }

    for (chan = 0; chan < chans; chan++)
      *optr++ = *iptr++ * p->softvol;

    /* If we're slowly raising the volume, do so for the next sample frame */
    if (p->double_time != 0.0f)
      p->softvol *= p->mult_per_sample;
  }
  *osamp = *isamp = done * chans;

  return SOX_SUCCESS;
}

/*
 * Drain out remaining samples if the effect generates any.
 */
static int drain(sox_effect_t UNUSED * effp, sox_sample_t UNUSED *obuf, size_t *osamp)
{
  *osamp = 0;
  /* Return SOX_EOF when drain
   * will not output any more samples.
   * *osamp == 0 also indicates that.
   */
  return SOX_EOF;
}

/*
 * Do anything required when you stop reading samples.
 */
static int stop(sox_effect_t UNUSED * effp)
{
  return SOX_SUCCESS;
}

/*
 * Do anything required when you kill an effect.
 *      (free allocated memory, etc.)
 */
static int lsx_kill(sox_effect_t UNUSED * effp)
{
  softvol_priv = NULL;
  return SOX_SUCCESS;
}

/*
 * Function returning effect descriptor. This should be the only
 * externally visible object.
 */
const sox_effect_handler_t *lsx_softvol_effect_fn(void)
{
  static const char usage[] = "[volume [double-time [headroom]]]";
  static sox_effect_handler_t handler = {
    "softvol", usage, NULL, SOX_EFF_MCHAN | SOX_EFF_GAIN,
    getopts, start, flow, drain, stop, lsx_kill, sizeof(priv_t)
  };
  return &handler;
}
