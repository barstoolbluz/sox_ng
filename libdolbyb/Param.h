/* Param.h */

#ifndef PARAM_H
# define PARAM_H 1

#include <math.h>  /* for pow() */

#define ParamMaxChnl    2   /* Maximum number of channels allowed */

/* Advanced parameters */
/* gcc-2.95 warns "integer constant out of range" and Ansi C disallows
 * long long constants (1000000000LL) so here's a halfway house */
#define ParamCapMux ((int64_t)100000*100000)
       /* Values of capacitors are small fractions of a farad.
        * The easiest way to represent them is by multiplying them up
        * so they can be defined as integer values. */
#define ParamVltMux ((int64_t)10000*100000)
        /* Voltages are represented by integer values. However since
         * the program needs to represent voltages to a fraction of
         * a volt, voltages are multiplied up by VltMux to convert
         * everything to nano volts, so the value is 1000000000. */
#define ParamAlpTgt (int64_t)1000000
        /* Digital filters require an alpha value that is a fraction.
         * The program multiplies these values up so it can use integer
         * arithmetic, as this is faster than dealing with Real values.
         * AlpTgt decides how much they are multiplied up, by defining
         * a target value; the program chooses a multiplier so that
         * Alpha ends up close to AlpTgt, calculated for the highest
         * frequency, hence the lowest alpha value. */
#define ParamSMux24 (int64_t)238
#define ParamSMux16 (int64_t)61035
#define ParamSMux08 (int64_t)15624980
        /* Sample values are multiplied up to reduce rounding errors.
         * These values convert sample values to nano volts.
         * This is not exact, but it's not critical to get the exact values.
         * There a 3 values depending on whether the input file has
         * a bit depth of 24, 16 or 8. */
#define ParamFETSVt1 (45 * (int64_t)256153657) /* 11526914565 */
#define ParamFETSVt2 (5 * (int64_t)2291199481U) /* 11455997405 */
#define ParamFETSVt3 (15 * (int64_t)768460969) /* 11526914535 */
#define ParamFETSVt4 (13 * (int64_t)883905461) /* 11490770993 */
#define ParamDioClp 0.6;
#define ParamSidAmp1 (double)3.3394756317
#define ParamSidAmp2 (double)3.3268103599
#define ParamSidAmp3 (double)3.3394756317
#define ParamSidAmp4 (double)2.4765062332
#define ParamGanAmp  10.0 /* Amplification of the signal when it goes into
                           * the gain control circuit. */
#define ParamMxFqRt  0.2

/* 2 parameters that affect the way the FET opperates. */
#define ParamIDSS    6.0
#define ParamVgsOff  (-4.0)

/* Component values */
#define ParamLR    ((int64_t)10000)

/* Resistor and capacitor for the fixed high pass filter in the side path. */
#define ParamR1    ((int64_t)3300)
#define ParamC1    ((double)326 / ParamCapMux)

/* Resistor and capacitor used in the Dolby sliding filter.
 * This filter also depends on the current resistance of the FET. */
#define ParamR2    ((int64_t)47000)
#define ParamC2    ((double)47 / ParamCapMux)

/* Resistors and capacitors used in the gain control part of the circuit. */
#define ParamGCR1  ((int64_t)2700)
#define ParamGCR1d ((int64_t)270000)
#define ParamGCC1  ((double)1000 / ParamCapMux)
#define ParamGCR2  ((int64_t)270000)
#define ParamGCC2  ((double)3300 / ParamCapMux)

#define ParamLC    ((double)1000000 / ParamCapMux)

#define ParamConvertDb(Db) pow(10, Db / 20)

/* Set a multiplier value so that RelVal * multiplier */
/* is as close as possible to TrgVal */
#define ParamMuxValue(RelVal, TrgVal) round(TrgVal / RelVal)

#if DEBUG
#include "dolbyb.h"
extern void dolbyb_dumpparam(dolbyb_t *Param);
#endif

#endif /* PARAM_H */
