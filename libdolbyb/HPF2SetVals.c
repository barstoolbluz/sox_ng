/*
 * HPF2SetVals.c, part of libdolbyb:
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

#include "HPF2SetVals.h"

#include "HPFAdj.h"
#include "Param.h"
#include "SetGate.h"

#include <stdlib.h>  /* for malloc() and calloc() */

/* Try adding a facility to start attenuating the filter output */
/* rather than keep increasing to fc of the filter */

/* Amend to upsample and then downsample the audio   */
/* going through the filter according to Param.UpSmp */

#define HPF2SetValsPotTarget  1000000L
#define HPF2SetValsMaxAlpVal  1000000L

/* Calculate the resistance of a FET from its gate voltage. */
/* I will attempt to use */
/*1/Rds=1/rDS=2IDSS/-Vgs(off)*(1-vgs/Vgs(off)+Vds/Vgs(off))*/

static double FETRes(double Vgs)
{
  double MinVgs, NewVgs, Val1, val2, OutVal;

  /* Vgs must be at least slightly more than ParamVgsOff */
  /* to avoid a division by zero                         */
  MinVgs = ParamVgsOff + 0.0001;

  /* Check input is within range */
  if (Vgs < MinVgs)
    NewVgs = MinVgs;
  else
    NewVgs = Vgs;

  /* Lower part of the equation */
  Val1 = 1.0 - NewVgs / ParamVgsOff;

  /* Upper part of the equation */
  val2 = 1.0 / Val1;

  /* Result */
  OutVal = 2000.0 / ParamIDSS * val2;
  if (OutVal > 1000000.0) OutVal = 1000000.0;
  if (OutVal < 100.0)     OutVal = 100.0;
  return OutVal;
}


/*********************************/
/****  Management of tables   ****/
/*********************************/

static uint16_t HPF2SetValsTabPos(dolbyb_t *Param, int64_t VltDif)
{
  /* Return table position for an input voltage */

  return (uint16_t)(VltDif / Param->HPF2SetValsTabRes);
}

static void HPF2SetValsTableRes(dolbyb_t *Param)
{
  /* Set table resolution and maximum voltage */
  Param->HPF2SetValsMaxVlt = round(ParamVltMux * -ParamVgsOff);
  Param->HPF2SetValsTabRes = Param->HPF2SetValsMaxVlt / HPF2SetValsTableSize;
  while (HPF2SetValsTabPos(Param, Param->HPF2SetValsMaxVlt) < HPF2SetValsTableSize)
    Param->HPF2SetValsTabRes++;
  while (HPF2SetValsTabPos(Param, Param->HPF2SetValsMaxVlt) > HPF2SetValsTableSize)
    Param->HPF2SetValsTabRes--;
}


static double HPF2SetValsRes(dolbyb_t *Param, uint16_t TabPos)
{
  /* Return FET resistance for different table positions */
  int64_t Vlt;
  double Vgs, Res;

  Vlt = TabPos * Param->HPF2SetValsTabRes;
  Vgs = 0.0 - (double)Vlt / ParamVltMux;
  Res = FETRes(Vgs);

  return Res;
}


static void HPF2SetValsMakePotTable(dolbyb_t *Param)
{
  /* Make table for first attenuation stage */
  uint16_t TabCnt;
  double ResVal, PotVal;

  /* Set lowest resistance to get multiplier */
  ResVal = HPF2SetValsRes(Param, 0);
  PotVal = ResVal / (ResVal + ParamR2);
  Param->HPF2SetValsPotMux = ParamMuxValue(PotVal, HPF2SetValsPotTarget);

  /* Set table values */
  for (TabCnt = 0; TabCnt <= HPF2SetValsTableSize; TabCnt++) {
    ResVal = HPF2SetValsRes(Param, TabCnt);
    PotVal = ResVal / (ResVal + ParamR2);
    Param->HPF2SetValsPotTab[TabCnt] = round(Param->HPF2SetValsPotMux * PotVal);
  }
}

static double HPF2SetValsMakeAlpVal(dolbyb_t *Param, uint16_t TabPos, uint32_t SmpSec)
{
  /* Calculate alpha value for different table positions */
  /* Also calculate additional attenuation, */
  /* rather than set filter frequency too high */
  double dt, fc, RC, Att;

  dt = 1.0 / SmpSec;
  RC = HPF2SetValsRes(Param, TabPos) * ParamC2;

  /* Minimise frequency if required */
  if (Param->FltTyp == 2 || Param->FltTyp == 4) {
    if (RC > Param->HPF2SetValsMaxRC)
      RC = Param->HPF2SetValsMaxRC;
  }

  /* Maximise frequency if necessary and add attenuation instead */
  if (Param->HPF2SetValsRCDif > 0.0 && RC < Param->HPF2SetValsMinRC) {
    Att = 1.0 - (Param->HPF2SetValsMinRC - RC) / Param->HPF2SetValsRCDif;
    RC = Param->HPF2SetValsMinRC;
  } else
    Att = 1.0;
  Param->HPF2SetValsFLTATb[TabPos] = round(HPF2SetValsFltAttMux * Att);

  /* Adjust RC and calculate alpha */
  fc = 1 / (2 * M_PI * RC);
  RC *= HPFAdj(fc / SmpSec);
  return (RC / (RC + dt));
}


static void HPF2SetValsMakeAlpTable(dolbyb_t *Param)
{
  /* Create table of alpha values for filter */
  uint16_t TabCnt;
  uint32_t SmpSec;
  int64_t AlpVal;
  double a;
  int64_t PrvAlp = 0;

  /* Set some initial values */
  SmpSec = Param->UpSmp * Param->SmpSec;

  /* Set high value to get multiplier */
  a = HPF2SetValsMakeAlpVal(Param, HPF2SetValsTableSize, SmpSec);
  Param->HPF2SetValsAlpMux = ParamMuxValue(a, ParamAlpTgt);

  /* Now calculate table values */
  for (TabCnt = 0; TabCnt <= HPF2SetValsTableSize; TabCnt++) {
    a = HPF2SetValsMakeAlpVal(Param, TabCnt, SmpSec);
    AlpVal = round(Param->HPF2SetValsAlpMux * a);
    if (AlpVal < PrvAlp)
      AlpVal = PrvAlp;
    PrvAlp = AlpVal;
    Param->HPF2SetValsAlpTab[TabCnt] = AlpVal;
  }
}


/********************************************************/
/****  Extra table for attenuation of filter output  ****/
/********************************************************/

static void HPF2SetValsSetHighestFreq(dolbyb_t *Param)
{
  /* Set highest frequency for filter, and so set minimum RC value */
  /* Also set difference in RC when program would like to use higher frequencies */
  uint32_t SmpSec;
  double ResVal, MaxFrq;

  SmpSec = Param->UpSmp * Param->SmpSec;

  MaxFrq = (double)SmpSec * ParamMxFqRt;   /* Max filter frequency allowed */
  Param->HPF2SetValsMinRC = 1 / (2 * M_PI * MaxFrq);  /* RC for max frequency */

  /* Now calculate highest frequency that the program might want to use */
  /* and calculate difference in RC */
  ResVal = HPF2SetValsRes(Param, 0);   /* Lowest resistance FET value */
  Param->HPF2SetValsLowRC = ResVal * ParamC2;   /* RC for highst frequency wanted */
  if (Param->HPF2SetValsMinRC <= Param->HPF2SetValsLowRC)
    Param->HPF2SetValsMinRC = Param->HPF2SetValsLowRC;

  Param->HPF2SetValsRCDif = Param->HPF2SetValsMinRC - Param->HPF2SetValsLowRC;
}


/****************************/
/****   Initialisation   ****/
/****************************/

char *HPF2SetValsInit(dolbyb_t *Param)
{
  if (Param->HPF2SetValsPotTab == NULL)
    Param->HPF2SetValsPotTab = calloc(HPF2SetValsTableSize + 1, sizeof(*Param->HPF2SetValsPotTab));
  if (Param->HPF2SetValsAlpTab == NULL)
    Param->HPF2SetValsAlpTab = calloc(HPF2SetValsTableSize + 1, sizeof(*Param->HPF2SetValsAlpTab));
  if (Param->HPF2SetValsFLTATb == NULL)
    Param->HPF2SetValsFLTATb = calloc(HPF2SetValsTableSize + 1, sizeof(*Param->HPF2SetValsFLTATb));
  if (Param->HPF2SetValsPotTab == NULL ||
      Param->HPF2SetValsAlpTab == NULL ||
      Param->HPF2SetValsFLTATb == NULL) return "Out of memory";

  HPF2SetValsSetHighestFreq(Param);
  Param->HPF2SetValsMaxRC = ParamR1 * ParamC1;
  HPF2SetValsTableRes(Param);
  HPF2SetValsMakePotTable(Param);
  HPF2SetValsMakeAlpTable(Param);
  return NULL;
}


/************************/
/****   Set Values   ****/
/************************/

void HPF2SetVals(dolbyb_t *Param, uint16_t FltNum)
{
  int64_t VltDif, TabVlt, TbVtDf, PotVal, PotDif, AlpVal, AlpDif, FAtVal,
	FAtDif, SVlt;
  uint16_t TabPos, NxTbPs;

  /* Find voltage value */
  SVlt = Param->FETSVt;
  VltDif = SVlt - Param->FETGVt - Param->SetGateOut[FltNum-1];
  if (VltDif < 0)
    VltDif = 0;
  if (VltDif > Param->HPF2SetValsMaxVlt)
    VltDif = Param->HPF2SetValsMaxVlt;

  /* Set table positions from voltage */
  TabPos = HPF2SetValsTabPos(Param, VltDif);

  if (TabPos < HPF2SetValsTableSize)
    NxTbPs = TabPos + 1;   /* Next position in table if possible */
  else
    NxTbPs = HPF2SetValsTableSize;

  /* Work out effective voltage for table position */
  TabVlt = TabPos * Param->HPF2SetValsTabRes;   /* Voltage for table position */
  TbVtDf = VltDif - TabVlt;              /* Difference between actual voltage
                                          * and table voltage*/

  /* Get values for table position and next table position */
  PotVal = Param->HPF2SetValsPotTab[TabPos];
  PotDif = Param->HPF2SetValsPotTab[NxTbPs] - PotVal;
  AlpVal = Param->HPF2SetValsAlpTab[TabPos];
  AlpDif = Param->HPF2SetValsAlpTab[NxTbPs] - AlpVal;
  FAtVal = Param->HPF2SetValsFLTATb[TabPos];
  FAtDif = Param->HPF2SetValsFLTATb[NxTbPs] - FAtVal;

  /* Set final values */
  PotVal += TbVtDf * PotDif / Param->HPF2SetValsTabRes;
  AlpVal += TbVtDf * AlpDif / Param->HPF2SetValsTabRes;
  FAtVal += TbVtDf * FAtDif / Param->HPF2SetValsTabRes;
  Param->HPF2SetValsPot[FltNum-1] = PotVal;
  Param->HPF2SetValsAlp[FltNum-1] = AlpVal;
  Param->HPF2SetValsFPt[FltNum-1] = FAtVal;
}
