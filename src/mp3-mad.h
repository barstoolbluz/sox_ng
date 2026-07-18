/* Copyright (C) 2025 Martin Guy <martinwguy@gmail.com>
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

/* Declarations for callers of mp3-mad.c */

#if HAVE_MAD
extern int startread_mad(sox_format_t *ft);
extern size_t read_mad(sox_format_t *ft, sox_sample_t *buf, size_t len);
extern int seek_mad(sox_format_t * ft, sox_uint64_t offset);
extern int stopread_mad(sox_format_t *ft);
#else
# define startread_mad NULL
# define read_mad NULL
# define seek_mad NULL
# define stopread_mad NULL
#endif
