/*
 * SidePath.c - Side path routines.
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

#include "Param.h"
#include "SidePath.h"

#include "SetGate.h"
#include "HPF2SetVals.h"
#include "DCfilter.h"
#include "HPF1.h"
#include "HPF2.h"
#include "DiodeClip.h"

#define SidePathNumberOfPaths  ParamMaxChnl

char *SidePathInit(dolbyb_t *Param)
{
  double AmpVal;
  uint16_t TabCnt;
  char *err;

  DCfilterInit(Param);
  HPF1Init(Param);
  HPF2Init(Param);
  if ((err = SetGateInit(Param))) return err;
  if ((err = HPF2SetValsInit(Param))) return err;
  DiodeClipInit(Param);

  /* Set side path gain */
  AmpVal = Param->SidAmp;
  Param->SidePathGainMux = ParamMuxValue(AmpVal, 1048576L);
  Param->SidePathGainVal = round(AmpVal * Param->SidePathGainMux);

  /* Set gain before gain control circuits */
  AmpVal = ParamGanAmp;
  if (Param->ThGain != 1.0) AmpVal *= Param->ThGain;
  Param->SidePathGcMux = ParamMuxValue(AmpVal, 1048576L);
  Param->SidePathGcVal = round(AmpVal * Param->SidePathGcMux);

  /* Set some initial gain control values */
  for (TabCnt = 1; TabCnt <= SidePathNumberOfPaths; TabCnt++) {
    SetGate(Param, 0, TabCnt);
    HPF2SetVals(Param, TabCnt);
  }
  return NULL;
}


int64_t SidePathCheck(dolbyb_t *Param, int64_t InSamp, uint16_t Chn)
{
  /* Apply Dolby side path to a sample, but only as a check */
  /* so do not update filters and gain controll             */
  int64_t SmpVal;

  /* Apply gain for audio entering side path */
  SmpVal = InSamp * Param->SidePathGainVal / Param->SidePathGainMux;

  /* Apply 1st high pass filter (if filter method 1 or 3) */
  if (Param->FltTyp == 1 || Param->FltTyp == 3)
    SmpVal = HPF1Check(Param, SmpVal, Chn);

  /* Apply sliding filter */
  SmpVal = HPF2Check(Param, SmpVal, Chn);

  /* Add overshoot diodes and return the result */
  return DiodeClip(Param, SmpVal);
}


int64_t SidePath(dolbyb_t *Param, int64_t InSamp, uint16_t Chn)
{
  /* Apply Dolby side path to a sample */
  int64_t SmpVal;

  /* Apply gain for audio entering side path */
  SmpVal = InSamp * Param->SidePathGainVal / Param->SidePathGainMux;

  /* Apply 1st high pass filter (if filter method 1 or 3) */
  if (Param->FltTyp == 1 || Param->FltTyp == 3)
    SmpVal = HPF1(Param, SmpVal, Chn);

  /* Apply sliding filter */
  SmpVal = HPF2(Param, SmpVal, Chn);

  /* Now send to gain control circuits */
  /* Apply gain before sending to gain control */
  if (Param->FETClp)
    Param->SetGateOut[Chn-1] = 0;
  else
    SetGate(Param, SmpVal * Param->SidePathGcVal / Param->SidePathGcMux, Chn);
  HPF2SetVals(Param, Chn);

  /* Add overshoot diodes and return the result */
  return DiodeClip(Param, SmpVal);
}


int64_t SidePathUpdate(dolbyb_t *Param, uint16_t Chn)
{
  int64_t OutVal, GanSmp;

  HPF1Update(Param, Chn);
  HPF2Update(Param, Chn);
  OutVal = Param->HPF2PrvOtTot[Chn-1];

  /* Now send to gain control circuits */
  /* Apply gain before sending to gain control */
  GanSmp = OutVal * Param->SidePathGcVal / Param->SidePathGcMux;
  if (Param->FETClp)
    Param->SetGateOut[Chn-1] = 0;
  else
    SetGate(Param, GanSmp, Chn);
  HPF2SetVals(Param, Chn);

  /* Add overshoot diodes and return the result */
  return DiodeClip(Param, OutVal);
}
