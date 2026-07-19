/* libSoX effect: change tempo (and duration) or pitch (maintain duration)
 * Copyright (c) 2007,8 robs@users.sourceforge.net
 * Based on ideas from Olli Parviainen's SoundTouch Library.
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
#include "fifo.h"

typedef struct {
  /* Configuration parameters: */
  size_t channels;
  sox_bool quick_search; /* Whether to quick search or linear search */
  double factor;         /* 1 for no change, < 1 for slower, > 1 for faster. */
  size_t search;         /* Wide samples to search for best overlap position */
  size_t segment;        /* Processing segment length in wide samples */
  size_t overlap;        /* In wide samples */

  size_t process_size;   /* # input wide samples needed to process 1 segment */

  /* Buffers: */
  fifo_t input_fifo;
  float * overlap_buf;
  fifo_t output_fifo;

  /* Counters: */
  uint64_t samples_in;
  uint64_t samples_out;
  uint64_t segments_total;
  uint64_t skip_total;
} tempo_t;

/* Waveform Similarity by least squares; works across multi-channels */
static float difference(const float * a, const float * b, size_t length)
{
  float diff = 0;
  size_t i = 0;

  #define _ diff += sqr(a[i] - b[i]), ++i; /* Loop optimization */
  do {_ _ _ _ _ _ _ _} while (i < length); /* N.B. length == 0 (mod 8) */
  #undef _
  return diff;
}

/* Find where the two segments are most alike over the overlap period. */
static size_t tempo_best_overlap_position(tempo_t * t, float const * new_win)
{
  float * f = t->overlap_buf;
  size_t j, best_pos, prev_best_pos = (t->search + 1) >> 1, step = 64;
  size_t i = best_pos = t->quick_search? prev_best_pos : 0;
  float diff, least_diff = difference(new_win + t->channels * i, f, t->channels * t->overlap);
  int k = 0;

  if (t->quick_search) do { /* hierarchical search */
    for (k = -1; k <= 1; k += 2) for (j = 1; j < 4 || step == 64; ++j) {
      i = prev_best_pos + k * j * step;
      if ((int)i < 0 || i >= t->search)
        break;
      diff = difference(new_win + t->channels * i, f, t->channels * t->overlap);
      if (diff < least_diff)
        least_diff = diff, best_pos = i;
    }
    prev_best_pos = best_pos;
  } while (step >>= 2);
  else for (i = 1; i < t->search; i++) { /* linear search */
    diff = difference(new_win + t->channels * i, f, t->channels * t->overlap);
    if (diff < least_diff)
      least_diff = diff, best_pos = i;
  }
  return best_pos;
}

static void tempo_overlap(
    tempo_t * t, const float * in1, const float * in2, float * output)
{
  size_t i, j, k = 0;
  float fade_step = 1.0f / (float) t->overlap;

  for (i = 0; i < t->overlap; ++i) {
    float fade_in  = fade_step * (float) i;
    float fade_out = 1.0f - fade_in;
    for (j = 0; j < t->channels; ++j, ++k)
      output[k] = in1[k] * fade_out + in2[k] * fade_in;
  }
}

static void tempo_process(tempo_t * t)
{
  while (lsx_fifo_occupancy(&t->input_fifo) >= t->process_size) {
    size_t skip, offset;

    /* Copy or overlap the first bit to the output */
    if (!t->segments_total) {
      offset = t->search / 2;
      lsx_fifo_write(&t->output_fifo, t->overlap, (float *) lsx_fifo_read_ptr(&t->input_fifo) + t->channels * offset);
    } else {
      offset = tempo_best_overlap_position(t, lsx_fifo_read_ptr(&t->input_fifo));
      tempo_overlap(t, t->overlap_buf,
          (float *) lsx_fifo_read_ptr(&t->input_fifo) + t->channels * offset,
          lsx_fifo_write(&t->output_fifo, t->overlap, NULL));
    }
    /* Copy the middle bit to the output */
    lsx_fifo_write(&t->output_fifo, t->segment - 2 * t->overlap,
               (float *) lsx_fifo_read_ptr(&t->input_fifo) +
               t->channels * (offset + t->overlap));

    /* Copy the end bit to overlap_buf ready to be mixed with
     * the beginning of the next segment. */
    memcpy(t->overlap_buf,
           (float *) lsx_fifo_read_ptr(&t->input_fifo) +
           t->channels * (offset + t->segment - t->overlap),
           t->channels * t->overlap * sizeof(*(t->overlap_buf)));

    /* Advance through the input stream */
    skip = t->factor * (++t->segments_total * (t->segment - t->overlap)) + 0.5;
    t->skip_total += skip -= t->skip_total;
    lsx_fifo_read(&t->input_fifo, skip, NULL);
  }
}

static float * tempo_input(tempo_t * t, float const * samples, size_t n)
{
  t->samples_in += n;
  return lsx_fifo_write(&t->input_fifo, n, samples);
}

static float const * tempo_output(tempo_t * t, float * samples, size_t * n)
{
  t->samples_out += *n = min(*n, lsx_fifo_occupancy(&t->output_fifo));
  return lsx_fifo_read(&t->output_fifo, *n, samples);
}

/* Flush samples remaining in overlap_buf & input_fifo to the output. */
static void tempo_flush(tempo_t * t)
{
  uint64_t samples_out = t->samples_in / t->factor + .5;
  size_t remaining = samples_out > t->samples_out ?
      (size_t)(samples_out - t->samples_out) : 0;
  float * buff;

  lsx_vcalloc(buff, 128 * t->channels);

  if (remaining > 0) {
    while (lsx_fifo_occupancy(&t->output_fifo) < remaining) {
      tempo_input(t, buff, (size_t) 128);
      tempo_process(t);
    }
    lsx_fifo_trim_to(&t->output_fifo, remaining);
    t->samples_in = 0;
  }
  free(buff);
}

static void tempo_setup(tempo_t * t,
  double sample_rate, sox_bool quick_search, double factor,
  double segment_ms, double search_ms, double overlap_ms)
{
  size_t max_skip;
  t->quick_search = quick_search;
  t->factor = factor;
  t->segment = sample_rate * segment_ms / 1000 + .5;
  t->search  = sample_rate * search_ms / 1000 + .5;
  t->overlap = max(sample_rate * overlap_ms / 1000 + 4.5, 16);
  t->overlap &= ~7; /* Make divisible by 8 for loop optimization */
  if (t->overlap * 2 > t->segment)
    t->overlap -= 8;
  lsx_valloc(t->overlap_buf, t->overlap * t->channels);
  max_skip = ceil(factor * (t->segment - t->overlap));
  t->process_size = max(max_skip + t->overlap, t->segment) + t->search;
  memset(lsx_fifo_reserve(&t->input_fifo, t->search / 2), 0, (t->search / 2) * t->channels * sizeof(float));
}

static void tempo_delete(tempo_t * t)
{
  free(t->overlap_buf);
  lsx_fifo_delete(&t->output_fifo);
  lsx_fifo_delete(&t->input_fifo);
  free(t);
}

static tempo_t * tempo_create(size_t channels)
{
  tempo_t * t = lsx_calloc(1, sizeof(*t));
  t->channels = channels;
  lsx_fifo_create(&t->input_fifo, t->channels * sizeof(float));
  lsx_fifo_create(&t->output_fifo, t->channels * sizeof(float));
  return t;
}

/*------------------------------- SoX Wrapper --------------------------------*/

typedef struct {
  tempo_t     * tempo;
  sox_bool    quick_search;
  double      factor, segment, search, overlap; /* the last three are in ms */
} priv_t;

static int getopts_tempo(sox_effect_t * effp, int argc, char **argv)
{
  priv_t * p = (priv_t *)effp->priv;
  enum {Default, Music, Speech, Linear} profile = Default;
  static const double segments_ms [] = {   82,82,  35  , 20};
  static const double segments_pow[] = {    0, 1, .33  , 1};
  static const double overlaps_div[] = {6.833, 7,  2.5 , 2};
  static const double searches_div[] = {5.587, 6,  2.14, 2};
  int c;
  lsx_getopt_t optstate;
  lsx_getopt_init(argc, argv, "+qmls", NULL, lsx_getopt_flag_none, 1, &optstate);

  p->factor = p->segment = p->search = p->overlap = HUGE_VAL;
  while ((c = lsx_getopt(&optstate)) != -1) switch (c) {
    case 'q': p->quick_search  = sox_true;   break;
    case 'm': profile = Music; break;
    case 's': profile = Speech; break;
    case 'l': profile = Linear; p->search = 0; break;
    default: lsx_fail("invalid option `-%c'", optstate.opt); return lsx_usage(effp);
  }
  argc -= optstate.ind, argv += optstate.ind;
  if (argc < 1) {	/* The "factor" parameter is obbligatory */
    lsx_fail("the `factor' parameter is obligatory");
    return SOX_EOF;
  }
  do {                    /* break-able block */
    NUMERIC_PARAMETER(factor   ,0.1 , 100 )
    NUMERIC_PARAMETER(segment  , 10 , 120)
    NUMERIC_PARAMETER(search   , 0  , 30 )
    NUMERIC_PARAMETER(overlap  , 0  , 30 )
  } while (0);

  if (argc) {
    /* Either there was stuff that we didn't understand as numbers
     * (starting with something not a digit, plus or minus)
     * or they gave more than four arguments */
    if (p->overlap != HUGE_VAL)
      lsx_fail("superfluous argument `%s'", *argv);
    else
      lsx_fail("%s `%s' is not a number",
        p->factor == HUGE_VAL ? "factor" :
        p->segment == HUGE_VAL ? "segment" :
        p->search == HUGE_VAL ? "search" :
        p->overlap == HUGE_VAL ? "overlap" :
	"argument", /* "can't happen" */
      *argv);
    return SOX_EOF;
  }

  if (p->segment == HUGE_VAL)
    p->segment = max(10, segments_ms[profile] / max(pow(p->factor, segments_pow[profile]), 1));
  if (p->overlap == HUGE_VAL)
    p->overlap = p->segment / overlaps_div[profile];
  if (p->search == HUGE_VAL)
    p->search = p->segment / searches_div[profile];

  p->overlap = min(p->overlap, p->segment / 2);
  lsx_report("quick_search=%u factor=%g segment=%g search=%g overlap=%g",
    p->quick_search, p->factor, p->segment, p->search, p->overlap);
  return SOX_SUCCESS;
}

static int start_tempo(sox_effect_t * effp)
{
  priv_t * p = (priv_t *)effp->priv;

  if (p->factor == 1)
    return SOX_EFF_NULL;

  p->tempo = tempo_create((size_t)effp->in_signal.channels);
  tempo_setup(p->tempo, effp->in_signal.rate, p->quick_search, p->factor,
      p->segment, p->search, p->overlap);

  effp->out_signal.length = SOX_UNKNOWN_LEN;
  if (effp->in_signal.length != SOX_UNKNOWN_LEN) {
    uint64_t in_length = effp->in_signal.length / effp->in_signal.channels;
    uint64_t out_length = in_length / p->factor + .5;
    effp->out_signal.length = out_length * effp->in_signal.channels;
  }

  return SOX_SUCCESS;
}

static int flow_tempo(sox_effect_t * effp, const sox_sample_t * ibuf,
                sox_sample_t * obuf, size_t * isamp, size_t * osamp)
{
  priv_t * p = (priv_t *)effp->priv;
  size_t i, odone = *osamp /= effp->in_signal.channels;
  float const * s = tempo_output(p->tempo, NULL, &odone);
  SOX_SAMPLE_LOCALS;

  for (i = 0; i < odone * effp->in_signal.channels; ++i)
    *obuf++ = SOX_FLOAT_32BIT_TO_SAMPLE(*s++, effp->clips);

  if (*isamp && odone < *osamp) {
    float * t = tempo_input(p->tempo, NULL, *isamp / effp->in_signal.channels);
    for (i = *isamp; i; --i)
      *t++ = SOX_SAMPLE_TO_FLOAT_32BIT(*ibuf++, effp->clips);
    tempo_process(p->tempo);
  }
  else *isamp = 0;

  *osamp = odone * effp->in_signal.channels;
  return SOX_SUCCESS;
}

static int drain_tempo(sox_effect_t * effp, sox_sample_t * obuf, size_t * osamp)
{
  priv_t * p = (priv_t *)effp->priv;
  static size_t isamp = 0;
  tempo_flush(p->tempo);
  return flow_tempo(effp, 0, obuf, &isamp, osamp);
}

static int stop_tempo(sox_effect_t * effp)
{
  priv_t * p = (priv_t *)effp->priv;
  tempo_delete(p->tempo);
  return SOX_SUCCESS;
}

sox_effect_handler_t const * lsx_tempo_effect_fn(void)
{
  static const char usage[] =
    "[-q] [-m|-s|-l] factor [segment [search [overlap]]]";

  static char const * const extra_usage[] = {
"-q      Use tree searches instead of linear ones",
"-m      Optimize segment, search and overlap for music",
"-s      Optimize segment, search and overlap for speech",
"-l      Optimize segment, search and overlap for linear processing",
"OPTION  RANGE DEFAULT DESCRIPTION",
"factor  .1-100        Ratio of new tempo to old; >1 speeds up, <1 slows down",
"segment 10-120 82     Segment size in milliseconds",
"search   0-30  14.68  Audio length over which to search for overlap point",
"overlap  0-30  12     Segment overlap length in milliseconds",
    NULL
  };

  static sox_effect_handler_t handler = {
    "tempo", usage, SOX_EFF_MCHAN | SOX_EFF_LENGTH,
    getopts_tempo, start_tempo, flow_tempo, drain_tempo, stop_tempo, NULL,
    sizeof(priv_t), extra_usage, NULL, NULL,
  };
  return &handler;
}

/*---------------------------------- pitch -----------------------------------*/
#define log2(n) log(n)/log(2.)

static int getopts_pitch(sox_effect_t * effp, int argc, char **argv)
{
  double cents, factor;
  char dummy, arg[100], **argv2;
  int result, pos = (argc > 1 && !strcmp(argv[1], "-q"))? 2 : 1;

  lsx_valloc(argv2, argc);
  if (argc <= pos || sscanf(argv[pos], "%lf %c", &cents, &dummy) != 1) {
    lsx_fail("cannot parse shift factor `%s'", argv[pos]);
    return SOX_EOF;
  }

  factor = pow(2., cents / 1200);  /* cents --> factor */
  /* "1 / factor" can be from 0.1 to 100, so "cents"... */
  if (1/factor < 0.1 || 1/factor > 100) {
    /* Inverting the formula... */
    lsx_fail("pitch's cents `%g' must be from %g to %g\n", cents,
            1200 * log2(1/100.0), 1200 * log2(1/0.1));
    return SOX_EOF;
  }
  sprintf(arg, "%g", 1 / factor);
  memcpy(argv2, argv, argc * sizeof(*argv2));
  argv2[pos] = arg;
  result = getopts_tempo(effp, argc, argv2);
  free(argv2);
  return result;
}

static int start_pitch(sox_effect_t * effp)
{
  priv_t * p = (priv_t *) effp->priv;
  int result = start_tempo(effp);

  effp->out_signal.rate = effp->in_signal.rate / p->factor;
  return result;
}

sox_effect_handler_t const * lsx_pitch_effect_fn(void)
{
  static sox_effect_handler_t handler;
  static char const * const usage = "[-q] shift [segment [search [overlap]]]";
  static char const * const extra_usage[] = {
"-q      Use tree searches instead of linear ones",
"OPTION  RANGE  DFLT  DESCRIPTION",
"shift    any         Pitch shift in cents; >0 is higher, <0 is lower",
"segment .1-100 82    Segment size in milliseconds",
"search   0-30  14.68 Audio length in ms in which to search for overlap point",
"overlap  0-30  12    Segment overlap length in milliseconds",
    NULL
  };

  handler = *lsx_tempo_effect_fn();
  handler.name = "pitch";
  handler.usage = usage;
  handler.extra_usage = extra_usage;
  handler.getopts = getopts_pitch;
  handler.start = start_pitch;
  handler.flags &= ~SOX_EFF_LENGTH;
  handler.flags |= SOX_EFF_RATE;
  return &handler;
}
