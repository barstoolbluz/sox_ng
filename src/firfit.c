/* Effect: firfit filter     Copyright (c) 2009 robs@users.sourceforge.net
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

/* FIXME: Need to add other interpolation types e.g. linear, bspline,
 * window types, and filter length, maybe phase response type too.
 */

#include "sox_i.h"
#include "dft_filter.h"

#include <ctype.h>

typedef struct {
  dft_filter_priv_t base;
  char const         * filename;
  struct {double f, gain;} * knots;
  int        num_knots, n;
} priv_t;

static int create(sox_effect_t * effp, int argc, char **argv)
{
  priv_t * p = (priv_t *)effp->priv;
  dft_filter_priv_t * b = &p->base;
  int i;

  b->filter_ptr = &b->filter;
  --argc, ++argv;
  if (!argc)
    p->filename = "-";
  else if (argc == 1)
    p->filename = argv[0], --argc;
  else {
    if (argc % 2) {
      lsx_fail("odd number of arguments");
      return SOX_EOF;
    }
    for (i=0; i < argc - 1; i += 2) {
      double value;
      char *ptr;
      int n;

      lsx_revalloc(p->knots, p->num_knots + 1);
      value = lsx_parse_frequency(argv[i], &ptr);
      if (value < 0 || *ptr != '\0') {
        lsx_fail("knot freq '%s' is not a valid frequency", argv[i]);
        return SOX_EOF;
      }
      p->knots[p->num_knots].f = value;

      if (sscanf(argv[i+1], "%lf%n", &p->knots[p->num_knots].gain, &n) != 1 ||
          argv[i+1][n] != '\0') {
        lsx_fail("knot gain '%s' is not a number", argv[i+1]);
        return SOX_EOF;
      }
      if (p->num_knots > 0 && p->knots[p->num_knots].f <= p->knots[p->num_knots - 1].f) {
        lsx_fail("knot frequencies must be strictly increasing");
        return SOX_EOF;
      }
      p->num_knots++;
    }
    argc = 0;
  }
  p->n = 2047;
  return argc? lsx_usage(effp) : SOX_SUCCESS;
}

static double * make_filter(sox_effect_t * effp)
{
  priv_t * p = (priv_t *)effp->priv;
  double * log_freqs, * gains, * d, * work, * h;
  sox_rate_t rate = effp->in_signal.rate;
  int i, work_len;

  lsx_valloc(log_freqs , p->num_knots);
  lsx_valloc(gains, p->num_knots);
  lsx_valloc(d  , p->num_knots);
  for (i = 0; i < p->num_knots; ++i) {
    log_freqs[i] = log(max(p->knots[i].f, 1));
    gains[i] = p->knots[i].gain;
  }
  lsx_prepare_spline3(log_freqs, gains, p->num_knots, HUGE_VAL, HUGE_VAL, d);

  for (work_len = 8192; work_len < rate / 2; work_len <<= 1);
  lsx_vcalloc(work, work_len + 2);
  lsx_valloc(h, p->n);

  for (i = 0; i <= work_len; i += 2) {
    double f = rate * 0.5 * i / work_len;
    double spl1 = f < max(p->knots[0].f, 1)? gains[0] :
                  f > p->knots[p->num_knots - 1].f? gains[p->num_knots - 1] :
                  lsx_spline3(log_freqs, gains, d, p->num_knots, log(f));
    work[i] = dB_to_linear(spl1);
  }
  LSX_PACK(work, work_len);
  lsx_safe_rdft(work_len, -1, work);
  for (i = 0; i < p->n; ++i)
    h[i] = work[(work_len - p->n / 2 + i) % work_len] * 2. / work_len;
  lsx_apply_blackman_nutall(h, p->n);

  free(work);
  return h;
}

static sox_bool read_knots(sox_effect_t * effp)
{
  priv_t * p = (priv_t *) effp->priv;
  FILE * file = lsx_open_input_file(effp, p->filename, sox_true);
  sox_bool result = sox_false;
  char line[82]; /* Line + \n + \0 */

  if (!file) return sox_false;

  while (fgets(line, sizeof(line), file) != NULL) {
    char *linep, *endp;
    double freq, gain;

    linep = line;

    /* Syntax:
     * comments: white-space # anything \n
     * frequency pair: white-space frequency white-space frequency \n
     * Make sure CRLF is OK in case they import a knots file from DOS:
     */

    /* Skip comment lines */
    {
      char c;
      if (sscanf(line, " #%*[^\n]%c", &c) == 1) continue;
    }

    linep = line;
    while (*linep && isspace((int)*linep)) linep++; /* Skip whitespace */

    if (*linep == '\0') continue;              /* Ignore blank lines */

more:
    /* Convert the frequency */

    /* Find the end of the first frequency and terminate it */
    for (endp = linep; *endp && !isspace((int)*endp); endp++) ;
    *endp = '\0';
    freq = lsx_parse_frequency(linep, &endp);
    if (freq < 0) {
      lsx_fail("invalid knot frequency `%s'", linep);
      break;
    }
    linep = endp + 1;

    while (*linep && isspace((int)*linep)) linep++; /* Skip whitespace */

    /* Convert the gain */
    {
      int n; char c;
      if (sscanf(linep, "%lf%c%n", &gain, &c, &n) != 2 || !isspace((int)c)) {
	lsx_fail("%s gain for freq %g",
		 *linep ? "invalid" : "missing",
		 freq);
	break;
      }
      linep += n;
    }

    if (p->num_knots && freq <= p->knots[p->num_knots - 1].f) {
      lsx_fail("knot frequencies must be strictly increasing");
      break;
    }

    lsx_revalloc(p->knots, ++p->num_knots);
    p->knots[p->num_knots - 1].f = freq;
    p->knots[p->num_knots - 1].gain = gain;

    /*
     * The original firfit would read several pairs from one line.
     * Be compatible but don't document it.
     */
    while (*linep && isspace((int)*linep)) linep++; /* Skip whitespace */
    if (*linep) goto more;
  }
  lsx_report("%i knots", p->num_knots);
  if (feof(file))
    result = sox_true;

  if (file != stdin)
    fclose(file);
  return result;
}

static int start(sox_effect_t * effp)
{
  priv_t * p = (priv_t *) effp->priv;
  dft_filter_t * f = p->base.filter_ptr;

  if (!f->num_taps) {
    double * h;
    if (!p->num_knots && !read_knots(effp))
      return SOX_EOF;
    h = make_filter(effp);
    if (effp->global_info->plot != sox_plot_off) {
      lsx_plot_fir(h, p->n, effp->in_signal.rate,
          effp->global_info->plot, "SoX effect: firfit", -30., +30.);
      return SOX_EOF;
    }
    lsx_set_dft_filter(f, h, p->n, p->n >> 1);
  }
  return lsx_dft_filter_effect_fn()->start(effp);
}

sox_effect_handler_t const * lsx_firfit_effect_fn(void)
{
  static char const usage[] = "[knots-file|<freq gain>]";
  static char const * const extra_usage[] = {
    "No argument or `-' reads freq gain pairs from stdin",
    NULL
  };
  static sox_effect_handler_t handler;
  handler = *lsx_dft_filter_effect_fn();
  handler.name = "firfit";
  handler.usage = usage;
  handler.extra_usage = extra_usage;
  handler.getopts = create;
  handler.start = start;
  handler.priv_size = sizeof(priv_t);
  return &handler;
}
