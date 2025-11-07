/* libSoX effect: Stereo Flanger   (c) 2006 robs@users.sourceforge.net
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

/* TODO: Slide in the delay at the start? */

#include "sox_i.h"

typedef enum {INTERP_LINEAR, INTERP_QUADRATIC} interp_t;

#define MAX_CHANNELS 4

typedef struct {
  /* Parameters */
  double     delay;
  double     depth;
  double     regen;
  double     width;
  double     speed;
  lsx_wave_t  wave_shape;
  double     phase;
  interp_t   interpolation;

  /* Delay buffers */
  double **  delay_bufs;
  size_t  delay_buf_length;
  size_t  delay_buf_pos;
  double  *  delay_last;

  /* Low Frequency Oscillator */
  float *    lfo;
  size_t  lfo_length;
  size_t  lfo_pos;

  /* Balancing */
  double     gain_in;
} priv_t;



static lsx_enum_item const interp_enum[] = {
  LSX_ENUM_ITEM(INTERP_,LINEAR)
  LSX_ENUM_ITEM(INTERP_,QUADRATIC)
  {0, 0}};



static int getopts(sox_effect_t * effp, int argc, char *argv[])
{
  priv_t * p = (priv_t *) effp->priv;
  --argc, ++argv;

  /* Set non-zero defaults: */
  p->depth = 2;
  p->width = 71;
  p->speed = 0.5;
  p->phase = 25;

  do { /* break-able block */
    NUMERIC_PARAMETER(delay, 0   , 1000 )
    NUMERIC_PARAMETER(depth, 0   , 1000 )
    NUMERIC_PARAMETER(regen,-100 , 100 )
    NUMERIC_PARAMETER(width, 0   , INFINITY )
    NUMERIC_PARAMETER(speed, 0   , 192000 )
    TEXTUAL_PARAMETER(wave_shape, lsx_get_wave_enum())
    NUMERIC_PARAMETER(phase, 0   , 100)
    TEXTUAL_PARAMETER(interpolation, interp_enum)
  } while (0);

  if (argc != 0)
    return lsx_usage(effp);

  lsx_report("parameters:\n"
      "delay = %gms\n"
      "depth = %gms\n"
      "regen = %g%%\n"
      "width = %g%%\n"
      "speed = %gHz\n"
      "shape = %s\n"
      "phase = %g%%\n"
      "interp= %s",
      p->delay,
      p->depth,
      p->regen,
      p->width,
      p->speed,
      lsx_get_wave_enum()[p->wave_shape].text,
      p->phase,
      interp_enum[p->interpolation].text);

  /* Scale to unity: */
  p->regen /= 100;
  if (isfinite(p->width)) p->width /= 100;
  p->phase /= 100;
  p->delay /= 1000;
  p->depth /= 1000;

  return SOX_SUCCESS;
}



static int start(sox_effect_t * effp)
{
  priv_t * f = (priv_t *) effp->priv;
  int c, channels = effp->in_signal.channels;

  lsx_valloc(f->delay_bufs, channels);
  lsx_valloc(f->delay_last, channels);

  /* Balance output */
  if (isfinite(f->width)) {
    f->gain_in = 1 / (1 + f->width);
    f->width  /= 1 + f->width;
  } else {
    f->gain_in = 0;
    f->width   = 1;
  }

  /* Balance feedback loop */
  f->width *= 1 - fabs(f->regen);

  lsx_debug("gain_in=%g regen=%g width=%g\n",
      f->gain_in, f->regen, f->width);

  /* Create the delay buffers, one for each channel: */
  f->delay_buf_length = (f->delay + f->depth) * effp->in_signal.rate;
  if (f->delay_buf_length < 1) {
    lsx_fail("delay+depth can't be less than %g", 1000 / effp->in_signal.rate);
    return SOX_EOF;
  }
  ++f->delay_buf_length;  /* Need 0 to n, i.e. n + 1. */
  ++f->delay_buf_length;  /* Quadratic interpolator needs one more. */
  for (c = 0; c < channels; ++c)
    lsx_vcalloc(f->delay_bufs[c], f->delay_buf_length);

  /* Create the LFO lookup table: */
  f->lfo_length = effp->in_signal.rate / f->speed;
  if (f->lfo_length < 1) {
    lsx_fail("speed can't be more that the sample rate");
    return SOX_EOF;
  }
  lsx_vcalloc(f->lfo, f->lfo_length);
  lsx_generate_wave_table(
      f->wave_shape,
      SOX_FLOAT,
      f->lfo,
      f->lfo_length,
      floor(f->delay * effp->in_signal.rate + .5),
      f->delay_buf_length - 2.,
      3 * M_PI_2);  /* Start the sweep at minimum delay (for mono at least) */

  lsx_debug("delay_buf_length=%" PRIuPTR " lfo_length=%" PRIuPTR "\n",
      f->delay_buf_length, f->lfo_length);

  return SOX_SUCCESS;
}



static int flow(sox_effect_t * effp, sox_sample_t const * ibuf,
    sox_sample_t * obuf, size_t * isamp, size_t * osamp)
{
  priv_t * f = (priv_t *) effp->priv;
  int c, channels = effp->in_signal.channels;
  size_t len = (*isamp > *osamp ? *osamp : *isamp) / channels;

  *isamp = *osamp = len * channels;

  while (len--) {
    f->delay_buf_pos =
      (f->delay_buf_pos + f->delay_buf_length - 1) % f->delay_buf_length;
    for (c = 0; c < channels; ++c) {
      double delayed_0, delayed_1;
      double delayed;
      double in, out;
      size_t phase = c * f->lfo_length * f->phase + .5;
      double delay = f->lfo[(f->lfo_pos + phase) % f->lfo_length];
      double frac_delay = modf(delay, &delay);
      size_t int_delay = (size_t)delay;

      in = *ibuf++;
      f->delay_bufs[c][f->delay_buf_pos] = in + f->delay_last[c] * f->regen;

      delayed_0 = f->delay_bufs[c]
        [(f->delay_buf_pos + int_delay++) % f->delay_buf_length];
      delayed_1 = f->delay_bufs[c]
        [(f->delay_buf_pos + int_delay++) % f->delay_buf_length];

      if (f->interpolation == INTERP_LINEAR)
        delayed = delayed_0 + (delayed_1 - delayed_0) * frac_delay;
      else /* if (f->interpolation == INTERP_QUADRATIC) */
      {
        double a, b;
        double delayed_2 = f->delay_bufs[c]
          [(f->delay_buf_pos + int_delay++) % f->delay_buf_length];
        delayed_2 -= delayed_0;
        delayed_1 -= delayed_0;
        a = delayed_2 *.5 - delayed_1;
        b = delayed_1 * 2 - delayed_2 *.5;
        delayed = delayed_0 + (a * frac_delay + b) * frac_delay;
      }

      f->delay_last[c] = delayed;
      out = in * f->gain_in + delayed * f->width;
      *obuf++ = SOX_ROUND_CLIP_COUNT(out, effp->clips);
    }
    f->lfo_pos = (f->lfo_pos + 1) % f->lfo_length;
  }

  return SOX_SUCCESS;
}



static int stop(sox_effect_t * effp)
{
  priv_t * f = (priv_t *) effp->priv;
  int c, channels = effp->in_signal.channels;

  for (c = 0; c < channels; ++c)
    free(f->delay_bufs[c]);
  free(f->delay_bufs);
  free(f->delay_last);
  free(f->lfo);

  memset(f, 0, sizeof(*f));

  return SOX_SUCCESS;
}



sox_effect_handler_t const * lsx_flanger_effect_fn(void)
{
  static const char usage[] =
"[delay [depth [regen [width [speed [shape [phase [interp]]]]]]]";
  static char const * const extra_usage[] = {
"",
"            +----------------+",
"            |    * regen     |",
"           _V_     _______   |            ___",
"          |   |   |       |  |           |   |",
"    +---->| + |-->| delay |--+---------->|   |",
"    |     |___|   |_______| * width/100  |   |",
"    |                 ^                  |   |",
"In  |                 | * depth          |   |               Out",
"--->+         +---------------+          | + |------------------>",
"    | speed-->| sine/triangle |          |   | / (1 + width/100)",
"    |         +---------------+          |   |",
"    |                                    |   |",
"    +----------------------------------->|   |",
"                                         |___|",
"        RANGE DEFAULT DESCRIPTION",
"delay  0-1000    0    base delay in milliseconds",
"depth  0-1000    2    added swept delay in milliseconds",
"regen -100-100   0    percentage regeneration (delayed signal feedback)",
"width   0-inf   71    percentage of delayed signal mixed with original",
"speed   0-192k  0.5   sweeps per second (no more than the sample rate)",
"shape    s|t   sine   swept wave shape: sine|triangle",
"phase   0-100   25    percent phase shift of swept wave in multichannel flange",
"                      0 = 100 = same phase on each channel",
"interp   l|q  linear  delay-line interpolation: linear|quadratic",
    NULL
  };

  static sox_effect_handler_t handler = {
    "flanger", usage, extra_usage, SOX_EFF_MCHAN,
    getopts, start, flow, NULL, stop, NULL, sizeof(priv_t)};

  return &handler;
}
