/*
 * Copyright (c) 2008 Rob Sykes (robs@users.sourceforge.net)
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
#include "fft4g.h"
#define  FIFO_SIZE_T int
#include "fifo.h"

typedef struct {
  int        dft_length, num_taps, post_peak;
  double     * coefs;
} dft_filter_t;

typedef struct {
  uint64_t   samples_in, samples_out;
  fifo_t     input_fifo, output_fifo;
  dft_filter_t   filter, * filter_ptr;
} dft_filter_priv_t;

void lsx_set_dft_filter(dft_filter_t * f, double * h, int n, int post_peak);
