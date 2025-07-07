/* libSoX CSL NSP format.
 * http://web.archive.org/web/20160525045942/http://www-mmsp.ece.mcgill.ca/Documents/AudioFormats/CSL/CSL.html
 *
 * Copyright 2017 Jim O'Regan
 *
 * based on aiff.c
 * Copyright 1991-2007 Guido van Rossum And Sundry Contributors
 *
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained.
 * Guido van Rossum And Sundry Contributors are not responsible for
 * the consequences of using this software.
 */

#include "sox_i.h"

#include <time.h>      /* for time stamping comments */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <limits.h>

static const char read_error_msg[] = "file is truncated in %.4s chunk";
#define read_error() { \
        lsx_fail_errno(ft,SOX_EOF, read_error_msg, buf); \
        return(SOX_EOF); \
}
int lsx_nspstartread(sox_format_t * ft)
{
  char buf[8];
  uint32_t hchunksize;
  uint32_t chunksize;
  unsigned short channels = 0;
  sox_encoding_t enc = SOX_ENCODING_SIGN2;
  unsigned short bits = 16;
  double rate = 0.0;
  uint64_t seekto = 0;
  int i;
  size_t ssndsize = 0;

  char date[20];
  char *comment;
  uint16_t maxabschan[8];
  uint32_t datalength;
  uint32_t samplerate;
  int numchannels;

  uint8_t trash8;

  /* FORM chunk */
  if (lsx_readbuf(ft, buf, (size_t)8) != (size_t)8 || \
      strncmp(buf, "FORMDS16", (size_t)8) != 0) {
    lsx_fail_errno(ft,SOX_EHDR,"header does not begin with magic word `FORMDS16'");
    return(SOX_EOF);
  }
  if (lsx_readdw(ft, &hchunksize))
    read_error();

  while (1) {
    if (lsx_readbuf(ft, buf, (size_t)4) != (size_t)4) {
      if (ssndsize > 0)
        break;
      else {
        lsx_fail_errno(ft,SOX_EHDR,"missing SDA_ chunk");
        return(SOX_EOF);
      }
    }
    if (strncmp(buf, "HEDR", (size_t)4) == 0) {
      /* HEDR chunk */
      if (lsx_readdw(ft, &chunksize) ||
          lsx_readbuf(ft, date, (size_t)20) != (size_t)20 ||
          lsx_readdw(ft, &samplerate) ||
          lsx_readdw(ft, &datalength) ||
          lsx_readw(ft, &maxabschan[0]) ||
          lsx_readw(ft, &maxabschan[1]))
        read_error();
      rate = (double)samplerate;

      /* Most likely there will only be 1 channel, but there can be 2 here */
      if (maxabschan[0] == 0xffff && maxabschan[1] == 0xffff) {
        lsx_fail_errno(ft,SOX_EHDR,"channels A and B undefined");
        return(SOX_EOF);
      } else if (maxabschan[0] == 0xffff || maxabschan[1] == 0xffff) {
        ft->signal.channels = 1;
      } else {
        ft->signal.channels = 2;
      }
    } else if (strncmp(buf, "HDR8", (size_t)4) == 0) {
      /* HDR8 chunk */
      if (lsx_readdw(ft, &chunksize) ||
          lsx_readbuf(ft, date, (size_t)20) != (size_t)20 ||
          lsx_readdw(ft, &samplerate) ||
          lsx_readdw(ft, &datalength) ||
          lsx_readw(ft, &maxabschan[0]) ||
          lsx_readw(ft, &maxabschan[1]) ||
          lsx_readw(ft, &maxabschan[2]) ||
          lsx_readw(ft, &maxabschan[3]) ||
          lsx_readw(ft, &maxabschan[4]) ||
          lsx_readw(ft, &maxabschan[5]) ||
          lsx_readw(ft, &maxabschan[6]) ||
          lsx_readw(ft, &maxabschan[7]))
        read_error();
      rate = (double)samplerate;

      /* Can be up to 8 channels */
      numchannels = 0;
      for (i = 0; i < 7; i++) {
        if (maxabschan[i] != 0xffff) {
          numchannels++;
        }
      }
      if (numchannels == 0) {
        lsx_fail_errno(ft,SOX_EHDR,"no channels defined");
        return(SOX_EOF);
      }
      ft->signal.channels = numchannels;
    } else if (strncmp(buf, "NOTE", (size_t)4) == 0) {
      unsigned char nullc = 0;
      /* NOTE chunk */
      if (lsx_readdw(ft, &chunksize))
        read_error();
      comment = lsx_malloc(chunksize + 1);
      if (lsx_reads(ft, comment, (size_t)chunksize))
        read_error();
      if(strlen(comment) != 0)
        lsx_debug("NSP comment: %s", comment);
      free(comment);
      if (lsx_readb(ft, &nullc))
        read_error();

    } else if (strncmp(buf, "SDA_", (size_t)4) == 0) {
      if (lsx_readdw(ft, &chunksize))
        read_error();
      ssndsize = chunksize;
      /* if can't seek, just do sound now */
      if (!ft->seekable)
        break;
      /* else, seek to end of sound and hunt for more */
      seekto = lsx_tell(ft);
      if (lsx_seeki(ft, (off_t)chunksize, SEEK_CUR))
        read_error();
    } else {
      if (lsx_eof(ft))
        break;
      buf[4] = 0;
      lsx_debug("NSPstartread: ignoring `%s' chunk", buf);
      if (lsx_readdw(ft, &chunksize))
        read_error();
      if (lsx_eof(ft))
        break;
      /* Skip the chunk using lsx_readb() so we may read
         from a pipe */
      while (chunksize-- > 0) {
        if (lsx_readb(ft, &trash8) == SOX_EOF)
          break;
      }
    }
    if (lsx_eof(ft))
      break;
  }

  if (ft->seekable) {
    if (seekto > 0) {
      if (lsx_seeki(ft, seekto, SEEK_SET)) {
        lsx_fail_errno(ft,SOX_EOF,"cannot seek");
        return(SOX_EOF);
      }
    } else {
      lsx_fail_errno(ft,SOX_EOF,"no sound data on input file");
      return(SOX_EOF);
    }
  }

  return lsx_check_read_params(
      ft, channels, rate, enc, bits, (uint64_t)ssndsize/2, sox_false);
}

LSX_FORMAT_HANDLER(nsp)
{
  static char const * const names[] = {"nsp", NULL };
  static sox_format_handler_t const handler = {SOX_LIB_VERSION_CODE,
    "Computerized Speech Lab NSP file",
    names, SOX_FILE_LIT_END,
    lsx_nspstartread, lsx_rawread, NULL,
    NULL, NULL, NULL,
    NULL, NULL, NULL, 0
  };
  return &handler;
}
