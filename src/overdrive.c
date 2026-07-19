/* libSoX effect: Overdrive            (c) 2008 robs@users.sourceforge.net
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

typedef struct {
  double gain, color, last_in, last_out, b0, b1, a1;
} priv_t;

static int create_overdrive(sox_effect_t * effp, int argc, char * * argv)
{
  priv_t * p = (priv_t *)effp->priv;
  --argc, ++argv;
  p->gain = p->color = 20;
  do {
    NUMERIC_PARAMETER(gain, 0, 100)
    NUMERIC_PARAMETER(color, 0, 100)
  } while (0);
  p->gain = dB_to_linear(p->gain);
  p->color /= 200;
  return argc? lsx_usage(effp) : SOX_SUCCESS;
}

static char *
get_overdrive(sox_effect_t *effp, char *name)
{
  priv_t *p = (priv_t *)effp->priv;
  char *s = NULL;

  if (!strcmp(name, "gain")) {
    double v = linear_to_dB(p->gain);
    s = lsx_malloc(16);
    sprintf(s, "%g", v);
  }
  if (!strcmp(name, "color")) {
    double v = p->color * 200;
    s = lsx_malloc(16);
    sprintf(s, "%g", v);
  }

  return s;
}

static char *
set_overdrive(sox_effect_t *effp, char *name, char *value)
{
  priv_t *p = (priv_t *)effp->priv;
  char *s = NULL;
  char *endptr = value;
  double v = lsx_strtod(value, &endptr);

  if (endptr == value || *endptr != '\0') return NULL;

  if (!strcmp(name, "gain")) {
    if (v < 0)   v = 0;
    if (v > 100) v = 100;
    p->gain = dB_to_linear(v);
    s = lsx_malloc(16);
    sprintf(s, "%g", v);
  }
  if (!strcmp(name, "color")) {
    if (v < 0)   v = 0;
    if (v > 100) v = 100;
    p->color = v / 200;
    s = lsx_malloc(16);
    sprintf(s, "%g", v);
  }

  return s;
}

static int start_overdrive(UNUSED sox_effect_t * effp)
{
  /* gain is now keymapped so it may change */
  priv_t * p = (priv_t *)effp->priv;

  if (p->gain == 1 && !sox_is_keymapped("overdrive.gain"))
    return SOX_EFF_NULL;

  return SOX_SUCCESS;
}

static int flow_overdrive(sox_effect_t * effp, const sox_sample_t * ibuf,
    sox_sample_t * obuf, size_t * isamp, size_t * osamp)
{
  priv_t * p = (priv_t *)effp->priv;
  size_t len = *isamp = *osamp = min(*isamp, *osamp);
  while (len--) {
    SOX_SAMPLE_LOCALS;
    double d = SOX_SAMPLE_TO_FLOAT_64BIT_NOCLIPS(*ibuf++), d0 = d;
    d *= p->gain;
    d += p->color;
    d = d < -1? -2./3 : d > 1? 2./3 : d - d * d * d * (1./3);
    p->last_out = d - p->last_in + .995 * p->last_out;
    /* Denormalized floating point values run between 7 and 113 times slower
     * on some CPUs so blat them to zero. */
    if (!isnormal(p->last_out)) p->last_out = 0;
    p->last_in = d;
    /* Apparently there is no need to clip count here */
    *obuf++ = SOX_FLOAT_64BIT_TO_SAMPLE_NOCLIPS(d0 * .5 + p->last_out * .75);
  }
  return SOX_SUCCESS;
}

sox_effect_handler_t const * lsx_overdrive_effect_fn(void)
{
  static char const usage[] = "[gain(20) [color(20)]]";
  static char const * const extra_usage[] = {
    "OPTION  RANGE  DEFAULT  DESCRIPTION",
    "gain    0-100    20     Decibels of gain to apply",
    "color   0-100    20     Amount of even harmonic content in the output",
    "Keymaps: overdrive.(gain|color)",
    NULL
  };
  static sox_effect_handler_t handler = {
    "overdrive", usage, SOX_EFF_GAIN,
    create_overdrive, start_overdrive, flow_overdrive, NULL, NULL, NULL,
    sizeof(priv_t), extra_usage, get_overdrive, set_overdrive,
  };
  return &handler;
}
