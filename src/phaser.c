/* SoX flanger effect
 * Copyright (C) 24 August 1998, Juergen Mueller And Sundry Contributors
 *
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained.
 * Juergen Mueller And Sundry Contributors are not responsible for
 * the consequences of using this software.
 */

#include "sox_i.h"

typedef struct {
  double     gain_in, gain_out, delay, decay, speed;
  lsx_wave_t mod_type;

  int        * mod_buf;
  size_t     mod_buf_len;
  int        mod_pos;
            
  double     * delay_buf;
  size_t     delay_buf_len;
  int        delay_pos;
} priv_t;

static int getopts(sox_effect_t * effp, int argc, char * * argv)
{
  priv_t * p = (priv_t *) effp->priv;
  char chars[2];

  /* Set non-zero defaults: */
  p->gain_in   = .4;
  p->gain_out  = .74;
  p->delay  = 3.;
  p->decay     = .4;
  p->speed = .5;

  --argc, ++argv;
  do { /* break-able block */
    NUMERIC_PARAMETER(gain_in  , -1, 1)
    NUMERIC_PARAMETER(gain_out , -1, 1)
    NUMERIC_PARAMETER(delay    ,  0, 1000)
    NUMERIC_PARAMETER(decay    ,  0, 1)
    NUMERIC_PARAMETER(speed    ,  0, 192000)
  } while (0);

  if (argc && sscanf(*argv, "-%1[st]%c", chars, chars + 1) == 1) {
    p->mod_type = *chars == 's'? SOX_WAVE_SINE : SOX_WAVE_TRIANGLE;
    --argc, ++argv;
  }

  if (p->gain_in > (1 - p->decay * p->decay))
    lsx_warn("warning: gain-in might cause clipping");
  if (p->gain_in / (1 - p->decay) > 1 / p->gain_out)
    lsx_warn("warning: gain-out might cause clipping");

  return argc? lsx_usage(effp) : SOX_SUCCESS;
}

static int start(sox_effect_t * effp)
{
  priv_t * p = (priv_t *) effp->priv;

  p->delay_buf_len = p->delay * .001 * effp->in_signal.rate;
  if (p->delay_buf_len < 1) {
    lsx_fail("delay can't be less than %g", 1000 / effp->in_signal.rate);
    return SOX_EOF;
  }
  lsx_vcalloc(p->delay_buf, p->delay_buf_len);

  p->mod_buf_len = effp->in_signal.rate / p->speed;
  if (p->mod_buf_len < 1) {
    lsx_fail("speed can't be more than %g", effp->in_signal.rate);
    return SOX_EOF;
  }
  lsx_valloc(p->mod_buf, p->mod_buf_len);
  lsx_generate_wave_table(p->mod_type, SOX_INT, p->mod_buf, p->mod_buf_len,
      1., (double)p->delay_buf_len, M_PI_2);

  p->delay_pos = p->mod_pos = 0;

  effp->out_signal.length = SOX_UNKNOWN_LEN; /* TODO: calculate actual length */
  return SOX_SUCCESS;
}

static int flow(sox_effect_t * effp, const sox_sample_t *ibuf,
    sox_sample_t *obuf, size_t *isamp, size_t *osamp)
{
  priv_t * p = (priv_t *) effp->priv;
  size_t len = *isamp = *osamp = min(*isamp, *osamp);

  while (len--) {
    double d = *ibuf++ * p->gain_in + p->delay_buf[
      (p->delay_pos + p->mod_buf[p->mod_pos]) % p->delay_buf_len] * p->decay;
    p->mod_pos = (p->mod_pos + 1) % p->mod_buf_len;
    
    p->delay_pos = (p->delay_pos + 1) % p->delay_buf_len;
    p->delay_buf[p->delay_pos] = d;

    *obuf++ = SOX_ROUND_CLIP_COUNT(d * p->gain_out, effp->clips);
  }
  return SOX_SUCCESS;
}

static int stop(sox_effect_t * effp)
{
  priv_t * p = (priv_t *) effp->priv;

  free(p->delay_buf);
  free(p->mod_buf);
  return SOX_SUCCESS;
}

sox_effect_handler_t const * lsx_phaser_effect_fn(void)
{
  static const char usage[] = "[gain-in [gain-out [delay [decay [speed [-s|-t]]]]]]";
  static char const * const extra_usage[] = {
"               ___",
"In ---------->|   |------------> Out",
"    * gain-in | + | * gain-out",
"         +--->|___|",
"         |      |",
" * decay |   ___v___",
"         |  |       |   +---------------+",
"         +--| delay |<--| sine/triangle |<-- speed",
"            |_______|   +---------------+",
"",
"         RANGE  DEFAULT  DESCRIPTION",
"gain-in  -1-1     0.4    Proportion of input delivered to output and delay",
"gain-out -1-1     0.74   Final output volume adjustment",
"delay     0-1000   3     Delay in milliseconds",
"decay    -1-1     0.4    Proportion of delay that is fed back",
"speed     0-192k  0.5    Modulation speed (no more than the sample rate)",
"-s|-t             sine   Sinusoidal or triangular modulation",
"",
"Hint: gain-in  < (1 - decay * decay)",
"      gain-out < (1 - decay) / gain-in",
    NULL
  };

  static sox_effect_handler_t handler = {
    "phaser", usage, extra_usage, SOX_EFF_LENGTH | SOX_EFF_GAIN,
    getopts, start, flow, NULL, stop, NULL, sizeof(priv_t)
  };

  return &handler;
}
