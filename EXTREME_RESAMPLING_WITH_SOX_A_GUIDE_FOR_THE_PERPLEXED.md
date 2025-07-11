# SoX-ng Audio Processing Technical Guide

This document covers **SoX-ng (sox_ng)**, a next-generation fork of the Sound eXchange command-line audio processing tool. SoX-ng is a professional-grade application for manipulating digital audio files via modular, scriptable effects (EQ, resampling, filtering, dither, etc.), each of which can be combined in user-defined pipelines.

**Note**: This documentation covers both standard SoX-ng operation (which provides robust, high-quality resampling that is audibly transparent for most applications) and specialized configurations using extremely long FIR filters. The standard SoX-ng resampling algorithms are mathematically rigorous and more than sufficient for professional audio work. The extreme configurations documented here are primarily of interest for archival purposes, scientific research, or users who report perceiving differences with radical filter implementations.

## What Does This Chain Do?

### Example command:

```bash
sox input.wav -b 16 output.wav upsample 4 sinc -22050 -n 1638400 -L -b 0 vol 4 rate -v 44100 dither -F shibata -S
```

### Breakdown:

- **`upsample 4`**: Increases sample rate by 4× by inserting zeros between samples (used for oversampling or prepping for further processing).

- **`sinc -22050 -n 1638400 ...`**: Applies a linear-phase FIR filter (here, a brick-wall low-pass at 22.05 kHz using a 1.6 million tap filter for an extremely sharp cutoff and maximum stop-band rejection).

- **`vol 4`**: Boosts signal amplitude by 12 dB.

- **`rate -v 44100`**: Sample rate conversion (here, downsampling to 44.1 kHz) using SoX's or libsoxr's highest-quality resampling algorithm, which itself also uses multi-stage FIR filtering.

- **`dither -F shibata -S`**: Applies TPDF dither with Shibata-style noise shaping before reducing the bit depth to 16-bit integer, minimizing audible quantization noise.

## Why Does This Exist?

- **Ultimate control**: SoX exposes nearly every aspect of digital audio transformation to the user: filter shapes, kernel sizes, sample rates, bit-depth, noise-shaping, and effect order.

- **Scientific & archival use**: Researchers and engineers use SoX to design, test, and execute precise, reproducible DSP pipelines—e.g., for brick-wall anti-aliasing, psychoacoustic analysis, or bit-depth studies.

- **Audiophile and mastering**: Audio pros use SoX to prepare files for mastering, format conversion, or DAC-specific requirements with total control over filtering and dither.

### Key Features & Use Cases

### Resampling up or down
SoX-ng supports both upsampling (increasing sample rate for DSP or synthesis, with zero-stuffing and LPF to suppress images) and downsampling (reducing sample rate, with anti-alias filtering to avoid foldover).

### Arbitrary filtering
The `sinc` effect can be used to specify any cutoff frequency, transition width, filter window, and tap length—enabling brick-wall filtering at any desired frequency (e.g., 22.05 kHz for Red Book, 50 kHz for high-res).

### Multi-stage workflows
Effects can be chained in any order. You can, for example:
- Upsample → nonlinear effect → brick-wall → downsample
- Bandlimit for measurement or scientific analysis
- Dither and noise-shape to match target bit depth and psychoacoustic criteria

### Dither & noise shaping
SoX-ng provides multiple dither algorithms and noise-shaping curves to maximize subjective quality at lower bit depths:

**Dither types**:
- **TPDF**: Triangular Probability Density Function (default)
- **Gaussian**: Alternative noise distribution

**Noise shaping filters** (`-f` option):
- **`lipshitz`**: Minimal shaping, flat noise distribution
- **`f-weighted`**: ITU-R 468 psychoacoustic weighting
- **`modified-e-weighted`**: More aggressive HF emphasis
- **`improved-e-weighted`**: Maximum HF emphasis
- **`gesemann`**: Modern psychoacoustic curve
- **`shibata`**: Pushes noise into ultrasonic frequencies for improved subjective performance
- **`none`**: Flat dither without shaping

**Precision** (`-p` bits): Target bit depth (e.g., `-p 21` for Yggdrasil DAC compatibility)

**When to use**: Always apply dither when reducing bit depth to avoid quantization artifacts. Noise shaping is most beneficial for 16-bit or lower output.

## How Does It Fit into Resampling Workflows?

### Increasing sample rate (upsampling):
- Insert zeros (`upsample`) → brick-wall filter (`sinc`) to suppress images → further DSP or resampling
- **Critical**: When upsampling, set the brick-wall filter at the *source's* Nyquist frequency, not the target's
- Example: 44.1 → 176.4 kHz uses `sinc -22050` (half of 44.1 kHz)
- Useful for prepping audio for nonlinear effects or analog modeling that would otherwise generate in-band aliasing

### Decreasing sample rate (downsampling):
- Brick-wall filter (`sinc`) at the *target's* Nyquist frequency → downsample to prevent aliasing
- The user can control how steep and precise the anti-aliasing is by adjusting `sinc` parameters or `rate`'s `-b` setting

### Bit depth reduction workflow:
When reducing bit depth (e.g., 24 → 16 bit), the proper chain is:
1. **Resample** (if changing sample rate)
2. **Apply dither** with noise shaping
3. **Quantize** to target bit depth

Example:
```bash
sox input.flac -b 16 output.flac rate -v 176400 dither -S -f shibata
```

### Custom workflow:
- Define any cutoff (e.g., 50 kHz instead of 22.05 kHz)
- Apply multi-stage filtering/EQ for measurement, mastering, or experimental DSP
- Apply exactly the dither/noise-shaping curve desired for final delivery
- Use gain compensation (`vol`) to maintain unity gain after filtering

## Why Does SoX Matter?

- **Precision**: Lets you define exactly how and where the signal is bandlimited, with tap counts and filter shapes far beyond what typical DAWs or SRC libraries expose.

- **Flexibility**: Effects can be sequenced, repeated, or omitted entirely. Nearly every DSP parameter is user-tweakable.

- **Transparency**: SoX can be run at ultra-high precision (64-bit float internal processing), and outputs can be bit-exact and fully reproducible.

## Summary

SoX-ng is a modular, scriptable DSP environment for audio, exposing control over filtering, resampling, gain, dither, and noise-shaping. It can be used for standard audio conversion with excellent results, or configured for extreme experimental workflows with precise control over filter response, bit depth, and noise characteristics.

Standard SoX-ng resampling provides transparent, high-quality results that meet or exceed the requirements of professional audio production. The extreme configurations documented below push computational limits far beyond what is necessary for audibly transparent conversion.

## Extreme Resampling Configuration

### Overview

This section documents configurations using filters with millions of taps (versus the thousands used in standard high-quality resampling). These extreme configurations achieve frequency response accuracy within 0.0001 dB of theoretical ideals. 

**Important context**: Standard SoX resampling with default settings provides mathematically accurate, audibly transparent results. The extreme configurations described here are primarily relevant for:

- **Archival applications**: Where theoretical perfection is prioritized over practical considerations
- **Scientific research**: Testing filter designs and measuring theoretical limits
- **Specialized use cases**: Users who report perceiving differences with extremely steep filter implementations

**Performance considerations**: These configurations require significant computational resources (potentially hours of processing and gigabytes of RAM) while providing improvements that are, at best, at the threshold of perception on exceptional playback systems.

### Key Differences from Standard SoX-ng

1. **Filter Length**: Standard high-quality mode uses thousands of taps; extreme configurations use 65,536 to 67,108,864+ taps
2. **Memory Requirements**: Can require multiple GB RAM versus standard's modest requirements
3. **Processing Time**: Hours versus seconds for typical files
4. **Theoretical Performance**: Achieves frequency response accuracy within 0.0001 dB of mathematical ideal
5. **Practical Benefit**: Marginal to imperceptible for most listeners and playback systems

### Configuration Parameters

| Parameter | Range | Typical | Description |
|-----------|-------|---------|-------------|
| **Filter Taps** | 65,536 to 67,108,864+ | 1,638,400 | Number of filter coefficients. Standard SoX uses ~2,000-10,000 for high quality |
| **Transition Band** | 1-1000 Hz | 50 Hz | Width between passband and stopband. Narrower transitions require more computational resources |
| **Passband Frequency** | 15 kHz to Nyquist/2 | Auto | Upper frequency limit. Usually set automatically based on source |
| **Kaiser Beta** | 0.0-50.0 | 16.0 | Window function parameter. Higher values reduce passband ripple |
| **Phase Response** | linear/minimum | linear | Linear phase preserves time-domain relationships |

### Performance Characteristics

| Filter Taps | RAM Usage | Processing Time* | Notes |
|------------|-----------|------------------|-------|
| 2,048 | ~10 MB | <1 second | Standard high quality |
| 65,536 | ~100 MB | 5-10 seconds | Extreme configuration |
| 262,144 | ~200 MB | 20-40 seconds | |
| 1,638,400 | ~500 MB | 2-5 minutes | |
| 67,108,864 (2²⁶) | ~16 GB | Several hours | Power-of-2 optimization provides ~2.5× speed improvement |

*Approximate times for a 5-minute stereo file on modern hardware

### Example Commands

Standard high-quality resampling (recommended for most uses):
```bash
sox input.flac -b 24 output.flac rate -v 176400
```

Extreme configuration (44.1 → 176.4 kHz):
```bash
sox -S -V6 input.flac -b 24 -r 176400 output.flac \
  upsample 4 \
  sinc -22050 -n 67108864 -L -b 0 \
  vol 1
```

**Command Options Explained**:
- `-S`: Show progress (essential for long operations)
- `-V6`: Maximum verbosity level for debugging
- `upsample 4`: 4× oversampling before filtering
- `sinc -22050`: Brick-wall filter at 22.05 kHz (source's Nyquist for 44.1 kHz files)
- `-n 64000000`: 64 million tap filter (!!)
- `-L`: Linear phase response
- `-b 0`: Maximum precision Kaiser window
- `vol 4`: Gain adjustment (see below)

**Gain compensation (`vol` parameter)**:
The `vol` setting serves several purposes:
- **Filter compensation**: Extreme sinc filters may slightly attenuate the passband despite being designed for unity gain
- **DAC optimization**: Different DACs have varying input sensitivities and may perform better with signals closer to 0 dBFS
- **Headroom management**: Upsampling can reveal intersample peaks not visible at the original sample rate
- **Avoiding downstream attenuation**: Ensuring optimal levels during high-precision resampling prevents bit depth loss from later digital volume adjustments

The optimal value is system-specific and depends on:
- Your filter parameters (tap count, transition band, window function)
- Your DAC's input sensitivity and internal headroom
- Your playback chain's gain structure

Start with `vol 1` (unity) and increase gradually while monitoring for clipping. The `vol 4` in the examples appears to be empirically determined for specific equipment (likely Schiit Audio DACs based on context).

### Alternative Approaches

**Modified SoX-ng builds**: Some users modify the SoX-ng source code to enable even more extreme settings:
- Bandwidth up to 99.98% (vs. standard 99.7%)
- Larger FFT sizes (524288 vs. 262144)
- Enables 300,000+ tap filters via the `rate` command:
  ```bash
  sox in.flac -b 24 out.flac rate -u -b 99.98 176400
  ```

**Using libsoxr explicitly**: For the highest quality rate conversion:
```bash
sox input.flac output.flac rate -v 176400  # -v invokes libsoxr VHQ mode
```

### Complete Processing Chain Example

For 44.1 → 176.4 kHz with 24 → 21 bit conversion:
```bash
sox -S input.flac -b 24 output.flac \
  upsample 4 \
  sinc -22050 -n 67108864 -L -b 0 \
  vol 4 \
  rate -v 176400 \
  dither -S -p 21 -f gesemann
```

This chain:
1. Upsamples with zero-insertion
2. Applies 67M-tap brick-wall filter at source Nyquist
3. Compensates gain
4. Fine-tunes rate with libsoxr
5. Applies psychoacoustic dither to 21 bits

The extreme tap counts and narrow transition bands demonstrate the theoretical limits of FIR filter design, far exceeding the requirements for transparent audio conversion.

### Practical Considerations

**Use cases for extreme configurations**:
- **Archival preservation**: When theoretical perfection is prioritized regardless of practical considerations
- **Scientific research**: Testing filter designs, measuring equipment, or studying digital signal processing limits
- **Specialized applications**: Some users with high-end playback systems report perceiving differences with extremely steep filter implementations

**Technical reality**:
- Standard SoX-ng resampling with default parameters provides transparent, accurate conversion
- Differences between standard and extreme configurations are typically below the threshold of audibility
- Most DACs and playback systems cannot resolve theoretical improvements beyond standard high-quality resampling
- Processing time increases dramatically (hours versus seconds) for marginal theoretical gains

**Important note**: The subjective improvements reported by some users with extreme filter configurations are based on informal listening tests with small sample sizes. These observations have not been validated through controlled testing and may be influenced by expectation bias or system-specific characteristics.

For the vast majority of professional and consumer audio applications, standard SoX-ng resampling provides results that are mathematically accurate and audibly indistinguishable from these extreme configurations.
