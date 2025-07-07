/*
 * HPF2.c, part of libdolbyb.
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

#include "HPF2.h"

#include "DCfilter.h"
#include "HPF1.h"
#include "HPF2SetVals.h"
#include "Param.h"

#define HPF2NumberOfFilters  ParamMaxChnl

/****************************/
/****   Initialisation   ****/
/****************************/

void HPF2Init(dolbyb_t *Param)
{
  uint16_t Chn;

  if (Param->AllHig)
    Param->HPF2UpSmp = 1;
  else
    Param->HPF2UpSmp = Param->UpSmp;

  /* Set previous sample values to 0 */
  for (Chn = 0; Chn <= HPF2NumberOfFilters - 1; Chn++) {
    Param->HPF2PrvIn[Chn] = 0;
    Param->HPF2PrvOt[Chn] = 0;
  }
}


/*****************************************/
/****  Side channel filter routines   ****/
/*****************************************/

int64_t HPF2Check(dolbyb_t *Param, int64_t InSamp, uint16_t Chn)
{
  int64_t PotVal, SmpVal, OutVal;
  int64_t OutTot = 0;
  uint16_t UpSmCt = 0;
  int64_t PrvIn, PrvOut;

  /* Calculate output value for the potentiometer */
  if (Param->FltTyp > 2)
    PotVal = 0;
  else
    PotVal = InSamp * Param->HPF2SetValsPot[Chn-1] / Param->HPF2SetValsPotMux;

  /* Apply sliding high pass filter, several times if appropiate */
  SmpVal = InSamp - PotVal;
  PrvIn = Param->HPF2PrvIn[Chn-1];
  PrvOut = Param->HPF2PrvOt[Chn-1];
  while (UpSmCt < Param->HPF2UpSmp) {
    UpSmCt++;
    /* Sliding high pass filter */
    OutVal = PrvOut + SmpVal - PrvIn;
    OutVal *= Param->HPF2SetValsAlp[Chn-1];
    OutVal /= Param->HPF2SetValsAlpMux;

    /* Update previous values */
    PrvIn = SmpVal;
    PrvOut = OutVal;
    /* Add result to total */
    OutTot += OutVal;
  }

  /* Remember previous values in case they become permanent */
  Param->HPF2PrvInVal[Chn-1] = PrvIn;
  Param->HPF2PrvOtVal[Chn-1] = PrvOut;

  /* Extract values from totals */
  OutVal = OutTot / Param->HPF2UpSmp;
  OutVal = OutVal * Param->HPF2SetValsFPt[Chn-1] / HPF2SetValsFltAttMux;

  /* Newer method: potentiometer is difference from filter */
  if (Param->FltTyp > 2)
    PotVal = (InSamp - OutVal) * Param->HPF2SetValsPot[Chn-1] / Param->HPF2SetValsPotMux;

  /* If new filtering method, apply fixed filter to potentiometer output */
  if (Param->FltTyp == 2 || Param->FltTyp == 4)
    PotVal = HPF1Check(Param, PotVal, Chn);

  /* Send output */
  OutVal += PotVal;
  Param->HPF2PrvOtTot[Chn-1] = OutVal;
  return OutVal;
}


int64_t HPF2(dolbyb_t *Param, int64_t InSamp, uint16_t Chn)
{
  int64_t PotVal, SmpVal, OutVal, LpFilt;
  int64_t OutTot = 0;
  uint16_t UpSmCt = 0;
  uint16_t DCFltN;

  /* Calculate output value for the potentiometer */
  if (Param->FltTyp > 2)
    PotVal = 0;
  else
    PotVal = InSamp * Param->HPF2SetValsPot[Chn-1] / Param->HPF2SetValsPotMux;

  /* Apply sliding high pass filter, several times if appropiate */
  SmpVal = InSamp - PotVal;
  while (UpSmCt < Param->HPF2UpSmp) {
    UpSmCt++;
    /* Sliding high pass filter */
    OutVal = Param->HPF2PrvOt[Chn-1] + SmpVal - Param->HPF2PrvIn[Chn-1];
    OutVal *= Param->HPF2SetValsAlp[Chn-1];
    OutVal /= Param->HPF2SetValsAlpMux;

    /* Update previous values */
    Param->HPF2PrvIn[Chn-1] = SmpVal;
    Param->HPF2PrvOt[Chn-1] = OutVal;
    /* Add result to total */
    OutTot += OutVal;
  }
  /* Extract values from totals */
  OutVal = OutTot / UpSmCt;
  OutVal = OutVal * Param->HPF2SetValsFPt[Chn-1] / HPF2SetValsFltAttMux;

  /* Run through low pass filter for DC correction */
  DCFltN = HPF1NumberOfFilters + Chn;
  LpFilt = DCfilter(Param, OutVal, DCFltN);

  /* Subtract DC correction from previous output */
  /* to make correction for next time */
  Param->HPF2PrvOt[Chn-1] -= LpFilt;

  /* Newer method: potentiometer is difference from filter */
  if (Param->FltTyp > 2)
    PotVal = (InSamp - OutVal) * Param->HPF2SetValsPot[Chn-1] / Param->HPF2SetValsPotMux;

  /* If new filtering method, apply fixed filter to potentiometer output */
  if (Param->FltTyp == 2 || Param->FltTyp == 4)
    PotVal = HPF1(Param, PotVal, Chn);

  /* Send output */
  return OutVal + PotVal;
}


void HPF2Update(dolbyb_t *Param, uint16_t Chn)
{
  /* Update from last guess rather than running the filter again */
  int64_t PrevIn, PrvOut, PrvTot, LpFilt;
  uint16_t DCFltN;

  PrevIn = Param->HPF2PrvInVal[Chn-1];
  PrvOut = Param->HPF2PrvOtVal[Chn-1];
  PrvTot = Param->HPF2PrvOtTot[Chn-1];

  /* Update previous values */
  Param->HPF2PrvIn[Chn-1] = PrevIn;

  /* Run through pass filter for DC correction */
  DCFltN = HPF1NumberOfFilters + Chn;
  LpFilt = DCfilter(Param, PrvTot, DCFltN);

  /* Subtract DC correction from previous output */
  /* to make correction for next time */
  Param->HPF2PrvOt[Chn-1] = PrvOut - LpFilt;
}
