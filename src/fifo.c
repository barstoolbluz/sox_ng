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

#include "sox_i.h"
#include "fifo.h"

#define FIFO_MIN 0x4000

void lsx_fifo_clear(fifo_t * f)
{
  f->end = f->begin = 0;
  free(f->data);
  f->data = NULL;
  f->allocation = 0;
}

void * lsx_fifo_reserve(fifo_t * f, size_t n)
{
  n *= f->item_size;

  if (f->begin == f->end)
    lsx_fifo_clear(f);

  while (1) {
    if (f->end + n <= f->allocation) {
      void *p = f->data + f->end;

      f->end += n;
      return p;
    }
    if (f->begin > FIFO_MIN) {
      memmove(f->data, f->data + f->begin, f->end - f->begin);
      f->end -= f->begin;
      f->begin = 0;
      continue;
    }
    if (f->allocation == 0) f->allocation = FIFO_MIN;
    while (f->allocation < f->end + n) f->allocation *= 2;
    f->data = lsx_realloc(f->data, f->allocation);
  }
}

void * lsx_fifo_write(fifo_t * f, size_t n, void const * data)
{
  void * s = lsx_fifo_reserve(f, n);
  if (data)
    memcpy(s, data, n * f->item_size);
  return s;
}

void lsx_fifo_trim_to(fifo_t * f, size_t n)
{
  n *= f->item_size;
  f->end = f->begin + n;
}

void lsx_fifo_trim_by(fifo_t * f, size_t n)
{
  n *= f->item_size;
  f->end -= n;
}

size_t lsx_fifo_occupancy(fifo_t * f)
{
  return (f->end - f->begin) / f->item_size;
}

void * lsx_fifo_read(fifo_t * f, size_t n, void * data)
{
  char * ret = f->data + f->begin;
  n *= f->item_size;
  if (n > (size_t)(f->end - f->begin))
    return NULL;
  if (data)
    memcpy(data, ret, (size_t)n);
  f->begin += n;
  return ret;
}

void lsx_fifo_delete(fifo_t * f)
{
  free(f->data);
}

void lsx_fifo_create(fifo_t * f, size_t item_size)
{
  f->item_size = item_size;
  lsx_fifo_clear(f);
}
