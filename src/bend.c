/* libSoX effect: Pitch Bend   (c) 2008 robs@users.sourceforge.net
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

/* Portions based on http://www.dspdimension.com/download smbPitchShift.cpp:
 *
 * COPYRIGHT 1999-2006 Stephan M. Bernsee <smb [AT] dspdimension [DOT] com>
 *
 *             The Wide Open License (WOL)
 *
 * Permission to use, copy, modify, distribute and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice and this license appear in all source copies. 
 * THIS SOFTWARE IS PROVIDED "AS IS" WITHOUT EXPRESS OR IMPLIED WARRANTY OF
 * ANY KIND. See http://www.dspguru.com/wol.htm for more information.
 */

#ifdef NDEBUG /* Enable assert always. */
#undef NDEBUG /* Must undef above assert.h or other that might include it. */
#endif

#include "sox_i.h"
#include "fft4g.h"

#define MAX_FRAME_LENGTH (FFT4G_MAX_SIZE / 2)

typedef struct {
  unsigned nbends;       /* Number of bends requested */
  struct {
    char *str;           /* Command-line argument to parse for this bend */
    uint64_t start;      /* Start bending when in_pos equals this */
    float cents;
    float cents_1200;    /* == cents/1200 */
    uint64_t duration;   /* Number of samples to bend */
  } *bends;

  unsigned frame_rate;
  size_t in_pos;         /* Number of samples read from the input stream */
  unsigned bends_pos;    /* Number of bends completed so far */

  float shift;

  float *gInFIFO;       /* [FRAME_LENGTH] */
  float *gOutFIFO;      /* [FRAME_LENGTH] */
  double *gFFTworksp;   /* [2 * FRAME_LENGTH] */
  float *gLastPhase;    /* [FRAME_LENGTH / 2 + 1] */
  float *gSumPhase;     /* [FRAME_LENGTH / 2 + 1] */
  float *gOutputAccum;  /* [2 * FRAME_LENGTH] */
  float *gAnaFreq;      /* [FRAME_LENGTH] */
  float *gAnaMagn;      /* [FRAME_LENGTH] */
  float *gSynFreq;      /* [FRAME_LENGTH] */
  float *gSynMagn;      /* [FRAME_LENGTH] */
  float *gWindow;       /* [FRAME_LENGTH] */
  unsigned gRover;
  unsigned fftFrameSize, over_sample;
} priv_t;

static int parse(sox_effect_t * effp, char **argv, sox_rate_t rate)
{
  priv_t *p = (priv_t *) effp->priv;
  unsigned i;
  char const *next;
  uint64_t last_seen = 0;
  const uint64_t in_length = argv ? 0 :
    (effp->in_signal.length != SOX_UNKNOWN_LEN ?
     effp->in_signal.length / effp->in_signal.channels : SOX_UNKNOWN_LEN);

  for (i = 0; i < p->nbends; ++i) {
    if (argv)   /* 1st parse only */
      p->bends[i].str = lsx_strdup(argv[i]);

    next = lsx_parseposition(rate, p->bends[i].str,
             argv ? NULL : &p->bends[i].start, last_seen, in_length, '+');
    last_seen = p->bends[i].start;
    if (next == NULL || *next != ',')
      break;

    {
      const char *oldnext = next;
      p->bends[i].cents = strtof(next + 1, (char **)&next);
      if (next == oldnext || !isfinite(p->bends[i].cents) ||
          fabsf(p->bends[i].cents) == HUGE_VAL || *next != ',')
        break;
    }
    p->bends[i].cents_1200 = p->bends[i].cents / 1200.f;

    next = lsx_parseposition(rate, next + 1,
             argv ? NULL : &p->bends[i].duration, last_seen, in_length, '+');
    last_seen = p->bends[i].duration;
    if (next == NULL || *next != '\0')
      break;

    /* sanity checks */
    if (!argv && p->bends[i].duration < p->bends[i].start) {
      lsx_fail("bend %u has negative width", i+1);
      return SOX_EOF;
    }
    if (!argv && i && p->bends[i].start < p->bends[i-1].start) {
      lsx_fail("bend %u overlaps with previous one", i+1);
      return SOX_EOF;
    }

    p->bends[i].duration -= p->bends[i].start;
  }
  if (i < p->nbends) {
    lsx_fail("cannot parse `%s' as start,cents,end", p->bends[i].str);
    return SOX_EOF;
  }
  return SOX_SUCCESS;
}

static int create_bend(sox_effect_t * effp, int argc, char **argv)
{
  priv_t *p = (priv_t *) effp->priv;
  char const * opts = "f:o:";
  int c;
  lsx_getopt_t optstate;
  lsx_getopt_init(argc, argv, opts, NULL, lsx_getopt_flag_none, 1, &optstate);

  p->frame_rate = 25;
  p->over_sample = 16;
  while ((c = lsx_getopt(&optstate)) != -1) switch (c) {
    GETOPT_NUMERIC(optstate, 'f', frame_rate, 10, 80)
    GETOPT_NUMERIC(optstate, 'o', over_sample, 4, 32)
    default: lsx_fail("invalid option `-%c'", optstate.opt);
             return lsx_usage(effp);
  }
  argc -= optstate.ind, argv += optstate.ind;

  p->nbends = argc;
  lsx_vcalloc(p->bends, p->nbends);
  return parse(effp, argv, 0.);     /* No rate yet; parse with dummy */
}

static int start_bend(sox_effect_t * effp)
{
  priv_t *p = (priv_t *) effp->priv;
  unsigned i;

  int n = effp->in_signal.rate / p->frame_rate + .5;
  for (p->fftFrameSize = 2; n > 2; p->fftFrameSize <<= 1, n >>= 1);
  if (p->fftFrameSize > MAX_FRAME_LENGTH) {
    lsx_fail("FFT frame size is too large (%d > %d)", p->fftFrameSize, MAX_FRAME_LENGTH);
    return SOX_EOF;
  }
  p->shift = 1;
  /* Re-parse now rate is known */
  if (parse(effp, 0, effp->in_signal.rate))
    return SOX_EFF_NULL;

  p->in_pos = p->bends_pos = 0;

  /* If none of the bends have any duration, we are a null effect */
  {
    sox_bool any_duration = sox_false;

    for (i = 0; i < p->nbends; ++i)
      if (p->bends[i].duration) {
        any_duration = sox_true;
        break;
      }
    if (!any_duration) return SOX_EFF_NULL;
  }

  lsx_vcalloc(p->gInFIFO,      p->fftFrameSize);
  lsx_vcalloc(p->gOutFIFO,     p->fftFrameSize);
  lsx_vcalloc(p->gFFTworksp,   2 * p->fftFrameSize);
  lsx_vcalloc(p->gLastPhase,   p->fftFrameSize / 2 + 1);
  lsx_vcalloc(p->gSumPhase,    p->fftFrameSize / 2 + 1);
  lsx_vcalloc(p->gOutputAccum, 2 * p->fftFrameSize);
  lsx_vcalloc(p->gAnaFreq,     p->fftFrameSize);
  lsx_vcalloc(p->gAnaMagn,     p->fftFrameSize);
  lsx_vcalloc(p->gSynFreq,     p->fftFrameSize);
  lsx_vcalloc(p->gSynMagn,     p->fftFrameSize);
  lsx_vcalloc(p->gWindow,      p->fftFrameSize);

  /* Precalculate the window function */
  { unsigned k;
    for (k = 0; k < p->fftFrameSize; k++)
      p->gWindow[k] = -.5f * cosf(2 * M_PI * k / (float) p->fftFrameSize) + .5f;
  }

  return SOX_SUCCESS;
}

static int flow_bend(sox_effect_t * effp, const sox_sample_t * ibuf,
                     sox_sample_t * obuf, size_t * isamp, size_t * osamp)
{
  priv_t *p = (priv_t *) effp->priv;
  size_t i, len = *isamp = *osamp = min(*isamp, *osamp);
  float magn, phase, tmp, real, imag;
  float freqPerBin, expct;
  unsigned k, index, inFifoLatency, stepSize, fftFrameSize2;
  long qpd;
  float pitchShift = p->shift;

  /* set up some handy variables */
  fftFrameSize2 = p->fftFrameSize / 2;
  stepSize = p->fftFrameSize / p->over_sample;
  freqPerBin = effp->in_signal.rate / p->fftFrameSize;
  expct = (float)(2. * M_PI) * (float) stepSize / (float) p->fftFrameSize;
  inFifoLatency = p->fftFrameSize - stepSize;
  if (!p->gRover)
    p->gRover = inFifoLatency;

  /* main processing loop */
  for (i = 0; i < len; i++) {
    SOX_SAMPLE_LOCALS;
    ++p->in_pos;

    /* As long as we have not yet collected enough data just read in */
    p->gInFIFO[p->gRover] = SOX_SAMPLE_TO_FLOAT_32BIT(ibuf[i], effp->clips);
    obuf[i] = SOX_FLOAT_32BIT_TO_SAMPLE(
        p->gOutFIFO[p->gRover - inFifoLatency], effp->clips);
    p->gRover++;

    /* now we have enough data for processing */
    if (p->gRover >= p->fftFrameSize) {
      if (p->bends_pos != p->nbends && p->in_pos >=
          p->bends[p->bends_pos].start + p->bends[p->bends_pos].duration) {
        pitchShift = p->shift *= powf(2.f, p->bends[p->bends_pos].cents_1200);
        ++p->bends_pos;
      }
      if (p->bends_pos != p->nbends && p->in_pos >= p->bends[p->bends_pos].start) {
        float progress = (float)(p->in_pos - p->bends[p->bends_pos].start) /
                         (float)p->bends[p->bends_pos].duration;
        progress *= p->bends[p->bends_pos].cents_1200;
        pitchShift = p->shift * powf(2.f, progress);
      }

      p->gRover = inFifoLatency;

      /* do windowing and re,im interleave */
      for (k = 0; k < p->fftFrameSize; k++) {
        p->gFFTworksp[2 * k] = p->gInFIFO[k] * p->gWindow[k];
        p->gFFTworksp[2 * k + 1] = 0.;
      }

      /* ***************** ANALYSIS ******************* */
      lsx_safe_cdft(2 * p->fftFrameSize, 1, p->gFFTworksp);

      /* this is the analysis step */
      for (k = 0; k <= fftFrameSize2; k++) {
        /* de-interlace FFT buffer */
        real = p->gFFTworksp[2 * k];
        imag = - p->gFFTworksp[2 * k + 1];

        /* compute magnitude and phase */
        magn = 2.f * sqrtf(real * real + imag * imag);
        phase = atan2f(imag, real);

        /* compute phase difference */
        tmp = phase - p->gLastPhase[k];
        p->gLastPhase[k] = phase;

        tmp -= (float) k *expct; /* subtract expected phase difference */

        /* map delta phase into +/- Pi interval */
        qpd = tmp / (float)M_PI;
        if (qpd >= 0)
          qpd += qpd & 1;
        else qpd -= qpd & 1;
        tmp -= (float)M_PI * (float) qpd;

        /* get deviation from bin frequency from the +/- Pi interval */
        tmp = p->over_sample * tmp / (float)(2. * M_PI);

        /* compute the k-th partials' true frequency */
        tmp = (k + tmp) * freqPerBin;

        /* store magnitude and true frequency in analysis arrays */
        p->gAnaMagn[k] = magn;
        p->gAnaFreq[k] = tmp;

      }

      /* this does the actual pitch shifting */
      memset(p->gSynMagn, 0, p->fftFrameSize * sizeof(float));
      memset(p->gSynFreq, 0, p->fftFrameSize * sizeof(float));
      for (k = 0; k <= fftFrameSize2; k++) {
        index = k * pitchShift;
        if (index <= fftFrameSize2) {
          p->gSynMagn[index] += p->gAnaMagn[k];
          p->gSynFreq[index] = p->gAnaFreq[k] * pitchShift;
        }
      }

      for (k = 0; k <= fftFrameSize2; k++) { /* SYNTHESIS */
        /* get magnitude and true frequency from synthesis arrays */
        magn = p->gSynMagn[k], tmp = p->gSynFreq[k];
        tmp -= k * freqPerBin; /* subtract bin mid frequency */
        tmp /= freqPerBin; /* get bin deviation from freq deviation */
        tmp = 2.f * (float)M_PI * tmp / p->over_sample; /* take p->over_sample into account */
        tmp += k * expct; /* add the overlap phase advance back in */
        p->gSumPhase[k] += tmp; /* accumulate delta phase to get bin phase */
        phase = p->gSumPhase[k];
        /* get real and imag part and re-interleave */
        p->gFFTworksp[2 * k] = magn * cosf(phase);
        p->gFFTworksp[2 * k + 1] = - magn * sinf(phase);
      }

      for (k = p->fftFrameSize + 2; k < 2 * p->fftFrameSize; k++)
        p->gFFTworksp[k] = 0.; /* zero negative frequencies */

      lsx_safe_cdft(2 * p->fftFrameSize, -1, p->gFFTworksp);

      /* do windowing and add to output accumulator */
      for (k = 0; k < p->fftFrameSize; k++) {
        p->gOutputAccum[k] +=
            2.f * p->gWindow[k] * (float)p->gFFTworksp[2 * k] / (fftFrameSize2 * p->over_sample);
      }

      memcpy(p->gOutFIFO,     /* generate output */
             p->gOutputAccum, stepSize * sizeof(float));

      memmove(p->gOutputAccum, /* shift accumulator */
              p->gOutputAccum + stepSize, p->fftFrameSize * sizeof(float));

      memmove(p->gInFIFO,      /* move input FIFO */
              p->gInFIFO + stepSize, inFifoLatency * sizeof(float));
    }
  }
  return SOX_SUCCESS;
}

static int stop_bend(sox_effect_t * effp)
{
  priv_t *p = (priv_t *) effp->priv;

  if (p->bends_pos != p->nbends)
    lsx_warn("input audio too short; bends not applied: %u",
        p->nbends - p->bends_pos);
  return SOX_SUCCESS;
}

static int kill_bend(sox_effect_t * effp)
{
  priv_t *p = (priv_t *) effp->priv;
  unsigned i;

  free(p->gInFIFO);
  free(p->gOutFIFO);
  free(p->gFFTworksp);
  free(p->gLastPhase);
  free(p->gSumPhase);
  free(p->gOutputAccum);
  free(p->gAnaFreq);
  free(p->gAnaMagn);
  free(p->gSynFreq);
  free(p->gSynMagn);
  free(p->gWindow);

  for (i = 0; i < p->nbends; ++i)
    free(p->bends[i].str);
  free(p->bends);
  return SOX_SUCCESS;
}

static char const usage[] =
"[-f frame-rate] [-o over-sample] {start(+),cents,end(+)}";
static char const * const extra_usage[] = {
  "OPTION   RANGE  DEFAULT  DESCRIPTION",
  "-f rate  10-80    25     Frame rate",
  "-o ratio  4-32    16     Oversampling",
  NULL
};

sox_effect_handler_t const *lsx_bend_effect_fn(void)
{
  static sox_effect_handler_t handler = {
    "bend", usage, 0,
    create_bend, start_bend, flow_bend, NULL, stop_bend, kill_bend,
    sizeof(priv_t), extra_usage, NULL, NULL,
  };
  return &handler;
}
