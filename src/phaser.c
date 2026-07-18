/* SoX phaser effect
 * Copyright (C) 24 August 1998, Juergen Mueller And Sundry Contributors
 *
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained.
 * Juergen Mueller And Sundry Contributors are not responsible for
 * the consequences of using this software.
 */

#include "sox_i.h"

/** the allowed interpolation types, mirroring those of flanger */
typedef enum {INTERP_NONE, INTERP_LINEAR, INTERP_QUADRATIC} interp_t;

/** an auxiliary macro for doing a modular increment */
#define MODULAR_INCREMENT(a, b)     a = ((a) + 1) % (b)

typedef struct {
  interp_t   interpolation;
  double     gain_in, gain_out, delay, regen, speed;
  lsx_wave_t mod_type;

  int        * mod_buf_i;  /* Used when not interpolating */
  float      * mod_buf_f;  /* Used when interpolating */
  size_t     mod_buf_len;
  int        mod_pos;
            
  double     * delay_buf;
  size_t     delay_buf_len;
  int        delay_pos;
} priv_t;

static int getopts_phaser(sox_effect_t * effp, int argc, char * * argv)
{
  priv_t * p = (priv_t *) effp->priv;

  /* Set non-zero defaults: */
  p->gain_in   = .4;
  p->gain_out  = .74;
  p->delay  = 3.;
  p->regen     = .4;
  p->speed = .5;

  --argc, ++argv;  /* Skip the effect name */

  while (argc > 0 && argv[0][0] == '-') {
    switch (argv[0][1]) {
    case 'n': p->interpolation = INTERP_NONE; break;
    case 'l': p->interpolation = INTERP_LINEAR; break;
    case 'q': p->interpolation = INTERP_QUADRATIC; break;
    case 's': p->mod_type = SOX_WAVE_SINE; break;
    case 't': p->mod_type = SOX_WAVE_TRIANGLE; break;
    default: lsx_fail("invalid option `%s'", *argv);
             return lsx_usage(effp);
    }
    /* Ignore the rest of -sine -quad etc */
    argc--; argv++;
  }

  do { /* break-able block */
    NUMERIC_PARAMETER(gain_in  , -1, 1)
    NUMERIC_PARAMETER(gain_out , -1, 1)
    NUMERIC_PARAMETER(delay    ,  0, 1000)
    NUMERIC_PARAMETER(regen    , -1, 1)
    NUMERIC_PARAMETER(speed    ,  0, 192000)
  } while (0);

  if (argc && argv[0][0] == '-') {
    switch (argv[0][1]) {
    case 's': p->mod_type = SOX_WAVE_SINE; break;
    case 't': p->mod_type = SOX_WAVE_TRIANGLE; break;
    default:  lsx_fail("invalid option `%s'", *argv);
              return lsx_usage(effp);
    }
    --argc, ++argv;
  }

  if (p->gain_in > (1 - p->regen * p->regen))
    lsx_warn("warning: gain-in might cause clipping");
  if (p->gain_in / (1 - p->regen) > 1 / p->gain_out)
    lsx_warn("warning: gain-out might cause clipping");

  return argc? lsx_usage(effp) : SOX_SUCCESS;
}

static char *
get_phaser(sox_effect_t *effp, char *name)
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
  if (!strcmp(name, "regen")) {
    s = lsx_malloc(16);
    sprintf(s, "%g", p->regen);
  }

  return s;
}

static char *
set_phaser(sox_effect_t *effp, char *name, char *value)
{
  priv_t *p = (priv_t *)effp->priv;
  char *s = NULL;
  char *endptr = value;
  double v = lsx_strtod(value, &endptr);

  if (endptr == value || *endptr != '\0') return NULL;

  if (!strcmp(name, "gain_in")) {
    if (v > 1.0)  v = 1.0;
    if (v < -1.0) v = -1.0;
    p->gain_in = v;
    s = lsx_malloc(16);
    sprintf(s, "%g", v);
  }
  if (!strcmp(name, "gain_out")) {
    if (v > 1.0)  v = 1.0;
    if (v < -1.0) v = -1.0;
    p->gain_out = v;
    s = lsx_malloc(16);
    sprintf(s, "%g", v);
  }
  if (!strcmp(name, "regen")) {
    if (v > 1.0)  v = 1.0;
    if (v < -1.0) v = -1.0;
    p->regen = v;
    s = lsx_malloc(16);
    sprintf(s, "%g", v);
  }

  return s;
}

static int start_phaser(sox_effect_t * effp)
{
  priv_t * p = (priv_t *) effp->priv;

  p->delay_buf_len = p->delay * .001 * effp->in_signal.rate;
  if (p->delay_buf_len < 1) {
    lsx_fail("delay can't be less than %g", 1000 / effp->in_signal.rate);
    return SOX_EOF;
  }
  switch (p->interpolation) {
  case INTERP_NONE:
        break;
  case INTERP_LINEAR:
        p->delay_buf_len += 1;  /* Need 0 to n, i.e. n + 1. */
        break;
  case INTERP_QUADRATIC:
        p->delay_buf_len += 2;  /* Quadratic interpolator needs one more. */
        break;
  }
  lsx_vcalloc(p->delay_buf, p->delay_buf_len);

  p->mod_buf_len = effp->in_signal.rate / p->speed;
  if (p->mod_buf_len < 1) {
    lsx_fail("speed can't be more than %g", effp->in_signal.rate);
    return SOX_EOF;
  }
  switch (p->interpolation) {
  case INTERP_NONE:
    lsx_valloc(p->mod_buf_i, p->mod_buf_len);
    lsx_generate_wave_table(p->mod_type, SOX_INT, p->mod_buf_i, p->mod_buf_len,
                            1., (double)p->delay_buf_len, M_PI_2);
    break;
  case INTERP_LINEAR:
  case INTERP_QUADRATIC:
    lsx_valloc(p->mod_buf_f, p->mod_buf_len);
    lsx_generate_wave_table(p->mod_type, SOX_FLOAT, p->mod_buf_f, p->mod_buf_len,
                            1., (double)p->delay_buf_len, M_PI_2);
    break;
  }

  p->delay_pos = p->mod_pos = 0;

  effp->out_signal.length = effp->in_signal.length;
  return SOX_SUCCESS;
}

static int flow_phaser(sox_effect_t * effp, const sox_sample_t *ibuf,
    sox_sample_t *obuf, size_t *isamp, size_t *osamp)
{
  priv_t * p = (priv_t *) effp->priv;
  size_t len = *isamp = *osamp = min(*isamp, *osamp);

  switch (p->interpolation) {
  case INTERP_NONE:
    while (len--) {
      double d = *ibuf++ * p->gain_in + p->regen * p->delay_buf[
             (p->delay_pos + p->mod_buf_i[p->mod_pos]) % p->delay_buf_len];

      MODULAR_INCREMENT(p->mod_pos, p->mod_buf_len);
      MODULAR_INCREMENT(p->delay_pos, p->delay_buf_len);
      p->delay_buf[p->delay_pos] = d;

      *obuf++ = SOX_ROUND_CLIP_COUNT(d * p->gain_out, effp->clips);
    }
    break;

  case INTERP_LINEAR:
    while (len--) {
      float offset_f = p->mod_buf_f[p->mod_pos];
      int   offset_i = offset_f;  /* == floorf() */
      float frac = offset_f - offset_i;
      sox_uint32_t delay_index = (p->delay_pos + offset_i) % p->delay_buf_len;
      double delayed_0 = p->delay_buf[delay_index];
      double delayed_1 = p->delay_buf[(delay_index + 1) % p->delay_buf_len];
      double d = *ibuf++ * p->gain_in + p->regen *
                 (delayed_0 * (1 - frac) + delayed_1 * frac);

      MODULAR_INCREMENT(p->mod_pos, p->mod_buf_len);
      MODULAR_INCREMENT(p->delay_pos, p->delay_buf_len);
      p->delay_buf[p->delay_pos] = d;

      *obuf++ = SOX_ROUND_CLIP_COUNT(d * p->gain_out, effp->clips);
    }
    break;

  case INTERP_QUADRATIC:
    while (len--) {
      float offset_f = p->mod_buf_f[p->mod_pos];
      int   offset_i = offset_f;  /* == floorf() */
      float frac = offset_f - offset_i;
      sox_uint32_t delay_index = (p->delay_pos + offset_i) % p->delay_buf_len;
      double delayed_0 = p->delay_buf[delay_index];
      double delayed_1 = p->delay_buf[(delay_index + 1) % p->delay_buf_len];
      double delayed_2 = p->delay_buf[(delay_index + 2) % p->delay_buf_len];
      double d;
      {
        double a, b, delayed;
        delayed_2 -= delayed_0;
        delayed_1 -= delayed_0;
        a = delayed_2 *.5 - delayed_1;
        b = delayed_1 * 2 - delayed_2 *.5;
        delayed = delayed_0 + (a * frac + b) * frac;
        d = *ibuf++ * p->gain_in + p->regen * delayed;
      }
      MODULAR_INCREMENT(p->mod_pos, p->mod_buf_len);
      MODULAR_INCREMENT(p->delay_pos, p->delay_buf_len);
      p->delay_buf[p->delay_pos] = d;

      *obuf++ = SOX_ROUND_CLIP_COUNT(d * p->gain_out, effp->clips);
    }
    break;
  }
  return SOX_SUCCESS;
}

static int stop_phaser(sox_effect_t * effp)
{
  priv_t * p = (priv_t *) effp->priv;

  free(p->delay_buf);
  switch (p->interpolation) {
  case INTERP_NONE:      free(p->mod_buf_i); break;
  case INTERP_LINEAR:
  case INTERP_QUADRATIC: free(p->mod_buf_f); break;
  }
  return SOX_SUCCESS;
}

sox_effect_handler_t const * lsx_phaser_effect_fn(void)
{
  static const char usage[] = "[-n|l|q] [-s|t] [gain-in [gain-out [delay [regen [speed [-s|t]]]]]]";
  static char const * const extra_usage[] = {
"               ___",
"In ---------->|   |------------> Out",
"    * gain-in | + | * gain-out",
"         +--->|___|",
"         |      |",
" * regen |   ___v___",
"         |  |       |   +---------------+",
"         +--| delay |<--| sine/triangle |<-- speed",
"            |_______|   +---------------+",
"",
"OPTION   RANGE  DEFAULT  DESCRIPTION",
"interp -n|-l|-q   -n     Interpolation type: none, linear or quadratic",
"gain-in  -1-1     0.4    Proportion of input delivered to output and delay",
"gain-out -1-1     0.74   Final output volume adjustment",
"delay     0-1000   3     Delay in milliseconds",
"regen    -1-1     0.4    Proportion of delay that is fed back",
"speed     0-192k  0.5    Modulation speed (no more than the sample rate)",
"-s|-t             sine   Sinusoidal or triangular modulation",
"",
"Hint: gain-in  < (1 - regen * regen)",
"      gain-out < (1 - regen) / gain-in",
"Keymaps: phaser.(gain_in|gain_out|regen)",
    NULL
  };

  static sox_effect_handler_t handler = {
    "phaser", usage, SOX_EFF_GAIN,
    getopts_phaser, start_phaser, flow_phaser, NULL, stop_phaser, NULL,
    sizeof(priv_t), extra_usage, get_phaser, set_phaser,
  };

  return &handler;
}
