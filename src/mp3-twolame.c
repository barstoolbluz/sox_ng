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

/* TWOLAME support for SoX */

#include "sox_i.h"

#if HAVE_TWOLAME

#include "mp3.h"
#include "mp3-twolame.h"

/* Twolame takes float values as input. */
#define MP2_TWOLAME_PRECISION   24

/* Note: sox_precision() returns 0 for SOX_ENCODING_MP2 and _MP3, as it varies
 * according to whether you're encoding or decoding but sox_ng.c knows
 * about this and has a special case and reports 24 as the Writes: precision */

#define LAME_BUFFER_SIZE(num_samples) (((num_samples) + 3) / 4 * 5 + 7200)

int startwrite_twolame(sox_format_t * ft)
{
  priv_t *p = (priv_t *) ft->priv;
  int openlibrary_result;

  LSX_DLLIBRARY_OPEN(
      p,
      twolame_dl,
      TWOLAME_FUNC_ENTRIES,
      "Twolame encoder library",
      twolame_library_names,
      openlibrary_result);
  if (openlibrary_result)
    return SOX_EOF;

  if ((p->opt = p->twolame_init()) == NULL){
    lsx_fail_errno(ft,SOX_EOF,"initialization of Twolame library failed");
    return SOX_EOF;
  }
  (void) p->twolame_set_verbosity(p->opt, 0);

  if (ft->encoding.encoding != SOX_ENCODING_MP2) {
    if(ft->encoding.encoding != SOX_ENCODING_UNKNOWN)
      lsx_report("encoding forced to MP2");
    ft->encoding.encoding = SOX_ENCODING_MP2;
  }

  ft->signal.precision = MP2_TWOLAME_PRECISION;

  if (ft->signal.channels != SOX_ENCODING_UNKNOWN) {
    if (p->twolame_set_num_channels(p->opt,(int)ft->signal.channels) != 0) {
      lsx_fail_errno(ft,SOX_EOF,"unsupported number of channels");
      return(SOX_EOF);
    }
  } else {
    ft->signal.channels = p->twolame_get_num_channels(p->opt); /* Twolame default */
  }

  p->twolame_set_in_samplerate(p->opt,(int)ft->signal.rate);
  p->twolame_set_out_samplerate(p->opt,(int)ft->signal.rate);

  lsx_debug("-C option is %f", ft->encoding.compression);

  if (ft->encoding.compression == HUGE_VAL) {
    /* Do nothing, use defaults: */
    lsx_report("using MP2 encoding defaults");
  } else {
    /* Twolame's CBR rates are exactly 32,48,64... and VBR rates a float
     * from -50 to +50 with "useful" values from -10 to +10 so we count
     * any floating point value from -50 to 50 as VBR unless it is exactly
     * 32 or 48, and anything else as CBR.
     */
    if (ft->encoding.compression >=-50 && ft->encoding.compression <= 50 &&
        ft->encoding.compression != 32 && ft->encoding.compression != 48) {
        if (p->twolame_set_VBR(p->opt, 1) != 0) {
          lsx_fail_errno(ft,SOX_EOF,"failed to enable MP2 VBR");
          return(SOX_EOF);
        }
        if (p->twolame_set_VBR_level(p->opt, ft->encoding.compression) != 0) {
          lsx_fail_errno(ft,SOX_EOF,"failed to set MP2 VBR level");
          return(SOX_EOF);
        }
    } else {
      if (p->twolame_set_brate(p->opt, (int)ft->encoding.compression) != 0) {
        lsx_fail_errno(ft, SOX_EOF, "invalid MP2 bitrate");
        return(SOX_EOF);
      }
      lsx_report("twolame set bitrate(%d)", (int)ft->encoding.compression);
    }
  }

  if (p->twolame_init_params(p->opt) != 0) {
    lsx_fail_errno(ft,SOX_EOF,"Twolame initialization failed");
    return(SOX_EOF);
  }

  p->mp3_buffer_size = LAME_BUFFER_SIZE(sox_globals.bufsiz / max(ft->signal.channels, 1));
  p->mp3_buffer = lsx_malloc(p->mp3_buffer_size);

  p->pcm_buffer_size = sox_globals.bufsiz * sizeof(float);
  p->pcm_buffer = lsx_malloc(p->pcm_buffer_size);

  return(SOX_SUCCESS);
}

size_t write_twolame(sox_format_t *ft, const sox_sample_t *buf, size_t samp)
{
  priv_t *p = (priv_t *)ft->priv;
  size_t new_buffer_size;
  float *buffer;
  int nsamples = samp/ft->signal.channels;
  int written = 0;
  SOX_SAMPLE_LOCALS;
  size_t s;

  new_buffer_size = samp * sizeof(float);
  if (p->pcm_buffer_size < new_buffer_size) {
    float *new_buffer = lsx_realloc(p->pcm_buffer, new_buffer_size);
    p->pcm_buffer_size = new_buffer_size;
    p->pcm_buffer = new_buffer;
  }

  buffer = p->pcm_buffer;

  for(s = 0; s < samp; s++)
    /* Apparently there is no need to clip count here */
    buffer[s] = SOX_SAMPLE_TO_FLOAT_32BIT_NOCLIPS(buf[s]);

  new_buffer_size = LAME_BUFFER_SIZE(nsamples);
  if (p->mp3_buffer_size < new_buffer_size) {
    unsigned char *new_buffer = lsx_realloc(p->mp3_buffer, new_buffer_size);
    p->mp3_buffer_size = new_buffer_size;
    p->mp3_buffer = new_buffer;
  }

  written = p->twolame_encode_buffer_float32_interleaved(p->opt, buffer,
            nsamples, p->mp3_buffer, (int)p->mp3_buffer_size);
  if (written < 0) {
    lsx_fail_errno(ft,SOX_EOF,"encoding failed");
    return 0;
  }

  if (lsx_writebuf(ft, p->mp3_buffer, (size_t)written) < (size_t)written) {
    lsx_fail_errno(ft,SOX_EOF,"file write failed");
    return 0;
  }

  return samp;
}

int stopwrite_twolame(sox_format_t * ft)
{
  priv_t *p = (priv_t *) ft->priv;
  int written = 0;

  written = p->twolame_encode_flush(p->opt, p->mp3_buffer, (int)p->mp3_buffer_size);
  if (written < 0)
    lsx_fail_errno(ft, SOX_EOF, "encoding failed");
  else if (lsx_writebuf(ft, p->mp3_buffer, (size_t)written) < (size_t)written)
    lsx_fail_errno(ft, SOX_EOF, "file write failed");

  free(p->mp3_buffer);
  free(p->pcm_buffer);

  p->twolame_close(&p->opt);
  LSX_DLLIBRARY_CLOSE(p, twolame_dl);

  return SOX_SUCCESS;
}

#if 0
LSX_FORMAT_HANDLER(twolame)
{
  static char const * const names[] = {"twolame", "mp2", NULL};
  static unsigned const write_encodings[] = {
    SOX_ENCODING_MP2, 0, 0};
  static sox_rate_t const write_rates[] = {
    16000, 22050, 24000, 32000, 44100, 48000, 0};
  static sox_format_handler_t const handler = {SOX_LIB_VERSION_CODE,
    "MPEG-1 Layer 2 lossy audio compression", names, 0,
    NULL, NULL, NULL,
    startwrite_twolame, write_twolame, stopwrite_twolame,
    NULL, write_encodings, write_rates, sizeof(priv_t)
  };
  return &handler;
}
#endif

#endif /* HAVE_TWOLAME */
