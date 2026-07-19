/* libSoX effect: Contrast Enhancement    (c) 2008 robs@users.sourceforge.net
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

typedef struct {double amount;} priv_t;

static int create_contrast(sox_effect_t * effp, int argc, char * * argv)
{
  priv_t * p = (priv_t *)effp->priv;
  p->amount = 75;
  --argc, ++argv;
  do {NUMERIC_PARAMETER(amount, 0, 100)} while (0);
  p->amount /= 750; /* shift range to 0 to 0.1333, default 0.1 */
  return argc? lsx_usage(effp) : SOX_SUCCESS;
}

static char * get_contrast(sox_effect_t *effp, char *name)
{
  priv_t *p = (priv_t *)effp->priv;
  char *s = NULL;

  if (!strcmp(name, "amount")) {
    s = lsx_malloc(32);
    sprintf(s, "%g", p->amount * 750);
  }

  return s;
}

static char *
set_contrast(sox_effect_t *effp, char *name, char *value)
{
  priv_t *p = (priv_t *)effp->priv;
  char *s = NULL;
  char *endptr = value;

  if (!strcmp(name, "amount")) {
    double amount = lsx_strtod(value, &endptr);
    if (endptr == value || *endptr != '\0') return NULL;
    if (amount > 100) amount = 100;
    if (amount < 0)   amount = 0;
    p->amount = amount / 750;
    s = malloc(32);
    sprintf(s, "%g", amount);
  }
  return s;
}

static int flow_contrast(sox_effect_t * effp, const sox_sample_t * ibuf,
    sox_sample_t * obuf, size_t * isamp, size_t * osamp)
{
  priv_t * p = (priv_t *)effp->priv;
  size_t len = *isamp = *osamp = min(*isamp, *osamp);
  while (len--) {
    double d = *ibuf++ * (-M_PI_2 / SOX_SAMPLE_MIN);
    *obuf++ = sin(d + p->amount * sin(d * 4)) * SOX_SAMPLE_MAX;
  }
  return SOX_SUCCESS;
}

sox_effect_handler_t const * lsx_contrast_effect_fn(void)
{
  static char const * const extra_usage[] = {
    "OPTION  RANGE  DEFAULT  DESCRIPTION",
    "amount  0-100    75     How much to make it sound louder",
    "Keymap: contrast.amount",
    NULL
  };
  static sox_effect_handler_t handler = {
    "contrast", "[amount]",
    0, create_contrast, NULL, flow_contrast, NULL, NULL, NULL,
    sizeof(priv_t),
    extra_usage, get_contrast, set_contrast,
  };
  return &handler;
}
