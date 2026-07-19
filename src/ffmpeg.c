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
static int startread_ffmpeg(sox_format_t * ft)
{
  char *command_argv[] = {
    "ffmpeg",
    "-loglevel", "quiet",
    "-nostdin",
    "-strict", "-2",
    "-i", NULL,
    "-f", "au",
    "-",
    NULL
  };
#define filename_index 7   /* the NULL after "-i" */

  command_argv[filename_index] = ft->filename;

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
	  lsx_fail_errno(ft, errno, "cannot redirect stdin into a pipe\n");
	  return SOX_EOF;
      }
      close(pipefd[0]);
    }
#else
    lsx_warn("when stdin is a pipe, bypass filetype autodetection using -t ffmpeg -");
#endif
  }

  ft->fp = lsx_popen(command_argv, 'r', filename_index);
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
    /* Names of the format-specific handlers below */
    "3gp", "3g2",
    "aa", "aac", "ac3", "act", "adts", "adx", "ape", "apm", "aptx", "argo_asf",
    "asf", "ast", "avi", "dfpwm", "dts",
    "ea", "eac3", "f4v", "flv", "gxf", "ism", "kvag",
    "m4a", "m4v", "mkv", "mlp", "mov", "mp4", "mpeg", "mpegts", "mxf_opatom",
    "nut", "oga", "ra", "rm", "rso",
    "sbc", "smjpeg", "spdif", "spx", "tta", "vag", "wma", "wsaud", "wtv",
    /* Other audio filename extensions that ffmpeg can decode */
    /* Ripped out because when they are dynamic modules, static ffmpeg
     * usurps them and renders them read-only
     "caf", "flac", "ircam", "mp2", "mp3", "ogg", "sox", "voc", "w64", "wv",
     */
    NULL
  };
  static sox_format_handler_t handler;

  handler = *lsx_au_format_fn();
  handler.description = "Pseudo format to use ffmpeg";
  handler.names = names;
  handler.startread = startread_ffmpeg;
  handler.write_formats = NULL;
  handler.startwrite = NULL;
  handler.write = NULL;
  handler.stopwrite = NULL;
  handler.seek = NULL;

  return &handler;
}

/* All the formats that ffmpeg handles that sox doesn't otherwise,
 * created with yet more macros because there are too many!
 * This lets us add a description for --help-format and
 * lets us autodetect them from their header contents in formats.c
 *
 * For example, the "3gp" macro expands to:
LSX_FORMAT_HANDLER(3gp)
{
  static char const * const names[] = { "3gp", "3gpp", NULL };
  static sox_format_handler_t handler;

  handler = *lsx_ffmpeg_format_fn();
  handler.description = "Third Generation Partnership Project"
  handler.names = names;
  return &handler;
}
 */

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

FFMPEG_FORMAT(3gp) "3gp", "3gpp"
FFMPEG_DESCRIPTION "Third Generation Partnership Project"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(3g2) "3g2", "3gp2", "3gpp2"
FFMPEG_DESCRIPTION "Third Generation Partnership Project 2"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(aa) "aa", "aax"
FFMPEG_DESCRIPTION "Audible Audiobook"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(aac) "aac"
FFMPEG_DESCRIPTION "Advanced Audio Coding"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(ac3) "ac3"
FFMPEG_DESCRIPTION "Audio Codec 3 (Dolby Digital)"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(act) "act"
FFMPEG_DESCRIPTION "G729A speech compression"
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
FFMPEG_DESCRIPTION "Advanced Systems Format"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(ast) "ast"
FFMPEG_DESCRIPTION "AST (Audio Stream)"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(avi) "avi"
FFMPEG_DESCRIPTION "Audio Video Interleaved"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(dfpwm) "dfpwm"
FFMPEG_DESCRIPTION "Dynamic Filter Pulse Width Modulation"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(dts) "dts"
FFMPEG_DESCRIPTION "Digital Theatre Systems"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(ea) "ea"
FFMPEG_DESCRIPTION "Electronic Arts Multimedia"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(eac3) "eac3"
FFMPEG_DESCRIPTION "Enhanced AC-3 Audio"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(f4v) "f4v"
FFMPEG_DESCRIPTION "F4V MOV file"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(flv) "flv", "kux"
FFMPEG_DESCRIPTION "Macromedia Flash Video"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(gxf) "gxf"
FFMPEG_DESCRIPTION "General eXchange Format"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(ism) "ism"
FFMPEG_DESCRIPTION "ISM streaming video"
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
FFMPEG_DESCRIPTION "Matroska / WebM"
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
