# Implementing Large Sinc Filter Support in SoX_ng

## Overview

This document explains how to enable support for extremely large sinc filters (up to ~1 billion taps) in SoX_ng, based on the sox-sinc project's improvements.

## Changes Required

### 1. FFT Size Limit (fft4g.h)

**File**: `src/fft4g.h`  
**Line**: 18  
**Change**: Increase `FFT4G_MAX_SIZE` from `262144` to `1073741824`

```c
// Original:
#define FFT4G_MAX_SIZE 262144

// New:
#define FFT4G_MAX_SIZE 1073741824
```

**Explanation**: This increases the maximum FFT size from 2^18 to 2^30, enabling much larger filter processing.

### 2. FFT Implementation Arrays (fft4g.c)

**File**: `src/fft4g.c`  
**Lines**: 745 and 846  
**Change**: Increase the `ip[]` array size in both `bitrv2` and `bitrv2conj` functions

```c
// Original (line 745):
static void bitrv2(int n, int *ip0, double *a)
{
    int j, j1, k, k1, l, m, m2, ip[256];

// New:
static void bitrv2(int n, int *ip0, double *a)
{
    int j, j1, k, k1, l, m, m2, ip[16384];

// Original (line 846):
static void bitrv2conj(int n, int *ip0, double *a)
{
    int j, j1, k, k1, l, m, m2, ip[256];

// New:
static void bitrv2conj(int n, int *ip0, double *a)
{
    int j, j1, k, k1, l, m, m2, ip[16384];
```

**Explanation**: The relationship is: for every power of 4 increase in FFT4G_MAX_SIZE, increase ip[] array by a power of 2.
- Original: FFT4G_MAX_SIZE = 262144 = 4^9, ip[256] = 2^8
- New: FFT4G_MAX_SIZE = 1073741824 = 4^15, ip[16384] = 2^14
- Increase: 4^6 times larger FFT → 2^6 = 64 times larger ip array

### 3. Sinc Filter Tap Limits (sinc.c)

**File**: `src/sinc.c`  
**Lines**: 51 and 92  
**Change**: Increase the maximum number of taps from 32767 to 1073741823

```c
// Line 51 - Original:
GETOPT_NUMERIC(optstate, 'n', num_taps[1], 11, 32767)

// Line 51 - New:
GETOPT_NUMERIC(optstate, 'n', num_taps[1], 11, 1073741823)

// Line 92 - Original:
*num_taps = range_limit(n, 11, 32767);

// Line 92 - New:
*num_taps = range_limit(n, 11, 1073741823);
```

**Explanation**: This allows users to specify up to ~1 billion filter taps instead of just 32,767.

## Applying the Changes

### Option 1: Manual Edit
1. Open each file and make the changes as specified above
2. Rebuild sox_ng:
   ```bash
   make clean
   make
   sudo make install
   ```

### Option 2: Apply Patch
1. Apply the provided patch:
   ```bash
   cd /mnt/hodgepodge/dev/sox_ng
   patch -p1 < enable-large-sinc-filters.patch
   ```
2. Rebuild sox_ng:
   ```bash
   make clean
   make
   sudo make install
   ```

## Testing

Test with progressively larger filter sizes:

```bash
# Test with 100,000 taps (should work with patched version)
sox_ng input.wav output.wav sinc -n 100000

# Test with 1 million taps (will be slow!)
sox_ng input.wav output.wav sinc -n 1000000

# Test with 8 million taps (very slow, needs lots of RAM)
sox_ng input.wav output.wav sinc -n 8000000
```

## Memory Considerations

- Each tap requires ~8 bytes (double precision)
- 1 million taps ≈ 8 MB just for coefficients
- 8 million taps ≈ 64 MB for coefficients
- Additional working memory is needed for FFT operations

## Limitations

1. **32-bit Integer Limits**: Despite allowing up to 1 billion taps, practical limits are around 8-16 million due to 32-bit integer arithmetic in various parts of the code.

2. **Memory Allocation**: Very large filters may fail with allocation errors if insufficient RAM is available.

3. **Processing Time**: Filters with millions of taps can take many minutes to process even short audio files.

## Performance Tips

1. Use the `-V3` flag to see verbose output about filter parameters
2. Monitor memory usage during processing
3. Consider using the `upsample` effect for better quality as shown in sox-sinc examples
4. For production use, test thoroughly with your specific use cases

## Example Usage (Insane Mode)

```bash
# Ultra-high quality upsampling from 96kHz to 192kHz
sox_ng -S -V3 input_96k.flac -b 24 -r 192000 output_192k.flac \
  upsample 4 \
  sinc -n 1638400 -b 16 \
  vol 1
```

This uses a 1.6 million tap sinc filter for exceptional frequency response accuracy.