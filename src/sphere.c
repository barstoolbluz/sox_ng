/* libSoX NIST Sphere file format handler.
 *
 * August 7, 2000
 *
 * Copyright (C) 2000 Chris Bagwell (cbagwell@sprynet.com)
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

static int start_read(sox_format_t * ft)
{
  unsigned long header_size_ul = 0, num_samples_ul = 0;
  sox_encoding_t encoding = SOX_ENCODING_SIGN2;
  size_t     header_size, bytes_read;
  size_t     num_samples = 0;
  unsigned       bytes_per_sample = 0;
  unsigned       channels = 1;
  unsigned       rate = 16000;
  char           fldname[64], fldtype[16], fldsval[128];
  char           * buf;

  /* Magic header */
  if (lsx_reads(ft, fldname, (size_t)8) || strncmp(fldname, "NIST_1A", (size_t)7) != 0) {
    lsx_fail_errno(ft, SOX_EHDR, "header does not begin with magic word `NIST_1A'");
    return (SOX_EOF);
  }

  if (lsx_reads(ft, fldsval, (size_t)8)) {
    lsx_fail_errno(ft, SOX_EHDR, "error reading header");
    return (SOX_EOF);
  }

  /* Determine header size, and allocate a buffer large enough to hold it. */
  sscanf(fldsval, "%lu", &header_size_ul);
  if (header_size_ul < 16) {
    lsx_fail_errno(ft, SOX_EHDR, "error reading header");
    return (SOX_EOF);
  }

  buf = lsx_malloc(header_size = header_size_ul);

  /* Skip what we have read so far */
  header_size -= 16;

  if (lsx_reads(ft, buf, header_size) == SOX_EOF) {
    lsx_fail_errno(ft, SOX_EHDR, "error reading header");
    free(buf);
    return (SOX_EOF);
  }

  bytes_read = strlen(buf);
  header_size -= bytes_read >= header_size ? header_size : bytes_read + 1;

  while (strncmp(buf, "end_head", (size_t)8) != 0) {
    if (strncmp(buf, "sample_n_bytes", (size_t)14) == 0)
      sscanf(buf, "%63s %15s %u", fldname, fldtype, &bytes_per_sample);
    else if (strncmp(buf, "channel_count", (size_t)13) == 0)
      sscanf(buf, "%63s %15s %u", fldname, fldtype, &channels);
    else if (strncmp(buf, "sample_count ", (size_t)13) == 0)
      sscanf(buf, "%53s %15s %lu", fldname, fldtype, &num_samples_ul);
    else if (strncmp(buf, "sample_rate ", (size_t)12) == 0)
      sscanf(buf, "%53s %15s %u", fldname, fldtype, &rate);
    else if (strncmp(buf, "sample_coding", (size_t)13) == 0) {
      sscanf(buf, "%63s %15s %127s", fldname, fldtype, fldsval);
      if (!strcasecmp(fldsval, "ulaw") || !strcasecmp(fldsval, "mu-law"))
        encoding = SOX_ENCODING_ULAW;
      else if (!strcasecmp(fldsval, "pcm"))
        encoding = SOX_ENCODING_SIGN2;
      else if (!strcasecmp(fldsval, "alaw"))
        encoding = SOX_ENCODING_ALAW;
      else {
        lsx_fail_errno(ft, SOX_EFMT, "unsupported coding `%s'", fldsval);
        free(buf);
        return SOX_EOF;
      }
    }
    else if (strncmp(buf, "sample_byte_format", (size_t)18) == 0) {
      sscanf(buf, "%53s %15s %127s", fldname, fldtype, fldsval);
      if (strcmp(fldsval, "01") == 0)         /* Data is little endian. */
        ft->encoding.reverse_bytes = MACHINE_IS_BIGENDIAN;
      else if (strcmp(fldsval, "10") == 0)    /* Data is big endian. */
        ft->encoding.reverse_bytes = MACHINE_IS_LITTLEENDIAN;
      else if (strcmp(fldsval, "1")) {
        lsx_fail_errno(ft, SOX_EFMT, "unsupported coding `%s'", fldsval);
        free(buf);
        return SOX_EOF;
      }
    }

    if (lsx_reads(ft, buf, header_size) == SOX_EOF) {
      lsx_fail_errno(ft, SOX_EHDR, "error reading Sphere header");
      free(buf);
      return (SOX_EOF);
    }

    bytes_read = strlen(buf);
    header_size -= bytes_read >= header_size ? header_size : bytes_read + 1;
  }

  if (!bytes_per_sample)
    bytes_per_sample = encoding == SOX_ENCODING_ULAW? 1 : 2;

  while (header_size) {
    bytes_read = lsx_readbuf(ft, buf, header_size);
    if (bytes_read == 0) {
      free(buf);
      return (SOX_EOF);
    }
    header_size -= bytes_read;
  }
  free(buf);

  if (ft->seekable) {
    /* Check first four bytes of data to see if it's shorten compressed. */
    char           shorten_check[4];

    if (lsx_readchars(ft, shorten_check, sizeof(shorten_check)) ||
        lsx_seeki(ft, -(off_t)sizeof(shorten_check), SEEK_CUR))
      return SOX_EOF;

    if (!memcmp(shorten_check, "ajkg", sizeof(shorten_check))) {
      lsx_fail_errno(ft, SOX_EFMT,
                     "File uses shorten compression, cannot handle this");
      return (SOX_EOF);
    }
  }

  num_samples = num_samples_ul;
  return lsx_check_read_params(ft, channels, (sox_rate_t)rate, encoding,
      bytes_per_sample << 3, (uint64_t)num_samples * channels, sox_true);
}

static const char write_error_msg[] = "write error";
#define write_error() { \
    lsx_fail_errno(ft, SOX_EOF, write_error_msg); \
    return SOX_EOF; \
}

static int write_header(sox_format_t * ft)
{
  char buf[128];
  uint64_t samples = (ft->olength ? ft->olength : ft->signal.length) / ft->signal.channels;

  if (lsx_writes(ft, "NIST_1A\n") ||
      lsx_writes(ft, "   1024\n"))
    write_error();

  if (samples) {
    sprintf(buf, "sample_count -i %" PRIu64 "\n", samples);
    if (lsx_writes(ft, buf))
      write_error();
  }

  sprintf(buf, "sample_n_bytes -i %d\n", ft->encoding.bits_per_sample >> 3);
  if (lsx_writes(ft, buf))
    write_error();

  sprintf(buf, "channel_count -i %d\n", ft->signal.channels);
  if (lsx_writes(ft, buf))
    write_error();

  if (ft->encoding.bits_per_sample == 8)
    sprintf(buf, "sample_byte_format -s1 1\n");
  else
    sprintf(buf, "sample_byte_format -s2 %s\n",
            ft->encoding.reverse_bytes != MACHINE_IS_BIGENDIAN ? "10" : "01");
  if (lsx_writes(ft, buf))
    write_error();

  sprintf(buf, "sample_rate -i %u\n", (unsigned) (ft->signal.rate + .5));
  if (lsx_writes(ft, buf))
    write_error();

  if (ft->encoding.encoding == SOX_ENCODING_ULAW) {
    if (lsx_writes(ft, "sample_coding -s4 ulaw\n"))
      write_error();
  } else {
    if (lsx_writes(ft, "sample_coding -s3 pcm\n"))
      write_error();
  }
  if (lsx_writes(ft, "end_head\n") ||
      lsx_padbytes(ft, 1024 - (size_t)lsx_tell(ft)))
    write_error();
  return SOX_SUCCESS;
}

LSX_FORMAT_HANDLER(sphere)
{
  static char const *const names[] = {"sph", "nist", NULL};
  static unsigned const write_encodings[] = {
    SOX_ENCODING_SIGN2, 8, 16, 24, 32, 0,
    SOX_ENCODING_ULAW, 8, 0,
    0
  };
  static sox_format_handler_t const handler = {SOX_LIB_VERSION_CODE,
    "SPeech HEader Resources; defined by NIST", names, SOX_FILE_REWIND,
    start_read, lsx_rawread, NULL,
    write_header, lsx_rawwrite, NULL,
    lsx_rawseek, write_encodings, NULL, 0
  };
  return &handler;
}
