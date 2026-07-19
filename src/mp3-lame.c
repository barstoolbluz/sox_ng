/* mp3-lame.c - libmp3lame encoding support for SoX
 *
 * Copyright (C) 2002 Fabrizio Gennari <fabrizio.ge@tiscali.it>
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
#include "mp3.h"
#include "mp3-lame.h"

#if HAVE_LAME

#define MAXFRAMESIZE 2880
#define ID3PADDING 128
/* LAME takes float values as input. */
#define MP3_LAME_PRECISION   24
#define LAME_BUFFER_SIZE(num_samples) (((num_samples) + 3) / 4 * 5 + 7200)

/* Note: sox_precision() returns 0 for SOX_ENCODING_MP2 and _MP3, as it varies
 * according to whether you're encoding or decoding but sox_ng.c knows
 * about this and has a special case and reports 24 as the Writes: precision */

static void write_comments(sox_format_t * ft)
{
  priv_t *p = (priv_t *) ft->priv;
  const char* comment;

  p->id3tag_init(p->gfp);
  p->id3tag_set_pad(p->gfp, (size_t)ID3PADDING);

  /* Note: id3tag_set_fieldvalue is not present in LAME 3.97, so we're using
     the 3.97-compatible methods for all of the tags that 3.97 supported. */
  /* FIXME: This is no more necessary, since support for LAME 3.97 has ended. */
  if ((comment = sox_find_comment(ft->oob.comments, "Title")))
    p->id3tag_set_title(p->gfp, comment);
  if ((comment = sox_find_comment(ft->oob.comments, "Artist")))
    p->id3tag_set_artist(p->gfp, comment);
  if ((comment = sox_find_comment(ft->oob.comments, "Album")))
    p->id3tag_set_album(p->gfp, comment);
  if ((comment = sox_find_comment(ft->oob.comments, "Tracknumber")))
    p->id3tag_set_track(p->gfp, comment);
  if ((comment = sox_find_comment(ft->oob.comments, "Year")))
    p->id3tag_set_year(p->gfp, comment);
  if ((comment = sox_find_comment(ft->oob.comments, "Comment")))
    p->id3tag_set_comment(p->gfp, comment);
  if ((comment = sox_find_comment(ft->oob.comments, "Genre")))
  {
    if (p->id3tag_set_genre(p->gfp, comment))
      lsx_warn("\"%s\" is not a recognized ID3v1 genre.", comment);
  }

  if ((comment = sox_find_comment(ft->oob.comments, "Discnumber")))
  {
    char* id3tag_buf = lsx_malloc(strlen(comment) + 6);
    if (id3tag_buf)
    {
      sprintf(id3tag_buf, "TPOS=%s", comment);
      p->id3tag_set_fieldvalue(p->gfp, id3tag_buf);
      free(id3tag_buf);
    }
  }
}

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

/* These functions are considered optional. If they aren't present in the
   library, the stub versions defined here will be used instead. */

UNUSED static void id3tag_init_stub(lame_global_flags * gfp UNUSED)
  { return; }
UNUSED static void id3tag_set_title_stub(lame_global_flags * gfp UNUSED, const char* title UNUSED)
  { return; }
UNUSED static void id3tag_set_artist_stub(lame_global_flags * gfp UNUSED, const char* artist UNUSED)
  { return; }
UNUSED static void id3tag_set_album_stub(lame_global_flags * gfp UNUSED, const char* album UNUSED)
  { return; }
UNUSED static void id3tag_set_year_stub(lame_global_flags * gfp UNUSED, const char* year UNUSED)
  { return; }
UNUSED static void id3tag_set_comment_stub(lame_global_flags * gfp UNUSED, const char* comment UNUSED)
  { return; }
UNUSED static void id3tag_set_track_stub(lame_global_flags * gfp UNUSED, const char* track UNUSED)
  { return; }
UNUSED static int id3tag_set_genre_stub(lame_global_flags * gfp UNUSED, const char* genre UNUSED)
  { return 0; }
UNUSED static size_t id3tag_set_pad_stub(lame_global_flags * gfp UNUSED, size_t n UNUSED)
  { return 0; }
UNUSED static size_t lame_get_id3v2_tag_stub(lame_global_flags * gfp UNUSED, unsigned char * buffer UNUSED, size_t size UNUSED)
  { return 0; }
UNUSED static int id3tag_set_fieldvalue_stub(lame_global_flags * gfp UNUSED, const char *fieldvalue UNUSED)
  { return 0; }

static int get_id3v2_tag_size(sox_format_t * ft)
{
  size_t bytes_read;
  int id3v2_size;
  unsigned char id3v2_header[10];

  if (lsx_seeki(ft, (off_t)0, SEEK_SET) != 0) {
    lsx_warn("cannot update id3 tag - failed to seek to beginning");
    return SOX_EOF;
  }

  /* read 10 bytes in case there's an ID3 version 2 header here */
  bytes_read = lsx_readbuf(ft, id3v2_header, sizeof(id3v2_header));
  if (bytes_read != sizeof(id3v2_header)) {
    lsx_warn("cannot update id3 tag - failed to read id3 header");
    return SOX_EOF;      /* not readable, maybe opened Write-Only */
  }

  /* does the stream begin with the ID3 version 2 file identifier? */
  if (!strncmp((char *) id3v2_header, "ID3", (size_t)3)) {
    /* the tag size (minus the 10-byte header) is encoded into four
     * bytes where the most significant bit is clear in each byte */
    id3v2_size = (((id3v2_header[6] & 0x7f) << 21)
                    | ((id3v2_header[7] & 0x7f) << 14)
                    | ((id3v2_header[8] & 0x7f) << 7)
                    | (id3v2_header[9] & 0x7f))
        + sizeof(id3v2_header);
  } else {
    /* no ID3 version 2 tag in this stream */
    id3v2_size = 0;
  }
  return id3v2_size;
}

static void rewrite_id3v2_tag(sox_format_t * ft, size_t id3v2_size, uint64_t num_samples)
{
  priv_t *p = (priv_t *)ft->priv;
  size_t new_size;
  unsigned char * buffer;

  if (LSX_DLFUNC_IS_STUB(p, lame_get_id3v2_tag))
  {
    if (p->num_samples) {
      lsx_warn("this version of LAME does not support tag update;");
      lsx_warn("the track length will be incorrect");
    } else {
      lsx_report("this version of LAME does not support tag update;");
      lsx_report("the track length will be unspecified");
    }
    return;
  }

  buffer = lsx_malloc(id3v2_size);

  if (num_samples > ULONG_MAX)
  {
    lsx_warn("cannot accurately update track length info - file is too long");
    num_samples = 0;
  }
  p->lame_set_num_samples(p->gfp, (unsigned long)num_samples);
  lsx_debug("updated MP3 TLEN to %lu samples", (unsigned long)num_samples);

  new_size = p->lame_get_id3v2_tag(p->gfp, buffer, id3v2_size);

  if (new_size != id3v2_size && new_size-ID3PADDING <= id3v2_size) {
    p->id3tag_set_pad(p->gfp, ID3PADDING + id3v2_size - new_size);
    new_size = p->lame_get_id3v2_tag(p->gfp, buffer, id3v2_size);
  }

  if (new_size != id3v2_size) {
    if (LSX_DLFUNC_IS_STUB(p, id3tag_set_pad))
    {
      if (p->num_samples) {
        lsx_report("this version of LAME does not support tag size adjustment;");
        lsx_report("the track length will be invalid");
      } else {
        lsx_report("this version of LAME does not support tag size adjustment;");
        lsx_report("the track length will be unspecified");
      }
    }
    else
      lsx_warn("cannot update track length info - failed to adjust tag size");
  } else {
    if (lsx_seeki(ft, (off_t)0, SEEK_SET))
      lsx_warn("cannot rewrite Id3v2 tag");
      
    /* Overwrite the Id3v2 tag (this time TLEN should be accurate) */
    if (lsx_writebuf(ft, buffer, id3v2_size) != 1) {
      lsx_debug("Rewrote Id3v2 tag (%" PRIuPTR " bytes)", id3v2_size);
    }
  }

  free(buffer);
}

static void rewrite_tags(sox_format_t * ft, uint64_t num_samples)
{
  priv_t *p = (priv_t *)ft->priv;

  off_t file_size;
  size_t id3v2_size;

  if (lsx_seeki(ft, (off_t)0, SEEK_END)) {
    lsx_warn("cannot update tags - seek to end failed");
    return;
  }

  /* Get file size */
  file_size = lsx_tell(ft);

  if (file_size == 0) {
    lsx_warn("cannot update tags - file size is 0");
    return;
  }

  id3v2_size = get_id3v2_tag_size(ft);
  if (id3v2_size > 0 && num_samples != p->num_samples) {
    rewrite_id3v2_tag(ft, id3v2_size, num_samples);
  }

  if (p->vbr_tag) {
    size_t lametag_size;
    uint8_t buffer[MAXFRAMESIZE];

    if (lsx_seeki(ft, (off_t)id3v2_size, SEEK_SET)) {
      lsx_warn("cannot write VBR tag - seek to tag block failed");
      return;
    }

    lametag_size = p->lame_get_lametag_frame(p->gfp, buffer, sizeof(buffer));
    if (lametag_size > sizeof(buffer)) {
      lsx_warn("cannot write VBR tag - VBR tag too large for buffer");
      return;
    }

    if (lametag_size < 1) {
      return;
    }

    if (lsx_writebuf(ft, buffer, lametag_size) != lametag_size) {
      lsx_warn("cannot write VBR tag - VBR tag write failed");
    } else {
      lsx_debug("rewrote VBR tag (%" PRIuPTR " bytes)", lametag_size);
    }
  }
}

int startwrite_lame(sox_format_t * ft)
{
  priv_t *p = (priv_t *) ft->priv;
  int openlibrary_result;

  LSX_DLLIBRARY_OPEN(
      p,
      lame_dl,
      LAME_FUNC_ENTRIES,
      "LAME encoder library",
      lame_library_names,
      openlibrary_result);
  if (openlibrary_result)
    return SOX_EOF;

  if ((p->gfp = p->lame_init()) == NULL){
    lsx_fail_errno(ft,SOX_EOF,"initialization of LAME library failed");
    return SOX_EOF;
  }

  /* First set message callbacks so we don't miss any messages: */
  p->lame_set_errorf(p->gfp,errorf);
  p->lame_set_debugf(p->gfp,debugf);
  p->lame_set_msgf  (p->gfp,msgf);

  p->num_samples = ft->signal.length == SOX_IGNORE_LENGTH ? 0 : ft->signal.length / max(ft->signal.channels, 1);
  p->lame_set_num_samples(p->gfp, p->num_samples > ULONG_MAX ? 0 : (unsigned long)p->num_samples);

  if (ft->encoding.encoding != SOX_ENCODING_MP3) {
    if(ft->encoding.encoding != SOX_ENCODING_UNKNOWN)
      lsx_report("encoding forced to MP3");
    ft->encoding.encoding = SOX_ENCODING_MP3;
  }

  ft->signal.precision = MP3_LAME_PRECISION;

  if (ft->signal.channels != SOX_ENCODING_UNKNOWN) {
    if (p->lame_set_num_channels(p->gfp,(int)ft->signal.channels) < 0) {
      lsx_fail_errno(ft,SOX_EOF,"unsupported number of channels");
      return(SOX_EOF);
    }
  } else {
    ft->signal.channels = p->lame_get_num_channels(p->gfp); /* LAME default */
  }

  p->lame_set_in_samplerate(p->gfp,(int)ft->signal.rate);
  p->lame_set_out_samplerate(p->gfp,(int)ft->signal.rate);

  if (!LSX_DLFUNC_IS_STUB(p, id3tag_init))
    write_comments(ft);

  /* The primary parameter to the LAME encoder is the bit rate. If the
   * value of encoding.compression is a positive integer, it's taken as
   * the bitrate in kbps (that is if you specify 128, it use 128 kbps).
   *
   * The second most important parameter is probably "quality" (really
   * performance), which allows balancing encoding speed vs. quality.
   * In LAME, 0 specifies highest quality but is very slow, while
   * 9 selects poor quality, but is fast. (5 is the default and 2 is
   * recommended as a good trade-off for high quality encodes.)
   *
   * Because encoding.compression is a float, the fractional part is used
   * to select quality. 128.2 selects 128 kbps encoding with a quality
   * of 2. There is one problem with this approach. We need 128 to specify
   * 128 kbps encoding with default quality, so .0 means use default. Instead
   * of .0 you have to use .01 to specify the highest quality (128.01).
   *
   * LAME uses bitrate to specify a constant bitrate, but higher quality
   * can be achieved using Variable Bit Rate (VBR). VBR quality (really
   * size) is selected using a number from 0 to 9. Use a value of 0 for high
   * quality, larger files, and 9 for smaller files of lower quality. 4 is
   * the default.
   *
   * In order to squeeze the selection of VBR into the encoding.compression
   * float we use negative numbers to select VRR. -4.2 would select default
   * VBR encoding (size) with high quality (speed). One special case is 0,
   * which is a valid VBR encoding parameter but not a valid bitrate.
   * Compression value of 0 is always treated as a high quality vbr, as a
   * result both -0.2 and 0.2 are treated as highest quality VBR (size) and
   * high quality (speed).
   *
   * Note: It would have been nice to simply use low values, 0-9, to trigger
   * VBR mode, but 8 kbps is a valid bit rate, so negative values were
   * used instead.
   */

  lsx_debug("-C option is %f", ft->encoding.compression);

  if (ft->encoding.compression == HUGE_VAL) {
    /* Do nothing, use defaults: */
    lsx_report("using MP3 encoding defaults");
  } else {
    double abs_compression = fabs(ft->encoding.compression);
    double floor_compression = floor(abs_compression);
    double fraction_compression = abs_compression - floor_compression;
    int bitrate_q = (int)floor_compression;
    int encoder_q =
        fraction_compression == 0.0
        ? -1
        : (int)(fraction_compression * 10.0 + 0.5);

    if (ft->encoding.compression < 0.5) {
      if (p->lame_get_VBR(p->gfp) == vbr_off)
        p->lame_set_VBR(p->gfp, vbr_default);

      if (ft->seekable) {
        p->vbr_tag = 1;
      } else {
        lsx_warn("unable to write VBR tag because we can't seek");
      }

      if (p->lame_set_VBR_q(p->gfp, bitrate_q) < 0)
      {
        lsx_fail_errno(ft, SOX_EOF,
          "lame_set_VBR_q(%d) failed (should be between 0 and 9)",
          bitrate_q);
        return(SOX_EOF);
      }
      lsx_report("lame_set_VBR_q(%d)", bitrate_q);
    } else {
      if (p->lame_set_brate(p->gfp, bitrate_q) < 0) {
        lsx_fail_errno(ft, SOX_EOF,
          "lame_set_brate(%d) failed", bitrate_q);
        return(SOX_EOF);
      }
      lsx_report("lame_set_brate(%d)", bitrate_q);
    }

    /* Set Quality */

    if (encoder_q < 0) {
      /* use default quality value */
      lsx_report("using MP3 default quality");
    } else {
      if (p->lame_set_quality(p->gfp, encoder_q) < 0) {
        lsx_fail_errno(ft, SOX_EOF,
          "lame_set_quality(%d) failed", encoder_q);
        return(SOX_EOF);
      }
      lsx_report("lame_set_quality(%d)", encoder_q);
    }
  }

  p->lame_set_bWriteVbrTag(p->gfp, p->vbr_tag);

  if (p->lame_init_params(p->gfp) < 0) {
    lsx_fail_errno(ft,SOX_EOF,"LAME initialization failed");
    return(SOX_EOF);
  }

  p->mp3_buffer_size = LAME_BUFFER_SIZE(sox_globals.bufsiz / max(ft->signal.channels, 1));
  p->mp3_buffer = lsx_malloc(p->mp3_buffer_size);

  p->pcm_buffer_size = sox_globals.bufsiz * sizeof(float);
  p->pcm_buffer = lsx_malloc(p->pcm_buffer_size);

  return(SOX_SUCCESS);
}

#define MP3_SAMPLE_TO_FLOAT(d,clips) ((float)(32768*SOX_SAMPLE_TO_FLOAT_32BIT(d,clips)))

size_t write_lame(sox_format_t *ft, const sox_sample_t *buf, size_t samp)
{
  priv_t *p = (priv_t *)ft->priv;
  size_t new_buffer_size;
  float *buffer_l, *buffer_r = NULL;
  int nsamples = samp/ft->signal.channels;
  int i, j;
  int written = 0;
  int clips = 0;
  SOX_SAMPLE_LOCALS;

  new_buffer_size = samp * sizeof(float);
  if (p->pcm_buffer_size < new_buffer_size) {
    float *new_buffer = lsx_realloc(p->pcm_buffer, new_buffer_size);
    p->pcm_buffer_size = new_buffer_size;
    p->pcm_buffer = new_buffer;
  }

  buffer_l = p->pcm_buffer;

  j=0;
  if (ft->signal.channels == 2)
  {
    /* lame doesn't support interleaved samples for floats so we must break
     * them out into seperate buffers.
     */
    buffer_r = p->pcm_buffer + nsamples;
    for (i = 0; i < nsamples; i++)
    {
      buffer_l[i] = MP3_SAMPLE_TO_FLOAT(buf[j++], clips);
      buffer_r[i] = MP3_SAMPLE_TO_FLOAT(buf[j++], clips);
    }
  } else {
    for (i = 0; i < nsamples; i++) {
      buffer_l[i] = MP3_SAMPLE_TO_FLOAT(buf[j++], clips);
    }
  }

  new_buffer_size = LAME_BUFFER_SIZE(nsamples);
  if (p->mp3_buffer_size < new_buffer_size) {
    unsigned char *new_buffer = lsx_realloc(p->mp3_buffer, new_buffer_size);
    p->mp3_buffer_size = new_buffer_size;
    p->mp3_buffer = new_buffer;
  }

  written = p->lame_encode_buffer_float(p->gfp, buffer_l, buffer_r,
            nsamples, p->mp3_buffer, (int)p->mp3_buffer_size);
  if (written < 0) {
    lsx_fail_errno(ft,SOX_EOF,"encoding failed");
    return 0;
  }

  if (lsx_writebuf(ft, p->mp3_buffer, (size_t)written) < (size_t)written)
  {
    lsx_fail_errno(ft,SOX_EOF,"file write failed");
    return 0;
  }

  return samp;
}

int stopwrite_lame(sox_format_t * ft)
{
  priv_t *p = (priv_t *) ft->priv;
  uint64_t num_samples = ft->olength == SOX_IGNORE_LENGTH ? 0 : ft->olength / max(ft->signal.channels, 1);
  int written = 0;

  written = p->lame_encode_flush(p->gfp, p->mp3_buffer, (int)p->mp3_buffer_size);
  if (written < 0)
    lsx_fail_errno(ft, SOX_EOF, "encoding failed");
  else if (lsx_writebuf(ft, p->mp3_buffer, (size_t)written) < (size_t)written)
    lsx_fail_errno(ft, SOX_EOF, "file write failed");
  else if (!p->mp2) {
    if (ft->seekable && (num_samples != p->num_samples || p->vbr_tag))
      rewrite_tags(ft, num_samples);
  }

  free(p->mp3_buffer);
  free(p->pcm_buffer);

  p->lame_close(p->gfp);
  LSX_DLLIBRARY_CLOSE(p, lame_dl);
  return SOX_SUCCESS;
}

#if 0
LSX_FORMAT_HANDLER(lame)
{
  static char const * const names[] = {"lame", "mp3", NULL};
  static unsigned const write_encodings[] = {
    SOX_ENCODING_MP3, 0, 0};
  static sox_rate_t const write_rates[] = {
    8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000, 0};
  static sox_format_handler_t const handler = {SOX_LIB_VERSION_CODE,
    "MPEG-1 Layer 3 lossy audio compression", names, 0,
    NULL, NULL, NULL,
    startwrite_lame, write_lame, stopwrite_lame,
    NULL, write_encodings, write_rates, sizeof(priv_t)
  };
  return &handler;
}
#endif

#endif /* HAVE_LAME */
