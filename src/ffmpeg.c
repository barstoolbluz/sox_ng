/* libSoX ffmpeg file formats
 *
 * Copyright (c) 2024 Martin Guy <sox_ng@fastmail.com>
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

#if USING_FFMPEG
/* "configure" checks that we HAVE_POPEN */

#include <ctype.h>
#include <unistd.h>

extern sox_format_handler_t const * lsx_au_format_fn(void);

/*
 * Open file with ffmpeg
 */
static int startread(sox_format_t * ft)
{
  char *quoted_filename;
  char *p, *q;
  char const * const command_fmt = "ffmpeg -loglevel quiet -nostdin -strict -2 -i \"%s\" -f au -";
  char *command;

  /* Quote special characters in the filename. */
  /* This is for the Unix shell. I dunno about Windows. */
  quoted_filename = lsx_malloc(strlen(ft->filename) * 2 + 1);
  for (p=ft->filename, q=quoted_filename; *p; p++, q++) {
    switch (*p) {
    case '"':
    case '`':
    case '\\':
    case '$':
    case '\n':
      *q++ = '\\';
      break;
    }
    *q = *p;
  }
  *q = '\0';

  command = malloc(strlen(quoted_filename) + strlen(command_fmt) + 1);
  sprintf(command, command_fmt, quoted_filename);
  free(quoted_filename);

  /* If the input is stdin, sox may already have read 256 bytes from it
   * for autodetection so we have to lauch something that feeds ffmpeg
   * the data we've read and then all the rest.
   * Empirically, if stdin comes from a file, ffmpeg rewinds it anyway
   * so the pipe/fork trick is only needed when reading from a pipe.
   */
  if (strcmp(ft->filename, "-") == 0 && !ft->seekable) {
#if defined(HAVE_PIPE) && defined(HAVE_FORK)
    int pipefd[2]; /* [0] is the read end, [1] the write end */

    if (pipe(pipefd) != 0) {
      lsx_fail_errno(ft, errno, "cannot make a pipe\n");
      return SOX_EOF;
    }
    switch (fork()) {
    case -1:
      lsx_fail_errno(ft, errno, "cannot fork to copy stdin\n");
      return SOX_EOF;
    case 0:
      /* Child: Regurgitate the already-read data into the pipe
       * then copy the rest. lsx_rewind() will already have been called.
       */
      close(pipefd[0]);
      {
	char buf[4096];
	int nread;

	while ((nread = lsx_readbuf(ft, buf, sizeof(buf))) > 0) {
	  int nwritten = 0;
	  while (nwritten < nread) {
	      int n = write(pipefd[1], buf + nwritten, nread - nwritten);
	      if (n <= 0) exit(EIO);
	      nwritten += n;
	  }
	}
	exit(nread == 0 ? 0 : EIO);
      }
      break;
    default: /* Parent */
      /* Make ffmpeg's stdin read the pipe; ft->fp will read from ffmpeg */
      close(pipefd[1]);
      if (dup2(pipefd[0], 0) != 0) {
	  lsx_fail_errno(ft, errno, "cannot redirect stdin into pipe\n");
	  return SOX_EOF;
      }
      close(pipefd[0]);
    }
#else
    lsx_warn("When stdin is a pipe, bypass filetype autodetection using -t ffmpeg -");
#endif
  }

#ifdef _WIN32
  ft->fp = popen(command, "rb");
#else
  ft->fp = popen(command, "r");
#endif
  free(command);
  if (ft->fp == NULL) {
    lsx_fail("could not create a pipe for ffmpeg");
    return SOX_EOF;
  }

  return lsx_au_format_fn()->startread(ft);
}

LSX_FORMAT_HANDLER(ffmpeg)
{
  static char const * const names[] = {
    "ffmpeg", /* Special type to force use of ffmpeg */
    NULL
  };
  static sox_format_handler_t handler;

  handler = *lsx_au_format_fn();
  handler.description = "Pseudo format to use ffmpeg";
  handler.names = names;
  handler.startread = startread;
  handler.write_formats = NULL;
  handler.startwrite = NULL;
  handler.write = NULL;
  handler.stopwrite = NULL;
  handler.seek = NULL;

  return &handler;
}

/* All the formats ffmpeg handles that sox doesn't otherwise,
 * created with yet more macros because there are too many! */

/* For example, the three "3gp" macros expand to: */
#if 0
LSX_FORMAT_HANDLER(3gp)
{
  static char const * const names[] = { "3gp", "3gpp", NULL };
  static sox_format_handler_t handler;

  handler = *lsx_ffmpeg_format_fn();
  handler.description = "Third Generation Partnership Project"
  handler.names = names;
  return &handler;
}
#endif

#define FFMPEG_FORMAT(name) \
LSX_FORMAT_HANDLER(name) \
{ static char const * const names[] = {

#define FFMPEG_DESCRIPTION , NULL}; \
  static sox_format_handler_t handler; \
  handler = *lsx_ffmpeg_format_fn(); \
  handler.description =

#define FFMPEG_ENDFORMAT ; \
  handler.names = names; \
  return &handler; \
}

FFMPEG_FORMAT(3g2) "3g2", "3gp2", "3gpp2"
FFMPEG_DESCRIPTION "Third Generation Partnership Project 2"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(3gp) "3gp", "3gpp"
FFMPEG_DESCRIPTION "Third Generation Partnership Project"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(aac) "aac"
FFMPEG_DESCRIPTION "Advanced Audio Coding"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(ac3) "ac3"
FFMPEG_DESCRIPTION "Audio Codec 3 (Dolby Digital)"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(adts) "adts"
FFMPEG_DESCRIPTION "Audio Data Transport Stream"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(adx) "adx"
FFMPEG_DESCRIPTION "CRI ADX"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(ape) "ape"
FFMPEG_DESCRIPTION "Monkey's Audio"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(apm) "apm"
FFMPEG_DESCRIPTION "Ubisoft Rayman 2 APM"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(aptx) "aptx", "aptxhd"
FFMPEG_DESCRIPTION "Audio Processing Technology for Bluetooth"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(argo_asf) "argo_asf"
FFMPEG_DESCRIPTION "Argonaut Games ASF"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(asf) "asf"
FFMPEG_DESCRIPTION "Advanced / Active Streaming Format"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(ast) "ast"
FFMPEG_DESCRIPTION "AST (Audio Stream)"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(avi) "avi"
FFMPEG_DESCRIPTION "Audio Video Interleaved"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(dfpwm) "dfpwm"
FFMPEG_DESCRIPTION "DFPWM1a"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(dts) "dts"
FFMPEG_DESCRIPTION "Digital Theatre Systems"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(eac3) "eac3"
FFMPEG_DESCRIPTION "Enhanced AC-3 Audio"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(f4v) "f4v"
FFMPEG_DESCRIPTION "F4V MOV file"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(flv) "flv"
FFMPEG_DESCRIPTION "Macromedia Flash Video"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(gxf) "gxf"
FFMPEG_DESCRIPTION "General eXchange Format"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(ism) "ism"
FFMPEG_DESCRIPTION "ISM streaming video format"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(kvag) "kvag"
FFMPEG_DESCRIPTION "Simon & Schuster Interactive VAG"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(m4a) "m4a"
FFMPEG_DESCRIPTION "MPEG-4 Audio"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(m4v) "m4v"
FFMPEG_DESCRIPTION "MPEG-4 Video"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(mkv) "mkv", "webm"
FFMPEG_DESCRIPTION "Matroska / WebM format"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(mlp) "mlp"
FFMPEG_DESCRIPTION "Meridian Lossless Packing"
FFMPEG_ENDFORMAT

/* "Registered extensions" according to ffmpeg-formats(1) are these plus
 *  mp4, m4a, 3gp, 3g2, f4v which we handle separately to give them names */
FFMPEG_FORMAT(mov) "mov", "mj2", "m4b", "ism", "isma", "ismv", "psp"
FFMPEG_DESCRIPTION "Quicktime / ISO/IEC Base Media File"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(mp4) "mp4"
FFMPEG_DESCRIPTION "MPEG-4 video"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(mpeg) "mpg", "mpeg", "vcd", "svcd", "vob", "dvd"
FFMPEG_DESCRIPTION "MPEG program stream"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(mpegts) "mpegts"
FFMPEG_DESCRIPTION "MPEG Transport Stream"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(mxf_opatom) "mxf", "mxf_opatom"
FFMPEG_DESCRIPTION "Material eXchange Format Operational Pattern OP1A \"OP-Atom\" format (SMPTE 390M)"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(nut) "nut"
FFMPEG_DESCRIPTION "NUT Container Format"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(oga) "oga"
FFMPEG_DESCRIPTION "Ogg Audio"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(ra)  "ra"
FFMPEG_DESCRIPTION "RealAudio"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(rm)  "rm"
FFMPEG_DESCRIPTION "RealMedia"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(rso) "rso"
FFMPEG_DESCRIPTION "Lego Mindstorms RSO"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(sbc) "sbc"
FFMPEG_DESCRIPTION "Bluetooth SIG low-complexity subband codec audio"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(smjpeg) "smjpeg"
FFMPEG_DESCRIPTION "Loki SDL MJPEG"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(spdif) "spdif"
FFMPEG_DESCRIPTION "IEC 61937 S/PDIF"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(spx) "spx", "speex"
FFMPEG_DESCRIPTION "Ogg Speex"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(tta) "tta"
FFMPEG_DESCRIPTION "True Audio"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(vag) "vag"
FFMPEG_DESCRIPTION "Sony PS2 VAG"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(wma) "wma"
FFMPEG_DESCRIPTION "Windows Media Audio"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(wsaud) "wsaud"
FFMPEG_DESCRIPTION "Westwood Studios audio"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(wtv) "wtv"
FFMPEG_DESCRIPTION "Windows Television"
FFMPEG_ENDFORMAT

#endif
