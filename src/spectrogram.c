/* libSoX effect: Spectrogram       (c) 2008-9 robs@users.sourceforge.net
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

/*
 * Plotting on a log frequency axis (-L) and limiting the frequency range of the plot
 * (-R low:high) are by Joe Desbonnet 17 Feb 2014 from
 * https://github.com/jdesbonnet/joe-desbonnet-blog/tree/dc6c44bfc06dfac875224eda18690ffacf70409d/projects/sox-log-spectrogram
 * More details from this blog post:
 * http://jdesbonnet.blogspot.ie/2014/02/sox-spectrogram-log-frequency-axis-and.html
 */

#ifdef NDEBUG /* Enable assert always. */
#undef NDEBUG /* Must undef above assert.h or other that might include it. */
#endif

#include "sox_i.h"
#include "fft4g.h"

#ifdef HAVE_LIBPNG_PNG_H
#include <libpng/png.h>
#else
#include <png.h>
#endif
#include <zlib.h>

#ifdef HAVE_FFTW3_H
#include <fftw3.h>
#endif

/* For SET_BINARY_MODE: */
#include <fcntl.h>
#ifdef HAVE_IO_H
  #include <io.h>
#endif

#define is_p2(x) !(x & (x - 1))

#define MAX_X_SIZE 1000000	/* Limits enforced by libpng */
#define MAX_Y_SIZE 1000000

typedef enum {Window_Hann, Window_Hamming, Window_Bartlett, Window_Rectangular, Window_Kaiser, Window_Dolph} win_type_t;
static lsx_enum_item const window_options[] = {
  LSX_ENUM_ITEM(Window_,Hann)
  LSX_ENUM_ITEM(Window_,Hamming)
  LSX_ENUM_ITEM(Window_,Bartlett)
  LSX_ENUM_ITEM(Window_,Rectangular)
  LSX_ENUM_ITEM(Window_,Kaiser)
  LSX_ENUM_ITEM(Window_,Dolph)
  {0, 0}};

typedef struct {
  /* Parameters */
  double     pixels_per_sec, window_adjust;
  int        x_size0, y_size, Y_size, dB_range, gain, spectrum_points, perm;
  sox_bool   monochrome, light_background, high_colour, slack_overlap, no_axes;
  sox_bool   normalize, raw, alt_palette, truncate;
  win_type_t win_type;
  char const * out_name, * title, * comment;
  char const *duration_str, *start_time_str;
  sox_bool   using_stdout; /* output image to stdout */
  sox_bool   log10_axis;   /* plot frequency on log10 axis */
  int        low_freq, high_freq;

  /* Shared work area */
  double     * shared, * * shared_ptr;

  /* Per-channel work area */
  uint64_t   skip;
  int        dft_size, step_size, block_steps, block_num, rows, cols, read;
  int        x_size, end, end_min, last_end;
  sox_bool   truncated;
  double     * buf;             /* [dft_size] */
  double     * dft_buf;         /* [dft_size] */
  double     * window;          /* [dft_size + 1] */
  double     block_norm, max;
  double     * magnitudes;      /* [dft_size / 2 + 1] */
  float      *** tiles;
  unsigned   tile_rows;         /* number of tiles per column */
#if HAVE_FFTW
  fftw_plan  fftw_plan;		/* Used if FFT_type == FFT_fftw */
#endif
} priv_t;

/*
 * Keeping a simple square array for the spectrogram values causes the
 * computer to thrash when the spectrogram data is larger than the size
 * of the RAM because it is written column by column and read row by row.
 * With the simple layout, the inner loop one of the image-scanning loop
 * accesses one float out of every row or one out of every column which
 * keeps the entire working set swapped into RAM all the time.
 * The strategy here avoids that by storing the spectrogram's pixel values
 * in such a way that a single VM page covers a square tile of the final image
 * so only one column of pixel tiles (or one row) has to be kept in RAM
 * at a time.  The speedup is unmeasurable because one thrashing process
 * makes the entire system grind to a halt.
 */

#define PAGE_SIZE 4096
#define TILE_HEIGHT 32  /* = sqrt(PAGE_SIZE / sizeof(float)) */
#define TILE_WIDTH  32  /* = sqrt(PAGE_SIZE / sizeof(float)) */

/* Macro for accessing image data. (unsigned) to ensure * and / are shifts */
#define pdBfs(p,row,col) (p->tiles[(unsigned)(col) / TILE_WIDTH]\
                                  [(unsigned)(row) / TILE_HEIGHT]\
                                  [((unsigned)(row) % TILE_HEIGHT) * TILE_WIDTH\
                                   + (unsigned)(col) % TILE_WIDTH])
static void free_tiles(priv_t *p)
{
  unsigned tile_col, tile_row;
  unsigned tile_cols = (p->cols + TILE_WIDTH - 1) / TILE_WIDTH;

  for (tile_col = 0; tile_col < tile_cols; tile_col++) {
    for (tile_row = 0; tile_row < p->tile_rows; tile_row++)
      free(p->tiles[tile_col][tile_row]);
    free(p->tiles[tile_col]);
  }
  free(p->tiles);
}

#define secs(cols) \
  ((double)(cols) * p->step_size * p->block_steps / effp->in_signal.rate)

static unsigned char const alt_palette[] =
  "\0\0\0\0\0\3\0\1\5\0\1\10\0\1\12\0\1\13\0\1\16\1\2\20\1\2\22\1\2\25\1\2\26"
  "\1\2\30\1\3\33\1\3\35\1\3\37\1\3\40\1\3\"\1\3$\1\3%\1\3'\1\3(\1\3*\1\3,\1"
  "\3.\1\3/\1\3""0\1\3""2\1\3""4\2\3""6\4\3""8\5\3""9\7\3;\11\3=\13\3?\16\3"
  "A\17\2B\21\2D\23\2F\25\2H\27\2J\30\2K\32\2M\35\2O\40\2Q$\2S(\2U+\2W0\2Z3"
  "\2\\7\2_;\2a>\2cB\2eE\2hI\2jM\2lQ\2nU\2pZ\2r_\2tc\2uh\2vl\2xp\3zu\3|z\3}"
  "~\3~\203\3\200\207\3\202\214\3\204\220\3\205\223\3\203\226\3\200\230\3~\233"
  "\3|\236\3z\240\3x\243\3u\246\3s\251\3q\253\3o\256\3m\261\3j\263\3h\266\3"
  "f\272\3b\274\3^\300\3Z\303\3V\307\3R\312\3N\315\3J\321\3F\324\3C\327\3>\333"
  "\3:\336\3""6\342\3""2\344\3/\346\7-\350\15,\352\21+\354\27*\355\33)\356\40"
  "(\360&'\362*&\364/$\3654#\3669#\370>!\372C\40\374I\40\374O\"\374V&\374]*"
  "\374d,\374k0\374r3\374z7\375\201;\375\210>\375\217B\375\226E\375\236I\375"
  "\245M\375\254P\375\261T\375\267X\375\274\\\375\301a\375\306e\375\313i\375"
  "\320m\376\325q\376\332v\376\337z\376\344~\376\351\202\376\356\206\376\363"
  "\213\375\365\217\374\366\223\373\367\230\372\367\234\371\370\241\370\371"
  "\245\367\371\252\366\372\256\365\372\263\364\373\267\363\374\274\361\375"
  "\300\360\375\305\360\376\311\357\376\314\357\376\317\360\376\321\360\376"
  "\324\360\376\326\360\376\330\360\376\332\361\377\335\361\377\337\361\377"
  "\341\361\377\344\361\377\346\362\377\350\362\377\353";
#define alt_palette_len ((array_length(alt_palette) - 1) / 3)

/**
 * Parse an integer and allow for multiplier suffix
 * such as 'k' (x 1000) and 'M' (x 1000000).
 * Return 0 of successful, -1 on failure.
 */
static int parse_num_with_suffix (const char *s, int *a) {

  size_t n;
  char k,dummy;

  /* Empty string interpreted as 0 */
  if (*s==0) {
	return 0;
  }

  n = sscanf(s, "%d %c %c",a, &k, &dummy);
  if (n < 1 || n > 2) {
    return -1;
  }

  /* Allow for 'k' and 'M' suffix, but make case insensitive */
  if (n==2) {
    switch (k) {
      case 'k':
      case 'K':
        *a *= 1000;
        break;
      case 'M':
      case 'm':
        *a *= 1000000;
        break;
      default: return -1;
    }
  }

  /* Success */
  return 0;
}

/**
 * Given a string in format <a>:<b> where <a> and <b> are integers
 * but may have multiplier suffix eg 'k' (eg 10:1000 or 10:8k)
 * return the upper and lower values as integers.
 * If there is no colon set a value in 'a', and
 * b is left unchanged. If :<b> then 'a' will be set to 0.
 * Return 0 of successful, -1 on failure.
 */
static int parse_range (const char *s, int *a, int *b) {
  int a_status, b_status;
  char *colon = strchr(s,':');
  if (colon) {
    /* Colon found, so have a number range */
    *colon = 0; /* Temporarily put string terminator where colon is */
    a_status = parse_num_with_suffix(s, a);
    b_status = parse_num_with_suffix(colon+1,b);
    *colon = ':'; /* Restore colon */
    return a_status || b_status;
  } else {
    /* no colon: so just one value */
    return parse_num_with_suffix(s, a);
  }
}

static int getopts(sox_effect_t * effp, int argc, char **argv)
{
  priv_t * p = (priv_t *)effp->priv;
  uint64_t dummy;
  char const * next;
  int c;
  lsx_getopt_t optstate;
  lsx_getopt_init(argc, argv, "+S:d:x:X:y:Y:z:Z:q:p:W:w:st:c:AarmnlhTo:LR:", NULL, lsx_getopt_flag_none, 1, &optstate);

  p->dB_range = 120, p->spectrum_points = 249, p->perm = 1; /* Non-0 defaults */
  p->out_name = "spectrogram.png", p->comment = "Created by SoX";

  /* Default to 0 -> nyquist freq. But don't have sample rate at this point so
   * set high_freq=-1 as flag. Replace with nyquist freq in stop() function. */
  p->low_freq = 0;
  p->high_freq = -1;

  while ((c = lsx_getopt(&optstate)) != -1) switch (c) {
    GETOPT_NUMERIC(optstate, 'x', x_size0       , 100, MAX_X_SIZE)
    GETOPT_NUMERIC(optstate, 'X', pixels_per_sec,  1 , 5000)
    GETOPT_NUMERIC(optstate, 'y', y_size        , 64 , MAX_Y_SIZE)
    GETOPT_NUMERIC(optstate, 'Y', Y_size        , 130, MAX_Y_SIZE)
    GETOPT_NUMERIC(optstate, 'z', dB_range      , 20 , 180)
    GETOPT_NUMERIC(optstate, 'Z', gain          ,-100, 100)
    GETOPT_NUMERIC(optstate, 'q', spectrum_points, 0 , p->spectrum_points)
    GETOPT_NUMERIC(optstate, 'p', perm          ,  1 , 6)
    GETOPT_NUMERIC(optstate, 'W', window_adjust , -10, 10)
    case 'w': p->win_type = lsx_enum_option(c, optstate.arg, window_options);   break;
    case 's': p->slack_overlap    = sox_true;   break;
    case 'A': p->alt_palette      = sox_true;   break;
    case 'a': p->no_axes          = sox_true;   break;
    case 'r': p->raw              = sox_true;   break;
    case 'm': p->monochrome       = sox_true;   break;
    case 'n': p->normalize        = sox_true;   break;
    case 'l': p->light_background = sox_true;   break;
    case 'h': p->high_colour      = sox_true;   break;
    case 'T': p->truncate         = sox_true;   break;
    case 'L': p->log10_axis       = sox_true;   break;
    case 't': p->title            = optstate.arg; break;
    case 'c': p->comment          = optstate.arg; break;
    case 'o': p->out_name         = optstate.arg; break;
    case 'S': next = lsx_parseposition(0., optstate.arg, NULL, (uint64_t)0, (uint64_t)0, '=');
      if (next && !*next) {p->start_time_str = lsx_strdup(optstate.arg); break;}
      return lsx_usage(effp);
    case 'd': next = lsx_parsesamples(1e5, optstate.arg, &dummy, 't');
      if (next && !*next) {p->duration_str = lsx_strdup(optstate.arg); break;}
      return lsx_usage(effp);
    case 'R':
      if (parse_range (optstate.arg, &p->low_freq, &p->high_freq)) {
         lsx_fail("frequency range `%s' is invalid.", optstate.arg);
         return SOX_EOF;
      }
      if (p->low_freq < 0 || p->high_freq < 0) {
        lsx_fail("frequency range `%s' is invalid. Frequencies must be positive.", optstate.arg);
        return SOX_EOF;
      }
      if (p->low_freq >= p->high_freq) {
        lsx_fail("frequency range `%s' is invalid. Lower frequency must be less than higher frequency.", optstate.arg);
        exit(1);
      }
      break;
    default: lsx_fail("invalid option `-%c'", optstate.opt); return lsx_usage(effp);
  }
  if (!!p->x_size0 + !!p->pixels_per_sec + !!p->duration_str > 2) {
    lsx_fail("only two of -x, -X, -d may be given");
    return SOX_EOF;
  }
  if (p->y_size && p->Y_size) {
    lsx_fail("only one of -y, -Y may be given");
    return SOX_EOF;
  }
  p->gain = -p->gain;
  --p->perm;
  p->spectrum_points += 2;
  if (p->alt_palette)
    p->spectrum_points = min(p->spectrum_points, (int)alt_palette_len);
  p->shared_ptr = &p->shared;
  if (!strcmp(p->out_name, "-")) {
    if (effp->global_info->global_info->stdout_in_use_by) {
      lsx_fail("stdout already in use by `%s'", effp->global_info->global_info->stdout_in_use_by);
      return SOX_EOF;
    }
    effp->global_info->global_info->stdout_in_use_by = effp->handler.name;
    p->using_stdout = sox_true;
  }
  return optstate.ind !=argc || p->win_type == INT_MAX? lsx_usage(effp) : SOX_SUCCESS;
}


static double make_window(priv_t * p, int end)
{
  double sum = 0, * w = end < 0? p->window : p->window + end;
  int i, n = p->dft_size - abs(end);

  if (end) memset(p->window, 0, sizeof(*p->window) * (p->dft_size + 1));
  for (i = 0; i < n; ++i) w[i] = 1;
  switch (p->win_type) {
    case Window_Hann: lsx_apply_hann(w, n); break;
    case Window_Hamming: lsx_apply_hamming(w, n); break;
    case Window_Bartlett: lsx_apply_bartlett(w, n); break;
    case Window_Rectangular: break;
    case Window_Kaiser: lsx_apply_kaiser(w, n, lsx_kaiser_beta(
        (p->dB_range + p->gain) * (1.1 + p->window_adjust / 50), .1)); break;
    default: lsx_apply_dolph(w, n,
        (p->dB_range + p->gain) * (1.005 + p->window_adjust / 50) + 6);
  }
  for (i = 0; i < p->dft_size; ++i) sum += p->window[i];
  for (i = 0; i < p->dft_size; ++i) p->window[i] *= 2 / sum
    * sqr((double)n / p->dft_size);    /* empirical small window adjustment */
  return sum;
}

#if !HAVE_FFTW

static double * rdft_init(size_t n)
{
  double * q, *p;
  size_t i, j;

  lsx_valloc(q,  2 * (n / 2 + 1) * n * sizeof(*q));
  p = q;
  for (j = 0; j <= n / 2; ++j) for (i = 0; i < n; ++i)
    *p++ = cos(2 * M_PI * j * i / n), *p++ = sin(2 * M_PI * j * i / n);
  return q;
}

static void rdft_p(double const * q, double const * in, double * out, int n)
{
  int i, j;
  for (j = 0; j <= n / 2; ++j) {
    double re = 0, im = 0;
    for (i = 0; i < (n & ~7);) {
      re += in[i] * *q++, im += in[i++] * *q++;
      re += in[i] * *q++, im += in[i++] * *q++;
      re += in[i] * *q++, im += in[i++] * *q++;
      re += in[i] * *q++, im += in[i++] * *q++;
      re += in[i] * *q++, im += in[i++] * *q++;
      re += in[i] * *q++, im += in[i++] * *q++;
      re += in[i] * *q++, im += in[i++] * *q++;
      re += in[i] * *q++, im += in[i++] * *q++;
    }
    while (i < n) re += in[i] * *q++, im += in[i++] * *q++;
    *out++ += re * re + im * im;
  }
}

#endif /* HAVE_FFTW */

static int start(sox_effect_t * effp)
{
  priv_t * p = (priv_t *)effp->priv;
  double actual, duration = 0.0, start_time = 0.0,
         pixels_per_sec = p->pixels_per_sec;
  uint64_t d;

  if (p->duration_str) {
    lsx_parsesamples(effp->in_signal.rate, p->duration_str, &d, 't');
    duration = d / effp->in_signal.rate;
  }
  if (p->start_time_str) {
    uint64_t in_length = effp->in_signal.length != SOX_UNKNOWN_LEN ?
      effp->in_signal.length / effp->in_signal.channels : SOX_UNKNOWN_LEN;
    if (!lsx_parseposition(effp->in_signal.rate, p->start_time_str, &d, (uint64_t)0, in_length, '=') || d == SOX_UNKNOWN_LEN) {
      lsx_fail("-S option: audio length is unknown");
      return SOX_EOF;
    }
    start_time = d / effp->in_signal.rate;
    p->skip = d;
  }

  p->x_size = p->x_size0;
  while (sox_true) {
    if (!pixels_per_sec && p->x_size && duration)
      pixels_per_sec = min(5000, p->x_size / duration);
    else if (!p->x_size && pixels_per_sec && duration)
      p->x_size = min(MAX_X_SIZE, (int)(pixels_per_sec * duration + .5));
    if (!duration && effp->in_signal.length != SOX_UNKNOWN_LEN) {
      duration = effp->in_signal.length / (effp->in_signal.rate * effp->in_signal.channels);
      duration -= start_time;
      if (duration <= 0)
        duration = 1;
      continue;
    } else if (!p->x_size) {
      p->x_size = 800;
      continue;
    } else if (!pixels_per_sec) {
      pixels_per_sec = 100;
      continue;
    }
    break;
  }

  if (p->y_size) {
    p->dft_size = 2 * (p->y_size - 1);
#if !HAVE_FFTW
    if (!is_p2(p->dft_size) && !effp->flow)
      p->shared = rdft_init(p->dft_size);
#endif
  } else {
   int y = max(32, (p->Y_size? p->Y_size : 550) / effp->in_signal.channels - 2);
   for (p->dft_size = 128; p->dft_size <= y; p->dft_size <<= 1);
  }

  /* Now that dft_size is set, allocate variable-sized elements of priv_t */
  lsx_vcalloc(p->buf, p->dft_size);
  lsx_vcalloc(p->dft_buf, p->dft_size);
  lsx_vcalloc(p->window, p->dft_size + 1);
  lsx_vcalloc(p->magnitudes, p->dft_size / 2 + 1);

  /* Initialize the FFT routine */
#if HAVE_FFTW
  /* We have one FFT plan per flow because the input/output arrays differ. */
  p->fftw_plan = fftw_plan_r2r_1d(p->dft_size, p->dft_buf, p->dft_buf,
                      FFTW_R2HC, FFTW_MEASURE);
#else
  if (is_p2(p->dft_size) && !effp->flow)
    lsx_safe_rdft(p->dft_size, 1, p->dft_buf);
#endif
  lsx_debug("duration=%g x_size=%i pixels_per_sec=%g dft_size=%i", duration, p->x_size, pixels_per_sec, p->dft_size);

  p->end = p->dft_size;
  p->rows = (p->dft_size / 2) + 1;
  actual = make_window(p, p->last_end = 0);
  lsx_debug("window_density=%g", actual / p->dft_size);
  p->step_size = (p->slack_overlap? sqrt(actual * p->dft_size) : actual) + .5;
  p->block_steps = max(effp->in_signal.rate / pixels_per_sec, 1);
  p->step_size = p->block_steps / ceil((double)p->block_steps / p->step_size) +.5;
  p->block_steps = floor((double)p->block_steps / p->step_size +.5);
  p->block_norm = 1. / p->block_steps;
  actual = effp->in_signal.rate / p->step_size / p->block_steps;
  if (!effp->flow && actual != pixels_per_sec)
    lsx_report("actual pixels/s = %g", actual);
  lsx_debug("step_size=%i block_steps=%i", p->step_size, p->block_steps);
  p->max = -p->dB_range;
  p->read = (p->step_size - p->dft_size) / 2;
  p->tile_rows = (p->rows + TILE_HEIGHT - 1) / TILE_HEIGHT;

  return SOX_SUCCESS;
}

static int do_column(sox_effect_t * effp)
{
  priv_t * p = (priv_t *)effp->priv;
  int row;

  if (p->cols == p->x_size) {
    p->truncated = sox_true;
    if (!effp->flow)
      lsx_report("PNG truncated at %g seconds", secs(p->cols));
    return p->truncate? SOX_EOF : SOX_SUCCESS;
  }

  /* Do we need to allocate another column of tiles? */
  if (p->cols % TILE_WIDTH == 0) {
    unsigned tile_col_index = p->cols / TILE_WIDTH;
    unsigned i;

    lsx_revalloc(p->tiles, p->cols / TILE_WIDTH + 1);
    lsx_valloc(p->tiles[tile_col_index], p->rows);
    for (i = 0; i < p->tile_rows; i++)
      p->tiles[tile_col_index][i] = lsx_malloc(PAGE_SIZE);
  }
  ++p->cols;

  for (row = 0; row < p->rows; ++row) {
    double dBfs = 10 * log10(p->magnitudes[row] * p->block_norm);
    pdBfs(p, row, p->cols - 1) = dBfs + p->gain;
    p->max = max(dBfs, p->max);
  }
  memset(p->magnitudes, 0, p->rows * sizeof(*p->magnitudes));
  p->block_num = 0;
  return SOX_SUCCESS;
}

static int flow(sox_effect_t * effp,
    const sox_sample_t * ibuf, sox_sample_t * obuf,
    size_t * isamp, size_t * osamp)
{
  priv_t * p = (priv_t *)effp->priv;
  size_t len = *isamp = *osamp = min(*isamp, *osamp);
  int i;

  memcpy(obuf, ibuf, len * sizeof(*obuf)); /* Pass on audio unaffected */

  if (p->skip) {
    if (p->skip >= len) {
      p->skip -= len;
      return SOX_SUCCESS;
    }
    ibuf += p->skip;
    len -= p->skip;
    p->skip = 0;
  }
  while (!p->truncated) {
    if (p->read == p->step_size) {
      memmove(p->buf, p->buf + p->step_size,
          (p->dft_size - p->step_size) * sizeof(*p->buf));
      p->read = 0;
    }
    for (; len && p->read < p->step_size; --len, ++p->read, --p->end)
      p->buf[p->dft_size - p->step_size + p->read] =
        SOX_SAMPLE_TO_FLOAT_64BIT(*ibuf++,);
    if (p->read != p->step_size)
      break;

    if ((p->end = max(p->end, p->end_min)) != p->last_end)
      make_window(p, p->last_end = p->end);
    for (i = 0; i < p->dft_size; ++i) p->dft_buf[i] = p->buf[i] * p->window[i];
#if HAVE_FFTW
    fftw_execute(p->fftw_plan);
    /* Convert from FFTW's "half complex" format to an array of magnitudes.
     * In HC format, the values are stored:
     * r0, r1, r2 ... r(n/2), i(n+1)/2-1 .. i2, i1
     */
    p->magnitudes[0] += sqr(p->dft_buf[0]);
    for (i = 1; i < p->dft_size / 2; ++i) {
      p->magnitudes[i] += sqr(p->dft_buf[i]) + sqr(p->dft_buf[p->dft_size - i
]);
    }
    p->magnitudes[p->dft_size / 2] += sqr(p->dft_buf[p->dft_size / 2]);
#else /* ! HAVE_FFTW */
    if (is_p2(p->dft_size)) {
      lsx_safe_rdft(p->dft_size, 1, p->dft_buf);
      p->magnitudes[0] += sqr(p->dft_buf[0]);
      for (i = 1; i < p->dft_size >> 1; ++i)
        p->magnitudes[i] += sqr(p->dft_buf[2*i]) + sqr(p->dft_buf[2*i+1]);
      p->magnitudes[p->dft_size >> 1] += sqr(p->dft_buf[1]);
    }
    else rdft_p(*p->shared_ptr, p->dft_buf, p->magnitudes, p->dft_size);
#endif /* ! HAVE_FFTW */

    if (++p->block_num == p->block_steps && do_column(effp) == SOX_EOF)
      return SOX_EOF;
  }
  return SOX_SUCCESS;
}

static int drain(sox_effect_t * effp, sox_sample_t * obuf_, size_t * osamp)
{
  priv_t * p = (priv_t *)effp->priv;

  if (!p->truncated) {
    sox_sample_t * ibuf, * obuf;
    size_t isamp = (p->dft_size - p->step_size) / 2;
    int left_over = (isamp + p->read) % p->step_size;

    lsx_vcalloc(ibuf, p->dft_size);
    lsx_vcalloc(obuf, p->dft_size);

    if (left_over >= p->step_size >> 1)
      isamp += p->step_size - left_over;
    lsx_debug("cols=%i left=%i end=%i", p->cols, p->read, p->end);
    p->end = 0, p->end_min = -p->dft_size;
    if (flow(effp, ibuf, obuf, &isamp, &isamp) == SOX_SUCCESS && p->block_num) {
      p->block_norm *= (double)p->block_steps / p->block_num;
      do_column(effp);
    }
    lsx_debug("flushed cols=%i left=%i end=%i", p->cols, p->read, p->end);
    free(obuf);
    free(ibuf);
  }
  (void)obuf_, *osamp = 0;
  return SOX_SUCCESS;
}

enum {Background, Text, Labels, Grid, fixed_palette};

static unsigned colour(priv_t const * p, double x)
{
  unsigned c = x < -p->dB_range? 0 : x >= 0? p->spectrum_points - 1 :
      1 + (1 + x / p->dB_range) * (p->spectrum_points - 2);
  return fixed_palette + c;
}

static void make_palette(priv_t const * p, png_color * palette)
{
  int i;

  if (p->light_background) {
    memcpy(palette++, (p->monochrome)? "\337\337\337":"\335\330\320", (size_t)3);
    memcpy(palette++, "\0\0\0"      , (size_t)3);
    memcpy(palette++, "\077\077\077", (size_t)3);
    memcpy(palette++, "\077\077\077", (size_t)3);
  } else {
    memcpy(palette++, "\0\0\0"      , (size_t)3);
    memcpy(palette++, "\377\377\377", (size_t)3);
    memcpy(palette++, "\277\277\277", (size_t)3);
    memcpy(palette++, "\177\177\177", (size_t)3);
  }
  for (i = 0; i < p->spectrum_points; ++i) {
    double c[3], x = (double)i / (p->spectrum_points - 1);
    int at = p->light_background? p->spectrum_points - 1 - i : i;
    if (p->monochrome) {
      c[2] = c[1] = c[0] = x;
      if (p->high_colour) {
        c[(1 + p->perm) % 3] = x < .4? 0 : 5 / 3. * (x - .4);
        if (p->perm < 3)
          c[(2 + p->perm) % 3] = x < .4? 0 : 5 / 3. * (x - .4);
      }
      palette[at].red  = .5 + 255 * c[0];
      palette[at].green= .5 + 255 * c[1];
      palette[at].blue = .5 + 255 * c[2];
      continue;
    }
    if (p->high_colour) {
      static const int states[3][7] = {
        {4,5,0,0,2,1,1}, {0,0,2,1,1,3,2}, {4,1,1,3,0,0,2}};
      int j, phase_num = min(7 * x, 6);
      for (j = 0; j < 3; ++j) switch (states[j][phase_num]) {
        case 0: c[j] = 0; break;
        case 1: c[j] = 1; break;
        case 2: c[j] = sin((7 * x - phase_num) * M_PI / 2); break;
        case 3: c[j] = cos((7 * x - phase_num) * M_PI / 2); break;
        case 4: c[j] =      7 * x - phase_num;  break;
        case 5: c[j] = 1 - (7 * x - phase_num); break;
      }
    } else if (p->alt_palette) {
      int n = (double)i / (p->spectrum_points - 1) * (alt_palette_len - 1) + .5;
      c[0] = alt_palette[3 * n + 0] / 255.;
      c[1] = alt_palette[3 * n + 1] / 255.;
      c[2] = alt_palette[3 * n + 2] / 255.;
    } else {
      if      (x < .13) c[0] = 0;
      else if (x < .73) c[0] = 1  * sin((x - .13) / .60 * M_PI / 2);
      else              c[0] = 1;
      if      (x < .60) c[1] = 0;
      else if (x < .91) c[1] = 1  * sin((x - .60) / .31 * M_PI / 2);
      else              c[1] = 1;
      if      (x < .60) c[2] = .5 * sin((x - .00) / .60 * M_PI);
      else if (x < .78) c[2] = 0;
      else              c[2] =          (x - .78) / .22;
    }
    palette[at].red  = .5 + 255 * c[p->perm % 3];
    palette[at].green= .5 + 255 * c[(1 + p->perm + (p->perm % 2)) % 3];
    palette[at].blue = .5 + 255 * c[(2 + p->perm - (p->perm % 2)) % 3];
  }
}

static const Bytef fixed[] =
  "x\332eT\241\266\2450\fDVV>Y\371$re%2\237\200|2\22YY\211D\"+\337'<y\345\312"
  "\375\fd\345f\222\224\313\236\235{\270\344L\247a\232\4\246\351\201d\230\222"
  "\304D\364^ \352\362S\"m\347\311\237\237\27\64K\243\2302\265\35\v\371<\363y"
  "\354_\226g\354\214)e \2458\341\17\20J4\215[z<\271\277\367\0\63\64@\177\330c"
  "\227\204 Ir.\5$U\200\260\224\326S\17\200=\\k\20QA\334%\342\20*\303P\234\211"
  "\366\36#\370R\276_\316s-\345\222Dlz\363my*;\217\373\346z\267\343\236\364\246"
  "\236\365\2419\305p\333\267\23(\207\265\333\233\325Y\342\243\265\357\262\215"
  "\263t\271$\276\226ea\271.\367&\320\347\202_\234\27\377\345\222\253?\3422\364"
  "\207y\256\236\229\331\33\f\376\227\266\"\356\253j\366\363\347\334US\34]\371?"
  "\255\371\336\372z\265v\34\226\247\32\324\217\334\337\317U4\16\316{N\370\31"
  "\365\357iL\231y\33y\264\211D7\337\4\244\261\220D\346\1\261\357\355>\3\342"
  "\223\363\0\303\277\f[\214A,p\34`\255\355\364\37\372\224\342\277\f\207\255\36"
  "_V\7\34\241^\316W\257\177\b\242\300\34\f\276\33?/9_\331f\346\36\25Y)\2301"
  "\257\2414|\35\365\237\3424k\3\244\3\242\261\6\b\275>z$\370\215:\270\363w\36/"
  "\265kF\v\20o6\242\301\364\336\27\325\257\321\364fs\231\215G\32=\257\305Di"
  "\304^\177\304R\364Q=\225\373\33\320\375\375\372\200\337\37\374}\334\337\20"
  "\364\310(]\304\267b\177\326Yrj\312\277\373\233\37\340";
static unsigned char * font;
#define font_x 5
#define font_y 12
#define font_X (font_x + 1)

#define pixel(x,y) pixels[(y) * cols + (x)]
#define print_at(x,y,c,t) print_at_(pixels,cols,x,y,c,t,0)
#define print_up(x,y,c,t) print_at_(pixels,cols,x,y,c,t,1)

static void print_at_(png_byte * pixels, int cols, int x, int y, int c, char const * text, int orientation)
{
  for (;*text; ++text) {
    int pos = ((*text < ' ' || *text > '~'? '~' + 1 : *text) - ' ') * font_y;
    int i, j;
    for (i = 0; i < font_y; ++i) {
      unsigned line = font[pos++];
      for (j = 0; j < font_x; ++j, line <<= 1)
        if (line & 0x80) switch (orientation) {
          case 0: pixel(x + j, y - i) = c; break;
          case 1: pixel(x + i, y + j) = c; break;
        }
    }
    switch (orientation) {
      case 0: x += font_X; break;
      case 1: y += font_X; break;
    }
  }
}

static int axis(double to, int max_steps, double * limit, char * * prefix)
{
  double scale = 1, step = max(1, 10 * to);
  int i, prefix_num = 0;
  if (max_steps) {
    double try, log_10 = HUGE_VAL, min_step = (to *= 10) / max_steps;
    for (i = 5; i; i >>= 1) if ((try = ceil(log10(min_step * i))) <= log_10)
      step = pow(10., log_10 = try) / i, log_10 -= i > 1;
    prefix_num = floor(log_10 / 3);
    scale = pow(10., -3. * prefix_num);
  }
  *prefix = &"pnum-kMGTPE"[prefix_num + (prefix_num? 4 : 11)];
  *limit = to * scale;
  return step * scale + .5;
}

#define below 48
#define left 58
#define between 37
#define spectrum_width 14
#define right 35

static int stop(sox_effect_t * effp) /* only called, by end(), on flow 0 */
{
  priv_t *    p        = (priv_t *) effp->priv;
  uLong       font_len = 96 * font_y;
  int         chans    = effp->in_signal.channels;
  int         c_rows   = p->rows * chans + chans - 1;
  int         rows     = p->raw? c_rows : below + c_rows + 30 + 20 * !!p->title;
  int         cols     = p->raw? p->cols : left + p->cols + between + spectrum_width + right;
  png_byte *  pixels;
  png_color   palette[256];
  int         tick_len = 3 - p->no_axes;
  float       autogain = 0.0;	/* Is changed if the -n flag was supplied */

  float log10_low_freq, log10_high_freq;
  float nyquist_freq = (float)effp->in_signal.rate / 2;

  /* No chart upper freq set, so use nyquist freq as default */
  if (p->high_freq == -1) {
    p->high_freq = effp->in_signal.rate/2;
  }
  /* Cannot have 0Hz on log axis. Use 1Hz instead. */
  if (p->log10_axis && p->low_freq==0) {
    p->low_freq = 1;
  }

  log10_low_freq = log10f((float)p->low_freq);
  log10_high_freq = log10f((float)p->high_freq);

  free(p->shared);
  lsx_debug("signal-max=%g", p->max);
  font = lsx_malloc(font_len);
  assert(uncompress(font, &font_len, fixed, sizeof(fixed)-1) == Z_OK);
  make_palette(p, palette);
  lsx_valloc(pixels, cols * rows);
  memset(pixels, Background, cols * rows * sizeof(*pixels));

  /* Spectrogram */

  if (p->normalize)
    /* values are already in dB, so we subtract the maximum value
     * (which will normally be negative) to raise the maximum to 0.0.
     */
    autogain = -p->max;

  {
    int chan;

    for (chan = 0; chan < chans; ++chan) {
      float log_scale_factor = (log10_high_freq- log10_low_freq)/(float)p->rows;
      float lin_scale_factor = (p->high_freq-p->low_freq)/(float)p->rows;
      priv_t * q = (priv_t *)(effp - effp->flow + chan)->priv;
      int row, base;

      if (p->normalize) {
	int row, col;

	for (row=p->rows; row >=0; row--)
	  for (col=p->cols; col >=0; col--)
	    pdBfs(q, row, col) += autogain;
      }

      base = !p->raw * below + (chans - 1 - chan) * (p->rows + 1);

      for (row = 0; row < p->rows; ++row) {
	int dBfsi, col, freq;

	if (p->log10_axis) {
	  freq = (int)powf(10.0, (float)row * log_scale_factor + log10_low_freq);
	} else {
	  freq = (float)row * lin_scale_factor + p->low_freq;
	}
	/* dBfsi: index into dBfs[] corresponding to frequency at this row */
	dBfsi = (freq * p->rows) / nyquist_freq;
	/* It is possible that upper freq > Nyquist freq: deal with that */
	if (dBfsi >= p->rows) {
	  dBfsi = p->rows - 1;
	}
	for (col = 0; col < p->cols; ++col) {
	  pixel(!p->raw * left + col, base + row) =
	    colour(p, pdBfs(q, dBfsi, col));
	}
	/* Y-axis lines */
	if (!p->raw && !p->no_axes) {
	  pixel(left - 1, base + row) = Grid;
	  pixel(left + p->cols, base + row) = Grid;
	}
      }
      if (!p->raw && !p->no_axes) {
	int x;

	/* X-axis lines */
        for (x = -1; x <= p->cols; ++x) {
	  pixel(left + x, base - 1) = Grid;
	  pixel(left + x, base + p->rows) = Grid;
	}
      }
    }
  }

  if (!p->raw) {
    if (p->title) {
      int width = (int)strlen(p->title) * font_X;
      if (width < cols + 1) /* Title */
        print_at((cols - width) / 2, rows - font_y, Text, p->title);
    }

    if ((int)strlen(p->comment) * font_X < cols + 1)     /* Footer comment */
      print_at(1, font_y, Text, p->comment);

    {
      int label_width;

      /* X-axis */
      {
	int step;
	double dstep;
	double limit;
	char *prefix;
	char text[16];

	dstep = step =
	  axis(secs(p->cols), p->cols / (font_X * 9 / 2), &limit, &prefix);
	sprintf(text, "Time (%.1ss)", prefix);               /* Axis label */
	print_at(left + (p->cols - font_X * (int)strlen(text)) / 2, 24, Text, text);
	{ int i, di;
	  for (i = 0, di = 0; i <= limit; i += step, di += dstep) {
	    int x = limit? di / limit * p->cols + .5 : 0;
	    int y;

	    for (y = 0; y < tick_len; ++y) {                   /* Ticks */
	      pixel(left-1+x, below-1-y) = Grid;
	      pixel(left-1+x, below+c_rows+y) = Grid;
	    }
	    if (step == 5 && (i%10))
	      continue;
	    sprintf(text, "%g", .1 * di);     /* Tick labels */
	    x = left + x - 3 * strlen(text);
	    print_at(x, below - 6, Labels, text);
	    print_at(x, below + c_rows + 14, Labels, text);
	  }
	}
	/* Used subsequently to position the vertical text of the Y axis */
	label_width = font_X * strlen(text);
      }

      /* Y-axis */
      if (p->log10_axis) {
	/* Log Y axis ticks and labels */
	int start_decade = (int)log10_low_freq;
	int end_decade = (int)log10_high_freq;
	float log_scale = (float)p->rows / (log10_high_freq - log10_low_freq);

	print_up(10, below + (c_rows - label_width) / 2, Text, "Frequency (Hz)");

	{
	  int chan;

	  for (chan = 0; chan < chans; ++chan) {
	    int base = below + chan * (p->rows + 1);
	    int i;
	    float fi;

	    /* Label 10^n decades in view */
	    for (fi = i = start_decade; i <= end_decade; i++, fi++) {
	      int f = (int)powf(10.0, fi);

	      {
		int y = (fi - log10_low_freq) * log_scale;

		if (y >= 0) {
		  char text[16];
		  sprintf(text, i ? "%5i" : "   DC", f);  /* Tick label (left) */
		  print_at(left - 4 - font_X * 5, base + y + 5, Labels, text);
		  sprintf(text, i ? "%i" : "DC",  f);     /* Tick label (right) */
		  print_at(left + p->cols + 6, base + y + 5, Labels, text);
		}
	      }

	      /* intra-decade tick marks */
	      {
	        int j;

		for (j = 1; j <= 10; j++) {
		  int y = (log10f((float)(j * f)) - log10_low_freq) * log_scale;

		  if (y > 0 && y < p->rows) {
		    int x;

		    for (x = 0; x < tick_len; ++x) {
		      pixel(left - 1 - x, base + y) = Grid;
		      pixel(left + p->cols + x, base + y) = Grid;
		    }
		  }
		}
	      }
	    }
	  }
	}
      } else {
	/* Linear Y axis ticks and labels */
	double limit;
	char *prefix;
        char text[16]; /* exactly! */
	int step;
	double dstep;

	dstep = step = axis(p->high_freq - p->low_freq,
			    (p->rows - 1) / ((font_y * 3 + 1) >> 1),
			    &limit, &prefix);
	sprintf(text, "Frequency (%.1sHz)", prefix);         /* Axis label */
	print_up(10, below + (c_rows - font_X * strlen(text)) / 2, Text, text);
	{ int chan;
	  for (chan = 0; chan < chans; ++chan) {
	    int base = below + chan * (p->rows + 1);
	    int i;
	    double di;

	    for (di = i = 0; i <= limit; i += step, di += dstep) {
	      int f = p->low_freq/100 + i;       /* Frequency in 100Hz units */
	      int y = limit ? di / limit * (p->rows - 1) + .5 : 0;
	      int x;

	      for (x = 0; x < tick_len; ++x) {                 /* Ticks */
		pixel(left - 1 - x, base + y) = Grid;
	        pixel(left + p->cols + x, base + y) = Grid;
	      }
	      if ((step == 5 && (i % 10)) || (!i && chan && chans > 1))
		continue;

	      sprintf(text, f?"%5g":"   DC", .1 * f);         /* Tick labels */
	      print_at(left - 4 - font_X * 5, base + y + 5, Labels, text);
	      sprintf(text, f?"%g":"DC", .1 * f);
	      print_at(left + p->cols + 6, base + y + 5, Labels, text);
	    }
	  }
	}
      }
    }

    /* Z-axis */
    {
      int step; double dstep;
      int base, k;

      k = min(400, c_rows);
      base = below + (c_rows - k) / 2;
      print_at(cols - right - 2 - font_X, base - 13, Text, "dBFS");/* Axis label */
      {
        int y;

	for (y = 0; y < k; ++y) {                          /* Spectrum */
	  png_byte b = colour(p, p->dB_range * (y / (k - 1.) - 1));
	  int x;

	  for (x = 0; x < spectrum_width; ++x)
	    pixel(cols - right - 1 - x, base + y) = b;
	}
      }
      dstep = step = 10 * ceil(p->dB_range / 10. * (font_y + 2) / (k - 1));
      {
        int i; double di;

	/* (Tick) labels */
	for (di = i = 0; i <= p->dB_range; i += step, di += dstep) {
          char text[16];
	  int y = di / p->dB_range * (k - 1) + .5;

	  sprintf(text, "%+i", i - p->gain - p->dB_range - (int)(autogain+0.5));
	  print_at(cols - right + 1, base + y + 5, Labels, text);
	}
      }
    }
  }
  free(font);

  /* Create the PNG */
  {
    png_structp png;
    png_infop   png_info;
    png_bytepp  png_rows;
    FILE *      file;

    lsx_valloc(png_rows, rows);
    if (p->using_stdout) {
      SET_BINARY_MODE(stdout);
      file = stdout;
    } else {
      file = lsx_fopen(p->out_name, "wb");
      if (!file) {
	lsx_fail("failed to create `%s': %s", p->out_name, strerror(errno));
	goto error;
      }
    }
    png = png_create_write_struct(PNG_LIBPNG_VER_STRING, 0, 0,0);
    png_info = png_create_info_struct(png);
    png_init_io(png, file);
    png_set_PLTE(png, png_info, palette, fixed_palette + p->spectrum_points);
    png_set_IHDR(png, png_info, (png_uint_32)cols, (png_uint_32)rows, 8,
	PNG_COLOR_TYPE_PALETTE, PNG_INTERLACE_NONE,
	PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    { int row;
      for (row = 0; row < rows; ++row)    /* Put (0,0) at bottom-left of PNG */
	png_rows[rows - 1 - row] = (png_bytep)(pixels + row * cols);
    }
    png_set_rows(png, png_info, png_rows);
    png_write_png(png, png_info, PNG_TRANSFORM_IDENTITY, NULL);
    png_destroy_write_struct(&png, &png_info);
    if (!p->using_stdout)
      fclose(file);
error:
    free(png_rows);
  }
  free(pixels);
  free_tiles(p);
  free(p->buf);
  free(p->dft_buf);
  free(p->window);
  free(p->magnitudes);
#if HAVE_FFTW
  fftw_destroy_plan(p->fftw_plan);
#endif
  return SOX_SUCCESS;
}

static int end(sox_effect_t * effp)
{
  priv_t *p = (priv_t *)effp->priv;
  if (effp->flow == 0)
    return stop(effp);
  free_tiles(p);
#if HAVE_FFTW
  if (p->fftw_plan) fftw_destroy_plan(p->fftw_plan);
#endif
  return SOX_SUCCESS;
}

sox_effect_handler_t const * lsx_spectrogram_effect_fn(void)
{
  static char const usage[] = "[options]";
  static char const * const extra_usage[] = {
"-x num  X-axis size in pixels; default: derived or 800",
"-X num  X-axis pixels/second; default: derived or 100",
"-y num  Y-axis size in pixels per channel",
"-Y num  Total height; default 550",
"-z num  Z-axis range in dB; default 120",
"-Z num  Z-axis maximum in dBFS; default 0",
"-n      normalize: Set Z-axis maximum to the brightest pixel",
"-q num  Z-axis quantisation (0-249); default 249",
"-w name Window: Hann(default)/Hamming/Bartlett/Rectangular/Kaiser/Dolph",
"-W num  Window adjust parameter (-10-10); applies only to Kaiser/Dolph",
"-s      Slack overlap of windows",
"-a      Suppress axis lines",
"-r      Raw spectrogram; no axes or legends",
"-l      Light background",
"-m      Monochrome",
"-h      High colour",
"-L      Plot the frequency on logarithmic axis",
"-R L:H  Specify the frequency range (from L to H)",
"-p num  Permute colours (1-6); default 1",
"-A      Alternative, inferior, fixed colour-set (for compatibility only)",
"-t text Title text",
"-c text Comment text",
"-o text Output file name; default `spectrogram.png'",
"-d time Audio duration to fit to the X-axis",
"-S pos  Start the spectrogram at the given input time",
    NULL
  };
  static sox_effect_handler_t handler = {
    "spectrogram", usage, extra_usage, SOX_EFF_MODIFY,
    getopts, start, flow, drain, end, 0, sizeof(priv_t)};

  return &handler;
}
