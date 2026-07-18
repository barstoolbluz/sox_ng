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

/* Declarations for callers of mp3-twolame.c */

#if HAVE_TWOLAME
extern int startwrite_twolame(sox_format_t *ft);
extern size_t write_twolame(sox_format_t *ft, const sox_sample_t *buf, size_t len);
extern int stopwrite_twolame(sox_format_t *ft);
#else
# define startwrite_twolame NULL
# define write_twolame NULL
# define stopwrite_twolame NULL
#endif
