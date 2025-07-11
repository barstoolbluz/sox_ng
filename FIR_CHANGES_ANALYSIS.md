# Complete Analysis of FIR-Related Changes in sox-sinc

## Overview

After a thorough comparison of sox_ng and sox-sinc repositories, I've identified several FIR-related changes beyond the initial sinc filter tap limit increases. These changes appear to be focused on:

1. Memory allocation patterns
2. Error handling simplification
3. Code simplification in some areas

## Detailed Changes Found

### 1. Core FFT Implementation Changes

**Files**: `src/fft4g.h`, `src/fft4g.c`

```c
// fft4g.h
- #define FFT4G_MAX_SIZE 262144
+ #define FFT4G_MAX_SIZE 1073741824

// fft4g.c (line ~745)
- int j, j1, k, k1, l, m, m2, ip[256];
+ int j, j1, k, k1, l, m, m2, ip[16384];

// fft4g.c (line ~846)
- int j, j1, k, k1, l, m, m2, ip[256];
+ int j, j1, k, k1, l, m, m2, ip[16384];
```

**Impact**: Enables processing of much larger FFTs, supporting filters with millions of taps.

### 2. Sinc Filter Changes

**File**: `src/sinc.c`

```c
// Line 51
- GETOPT_NUMERIC(optstate, 'n', num_taps[1], 11, 32767)
+ GETOPT_NUMERIC(optstate, 'n', num_taps[1], 11, 1073741823)

// Line 92
- *num_taps = range_limit(n, 11, 32767);
+ *num_taps = range_limit(n, 11, 1073741823);
```

**Impact**: Allows users to specify up to ~1 billion filter taps.

### 3. Memory Allocation Pattern Changes

Throughout multiple files, sox-sinc replaces SoX's allocation macros with direct calls:

**In `src/fir.c`**:
```c
- lsx_revalloc(p->h, p->n);
+ p->h = lsx_realloc(p->h, p->n * sizeof(*p->h));
```

**In `src/dft_filter.c`**:
```c
- lsx_vcalloc(f->coefs, f->dft_length);
+ f->coefs = lsx_calloc(f->dft_length, sizeof(*f->coefs));

- lsx_vcalloc(buff, 1024);
+ double * buff = lsx_calloc(1024, sizeof(*buff));
```

**In `src/rate.c`**:
```c
- lsx_valloc(result, length * (interp_order + 1));
+ sample_t * result = malloc(length * (interp_order + 1) * sizeof(*result));

- lsx_vcalloc(p->stages, p->num_stages + 1);
+ p->stages = calloc(p->num_stages + 1, sizeof(*p->stages));
```

**Impact**: More explicit memory allocation that may handle large allocations better.

### 4. Error Handling Simplification

**In `src/rate.c`**:

The sox-sinc version removes error checking from several functions:

```c
// Function signature change
- static int dft_stage_init(...)
+ static void dft_stage_init(...)

// Removed error handling
- if (L > dft_length) {
-   lsx_fail("invalid DFT parameters");
-   return SOX_EINVAL;
- }

// Simplified function returns
- static int rate_init(...)
+ static void rate_init(...)
```

**Impact**: Simplified code flow, but potentially less robust error handling.

### 5. firfit.c Simplification

The sox-sinc version significantly simplifies the firfit effect:

- Removes command-line knot specification capability
- Simplifies file parsing logic
- Marks the effect as SOX_EFF_ALPHA (experimental)
- Changes from using `lsx_revalloc` to direct allocation

**Impact**: Reduced functionality but simpler code.

### 6. Minor Changes

- Added `#include <string.h>` to several files
- Removed some copyright headers
- Changed precision limit in rate.c: `bw_3dB_pc, 74, 99.7` → `bw_3dB_pc, 74, 99.999999`
- Added defines in rate.c:
  ```c
  #define calloc     lsx_calloc
  #define malloc     lsx_malloc
  #define raw_coef_t double
  ```

## Recommendations for Integration

### Essential Changes for Large FIR Support:

1. **FFT size increases** (fft4g.h, fft4g.c) - REQUIRED
2. **Sinc tap limit increases** (sinc.c) - REQUIRED
3. **Memory allocation pattern changes** - RECOMMENDED for large allocations

### Optional/Questionable Changes:

1. **Error handling removal** - NOT RECOMMENDED (reduces robustness)
2. **firfit simplification** - OPTIONAL (depends on feature requirements)

### Integration Approach:

1. Apply the core FFT and sinc changes as in the original patch
2. Consider adopting the direct memory allocation pattern for better handling of large allocations
3. Keep the error handling from sox_ng for robustness
4. Evaluate whether the firfit simplification is desired

## Updated Patch Recommendation

The original patch covered the essential changes. For completeness, you might also want to:

1. Change memory allocation in dft_filter.c to handle large filters better
2. Test with various filter sizes to ensure stability
3. Add appropriate error messages when allocation fails due to size limits

The key insight is that sox-sinc focused on enabling extreme filter sizes at the expense of some error checking and features. For sox_ng, we can adopt the size increases while maintaining the robust error handling.