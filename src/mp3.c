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

/* MP3 support for SoX
 *
 * Uses libmad for MP3 decoding
 * libmp3lame for MP3 encoding
 * and libtwolame for MP2 encoding
 */

#include "sox_i.h"
#include "mp3.h"
#include "mp3-mad.h"
#include "mp3-lame.h"
#include "mp3-hip.h"
#include "mp3-twolame.h"

#if HAVE_MAD
/* MAD can tell the difference between Layer 1 and Layer 2 but decodes them both
 * with the same decoder (Layer 1 is a subset of Layer 2) but soxi will
 * tell people if they have an MP1 file */
LSX_FORMAT_HANDLER(mp1)
{
  static char const * const names[] = {"mp1", "m1a", NULL};
  static sox_format_handler_t const handler = {SOX_LIB_VERSION_CODE,
    "MPEG-1 Layer 1 lossy audio compression", names, 0,
    startread_mad, read_mad, stopread_mad,
    NULL, NULL, NULL,
    seek_mad, NULL, NULL, sizeof(priv_t)
  };
  return &handler;
}
#endif

#if HAVE_MAD || HAVE_TWOLAME
LSX_FORMAT_HANDLER(mp2)
{
  static char const * const names[] = {"mp2", NULL};
  static unsigned const write_encodings[] = {
    SOX_ENCODING_MP2, 0, 0};
  static sox_rate_t const write_rates[] = {
    16000, 22050, 24000, 32000, 44100, 48000, 0};
  static sox_format_handler_t const handler = {SOX_LIB_VERSION_CODE,
    "MPEG-1 Layer 2 lossy audio compression", names, 0,
    startread_mad, read_mad, stopread_mad,
    startwrite_twolame, write_twolame, stopwrite_twolame,
    seek_mad, write_encodings, write_rates, sizeof(priv_t)
  };
  return &handler;
}
#endif

#if !HAVE_MAD && HAVE_SNDFILE
extern int startread_sndfile(sox_format_t * ft);
extern size_t read_samples_sndfile(sox_format_t * ft, sox_sample_t *buf, size_t len);
extern int seek_sndfile(sox_format_t * ft, sox_uint64_t offset);
extern int stop_sndfile(sox_format_t * ft);
#endif

#if HAVE_MAD || HAVE_LAME
LSX_FORMAT_HANDLER(mp3)
{
  /* "mp3" is both the name of a dynamic format handler plugin and
   * a filename extension and to load the above two handlers --with-mp3=dyn
   * they must be listed here because init_format() relies on that to guess
   * the symbol names of lsx_mp[12]_format_fn() and "mp3", the one that
   * corresponds to the symbol name of this handler, must come first.
   *
   * In case someone is configuring with sndfile but not mad,
   * make the default MP3 decoder fall back to sndfile; otherwise
   * they result unreadable.
   */
  static char const * const names[] = {"mp3", "mp2",
#if HAVE_MAD
    "mp1",
#endif
    NULL};
  static unsigned const write_encodings[] = {
    SOX_ENCODING_MP3, 0, 0};
  static sox_rate_t const write_rates[] = {
    8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000, 0};
  static sox_format_handler_t const handler = {SOX_LIB_VERSION_CODE,
    "MPEG-1 Layer 3 lossy audio compression", names, 0,
#if HAVE_MAD
    startread_mad, read_mad, stopread_mad,
#elif HAVE_SNDFILE
    startread_sndfile, read_samples_sndfile, stop_sndfile,
#elif HAVE_LAME
    startread_hip, read_hip, stopread_hip,
#else
    NULL, NULL, NULL,
#endif
    startwrite_lame, write_lame, stopwrite_lame,
#if HAVE_MAD
    seek_mad,
#elif HAVE_SNDFILE
    seek_sndfile,
#else
    NULL,
#endif
    write_encodings, write_rates, sizeof(priv_t)
  };
  return &handler;
}
#endif
