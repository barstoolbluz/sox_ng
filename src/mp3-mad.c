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

/* The decoding part is based on the decoder-tutorial program madlld
 * which is Copyright (c) 2001-2004 Bertrand Petit <madlld@phoe.fmug.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * 3. Neither the name of the author nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* libmad support for SoX */

#include "sox_i.h"

#if HAVE_MAD

#include "mp3.h"
#include "mp3-mad.h"

#if defined(HAVE_ID3TAG) && (defined(HAVE_IO_H) || defined(HAVE_UNISTD_H))
#define USING_ID3TAG 1
#endif

#if USING_ID3TAG
  #include <id3tag.h>
  #include "id3.h"
#if defined(HAVE_UNISTD_H)
  #include <unistd.h>
#elif defined(HAVE_IO_H)
  #include <io.h>
#endif
#else
  #define ID3_TAG_FLAG_FOOTERPRESENT 0x10
#endif

#define MAXFRAMESIZE 2880
#define ID3PADDING 128

/* MAD returns values with MAD_F_FRACBITS (28) bits of precision, though it's
   not certain that all of them are meaningful. Default to 16 bits to
   align with most users expectation of output file should be 16 bits. */
#define MP3_MAD_PRECISION    16

/* This function merges the functions tagtype() and id3_tag_query()
   from MAD's libid3tag, so we don't have to link to it
   Returns 0 if the frame is not an ID3 tag, tag length if it is */

static int tagtype(const unsigned char *data, size_t length)
{
    if (length >= 3 && data[0] == 'T' && data[1] == 'A' && data[2] == 'G')
    {
        return 128; /* ID3V1 */
    }

    if (length >= 10 &&
        (data[0] == 'I' && data[1] == 'D' && data[2] == '3') &&
        data[3] < 0xff && data[4] < 0xff &&
        data[6] < 0x80 && data[7] < 0x80 && data[8] < 0x80 && data[9] < 0x80)
    {     /* ID3V2 */
        unsigned char flags;
        unsigned int size;
        flags = data[5];
        size = 10 + (data[6]<<21) + (data[7]<<14) + (data[8]<<7) + data[9];
        if (flags & ID3_TAG_FLAG_FOOTERPRESENT)
            size += 10;
        for (; size < length && !data[size]; ++size);  /* Consume padding */
        return size;
    }

    return 0;
}

#if USING_ID3TAG
extern char const * lsx_id3tagmap[][2];
#endif

static unsigned long xing_frames(priv_t * p, struct mad_bitptr ptr, unsigned bitlen)
{
  #define XING_MAGIC ( ('X' << 24) | ('i' << 16) | ('n' << 8) | 'g' )
  if (bitlen >= 96 && p->mad_bit_read(&ptr, 32) == XING_MAGIC &&
      (p->mad_bit_read(&ptr, 32) & 1 )) /* XING_FRAMES */
    return p->mad_bit_read(&ptr, 32);
  return 0;
}

static void mad_timer_mult(mad_timer_t * t, double d)
{
  t->seconds = (signed long)(d *= (t->seconds + t->fraction * (1. / MAD_TIMER_RESOLUTION)));
  t->fraction = (unsigned long)((d - t->seconds) * MAD_TIMER_RESOLUTION + .5);
}

static size_t mp3_duration_ms(sox_format_t * ft)
{
  priv_t              * p = (priv_t *) ft->priv;
  struct mad_stream   mad_stream;
  struct mad_header   mad_header;
  struct mad_frame    mad_frame;
  mad_timer_t         time = mad_timer_zero;
  size_t              initial_bitrate = 0; /* Initialized to prevent warning */
  size_t              tagsize = 0, consumed = 0, frames = 0;
  sox_bool            vbr = sox_false, depadded = sox_false;

  p->mad_stream_init(&mad_stream);
  p->mad_header_init(&mad_header);
  p->mad_frame_init(&mad_frame);

  do {  /* Read data from the MP3 file */
    int read, padding = 0;
    size_t leftover = mad_stream.bufend - mad_stream.next_frame;

    memmove(p->mp3_buffer, mad_stream.this_frame, leftover);
    read = lsx_readbuf(ft, p->mp3_buffer + leftover, p->mp3_buffer_size - leftover);
    if (read <= 0) {
      lsx_debug("got exact duration by scan to EOF (frames=%" PRIuPTR " leftover=%" PRIuPTR ")", frames, leftover);
      break;
    }
    for (; !depadded && padding < read && !p->mp3_buffer[padding]; ++padding);
    depadded = sox_true;
    p->mad_stream_buffer(&mad_stream, p->mp3_buffer + padding, leftover + read - padding);

    while (sox_true) {  /* Decode frame headers */
      mad_stream.error = MAD_ERROR_NONE;
      if (p->mad_header_decode(&mad_header, &mad_stream) == -1) {
        if (mad_stream.error == MAD_ERROR_BUFLEN)
          break;  /* Normal behaviour; get some more data from the file */
        if (!MAD_RECOVERABLE(mad_stream.error)) {
          lsx_warn("unrecoverable MAD error");
          break;
        }
        if (mad_stream.error == MAD_ERROR_LOSTSYNC) {
          unsigned available = (mad_stream.bufend - mad_stream.this_frame);
          tagsize = tagtype(mad_stream.this_frame, (size_t) available);
          if (tagsize) {   /* It's some ID3 tags, so just skip */
            if (tagsize >= available) {
              lsx_seeki(ft, (off_t)(tagsize - available), SEEK_CUR);
              depadded = sox_false;
            }
            p->mad_stream_skip(&mad_stream, min(tagsize, available));
          }
          else lsx_warn("MAD lost sync");
        }
        else lsx_warn("recoverable MAD error");
        continue; /* Not an audio frame */
      }

      p->mad_timer_add(&time, mad_header.duration);
      consumed += mad_stream.next_frame - mad_stream.this_frame;

      lsx_debug_more("bitrate=%lu", mad_header.bitrate);
      if (!frames) {
        initial_bitrate = mad_header.bitrate;

        /* Get the precise frame count from the XING header if present */
        mad_frame.header = mad_header;
        if (p->mad_frame_decode(&mad_frame, &mad_stream) == -1)
          if (!MAD_RECOVERABLE(mad_stream.error)) {
            lsx_warn("unrecoverable MAD error");
            break;
          }
        if ((frames = xing_frames(p, mad_stream.anc_ptr, mad_stream.anc_bitlen))) {
          p->mad_timer_multiply(&time, (signed long)frames);
          lsx_debug("got exact duration from XING frame count (%" PRIuPTR ")", frames);
          break;
        }
      }
      else vbr |= mad_header.bitrate != initial_bitrate;

      /* If not VBR, we can time just a few frames then extrapolate */
      if (++frames == 25 && !vbr) {
        mad_timer_mult(&time, (double)(lsx_filelength(ft) - tagsize) / consumed);
        lsx_debug("got approx. duration by CBR extrapolation");
        break;
      }
    }
  } while (mad_stream.error == MAD_ERROR_BUFLEN);

  p->mad_frame_finish(&mad_frame);
  mad_header_finish(&mad_header);
  p->mad_stream_finish(&mad_stream);
  lsx_rewind(ft);
  return p->mad_timer_count(time, MAD_UNITS_MILLISECONDS);
}

/*
 * (Re)fill the stream buffer that is to be decoded.  If any data
 * still exists in the buffer then they are first shifted to be
 * front of the stream buffer.
 */
static int sox_mad_input(sox_format_t * ft)
{
    priv_t *p = (priv_t *) ft->priv;
    size_t bytes_read;
    size_t remaining;

    remaining = p->Stream.bufend - p->Stream.next_frame;

    /* libmad does not consume all the buffer it's given. Some
     * data, part of a truncated frame, is left unused at the
     * end of the buffer. That data must be put back at the
     * beginning of the buffer and taken in account for
     * refilling the buffer. This means that the input buffer
     * must be large enough to hold a complete frame at the
     * highest observable bit-rate (currently 448 kb/s).
     * TODO: Is 2016 bytes the size of the largest frame?
     * (448000*(1152/32000))/8
     */
    memmove(p->mp3_buffer, p->Stream.next_frame, remaining);

    bytes_read = lsx_readbuf(ft, p->mp3_buffer+remaining,
                            p->mp3_buffer_size-remaining);
    if (bytes_read == 0)
    {
        return SOX_EOF;
    }

    p->mad_stream_buffer(&p->Stream, p->mp3_buffer, bytes_read+remaining);
    p->Stream.error = 0;

    return SOX_SUCCESS;
}

/* Attempts to read an ID3 tag at the current location in stream and
 * consume it all.  Returns SOX_EOF if no tag is found.  Its up to
 * caller to recover.
 * */
static int sox_mad_inputtag(sox_format_t * ft)
{
    priv_t *p = (priv_t *) ft->priv;
    int rc = SOX_EOF;
    size_t remaining;
    size_t tagsize;


    /* FIXME: This needs some more work if we are to ever
     * look at the ID3 frame.  This is because the Stream
     * may not be able to hold the complete ID3 frame.
     * We should consume the whole frame inside tagtype()
     * instead of outside of tagframe().  That would support
     * recovering when Stream contains less then 8-bytes (header)
     * and also when ID3v2 is bigger then Stream buffer size.
     * Need to pass in stream so that buffer can be
     * consumed as well as letting additional data to be
     * read in.
     */
    remaining = p->Stream.bufend - p->Stream.next_frame;
    if ((tagsize = tagtype(p->Stream.this_frame, remaining)))
    {
        p->mad_stream_skip(&p->Stream, tagsize);
        rc = SOX_SUCCESS;
    }

    /* We know that a valid frame hasn't been found yet
     * so help libmad out and go back into frame seek mode.
     * This is true whether an ID3 tag was found or not.
     */
    p->mad_stream_sync(&p->Stream);

    return rc;
}

/*
 * This is the error callback function. It is called whenever a decoding
 * error occurs. The error is indicated by stream->error; the list of
 * possible MAD_ERROR_* errors can be found in the mad.h (or stream.h)
 * header file.
 */

static char const *mad_error = NULL;

static
enum mad_flow error(LSX_UNUSED void *data,
                    struct mad_stream *stream,
                    LSX_UNUSED struct mad_frame *frame)
{
  mad_error = mad_stream_errorstr(stream);

  /* return MAD_FLOW_BREAK here to stop decoding (and propagate an error) */

  return MAD_FLOW_CONTINUE;
}

int startread_mad(sox_format_t * ft)
{
  priv_t *p = (priv_t *) ft->priv;
  size_t ReadSize;
  sox_bool ignore_length = ft->signal.length == SOX_IGNORE_LENGTH;
  int open_library_result;
  sox_bool done_init = sox_false;
  struct mad_decoder decoder;

  LSX_DLLIBRARY_OPEN(
      p,
      mad_dl,
      MAD_FUNC_ENTRIES,
      "MAD decoder library",
      mad_library_names,
      open_library_result);
  if (open_library_result)
    return SOX_EOF;

  p->mp3_buffer_size = sox_globals.bufsiz;
  p->mp3_buffer = lsx_malloc(p->mp3_buffer_size);

  /* configure error function */

  mad_decoder_init(&decoder, NULL,
                   0 /* input */, 0 /* header */, 0 /* filter */,
                   0 /* output */, error, 0 /* message */);

  ft->signal.length = SOX_UNSPEC;
  if (ft->seekable) {
#ifdef USING_ID3TAG
    lsx_id3_read_tag(ft, sox_true);
    lsx_rewind(ft);
    if (!ft->signal.length)
#endif
      if (!ignore_length) {
        ft->signal.length = mp3_duration_ms(ft);
        done_init = sox_true;
      }
  }

  if (!done_init) {
    p->mad_stream_init(&p->Stream);
    p->mad_frame_init(&p->Frame);
    p->mad_synth_init(&p->Synth);
  }
  mad_timer_reset(&p->Timer);

  /* Decode at least one valid frame to find out the input
   * format.  The decoded frame will be saved off so that it
   * can be processed later.
   */
  ReadSize = lsx_readbuf(ft, p->mp3_buffer, p->mp3_buffer_size);
  if (ReadSize != p->mp3_buffer_size && lsx_error(ft))
    return SOX_EOF;

  p->mad_stream_buffer(&p->Stream, p->mp3_buffer, ReadSize);

  /* Find a valid frame before starting up.  This makes sure
   * that we have a valid MP3 and also skips past ID3v2 tags
   * at the beginning of the audio file.
   */

  p->Stream.error = 0;
  while (p->mad_frame_decode(&p->Frame,&p->Stream))
  {
      /* check whether input buffer needs a refill */
      if (p->Stream.error == MAD_ERROR_BUFLEN)
      {
          if (sox_mad_input(ft) == SOX_EOF)
              return SOX_EOF;

          continue;
      }

      /* Consume any ID3 tags */
      sox_mad_inputtag(ft);

      /* FIXME: We should probably detect when we've read
       * a bunch of non-ID3 data and still haven't found a
       * frame.  In that case we can abort early without
       * scanning the whole file.
       */
      p->Stream.error = 0;
  }

  if (mad_error)
  {
      lsx_fail("%s", mad_error);
      return SOX_EOF;
  }
  if (p->Stream.error)
  {
      lsx_fail_errno(ft,SOX_EOF,"no valid MP3 frame found");
      return SOX_EOF;
  }

  lsx_report("MAD says it's MPEG-1 layer %d",
      p->Frame.header.layer == MAD_LAYER_I ? 1 :
      p->Frame.header.layer == MAD_LAYER_II ? 2 :
      p->Frame.header.layer == MAD_LAYER_III ? 3 :
      p->Frame.header.layer);

  switch (p->Frame.header.layer) {
  case MAD_LAYER_I:
      /* MPEG-1 Layer I is a simpler encoding at bitrates from 32 to 448 kbps
       * and sampling rates of 32, 44.1 and 48 kHz.
       * It uses a 512-point DFT as opposed to Layer 2's 1024-point DFT
       * and was used in the Philips Digital Compact Cassette.
       * MP1 is a subset of MP2 and MP2 decoders can decode MP1 */
      ft->encoding.encoding = SOX_ENCODING_MP1;
      break;
  case MAD_LAYER_II:
      ft->encoding.encoding = SOX_ENCODING_MP2;
      break;
  case MAD_LAYER_III: /* mono or stereo */
      ft->encoding.encoding = SOX_ENCODING_MP3;
      break;
  default:
      lsx_fail_errno(ft,SOX_EOF,"no valid MPEG layer found");
      return SOX_EOF;
  }

  switch(p->Frame.header.mode)
  {
      case MAD_MODE_SINGLE_CHANNEL:
      case MAD_MODE_DUAL_CHANNEL:
      case MAD_MODE_JOINT_STEREO:
      case MAD_MODE_STEREO:
          ft->signal.channels = MAD_NCHANNELS(&p->Frame.header);
          break;
      default:
          lsx_fail_errno(ft, SOX_EFMT, "cannot determine number of channels");
          return SOX_EOF;
  }

  p->FrameCount=1;

  p->mad_timer_add(&p->Timer,p->Frame.header.duration);
  p->mad_synth_frame(&p->Synth,&p->Frame);
  ft->signal.precision = MP3_MAD_PRECISION;
  ft->signal.rate=p->Synth.pcm.samplerate;
  if (ignore_length)
    ft->signal.length = SOX_UNSPEC;
  else {
    ft->signal.length = (uint64_t)(ft->signal.length * .001 * ft->signal.rate + .5);
    ft->signal.length *= ft->signal.channels;  /* Keep separate from line above! */
  }

  p->cursamp = 0;

  return SOX_SUCCESS;
}

/*
 * Read up to len samples from p->Synth
 * If needed, read some more MP3 data, decode them and synth them
 * Place in buf[].
 * Return number of samples read.
 */
size_t read_mad(sox_format_t * ft, sox_sample_t *buf, size_t len)
{
    priv_t *p = (priv_t *) ft->priv;
    size_t donow,i,done=0;
    mad_fixed_t sample;
    size_t chan;

    do {
        size_t x = (p->Synth.pcm.length - p->cursamp)*ft->signal.channels;
        donow=min(len, x);
        i=0;
        while(i<donow){
            for(chan=0;chan<ft->signal.channels;chan++){
                sample=p->Synth.pcm.samples[chan][p->cursamp];
                if (sample < -MAD_F_ONE)
                    sample=-MAD_F_ONE;
                else if (sample >= MAD_F_ONE)
                    sample=MAD_F_ONE-1;
                *buf++=(sox_sample_t)(sample<<(32-1-MAD_F_FRACBITS));
                i++;
            }
            p->cursamp++;
        };

        len-=donow;
        done+=donow;

        if (len==0) break;

        /* check whether input buffer needs a refill */
        if (p->Stream.error == MAD_ERROR_BUFLEN)
        {
            if (sox_mad_input(ft) == SOX_EOF) {
                lsx_debug("sox_mad_input EOF");
                break;
            }
        }

        if (p->mad_frame_decode(&p->Frame,&p->Stream))
        {
            if(MAD_RECOVERABLE(p->Stream.error))
            {
                sox_mad_inputtag(ft);
                continue;
            }
            else
            {
                if (p->Stream.error == MAD_ERROR_BUFLEN)
                    continue;
                else
                {
                    lsx_report("unrecoverable frame level error (%s)",
                              p->mad_stream_errorstr(&p->Stream));
                    break;
                }
            }
        }
        p->FrameCount++;
        p->mad_timer_add(&p->Timer,p->Frame.header.duration);
        p->mad_synth_frame(&p->Synth,&p->Frame);
        p->cursamp=0;
    } while(1);

    return done;
}

int stopread_mad(sox_format_t * ft)
{
  priv_t *p=(priv_t*) ft->priv;

  mad_synth_finish(&p->Synth);
  p->mad_frame_finish(&p->Frame);
  p->mad_stream_finish(&p->Stream);

  free(p->mp3_buffer);
  LSX_DLLIBRARY_CLOSE(p, mad_dl);
  return SOX_SUCCESS;
}

int seek_mad(sox_format_t * ft, sox_uint64_t offset)
{
  priv_t   * p = (priv_t *) ft->priv;
  size_t   initial_bitrate = p->Frame.header.bitrate;
  size_t   tagsize = 0, consumed = 0;
  sox_bool vbr = sox_false; /* Variable Bit Rate */
  sox_bool depadded = sox_false;
  uint64_t to_skip_samples = 0;

  /* Reset all */
  lsx_rewind(ft);
  mad_timer_reset(&p->Timer);
  p->FrameCount = 0;

  /* They where opened in startread */
  mad_synth_finish(&p->Synth);
  p->mad_frame_finish(&p->Frame);
  p->mad_stream_finish(&p->Stream);

  p->mad_stream_init(&p->Stream);
  p->mad_frame_init(&p->Frame);
  p->mad_synth_init(&p->Synth);

  offset /= ft->signal.channels;
  to_skip_samples = offset;

  while(sox_true) {  /* Read data from the MP3 file */
    size_t padding = 0;
    size_t read;
    size_t leftover = p->Stream.bufend - p->Stream.next_frame;

    memmove(p->mp3_buffer, p->Stream.this_frame, leftover);
    read = lsx_readbuf(ft, p->mp3_buffer + leftover, p->mp3_buffer_size - leftover);
    if (read == 0) {
      lsx_debug("seek failure. unexpected EOF (frames=%" PRIuPTR " leftover=%" PRIuPTR ")", p->FrameCount, leftover);
      break;
    }
    for (; !depadded && padding < read && !p->mp3_buffer[padding]; ++padding);
    depadded = sox_true;
    p->mad_stream_buffer(&p->Stream, p->mp3_buffer + padding, leftover + read - padding);

    while (sox_true) {  /* Decode frame headers */
      static unsigned short samples;
      p->Stream.error = MAD_ERROR_NONE;

      /* Not an audio frame */
      if (p->mad_header_decode(&p->Frame.header, &p->Stream) == -1) {
        if (p->Stream.error == MAD_ERROR_BUFLEN)
          break;  /* Normal behaviour; get some more data from the file */
        if (!MAD_RECOVERABLE(p->Stream.error)) {
          lsx_warn("unrecoverable MAD error");
          break;
        }
        if (p->Stream.error == MAD_ERROR_LOSTSYNC) {
          unsigned available = (p->Stream.bufend - p->Stream.this_frame);
          tagsize = tagtype(p->Stream.this_frame, (size_t) available);
          if (tagsize) {   /* It's some ID3 tags, so just skip */
            if (tagsize >= available) {
              if (lsx_seeki(ft, (off_t)(tagsize - available), SEEK_CUR))
	        return SOX_EOF;
              depadded = sox_false;
            }
            p->mad_stream_skip(&p->Stream, min(tagsize, available));
          }
          else lsx_warn("MAD lost sync");
        }
        else lsx_warn("recoverable MAD error");
        continue;
      }

      consumed += p->Stream.next_frame - p->Stream.this_frame;
      vbr      |= (p->Frame.header.bitrate != initial_bitrate);

      samples = 32 * MAD_NSBSAMPLES(&p->Frame.header);

      p->FrameCount++;
      p->mad_timer_add(&p->Timer, p->Frame.header.duration);

      if(to_skip_samples <= samples)
      {
        p->mad_frame_decode(&p->Frame,&p->Stream);
        p->mad_synth_frame(&p->Synth, &p->Frame);
        p->cursamp = to_skip_samples;
        return SOX_SUCCESS;
      }
      else to_skip_samples -= samples;

      /* If not VBR, we can extrapolate frame size */
      if (p->FrameCount == 64 && !vbr) {
        p->FrameCount = offset / samples;
        to_skip_samples = offset % samples;

        if (SOX_SUCCESS != lsx_seeki(ft, (off_t)(p->FrameCount * consumed / 64 + tagsize), SEEK_SET))
          return SOX_EOF;

        /* Reset Stream for refilling buffer */
        p->mad_stream_finish(&p->Stream);
        p->mad_stream_init(&p->Stream);
        break;
      }
    }
  };

  return SOX_EOF;
}

#if 0
LSX_FORMAT_HANDLER(mad)
{
  static char const * const names[] = {"mad", "mp1", "mp2", "mp3", NULL};
  static sox_format_handler_t const handler = {SOX_LIB_VERSION_CODE,
    "MPEG-1 Layer 3 lossy audio compression", names, 0,
    startread_mad, read_mad, stopread_mad,
    NULL, NULL, NULL,
    seek_mad, NULL, NULL, sizeof(priv_t)
  };
  return &handler;
}
#endif

#endif /* HAVE_MAD */
