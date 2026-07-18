/* Copyright (C) 2002-2025 Fabrizio Gennari <fabrizio.ge@tiscali.it>
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

/* mp3.h: Declarations common to mp1, mp2 and mp3 file formats.
 * They have to be one format handler because they share a common priv_t.
 *
 * You can't make separate mp1, mp2 and mp3, or mad, lame and twolame plugins
 * because one plugin can't load functions from another plugin, only from
 * libsox, so here "mp3" refers to the plugin's name, not the file format.
 */

/* Callers should already have included sox_i.h */

#if !HAVE_LIBLTDL
  #undef DL_MAD
  #undef DL_LAME
  #undef DL_TWOLAME
#endif

#if HAVE_MAD_H
#include <mad.h>
#elif DL_MAD
#include "bit-rot/mad.h"
/* Under Windows, importing data from DLLs is a dicey proposition. This is true
 * when using dlopen, but also true if linking directly against the DLL if the
 * header does not mark the data as __declspec(dllexport), which mad.h does not.
 * Sidestep the issue by defining our own mad_timer_zero. This is needed because
 * mad_timer_zero is used in some of the mad.h macros.
 */
#define mad_timer_zero mad_timer_zero_stub
static mad_timer_t const mad_timer_zero_stub = {0, 0};
#endif

#if HAVE_LAME_LAME_H
#include <lame/lame.h>
#elif HAVE_LAME_H
#include <lame.h>
#elif DL_LAME
typedef struct lame_global_struct lame_global_flags;
typedef enum {
  vbr_off=0,
  vbr_default=4
} vbr_mode;
#endif

#if HAVE_TWOLAME_H
#include <twolame.h>
#elif DL_LAME
typedef struct twolame_options_struct twolame_options;
#endif

#if HAVE_MAD
static const char* const mad_library_names[] = {
#ifdef DL_MAD
    "libmad",
    "libmad-0",
    "cygmad-0",
#endif
    NULL
};

#if DL_MAD
  #define MAD_FUNC LSX_DLENTRY_DYNAMIC
#else
  #define MAD_FUNC LSX_DLENTRY_STATIC
#endif

#define MAD_FUNC_ENTRIES(f,x) \
  MAD_FUNC(f,x, void, mad_stream_buffer, (struct mad_stream *, unsigned char const *, unsigned long)) \
  MAD_FUNC(f,x, void, mad_stream_skip, (struct mad_stream *, unsigned long)) \
  MAD_FUNC(f,x, int, mad_stream_sync, (struct mad_stream *)) \
  MAD_FUNC(f,x, void, mad_stream_init, (struct mad_stream *)) \
  MAD_FUNC(f,x, void, mad_frame_init, (struct mad_frame *)) \
  MAD_FUNC(f,x, void, mad_synth_init, (struct mad_synth *)) \
  MAD_FUNC(f,x, int, mad_frame_decode, (struct mad_frame *, struct mad_stream *)) \
  MAD_FUNC(f,x, void, mad_timer_add, (mad_timer_t *, mad_timer_t)) \
  MAD_FUNC(f,x, void, mad_synth_frame, (struct mad_synth *, struct mad_frame const *)) \
  MAD_FUNC(f,x, char const *, mad_stream_errorstr, (struct mad_stream const *)) \
  MAD_FUNC(f,x, void, mad_frame_finish, (struct mad_frame *)) \
  MAD_FUNC(f,x, void, mad_stream_finish, (struct mad_stream *)) \
  MAD_FUNC(f,x, unsigned long, mad_bit_read, (struct mad_bitptr *, unsigned int)) \
  MAD_FUNC(f,x, int, mad_header_decode, (struct mad_header *, struct mad_stream *)) \
  MAD_FUNC(f,x, void, mad_header_init, (struct mad_header *)) \
  MAD_FUNC(f,x, signed long, mad_timer_count, (mad_timer_t, enum mad_units)) \
  MAD_FUNC(f,x, void, mad_timer_multiply, (mad_timer_t *, signed long))
#endif /* HAVE_MAD */

#if HAVE_LAME
static const char* const lame_library_names[] = {
#ifdef DL_LAME
  "libmp3lame",
  "libmp3lame-0",
  "lame-enc",
  "cygmp3lame-0",
#endif
  NULL
};
#endif

#if DL_LAME
  /* Expected to be present in all builds of LAME. */
  #define LAME_FUNC           LSX_DLENTRY_DYNAMIC
  /* id3tag support is an optional component of LAME. Use if available. */
  #define LAME_FUNC_ID3       LSX_DLENTRY_STUB
#else /* DL_LAME */
  /* Expected to be present in all builds of LAME. */
  #define LAME_FUNC           LSX_DLENTRY_STATIC
  /* id3tag support is an optional component of LAME. Use if available. */
  #ifdef HAVE_LAME_ID3TAG
    #define LAME_FUNC_ID3     LSX_DLENTRY_STATIC
  #else
    #define LAME_FUNC_ID3     LSX_DLENTRY_STUB
  #endif
#endif /* DL_LAME */

#define LAME_FUNC_ENTRIES(f,x) \
  LAME_FUNC(f,x, lame_global_flags*, lame_init, (void)) \
  LAME_FUNC(f,x, int, lame_set_errorf, (lame_global_flags *, void (*)(const char *, va_list))) \
  LAME_FUNC(f,x, int, lame_set_debugf, (lame_global_flags *, void (*)(const char *, va_list))) \
  LAME_FUNC(f,x, int, lame_set_msgf, (lame_global_flags *, void (*)(const char *, va_list))) \
  LAME_FUNC(f,x, int, lame_set_num_samples, (lame_global_flags *, unsigned long)) \
  LAME_FUNC(f,x, int, lame_get_num_channels, (const lame_global_flags *)) \
  LAME_FUNC(f,x, int, lame_set_num_channels, (lame_global_flags *, int)) \
  LAME_FUNC(f,x, int, lame_set_in_samplerate, (lame_global_flags *, int)) \
  LAME_FUNC(f,x, int, lame_set_out_samplerate, (lame_global_flags *, int)) \
  LAME_FUNC(f,x, int, lame_set_bWriteVbrTag, (lame_global_flags *, int)) \
  LAME_FUNC(f,x, int, lame_set_brate, (lame_global_flags *, int)) \
  LAME_FUNC(f,x, int, lame_set_quality, (lame_global_flags *, int)) \
  LAME_FUNC(f,x, vbr_mode, lame_get_VBR, (const lame_global_flags *)) \
  LAME_FUNC(f,x, int, lame_set_VBR, (lame_global_flags *, vbr_mode)) \
  LAME_FUNC(f,x, int, lame_set_VBR_q, (lame_global_flags *, int)) \
  LAME_FUNC(f,x, int, lame_init_params, (lame_global_flags *)) \
  LAME_FUNC(f,x, int, lame_encode_buffer_float, (lame_global_flags *, const float[], const float[], const int, unsigned char *, const int)) \
  LAME_FUNC(f,x, int, lame_encode_flush, (lame_global_flags *, unsigned char *, int)) \
  LAME_FUNC(f,x, int, lame_close, (lame_global_flags *)) \
  LAME_FUNC(f,x, size_t, lame_get_lametag_frame, (const lame_global_flags *, unsigned char*, size_t)) \
  LAME_FUNC(f,x, hip_t, hip_decode_init, (void)) \
  LAME_FUNC(f,x, void, hip_set_msgf, (hip_t hip, lame_report_function)) \
  LAME_FUNC(f,x, void, hip_set_errorf, (hip_t hip, lame_report_function)) \
  LAME_FUNC(f,x, void, hip_set_debugf, (hip_t hip, lame_report_function)) \
  LAME_FUNC(f,x, int, hip_decode1_headers, (hip_t, unsigned char * mp3buf, size_t len, short pcm_l[], short pcm_r[], mp3data_struct* mp3data)) \
  LAME_FUNC(f,x, int, hip_decode1_headersB, (hip_t, unsigned char * mp3buf, size_t len, short pcm_l[], short pcm_r[], mp3data_struct* mp3data, int *enc_delay, int *enc_padding)) \
  LAME_FUNC(f,x, int, hip_decode_exit, (hip_t)) \
  LAME_FUNC_ID3(f,x, void, id3tag_init, (lame_global_flags *)) \
  LAME_FUNC_ID3(f,x, void, id3tag_set_title, (lame_global_flags *, const char* title)) \
  LAME_FUNC_ID3(f,x, void, id3tag_set_artist, (lame_global_flags *, const char* artist)) \
  LAME_FUNC_ID3(f,x, void, id3tag_set_album, (lame_global_flags *, const char* album)) \
  LAME_FUNC_ID3(f,x, void, id3tag_set_year, (lame_global_flags *, const char* year)) \
  LAME_FUNC_ID3(f,x, void, id3tag_set_comment, (lame_global_flags *, const char* comment)) \
  LAME_FUNC_ID3(f,x, int, id3tag_set_track, (lame_global_flags *, const char* track)) \
  LAME_FUNC_ID3(f,x, int, id3tag_set_genre, (lame_global_flags *, const char* genre)) \
  LAME_FUNC_ID3(f,x, size_t, id3tag_set_pad, (lame_global_flags *, size_t)) \
  LAME_FUNC_ID3(f,x, size_t, lame_get_id3v2_tag, (lame_global_flags *, unsigned char*, size_t)) \
  LAME_FUNC_ID3(f,x, int, id3tag_set_fieldvalue, (lame_global_flags *, const char *))

#ifdef HAVE_TWOLAME
static const char* const twolame_library_names[] = {
#ifdef DL_TWOLAME
  "libtwolame",
  "libtwolame-0",
#endif
  NULL
};
#endif /* HAVE_TWOLAME */

#ifdef DL_TWOLAME
  #define TWOLAME_FUNC LSX_DLENTRY_DYNAMIC
#else
  #define TWOLAME_FUNC LSX_DLENTRY_STATIC
#endif

#define TWOLAME_FUNC_ENTRIES(f,x) \
  TWOLAME_FUNC(f,x, twolame_options*, twolame_init, (void)) \
  TWOLAME_FUNC(f,x, int, twolame_set_verbosity, (twolame_options*, int)) \
  TWOLAME_FUNC(f,x, int, twolame_get_num_channels, (twolame_options*)) \
  TWOLAME_FUNC(f,x, int, twolame_set_num_channels, (twolame_options*, int)) \
  TWOLAME_FUNC(f,x, int, twolame_set_in_samplerate, (twolame_options *, int)) \
  TWOLAME_FUNC(f,x, int, twolame_set_out_samplerate, (twolame_options *, int)) \
  TWOLAME_FUNC(f,x, int, twolame_set_brate, (twolame_options *, int)) \
  TWOLAME_FUNC(f,x, int, twolame_set_VBR, (twolame_options *, int)) \
  TWOLAME_FUNC(f,x, int, twolame_set_VBR_level, (twolame_options *, float)) \
  TWOLAME_FUNC(f,x, int, twolame_init_params, (twolame_options *)) \
  TWOLAME_FUNC(f,x, int, twolame_encode_buffer_float32_interleaved, (twolame_options *, const float [], int, unsigned char *, int)) \
  TWOLAME_FUNC(f,x, int, twolame_encode_flush, (twolame_options *, unsigned char *, int)) \
  TWOLAME_FUNC(f,x, void, twolame_close, (twolame_options **))

/* Private data */
typedef struct mp3_priv_t {
  unsigned char *mp3_buffer;
  size_t mp3_buffer_size;

#if HAVE_MAD
  struct mad_stream       Stream;
  struct mad_frame        Frame;
  struct mad_synth        Synth;
  mad_timer_t             Timer;
  ptrdiff_t               cursamp;
  size_t                  FrameCount;
  LSX_DLENTRIES_TO_PTRS(MAD_FUNC_ENTRIES, mad_dl);
#endif /* HAVE_MAD */

#if HAVE_LAME || HAVE_TWOLAME
  float *pcm_buffer;
  size_t pcm_buffer_size;
  char mp2;
#endif

#if HAVE_LAME
  lame_global_flags *gfp;
  uint64_t num_samples;
  int vbr_tag;
  /* Stuff for its MP3 decoder */
  hip_t          hip;
  mp3data_struct hip_mp3data; /* Header information */
  unsigned char *hip_buffer;  /* Buffer of MPEG data to pass to hip_decode() */
  short          *hip_pcm_l, *hip_pcm_r;  /* The samples it returns */
  /* How many samples are left in hip_pcm_?[] that we haven't returned yet? */
  size_t         hip_pending;

  LSX_DLENTRIES_TO_PTRS(LAME_FUNC_ENTRIES, lame_dl);
#endif

#if HAVE_TWOLAME
  twolame_options *opt;
  LSX_DLENTRIES_TO_PTRS(TWOLAME_FUNC_ENTRIES, twolame_dl);
#endif
} priv_t;
