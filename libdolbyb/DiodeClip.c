/*
 * DiodeClip.c, part of libdolbyb.
 *
 * Copyright (C) 2025 Martin Guy <martinwguy@gmail.com>
 * based on dolbybcsoftwaredecode by Richard Evans 2018.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation. See COPYING for details.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
 * Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "dolbyb.h"

#include "DiodeClip.h"

#include "Param.h"

void DiodeClipInit(dolbyb_t *Param)
{
  double DioVal = ParamDioClp;

  if (Param->ThGain != 1.0) DioVal /= Param->ThGain;
  Param->DiodeClipMaxVal = round(DioVal * ParamVltMux);
  Param->DiodeClipMinVal = -Param->DiodeClipMaxVal;
}

int64_t DiodeClip(dolbyb_t *Param, int64_t InSamp)
{
  /* Clip the output from the side channel if necessary */
  /* to avoid sudden brief overshoots                   */

  if (InSamp > Param->DiodeClipMaxVal) {
    Param->DiodeClipCliped = 1;
    return Param->DiodeClipMaxVal;
  }
  if (InSamp < Param->DiodeClipMinVal) {
    Param->DiodeClipCliped = 1;
    return Param->DiodeClipMinVal;
  }
  Param->DiodeClipCliped = 0;
  return InSamp;
}
