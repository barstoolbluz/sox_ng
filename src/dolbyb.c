/* libSoX effect: Skeleton effect used as sample for creating new effects.
 *
 * Copyright 1999-2008 Chris Bagwell And SoX Contributors
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

#ifdef EXTERNAL_DOLBYB
#include <dolbyb.h>
#else
#include "../libdolbyb/dolbyb.h"
#endif

/* Private data for effect */
typedef struct {
  dolbyb_t dolbyb;
  sox_bool Encode;   /* Initialized to 0 = sox_false to decode by default */
} priv_t;

/*
 * Called once for each channel
 * Process command-line options but don't do other
 * initialization now: effp->in_signal & effp->out_signal are not
 * yet filled in.
 */
static int init(sox_effect_t * effp, int argc, char **argv)
{
  priv_t *p = (priv_t *)effp->priv;
  dolbyb_t *dolbyb = &(p->dolbyb);
  lsx_getopt_t optstate;
  int c;

  dolbyb_init(dolbyb);
  lsx_getopt_init(argc, argv, "dehu:t:a:f:", NULL, lsx_getopt_flag_none, 1, &optstate);

  while((c = lsx_getopt(&optstate)) != -1) switch (c) {
  case 'd': p->Encode = sox_false; break;
  case 'e': p->Encode = sox_true; break;
  case 'h': dolbyb->AllHig = sox_true; break;
  GETOPT_NUMERIC(optstate, 'u', dolbyb.UpSamp, 0, 100)
  GETOPT_NUMERIC(optstate, 't', dolbyb.ThGndB, -100.0, 100.0)
  GETOPT_NUMERIC(optstate, 'a', dolbyb.DecAdB, -100, 0.0)
  GETOPT_NUMERIC(optstate, 'f', dolbyb.FltTyp, 1, 4)
  }
  argc -= optstate.ind, argv -= optstate.ind;
  return argc ? lsx_usage(effp) : SOX_SUCCESS;
}

/*
 * Prepare processing.
 * Called once for each channel
 */
static int start(sox_effect_t * effp)
{
  dolbyb_t *dolbyb = &(((priv_t *)effp->priv)->dolbyb);

  if (effp->in_signal.channels > 2) {
    lsx_fail("can only do mono and stereo");
    return SOX_EOF;
  }
  dolbyb->NumChn = effp->in_signal.channels;
  dolbyb->SmpSec = effp->in_signal.rate;
  dolbyb->BDepth = 16;

  if (dolbyb_start(dolbyb)) return SOX_EOF;

  effp->out_signal.length = effp->in_signal.length;

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
  dolbyb_t *dolbyb = &(p->dolbyb);
  size_t nsamp;  /* How many samples to process */
  short *ismp, *osmp, *smpp;
  sox_sample_t const *issp; sox_sample_t *ossp;
  size_t i;

  /* SoX works in 32-bit signed samples and dolbyb only works with 16-bit
   * so we have to pack a copy for it and unpack its results. Grooh.
   */

  /* Do the minumum of *isamp and *osamp */
  nsamp = *isamp < *osamp ? *isamp : *osamp;

  ismp = malloc(sizeof(*ismp) * nsamp);
  osmp = malloc(sizeof(*osmp) * nsamp);
  if (!ibuf || !obuf) return SOX_EOF;

  for (i=0, smpp=ismp, issp=ibuf; i<nsamp; i++) {
    *smpp++ = (short)(*issp++ >> 16);
  }

  if (p->Encode)
    dolbyb_encode(dolbyb, ismp, osmp, nsamp / effp->in_signal.channels);
  else
    dolbyb_decode(dolbyb, ismp, osmp, nsamp / effp->in_signal.channels);

  for (i=0, ossp=obuf, smpp=osmp; i<nsamp; i++) {
    *ossp++ = (sox_sample_t)*smpp++ << 16;
  }

  *isamp = *osamp = nsamp;
  free(ismp);
  free(osmp);

  return SOX_SUCCESS;
}

static int drain(sox_effect_t UNUSED * effp, sox_sample_t UNUSED *obuf, size_t UNUSED *osamp)
{
  *osamp = 0;
  return SOX_SUCCESS;
}

/*
 * Called once per channel
 */
static int stop(sox_effect_t UNUSED * effp)
{
  return SOX_SUCCESS;
}

/*
 * Called once per effect
 */
static int lsx_kill(sox_effect_t UNUSED * effp)
{
  dolbyb_t *dolbyb = &(((priv_t *)effp->priv)->dolbyb);

  dolbyb_free(dolbyb);
  return SOX_SUCCESS;
}

/*
 * Function returning effect descriptor. This should be the only
 * externally visible object.
 */
const sox_effect_handler_t *lsx_dolbyb_effect_fn(void)
{
  static char const usage[] =
    "[-e|-d] [-u upsamp] [-h] [-t gain] [-a precision] [-f{1|2|3|4}]";
  static char const * const extra_usage[] = {
    "OPTION    RANGE  DEFAULT  DESCRIPTION",
    "-e                        Encode instead of decoding",
    "-d                        Decode (the default)",
    "-u upsamp  1-100          Upsample the sliding filter",
    "                          (default: right for >= 200kHz)",
    "-h                        Upsample everything",
    "-t gain -100-100    0     Adjust the sliding filter's threshold in dB",
    "-a prec -100-0     -5     Adjust the accuracy in dB when decoding",
    "-f n       1-4      4     Filter type: 1=original 2=better 3=worse 4=best",
    NULL
  };
  static sox_effect_handler_t sox_dolbyb_effect = {
    "dolbyb", usage, extra_usage, SOX_EFF_MCHAN,
    init, start, flow, drain, stop, lsx_kill, sizeof(priv_t)
  };
  return &sox_dolbyb_effect;
}
