/*
 * Param.c - Routines to handle the parameters
 */

#include "dolbyb.h"

#if DEBUG
#include <stdio.h>
#include "Param.h"
#include "SetGate.h"
#include "HPF2SetVals.h"

void dolbyb_dumpparam(dolbyb_t *Param)
{
  static int count = 0;
#ifdef TABLE
  int i;
#endif

  count++;
  printf("%i UpSamp %d\n",             count, Param->UpSamp);
  printf("%i UpSmp %d\n",              count, Param->UpSmp);
  printf("%i FltTyp %d\n",             count, Param->FltTyp);
  printf("%i AllHig %d\n",             count, Param->AllHig);
  printf("%i DecAMX %.19f\n",          count, Param->DecAMX);
  printf("%i ThGain %.19f\n",          count, Param->ThGain);
  printf("%i InUS %d\n",               count, Param->InUS);
  printf("%i FETClp %d\n",             count, Param->FETClp);
  printf("%i FETGVt %lld\n",           count, Param->FETGVt);
  printf("%i FETSVt %lld\n",           count, Param->FETSVt);
  printf("%i SidAmp %.19f\n",          count, Param->SidAmp);
  printf("%i DiodeClipMinVal %lld\n",  count, Param->DiodeClipMinVal);
  printf("%i DiodeClipMaxVal %lld\n",  count, Param->DiodeClipMaxVal);
  printf("%i DiodeClipCliped %d\n",    count, Param->DiodeClipCliped);
  printf("%i DCfilterAlpMux %lld\n",   count, Param->DCfilterAlpMux);
  printf("%i DCfilterAlp %lld\n",      count, Param->DCfilterAlp);
  printf("%i DCfilterPrvVal %lld %lld %lld %lld\n", count, Param->DCfilterPrvVal[0], Param->DCfilterPrvVal[1], Param->DCfilterPrvVal[2], Param->DCfilterPrvVal[3]);
  printf("%i DCfilterPrvAccVal %lld %lld %lld %lld\n", count, Param->DCfilterPrvAccVal[0], Param->DCfilterPrvAccVal[1], Param->DCfilterPrvAccVal[2], Param->DCfilterPrvAccVal[3]);
  printf("%i SetGateAttMux %lld\n",    count, Param->SetGateAttMux);
  printf("%i SetGateA1Mux %lld\n",     count, Param->SetGateA1Mux);
  printf("%i SetGateA2Mux %lld\n",     count, Param->SetGateA2Mux);
  printf("%i SetGatedt %.19f\n",       count, Param->SetGatedt);
#ifdef TABLES
  for (i=0; i < SetGateTab1Ed - SetGateTab1St + 1; i++)
    printf("%i SetGateAttTab[%d] %lld\n", count, i, Param->SetGateAttTab[i]);
  for (i=0; i < SetGateTab1Ed - SetGateTab1St + 1; i++)
    printf("%i SetGateAlpTab1[%d] %lld\n", count, i, Param->SetGateAlpTab1[i]);
  for (i=0; i < SetGateTab2Ed - SetGateTab2St + 1; i++)
    printf("%i SetGateAlpTab2[%d] %lld\n", count, i, Param->SetGateAlpTab2[i]);
#endif
  printf("%i SetGatePvrSmp1 %lld %lld\n", count, Param->SetGatePvrSmp1[0], Param->SetGatePvrSmp1[1]);
  printf("%i SetGatePvrSmp2 %lld %lld\n", count, Param->SetGatePvrSmp2[0], Param->SetGatePvrSmp2[1]);
  printf("%i SetGateOut %lld %lld\n",  count, Param->SetGateOut[0], Param->SetGateOut[1]);
  printf("%i SetGatePrvResVal %u\n",   count, Param->SetGatePrvResVal);
  printf("%i SetGateSmpSec %u\n",      count, Param->SetGateSmpSec);
  printf("%i SetGateMinSmp %lld\n",    count, Param->SetGateMinSmp);
  printf("%i SetGateMaxSmp %lld\n",    count, Param->SetGateMaxSmp);
  printf("%i FindOutSmpPrvIn %lld %lld\n", count, Param->FindOutSmpPrvIn[0], Param->FindOutSmpPrvIn[1]);
  printf("%i FindOutSmpPrvOt %lld %lld\n", count, Param->FindOutSmpPrvOt[0], Param->FindOutSmpPrvOt[1]);
  printf("%i FindOutSmpMaxDif %lld\n", count, Param->FindOutSmpMaxDif);
  printf("%i SidePathGainMux %lld\n",  count, Param->SidePathGainMux);
  printf("%i SidePathGainVal %lld\n",  count, Param->SidePathGainVal);
  printf("%i SidePathGcMux %lld\n",    count, Param->SidePathGcMux);
  printf("%i SidePathGcVal %lld\n",    count, Param->SidePathGcVal);
  printf("%i CalibrateWrmSam %lld\n",  count, Param->CalibrateWrmSam);
  printf("%i CalibrateEndSam %lld\n",  count, Param->CalibrateEndSam);
  printf("%i CalibrateSmpDiv %.19f\n", count, Param->CalibrateSmpDiv);
  printf("%i CalibrateSmpMux %.19f\n", count, Param->CalibrateSmpMux);
  printf("%i HPF1Alp %lld\n",          count, Param->HPF1Alp);
  printf("%i HPF1AlpMux %lld\n",       count, Param->HPF1AlpMux);
  printf("%i HPF1PrvIn %lld %lld\n",   count, Param->HPF1PrvIn[0], Param->HPF1PrvIn[1]);
  printf("%i HPF1PrvOt %lld %lld\n",   count, Param->HPF1PrvOt[0], Param->HPF1PrvOt[1]);
  printf("%i HPF1PrvInVal %lld %lld\n",count, Param->HPF1PrvInVal[0], Param->HPF1PrvInVal[1]);
  printf("%i HPF1PrvOtVal %lld %lld\n",count, Param->HPF1PrvOtVal[0], Param->HPF1PrvOtVal[1]);
  printf("%i HPF2UpSmp %u\n", count, Param->HPF2UpSmp);
  printf("%i HPF2PrvIn %lld %lld\n",   count, Param->HPF2PrvIn[0], Param->HPF2PrvIn[1]);
  printf("%i HPF2PrvOt %lld %lld\n",   count, Param->HPF2PrvOt[0], Param->HPF2PrvOt[1]);
  printf("%i HPF2PrvInVal %lld %lld\n",count, Param->HPF2PrvInVal[0], Param->HPF2PrvInVal[1]);
  printf("%i HPF2PrvOtVal %lld %lld\n",count, Param->HPF2PrvOtVal[0], Param->HPF2PrvOtVal[1]);
  printf("%i HPF2PrvOtTot %lld %lld\n",count, Param->HPF2PrvOtTot[0], Param->HPF2PrvOtTot[1]);
  printf("%i HPF2SetValsPot %lld %lld\n", count, Param->HPF2SetValsPot[0], Param->HPF2SetValsPot[1]);
  printf("%i HPF2SetValsAlp %lld %lld\n", count, Param->HPF2SetValsAlp[0], Param->HPF2SetValsAlp[1]);
  printf("%i HPF2SetValsFPt %lld %lld\n", count, Param->HPF2SetValsFPt[0], Param->HPF2SetValsFPt[1]);
#ifdef TABLES
  for (i=0; i < HPF2SetValsTableSize + 1; i++)
    printf("%i HPF2SetValsPotTab[%d] %lld\n", count, i, Param->HPF2SetValsPotTab[i]);
#endif
  printf("%i HPF2SetValsPotMux %lld\n", count, Param->HPF2SetValsPotMux);
  printf("%i HPF2SetValsAlpMux %lld\n", count, Param->HPF2SetValsAlpMux);
#ifdef TABLES
  for (i=0; i < HPF2SetValsTableSize + 1; i++)
    printf("%i HPF2SetValsAlpTab[%d] %lld\n", count, i, Param->HPF2SetValsAlpTab[i]);
  for (i=0; i < HPF2SetValsTableSize + 1; i++)
    printf("%i HPF2SetValsFLTATb[%d] %lld\n", count, i, Param->HPF2SetValsFLTATb[i]);
#endif
  printf("%i HPF2SetValsMaxVlt %lld\n", count, Param->HPF2SetValsMaxVlt);
  printf("%i HPF2SetValsTabRes %lld\n", count, Param->HPF2SetValsTabRes);
  printf("%i HPF2SetValsLowRC %.19f\n", count, Param->HPF2SetValsLowRC);
  printf("%i HPF2SetValsMinRC %.19f\n", count, Param->HPF2SetValsMinRC);
  printf("%i HPF2SetValsRCDif %.19f\n", count, Param->HPF2SetValsRCDif);
  printf("%i HPF2SetValsMaxRC %.19f\n", count, Param->HPF2SetValsMaxRC);
  fflush(stdout);
}

#else
/* Shut a compiler warning up */
void dolbyb_dumpparam(void) { }
#endif
