/*
 * FindOutSmp.c - Routines to find the right output value
 * by trying different values through the side path.
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

#include "FindOutSmp.h"

#include "DiodeClip.h"
#include "Mixers.h"
#include "Param.h"
#include "SidePath.h"

#define FindOutSmpMinLoop  3
#define FindOutSmpNumberOfFilters  ParamMaxChnl

#define FindOutSmpHigVal  500000000000.0

/*******************************************/
/****   Routines to find output value   ****/
/*******************************************/

void FindOutSmpInit(dolbyb_t *Param)
{
  uint16_t Chn;

  Param->FindOutSmpMaxDif = round(Param->DecAMX * (ParamVltMux / Param->SmpMux));
  if (Param->FindOutSmpMaxDif < 10)
    Param->FindOutSmpMaxDif = 10;
  for (Chn = 0; Chn <= FindOutSmpNumberOfFilters - 1; Chn++) {
    Param->FindOutSmpPrvIn[Chn] = 0;
    Param->FindOutSmpPrvOt[Chn] = 0;
  }
}

static uint64_t FindOutSmpAmpVal(int64_t InVal)
{
  if (InVal < 0)
    return (-InVal);
  else
    return InVal;
}

static int64_t FindOutSmpCheckRange(int64_t InSamp, int64_t HigSmp, int64_t LowSmp)
{
  if (InSamp <= LowSmp)      return (LowSmp + 1);
  else if (InSamp >= HigSmp) return (HigSmp - 1);
  else                       return InSamp;
}

static double FindOutSmpCalcGuessMux(int64_t Gs1, int64_t GsP, int64_t Ot1, int64_t OtP)
{
  /* Calculate multiple used to guess next sample */
  int TryGus;
  int64_t DGs, DOt;
  uint64_t ADG, ADO;

  DGs = GsP - Gs1;
  DOt = OtP - Ot1;
  TryGus = (DGs <= 0 || DOt <= 0);
  if (!TryGus)
    TryGus = (DGs < 0 && DOt < 0);
  if (TryGus) {
    ADG = FindOutSmpAmpVal(DGs);
    ADO = FindOutSmpAmpVal(DOt);
    TryGus = (ADO < ADG * 20);
  }
  if (TryGus)
    return (double)DOt / DGs;
  else {
    return 1.0;
    /* 1.0 means it won't be used */
  }
}

static int64_t FindOutSmpTestGuess(dolbyb_t *Param, int64_t InSamp, int64_t TstSmp, uint16_t Chn)
{
  return MixersDecode(InSamp, SidePathCheck(Param, TstSmp, Chn));
}

static void FindOutSmpUpdateHigLow(int64_t NxtVal, int64_t OutVal, int64_t *HigVal,
				   int64_t *LowVal)
{
  if (OutVal == NxtVal) {  /* Guess was correct */
    *LowVal = NxtVal - 1;
    *HigVal = NxtVal + 1;
  }
  if (OutVal > NxtVal) {  /* Guess was too low */
    *LowVal = NxtVal;
    if (OutVal < *HigVal)
      *HigVal = OutVal + 1;
  }
  if (OutVal >= NxtVal)  /* Guess was too high */
    return;
  *HigVal = NxtVal;
  if (OutVal > *LowVal)
    *LowVal = OutVal - 1;
}

int64_t FindOutSmp(dolbyb_t *Param, int64_t InSamp, uint16_t Chn)
{
  int64_t LowVal = -FindOutSmpHigVal, HigVal = FindOutSmpHigVal;
  int64_t NxtGus, NxtOut, NxtDif;
  uint16_t GusCnt = 0, GusSta = 0;   /* 0=No Valid guess so far */
  /* 1= 1 valid guess so far */
  /* 2= 2 or more valid guesses */
  /* 3= Switch to binary search only */
  /* Guess Variables */
  int32_t GusVal[2], OutVal[2];
  double GusMux = 0.0;
  int GusNow;
  uint16_t LopCnt = 1;

  /* To stop compiler warnings */
  GusVal[0] = 0;
  GusVal[1] = 0;
  OutVal[0] = 0;
  OutVal[1] = 0;
  /* Initalise high and low, using very large values */

  /* Initialize guess method status */

  /* Make initial guess */
  NxtGus = Param->FindOutSmpPrvOt[Chn-1] + InSamp - Param->FindOutSmpPrvIn[Chn-1];

  /* Try initial guess */
  NxtOut = FindOutSmpTestGuess(Param, InSamp, NxtGus, Chn);
  /* Try again if clipped */
  if (!Param->DiodeClipCliped) {
    NxtGus = NxtOut;
    NxtOut = FindOutSmpTestGuess(Param, InSamp, NxtGus, Chn);
  }

  if (!Param->DiodeClipCliped) {
    GusSta = 1;
    GusVal[0] = NxtGus;
    OutVal[0] = NxtOut;
  }

  FindOutSmpUpdateHigLow(NxtGus, NxtOut, &HigVal, &LowVal);
  NxtDif = Param->FindOutSmpMaxDif + 1;   /* To make it sure there is a loop */

  /* Loop to make further guesses */
  while (NxtDif > Param->FindOutSmpMaxDif) {
    LopCnt++;
    /* Decide whether to use guess or binary search */
    GusNow = (GusSta == 2 && GusCnt < 2);
    if (GusNow) {
      GusMux = FindOutSmpCalcGuessMux(GusVal[0], GusVal[1], OutVal[0],
				      OutVal[1]);
      GusNow = (GusMux < 0.95 || GusMux > 1.05);
    }
    if (GusNow) {
      NxtGus = round((GusMux * GusVal[0] - OutVal[0]) / (GusMux - 1.0));
      GusNow = (NxtGus > LowVal && NxtGus < HigVal);
    }

    /* If guess was not OK then use binary search */
    if (GusNow)
      GusCnt++;
    else
      NxtGus = LowVal + (HigVal - LowVal) / 2;

    /* Check that guess is not out of range */
    NxtGus = FindOutSmpCheckRange(NxtGus, HigVal, LowVal);
    /* Try out new value */
    NxtOut = FindOutSmpTestGuess(Param, InSamp, NxtGus, Chn);
    if (!Param->DiodeClipCliped) {
      if (GusSta < 2)
	GusSta++;
      GusVal[GusSta-1] = NxtGus;
      OutVal[GusSta-1] = NxtOut;
    }
    FindOutSmpUpdateHigLow(NxtGus, NxtOut, &HigVal, &LowVal);
    NxtDif = HigVal - LowVal;
  }
  Param->FindOutSmpPrvIn[Chn-1] = InSamp;
  Param->FindOutSmpPrvOt[Chn-1] = NxtOut;
  return NxtOut;
}
