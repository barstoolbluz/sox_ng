/* libSoX statistics "effect" file.
 *
 * Compute various statistics on file and print them.
 *
 * Output is unmodified from input.
 *
 * July 5, 1991
 * Copyright 1991 Lance Norskog And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained.
 * Lance Norskog And Sundry Contributors are not responsible for
 * the consequences of using this software.
 */

#include "sox_i.h"

#if HAVE_EBUR128_H
# if EXTERNAL_EBUR128
#  include <ebur128.h>
# else
#  include "../libebur128/ebur128.h"
# endif
#endif

/* Private data for stat effect */
typedef struct {
  double min, max, mid;
  double asum;
  double sum1, sum2;            /* amplitudes */
  double dmin, dmax;
  double dsum1, dsum2;          /* deltas */
  double scale;                 /* scale-factor */
  double last;                  /* previous sample */
  uint64_t read;               /* samples processed */
  int volume;
  int srms;
  int fft;
  float *re_in;
  float *re_out;
  unsigned long fft_size;
  unsigned long fft_offset;
  sox_bool fft_average;
  sox_bool json;
#if HAVE_EBUR128_H
  sox_bool ebur128;             /* Was the -e flag given? */
  ebur128_state *ebur128_state;
  sox_bool ebur128_histogram;
#endif
} priv_t;


/*
 * Process options
 */
static int sox_stat_getopts(sox_effect_t * effp, int argc, char **argv)
{
  priv_t * stat = (priv_t *) effp->priv;

  stat->scale = SOX_SAMPLE_MAX;

  --argc, ++argv;
  for (; argc > 0; argc--, argv++) {
    if (!(strcmp(*argv, "-v")))
      stat->volume = 1;
    else if (!(strcmp(*argv, "-s"))) {
      if (argc <= 1) {
        lsx_fail("-s what?");
        return SOX_EOF;
      }
      argc--, argv++;              /* Move to next argument. */
      if (!sscanf(*argv, "%lf", &stat->scale)) {
        lsx_fail("cannot parse scale `%s'", *argv);
        return SOX_EOF;
      }
    } else if (!(strcmp(*argv, "-rms")))
      stat->srms = 1;
    else if (!(strcmp(*argv, "-freq")))
      stat->fft = 1;
    else if (!(strcmp(*argv, "-d")))
      stat->volume = 2;
    else if (!(strcmp(*argv, "-a")))
      stat->fft_average = sox_true;
    else if (!(strcmp(*argv, "-j")))
      stat->json = sox_true;
#if HAVE_EBUR128_H
    else if (!(strcmp(*argv, "-e")))
      stat->ebur128 = sox_true;
    else if (!(strcmp(*argv, "-h")))
      stat->ebur128_histogram = sox_true;
#endif
    else {
      lsx_fail("invalid option `%s'", *argv);
      return lsx_usage(effp);
    }
  }

  return SOX_SUCCESS;
}

/*
 * Prepare processing.
 */
static int sox_stat_start(sox_effect_t * effp)
{
  priv_t * stat = (priv_t *) effp->priv;

  stat->min = stat->max = stat->mid = 0;
  stat->asum = 0;
  stat->sum1 = stat->sum2 = 0;

  stat->dmin = stat->dmax = 0;
  stat->dsum1 = stat->dsum2 = 0;

  stat->last = 0;
  stat->read = 0;

  stat->fft_size = 4096;
  stat->fft_average = sox_false;
  stat->re_in = stat->re_out = NULL;

  if (stat->fft) {
    stat->fft_offset = 0;
    lsx_valloc(stat->re_in, stat->fft_size);
    lsx_valloc(stat->re_out, stat->fft_size / 2 + 1);
  }

#if HAVE_EBUR128_H
  if (stat->ebur128) {
    stat->ebur128_state = ebur128_init(
      (unsigned int)effp->in_signal.channels,
      (unsigned long)(effp->in_signal.rate + .5),
      EBUR128_MODE_M | EBUR128_MODE_S |
      EBUR128_MODE_I | EBUR128_MODE_TRUE_PEAK |
      (stat->ebur128_histogram ? EBUR128_MODE_HISTOGRAM : 0));
    if (stat->ebur128_state == NULL) {
      lsx_warn("initialization of libebur128 failed");
      stat->ebur128 = sox_false;
    }
  }
#endif

  return SOX_SUCCESS;
}

/*
 * Print power spectrum to given stream
 */
static void print_power_spectrum(unsigned samples, double rate, float *re_in, float *re_out)
{
  float ffa = rate / samples;
  unsigned i;

  lsx_power_spectrum_f((int)samples, re_in, re_out);
  for (i = 0; i < samples / 2; i++) /* FIXME: should be <= samples / 2 */
    fprintf(stderr, "%f  %f\n", ffa * i, re_out[i]);
}

/*
 * Processed signed long samples from ibuf to obuf.
 * Return number of samples processed.
 */
static int sox_stat_flow(sox_effect_t * effp, const sox_sample_t *ibuf, sox_sample_t *obuf,
                        size_t *isamp, size_t *osamp)
{
  priv_t * stat = (priv_t *) effp->priv;
  int done, x, len = min(*isamp, *osamp);
  short count = 0;
  float *re_average = NULL;
  unsigned samples = 0;
  float ffa = 0.0;
  unsigned i;

  if (stat->fft_average) {
      samples = (stat->fft_size / 2);
      ffa = effp->in_signal.rate / samples;
      lsx_valloc(re_average, samples);
  }

  if (len) {
    if (stat->read == 0)          /* 1st sample */
      stat->min = stat->max = stat->mid = stat->last = (*ibuf)/stat->scale;

    if (stat->fft) {
      for (x = 0; x < len; x++) {
        SOX_SAMPLE_LOCALS;
        stat->re_in[stat->fft_offset++] = SOX_SAMPLE_TO_FLOAT_32BIT(ibuf[x], effp->clips);

        if (stat->fft_offset >= stat->fft_size) {
          stat->fft_offset = 0;
          if (stat->fft_average) {
              lsx_power_spectrum_f((int)samples, stat->re_in, stat->re_out);
              for (i = 0; i < samples / 2; i++) /* FIXME: should be <= samples / 2 */
                  re_average[i] += stat->re_out[i];
          } else {
          print_power_spectrum((unsigned) stat->fft_size, effp->in_signal.rate, stat->re_in, stat->re_out);
          }
        }

      }
      if (stat->fft_average) {
          for (i = 0; i < samples / 2; i++) /* FIXME: should be <= samples / 2 */
              fprintf(stderr, " %f  %f\n", ffa * i, re_average[i] / len);
      }
    }

#if HAVE_EBUR128_H
    if (stat->ebur128)
      ebur128_add_frames_int(stat->ebur128_state, (int const *)ibuf,
                           len / effp->in_signal.channels);
#endif
    for (done = 0; done < len; done++) {
      long lsamp = *ibuf++;
      double delta, samp = (double)lsamp / stat->scale;
      *obuf++ = lsamp;

      if (stat->volume == 2) {
          fprintf(stderr,"%08lx ",lsamp);
          if (count++ == 5) {
              fprintf(stderr,"\n");
              count = 0;
          }
      }

      /* update min/max */
      if (stat->min > samp)
        stat->min = samp;
      else if (stat->max < samp)
        stat->max = samp;
      stat->mid = stat->min / 2 + stat->max / 2;

      stat->sum1 += samp;
      stat->sum2 += samp*samp;
      stat->asum += fabs(samp);

      delta = fabs(samp - stat->last);
      if (delta < stat->dmin)
        stat->dmin = delta;
      else if (delta > stat->dmax)
        stat->dmax = delta;

      stat->dsum1 += delta;
      stat->dsum2 += delta*delta;

      stat->last = samp;
    }
    stat->read += len;
  }

  free(re_average);
  *isamp = *osamp = len;
  /* Process all samples */

  return SOX_SUCCESS;
}

/*
 * Process tail of input samples.
 */
static int sox_stat_drain(sox_effect_t * effp, sox_sample_t *obuf UNUSED, size_t *osamp)
{
  priv_t * stat = (priv_t *) effp->priv;

  /* When we run out of samples, then we need to pad buffer with
   * zeros and then run FFT one last time to process any unprocessed
   * samples.
   */
  if (stat->fft && stat->fft_offset) {
    unsigned int x;

    for (x = stat->fft_offset; x < stat->fft_size; x++)
      stat->re_in[x] = 0;

    print_power_spectrum((unsigned) stat->fft_size, effp->in_signal.rate, stat->re_in, stat->re_out);
  }

  *osamp = 0;
  return SOX_EOF;
}

/*
 * Do anything required when you stop reading samples.
 * Don't close input file!
 */
static int sox_stat_stop(sox_effect_t * effp)
{
  priv_t * stat = (priv_t *) effp->priv;
  double amp, scale, rms = 0, freq;
  double ct;
#if HAVE_EBUR128_H
  double momentary, short_term, integrated, true_peak = -INFINITY;
#endif

  ct = stat->read;

  if (stat->srms) {  /* adjust results to units of rms */
    double f;
    rms = sqrt(stat->sum2/ct);
    f = 1.0/rms;
    stat->max *= f;
    stat->min *= f;
    stat->mid *= f;
    stat->asum *= f;
    stat->sum1 *= f;
    stat->sum2 *= f*f;
    stat->dmax *= f;
    stat->dmin *= f;
    stat->dsum1 *= f;
    stat->dsum2 *= f*f;
    stat->scale *= rms;
  }

  scale = stat->scale;

  amp = -stat->min;
  if (amp < stat->max)
    amp = stat->max;

#if HAVE_EBUR128_H
  if (stat->ebur128) {
    if (ebur128_loudness_momentary(stat->ebur128_state, &momentary)
        != EBUR128_SUCCESS) momentary = -INFINITY;
    if (ebur128_loudness_shortterm(stat->ebur128_state, &short_term)
        != EBUR128_SUCCESS) short_term = -INFINITY;
    if (ebur128_loudness_global(stat->ebur128_state, &integrated)
        != EBUR128_SUCCESS) integrated = -INFINITY;
    {
      unsigned int channel;
      double loudness;

      for (channel = 0; channel < effp->in_signal.channels; channel++)
        if (ebur128_true_peak(stat->ebur128_state, channel, &loudness)
            == EBUR128_SUCCESS && loudness > true_peak)
          true_peak = loudness;
    }
  }
#endif

  if (stat->json) {
    fprintf(stderr, "{\n");
    fprintf(stderr, "  \"samples_read\": %" PRIu64 ",\n", stat->read);
    fprintf(stderr, "  \"length\": %g,\n", (double)stat->read/effp->in_signal.rate/effp->in_signal.channels);
    if (stat->srms)
      fprintf(stderr, "  \"scaled_by_rms\": %g,\n", rms);
    else
      fprintf(stderr, "  \"scaled_by\": %g,\n", scale);
    fprintf(stderr, "  \"maximum_amplitude\": %g,\n", stat->max);
    fprintf(stderr, "  \"minimum_amplitude\": %g,\n", stat->min);
    fprintf(stderr, "  \"midline_amplitude\": %g,\n", stat->mid);
    if (ct > 0) {
      fprintf(stderr, "  \"mean_norm\": %g,\n", stat->asum/ct);
      /* Mean amplitude for a symmetrical wave (e.g. synth sine) is
       * too precise and says -1.94025e-14. which is 1/24000th of a
       * 31-bit sample value (one bit is the sign bit) so round it
       * to the nearest 31-bit sample value so that 0 is 0. */
      fprintf(stderr, "  \"mean_amplitude\": %g,\n",
              round((stat->sum1/ct) * (1<<31)) / (1<<31));
      fprintf(stderr, "  \"rms_amplitude\": %g,\n", sqrt(stat->sum2/ct));
    }
    if (ct > 1) {
      fprintf(stderr, "  \"maximum_delta\": %g,\n", stat->dmax);
      fprintf(stderr, "  \"minimum_delta\": %g,\n", stat->dmin);
      fprintf(stderr, "  \"mean_delta\": %g,\n", stat->dsum1/(ct-1));
      fprintf(stderr, "  \"rms_delta\": %g,\n", sqrt(stat->dsum2/(ct-1)));
    }
#if HAVE_EBUR128_H
    if (stat->ebur128) {
      if (isfinite(momentary))
        fprintf(stderr, "  \"ebur128_momentary\": %f,\n", momentary);
      if (isfinite(short_term))
        fprintf(stderr, "  \"ebur128_short_term\": %f,\n", short_term);
      if (isfinite(integrated))
        fprintf(stderr, "  \"ebur128_integrated\": %f,\n", integrated);
    }
#endif
    freq = sqrt(stat->dsum2/stat->sum2)*effp->in_signal.rate/(M_PI*2);
    fprintf(stderr, "  \"rough_frequency\": %d,\n", (int)freq);
    if (amp>0)
      fprintf(stderr, "  \"volume_adjustment\": %g\n", SOX_SAMPLE_MAX/(amp*scale));
    fprintf(stderr, "}\n");
    goto out;
  }

  /* Just print the volume adjustment */
  if (stat->volume == 1 && amp > 0) {
    fprintf(stderr, "%.3f\n", SOX_SAMPLE_MAX/(amp*scale));
    return SOX_SUCCESS;
  }
  if (stat->volume == 2)
    fprintf(stderr, "\n\n");
  /* print out the info */
  fprintf(stderr, "Samples read:      %12" PRIu64 "\n", stat->read);
  fprintf(stderr, "Length (seconds):  %12.6f\n", (double)stat->read/effp->in_signal.rate/effp->in_signal.channels);
  if (stat->srms)
    fprintf(stderr, "Scaled by rms:     %12.6f\n", rms);
  else
    fprintf(stderr, "Scaled by:         %12.1f\n", scale);
  fprintf(stderr, "Maximum amplitude: %12.6f\n", stat->max);
  fprintf(stderr, "Minimum amplitude: %12.6f\n", stat->min);
  fprintf(stderr, "Midline amplitude: %12.6f\n", stat->mid);
  fprintf(stderr, "Mean    norm:      %12.6f\n", ct == 0 ? 0.0f : stat->asum/ct);
  fprintf(stderr, "Mean    amplitude: %12.6f\n", ct == 0 ? 0.0f : stat->sum1/ct);
  fprintf(stderr, "RMS     amplitude: %12.6f\n", ct == 0 ? 0.0f : sqrt(stat->sum2/ct));
  fprintf(stderr, "Maximum delta:     %12.6f\n", stat->dmax);
  fprintf(stderr, "Minimum delta:     %12.6f\n", stat->dmin);
  fprintf(stderr, "Mean    delta:     %12.6f\n", stat->dsum1/(ct-1));
  fprintf(stderr, "RMS     delta:     %12.6f\n", sqrt(stat->dsum2/(ct-1)));
#if HAVE_EBUR128_H
  if (stat->ebur128) {
    if (isfinite(momentary))
      fprintf(stderr, "EBUR128 Momentary: %12.6f\n", momentary);
    if (isfinite(short_term))
      fprintf(stderr, "EBUR128 Short term:%12.6f\n", short_term);
    if (isfinite(integrated))
      fprintf(stderr, "EBUR128 Integrated:%12.6f\n", integrated);
    if (isfinite(true_peak))
      fprintf(stderr, "EBUR128 True Peak: %12.6f\n", true_peak);
  }
#endif
  freq = sqrt(stat->dsum2/stat->sum2)*effp->in_signal.rate/(M_PI*2);
  fprintf(stderr, "Rough   frequency: %12d\n", (int)freq);

  if (amp>0)
    fprintf(stderr, "Volume adjustment: %12.3f\n", SOX_SAMPLE_MAX/(amp*scale));

out:
  /* Release FFT memory */
  free(stat->re_in);
  free(stat->re_out);

#if HAVE_EBUR128_H
  if (stat->ebur128) ebur128_destroy(&stat->ebur128_state);
#endif

  return SOX_SUCCESS;

}

static char const usage[] = "[-s scale] [-rms] [-freq] [-v] [-d] [-a]"
#if HAVE_EBUR128_H
" [-e] [-h]"
#endif
" [-j]";
static char const * const extra_usage[] = {
  "-s     Scale the input data by a factor",
  "-rms   Convert all average values to root mean square",
  "-freq  Output the input's power spectrum (a 4096-point DFT)",
  "-v     Output only the `Volume Adjustment' value",
  "-d     Output a hex dump of the 32-bit signed PCM audio data",
  "-a     Output the average power spectrum",
#if HAVE_EBUR128_H
  "-e     Include EBU R 128 loudness figures",
  "-h     Use the histogram algorithm for integrated EBU R 128 loudness",
#endif
  "-j     Output the statistics in JSON format instead of plain text",
  NULL
};

static sox_effect_handler_t sox_stat_effect = {
  "stat",
  usage,
  SOX_EFF_MCHAN | SOX_EFF_MODIFY,
  sox_stat_getopts,
  sox_stat_start,
  sox_stat_flow,
  sox_stat_drain,
  sox_stat_stop,
  NULL,
  sizeof(priv_t),
  extra_usage,
  NULL,
  NULL,
};

const sox_effect_handler_t *lsx_stat_effect_fn(void)
{
  return &sox_stat_effect;
}
