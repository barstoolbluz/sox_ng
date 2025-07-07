/*
 * libdolbyb.c - API to libdolbyb
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
#include "Mixers.h"
#include "Calibrate.h"
#include "SidePath.h"
#include "FindOutSmp.h"

#include "../src/soxconfig.h"  /* for WORDS_BIGENDIAN */

#include <stdlib.h>  /* for free() */
#include <string.h>  /* for memset() */

/* Set default values */
void dolbyb_init(dolbyb_t *Param)
{
  memset((void *)Param, 0, sizeof(*Param));

  /* Set default values */
  Param->NumChn = 1;
  Param->BDepth = 16;
  Param->FltTyp = 4;
  Param->DecAdB = -5.0;
  Param->ThGndB = 0.0;

  /* Set initial values */
  Param->FETGVt = 75000*(int64_t)100000;
}

static char *SecondInit(dolbyb_t *Param)
{
  char *err;

  /* More initialising after the input wave header has been read,  */
  /* so the sample rate and number of samples is now known.        */

  Param->DecAMX = ParamConvertDb(Param->DecAdB);
  Param->ThGain = ParamConvertDb(Param->ThGndB);

  /* Set up sample rates */
  if (Param->UpSamp < 0) return "Upsampling cannot be negative";
  if (Param->UpSamp > 0)
    Param->UpSmp = Param->UpSamp;
  else {
    Param->UpSmp = 1;
    while (Param->UpSmp * Param->SmpSec < 200000L)
      Param->UpSmp++;
  }
  if (Param->AllHig)
    Param->InUS = Param->UpSmp;
  else
    Param->InUS = 1;

  /* Initialise side path routines */
  if ((err = SidePathInit(Param))) return err;
  FindOutSmpInit(Param);
  return NULL;
}

/* Do initialisation that depends on parameter changes */
char *dolbyb_start(dolbyb_t *Param)
{
  char *err;

  /* Check validity of parameters */

  if (Param->SmpSec <= 0)
    return "Did you forget to set dolbyb.SmpSec before calling dolbyb_start()?";

  if (Param->NumChn < 1 || Param->NumChn > 2)
    return "libdolbyb can only process mono and stereo audio";

  /* Check filter type is valid */
  if (Param->FltTyp < 1 || Param->FltTyp > 4)
    return "dolbyb.FltTyp must be from 1 to 4";

  switch (Param->FltTyp) {
  case 1: Param->FETSVt = ParamFETSVt1; Param->SidAmp = ParamSidAmp1; break;
  case 2: Param->FETSVt = ParamFETSVt2; Param->SidAmp = ParamSidAmp2; break;
  case 3: Param->FETSVt = ParamFETSVt3; Param->SidAmp = ParamSidAmp3; break;
  case 4: Param->FETSVt = ParamFETSVt4; Param->SidAmp = ParamSidAmp4; break;
  }

  /* Set multiplier for wave input and divider for wave output */
  switch (Param->BDepth) {
  case 8:  Param->SmpMux = ParamSMux08; break;
  case 16: Param->SmpMux = ParamSMux16; break;
  case 24: Param->SmpMux = ParamSMux24; break;
  default:
    return "dolbyb.BDepth must be 8, 16 or 24";
  }

  if ((err = SecondInit(Param))) return err;

  Calibrate(Param);

  return NULL;
}

char *dolbyb_encode(dolbyb_t *Param, void *in, void *out, size_t nframes)
{
  unsigned char *inp = in;
  unsigned char *outp = out;
  uint16_t Chn, UpCnt;
  int64_t SidSmp, TotSmp, OutSmp;
  size_t SmpCnt;
  int64_t MaxSamp, SubSamp;
  uint16_t NumByt;
  int32_t MaxVal, MinVal, AddVal;

  switch (Param->BDepth) {
  case 8:  NumByt = 1; MaxSamp = -1; SubSamp = 128; 
           MaxVal = 127; MinVal = -128; AddVal = 128;
           break;
  case 16: NumByt = 2; MaxSamp = 32767; SubSamp = 65536L;
           MaxVal = 32767; MinVal = -32768L; AddVal = 65536L;
           break;
  case 24: NumByt = 3; MaxSamp = 8388607L; SubSamp = 16777216L;
           MaxVal = 8388607L; MinVal = -8388608L; AddVal = 16777216L;
           break;
  default:
	   /* Shut the compiler warnings up */
           NumByt = MaxSamp = SubSamp = 0;
           MaxVal = MinVal = AddVal = 0;
           return "BDepth is not 8/16/24. Did you set it before calling dolbyb_start()?";
  }

  for (SmpCnt = 0; SmpCnt < nframes; SmpCnt++) {
    for (Chn = 1; Chn <= Param->NumChn; Chn++) {
      int64_t SmpVal;

      /* Get input */
      switch (NumByt) {
      case 1: SmpVal = (int64_t)(inp[0]);
              break;
#ifndef WORDS_BIGENDIAN
      case 2: SmpVal = (int64_t)((int32_t)inp[0] | ((int32_t)inp[1] << 8));
              break;
      case 3: SmpVal = (int64_t)((int32_t)inp[0] | ((int32_t)inp[1] << 8) | ((int32_t)inp[2] << 16));
#else
      case 2: SmpVal = (int64_t)((int32_t)inp[1] | ((int32_t)inp[0] << 8));
              break;
      case 3: SmpVal = (int64_t)((int32_t)inp[2] | ((int32_t)inp[1] << 8) | ((int32_t)inp[0] << 16));
#endif
              break;
      }
      inp += NumByt;
      if (SmpVal > MaxSamp) SmpVal -= SubSamp;
      SmpVal *= Param->SmpMux;

      /* If upsampling at the start then the same sample */
      /* is processed several times */
      TotSmp = 0;
      for (UpCnt = 1; UpCnt <= Param->InUS; UpCnt++) {
	/* Add audio from side path */
	SidSmp = SidePath(Param, SmpVal, Chn);
	TotSmp += MixersEncode(SmpVal, SidSmp);
      }
      OutSmp = (TotSmp / Param->InUS) / Param->SmpMux;

      /* Check for sample value out of range */
      if (OutSmp > MaxVal)
	SmpVal = MaxVal;
      else if (OutSmp < MinVal)
	SmpVal = MinVal;
      else
	SmpVal = OutSmp;
      /* Convert negative values */
      if (SmpVal < 0 || Param->BDepth == 8)
	SmpVal += AddVal;

      /* Send as output bytes */

      switch (NumByt) {
      case 1: *outp++ = SmpVal & 0xff;
              break;
#ifndef WORDS_BIGENDIAN
      case 2: *outp++ = SmpVal & 0xff;
              *outp++ = (SmpVal >> 8) & 0xff;
              break;
      case 3: *outp++ = SmpVal & 0xff;
              *outp++ = (SmpVal >> 8) & 0xff;
              *outp++ = (SmpVal >> 16) & 0xff;
#else
      case 2: *outp++ = (SmpVal >> 8) & 0xff;
              *outp++ = SmpVal & 0xff;
              break;
      case 3: *outp++ = (SmpVal >> 16) & 0xff;
              *outp++ = (SmpVal >> 8) & 0xff;
              *outp++ = SmpVal & 0xff;
#endif
              break;
      }
    }
  }
  return NULL;
}

char *dolbyb_decode(dolbyb_t *Param, void *in, void *out, size_t nframes)
{
  unsigned char *inp = in;
  unsigned char *outp = out;
  uint16_t NumChn, UpCnt, NumByt;
  int64_t TotSmp;
  int64_t MaxSamp, SubSamp, MinVal, MaxVal, AddVal;
  size_t SmpCnt;

  switch (Param->BDepth) {
  case 8:  NumByt = 1; MaxSamp = -1; SubSamp = 128; 
           MaxVal = 127; MinVal = -128; AddVal = 128;
           break;
  case 16: NumByt = 2; MaxSamp = 32767; SubSamp = 65536L;
           MaxVal = 32767; MinVal = -32768L; AddVal = 65536L;
           break;
  case 24: NumByt = 3; MaxSamp = 8388607L; SubSamp = 16777216L;
           MaxVal = 8388607L; MinVal = -8388608L; AddVal = 16777216L;
           break;
  default:
	   /* Shut the compiler warnings up */
           NumByt = MaxSamp = SubSamp = 0;
           MaxVal = MinVal = AddVal = 0;
           return "BDepth is not 8/16/24. Did you set it before calling dolbyb_start()?";
  }

  /* Process samples */
  for (SmpCnt = 0; SmpCnt < nframes; SmpCnt++) {
    for (NumChn = 1; NumChn <= Param->NumChn; NumChn++) {
      int64_t SmpVal, OutSmp;

      /* Get input */
      switch (NumByt) {
      case 1: SmpVal = (int64_t)(inp[0]);
              break;
#ifndef WORDS_BIGENDIAN
      case 2: SmpVal = (int64_t)((int32_t)inp[0] | ((int32_t)inp[1] << 8));
              break;
      case 3: SmpVal = (int64_t)((int32_t)inp[0] | ((int32_t)inp[1] << 8) | ((int32_t)inp[2] << 16));
#else
      case 2: SmpVal = (int64_t)((int32_t)inp[1] | ((int32_t)inp[0] << 8));
              break;
      case 3: SmpVal = (int64_t)((int32_t)inp[2] | ((int32_t)inp[1] << 8) | ((int32_t)inp[0] << 16));
#endif
              break;
      }
      inp += NumByt;
      if (SmpVal > MaxSamp) SmpVal -= SubSamp;
      SmpVal *= Param->SmpMux;

      /* If upsampling at the start then the same sample */
      /* is processed several times */
      TotSmp = 0;
      for (UpCnt = 1; UpCnt <= Param->InUS; UpCnt++) {
	/* Set and process output */
	(void) FindOutSmp(Param, SmpVal, NumChn);
	TotSmp += MixersDecode(SmpVal, SidePathUpdate(Param, NumChn));
      }
      OutSmp = (TotSmp / Param->InUS) / Param->SmpMux;

      /* Check for sample value out of range */
      if (OutSmp > MaxVal)
	SmpVal = MaxVal;
      else if (OutSmp < MinVal)
	SmpVal = MinVal;
      else
	SmpVal = OutSmp;
      /* Convert negative values */
      if (SmpVal < 0 || Param->BDepth == 8)
	SmpVal += AddVal;

      /* Send as output bytes */
      switch (NumByt) {
      case 1: *outp++ = SmpVal & 0xff;
              break;
#ifndef WORDS_BIGENDIAN
      case 2: *outp++ = SmpVal & 0xff;
              *outp++ = (SmpVal >> 8) & 0xff;
              break;
      case 3: *outp++ = SmpVal & 0xff;
              *outp++ = (SmpVal >> 8) & 0xff;
              *outp++ = (SmpVal >> 16) & 0xff;
#else
      case 2: *outp++ = (SmpVal >> 8) & 0xff;
              *outp++ = SmpVal & 0xff;
              break;
      case 3: *outp++ = (SmpVal >> 16) & 0xff;
              *outp++ = (SmpVal >> 8) & 0xff;
              *outp++ = SmpVal & 0xff;
#endif
              break;
      }
    }
  }
  return NULL;
}

void dolbyb_free(dolbyb_t *Param)
{
  if (Param->HPF2SetValsPotTab) {
    free(Param->HPF2SetValsPotTab); Param->HPF2SetValsPotTab = NULL;
  }
  if (Param->HPF2SetValsAlpTab) {
    free(Param->HPF2SetValsAlpTab); Param->HPF2SetValsAlpTab = NULL;
  }
  if (Param->HPF2SetValsFLTATb) {
    free(Param->HPF2SetValsFLTATb); Param->HPF2SetValsFLTATb = NULL;
  }
  if (Param->SetGateAttTab) {
    free(Param->SetGateAttTab); Param->SetGateAttTab = NULL;
  }
  if (Param->SetGateAlpTab1) {
    free(Param->SetGateAlpTab1); Param->SetGateAlpTab1 = NULL;
  }
  if (Param->SetGateAlpTab2) {
    free(Param->SetGateAlpTab2); Param->SetGateAlpTab2 = NULL;
  }
}
