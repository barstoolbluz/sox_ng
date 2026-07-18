/* Addressable FIFO buffer    Copyright (c) 2007 robs@users.sourceforge.net
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

typedef struct {
  char * data;
  size_t allocation;   /* Number of bytes allocated for data. */
  size_t item_size;    /* Size of each item in data */
  size_t begin;        /* Offset of the first byte to read. */
  size_t end;          /* 1 + Offset of the last byte byte to read. */
} fifo_t;

extern void lsx_fifo_clear(fifo_t * f);
extern void * lsx_fifo_reserve(fifo_t * f, size_t n);
extern void * lsx_fifo_write(fifo_t * f, size_t n, void const * data);
extern void lsx_fifo_trim_to(fifo_t * f, size_t n);
extern void lsx_fifo_trim_by(fifo_t * f, size_t n);
extern size_t lsx_fifo_occupancy(fifo_t * f);
extern void * lsx_fifo_read(fifo_t * f, size_t n, void * data);
#define lsx_fifo_read_ptr(f) lsx_fifo_read(f, (size_t)0, NULL)
extern void lsx_fifo_delete(fifo_t * f);
extern void lsx_fifo_create(fifo_t * f, size_t item_size);
