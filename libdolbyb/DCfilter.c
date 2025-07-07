/*
 * DCfilter.c - low pass filters used for DC correction.
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

#include "DCfilter.h"

#include "LPFAdj.h"
#include "Param.h"

#define DCfilterNumberOfFilters  (ParamMaxChnl * 2)

/* Set a multiplier value so that alpha ends up as 1 (or just over) */
static uint64_t ParamLowAlphaMuxValue(double AlpVal)
{
  uint64_t OutMux;

  OutMux = round(1.0 / AlpVal);

  /* Decrease if too high */
  while (AlpVal * OutMux > 1.0)
    OutMux--;

  /* Increase if too low */
  while (AlpVal * OutMux < 1.0)
    OutMux++;

  return OutMux;
}

/****************************/
/****   Initialisation   ****/
/****************************/

void DCfilterInit(dolbyb_t *Param)
{
  uint32_t SmpSec;
  double dt, RC, fc, a;
  uint16_t Flt;

  if (Param->AllHig)
    SmpSec = Param->SmpSec * Param->UpSmp;
  else
    SmpSec = Param->SmpSec;

  /* Calculate dt */
  dt = 1.0 / SmpSec;

  /* Calculate RC for low pass filters */
  RC = ParamLR * ParamLC;

  /* Adjust RC */
  fc = 1 / (2 * M_PI * RC);
  RC *= LPFadj(fc / SmpSec);

  /* Calculate alpha for low pass filter */
  a = dt / (RC + dt);

  /* Convert to integers */
  Param->DCfilterAlpMux = ParamLowAlphaMuxValue(a);
  Param->DCfilterAlp = round(a * Param->DCfilterAlpMux);

  for (Flt = 0; Flt <= DCfilterNumberOfFilters - 1; Flt++) {
    Param->DCfilterPrvVal[Flt] = 0;
    Param->DCfilterPrvAccVal[Flt] = 0;
  }
}

/******************************************/
/****  DC correction filter routines   ****/
/******************************************/

int64_t DCfilter(dolbyb_t *Param, int64_t InSamp, uint16_t Flt)
{
  /* Final run of filters, involves DC correction */
  int64_t AcuVal, OutVal;

  OutVal = InSamp - Param->DCfilterPrvVal[Flt-1];
  AcuVal = OutVal * Param->DCfilterAlp;
  /* Add some of the values stored previously */
  /* To reduce rounding errors */
  AcuVal += Param->DCfilterPrvAccVal[Flt-1];
  OutVal = AcuVal / Param->DCfilterAlpMux;
  Param->DCfilterPrvAccVal[Flt-1] = AcuVal % Param->DCfilterAlpMux;
  OutVal += Param->DCfilterPrvVal[Flt-1];
  Param->DCfilterPrvVal[Flt-1] = OutVal;
  return OutVal;
}
