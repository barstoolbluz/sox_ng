/* Copyright (c) 2004-2015 LoRd_MuldeR <mulder2@gmx.de>
   File: win32-unicode.c

   This file was originally part of a patch included with LameXP,
   released under the same license as the original audio tools.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   - Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

   - Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#if defined _WIN32

#include "win32-unicode.h"

#include <windows.h>
#include <io.h>

extern void *lsx_realloc_array(void *p, size_t n, size_t size);
#define lsx_valloc(v,n)  v = lsx_realloc_array(NULL, (n), sizeof(*(v)))

char *win32_utf16_to_utf8(const wchar_t *input)
{
  char *Buffer;
  int BuffSize = 0, Result = 0;

  BuffSize = WideCharToMultiByte(CP_UTF8, 0, input, -1, NULL, 0, NULL, NULL);
  lsx_valloc(Buffer, BuffSize);
  Result = WideCharToMultiByte(CP_UTF8, 0, input, -1, Buffer, BuffSize, NULL, NULL);

  return ((Result > 0) && (Result <= BuffSize)) ? Buffer : NULL;
}

wchar_t *win32_utf8_to_utf16(const char *input)
{
  wchar_t *Buffer;
  int BuffSize = 0, Result = 0;

  BuffSize = MultiByteToWideChar(CP_UTF8, 0, input, -1, NULL, 0);
  lsx_valloc(Buffer, BuffSize);
  Result = MultiByteToWideChar(CP_UTF8, 0, input, -1, Buffer, BuffSize);

  return ((Result > 0) && (Result <= BuffSize)) ? Buffer : NULL;
}

FILE *lsx_fopen(const char *filename_utf8, const char *mode_utf8)
{
  FILE *ret = NULL;
  wchar_t *filename_utf16 = win32_utf8_to_utf16(filename_utf8);
  wchar_t *mode_utf16 = win32_utf8_to_utf16(mode_utf8);
	
  if(filename_utf16 && mode_utf16) {
    ret = _wfopen(filename_utf16, mode_utf16);
  }

  if(filename_utf16) free(filename_utf16);
  if(mode_utf16) free(mode_utf16);

  return ret;
}

int lsx_stat(const char *path_utf8, struct stat *buf)
{
  int ret = -1;
  struct _stat st;
	
  wchar_t *path_utf16 = win32_utf8_to_utf16(path_utf8);
  if(path_utf16) {
    ret = _wstat(path_utf16, &st);
    buf->st_mode = st.st_mode;
    free(path_utf16);
  }
	
  return ret;
}

int lsx_unlink(const char *path_utf8)
{
  int ret = -1;
	
  wchar_t *path_utf16 = win32_utf8_to_utf16(path_utf8);
  if(path_utf16) {
    ret = _wunlink(path_utf16);
    free(path_utf16);
  }

  return ret;
}

#else

char unicode_dummy;	/* Silence compiler warning */

#endif
