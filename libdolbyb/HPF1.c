/*
 * HPF1.c - a library of routines to perform the first
 * high pass filter stages for the DolbyB or DolbyC side path.
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

#include "HPF1.h"
#include "DCfilter.h"
#include "HPFAdj.h"
#include "Param.h"

/****************************/
/****   Initialisation   ****/
/****************************/

void HPF1Init(dolbyb_t *Param)
{
  uint32_t SmpSec;
  double dt, RC, fc, a;
  uint16_t FltCnt;

  if (Param->AllHig)
    SmpSec = Param->SmpSec * Param->UpSmp;
  else
    SmpSec = Param->SmpSec;

  /* Calculate dt */
  dt = 1.0 / SmpSec;

  /* Calculate RC for high pass filter */
  RC = ParamR1 * ParamC1;

  /* Adjust RC */
  fc = 1 / (2 * M_PI * RC);
  RC *= HPFAdj(fc / SmpSec);

  /* Calculate alpha for high pass filter */
  a = RC / (RC + dt);

  /* Convert to integers */
  Param->HPF1AlpMux = ParamMuxValue(a, ParamAlpTgt);
  Param->HPF1Alp = round(a * Param->HPF1AlpMux);

  /* Set previous sample values to 0 */
  for (FltCnt = 0; FltCnt <= HPF1NumberOfFilters - 1; FltCnt++) {
    Param->HPF1PrvIn[FltCnt] = 0;
    Param->HPF1PrvOt[FltCnt] = 0;
  }
}

/*****************************************/
/****  Side channel filter routines   ****/
/*****************************************/

static int64_t HPF1Filter(dolbyb_t *Param, int64_t InSmp, uint16_t Chn)
{
  int64_t OutVal;

  OutVal = Param->HPF1PrvOt[Chn-1] + InSmp - Param->HPF1PrvIn[Chn-1];
  OutVal *= Param->HPF1Alp;
  OutVal /= Param->HPF1AlpMux;
  return OutVal;
}

static void HPF1SetPrevious(dolbyb_t *Param, int64_t InSmp, int64_t OtSmp, uint16_t Chn)
{
  /* Run output through DC correction */
  /* and set previous values */
  int64_t LpFilt;

  /* Run through low pass filter (for DC correction) */
  LpFilt = DCfilter(Param, OtSmp, Chn);

  /* Update previous values */
  Param->HPF1PrvIn[Chn-1] = InSmp;
  Param->HPF1PrvOt[Chn-1] = OtSmp - LpFilt;
      /* Subtract low pass filter for next time */
}

int64_t HPF1Check(dolbyb_t *Param, int64_t InSamp, uint16_t Chn)
{
  /* Run a trial of HPF1, without changing the previous filter values */
  int64_t OutVal;

  Param->HPF1PrvInVal[Chn-1] = InSamp;
  OutVal = HPF1Filter(Param, InSamp, Chn);
  Param->HPF1PrvOtVal[Chn-1] = OutVal;
  return OutVal;
}

int64_t HPF1(dolbyb_t *Param, int64_t InSamp, uint16_t Chn)
{
  /* Apply filter and update previous values */
  int64_t OutVal;

  OutVal = HPF1Filter(Param, InSamp, Chn);
  HPF1SetPrevious(Param, InSamp, OutVal, Chn);
  return OutVal;
}

void HPF1Update(dolbyb_t *Param, uint16_t Chn)
{
  /* Update from last guess rather than running the filter again */
  HPF1SetPrevious(Param, Param->HPF1PrvInVal[Chn-1], Param->HPF1PrvOtVal[Chn-1], Chn);
}
