/* Effect: sinc filters     Copyright (c) 2008-9 robs@users.sourceforge.net
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
#include "dft_filter.h"

#include <ctype.h>  /* for isdigit() */

typedef struct {
  dft_filter_priv_t  base;
  double             att, beta, phase, Fc0, Fc1, tbw0, tbw1;
  int                num_taps[2];
  sox_bool           round;
  sox_bool           delete;
} priv_t;

static int create(sox_effect_t * effp, int argc, char * * argv)
{
  priv_t * p = (priv_t *)effp->priv;
  dft_filter_priv_t * b = &p->base;
  char * parse_ptr = argv[0];
  char * frequency_range_argument; /* For error reporting */
  int i = 0; /* How many of the two frequency range specifiers have we seen? */

  lsx_getopt_t optstate;
  lsx_getopt_init(argc, argv, "+ra:b:p:MILt:n:d", NULL, lsx_getopt_flag_none, 1, &optstate);

  b->filter_ptr = &b->filter;
  p->phase = 50;
  p->beta = -1;
  while (i < 2) {
    int c = 1;
    int taps = p->num_taps[1]; /* Local alias for better usage message */

    while (c && (c = lsx_getopt(&optstate)) != -1) {
      switch (c) {
        char * parse_ptr2;
      case 'r': p->round = sox_true; break;
      GETOPT_NUMERIC(optstate, 'a', att,  40 , 180)
      GETOPT_NUMERIC(optstate, 'b', beta,  0 , 256)
      GETOPT_NUMERIC(optstate, 'p', phase, 0, 100)
      case 'M': p->phase =  0; break;
      case 'I': p->phase = 25; break;
      case 'L': p->phase = 50; break;
      GETOPT_LOCAL_NUMERIC(optstate, 'n', taps, 11, 1073741823)
      case 't': p->tbw1 = lsx_parse_frequency(optstate.arg, &parse_ptr2);
        if (p->tbw1 < 1) {
          lsx_fail("transition bandwidth must be 1 Hz or more");
          return SOX_EOF;
        }
        if (*parse_ptr2) {
          lsx_fail("invalid transition bandwidth `%s'", parse_ptr2);
          return SOX_EOF;
        }
        break;
      case 'd': p->delete = sox_true; break;
      case '?': case ':':
        if (optstate.ind < argc) {
	  /* '-' and more than one character or something that doesn't
	   * start with '-' (which should be a frequency range).
	   + optstate.ind is left indexing the unrecognized option.
	   */
	  if (argv[optstate.ind][0] == '-') {
	    char c1 = argv[optstate.ind][1];
	    if (isdigit(c1) || c1 == '%' || (c1 >= 'A' && c1 <= 'G'))
	      /* "-1000" or "-A4" or "-%[0-9.]*" or "-%-[0-9.]*" */
              goto endwhile;
	  }
	}
        if (isdigit((unsigned char)argv[optstate.ind - 1][1])) {
          /* -1 to -9: optstate.ind advances for an unknown single-char flag */
	  /* Not sure what -0 is supposed to mean - it gives silence */
          optstate.ind--;
          goto endwhile;
        }
        if (optstate.ind >= argc) {
          /* Missing obligatory parameter */
          lsx_fail("%s requires an argument", argv[optstate.ind - 1]);
          return SOX_EOF;
        }
        /* Invalid option flag */
        lsx_fail("invalid option `-%c'", optstate.opt);
	return lsx_usage(effp);

      default: goto endwhile; /* Alas, poor "break" */
      }
    }
endwhile: /* Alas, poor "break" */
    p->num_taps[1] = taps;

    if (p->att && p->beta >= 0) {
      lsx_fail("you can only give one of -a and -b");
      return SOX_EOF;
    }
    if (p->tbw1 && p->num_taps[1]) {
      lsx_fail("you can only give one of -t and -n");
      return SOX_EOF;
    }
    if (!i || !p->Fc1)
      p->tbw0 = p->tbw1, p->num_taps[0] = p->num_taps[1];
    if (!i++ && optstate.ind < argc) {
      frequency_range_argument = argv[optstate.ind];
      if (*(parse_ptr = argv[optstate.ind++]) != '-')
        p->Fc0 = lsx_parse_frequency(parse_ptr, &parse_ptr);
      if (*parse_ptr == '-')
        p->Fc1 = lsx_parse_frequency(parse_ptr + 1, &parse_ptr);
    }
  }
  if (optstate.ind != argc) {
    lsx_fail("extra parameter `%s' not understood", argv[optstate.ind]);
    return SOX_EOF;
  }
  if (p->Fc0 < 0 && p->Fc1 < 0) {
    lsx_fail("missing frequency range");
    return SOX_EOF;
  }
  if (p->Fc0 < 0 || p->Fc1 < 0) {
    lsx_fail("invalid frequency range `%s'", frequency_range_argument);
    return SOX_EOF;
  }
  if (*parse_ptr) {
    /* If no frequencies have been parsed, it's still pointing at "sinc" */
    if (parse_ptr == argv[0])
      lsx_fail("a frequency range is required");
    else /* Otherwise the parsing of a low or high frequency failed */
      lsx_fail("invalid frequency range `%s'", frequency_range_argument);
    return SOX_EOF;
  }

  return SOX_SUCCESS;
}

static void invert(double * h, int n)
{
  int i;
  for (i = 0; i < n; ++i)
    h[i] = -h[i];
  h[(n - 1) / 2] += 1;
}

static double * lpf(double Fn, double Fc, double tbw, int * num_taps, double att, double * beta, sox_bool round)
{
  int n = *num_taps;
  if ((Fc /= Fn) <= 0 || Fc >= 1) {
    *num_taps = 0;
    return NULL;
  }
  att = att? att : 120;
  lsx_kaiser_params(att, Fc, (tbw? tbw / Fn : .05) * .5, beta, num_taps);
  if (!n) {
    n = *num_taps;
    *num_taps = range_limit(n, 11, 1073741823);
    if (round)
      *num_taps = 1 + 2 * (int)((int)((*num_taps / 2) * Fc + .5) / Fc + .5);
    lsx_report("num taps = %i (from %i)", *num_taps, n);
  }
  return lsx_make_lpf(*num_taps |= 1, Fc, *beta, 0., 1., sox_false);
}

static int start(sox_effect_t * effp)
{
  priv_t * p = (priv_t *)effp->priv;
  dft_filter_t * f = p->base.filter_ptr;

  if (!f->num_taps) {
    double Fn = effp->in_signal.rate * .5;
    double * h[2];
    int i, n;
    size_t post_peak;
    int longer;

    if (p->Fc0 >= Fn || p->Fc1 >= Fn) {
      /* If low-pass filtering at a frequency above the sample rate
       * and -d, pass data through unmodified (i.e. remove ourselves)
       */
      if (p->Fc0 == 0 && p->delete) return SOX_EFF_NULL;

      lsx_fail("filter frequency must be less than sample-rate / 2");
      return SOX_EOF;
    }
    h[0] = lpf(Fn, p->Fc0, p->tbw0, &p->num_taps[0], p->att, &p->beta,p->round);
    h[1] = lpf(Fn, p->Fc1, p->tbw1, &p->num_taps[1], p->att, &p->beta,p->round);
    if (h[0])
      invert(h[0], p->num_taps[0]);

    longer = p->num_taps[1] > p->num_taps[0];
    n = p->num_taps[longer];
    if (h[0] && h[1]) {
      for (i = 0; i < p->num_taps[!longer]; ++i)
        h[longer][i + (n - p->num_taps[!longer])/2] += h[!longer][i];

      if (p->Fc0 < p->Fc1)
        invert(h[longer], n);

      free(h[!longer]);
    }
    if (p->phase != 50)
      lsx_fir_to_phase(&h[longer], &n, &post_peak, p->phase);
    else post_peak = n >> 1;

    if (effp->global_info->plot != sox_plot_off) {
      char title[100];
      sprintf(title, "SoX effect: sinc filter freq=%g-%g",
          p->Fc0, p->Fc1? p->Fc1 : Fn);
      lsx_plot_fir(h[longer], n, effp->in_signal.rate,
          effp->global_info->plot, title, -p->beta * 10 - 25, 5.);
      return SOX_EOF;
    }
    lsx_set_dft_filter(f, h[longer], n, post_peak);
  }
  return lsx_dft_filter_effect_fn()->start(effp);
}

static char const usage[] = "[-a att|-b beta] [-p phase|-M|-I|-L] [-t tbw|-n taps] [freqHP][-freqLP [-t tbw|-n taps]] [-r] [-d]";

static char const * const extra_usage[] = {
  "OPTION   RANGE    DEFAULT  DESCRIPTION",
  "-a att   40-180     120    Stop band attentuation in dB",
  "-b beta   0-256    varies  Kaiser window's `beta' parameter",
  "-p phase  0-100      50    Phase response: 0=minimum, 25=intermediate",
  "                                          50=linear, 100=maximum",
  "-M/-I/-L                   Phase response: minimum/intermediate/linear",
  "-t tbw    1-      5% band  Transition bandwidth",
  "-n taps  11-1e9    varies  Number of filter taps",
  "freq(s): 3k=high-pass; -4k=low-pass; 3k-4k=band-pass; 4k-3k=band-reject",
  "-t or -n before frequency range applies to both; after only affects freqLP",

  "-r  Round `taps' to the closest integer instead of the next lower one",
  "-d  If a low-pass filter's frequency is Nyquist or above, copy, don't fail",
  NULL
};

sox_effect_handler_t const * lsx_sinc_effect_fn(void)
{
  static sox_effect_handler_t handler;
  handler = *lsx_dft_filter_effect_fn();
  handler.name = "sinc";
  handler.usage = usage;
  handler.extra_usage = extra_usage;
  handler.getopts = create;
  handler.start = start;
  handler.priv_size = sizeof(priv_t);
  return &handler;
}
