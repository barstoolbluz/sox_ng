/* Copyright (c) 2004-2015 LoRd_MuldeR <mulder2@gmx.de>
   File: win32-unicode.h

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
#ifndef WIN32_UNICODE_H
#define WIN32_UNICODE_H

#ifdef _WIN32

#include <stdio.h>
#include <sys/stat.h>

#define WIN_UNICODE 1

extern FILE    * lsx_fopen(const char *filename_utf8, const char *mode_utf8);
extern int       lsx_stat(const char *path_utf8, struct stat *buf);
extern int       lsx_unlink(const char *path_utf8);

extern char    * win32_utf16_to_utf8(const wchar_t *input);
extern wchar_t * win32_utf8_to_utf16(const char *input);

# else  /* _WIN32 */

#define lsx_fopen(a,b) fopen(a,b)
#define lsx_stat(a,b)  stat(a,b)
#define lsx_unlink(a)  unlink(a)

#endif  /* _WIN32 */

#endif
