/* Effect: change sample rate  Copyright (c) 2008,12 robs@users.sourceforge.net
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

/* Inspired by, and builds upon some of the ideas presented in:
 * `The Quest For The Perfect Resampler' by Laurent De Soras;
 * http://ldesoras.free.fr/doc/articles/resampler-en.pdf */

#ifdef NDEBUG /* Enable assert always. */
#undef NDEBUG /* Must undef above assert.h or other that might include it. */
#endif

#define _GNU_SOURCE
#include "sox_i.h"
#include "fft4g.h"
#include "dft_filter.h"

typedef double raw_coef_t;

#if 0 /* For float32 version, as used in foobar */
  #define sample_t   float
  #define num_coefs4 ((num_coefs + 3) & ~3) /* align coefs for SSE */
  #define coefs4_check(i) ((i) < num_coefs)
#else
  #define sample_t   double
  #define num_coefs4 num_coefs
  #define coefs4_check(i) 1
#endif

#if defined M_PIl
  #define hi_prec_clock_t long double /* __float128 is also a (slow) option */
#else
  #define hi_prec_clock_t double
#endif

#define coef(coef_p, interp_order, fir_len, phase_num, coef_interp_num, fir_coef_num) coef_p[(fir_len) * ((interp_order) + 1) * (phase_num) + ((interp_order) + 1) * (fir_coef_num) + (interp_order - coef_interp_num)]

static sample_t * prepare_coefs(raw_coef_t const * coefs, int num_coefs,
    int num_phases, int interp_order, int multiplier)
{
  int i, j, length = num_coefs4 * num_phases;
  sample_t * result;
  double fm1 = coefs[0], f1 = 0, f2 = 0;

  lsx_valloc(result, length * (interp_order + 1));

  for (i = num_coefs4 - 1; i >= 0; --i)
    for (j = num_phases - 1; j >= 0; --j) {
      double f0 = fm1, b = 0, c = 0, d = 0; /* = 0 to kill compiler warning */
      int pos = i * num_phases + j - 1;
      fm1 = coefs4_check(i) && pos > 0 ? coefs[pos - 1] * multiplier : 0;
      switch (interp_order) {
        case 1: b = f1 - f0; break;
        case 2: b = f1 - (.5 * (f2+f0) - f1) - f0; c = .5 * (f2+f0) - f1; break;
        case 3: c=.5*(f1+fm1)-f0;d=(1/6.)*(f2-f1+fm1-f0-4*c);b=f1-f0-d-c; break;
        default: if (interp_order) assert(0);
      }
      #define coef_coef(x) \
        coef(result, interp_order, num_coefs4, j, x, num_coefs4 - 1 - i)
      coef_coef(0) = f0;
      if (interp_order > 0) coef_coef(1) = b;
      if (interp_order > 1) coef_coef(2) = c;
      if (interp_order > 2) coef_coef(3) = d;
      #undef coef_coef
      f2 = f1, f1 = f0;
    }
  return result;
}

typedef struct { /* So generated filter coefs may be shared between channels */
  sample_t   * poly_fir_coefs;
  dft_filter_t dft_filter[2];
} rate_shared_t;

struct stage;
typedef void (* stage_fn_t)(struct stage * input, fifo_t * output);
typedef struct stage {
  /* Common to all stage types: */
  stage_fn_t fn;
  fifo_t     fifo;
  size_t     pre;       /* Number of past samples to store */
  size_t     pre_post;  /* pre + number of future samples to store */
  size_t     preload;   /* Number of zero samples to pre-load the fifo */
  double     out_in_ratio; /* For buffer management. */

  /* For a stage with variable (run-time generated) filter coefs: */
  rate_shared_t * shared;
  int        dft_filter_num; /* Which, if any, of the 2 DFT filters to use */

  /* For a stage with variable L/M: */
  union {               /* 32bit.32bit fixed point arithmetic */
    #if defined(WORDS_BIGENDIAN)
    struct {int32_t integer; uint32_t fraction;} parts;
    #else
    struct {uint32_t fraction; int32_t integer;} parts;
    #endif
    int64_t all;
    #define MULT32 (65536. * 65536.)

    hi_prec_clock_t hi_prec_clock;
  } at, step;
  sox_bool   use_hi_prec_clock;
  int        L, remL, remM;
  int        n, phase_bits;
} stage_t;

#if 0
#define stage_occupancy(s) max(0, lsx_fifo_occupancy(&(s)->fifo) - (s)->pre_post)
#else
#define stage_occupancy(s) \
    ((lsx_fifo_occupancy(&(s)->fifo) > (s)->pre_post) \
   ? (lsx_fifo_occupancy(&(s)->fifo) - (s)->pre_post) : 0)
#endif
#define stage_read_p(s) ((sample_t *)lsx_fifo_read_ptr(&(s)->fifo) + (s)->pre)

static void cubic_stage_fn(stage_t * p, fifo_t * output_fifo)
{
  int i, num_in = stage_occupancy(p), max_num_out = 1 + num_in*p->out_in_ratio;
  sample_t const * input = stage_read_p(p);
  sample_t * output = lsx_fifo_reserve(output_fifo, max_num_out);

  for (i = 0; p->at.parts.integer < num_in; ++i, p->at.all += p->step.all) {
    sample_t const * s = input + p->at.parts.integer;
    sample_t x = p->at.parts.fraction * (1 / MULT32);
    sample_t b = .5*(s[1]+s[-1])-*s, a = (1/6.)*(s[2]-s[1]+s[-1]-*s-4*b);
    sample_t c = s[1]-*s-a-b;
    output[i] = ((a*x + b)*x + c)*x + *s;
  }
  assert(max_num_out - i >= 0);
  lsx_fifo_trim_by(output_fifo, max_num_out - i);
  lsx_fifo_read(&p->fifo, p->at.parts.integer, NULL);
  p->at.parts.integer = 0;
}

static void dft_stage_fn(stage_t * p, fifo_t * output_fifo)
{
  sample_t * output, tmp;
  size_t i;
  int j;
  size_t num_in = max(0, lsx_fifo_occupancy(&p->fifo));
  rate_shared_t const * s = p->shared;
  dft_filter_t const * f = &s->dft_filter[p->dft_filter_num];
  int const overlap = f->num_taps - 1;

  while (p->remL + p->L * num_in >= f->dft_length) {
    div_t divd = div(f->dft_length - overlap - p->remL + p->L - 1, p->L);
    sample_t const * input = lsx_fifo_read_ptr(&p->fifo);
    lsx_fifo_read(&p->fifo, divd.quot, NULL);
    num_in -= divd.quot;

    output = lsx_fifo_reserve(output_fifo, f->dft_length);
    if (lsx_is_power_of_2(p->L)) { /* F-domain */
      size_t portion = f->dft_length / p->L;
      memcpy(output, input, (unsigned)portion * sizeof(*output));
      lsx_safe_rdft(portion, 1, output);
      for (i = portion + 2; i < (portion << 1); i += 2)
        output[i] = output[(portion << 1) - i],
        output[i+1] = -output[(portion << 1) - i + 1];
      output[portion] = output[1];
      output[portion + 1] = 0;
      output[1] = output[0];
      for (portion <<= 1; i < f->dft_length; i += portion, portion <<= 1) {
        memcpy(output + i, output, portion * sizeof(*output));
        output[i + 1] = 0;
      }
    } else {
      if (p->L == 1)
        memcpy(output, input, f->dft_length * sizeof(*output));
      else {
        memset(output, 0, f->dft_length * sizeof(*output));
        for (j = 0, i = p->remL; i < f->dft_length; ++j, i += p->L)
          output[i] = input[j];
        p->remL = p->L - 1 - divd.rem;
      }
      lsx_safe_rdft(f->dft_length, 1, output);
    }
    output[0] *= f->coefs[0];
    if (p->step.parts.integer > 0) {
      output[1] *= f->coefs[1];
      for (i = 2; i < f->dft_length; i += 2) {
        tmp = output[i];
        output[i  ] = f->coefs[i  ] * tmp - f->coefs[i+1] * output[i+1];
        output[i+1] = f->coefs[i+1] * tmp + f->coefs[i  ] * output[i+1];
      }
      lsx_safe_rdft(f->dft_length, -1, output);
      if (p->step.parts.integer != 1) {
        for (j = 0, i = p->remM; i < f->dft_length - overlap; ++j,
            i += p->step.parts.integer)
          output[j] = output[i];
        p->remM = i - (f->dft_length - overlap);
        lsx_fifo_trim_by(output_fifo, f->dft_length - j);
      }
      else lsx_fifo_trim_by(output_fifo, overlap);
    }
    else { /* F-domain */
      int m = -p->step.parts.integer;
      for (i = 2; i < (f->dft_length >> m); i += 2) {
        tmp = output[i];
        output[i  ] = f->coefs[i  ] * tmp - f->coefs[i+1] * output[i+1];
        output[i+1] = f->coefs[i+1] * tmp + f->coefs[i  ] * output[i+1];
      }
      output[1] = f->coefs[i] * output[i] - f->coefs[i+1] * output[i+1];
      lsx_safe_rdft(f->dft_length >> m, -1, output);
      lsx_fifo_trim_by(output_fifo, (((1 << m) - 1) * f->dft_length + overlap) >>m);
    }
  }
}

static int dft_stage_init(
    unsigned instance, double Fp, double Fs, double Fn, double att,
    double phase, stage_t * stage, int L, int M)
{
  dft_filter_t * f = &stage->shared->dft_filter[instance];

  if (!f->num_taps) {
    int num_taps = 0, dft_length, i;
    int k = phase == 50 && lsx_is_power_of_2(L) && Fn == L? L << 1 : 4;
    double * h = lsx_design_lpf(Fp, Fs, Fn, att, &num_taps, -k, -1.);

    if (phase != 50)
      lsx_fir_to_phase(&h, &num_taps, &f->post_peak, phase);
    else f->post_peak = num_taps / 2;

    dft_length = lsx_set_dft_length(num_taps);

    if (L > dft_length) {
      lsx_fail("invalid DFT parameters");
      return SOX_EINVAL;
    }

    lsx_vcalloc(f->coefs, dft_length);
    for (i = 0; i < num_taps; ++i)
      f->coefs[(i + dft_length - num_taps + 1) & (dft_length - 1)]
        = h[i] / dft_length * 2 * L;
    free(h);
    f->num_taps = num_taps;
    f->dft_length = dft_length;
    lsx_safe_rdft(dft_length, 1, f->coefs);
    lsx_debug("fir_len=%i dft_length=%i Fp=%g Fs=%g Fn=%g att=%g %i/%i",
        num_taps, dft_length, Fp, Fs, Fn, att, L, M);
  }
  stage->fn = dft_stage_fn;
  stage->preload = f->post_peak / L;
  stage->remL    = f->post_peak % L;
  stage->L = L;
  stage->step.parts.integer = abs(3-M) == 1 && Fs == 1? -M/2 : M;
  stage->dft_filter_num = instance;

  return SOX_SUCCESS;
}

#include "rate_filters.h"

typedef struct {
  double     factor;
  int64_t    samples_in, samples_out, samples_out_max;
  int        num_stages;
  stage_t    * stages;
  sox_sample_t *lpc_buffer;
  int        lpc_length, lpc_trim, lpc_count, lpc_inratio;
} rate_t;

#define pre_stage       p->stages[shift]
#define arb_stage       p->stages[shift + have_pre_stage]
#define post_stage      p->stages[shift + have_pre_stage + have_arb_stage]
#define have_pre_stage  (preM  * preL  != 1)
#define have_arb_stage  (arbM  * arbL  != 1)
#define have_post_stage (postM * postL != 1)

#define TO_3dB(a)       ((1.6e-6*a-7.5e-4)*a+.646)
#define LOW_Q_BW0_PC    (67 + 5 / 8.)

typedef enum {
  rolloff_none, rolloff_small /* <= 0.01 dB */, rolloff_medium /* <= 0.35 dB */
} rolloff_t;

static size_t local_gcd(size_t a, size_t b)
{
  size_t res = ((a < b) ? a : b);
  while (res > 0) {
    if ((a % res == 0) && (b % res == 0)) {
      break;
    }
    res--;
  }
  return res;
}

static void calc_optimal_lpc_buffer_sizes(size_t inrate, size_t outrate, int *in_len, int *out_len, int *in_ratio)
{
  const size_t gcd = local_gcd(inrate, outrate);
  const size_t in = inrate / gcd;
  const size_t out = outrate / gcd;
  const size_t c = max((inrate / 20) / in, 1); /* try to get ~50 ms extrapolation buffer */
  *in_len = (int)(c * in);
  *out_len = (int)(c * out);
  *in_ratio = (int)in;
}

static int lpc_length(int samples, int ideallength, int in_ratio)
{
  int c;

  if (samples >= ideallength) return ideallength;
  c = max(samples / in_ratio, 1);
  return min(c * in_ratio, samples);
}

static void vorbis_lpc_from_data(sample_t *data, sample_t *lpci, int n, int m, int stride)
{
  double *aut = (double *)malloc((m + 1) * sizeof(double));
  double *lpc = (double *)malloc((m) * sizeof(double));
  double error;
  double epsilon;
  int i, j;
  if (!aut || !lpc) {
    free(lpc); free(aut);
    return;
  }

  /* FIXME: Apply a window to the input. */
  /* autocorrelation, p+1 lag coefficients */
  j = m + 1;
  while (j--) {
    double d = 0; /* double needed for accumulator depth */
    for (i = j; i < n; i++) d += (double)data[i * stride] * data[(i - j) * stride];
    aut[j] = d;
  }

  /* Apply lag windowing (better than bandwidth expansion) */
  if (m <= 64) {
    for (i = 1; i <= m; i++) {
      /* Approximate this gaussian for low enough order. */
      /* aut[i] *= exp(-.5*(2*M_PI*.002*i)*(2*M_PI*.002*i));*/
      aut[i] -= aut[i] * (0.008f * 0.008f) * i * i;
    }
  }
  /* Generate lpc coefficients from autocorr values */

  /* set our noise floor to about -200dB */
  error = aut[0] * (1. + 1e-12);
  epsilon = 1e-11 * aut[0] + 1e-12;

  for (i = 0; i < m; i++) {
    double r = -aut[i + 1];

    if (error < epsilon) {
      memset(lpc + i, 0, (m - i) * sizeof(*lpc));
      goto done;
    }

    /* Sum up this iteration's reflection coefficient; note that in
       Vorbis we don't save it.  If anyone wants to recycle this code
       and needs reflection coefficients, save the results of 'r' from
       each iteration. */

    for (j = 0; j < i; j++) r -= lpc[j] * aut[i - j];
    r /= error;

    /* Update LPC coefficients and total error */

    lpc[i] = r;
    for (j = 0; j < i / 2; j++) {
      double tmp = lpc[j];

      lpc[j] += r * lpc[i - 1 - j];
      lpc[i - 1 - j] += r * tmp;
    }
    if (i & 1) lpc[j] += lpc[j] * r;

    error *= 1. - r * r;
  }

done:

  /* slightly damp the filter */
  if (m <= 64) {
    const double g = .999;
    double damp = g;
    for (j = 0; j < m; j++) {
      lpc[j] *= damp;
      damp *= g;
    }
  }

  for (j = 0; j < m; j++) lpci[j] = (sample_t)lpc[j];

  free(lpc);
  free(aut);
}

static void extend_signal_out(sox_sample_t *x, int before, int after, int channels)
{
  sample_t *work, *window, *lpc;
  int lpc_order = 512;
  int i, c;

  if (after == 0) return;
  if ((before - 1) / 2 < lpc_order) lpc_order = (before - 1) / 2;
  work = (sample_t *)malloc((before + after) * sizeof(sample_t));
  window = (sample_t *)malloc(after * sizeof(sample_t));
  lpc = (sample_t *)malloc(lpc_order * sizeof(sample_t));
  if (before < 2 * lpc_order || !work || !window || !lpc) { /* was 4 */
    for (i = 0; i < after * channels; i++) x[i] = 0;
    if (!work || !window || !lpc) {
      free(work); free(lpc); free(window);
      return;
    }
  }

  {
    const sample_t LPC_GOERTZEL_CONST = (sample_t)(2.0 * cos(M_PI / after));
    /* Generate Window using a resonating IIR aka Goertzel's algorithm. */
    sample_t m0 = 1, m1 = 0.5 * LPC_GOERTZEL_CONST;
    sample_t a1 = LPC_GOERTZEL_CONST;
    window[0] = 1;
    for (i = 1; i < after; i++) {
      window[i] = a1 * m0 - m1;
      m1 = m0;
      m0 = window[i];
    }
    for (i = 0; i < after; i++) window[i] = 0.5 + 0.5 * window[i];
  }
  for (c = 0; c < channels; c++) {
    for (i = 0; i < before; ++i) work[i] = (sample_t)x[(i - before) * channels + c] / (sample_t)(1ul << 31ul);
    vorbis_lpc_from_data(work, lpc, before, lpc_order, 1);
    for (i = 0; i < after; i++) {
      sample_t sum = 0;
      int j;

      for (j = 0; j < lpc_order; j++) sum -= work[before + i - j - 1] * lpc[j];
      work[i + before] = sum;
    }
    for (i = 0; i < after; i++) x[i * channels + c] = (sox_sample_t)(work[i + before] * (sample_t)(1ul << 31ul) * window[i]);
  }
  free(work);
  free(lpc);
  free(window);
}

static void extend_signal_in(sox_sample_t *x, int before, int after, int channels)
{
  sox_sample_t *rev;
  int i, c;

  if (after == 0) return;
  rev = (sox_sample_t *)malloc((before + after) * channels * sizeof(sox_sample_t));
  if (!rev) {
    for (i = 0; i < after * channels; ++i) x[i - after * channels] = 0;
    return;
  }
  for (c = 0; c < channels; c++) {
    for (i = 0; i < before; i++) {
      rev[i * channels + c] = x[(before - i - 1) * channels + c];
    }
  }

  extend_signal_out(rev + before * channels, before, after, channels);

  for (c = 0; c < channels; c++) {
    for (i = 0; i < after; i++) {
      x[(i - after) * channels + c] = rev[(before + after - i - 1) * channels + c];
    }
  }

  free(rev);
}

static int rate_init(
  /* Private work areas (to be supplied by the client):                       */
  rate_t * p,                /* Per audio channel.                            */
  rate_shared_t * shared,    /* Between channels (undergoing same rate change)*/

  /* Public parameters:                                             Typically */
  sox_rate_t inrate,         /* Input samplerate                              */
  sox_rate_t outrate,        /* Output samplerate                             */
  /*double factor,*/         /* Input rate divided by output rate.            */
  double bits,               /* Required bit-accuracy (pass + stop)  16|20|28 */
  double phase,              /* Linear/minimum etc. filter phase.       50    */
  double bw_pc,              /* Pass-band % (0dB pt.) to preserve.   91.3|98.4*/
  double anti_aliasing_pc,   /* % bandwidth without aliasing            100   */
  rolloff_t rolloff,         /* Pass-band roll-off                    small   */
  sox_bool maintain_3dB_pt,  /*                                        true   */

  /* Primarily for test/development purposes:                                 */
  sox_bool use_hi_prec_clock,/* Increase irrational ratio accuracy.   false   */
  int interpolator,          /* Force a particular coef interpolator.   -1    */
  int max_coefs_size,        /* k bytes of coefs to try to keep below.  400   */
  sox_bool noSmallIntOpt)    /* Disable small integer optimizations.  false   */
{
  double factor = (outrate != 0) ? (double)inrate / (double)outrate : 0;
  double att = (bits + 1) * linear_to_dB(2.), attArb = att;    /* pass + stop */
  double tbw0 = 1 - bw_pc / 100, Fs_a = 2 - anti_aliasing_pc / 100;
  double arbM = factor, tbw_tighten = 1;
  int n = 0, i, preL = 1, preM = 1, shift = 0, arbL = 1, postL = 1, postM = 1;
  sox_bool upsample = sox_false, rational = sox_false, iOpt = !noSmallIntOpt;
  int mode = rolloff > rolloff_small? factor > 1 || bw_pc > LOW_Q_BW0_PC :
    ceil(2 + (bits - 17) / 4);
  stage_t * s;
  int err;

  assert(factor > 0);
  assert(!bits || (15 <= bits && bits <= 33));
  assert(0 <= phase && phase <= 100);
  assert(53 <= bw_pc && bw_pc <= 100);
  assert(85 <= anti_aliasing_pc && anti_aliasing_pc <= 100);

  p->factor = factor;
  if (bits) while (!n++) {                               /* Determine stages: */
    int try, L, M, x, maxL = interpolator > 0? 1 : mode? 2048 :
      ceil(max_coefs_size * 1000. / (U100_l * sizeof(sample_t)));
    double d, epsilon = 0, frac;
    upsample = arbM < 1;
    for (i = arbM * .5, shift = 0; i >>= 1; arbM *= .5, ++shift);
    preM = upsample || (arbM > 1.5 && arbM < 2);
    postM = 1 + (arbM > 1 && preM), arbM /= postM;
    preL = 1 + (!preM && arbM < 2) + (upsample && mode), arbM *= preL;
    if ((frac = arbM - (int)arbM))
      epsilon = fabs((uint32_t)(frac * MULT32 + .5) / (frac * MULT32) - 1);
    for (i = 1, rational = !frac; i <= maxL && !rational; ++i) {
      d = frac * i, try = d + .5;
      if ((rational = fabs(try / d - 1) <= epsilon)) {    /* No long doubles! */
        if (try == i)
          arbM = ceil(arbM), shift += arbM > 2, arbM /= 1 + (arbM > 2);
        else arbM = i * (int)arbM + try, arbL = i;
      }
    }
    L = preL * arbL, M = arbM * postM, x = (L|M)&1, L >>= !x, M >>= !x;
    if (iOpt && postL == 1 && (d = preL * arbL / arbM) > 4 && d != 5) {
      for (postL = 4, i = d / 16; i >>= 1; postL <<= 1);
      lsx_debug("postL=%d", postL);
      arbM = arbM * postL / arbL / preL, arbL = 1, n = 0;
    } else if (rational && (max(L, M) < 3 + 2 * iOpt || L * M < 6 * iOpt))
      preL = L, preM = M, arbM = arbL = postM = 1;
    if (!mode && (!rational || !n))
      ++mode, n = 0;
  }

  p->num_stages = shift + have_pre_stage + have_arb_stage + have_post_stage;

  if (!p->num_stages)
    return SOX_SUCCESS;

  if ((size_t)inrate == 0) {
    lsx_fail("input sample rate is %g", inrate);
    return SOX_EOF;
  }

  calc_optimal_lpc_buffer_sizes((size_t)inrate, (size_t)outrate, &p->lpc_length, &p->lpc_trim, &p->lpc_inratio);
  if (p->lpc_length > 0) {
    p->lpc_buffer = (sox_sample_t *)malloc(p->lpc_length * 2 * sizeof(sox_sample_t));
    if (!p->lpc_buffer) return SOX_ENOMEM;
  }
  p->lpc_count = 0;
  p->samples_out_max = 0;

  lsx_vcalloc(p->stages, p->num_stages + 1);
  for (i = 0; i < p->num_stages; ++i)
    p->stages[i].shared = shared;

  if ((n = p->num_stages) > 1) {                              /* Att. budget: */
    if (have_arb_stage)
      att += linear_to_dB(2.), attArb = att, --n;
    att += linear_to_dB((double)n);
  }

  for (n = 0; n + 1u < array_length(half_firs) && att > half_firs[n].att; ++n);
  for (i = 0, s = p->stages; i < shift; ++i, ++s) {
    s->fn = half_firs[n].fn;
    s->pre_post = 4 * half_firs[n].num_coefs;
    s->preload = s->pre = s->pre_post >> 1;
  }

  if (have_pre_stage) {
    if (maintain_3dB_pt && have_post_stage) {    /* Trans. bands overlapping. */
      double tbw3 = tbw0 * TO_3dB(att);               /* TODO: consider Fs_a. */
      double x = ((2.1429e-4 - 5.2083e-7 * att) * att - .015863) * att + 3.95;
      x = att * pow((tbw0 - tbw3) / (postM / (factor * postL) - 1 + tbw0), x);
      if (x > .035) {
        tbw_tighten = ((4.3074e-3 - 3.9121e-4 * x) * x - .040009) * x + 1.0014;
        lsx_debug("x=%g tbw_tighten=%g", x, tbw_tighten);
      }
    }
    err = dft_stage_init(0, 1 - tbw0 * tbw_tighten, Fs_a,
        preM ? max(preL, preM) : arbM / arbL,
        att, phase, &pre_stage, preL, max(preM, 1));
    if (err)
      return err;
  }

  if (!bits) {                                  /* Quick and dirty arb stage: */
    arb_stage.fn = cubic_stage_fn;
    arb_stage.step.all = arbM * MULT32 + .5;
    arb_stage.pre_post = max(3, arb_stage.step.parts.integer);
    arb_stage.preload = arb_stage.pre = 1;
    arb_stage.out_in_ratio = MULT32 * arbL / arb_stage.step.all;
  }
  else if (have_arb_stage) {                     /* Higher quality arb stage: */
    poly_fir_t const * f = &poly_firs[6*(upsample + !!preM) + mode - !upsample];
    int order, num_coefs = f->interp[0].scalar, phase_bits, phases, coefs_size;
    double x = .5, at, Fp, Fs, Fn, mult = upsample? 1 : arbL / arbM;
    poly_fir1_t const * f1;

    Fn = !upsample && preM? x = arbM / arbL : 1;
    Fp = !preM? mult : mode? .5 : 1;
    Fs = 2 - Fp;           /* Ignore Fs_a; it would have little benefit here. */
    Fp *= 1 - tbw0;
    if (rolloff > rolloff_small && mode)
      Fp = !preM? mult * .5 - .125 : mult * .05 + .1;
    else if (rolloff == rolloff_small)
      Fp = Fs - (Fs - .148 * x - Fp * .852) * (.00813 * bits + .973);

    i = (interpolator < 0? !rational : max(interpolator, !rational)) - 1;
    do {
      f1 = &f->interp[++i];
      assert(f1->fn);
      if (i)
        arbM /= arbL, arbL = 1, rational = sox_false;
      phase_bits = ceil(f1->scalar + log(mult)/log(2.));
      phases = !rational? (1 << phase_bits) : arbL;
      if (!f->interp[0].scalar) {
        int phases0 = max(phases, 19), n0 = 0;
        lsx_design_lpf(Fp, Fs, -Fn, attArb, &n0, phases0, f->beta);
        num_coefs = n0 / phases0 + 1, num_coefs += num_coefs & !preM;
      }
      if ((num_coefs & 1) && rational && (arbL & 1))
        phases <<= 1, arbL <<= 1, arbM *= 2;
      at = arbL * .5 * (num_coefs & 1);
      order = i + (i && mode > 4);
      coefs_size = num_coefs4 * phases * (order + 1) * sizeof(sample_t);
    } while (interpolator < 0 && i < 2 && f->interp[i+1].fn &&
        coefs_size / 1000 > max_coefs_size);

    if (!arb_stage.shared->poly_fir_coefs) {
      int num_taps = num_coefs * phases - 1;
      raw_coef_t * coefs = lsx_design_lpf(
          Fp, Fs, Fn, attArb, &num_taps, phases, f->beta);
      arb_stage.shared->poly_fir_coefs = prepare_coefs(
          coefs, num_coefs, phases, order, 1);
      lsx_debug("fir_len=%i phases=%i coef_interp=%i size=%s",
          num_coefs, phases, order, lsx_sigfigs3((double)coefs_size));
      free(coefs);
    }
    arb_stage.fn = f1->fn;
    arb_stage.pre_post = num_coefs4 - 1;
    arb_stage.preload = (num_coefs - 1) >> 1;
    arb_stage.n = num_coefs4;
    arb_stage.phase_bits = phase_bits;
    arb_stage.L = arbL;
    arb_stage.use_hi_prec_clock = mode > 1 && use_hi_prec_clock && !rational;
    if (arb_stage.use_hi_prec_clock) {
      arb_stage.at.hi_prec_clock = at;
      arb_stage.step.hi_prec_clock = arbM;
      arb_stage.out_in_ratio = arbL / arb_stage.step.hi_prec_clock;
    } else {
      arb_stage.at.all = at * MULT32 + .5;
      arb_stage.step.all = arbM * MULT32 + .5;
      arb_stage.out_in_ratio = MULT32 * arbL / arb_stage.step.all;
    }
  }

  if (have_post_stage) {
    err = dft_stage_init(1, 1 - (1 - (1 - tbw0) *
        (upsample? factor * postL / postM : 1)) * tbw_tighten, Fs_a,
        (double)max(postL, postM), att, phase, &post_stage, postL, postM);
    if (err)
      return err;
  }

  for (i = 0, s = p->stages; i < p->num_stages; ++i, ++s) {
    lsx_fifo_create(&s->fifo, (int)sizeof(sample_t));
    memset(lsx_fifo_reserve(&s->fifo, s->preload), 0, sizeof(sample_t)*s->preload);
    lsx_debug("%5zi|%-5zi preload=%zi remL=%i",
        s->pre, s->pre_post - s->pre, s->preload, s->remL);
  }
  lsx_fifo_create(&s->fifo, (int)sizeof(sample_t));

  return SOX_SUCCESS;
}

static void rate_process(rate_t * p)
{
  stage_t * stage = p->stages;
  int i;

  for (i = 0; i < p->num_stages; ++i, ++stage)
    stage->fn(stage, &(stage+1)->fifo);
}

static sample_t * rate_input(rate_t * p, sample_t const * samples, size_t n)
{
  p->samples_in += n;
  return lsx_fifo_write(&p->stages[0].fifo, (int)n, samples);
}

static sample_t const * rate_output(rate_t * p, sample_t * samples, size_t * n)
{
  fifo_t * fifo = &p->stages[p->num_stages].fifo;
  p->samples_out += *n = min(*n, (size_t)lsx_fifo_occupancy(fifo));
  return lsx_fifo_read(fifo, (int)*n, samples);
}

static void rate_flush(rate_t * p)
{
  fifo_t * fifo = &p->stages[p->num_stages].fifo;
  int64_t samples_out = p->samples_in / p->factor + .5;
  size_t remaining = samples_out > p->samples_out ?
      (size_t)(samples_out - p->samples_out) : 0;
  sample_t * buff;

  lsx_vcalloc(buff, 1024);

  if (remaining > 0) {
    while ((size_t)lsx_fifo_occupancy(fifo) < remaining) {
      rate_input(p, buff, (size_t) 1024);
      rate_process(p);
    }
    lsx_fifo_trim_to(fifo, (int)remaining);
    p->samples_in = 0;
  }
  free(buff);
}

static void rate_close(rate_t * p)
{
  rate_shared_t *shared;
  int i;

  if (!p->num_stages)
    return;

  free(p->lpc_buffer);

  shared = p->stages[0].shared;

  for (i = 0; i <= p->num_stages; ++i)
    lsx_fifo_delete(&p->stages[i].fifo);
  free(shared->dft_filter[0].coefs);
  free(shared->dft_filter[1].coefs);
  free(shared->poly_fir_coefs);
  memset(shared, 0, sizeof(*shared));
  free(p->stages);
}

/*------------------------------- SoX Wrapper --------------------------------*/

typedef struct {
  sox_rate_t      out_rate;
  int             rolloff, coef_interp, max_coefs_size;
  double          bit_depth, phase, bw_0dB_pc, anti_aliasing_pc;
  sox_bool        use_hi_prec_clock, noIOpt, given_0dB_pt;
  rate_t          rate;
  rate_shared_t   shared, * shared_ptr;
} priv_t;

static int create_rate(sox_effect_t * effp, int argc, char **argv)
{
  priv_t * p = (priv_t *) effp->priv;
  int c, quality;
  char * dummy_p;
  char const * found_at;
  char const * opts = "+i:c:b:B:A:p:Q:R:d:MILafnst" "qlmghevu";
  char const * qopts = strchr(opts, 'q');
  double rej = 0, bw_3dB_pc = 0;
  sox_bool allow_aliasing = sox_false;
  lsx_getopt_t optstate;
  lsx_getopt_init(argc, argv, opts, NULL, lsx_getopt_flag_none, 1, &optstate);

  p->coef_interp = quality = -1;
  p->rolloff = rolloff_small;
  p->phase = 50;
  p->max_coefs_size = 400;
  p->shared_ptr = &p->shared;

  while ((c = lsx_getopt(&optstate)) != -1) switch (c) {
    GETOPT_NUMERIC(optstate, 'i', coef_interp, -1, 2)
    GETOPT_NUMERIC(optstate, 'c', max_coefs_size, 100, INT_MAX)
    GETOPT_NUMERIC(optstate, 'p', phase, 0, 100)
    GETOPT_NUMERIC(optstate, 'B', bw_0dB_pc, 53, 99.5)
    GETOPT_NUMERIC(optstate, 'A', anti_aliasing_pc, 85, 100)
    GETOPT_NUMERIC(optstate, 'd', bit_depth, 15, 33)
    GETOPT_LOCAL_NUMERIC(optstate, 'b', bw_3dB_pc, 74, 99.7)
    GETOPT_LOCAL_NUMERIC(optstate, 'R', rej, 90, 200)
    GETOPT_LOCAL_NUMERIC(optstate, 'Q', quality, 0, 7)
    case 'M': p->phase =  0; break;
    case 'I': p->phase = 25; break;
    case 'L': p->phase = 50; break;
    case 'a': allow_aliasing = sox_true; break;
    case 'f': p->rolloff = rolloff_none; break;
    case 'n': p->noIOpt = sox_true; break;
    case 's': bw_3dB_pc = 99; break;
    case 't': p->use_hi_prec_clock = sox_true; break;
    default:
      if ((found_at = strchr(qopts, c)))
        quality = found_at - qopts;
      else {
        lsx_fail("invalid option `-%c'", optstate.opt);
        return lsx_usage(effp);
      }
  }
  argc -= optstate.ind, argv += optstate.ind;

  if ((unsigned)quality < 2 && (p->bw_0dB_pc || bw_3dB_pc || p->phase != 50 ||
        allow_aliasing || rej || p->bit_depth || p->anti_aliasing_pc)) {
    lsx_fail("override options only work at higher quality levels");
    return SOX_EOF;
  }
  if (quality < 0 && rej == 0 && p->bit_depth == 0)
    quality = 4;
  if (rej)
    p->bit_depth = rej / linear_to_dB(2.);
  else {
    if (quality >= 0) {
      p->bit_depth = quality? 16 + 4 * max(quality - 3, 0) : 0;
      if (quality <= 2)
        p->rolloff = rolloff_medium;
    }
    rej = p->bit_depth * linear_to_dB(2.);
  }

  if (bw_3dB_pc && p->bw_0dB_pc) {
    lsx_fail("conflicting bandwidth options");
    return SOX_EOF;
  }
  allow_aliasing |= p->anti_aliasing_pc != 0;
  if (!bw_3dB_pc && !p->bw_0dB_pc)
    p->bw_0dB_pc = quality == 1? LOW_Q_BW0_PC : 100 - 5 / TO_3dB(rej);
  else if (bw_3dB_pc && bw_3dB_pc < 85 && allow_aliasing) {
    lsx_fail("minimum allowed 3dB bandwidth with aliasing is %g%%", 85.);
    return SOX_EOF;
  }
  else if (p->bw_0dB_pc && p->bw_0dB_pc < 74 && allow_aliasing) {
    lsx_fail("minimum allowed bandwidth with aliasing is %g%%", 74.);
    return SOX_EOF;
  }
  if (bw_3dB_pc)
    p->bw_0dB_pc = 100 - (100 - bw_3dB_pc) / TO_3dB(rej);
  else {
    bw_3dB_pc = 100 - (100 - p->bw_0dB_pc) * TO_3dB(rej);
    p->given_0dB_pt = sox_true;
  }
  p->anti_aliasing_pc = p->anti_aliasing_pc? p->anti_aliasing_pc :
    allow_aliasing? bw_3dB_pc : 100;

  if (argc) {
    if ((p->out_rate = lsx_parse_frequency(*argv, &dummy_p)) <= 0 || *dummy_p) {
      lsx_fail("cannot parse frequency `%s'", *argv);
      return SOX_EOF;
    }
    argc--; argv++;
    effp->out_signal.rate = p->out_rate;
  }
  return argc? lsx_usage(effp) : SOX_SUCCESS;
}

static int start_rate(sox_effect_t * effp)
{
  priv_t * p = (priv_t *) effp->priv;
  double out_rate = p->out_rate != 0 ? p->out_rate : effp->out_signal.rate;
  int err;

  if (effp->in_signal.rate == out_rate)
    return SOX_EFF_NULL;

  if (effp->in_signal.mult)
    *effp->in_signal.mult *= .705; /* 1/(2/sinc(pi/3)-1); see De Soras 4.1.2 */

  effp->out_signal.channels = effp->in_signal.channels;
  effp->out_signal.rate = out_rate;
  err = rate_init(&p->rate, p->shared_ptr, effp->in_signal.rate, out_rate,
      p->bit_depth, p->phase, p->bw_0dB_pc, p->anti_aliasing_pc, p->rolloff,
      !p->given_0dB_pt, p->use_hi_prec_clock, p->coef_interp,
      p->max_coefs_size, p->noIOpt);

  if (err)
    return err;

  if (!p->rate.num_stages) {
    lsx_warn("input and output rates too close, skipping resampling");
    return SOX_EFF_NULL;
  }

  return SOX_SUCCESS;
}

static int flow_rate(sox_effect_t * effp, const sox_sample_t * ibuf,
                sox_sample_t * obuf, size_t * isamp, size_t * osamp)
{
  priv_t * p = (priv_t *)effp->priv;
  rate_t * rp = (rate_t *)&p->rate;
  size_t iavail = *isamp;
  size_t odone = *osamp;
  sample_t const *s;

  if (rp->lpc_count < rp->lpc_length) {
    int i;
    const int fill_buffer = (rp->lpc_count + (int)iavail < rp->lpc_length) ? (int)iavail : rp->lpc_length - rp->lpc_count;
    sox_sample_t *lpcbuf = rp->lpc_buffer + rp->lpc_length + rp->lpc_count; /* initially fill the end to leave room for backwards extrapolation */
    for (i=0; i<fill_buffer; i++) {
      *lpcbuf++ = *ibuf++;
    }
    rp->lpc_count += fill_buffer;
    iavail -= fill_buffer;
    if (rp->lpc_count == rp->lpc_length) {
      sample_t *t;
      extend_signal_in(rp->lpc_buffer + rp->lpc_length, rp->lpc_length, rp->lpc_length, 1);
      t = rate_input(rp, NULL, rp->lpc_length * 2);
      lsx_load_samples(t, rp->lpc_buffer, rp->lpc_length * 2);
      rate_process(&p->rate);
      memmove(rp->lpc_buffer, rp->lpc_buffer + rp->lpc_length, rp->lpc_length * sizeof(sox_sample_t));
      rp->samples_in -= rp->lpc_length;
    } else {
      *osamp = 0;
      return SOX_SUCCESS;
    }
  }

  { /* keep last input samples buffered for end extrapolation */
    const size_t keep_new_samples = (iavail < (size_t)rp->lpc_length) ? iavail : (size_t)rp->lpc_length;
    if (keep_new_samples < (size_t)rp->lpc_length) {
      memmove(rp->lpc_buffer, rp->lpc_buffer + keep_new_samples, ((size_t)rp->lpc_length - keep_new_samples) * sizeof(sox_sample_t));
    }
    memcpy(rp->lpc_buffer + (rp->lpc_length - keep_new_samples), (ibuf + iavail - keep_new_samples), keep_new_samples * sizeof(sox_sample_t));
  }

  if (rp->lpc_trim > 0) {
    size_t skip;

    s = rate_output(&p->rate, NULL, &odone);
    skip = (odone < (size_t)rp->lpc_trim) ? odone : (size_t)rp->lpc_trim;
    rp->lpc_trim -= skip;
    odone -= skip;
    s += skip;
    rp->samples_out -= skip;
    if (odone > 0) {
      lsx_save_samples(obuf, s, odone, &effp->clips);
    }
  } else {
    s = rate_output(&p->rate, NULL, &odone);
    lsx_save_samples(obuf, s, odone, &effp->clips);
  }

  if (iavail && odone < *osamp) {
    sample_t * t = rate_input(&p->rate, NULL, iavail);
    lsx_load_samples(t, ibuf, iavail);
    rate_process(&p->rate);
  }
  else *isamp = 0;
  *osamp = odone;
  return SOX_SUCCESS;
}

static int drain_rate(sox_effect_t * effp, sox_sample_t * obuf, size_t * osamp)
{
  priv_t * p = (priv_t *)effp->priv;
  rate_t * rp = (rate_t *)&p->rate;
  sample_t const *s;
  size_t odone = *osamp;
  size_t oavail = *osamp;
  size_t odone_tot = 0;
  if (rp->samples_out_max == 0) rp->samples_out_max = rp->samples_in / rp->factor + .5;

  if ((rp->lpc_count > 0) && (rp->lpc_count < rp->lpc_length) && (rp->lpc_trim > 0)) { /* not extrapolated yet */
    sample_t *t;
    const int use_samples = lpc_length(rp->lpc_count, rp->lpc_length, rp->lpc_inratio);
    extend_signal_in(rp->lpc_buffer + rp->lpc_length, use_samples, use_samples, 1);
    t = rate_input(&p->rate, NULL, use_samples + rp->lpc_count);
    lsx_load_samples(t, rp->lpc_buffer + (rp->lpc_length - use_samples), use_samples + rp->lpc_count);
    rate_process(&p->rate);
    memmove(rp->lpc_buffer, rp->lpc_buffer + rp->lpc_length, rp->lpc_count * sizeof(sox_sample_t));
    rp->samples_in -= use_samples;
    rp->samples_out_max = rp->samples_in / rp->factor + .5;
    rp->lpc_trim = use_samples / rp->factor + .5;
  }

  do {
    size_t skip;

    if (rp->lpc_trim > 0) { /* extrapolated beginning not trimmed away yet */
      odone = oavail;
      s = rate_output(&p->rate, NULL, &odone);
      skip = (odone < (size_t)rp->lpc_trim) ? odone : (size_t)rp->lpc_trim;
      rp->lpc_trim -= skip;
      s += skip;
      odone -= skip;
      rp->samples_out -= skip;
      if (odone > 0) {
        lsx_save_samples(obuf, s, odone, &effp->clips);
        obuf += odone;
        oavail -= odone;
        odone_tot += odone;
      }
      if ((odone == 0) && (skip == 0) && (rp->lpc_count == 0)) { /* no samples generated even though everything has been processed - signal flush */
        rate_flush(&p->rate);
      }
      odone = oavail;
    }

    if (rp->lpc_count > 0) { /* extrapolate the end of the file */
      sample_t *t;
      const size_t samples_left = (size_t)(rp->samples_out_max - rp->samples_out);
      const int use_samples = lpc_length(rp->lpc_count, rp->lpc_length, rp->lpc_inratio);
      size_t skip;
      extend_signal_out(rp->lpc_buffer + rp->lpc_count, use_samples, use_samples, 1);
      t = rate_input(&p->rate, NULL, use_samples);
      lsx_load_samples(t, rp->lpc_buffer + rp->lpc_count, use_samples);
      rp->lpc_count = 0;
      rate_process(&p->rate);
      s = rate_output(&p->rate, NULL, &odone);
      skip = (odone < (size_t)rp->lpc_trim) ? odone : (size_t)rp->lpc_trim;
      rp->lpc_trim -= skip;
      s += skip;
      odone -= skip;
      rp->samples_out -= skip;
      if (odone > samples_left) odone = samples_left;
      if (odone > 0) {
        lsx_save_samples(obuf, s, odone, &effp->clips);
        obuf += odone;
        oavail -= odone;
        odone_tot += odone;
      }
    }

    if (rp->samples_out + (int64_t)oavail > rp->samples_out_max) oavail = (rp->samples_out < rp->samples_out_max) ? (size_t)(rp->samples_out_max - rp->samples_out) : 0;

    if ((rp->lpc_trim == 0) && (oavail > 0)) {
      rate_flush(&p->rate);
      odone = oavail;
      s = rate_output(&p->rate, NULL, &odone);
      lsx_save_samples(obuf, s, odone, &effp->clips);
      obuf += odone;
      oavail -= odone;
      odone_tot += odone;
    }
  } while (oavail > 0);

  *osamp = odone_tot;
  return SOX_SUCCESS;
}

static int stop_rate(sox_effect_t * effp)
{
  priv_t * p = (priv_t *) effp->priv;
  rate_close(&p->rate);
  return SOX_SUCCESS;
}

sox_effect_handler_t const * lsx_rate_effect_fn(void)
{
  static const char usage[] =
    "[-q|-l|-m|-h|-v] [override-options] [frequency]";

  static char const * const extra_usage[] = {
"  -Q QUALITY BANDWIDTH REJ dB   TYPICAL USE",
"-q 0 quick      n/a  ~30 @ Fs/4 Playback on ancient hardware",
"-l 1 low        80%     100     Playback on old hardware (default for play)",
"-m 2 medium     95%     100     Audio playback",
"-g 3 generic    95%     100     16-bit",
"-h 4 high       95%     125     20-bit for 16-bit mastering (default for sox)",
"-e 5 extreme    95%     150     24-bit",
"-v 6 very high  95%     175     28-bit for 24-bit mastering",
"-u 7 ultra      95%     200     32-bit",
"OPTION RANGE  TYPICAL  DESCRIPTION",
"-Q n    0-7      4     Set the quality level to one of -[qlmghevu]",
"-i n   -1-2     -1     Force a particular interpolator coefficient",
"-c n   100-     400    Kbytes of coefficients to try to stay below",
"-B n  53-95     91.3   Pass-band % (0dB pt.) to preserve",
"-A n  85-100    100    % bandwidth without aliasing",
"-f                     Set no pass-band roll-off instead of 0.01dB for -Q 0-2",
"-n                     Disable small integer optimizations",
"-t                     Increase irrational ratio accuracy",
"OVERRIDE OPTIONS (only with -m or higher)",
"-M/-I/-L     Phase response = minimum/intermediate/linear(default)",
"-p 0-100     Any phase response (0 = minimum, 25 = intermediate,",
"                                50 = linear, 100 = maximum)",
"-s           Steep filter (band-width = 99%)",
"-b 74-99.7   Any band-width %",
"-a           Allow aliasing above the pass-band",
"-d 15-33     Required bit-accuracy (pass + stop)",
"-R 90-200    Set bit-accuracy to obtain R dB rejection",
    NULL
  };

  static sox_effect_handler_t handler = {
    "rate", usage, SOX_EFF_RATE,
    create_rate, start_rate, flow_rate, drain_rate, stop_rate, NULL,
    sizeof(priv_t), extra_usage, NULL, NULL,
  };

  return &handler;
}
