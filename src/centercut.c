/*
 * Center Cut 1.4.0
 * Copyright (C) 2006-2007  Moitah (moitah@yahoo.com)
 *
 * Translated from C# to C and adapted to SoX
 * by Martin Guy <martinwguy@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "sox_i.h"

static const double twopi = 6.283185307179586476925286766559;
static const double invsqrt2 = 0.70710678118654752440084436210485;
static const double ccscale = 32768.0;
static const double ccscaleinv = 1.0 / 32768.0;
static const double nodivbyzero = 0.000000000000001;

#define uint sox_uint32_t

typedef struct centercut {
  /* Optional parameters */
  sox_bool bassToSides;

  /* The original code's state veriables and data */
  int kWindowSize, kHalfWindow;
  int kOverlapSize;     /* Output size */
  int kOverlapCount;    /* is 4 and is never modified */
  int kPostWindowPower;
  int mSampleRate;
  int mInputSamplesNeeded;
  int mInputPos;        /* Input buffer position in sample frames */
  int mOutputDiscardBlocks, mFlushSamplesRemaining;
  double mAmpFactor;
  sox_bool mFlushing, mBassToSides;
  uint *mBitRev;
  double *mPreWindow, *mPostWindow, *mSineTab;
  double *mTempLBuffer, *mTempRBuffer, *mTempCBuffer;
  double *mInput;             /* Input buffer, interleaved stereo */
  double *mOutputCenter;      /* Output buffer center */
  double *mOutputLeft;        /* Output buffer left channel */
  double *mOutputRight;       /* Output buffer right channel */
  double *mOverlapC;          /* 3 arrays of kOverlapSize samples */

  /* Our own extra variables */
  int mOutputPos;      /* From where in mOutput* have we still got
                        * output samples that we haven't returned yet? */
  int mOutputCount;    /* How many samples are left in mOutput*[]
                        * still waiting to be returned? */

} priv_t;

/* This is easier than substituting them all, which adds noise */
#define kWindowSize p->kWindowSize
#define kHalfWindow p->kHalfWindow
#define kOverlapSize p->kOverlapSize
#define kOverlapCount p->kOverlapCount
#define kPostWindowPower p->kPostWindowPower
#define mSampleRate p->mSampleRate
#define mInputSamplesNeeded p->mInputSamplesNeeded
#define mInputPos p->mInputPos
#define mOutputDiscardBlocks p->mOutputDiscardBlocks
#define mFlushSamplesRemaining p->mFlushSamplesRemaining
#define mAmpFactor p->mAmpFactor
#define mFlushing p->mFlushing
#define mBassToSides p->mBassToSides
#define mBitRev p->mBitRev
#define mPreWindow p->mPreWindow
#define mPostWindow p->mPostWindow
#define mSineTab p->mSineTab
#define mTempLBuffer p->mTempLBuffer
#define mTempRBuffer p->mTempRBuffer
#define mTempCBuffer p->mTempCBuffer
#define mInput p->mInput
#define mOutputCenter p->mOutputCenter
#define mOutputLeft p->mOutputLeft
#define mOutputRight p->mOutputRight
#define mOverlapC p->mOverlapC
#define mOutputPos p->mOutputPos
#define mOutputCount p->mOutputCount

/* Top level interface (API) */
static sox_bool CenterCut_ProcessSamples(sox_effect_t *effp, int sampleCount);
static void CenterCut_Flush(sox_effect_t *effp);
static void CenterCut_Init(sox_effect_t *effp);
static void CenterCut_Run(sox_effect_t *effp);

/* Forward declarations */
static uint IntegerLog2(uint v);
static uint RevBits(uint x, uint bits);
static double *CreatePostWindow(int windowSize, int power);
static double *VDCreateRaisedCosineWindow(int n, double power);
static double *VDCreateHalfSineTable(int n);
static uint *VDCreateBitRevTable(int n);
static void VDComputeFHT(double *A, int nPoints, double *sinTab);

static sox_bool CenterCut_ProcessSamples(sox_effect_t *effp, int sampleCount) {
  priv_t *p = (priv_t *)effp->priv;

  mInputPos = (mInputPos + sampleCount) % kWindowSize;
  mInputSamplesNeeded -= sampleCount;

  if (mInputSamplesNeeded == 0) {
    CenterCut_Run(effp);
    if (mOutputDiscardBlocks == 0) {
      return sox_true;
    } else {
      mOutputDiscardBlocks--;
    }
  }

  return sox_false;
}

static int CenterCut_FlushWithReturn(sox_effect_t *effp) {
  priv_t *p = (priv_t *)effp->priv;
  int returnCount;

  if (!mFlushing) {
    mFlushing = sox_true;
    mFlushSamplesRemaining = kWindowSize - mInputSamplesNeeded;
  } else if (mFlushSamplesRemaining <= 0) {
    CenterCut_Flush(effp);
    return SOX_EOF;
  }

  /* Was:
   * Array.Copy(zeroBuff, 0, mInput, mInputPos * 2, mInputSamplesNeeded * 2)
   *            from, offset, to,    offset,        length
   */
  memset(mInput + mInputPos * 2, 0, mInputSamplesNeeded * 2 * sizeof(*mInput));

  if (CenterCut_ProcessSamples(effp, mInputSamplesNeeded)) {
    returnCount = min(mFlushSamplesRemaining, kOverlapSize);
  } else {
    returnCount = 0;
  }
  mFlushSamplesRemaining -= mInputSamplesNeeded;

  return returnCount;
}

static void CenterCut_Flush(sox_effect_t *effp) {
  priv_t *p = (priv_t *)effp->priv;

  mFlushing = sox_false;
  mInputPos = 0;
  mInputSamplesNeeded = kOverlapSize;
  mOutputDiscardBlocks = kOverlapCount - 1;
}

static void CenterCut_Init(sox_effect_t *effp) {
  priv_t *p = (priv_t *)effp->priv;
  double *window;
  double scale;
  int i;

  mFlushing = sox_false;

  kHalfWindow = kWindowSize / 2;
  kOverlapSize = kWindowSize / kOverlapCount;

  mBitRev = VDCreateBitRevTable(kWindowSize);
  mSineTab = VDCreateHalfSineTable(kWindowSize);

  mInputPos = 0;
  mInputSamplesNeeded = kOverlapSize;
  mOutputDiscardBlocks = kOverlapCount - 1;

  lsx_valloc(mInput,       kWindowSize * 2);
  lsx_valloc(mTempLBuffer, kWindowSize);
  lsx_valloc(mTempRBuffer, kWindowSize);
  lsx_valloc(mTempCBuffer, kWindowSize);
  lsx_valloc(mPreWindow,   kWindowSize);
  lsx_valloc(mOverlapC,     kOverlapSize * (kOverlapCount - 1));
  lsx_valloc(mOutputCenter, kOverlapSize);
  lsx_valloc(mOutputLeft,   kOverlapSize);
  lsx_valloc(mOutputRight,  kOverlapSize);

  window = VDCreateRaisedCosineWindow(kWindowSize, 1.0);

  scale = (1.0 / (double)kWindowSize) * (2.0 / (double)kOverlapCount) * 0.5 * ccscale;
  for (i = 0; i < kWindowSize; i++) {
    /* The correct Hartley<->FFT conversion is:
     *
     *  Fr(i) = 0.5(Hr(i) + Hi(i))
     *  Fi(i) = 0.5(Hr(i) - Hi(i))
     *
     * We omit the 0.5 in both the forward and reverse directions,
     * so we have a 0.25 to put here.  On the other hand, we are
     * using a raised cosine window with 1/4 step instead of a 1/2
     * step, which gives us a 2.0 factor.  So we only need 0.5.
     */
    mPreWindow[i] = window[mBitRev[i]] * scale;
  }

  free(window);

  mPostWindow = CreatePostWindow(kWindowSize, kPostWindowPower);
}

static void CenterCut_Quit(sox_effect_t *effp) {
  priv_t *p = (priv_t *)effp->priv;

  free(mInput);
  free(mTempLBuffer);
  free(mTempRBuffer);
  free(mTempCBuffer);
  free(mPreWindow);
  free(mOverlapC);
  free(mOutputCenter);
  free(mOutputLeft);
  free(mOutputRight);

  free(mPostWindow);
  free(mSineTab);
  free(mBitRev);
}

/* CenterCut_Run() reads samples in the range from mInput[0..kWindowSize-1]
 * and writes kOverlapSize samples into
 * mOutput{Center,Left,Right}[0..kOverlapSize-1]
 */
static void CenterCut_Run(sox_effect_t *effp) {
  priv_t *p = (priv_t *)effp->priv;
  int i;
  int freqBelowToSides = (int)(200.0 / ((double)mSampleRate / kWindowSize));

  /* copy to temporary buffer and FHT */

  for (i = 0; i < kWindowSize; i++) {
    uint j = mBitRev[i];
    uint k = (j + (uint)mInputPos) % (uint)kWindowSize;
    double w = mPreWindow[i];

    mTempLBuffer[i] = mInput[k*2+0] * w;
    mTempRBuffer[i] = mInput[k*2+1] * w;
  }

  VDComputeFHT(mTempLBuffer, kWindowSize, mSineTab);
  VDComputeFHT(mTempRBuffer, kWindowSize, mSineTab);

  /* perform stereo separation */

  mTempCBuffer[0] = 0;
  mTempCBuffer[1] = 0;
  for (i = 1; i < kHalfWindow; i++) {
    double lR = mTempLBuffer[i] + mTempLBuffer[kWindowSize - i];
    double lI = mTempLBuffer[i] - mTempLBuffer[kWindowSize - i];
    double rR = mTempRBuffer[i] + mTempRBuffer[kWindowSize - i];
    double rI = mTempRBuffer[i] - mTempRBuffer[kWindowSize - i];

    double sumR = lR + rR;
    double sumI = lI + rI;
    double diffR = lR - rR;
    double diffI = lI - rI;

    double sumSq = sumR * sumR + sumI * sumI;
    double diffSq = diffR * diffR + diffI * diffI;
    double alpha = 0.0;

    double cR, cI;

    if (sumSq > nodivbyzero) {
      alpha = 0.5 - sqrt(diffSq / sumSq) * 0.5;
    }

    cR = sumR * alpha;
    cI = sumI * alpha;

    if (mBassToSides && (i < freqBelowToSides)) {
      cR = cI = 0.0;
    }

    mTempCBuffer[mBitRev[i            ]] = cR + cI;
    mTempCBuffer[mBitRev[kWindowSize-i]] = cR - cI;
  }

  /* reconstitute left/right/center channels */

  VDComputeFHT(mTempCBuffer, kWindowSize, mSineTab);

  /* apply post-window */

  for (i = 0; i < kWindowSize; i++) {
    mTempCBuffer[i] *= mPostWindow[i];
  }

  /* writeout */

  for (i = 0; i < kOverlapSize; i++) {
    int currentBlockIndex, nextBlockIndex, blockOffset;

    double c = (mOverlapC[i] + mTempCBuffer[i]) * ccscaleinv;
    double l = mInput[(mInputPos + i) * 2 + 0] - c;
    double r = mInput[(mInputPos + i) * 2 + 1] - c;

    if (mAmpFactor == 1.0) {
      mOutputCenter[i] = c;
      mOutputLeft[i]   = l;
      mOutputRight[i]  = r;
    } else {
      mOutputCenter[i] = c * mAmpFactor;
      mOutputLeft[i]   = l * mAmpFactor;
      mOutputRight[i]  = r * mAmpFactor;
    }

    currentBlockIndex = 0;
    nextBlockIndex = 1;
    blockOffset = kOverlapSize;
    while (nextBlockIndex < kOverlapCount - 1) {
      mOverlapC[currentBlockIndex * kOverlapSize + i] =
         mOverlapC[nextBlockIndex * kOverlapSize + i] +
         mTempCBuffer[blockOffset + i];

      currentBlockIndex++;
      nextBlockIndex++;
      blockOffset += kOverlapSize;
    }
    mOverlapC[currentBlockIndex * kOverlapSize + i] = mTempCBuffer[blockOffset + i];
  }

  mInputSamplesNeeded = kOverlapSize;
}

static uint IntegerLog2(uint v) {
  uint i = 0;

  while (v > 1) {
    i++;
    v >>= 1;
  }

  return i;
}

static uint RevBits(uint x, uint bits) {
  uint y = 0;

  while (bits-- != 0) {
    y = (y + y) + (x & 1);
    x >>= 1;
  }

  return y;
}

static double *CreatePostWindow(int windowSize, int power) {
  double *post = VDCreateRaisedCosineWindow(windowSize, (double)power);
  double powerIntegrals[] = { 1.0, 1.0/2.0, 3.0/8.0, 5.0/16.0,
    35.0/128.0, 63.0/256.0, 231.0/1024.0, 429.0/2048.0 };
  double scalefac = powerIntegrals[1] / powerIntegrals[power + 1];
  int i;

  for(i = 0; i < windowSize; i++) {
    post[i] *= scalefac;
  }

  return post;
}

static double *VDCreateRaisedCosineWindow(int n, double power) {
  double twopi_over_n = twopi / n;
  double *dst;
  int i;

  lsx_valloc(dst, n);

  for (i = 0; i < n; i++) {
    dst[i] = pow(0.5 * (1.0 - cos(twopi_over_n * (i + 0.5))), power);
  }

  return dst;
}

static double *VDCreateHalfSineTable(int n) {
  double twopi_over_n = twopi / n;
  double *dst;
  int i;

  lsx_valloc(dst, n);

  for (i = 0; i < n; i++) {
    dst[i] = sin(twopi_over_n * i);
  }

  return dst;
}

static uint *VDCreateBitRevTable(int n) {
  uint bits = IntegerLog2((uint)n);
  uint *dst;
  int i;

  lsx_valloc(dst, n);

  for (i = 0; i < n; i++) {
    dst[i] = RevBits((uint)i, bits);
  }

  return dst;
}

static void VDComputeFHT(double *A, int nPoints, double *sinTab) {
  int i, n, n2, theta_inc;

  /* FHT - stage 1 and 2 (2 and 4 points) */

  for (i = 0; i < nPoints; i += 4) {
    double x0 = A[i];
    double x1 = A[i + 1];
    double x2 = A[i + 2];
    double x3 = A[i + 3];

    double y0 = x0 + x1;
    double y1 = x0 - x1;
    double y2 = x2 + x3;
    double y3 = x2 - x3;

    A[i]     = y0 + y2;
    A[i + 2] = y0 - y2;

    A[i + 1] = y1 + y3;
    A[i + 3] = y1 - y3;
  }

  /* FHT - stage 3 (8 points) */

  for (i = 0; i < nPoints; i += 8) {
    double alpha, beta;
    double beta1, beta2;

    alpha = A[i + 0];
    beta  = A[i + 4];

    A[i + 0] = alpha + beta;
    A[i + 4] = alpha - beta;

    alpha = A[i + 2];
    beta  = A[i + 6];

    A[i + 2] = alpha + beta;
    A[i + 6] = alpha - beta;

    alpha = A[i + 1];

    beta1 = invsqrt2 * (A[i + 5] + A[i + 7]);
    beta2 = invsqrt2 * (A[i + 5] - A[i + 7]);

    A[i + 1] = alpha + beta1;
    A[i + 5] = alpha - beta1;

    alpha = A[i + 3];

    A[i + 3] = alpha + beta2;
    A[i + 7] = alpha - beta2;
  }

  n = 16;
  n2 = 8;
  theta_inc = nPoints >> 4;

  while (n <= nPoints) {
    for (i = 0; i < nPoints; i += n) {
      int j;
      int theta = theta_inc;
      double alpha, beta;
      int n4 = n2 >> 1;

      alpha = A[i];
      beta  = A[i + n2];

      A[i]      = alpha + beta;
      A[i + n2] = alpha - beta;

      alpha = A[i + n4];
      beta  = A[i + n2 + n4];

      A[i + n4]      = alpha + beta;
      A[i + n2 + n4] = alpha - beta;

      for (j = 1; j < n4; j++) {
        double sinval = sinTab[theta];
        double cosval = sinTab[theta + (nPoints >> 2)];

        double alpha1 = A[i + j];
        double alpha2 = A[i - j + n2];
        double beta1  = A[i + j + n2] * cosval + A[i - j + n] * sinval;
        double beta2  = A[i + j + n2] * sinval - A[i - j + n] * cosval;

        theta += theta_inc;

        A[i + j]      = alpha1 + beta1;
        A[i + j + n2] = alpha1 - beta1;
        A[i - j + n2] = alpha2 + beta2;
        A[i - j + n]  = alpha2 - beta2;
      }
    }

    n *= 2;
    n2 *= 2;
    theta_inc >>= 1;
  }
}

/*
 * Process command-line options but don't do other
 * initialization now: effp->in_signal & effp->out_signal are not
 * yet filled in.
 */
static int getopts_centercut(sox_effect_t * effp, int argc, char **argv)
{
  priv_t *p = (priv_t *)effp->priv;

  kWindowSize = 8192;
  kOverlapCount = 4;
  kPostWindowPower = 2;
  mAmpFactor = 1.0;

  argc--, argv++; /* Skip effect name */

  while (argc > 0) {
    char ch;

    if (argv[0][0] != '-') {
      lsx_fail("invalid argument `%s`", argv[0]);
      return lsx_usage(effp);
    } else switch (ch = argv[0][1]) {
      char *string;

    case 'b':
      mBassToSides = sox_true;
      break;

    case 'a':
    case 'w':
      /* Find the argument attached to the flag or in the next one */
      if (argv[0][2]) string = &argv[0][2];
      else if (argc < 2) {
        lsx_fail("-%c what?", ch);
        return SOX_EOF;
      } else {
        argc--; argv++;
        string = argv[0];
      }
      /* Convert as appropriate */
      switch (ch) {
      case 'a':
        {
          char *endptr = string;
          mAmpFactor = lsx_strtod(string, &endptr);
          if (endptr == string || *endptr != '\0') {
            lsx_fail("-%c what?", ch);
            return SOX_EOF;
          }
        }
        break;
      case 'w':
        {
          char dummy;
          if (sscanf(string, "%u%c", &kWindowSize, &dummy) != 1) {
            lsx_fail("-%c what?", ch);
            return SOX_EOF;
          }
          /* Check it's a power of two. (x & (x-1)) clears one bit. */
          /* Empirically, it bombs on 4 and gives silence on 65536. */
          if ((kWindowSize < 8) || kWindowSize > 32768 ||
              (kWindowSize & (kWindowSize - 1))) {
            lsx_fail("window size must be a power of two from 8 to 32768");
            return SOX_EOF;
          }
        }
        break;
      }
      break;

    default:
      lsx_fail("invalid option `%s`", argv[0]);
      return lsx_usage(effp);
    }
    argc--; argv++;
  }
  effp->out_signal.channels = 3;

  return SOX_SUCCESS;
}

static char * get_centercut(sox_effect_t *effp, char *name)
{
  priv_t *p = (priv_t *)effp->priv;
  char *s = NULL;

  if (!strcmp(name, "gain")) {
    s = lsx_malloc(32);
    sprintf(s, "%g", mAmpFactor);
  }

  return s;
}

static char *
set_centercut(sox_effect_t *effp, char *name, char *value)
{
  priv_t *p = (priv_t *)effp->priv;
  char *s = NULL;
  char *endptr = value;

  if (!strcmp(name, "gain")) {
    /* Set it in the units they specified */
    double gain = lsx_strtod(value, &endptr);

    if (endptr == value) return NULL;

    mAmpFactor = gain;
    s = malloc(32);
    sprintf(s, "%g", gain);
  }
  return s;
}

/*
 * CenterCut_Init() needs to know the sample rate, which isn't set in getopts()
 * but we are an MCHAN effect so start() is only called once so we
 * initialize there.
 */
static int start_centercut(sox_effect_t * effp)
{
  priv_t *p = (priv_t *)effp->priv;

  if (effp->in_signal.channels != 2) {
    lsx_fail("can only process stereo");
    return SOX_EOF;
  }

  mSampleRate = lrint(effp->in_signal.rate);

  CenterCut_Init(effp);

  effp->out_signal.length = effp->in_signal.length;
  effp->out_signal.channels = 3;

  return SOX_SUCCESS;
}

/*
 * Process up to *isamp samples from ibuf and produce up to *osamp samples
 * in obuf.  Write back the actual numbers of samples to *isamp and *osamp.
 * Return SOX_SUCCESS or, if error occurs, SOX_EOF.
 *
 * flow() is given and wants to output arbitrary number of samples but
 * CenterCut_Run() wants mInput[0..kWindowSize-1] to be valid and outputs
 * kOverlapSize samples into kOutput{Center,Left,Right}[0..kOverlapSize-1].
 *
 * To match these two world views:
 * while (there's stuff to output or stuff to input) {
 *   if (there's stuff to output) output it;
 *   if (there's stuff to input) {
 *      Feed it as much input as it will take;
 *      ProcessSamples() if all the pending output has been sent to obuf[];
 *   }
 * }
 */
static int flow_centercut(sox_effect_t * effp, const sox_sample_t *ibuf, sox_sample_t *obuf,
                           size_t *isamp, size_t *osamp)
{
  priv_t *p = (priv_t *)effp->priv;
  size_t samples_written = 0; /* What shall we set *osamp to? */
  size_t samples_read = 0;    /* What shall we set *isamp to? */
  int i;

  /* While there's stuff to send to the output or to take from the input */

  while ((mOutputCount > 0 && samples_written < *osamp) ||
         (*isamp > 0 && mInputSamplesNeeded > 0)) {

    /* If there's stuff to output, output it */
    if (mOutputCount > 0 && samples_written < *osamp) {
      int frames_to_output = min((int)((*osamp - samples_written) / 3),
                                 mOutputCount);
      SOX_SAMPLE_LOCALS;

      for (i=0; i<frames_to_output; i++) {
        *obuf++ = SOX_FLOAT_64BIT_TO_SAMPLE(mOutputLeft[mOutputPos + i],
                                            effp->clips);
        *obuf++ = SOX_FLOAT_64BIT_TO_SAMPLE(mOutputRight[mOutputPos + i],
                                            effp->clips);
        *obuf++ = SOX_FLOAT_64BIT_TO_SAMPLE(mOutputCenter[mOutputPos + i],
                                            effp->clips);
      }
      *osamp -= 3;
      samples_written += frames_to_output * 3;
      mOutputPos += frames_to_output;
      mOutputCount -= frames_to_output;
    }

    /* If there's stuff to input, feed it to CenterCut */
    if (*isamp > 0 && mInputSamplesNeeded > 0) {
      int frames_to_input = min((int)(*isamp / 2), mInputSamplesNeeded);

      for (i=0; i < frames_to_input; i++) {
        mInput[((mInputPos + i) % kWindowSize) * 2 + 0] =
          SOX_SAMPLE_TO_FLOAT_64BIT(*ibuf, effp->clips);
        ibuf++;
        mInput[((mInputPos + i) % kWindowSize) * 2 + 1] =
          SOX_SAMPLE_TO_FLOAT_64BIT(*ibuf, effp->clips);
        ibuf++;
      }
      *isamp -= frames_to_input * 2;
      samples_read += frames_to_input * 2;

      /* Only Run if the output buffer has been exhausted */
      if (mOutputCount == 0) {
        if (CenterCut_ProcessSamples(effp, frames_to_input)) {
          /* It returned a block of data */
          mOutputPos = 0;
          mOutputCount = kOverlapSize;
        }
      }
    }
  }

  *isamp = samples_read;
  *osamp = samples_written;
  return SOX_SUCCESS;
}

static int drain_centercut(sox_effect_t * effp, sox_sample_t *obuf, size_t *osamp)
{
  priv_t *p = (priv_t *)effp->priv;
  size_t samples_written = 0; /* WHat shall we set *osamp to? */
  int i;

  /* First,. get rid of any pending output frames */
  if (mOutputCount > 0) {
    int frames_to_output = min((int)(*osamp / 3), mOutputCount);
    SOX_SAMPLE_LOCALS;

    for (i=0; i<frames_to_output; i++) {
      *obuf++ = SOX_FLOAT_64BIT_TO_SAMPLE(mOutputLeft[mOutputPos + i],
                                          effp->clips);
      *obuf++ = SOX_FLOAT_64BIT_TO_SAMPLE(mOutputRight[mOutputPos + i],
                                          effp->clips);
      *obuf++ = SOX_FLOAT_64BIT_TO_SAMPLE(mOutputCenter[mOutputPos + i],
                                          effp->clips);
    }
    samples_written += frames_to_output * 3;
    *osamp -= frames_to_output * 3;
    mOutputPos += frames_to_output;
    mOutputCount -= frames_to_output;
  }

  if (mOutputCount > 0) {
    *osamp = samples_written;
    return SOX_SUCCESS; /* There's more to output */
  }

  while (*osamp >= 3 &&
         (mOutputCount = CenterCut_FlushWithReturn(effp)) > 0) {
    int frames_to_output = min((int)(*osamp / 3), mOutputCount);
    SOX_SAMPLE_LOCALS;
    mOutputPos = 0;

    for (i=0; i<frames_to_output; i++) {
      *obuf++ = SOX_FLOAT_64BIT_TO_SAMPLE(mOutputLeft[mOutputPos + i],
                                          effp->clips);
      *obuf++ = SOX_FLOAT_64BIT_TO_SAMPLE(mOutputRight[mOutputPos + i],
                                          effp->clips);
      *obuf++ = SOX_FLOAT_64BIT_TO_SAMPLE(mOutputCenter[mOutputPos + i],
                                          effp->clips);
    }
    samples_written += frames_to_output * 3;
    *osamp -= frames_to_output * 3;
    mOutputPos += frames_to_output;
    mOutputCount -= frames_to_output;
  }

  *osamp = samples_written;

  return mOutputCount == 0 ? SOX_EOF : SOX_SUCCESS;
}

static int stop_centercut(sox_effect_t * effp)
{
  CenterCut_Quit(effp);

  return SOX_SUCCESS;
}

const sox_effect_handler_t *lsx_centercut_effect_fn(void)
{
  static char const usage[] = "[-a gain-out] [-b] [-w size]";
  static char const * const extra_usage[] = {
    "-a  Multiply all output channels by gain-out",
    "-b  Move the bass out of the center for better karaoke",
    "-w  Set the window size (default: 8192 samples)",
    "Keymap: centercut.gain for the -a parameter",
    NULL
  };
  static sox_effect_handler_t sox_centercut_effect = {
    "centercut", usage,
    SOX_EFF_MCHAN | SOX_EFF_CHAN | SOX_EFF_GAIN,
    getopts_centercut, start_centercut, flow_centercut, drain_centercut,
    stop_centercut, NULL,
    sizeof(priv_t),
    extra_usage, get_centercut, set_centercut,
  };
  return &sox_centercut_effect;
}
