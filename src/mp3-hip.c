/* Copyright (C) 2002 Fabrizio Gennari <fabrizio.ge@tiscali.it>
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

/* libmp3lame decoding support for SoX */

#include "sox_i.h"
#include "mp3.h"
#include "mp3-hip.h"

#if HAVE_LAME

/* The size of the char buffer used to pass MPEG data to the decoder */
#define HIP_MPEG_BUFSIZ 4096
/* The decoder returns up to 1152 samples per channel */
#define HIP_PCM_BUFSIZ 1152

/* Names for the mp3data.mode field */
static char *mode_string[] = { /* The meanings of mp3data.mode */
  "STEREO", "JOINT STEREO", "DUAL CHANNEL", "MONO"
};

/*
 * Adapters for message callbacks.
 */

/* The fmt string has a trailing newline which we don't want,
 * but we can't modify the const string we're passed so take a copy
 * and modify that.
 */
static char *unnewline(char const *in)
{
  char *out = lsx_strdup(in);
  char *newline = strrchr(out, '\n');

  if (newline) *newline = '\0';

  return out;
}

static void errorf(const char* fmt, va_list va)
{
  /* What they call "errors" are actually warnings
   * and they carry on all the same, hence 2, not 1.
   */
  char *fmt2 = unnewline(fmt);
  sox_globals.subsystem=__FILE__;
  if (sox_globals.output_message_handler)
    (*sox_globals.output_message_handler)(2,sox_globals.subsystem,fmt2,va);
  free(fmt2);
  return;
}

static void msgf(const char* fmt, va_list va)
{
  char *fmt2 = unnewline(fmt);
  sox_globals.subsystem=__FILE__;
  if (sox_globals.output_message_handler)
    (*sox_globals.output_message_handler)(3,sox_globals.subsystem,fmt2,va);
  free(fmt2);
  return;
}

static void debugf(const char* fmt, va_list va)
{
  char *fmt2 = unnewline(fmt);
  sox_globals.subsystem=__FILE__;
  if (sox_globals.output_message_handler)
    (*sox_globals.output_message_handler)(4,sox_globals.subsystem,fmt2,va);
  free(fmt2);
  return;
}

/* MP1/2/3 decoding using LAME's built-in mpglib */

/* The way you use mpglib (hip_decode*()) is:
 * - Read in 4096 bytes of MPEG data
 * - Call hip_decode1_headersB() on it. It returns 0.
 * - Read in another 4096 bytes of data and pass them to hip_decode1_headersB()
 * - Repeat until it returns you a non-zero result.
 *   - if it's -1, there was a decoding error
 *   - if it's positive there are samples to be taken from the PCM buffers.
 *     - At this point, enc_delay and enc_padding should have been filled in
 *       (but I'm getting 0 and 0 always) and the contents of mp3data
 *       are almost certainly valid. You can check by seeing if
 *       mp3data.header_parsed is true.
 *     - Copy those samples and call hip_decode1_headers() with a size of 0
 *     - It gives you more samples
 *     - Keep doing that until it returns you zero.
 *     - Read more MPEG data, give it to hip_decode1_headers() and repeat the
 *       above dance until it returns 0 and you don't have any more MPEG data
 *       to give it. At this point your output is complete
 *
 * This gives you a WAV file that, if you trim off the first 529 silent samples,
 * is bitwise identical to the audio data in the output of lame --decode.
 *
 * "lame --decode" itself is more complex: it parses the header itself before
 * calling hip_decode1_headers(), to find out if it's MP1, MP2 or MP3,
 * and is able to decode some files where hip alone fails to parse the header.
 */

/* Start decoding MPEG data. */
int startread_hip(sox_format_t * ft)
{
  priv_t *p = (priv_t *) ft->priv;
  sox_bool ignore_length = ft->signal.length == SOX_IGNORE_LENGTH;
  int openlibrary_result;
  int enc_delay, enc_padding;  /* The values hip_decode1_headersB() fills in */
  size_t skip_start, LSX_UNUSED skip_end; /* What those mean in sample frames */
  int nout;                    /* What hip_decode1_headers() returned */

  LSX_DLLIBRARY_OPEN(
      p,
      lame_dl,
      LAME_FUNC_ENTRIES,
      "LAME decoder library",
      lame_library_names,
      openlibrary_result);
  if (openlibrary_result)
    return SOX_EOF;

  if ((p->gfp = p->lame_init()) == NULL){
    lsx_fail_errno(ft,SOX_EOF,"initialization of LAME library failed");
    return SOX_EOF;
  }
  p->lame_set_errorf(p->gfp,errorf);
  p->lame_set_msgf  (p->gfp,msgf);
  p->lame_set_debugf(p->gfp,debugf);
  lame_set_decode_only(p->gfp, 1);
  if(lame_init_params(p->gfp) == -1) {
    lsx_fail("parameters failed to initialize");
    return SOX_EOF;
  }

  p->hip = p->hip_decode_init();
  p->hip_set_errorf(p->hip, &errorf);
  p->hip_set_msgf  (p->hip, &msgf);
  p->hip_set_debugf(p->hip, &debugf);

  lsx_valloc(p->hip_buffer, HIP_MPEG_BUFSIZ);
  lsx_valloc(p->hip_pcm_l,  HIP_PCM_BUFSIZ);
  lsx_valloc(p->hip_pcm_r,  HIP_PCM_BUFSIZ);
  p->hip_mp3data.nsamp = ULONG_MAX;

  /* Read the file until hip has filled in the mp3data from the headers */
  do {
    size_t nread = lsx_readbuf(ft, p->hip_buffer, HIP_MPEG_BUFSIZ);
    if (nread == 0) {
      /* EOF before decoding the header */
      lsx_fail("file ends before MPEG headers are decoded");
      return SOX_EOF;
    }
    enc_delay = 0; enc_padding = 0; /* Sometimes it doesn't set them */
    nout = p->hip_decode1_headersB(p->hip, p->hip_buffer, nread,
                                   p->hip_pcm_l, p->hip_pcm_r,
                                   &(p->hip_mp3data),
                                   &enc_delay, &enc_padding);
    if (nout < 0) {
      lsx_fail("decoding error in file header");
      return SOX_EOF;
    }
  } while (nout == 0);
  p->hip_pending = nout;

  if (!p->hip_mp3data.header_parsed) {
    lsx_fail("hip was unable to parse the MPEG header");
    return SOX_EOF;
  }
  lsx_report("decoding %s with samplerate %d",
             mode_string[p->hip_mp3data.mode],
             p->hip_mp3data.samplerate);

  /* LAME finds out if it's MP1, MP2 or MP3 by scanning the header itself
   * before calling hip_*() and this information seems not to be available
   * when using just the hip interface. */
  if (ft->encoding.encoding == SOX_ENCODING_UNKNOWN)
    ft->encoding.encoding = SOX_ENCODING_MP3;
  /*
   * Calculation of enc_delay and enc_padding from
   * lame/frontend/get_audio.c:setSkipStartAndEnd()
   */
  skip_start = 0;
  switch (ft->encoding.encoding) {
  case SOX_ENCODING_MP3:
    if (enc_delay > -1)
      skip_start = enc_delay + 528 + 1;
    if (enc_padding > 528 + 1)
      skip_end = enc_padding - (528 + 1);
    break;
  case SOX_ENCODING_MP2:
  case SOX_ENCODING_MP1:
    skip_start = 240 + 1;
    break;
  default:
    lsx_fail("MPEG file is said to have encoding %u", ft->encoding.encoding);
    return SOX_EOF;
  }

  /* We've never seen enc_padding being significant and enc_delay is always
   * less than the first batch of samples, so just dump them here. */
  if (skip_start > p->hip_pending) {
    lsx_warn("the first %zu silent samples are bogus",
             skip_start - p->hip_pending);
    p->hip_pending = 0;
  } else if (skip_start) {
    size_t n = skip_start;           /* How many samples to discard */
    size_t remaining = p->hip_pending - n;  /* How many to keep */
    size_t i;

    lsx_report("stripping %zd samples from the start", n);

    switch (ft->signal.channels) {
    case 1:
      for (i=0; i<remaining; i++)
        p->hip_pcm_l[i] = p->hip_pcm_l[i + n];
      break;
    case 2:
      for (i=0; i<remaining; i++) {
        p->hip_pcm_l[i] = p->hip_pcm_l[i + n];
        p->hip_pcm_r[i] = p->hip_pcm_r[i + n];
      }
      break;
    }
    p->hip_pending -= n;
  }

  switch (p->hip_mp3data.stereo) {
  case 1: case 2:
    ft->signal.channels = p->hip_mp3data.stereo;
    break;
  default:
    lsx_fail("MPEG file seems to have %d channels", p->hip_mp3data.stereo);
    return SOX_EOF;
  }
  ft->signal.precision = 16; /* LAME decoder always returns short ints */
  ft->signal.rate = p->hip_mp3data.samplerate;

  ft->signal.length = SOX_UNSPEC;
  if (!ignore_length) {
    if (p->hip_mp3data.totalframes > 0) {
      ft->signal.length = p->hip_mp3data.totalframes * ft->signal.channels;
      lsx_report("the file length from totalframes is %g seconds",
                 (double)(ft->signal.length / ft->signal.channels)
                 / ft->signal.rate);
    } else if (p->hip_mp3data.nsamp != ULONG_MAX) {
      ft->signal.length = p->hip_mp3data.nsamp * ft->signal.channels;
      lsx_report("the file length from nsamp is %g seconds",
                 (double)(ft->signal.length / ft->signal.channels)
                 / ft->signal.rate);
    } else if (ft->seekable) {
      /* This seems to get it wrong by one sample */
#if 0
      /* try to figure out num_samples from file size
       * assuming 2 bytes per sample
       */
      double flen = lsx_filelength(ft);
      if (flen >= 0 && p->hip_mp3data.bitrate > 0) {
        double totalseconds =
           (flen * 8.0 / (1000.0 * p->hip_mp3data.bitrate));
        unsigned long tmp_num_samples =
            totalseconds * lame_get_in_samplerate(p->gfp) + .5;

        (void) p->lame_set_num_samples(p->gfp, tmp_num_samples);
        ft->signal.length = tmp_num_samples;
        lsx_warn("Guessed the duration as %g from the file length",
                 (double)(ft->signal.length / ft->signal.channels)
                 / ft->signal.rate);
      }
#endif
    }
  }

  return SOX_SUCCESS;
}

size_t read_hip(sox_format_t *ft, sox_sample_t *buf, size_t samp)
{
  priv_t *p = (priv_t *) ft->priv;
  sox_sample_t *bufp = buf;
  size_t nwritten = 0; /* Number of samples already written into buf */
  SOX_SAMPLE_LOCALS;

  while (nwritten < samp) {
    /* If we still have some decoded data, return that first */
    if (p->hip_pending) {
      /* Number of sample frames to copy */
      size_t n = min(p->hip_pending, (samp - nwritten) / ft->signal.channels);
      size_t i;

      /* If the number of channels changed in mid-stream, compensate */
      if (ft->signal.channels == 1 && p->hip_mp3data.stereo == 2) {
        /* Mix stereo back to mono */
        for (i=0; i < n; i++)
          *bufp++ = SOX_SIGNED_16BIT_TO_SAMPLE(
                     (p->hip_pcm_l[i] + p->hip_pcm_r[i]) / 2, clips);
        nwritten += n;
      } else if (ft->signal.channels == 2 && p->hip_mp3data.stereo == 1) {
        /* Duplicate mono samples to stereo */
        for (i=0; i < n; i++) {
          sox_sample_t s = SOX_SIGNED_16BIT_TO_SAMPLE(p->hip_pcm_l[i], clips);
          *bufp++ = s; *bufp++ = s;
        }
        nwritten += 2 * n;
      } else switch (ft->signal.channels) {
      case 1:
        for (i=0; i < n; i++) {
          *bufp++ = SOX_SIGNED_16BIT_TO_SAMPLE(p->hip_pcm_l[i], clips);
        }
        nwritten += n;
        break;
      case 2:
        for (i=0; i < n; i++) {
          *bufp++ = SOX_SIGNED_16BIT_TO_SAMPLE(p->hip_pcm_l[i], clips);
          *bufp++ = SOX_SIGNED_16BIT_TO_SAMPLE(p->hip_pcm_r[i], clips);
        }
        nwritten += 2 * n;
        break;
      }
      /* If there were more pending samples than requested,
       * move the unused ones to the start of the buffer
       * and adjust the pending count.
       * At this point, hip_pending is the number of samples that were left
       * and n the number we took.
       * We should do this with memmove but I can't seem to get it right.
       */
      if (n < p->hip_pending) {
        size_t remaining = p->hip_pending - n;
        switch (p->hip_mp3data.stereo) {
          size_t i;
        case 1:
          for (i=0; i<remaining; i++)
            p->hip_pcm_l[i] = p->hip_pcm_l[i + n];
          break;
        case 2:
          for (i=0; i<remaining; i++) {
            p->hip_pcm_l[i] = p->hip_pcm_l[i + n];
            p->hip_pcm_r[i] = p->hip_pcm_r[i + n];
          }
          break;
        }
        p->hip_pending = remaining;

        /* Our output buffer is already full */
        return nwritten;
      } else {
        p->hip_pending = 0;
      }
    }

    /* No pending samples; get some more */
    if (nwritten < samp) {
      mp3data_struct mp3data; /* The format of this frame */
      int nout;               /* The value returned by hip_decode1_headers() */

      /* See if it's got any more data for us */
      nout = p->hip_decode1_headers(p->hip, p->hip_buffer, 0,
                                    p->hip_pcm_l, p->hip_pcm_r, &mp3data);

      if (nout < 0) {
        lsx_fail("decoding error");
        return nwritten;
      }
      while (nout == 0) {
        size_t nread = lsx_readbuf(ft, p->hip_buffer, HIP_MPEG_BUFSIZ);
        if (nread == 0) {
          /* No pending samples from hip, no more MPEG data - decoding ends */
          return nwritten;
        }
        nout = p->hip_decode1_headers(p->hip, p->hip_buffer, nread,
                                      p->hip_pcm_l, p->hip_pcm_r, &mp3data);
        if (nout < 0) {
          lsx_fail("decoding error in read_hip");
          return nwritten;
        }

        /* It put samples in pcm_l and pcm_r and filled the header
         * with the parameters for this frame. If they've changed,
         * (some stereo MP3 files contain mono frames) warn.
         */
        /* Changing between DUAL_MONO, STEREO and JOINT_STEREO is not
         * uncommon but harmless */
        if (mp3data.mode != p->hip_mp3data.mode) {
          lsx_warn("MPEG mode changed from %s to %s",
                   mode_string[p->hip_mp3data.mode],
                   mode_string[mp3data.mode]);
          p->hip_mp3data.mode = mp3data.mode;
        }
        if (mp3data.stereo != p->hip_mp3data.stereo) {
          /* Warn when a change happens */
          lsx_warn("MPEG channels changed from %d to %d%s",
                   p->hip_mp3data.stereo, mp3data.stereo,
                   ((unsigned)mp3data.stereo != ft->signal.channels)
                     ? "; compensating..." : "");
          /* This very rarely happens. Rarer instead is when it reports
           * the number of channels changing to 0
           */
          p->hip_mp3data.stereo = mp3data.stereo;
          /* The difference is noticed and stereoified or monized when
           * ft->signal.channels != p->hip_mp3data.stereo
           */
        }
        if (mp3data.samplerate != p->hip_mp3data.samplerate) {
          lsx_warn("MPEG samplerate changed from %d to %d",
                   p->hip_mp3data.samplerate, mp3data.samplerate);
          p->hip_mp3data.samplerate = mp3data.samplerate;
        }
      }
      p->hip_pending = nout;
    }
  }
  return nwritten;
}

int stopread_hip(sox_format_t * ft)
{
  priv_t *p = (priv_t *) ft->priv;

  hip_decode_exit(p->hip);
  free(p->hip_buffer); p->hip_buffer = NULL;
  free(p->hip_pcm_l);  p->hip_pcm_l = NULL;
  free(p->hip_pcm_r);  p->hip_pcm_r = NULL;
  lame_close(p->gfp);
  LSX_DLLIBRARY_CLOSE(p, lame_dl);
  return SOX_SUCCESS;
}

LSX_FORMAT_HANDLER(hip)
{
  static char const * const names[] = {"hip", "mp3", NULL};
  static sox_format_handler_t const handler = {SOX_LIB_VERSION_CODE,
    "MPEG-1 Layer 3 lossy audio decompression", names, 0,
    startread_hip, read_hip, stopread_hip,
    NULL, NULL, NULL,
    NULL, NULL, NULL, sizeof(priv_t)
  };
  return &handler;
}

#endif /* HAVE_LAME */
