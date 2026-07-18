/* libSoX effect: Saturation   (c) 2025 michael@briarproject.org
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

/* Ensure no clipping due to rounding errors in output gain compensation */
#define SAFETY_FACTOR 0.9999

typedef enum {SAT_TANH, SAT_SQRT, SAT_DIODE} sat_t;

typedef struct priv {
  /* Parameters */
  sat_t  sat_type;
  float blend;
  float offset;
  float drive;     /* used by tanh */
  float color;     /* used by sqrt */
  float threshold; /* used by diode */

  /* Recenter the output so zero in -> zero out */
  float offset_out;
  /* Keep the output within range */
  float gain_out;
} priv_t;



static lsx_enum_item const sat_enum[] = {
  LSX_ENUM_ITEM(SAT_,TANH)
  LSX_ENUM_ITEM(SAT_,SQRT)
  LSX_ENUM_ITEM(SAT_,DIODE)
  {0, 0}};



static float sat_tanh(priv_t *p, float d) {
  d += p->offset;
  return tanhf(p->drive * d) - p->offset_out;
}



static float sat_sqrt(priv_t *p, float d) {
  float root_d, sign_d_root_d, d_root_d;
  d += p->offset;
  root_d = sqrtf(fabsf(d));
  sign_d_root_d = d < 0 ? -root_d : root_d;
  d_root_d = d * root_d;
  return sign_d_root_d * p->color + d_root_d * (1 - p->color) - p->offset_out;
}



static float sat_diode(priv_t *p, float d) {
  d += p->offset;
  d = d > p->threshold ? p->threshold : d;
  return (d < -p->threshold ? -p->threshold : d) - p->offset_out;
}



static int getopts(sox_effect_t * effp, int argc, char *argv[])
{
  priv_t * p = (priv_t *) effp->priv;
  --argc, ++argv;

  /* Set defaults */
  p->sat_type = SAT_TANH;
  p->blend = 1;
  p->offset = 0;

  do {
    TEXTUAL_PARAMETER(sat_type, sat_enum)
    NUMERIC_PARAMETER(blend, 0, 1)
    NUMERIC_PARAMETER(offset, 0, 1)
  } while (0);

  switch (p->sat_type) {
    case SAT_TANH:
      p->drive = 1;
      do {
        NUMERIC_PARAMETER(drive, 1, INFINITY)
      } while (0);
      break;
    case SAT_SQRT:
      p->color = 0.5;
      do {
        NUMERIC_PARAMETER(color, 0, 1)
      } while (0);
      break;
    case SAT_DIODE:
      p->threshold = 0.5;
      do {
        NUMERIC_PARAMETER(threshold, 0, 1)
      } while (0);
      break;
    default:
      assert(sox_false);
  }

  if (argc != 0) {
    lsx_fail("invalid option `%s'", *argv);
    return lsx_usage(effp);
  }

  return SOX_SUCCESS;
}

static char *
get_saturation(sox_effect_t *effp, char *name)
{
  priv_t *p = (priv_t *)effp->priv;
  char *s = NULL;

  if (!strcmp(name, "blend")) {
    s = lsx_malloc(16);
    sprintf(s, "%g", p->blend);
  }
  if (!strcmp(name, "offset")) {
    s = lsx_malloc(16);
    sprintf(s, "%g", p->offset);
  }
  if (!strcmp(name, "drive") && p->sat_type == SAT_TANH) {
    s = lsx_malloc(16);
    sprintf(s, "%g", p->drive);
  }
  if (!strcmp(name, "color") && p->sat_type == SAT_SQRT) {
    s = lsx_malloc(16);
    sprintf(s, "%g", p->color);
  }
  if (!strcmp(name, "threshold") && p->sat_type == SAT_DIODE) {
    s = lsx_malloc(16);
    sprintf(s, "%g", p->threshold);
  }

  return s;
}

static char *
set_saturation(sox_effect_t *effp, char *name, char *value)
{
  priv_t *p = (priv_t *)effp->priv;
  char *s = NULL;
  char *endptr = value;
  double v = lsx_strtod(value, &endptr);

  if (endptr == value || *endptr != '\0') return NULL;

  if (!strcmp(name, "blend")) {
    if (v < 0) v = 0;
    if (v > 1) v = 1.0;
    p->blend = v;
    s = lsx_malloc(16);
    sprintf(s, "%g", v);
  }
  if (!strcmp(name, "offset")) {
    if (v < 0) v = 0;
    if (v > 1) v = 1;
    p->offset = v;
    s = lsx_malloc(16);
    sprintf(s, "%g", v);
  }
  if (!strcmp(name, "drive") && p->sat_type == SAT_TANH) {
    if (v < 1) v = 1;
    p->drive = v;
    s = lsx_malloc(16);
    sprintf(s, "%g", v);
  }
  if (!strcmp(name, "color") && p->sat_type == SAT_SQRT) {
    if (v < 0) v = 0;
    if (v > 1) v = 1;
    p->color = v;
    s = lsx_malloc(16);
    sprintf(s, "%g", v);
  }

  /* If anything changed, recalculate dependent variables */
  if (s) switch (p->sat_type) {
    case SAT_TANH:
      p->offset_out = sat_tanh(p, 0);
      p->gain_out = SAFETY_FACTOR / fmax(sat_tanh(p, 1), fabs(sat_tanh(p, -1)));
      break;
    case SAT_SQRT:
      p->offset_out = sat_sqrt(p, 0);
      p->gain_out = SAFETY_FACTOR / fmax(sat_sqrt(p, 1), fabs(sat_sqrt(p, -1)));
      break;
    case SAT_DIODE:
      p->offset_out = sat_diode(p, 0);
      p->gain_out = SAFETY_FACTOR / fmax(sat_diode(p, 1), fabs(sat_diode(p, -1)));
      break;
  }

  return s;
}


static int start(sox_effect_t * effp)
{
  priv_t * p = (priv_t *) effp->priv;

  /* Initialise to 0 and use saturation function to calculate the right value */
  p->offset_out = 0;

  switch (p->sat_type) {
    case SAT_TANH:
      p->offset_out = sat_tanh(p, 0);
      p->gain_out = SAFETY_FACTOR / fmax(sat_tanh(p, 1), fabs(sat_tanh(p, -1)));
      break;
    case SAT_SQRT:
      p->offset_out = sat_sqrt(p, 0);
      p->gain_out = SAFETY_FACTOR / fmax(sat_sqrt(p, 1), fabs(sat_sqrt(p, -1)));
      break;
    case SAT_DIODE:
      p->offset_out = sat_diode(p, 0);
      p->gain_out = SAFETY_FACTOR / fmax(sat_diode(p, 1), fabs(sat_diode(p, -1)));
      break;
    default:
      assert(sox_false);
  }

  effp->out_signal.length = effp->in_signal.length;

  return SOX_SUCCESS;
}



static int flow(sox_effect_t * effp, sox_sample_t const * ibuf,
    sox_sample_t * obuf, size_t * isamp, size_t * osamp)
{
  SOX_SAMPLE_LOCALS;
  priv_t * p = (priv_t *) effp->priv;
  size_t len = *isamp > *osamp ? *osamp : *isamp;
  float in, fx;

  *isamp = *osamp = len;

  switch (p->sat_type) {
    case SAT_TANH:
      while (len--) {
        in = SOX_SAMPLE_TO_FLOAT_32BIT(*ibuf++, effp->clips);
        fx = sat_tanh(p, in);
        fx = fx * p->gain_out * p->blend + in * (1 - p->blend);
        *obuf++ = SOX_FLOAT_32BIT_TO_SAMPLE(fx, effp->clips);
      }
      break;
    case SAT_SQRT:
      while (len--) {
        in = SOX_SAMPLE_TO_FLOAT_32BIT(*ibuf++, effp->clips);
        fx = sat_sqrt(p, in);
        fx = fx * p->gain_out * p->blend + in * (1 - p->blend);
        *obuf++ = SOX_FLOAT_32BIT_TO_SAMPLE(fx, effp->clips);
      }
      break;
    case SAT_DIODE:
      while (len--) {
        in = SOX_SAMPLE_TO_FLOAT_32BIT(*ibuf++, effp->clips);
        fx = sat_diode(p, in);
        fx = fx * p->gain_out * p->blend + in * (1 - p->blend);
        *obuf++ = SOX_FLOAT_32BIT_TO_SAMPLE(fx, effp->clips);
      }
      break;
  }

  return SOX_SUCCESS;
}



sox_effect_handler_t const * lsx_saturation_effect_fn(void)
{
  static const char usage[] =
"[type [blend [offset [drive|color|threshold]]]]";
  static char const * const extra_usage[] = {
"",
"           RANGE            DEFAULT  DESCRIPTION",
"type       tanh|sqrt|diode  tanh     saturation type",
"blend      0-1              1        mixture of dry and wet signals",
"offset     0-1              0        DC offset for asymmetric distortion",
"drive      1-inf            1        input gain (tanh)",
"color      0-1              0.5      mixture of two saturation colors (sqrt)",
"threshold  0-1              0.5      level at which clipping starts (diode)",
"Keymaps: saturation.(blend|offset|drive|color|threshold)",
    NULL
  };

  static sox_effect_handler_t handler = {
    "saturation", usage, SOX_EFF_GAIN,
    getopts, start, flow, NULL, NULL, NULL,
    sizeof(priv_t), extra_usage, get_saturation, set_saturation,
  };

  return &handler;
}
