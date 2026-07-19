/* Copyright (c) 2000 Fabien COELHO <fabien@coelho.net>
 * This source code is freely redistributable and may be used for any purpose.
 *
 * SoX vol effect; change volume with basic linear amplitude formula.
 * Beware of saturations!  Clipping is checked and reported.
 *
 * FIXME: deprecate or remove the limiter in favour of compand.
 */

#include "sox_i.h"

typedef enum {vol_amplitude, vol_dB, vol_power} vol_type_t;

static lsx_enum_item const vol_types[] = {
  LSX_ENUM_ITEM(vol_,amplitude)
  LSX_ENUM_ITEM(vol_,dB)
  LSX_ENUM_ITEM(vol_,power)
  {0, 0}};

typedef struct {
  double    gain; /* amplitude gain. */
  vol_type_t type;
  sox_bool  uselimiter;
  double    limiterthreshhold;
  double    limitergain;
  uint64_t  limited; /* number of limited values to report. */
  uint64_t  totalprocessed;
} priv_t;

/*
 * Process options: gain (float) type (amplitude, power, dB)
 */
static int getopts_vol(sox_effect_t * effp, int argc, char **argv)
{
  priv_t *  vol = (priv_t *) effp->priv;
  char      type_string[11];
  char *    type_ptr = type_string;
  char      dummy;             /* To check for extraneous chars. */
  sox_bool  have_type;
  --argc, ++argv;

  vol->gain = 1;               /* Default is no change. */
  vol->uselimiter = sox_false; /* Default is no limiter. */

  /* Get the vol, and the type if it's in the same arg. */
  if (!argc) return lsx_usage(effp);
  have_type = sscanf(argv[0], "%lf %10s %c", &vol->gain, type_string, &dummy) - 1;
  if (have_type > 1) {
    lsx_fail("trailing garbage in `%s'", argv[0]);
    return SOX_EOF;
  }
  ++argv, --argc;

  /* No type yet? Get it from the next arg: */
  if (!have_type && argc) {
    have_type = sox_true;
    type_ptr = *argv;
    ++argv, --argc;
  }

  if (have_type) {
    lsx_enum_item const * p = lsx_find_enum_text(type_ptr, vol_types, 0);
    if (!p) {
      lsx_fail("type must be one of amplitude, power and dB");
      return SOX_EOF;
    }
    switch (p->value) {
      case vol_dB: vol->gain = dB_to_linear(vol->gain); break;
      case vol_power: /* power to amplitude, keep phase change */
        vol->gain = vol->gain > 0 ? sqrt(vol->gain) : -sqrt(-vol->gain);
        break;
    }
    vol->type = p->value;
  }

  if (argc) {
    if (fabs(vol->gain) < 1 || sscanf(*argv, "%lf %c", &vol->limitergain, &dummy) != 1 || vol->limitergain <= 0 || vol->limitergain >= 1)
      return lsx_usage(effp);

    vol->uselimiter = sox_true;
    /* The following equation is derived so that there is no
     * discontinuity in output amplitudes */
    /* and a SOX_SAMPLE_MAX input always maps to a SOX_SAMPLE_MAX output
     * when the limiter is activated. */
    /* (NOTE: There **WILL** be a discontinuity in the slope
     * of the output amplitudes when using the limiter.) */
    vol->limiterthreshhold = SOX_SAMPLE_MAX * (1.0 - vol->limitergain) / (fabs(vol->gain) - vol->limitergain);
  }
  lsx_debug("mult=%g limit=%g", vol->gain, vol->limitergain);
  return SOX_SUCCESS;
}

static char * get_vol(sox_effect_t *effp, char *name)
{
  priv_t *vol = (priv_t *)effp->priv;
  char *s = NULL;
  double value = 0;

  if (!strcmp(name, "gain")) {
    /* Return it in whatever units they specified */
    switch (vol->type) {
    case vol_amplitude: value = vol->gain; break;
    case vol_dB: value = linear_to_dB(vol->gain); break;
    case vol_power: value = (vol->gain > 0) ? sqr(vol->gain) : -sqr(-vol->gain);
    }
    s = lsx_malloc(16);
    sprintf(s, "%g", value);
  }

  return s;
}

static char *
set_vol(sox_effect_t *effp, char *name, char *value)
{
  priv_t *vol = (priv_t *)effp->priv;
  char *s = NULL;
  char *endptr = value;

  if (!strcmp(name, "gain")) {
    /* Set it in the units they specified */
    double gain = lsx_strtod(value, &endptr);

    if (endptr == value) return NULL;

    switch (vol->type) {
    case vol_amplitude: vol->gain = gain; break;
    case vol_dB:        vol->gain = dB_to_linear(gain); break;
    case vol_power:     vol->gain = gain > 0 ? sqrt(gain) : -sqrt(-gain); break;
    }
    /* Return the value in the units they specified */
    s = lsx_malloc(16);
    sprintf(s, "%g", gain);
  }
  return s;
}

/*
 * Start processing
 */
static int start_vol(sox_effect_t * effp)
{
    priv_t * vol = (priv_t *) effp->priv;

    if (vol->gain == 1 && !sox_is_keymapped("vol.gain"))
      return SOX_EFF_NULL;

    vol->limited = 0;
    vol->totalprocessed = 0;

    return SOX_SUCCESS;
}

/*
 * Process data.
 */
static int flow_vol(sox_effect_t * effp, const sox_sample_t *ibuf, sox_sample_t *obuf,
                size_t *isamp, size_t *osamp)
{
    priv_t * vol = (priv_t *) effp->priv;
    register double gain = vol->gain;
    register double limiterthreshhold = vol->limiterthreshhold;
    register double sample;
    register size_t len;

    len = min(*osamp, *isamp);

    /* report back dealt with amount. */
    *isamp = len; *osamp = len;

    if (vol->uselimiter)
    {
        vol->totalprocessed += len;

        for (;len>0; len--)
            {
                sample = *ibuf++;

                if (sample > limiterthreshhold)
                {
                        sample =  (SOX_SAMPLE_MAX - vol->limitergain * (SOX_SAMPLE_MAX - sample));
                        vol->limited++;
                }
                else if (sample < -limiterthreshhold)
                {
                        sample = -(SOX_SAMPLE_MAX - vol->limitergain * (SOX_SAMPLE_MAX + sample));
                        /* FIXME: MIN is (-MAX)-1 so need to make sure we
                         * don't go over that.  Probably could do this
                         * check inside the above equation but I didn't
                         * think it thru.
                         */
                        if (sample < SOX_SAMPLE_MIN)
                            sample = SOX_SAMPLE_MIN;
                        vol->limited++;
                } else
                        sample = gain * sample;

                SOX_SAMPLE_CLIP_COUNT(sample, effp->clips);
               *obuf++ = sample;
            }
    }
    else
    {
        /* quite basic, with clipping */
        for (;len>0; len--)
        {
                sample = gain * *ibuf++;
                SOX_SAMPLE_CLIP_COUNT(sample, effp->clips);
                *obuf++ = sample;
        }
    }
    return SOX_SUCCESS;
}

static int stop_vol(sox_effect_t * effp)
{
  priv_t * vol = (priv_t *) effp->priv;
  if (vol->limited) {
    lsx_warn("limited %" PRIu64 " values (%d percent).",
         vol->limited, (int) (vol->limited * 100.0 / vol->totalprocessed));
  }
  return SOX_SUCCESS;
}

sox_effect_handler_t const * lsx_vol_effect_fn(void)
{
  static const char usage[] =
    "gain [a|p|d(a) [limitergain]]";

  static char const * const extra_usage[] = {
"gain  amplitude  <1 attenuates, >1 amplifies, negative inverts",
"      power      <1 attenuates, >1 amplifies, negative inverts",
"      dB         <0 attenuates, >0 amplifies",
"limitergain      Used on peaks to prevent clipping; its value should be",
"                 much less than 1 (e.g. 0.02 or 0.05). The default is none.",
"Keymap: vol.gain adjusted by the gain type you specified",
    NULL
  };

  static sox_effect_handler_t handler = {
    "vol", usage, SOX_EFF_MCHAN | SOX_EFF_GAIN,
    getopts_vol, start_vol, flow_vol, NULL, stop_vol, NULL,
    sizeof(priv_t), extra_usage, get_vol, set_vol,
  };
  return &handler;
}
