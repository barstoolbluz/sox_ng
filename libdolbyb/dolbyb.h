/*
 * dolbyb.h - Declarations for callers of libdolbyb
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

#ifndef DOLBYB_H
# define DOLBYB_H 1

#include <stdint.h>
#include <stddef.h>

typedef struct {
  /* Compulsory Parameters */
  uint32_t SmpSec; /* Samples per second in the input and output files */
  int16_t BDepth;  /* Size of samples in the input and output files: 8, 16 or 24 */
  int16_t NumChn;  /* Number of channels in the input and output files */

  /* Adjustable Parameters */
  int16_t UpSamp;  /* Upsampling to use when generating the highpass filter's
                    * alpha table. See also UpSmp below. */
  int16_t FltTyp;  /* Filter type to use; 1, 2, 3 or 4 (the default) */
  int     AllHig;  /* (Bool) Also upsample the Gate, DC and lowpass filters */
  double  DecAdB;  /* Decoding accuracy in decibels, default -5.0 */
  double  ThGndB;  /* Threshold level adjustmentin decibel, default 0.0 */

  /* Internal Parameters */
  int16_t UpSmp;   /* The actual value we use, since UpSamp=0 means automatic */
  double  DecAMX;  /* Internal version of the Decoding Accuracy */
  double  ThGain;  /* Adjust the threshold to the level the tape was digitized at (1.0) */
  int16_t InUS;    /* How many times to upsample the input samples */
  int     FETClp;  /* If set to TRUE, the voltage on the Gate pin of the FET
                    * is kept at FETGVt. This is used during calibration
                    * which involves clamping the gate to ground. */
  int64_t FETGVt;  /* Under normal operation, the voltage on the gate would be
                    * adjusted by the gain control circuit, so this would be
                    * the voltage when there is no audio. If the gate
                    * is clamped, this is the value it's clamped at. */
  int64_t FETSVt;  /* This is the voltage on the Source pin of the FET
                    * for filtering types 1 & 2. It is set during calibration
                    * and decides the point at which the sliding filter
                    * starts to slide. */
  int64_t SmpMux;
  double  SidAmp;  /* The audio is amplified when it goes into the side path.
                    * This determines how much it is amplified for filtering
                    * types 1 & 2 and is set during the calibration process. */

  /* DiodeClip variables */
                   /* The final part of the side path uses 2 diodes to get rid
                    * of overshoots by clipping the output for a moment
                    * until the gain can be reduced. This sets the voltage
                    * at which the diodes will start to clip. */
  int64_t DiodeClipMinVal;
  int64_t DiodeClipMaxVal;
  int DiodeClipCliped;  /* Did the diodes clip? */

  /* DCfilter variables */
  /* The arrays [0,1] are [left, right] for the first channel, etc. */
  /* Alpha  */
  int64_t DCfilterAlpMux;   /* Multiple used to multiply up alpha */
  int64_t DCfilterAlp;      /* Value of alpha                     */
  /* Previous values (for filters) */
  int64_t DCfilterPrvVal[4];
  /* Previous accumulated values (for filters) */
  int64_t DCfilterPrvAccVal[4];

  /* SetGate variables */
  int64_t SetGateAttMux;
  int64_t SetGateA1Mux;
  int64_t SetGateA2Mux;
  double SetGatedt;
  int64_t *SetGateAttTab;  /* [SetGateTab1Ed - SetGateTab1St + 1] */
  int64_t *SetGateAlpTab1; /* [SetGateTab1Ed - SetGateTab1St + 1] */
  int64_t *SetGateAlpTab2; /* [SetGateTab2Ed - SetGateTab2St + 1] */
  int64_t SetGatePvrSmp1[2];
  int64_t SetGatePvrSmp2[2];
  /* Resulting Gate voltage */
  int64_t SetGateOut[2];
  uint32_t SetGatePrvResVal;
  uint32_t SetGateSmpSec;
  int64_t SetGateMinSmp;
  int64_t SetGateMaxSmp;

  /* FindOutSmp variables */
  int64_t FindOutSmpPrvIn[2];
  int64_t FindOutSmpPrvOt[2];
  int64_t FindOutSmpMaxDif;

  /* SidePath variables */
  int64_t SidePathGainMux;  /* Gain for audio entering side path */
  int64_t SidePathGainVal;  /* The audio is amplified when it goes into
                             * the gain control circuit; this value is
                             * adjusted during the calibration process. */
  int64_t SidePathGcMux;
  int64_t SidePathGcVal;   /* Gain for audio entering gain control */

  /* Calibrate variables */
  int32_t CalibrateWrmSam;
  int32_t CalibrateEndSam;
  double CalibrateSmpDiv;  /* SmpCyc divided by 2 pi */
  double CalibrateSmpMux;  /* Multiply result of Sin to get sample value */
  int64_t *CalibrateSinTab;
  uint32_t CalibrateSinTabMax;

  /* HPF1 variables */
  int64_t HPF1Alp;      /* Alpha value */
  int64_t HPF1AlpMux;   /* Alpha multiplier */
  /* Previous values */
  int64_t HPF1PrvIn[2];
  int64_t HPF1PrvOt[2];
  /* Previous values if check method is used */
  /* as the values might be used or discarded */
  /* depending on whether check ends up being used */
  int64_t HPF1PrvInVal[2];
  int64_t HPF1PrvOtVal[2];

  /* HPF2 variables */
  uint16_t HPF2UpSmp;
  /* Previous values */
  int64_t HPF2PrvIn[2];
  int64_t HPF2PrvOt[2];
  /* Remember previous values if we are running a check */
  /* as we don't yet know if these values will become permanent */
  int64_t HPF2PrvInVal[2];
  int64_t HPF2PrvOtVal[2];
  int64_t HPF2PrvOtTot[2];

  /* HPF2SetVals variables */
  /* Attenuation of POT before high pass filter */
  int64_t HPF2SetValsPot[2];
  /* Alpha value for high pass filter */
  int64_t HPF2SetValsAlp[2];
  /* Extra attenuation after high pass filter */
  int64_t HPF2SetValsFPt[2];
  /* Table of attenuation values */
  int64_t *HPF2SetValsPotTab; /* [HPF2SetValsTableSize + 1]; */
  int64_t HPF2SetValsPotMux;
  int64_t HPF2SetValsAlpMux;
  /* Table of alpha values */
  int64_t *HPF2SetValsAlpTab; /* [HPF2SetValsTableSize + 1]; */
  /* Table of attenuation values for output of filter */
  int64_t *HPF2SetValsFLTATb; /* [HPF2SetValsTableSize + 1]; */
  /* Other Variables */
  int64_t HPF2SetValsMaxVlt;   /* Maximum input voltage */
  int64_t HPF2SetValsTabRes;   /* Voltage units per table position */
  /* Limit frequency of sliding filter */
  /* Limit highest frequency by setting minimum value for RC */
  double HPF2SetValsLowRC;   /* Lowest RC that the program might want */
  double HPF2SetValsMinRC;   /* To Limit maximum frequency */
  double HPF2SetValsRCDif;   /* Differenc between Low and min RC */
  /* Limit lowest frequency by setting maximum value for RC */
  double HPF2SetValsMaxRC;   /* Highest RC if using filter method 2 */

} dolbyb_t;

extern void dolbyb_init(dolbyb_t *);  /* Call before doing anything */
extern char *dolbyb_start(dolbyb_t *); /* Call when BDepth, NumChn and SmpSec are set */
extern char *dolbyb_encode(dolbyb_t *, void *in, void *out, size_t nframes);
extern char *dolbyb_decode(dolbyb_t *, void *in, void *out, size_t nframes);
extern void dolbyb_free(dolbyb_t *);

#endif
