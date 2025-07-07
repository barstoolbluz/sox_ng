/*
 * SetGate.c - a library to set the gate voltage of an FET
 * depending upon the level of the audio.
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

#include "SetGate.h"

#include "Param.h"
#include "LPFAdj.h"

#include <stdlib.h>  /* for malloc() etc */

#define SetGatesMaxAlpVal 1000000L
#define SetGateMax64      9000000000000000000.0
#define SetGateTabPosDiv1 (ParamVltMux / 100)
#define SetGateTabPosDiv2 (ParamVltMux / 1000)
#define SetGateDioVlt     (ParamVltMux * 6 / 10)

/***************************/
/***   Diode functions   ***/
/***************************/

#define DiodeE          2.718281828459
    /* e = 2.7182818284590452353602874713527.... */
#define DiodeIs         0.000000000001
    /* =10**-12 for silicon 10**-9 for germaium */
#define DiodeVt         0.02585

#define DiodeI(Vd) (DiodeIs * pow(DiodeE, (double)(Vd) / DiodeVt - 1));


/*******************************************************/
/****   Routines to set the resistance of a diode   ****/
/****   in series with a 2.7K resistor              ****/
/*******************************************************/

static void SetGateSetPrvRes(dolbyb_t *Param)
{
  /* Highest possible resistance value, */
  /* used for the high value when starting a binary search */
  Param->SetGatePrvResVal = ParamGCR1 + 1000000L;
}

static int SetGateResTooHigh(double Vin, uint32_t ResVal)
{
  /* Return 1 if ResVal is too much resistance for Vin */
  double ITot;   /* Total Current */
  double VRes;   /* Voltage across the resistor */
  double Vd;     /* Voltage across the diode */
  double Id;     /* Current through the diode */

  ITot = Vin / ResVal;
  VRes = ITot * ParamGCR1;
  Vd = Vin - VRes;
  if (Vd <= 0.0) {
    Id = 0.0;
    return (Id > ITot);
  }
  if (Vd > 0.7)
    Id = 1000.0;
  else
    Id = DiodeI(Vd);
  return (Id > ITot);
}


static uint32_t SetGateGetRes27(dolbyb_t *Param, double Vin)
{
  /* Find the actual resistance value of resistor + diode */
  uint32_t HigVal, LowVal, NxtVal;

  /* Set high and low values for binary search */
  HigVal = Param->SetGatePrvResVal;
  LowVal = HigVal - 1000;
  while (SetGateResTooHigh(Vin, LowVal)) {
    HigVal = LowVal;
    LowVal = HigVal - 1000;
  }
  /* Now use binary search to narrow things down */
  while (LowVal + 1 < HigVal) {
    NxtVal = LowVal + (HigVal - LowVal) / 2;
    if (SetGateResTooHigh(Vin, NxtVal))
      HigVal = NxtVal;
    else
      LowVal = NxtVal;
  }

  /* Now try last few values to find end result */
  NxtVal = LowVal + 1;
  while (!(SetGateResTooHigh(Vin, NxtVal) || NxtVal > HigVal)) {
    LowVal = NxtVal;
    NxtVal = LowVal + 1;
  }

  /* Return the result */
  Param->SetGatePrvResVal = LowVal;
  return LowVal;
}


/*********************************************************/
/****   Routines to calculate values for 1st filter   ****/
/*********************************************************/

static double SetGateInRes1(uint32_t InpRes)
{
  /* Calculate total of resistances in parallel */
  double InvRes;

  InvRes = 1.0 / InpRes + 1.0 / ParamGCR1d;
  return (1.0 / InvRes);
}


static double SetGateInAtt1(uint32_t InpRes)
{
  /* Calculate input attenuation for the first filter */
  int64_t ResVal, TotRes;

  ResVal = ParamGCR1d;
  /* Total of both resistances */
  TotRes = InpRes + ResVal;
  /* Output Value */
  return ((double)ResVal / TotRes);
}


static double SetGateInAlp1(dolbyb_t *Param, uint32_t InpRes)
{
  /* Calculate an Alpha value for the first filter */
  double Rv, fc, RC;

  Rv = SetGateInRes1(InpRes);   /* input resistance of filter */
  RC = Rv * ParamGCC1;
  fc = 1 / (2 * M_PI * RC);   /* Frequency of filter */
  RC *= LPFadj(fc / Param->SetGateSmpSec);
  return (Param->SetGatedt / (RC + Param->SetGatedt));
}


/*********************************************************/
/****   Routines to calculate values for 2nd filter   ****/
/*********************************************************/

static double SetGateRes2(double Vin)
{
  /* Calculate resistance in parallel with diode */
  double DI, DR, InvRes;

  DI = DiodeI(Vin);   /* Current through the diode */
  DR = Vin / DI;      /* Resistance of the diode   */
  InvRes = 1.0 / DR + 1.0 / ParamGCR2;
  return 1.0 / InvRes;
}


static double SetGateInAlp2(dolbyb_t *Param, double Vin)
{
  /* Alpha value, depending upon input sample */
  double IR, fc, RC;

  IR = SetGateRes2(Vin);   /* Input resistance of filter */
  RC = IR * ParamGCC2;
  fc = 1 / (2 * M_PI * RC);   /* Frequency of filter */
  /* Limit frequency to keep filter sensible */
  if (fc > 5000.0) {
    RC = 1 / (2 * M_PI * 5000.0);
    fc = 1 / (2 * M_PI * RC);
  }
  RC *= LPFadj(fc / Param->SetGateSmpSec);
  return (Param->SetGatedt / (RC + Param->SetGatedt));
}


/*************************************/
/****   Routines to make tables   ****/
/*************************************/

static void SetGateTab1Pos(dolbyb_t *Param, uint16_t TabPos)
{
  /* Make table values for table 1 (1st filter) */
  double Vin = TabPos / 100.0;
  uint32_t InpRes = SetGateGetRes27(Param, Vin);

  Param->SetGateAttTab[TabPos - SetGateTab1St] =
    round(Param->SetGateAttMux * SetGateInAtt1(InpRes));
  Param->SetGateAlpTab1[TabPos - SetGateTab1St] =
    round(Param->SetGateA1Mux * SetGateInAlp1(Param, InpRes));
}


static void SetGateTab2Pos(dolbyb_t *Param, uint16_t TabPos)
{
  /* Make table values for table 2 (2nd filter) */
  double Vin = TabPos / 1000.0;

  Param->SetGateAlpTab2[TabPos - SetGateTab2St] =
    round(Param->SetGateA2Mux * SetGateInAlp2(Param, Vin));
}


/***************************/
/****   Main Routines   ****/
/***************************/

char *SetGateInit(dolbyb_t *Param)
{
  double Vin = SetGateTab1Ed / 100.0;
  double Att;
  uint16_t TabPos, FltCnt;
  int64_t MaxMux, MaxSmp;
  size_t nmemb = SetGateTab1Ed - SetGateTab1St + 1;

  if (Param->SetGateAttTab == NULL)
    Param->SetGateAttTab  = calloc(nmemb, sizeof(*Param->SetGateAttTab));
  if (Param->SetGateAlpTab1 == NULL)
    Param->SetGateAlpTab1 = calloc(nmemb, sizeof(*Param->SetGateAlpTab1));
  if (Param->SetGateAlpTab2 == NULL)
    Param->SetGateAlpTab2 = calloc(nmemb, sizeof(*Param->SetGateAlpTab2));
  if (Param->SetGateAttTab == NULL ||
      Param->SetGateAlpTab1 == NULL ||
      Param->SetGateAlpTab2 == NULL) return "Out of memory";

  if (Param->AllHig)
    Param->SetGateSmpSec = Param->SmpSec * Param->UpSmp;
  else
    Param->SetGateSmpSec = Param->SmpSec;

  /* Set dt */
  Param->SetGatedt = 1.0 / Param->SetGateSmpSec;

  /* Set multiplier for attenuator */
  SetGateSetPrvRes(Param);
  Att = SetGateInAtt1(1000000L);
  Param->SetGateAttMux = ParamMuxValue(Att, 1000000L);

  /* Set multiplier for Alpha table 1 */
  SetGateSetPrvRes(Param);
  Param->SetGateA1Mux = ParamMuxValue(SetGateInAlp1(Param, 1000000L), ParamAlpTgt);
  /* Check multiplier is not too high */
  MaxMux = round(SetGatesMaxAlpVal / SetGateInAlp1(Param, SetGateGetRes27(Param, Vin)));
  if (Param->SetGateA1Mux > MaxMux)
    Param->SetGateA1Mux = MaxMux;

  /* Set multiplier for Alpha table 2 */
  Vin = SetGateTab2St / 1000.0;
  Param->SetGateA2Mux = ParamMuxValue(SetGateInAlp2(Param, Vin), ParamAlpTgt);
  /* Check multiplier is not too high */
  Vin = SetGateTab2Ed / 1000.0;
  MaxMux = round(SetGatesMaxAlpVal / SetGateInAlp2(Param, Vin));
  if (Param->SetGateA2Mux > MaxMux)
    Param->SetGateA2Mux = MaxMux;

  /* Make tables */
  SetGateSetPrvRes(Param);
  /* 1st table */
  for (TabPos = SetGateTab1St; TabPos <= SetGateTab1Ed; TabPos++)
    SetGateTab1Pos(Param, TabPos);
  /* 2nd table */
  for (TabPos = SetGateTab2St; TabPos <= SetGateTab2Ed; TabPos++)
    SetGateTab2Pos(Param, TabPos);

  /* Set previous values to 0 */
  for (FltCnt = 0; FltCnt <= SetGateNumberOfFilters - 1; FltCnt++) {
    Param->SetGatePvrSmp1[FltCnt] = 0;
    Param->SetGatePvrSmp2[FltCnt] = 0;
  }

  /* Set maximum and minimum sample values */
  Param->SetGateMaxSmp = SetGateMax64 / Param->SetGateAttMux;
  MaxSmp = SetGateMax64 / Param->SetGateA1Mux;
  if (Param->SetGateMaxSmp > MaxSmp)
    Param->SetGateMaxSmp = MaxSmp;
  MaxSmp = SetGateMax64 / Param->SetGateA2Mux;
  if (Param->SetGateMaxSmp > MaxSmp)
    Param->SetGateMaxSmp = MaxSmp;
  Param->SetGateMaxSmp /= 4;
  Param->SetGateMinSmp = -Param->SetGateMaxSmp;

  return NULL;
}

void SetGate(dolbyb_t *Param, int64_t InSamp, uint16_t TabNum)
{
  int64_t SmpDif, TabPos, SmpVal, PrvSmp, SmpMux, DifVal;

  /* Check range of input sample */
  SmpVal = InSamp;
  if (SmpVal > Param->SetGateMaxSmp) SmpVal = Param->SetGateMaxSmp;
  if (SmpVal < Param->SetGateMinSmp) SmpVal = Param->SetGateMinSmp;

  /* Table position for 1st set of tables */
  SmpDif = SmpVal - Param->SetGatePvrSmp1[TabNum-1];
  TabPos = SmpDif / SetGateTabPosDiv1;
  if (TabPos < SetGateTab1St) TabPos = SetGateTab1St;
  if (TabPos > SetGateTab1Ed) TabPos = SetGateTab1Ed;

  /* Attenuate sample */
  SmpMux = SmpVal * Param->SetGateAttTab[TabPos - SetGateTab1St];
  SmpVal = SmpMux / Param->SetGateAttMux;

  /* 1st low pass filter */
  DifVal = SmpVal - Param->SetGatePvrSmp1[TabNum-1];
  SmpMux = DifVal * Param->SetGateAlpTab1[TabPos - SetGateTab1St];
  DifVal = SmpMux / Param->SetGateA1Mux;
  SmpVal = Param->SetGatePvrSmp1[TabNum-1] + DifVal;
  Param->SetGatePvrSmp1[TabNum-1] = SmpVal;

  /* Table position for 2nd Alpha table */
  SmpDif = SmpVal - Param->SetGatePvrSmp2[TabNum-1];
  TabPos = SmpDif / SetGateTabPosDiv2;
  if (TabPos < SetGateTab2St) TabPos = SetGateTab2St;
  if (TabPos > SetGateTab2Ed) TabPos = SetGateTab2Ed;

  /* 2nd low pass filter */
  PrvSmp = SmpVal - SetGateDioVlt;
  DifVal = SmpVal - Param->SetGatePvrSmp2[TabNum-1];
  SmpMux = DifVal * Param->SetGateAlpTab2[TabPos - SetGateTab2St];
  DifVal = SmpMux / Param->SetGateA2Mux;
  SmpVal = Param->SetGatePvrSmp2[TabNum-1] + DifVal;
  if (SmpVal < PrvSmp) SmpVal = PrvSmp;
  Param->SetGatePvrSmp2[TabNum-1] = SmpVal;

  /* Set output value */
  Param->SetGateOut[TabNum-1] = SmpVal;
}
