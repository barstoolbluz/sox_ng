/* AVR file format handler for SoX
 * Copyright (C) 1999 Jan Paul Schmidt <jps@fundament.org>
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


#define AVR_MAGIC "2BIT"

/* Taken from the Audio File Formats FAQ */

typedef struct {
  char magic [5];      /* 2BIT */
  char name [8];       /* null-padded sample name */
  unsigned short mono; /* 0 = mono, 0xffff = stereo */
  unsigned short rez;  /* 8 = 8 bit, 16 = 16 bit */
  unsigned short sign; /* 0 = unsigned, 0xffff = signed */
  unsigned short loop; /* 0 = no loop, 0xffff = looping sample */
  unsigned short midi; /* 0xffff = no MIDI note assigned,
                          0xffXX = single key note assignment
                          0xLLHH = key split, low/hi note */
  uint32_t rate;       /* sample frequency in hertz */
  uint32_t size;       /* sample length in bytes or words (see rez) */
  uint32_t lbeg;       /* offset to start of loop in bytes or words.
                          set to zero if unused. */
  uint32_t lend;       /* offset to end of loop in bytes or words.
                          set to sample length if unused. */
  unsigned short res1; /* Reserved, MIDI keyboard split */
  unsigned short res2; /* Reserved, sample compression */
  unsigned short res3; /* Reserved */
  char ext[20];        /* Additional filename space, used
                          if (name[7] != 0) */
  char user[64];       /* User defined. Typically ASCII message. */
} priv_t;



/*
 * Do anything required before you start reading samples.
 * Read file header.
 *      Find out sampling rate,
 *      size and encoding of samples,
 *      mono/stereo/quad.
 */


static int startread(sox_format_t * ft)
{
  priv_t * avr = (priv_t *)ft->priv;
  int rc;

  static const char read_error_msg[] = "file is truncated";
#define read_error() { \
    lsx_fail_errno(ft, SOX_EOF, read_error_msg); \
    return(SOX_EOF); \
  }

  if (lsx_reads(ft, avr->magic, (size_t)4))
    read_error();

  if (strncmp (avr->magic, AVR_MAGIC, (size_t)4)) {
    lsx_fail_errno(ft,SOX_EHDR,"unknown header");
    return(SOX_EOF);
  }

  if (lsx_readbuf(ft, avr->name, sizeof(avr->name)) != sizeof(avr->name) ||
      lsx_readw (ft, &(avr->mono)))
    read_error();

  if (avr->mono) {
    ft->signal.channels = 2;
  }
  else {
    ft->signal.channels = 1;
  }

  if (lsx_readw (ft, &(avr->rez)))
    read_error();
  if (avr->rez == 8) {
    ft->encoding.bits_per_sample = 8;
  }
  else if (avr->rez == 16) {
    ft->encoding.bits_per_sample = 16;
  }
  else {
    lsx_fail_errno(ft,SOX_EFMT,"unsupported sample resolution");
    return(SOX_EOF);
  }

  if (lsx_readw (ft, &(avr->sign)))
    read_error();
  if (avr->sign) {
    ft->encoding.encoding = SOX_ENCODING_SIGN2;
  }
  else {
    ft->encoding.encoding = SOX_ENCODING_UNSIGNED;
  }

  if (lsx_readw (ft, &(avr->loop)) ||
      lsx_readw (ft, &(avr->midi)) ||
      lsx_readdw (ft, &(avr->rate)) ||
      lsx_readdw (ft, &(avr->size)) ||
      lsx_readdw (ft, &(avr->lbeg)) ||
      lsx_readdw (ft, &(avr->lend)) ||
      lsx_readw (ft, &(avr->res1)) ||
      lsx_readw (ft, &(avr->res2)) ||
      lsx_readw (ft, &(avr->res3)) ||
      lsx_readbuf(ft, avr->ext, sizeof(avr->ext)) != sizeof(avr->ext) ||
      lsx_readbuf(ft, avr->user, sizeof(avr->user)) != sizeof(avr->user))
    read_error();

  /*
   * No support for AVRs created by ST-Replay,
   * Replay Profesional and PRO-Series 12.
   *
   * Just masking the upper byte out.
   */
  ft->signal.rate = (avr->rate & 0x00ffffff);

  rc = lsx_rawstartread (ft);
  if (rc)
      return rc;

  return(SOX_SUCCESS);
}

static const char write_error_msg[] = "write error";
#define write_error() { \
    lsx_fail_errno(ft, SOX_EOF, write_error_msg); \
    return(SOX_EOF); \
}

static int startwrite(sox_format_t * ft)
{
  priv_t * avr = (priv_t *)ft->priv;
  int rc;

  if (!ft->seekable) {
    lsx_fail_errno(ft,SOX_EOF,"file is not seekable");
    return(SOX_EOF);
  }

  rc = lsx_rawstartwrite (ft);
  if (rc)
      return rc;

  if (
      /* magic */
      lsx_writes(ft, AVR_MAGIC) ||

      /* name */
      lsx_writeb(ft, 0) ||
      lsx_writeb(ft, 0) ||
      lsx_writeb(ft, 0) ||
      lsx_writeb(ft, 0) ||
      lsx_writeb(ft, 0) ||
      lsx_writeb(ft, 0) ||
      lsx_writeb(ft, 0) ||
      lsx_writeb(ft, 0))
    write_error();

  /* mono */
  if (ft->signal.channels == 1) {
    if (lsx_writew (ft, 0))
      write_error();
  }
  else if (ft->signal.channels == 2) {
    if (lsx_writew (ft, 0xffff))
      write_error();
  }
  else {
    lsx_fail_errno(ft,SOX_EFMT,"number of channels not supported");
    return(0);
  }

  /* rez */
  if (ft->encoding.bits_per_sample == 8) {
    if (lsx_writew (ft, 8))
      write_error();
  }
  else if (ft->encoding.bits_per_sample == 16) {
    if (lsx_writew (ft, 16))
      write_error();
  }
  else {
    lsx_fail_errno(ft,SOX_EFMT,"unsupported sample resolution");
    return(SOX_EOF);
  }

  /* sign */
  if (ft->encoding.encoding == SOX_ENCODING_SIGN2) {
    if (lsx_writew (ft, 0xffff))
      write_error();
  }
  else if (ft->encoding.encoding == SOX_ENCODING_UNSIGNED) {
    if (lsx_writew (ft, 0))
      write_error();
  }
  else {
    lsx_fail_errno(ft,SOX_EFMT,"unsupported encoding");
    return(SOX_EOF);
  }

  if (
      /* loop */
      lsx_writew (ft, 0xffff) ||

      /* midi */
      lsx_writew (ft, 0xffff) ||

      /* rate */
      lsx_writedw(ft, (unsigned)(ft->signal.rate + .5)) ||

      /* size */
      /* Don't know the size yet. */
      lsx_writedw (ft, 0) ||

      /* lbeg */
      lsx_writedw (ft, 0) ||

      /* lend */
      /* Don't know the size yet, so we can't set lend, either. */
      lsx_writedw (ft, 0) ||

      /* res1 */
      lsx_writew (ft, 0) ||

      /* res2 */
      lsx_writew (ft, 0) ||

      /* res3 */
      lsx_writew (ft, 0) ||

      /* ext */
      lsx_writebuf(ft, "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0",
                   sizeof(avr->ext)) != sizeof(avr->ext) ||

      /* user */
      lsx_writebuf(ft,
           "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
           "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
           "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
           "\0\0\0\0", sizeof(avr->user)) != sizeof(avr->user)
     )
        write_error();

  return(SOX_SUCCESS);
}

static size_t write_samples(sox_format_t * ft, const sox_sample_t *buf, size_t nsamp)
{
  priv_t * avr = (priv_t *)ft->priv;

  avr->size += nsamp;

  return (lsx_rawwrite (ft, buf, nsamp));
}

static int stopwrite(sox_format_t * ft)
{
  priv_t * avr = (priv_t *)ft->priv;

  unsigned size = avr->size / ft->signal.channels;

  /* Fix size */
  if (lsx_seeki(ft, (off_t)26, SEEK_SET) ||
      lsx_writedw (ft, size) ||
      /* Fix lend */
      lsx_seeki(ft, (off_t)34, SEEK_SET) ||
      lsx_writedw (ft, size))
    write_error();

  return(SOX_SUCCESS);
}

LSX_FORMAT_HANDLER(avr)
{
  static char const * const names[] = { "avr", NULL };
  static unsigned const write_encodings[] = {
    SOX_ENCODING_SIGN2, 16, 8, 0,
    SOX_ENCODING_UNSIGNED, 16, 8, 0,
    0};
  static sox_format_handler_t handler = {SOX_LIB_VERSION_CODE,
    "Audio Visual Research format; used on the Mac",
    names, SOX_FILE_BIG_END | SOX_FILE_MONO | SOX_FILE_STEREO,
    startread, lsx_rawread, NULL,
    startwrite, write_samples, stopwrite,
    NULL, write_encodings, NULL, sizeof(priv_t)
  };
  return &handler;
}
