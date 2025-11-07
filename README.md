# sox-sinc

A specialized build of `sox_ng` supporting extremely large sinc filters for audio resampling

## What is sox-sinc?

sox-sinc is a modified build of [sox_ng](https://codeberg.org/sox_ng/sox_ng) that removes the standard limitation on sinc filter lengths. Where standard builds limit filters to 32,767 taps, this build supports up to approximately 1 billion taps, enabling the implementation of extremely steep filter characteristics.

## Important Context

The standard sox resampling implementation is already incredibly robust and produces excellent results that are audibly transparent for the vast majority of use cases. The perceptual differences between standard sox resampling and extreme filter implementations are, at best, subtle.They're imperceptible to most listeners, they are not typically validated by rigorous testing, _et tamen sunt_. 

This specialized build exists primarily for:
- Those who wish to experiment with extreme filter implementations
- Research into the theoretical limits of digital filtering
- Specific applications where mathematical precision takes precedence over practicality
- Users who believe they can perceive differences with extremely steep filters

The computational cost of these extreme filters is substantial—processing times can increase from seconds to many minutes for typical audio files.

## Technical Specifications

- **Filter length**: Up to ~1 billion taps (vs 32,767 in standard builds)
- **FFT size**: Supports up to 2^30 points for DFT-based convolution
- **Memory usage**: Scales linearly with filter length (~8 bytes per tap)
- **Base**: Built on sox_ng, the actively maintained fork of SoX
- **Compatibility**: Functions as a drop-in replacement for standard sox

## Key Features

- **Large Sinc Filters**: Up to ~1 billion taps (1,073,741,823 vs 32,767 standard)
- **DSD Audio Support**: Native Direct Stream Digital format support (DSF, DSDIFF)
- **Full Codec Support**: FFmpeg integration for comprehensive format compatibility
- **Drop-in Replacement**: Binary named `sox` for seamless replacement of legacy SoX
- **Cross-Platform**: Linux (x86_64, aarch64) and macOS (x86_64, aarch64) support
- **Reproducible Builds**: Nix flakes with locked dependencies

## Installation

### Using Nix Flakes (Recommended for Nix Users)

```bash
# Build and run directly from GitHub
nix run github:barstoolbluz/sox_ng -- --version

# Build locally
nix build github:barstoolbluz/sox_ng
./result/bin/sox --version

# Or clone and build
git clone https://github.com/barstoolbluz/sox_ng.git
cd sox_ng
nix build .
./result/bin/sox --version
```

### Using Flox (Recommended for Flox Users)

```bash
# Clone the repository
git clone https://github.com/barstoolbluz/sox_ng.git
cd sox_ng

# Build with Flox (Nix expression - cross-platform)
flox build sox_ng
./result-sox_ng/bin/sox --version

# OR build with manifest (platform-specific)
flox build sox_ng-linux   # For Linux
flox build sox_ng-darwin  # For macOS

# Add to your PATH
export PATH="$PWD/result-sox_ng/bin:$PATH"
```

### Traditional Build

```bash
# Standard autotools build (requires all dependencies)
autoreconf -i
./configure
make
sudo make install
```

## Usage Examples

### Standard Usage
For typical resampling, the standard sox parameters remain appropriate:
```bash
sox input.flac -b 24 output.flac rate -v 192000
```

### Large Filter Example
For those wishing to experiment with larger filters:
```bash
sox -S -V3 input_96k.flac -b 24 -r 192000 output_192k.flac \
  upsample 4 \
  sinc -n 1638400 -b 16 \
  vol 1
```

This applies a 1.6 million tap filter. Processing time will be significantly longer than standard resampling.

### Extreme Filter Example
```bash
sox input.flac output.flac \
  upsample 8 \
  sinc -n 8388608 -b 20 \
  vol 1
```

Note: An 8 million tap filter may require 10-20 minutes to process a typical song.

## Resampling Parameters

The following parameters control the resampling process. These are standard sox options; this build simply extends the allowed ranges for some parameters.

### Option Explanations

#### Quality
**Description:**
Selects the overall resampling quality preset, controlling how aggressively SoX optimizes for accuracy versus performance. Higher quality settings use longer filters, sharper transitions, and stricter rejection of aliasing.

**Typical Use:**
Choose higher settings for archival, mastering, scientific, or audiophile tasks where preserving fidelity is paramount. Lower settings may be preferred for faster processing or real-time applications.

#### Filter Taps (`-n`)
**Description:**
Sets the number of coefficients ("taps") in the FIR filter kernel. 0 or "auto" allows the program to determine an appropriate length based on other parameters.

**Typical Use:**
Specify a value only if you need to reproduce a particular filter or are experimenting. "Auto" is optimal for general use.

**sox-sinc enhancement:** Supports up to ~1 billion taps (vs 32,767 in standard sox/sox_ng)

#### Transition Band
**Description:**
Controls how abruptly the filter transitions from pass-band (preserved) to stop-band (attenuated). Specified in Hz, with smaller values resulting in steeper, more "brick-wall" filtering.

**Typical Use:**
For extreme anti-aliasing, set a very narrow transition band (requires more filter taps and CPU/RAM). Broader transition bands are more efficient but less aggressive.

#### Passband Frequency
**Description:**
The upper frequency passed without attenuation—usually set as a fraction or percentage of the output's Nyquist frequency. "Auto" lets the software decide based on context.

**Typical Use:**
Override only if you require a specific cutoff or want to tailor the response for specialized workflows (e.g., early roll-off for measurement).

#### Kaiser Beta (`-b`)
**Description:**
Adjusts the window function used to design the FIR filter. Higher beta values reduce passband ripple and sidelobes (less ringing), at the cost of a slightly less sharp transition.

**Typical Use:**
Increase for smoother filters, decrease (or set to zero) for maximum steepness.

**Values:**
- 0: Rectangular window (sharp cutoff, most ringing)
- 8.6: Excellent rejection (-80 dB)
- 16.0: Exceptional rejection (-140 dB) [default]
- 25.0: Extreme rejection (-180 dB)

#### Phase Response
**Description:**
Selects linear-phase (preserves waveform shape, adds latency), minimum-phase (lower latency, possible transient smearing), or intermediate settings.

**Options:**
- `-L`: Linear phase (best for music)
- `-M`: Minimum phase
- `-I`: Intermediate phase

**Typical Use:**
Linear phase is recommended for offline processing and mastering; minimum phase may be preferred for real-time or emulation of analog hardware.

#### Gain Compensation
**Description:**
Applies a gain adjustment (in dB) to compensate for any level changes caused by filtering.

**Typical Use:**
Leave at zero unless you observe a consistent change in signal amplitude after resampling.

#### Allow Aliasing
**Description:**
If enabled, allows frequencies above the output's Nyquist to fold (alias) into the audible range, which is generally undesirable for hi-fi or scientific work but may be used for creative effects.

**Typical Use:**
Keep disabled for transparent conversion.

#### Dither
**Description:**
Specifies the type of dither noise added during bit-depth reduction. Triangular (TPDF) is widely accepted as the most transparent for audio.

**Typical Use:**
Always dither when reducing bit depth (e.g., from 24 to 16 bits) to avoid quantization artifacts.

#### Noise Shaping
**Description:**
Controls how dither noise is distributed across the frequency spectrum. Shaping curves like "Shibata" push more noise energy into ultrasonic frequencies, improving subjective noise performance within the audible band.

**Typical Use:**
Useful for maximizing perceived quality in 16-bit (or lower) output. Can be disabled or set to "none" for flat dither.

## Practical Considerations

### When Standard Sox is Sufficient
- Professional audio production and mastering
- Format conversion for distribution
- General resampling tasks
- Real-time processing

The standard sox resampling engine already provides excellent quality with sensible trade-offs between quality and processing time.

### When This Build May Be Considered
- Academic research into filter design
- Exploration of theoretical limits in digital filtering
- Specific measurement or analysis requirements
- Personal experimentation with extreme parameters
- Perceived discernment of actual, audible, aurally pleasing differences
- Metaphysical comfort, a la Nietzsche's account of tragedy: i.e., beneath all painful, transitory appearances there lies an indestructible, ever-renewing life-force which gives us to feel, however briefly, that existence is ultimately powerful and enduring--: that life and living are worth affirming. 

Remember that the audible benefits of extreme filter lengths are debatable and likely imperceptible in most listening scenarios.

## Performance Characteristics

| Filter Taps | Processing Time* | RAM Usage |
|------------|------------------|----------|
| 32,767 (standard) | <5 seconds | ~1 MB |
| 65,536 | ~10 seconds | ~1 MB |
| 262,144 | ~40 seconds | ~2 MB |
| 1,638,400 | 2-5 minutes | ~13 MB |
| 8,388,608 | 10-20 minutes | ~64 MB |

*For a 5-minute stereo track on modern hardware

Note the exponential increase in processing time with filter length.

## Implementation Details

This build modifies three constraints in the sox_ng codebase:

1. **FFT Size Limit**: `FFT4G_MAX_SIZE` increased from 262,144 to 1,073,741,824
2. **Working Array Size**: `ip[]` arrays in FFT routines increased from 256 to 16,384 elements
3. **Filter Length Limit**: Maximum sinc filter taps increased from 32,767 to 1,073,741,823

These changes allow the DFT-based convolution engine to handle much larger filters at the cost of increased memory usage and computation time.

## Differences from Standard sox_ng

- Increased maximum filter length from 32,767 to ~1 billion taps
- Proportionally increased memory allocation for FFT operations
- No changes to default behavior or standard usage patterns
- Fully backward compatible with existing sox scripts and workflows

## Relationship to sox_ng

sox-sinc is a specialized build of [sox_ng](https://codeberg.org/sox_ng/sox_ng), which is itself an actively maintained fork of the original SoX project. sox_ng:
- Imports, compares and refines bug fixes from 50+ distributions
- Makes regular releases with a six-monthly cadence
- Maintains compatibility while fixing long-standing issues
- Lives at [codeberg.org/sox_ng/sox_ng](https://codeberg.org/sox_ng/sox_ng)

This build adds extreme FIR filter capabilities on top of sox_ng's improvements.

## Building from Source

### Prerequisites

- GNU Autotools (autoconf, automake, libtool)
- C compiler (gcc/clang)
- Optional: FLAC, MP3, Vorbis libraries for format support

### Build Instructions

See [INSTALL](INSTALL) for detailed instructions.

## Documentation

- [EXTREME_RESAMPLING_WITH_SOX_A_GUIDE_FOR_THE_PERPLEXED.md](EXTREME_RESAMPLING_WITH_SOX_A_GUIDE_FOR_THE_PERPLEXED.md) - Detailed parameter guide
- [LARGE_SINC_FILTERS_IMPLEMENTATION.md](LARGE_SINC_FILTERS_IMPLEMENTATION.md) - Technical implementation details
- [SOX_DSD_BASICS.md](SOX_DSD_BASICS.md) - DSD encoding primer and usage guide
- [UPSTREAM_TRACKING.md](UPSTREAM_TRACKING.md) - Guide for updating from upstream sox_ng releases
- [sox_ng README](https://codeberg.org/sox_ng/sox_ng) - Parent project documentation
- Original SoX documentation in man pages

## Summary

sox-sinc is a specialized tool that extends the already capable sox_ng resampling engine to support extremely large filter implementations. While standard sox provides excellent quality for virtually all practical applications, this build exists for those who wish to explore the theoretical limits of FIR filtering or who believe they require filter characteristics beyond what standard implementations provide.

Users should carefully consider whether the substantial increase in processing time is justified for their specific use case.

## License

This project maintains SoX's original dual license:
- GPL for the executables
- LGPL for the library

See [LICENSE.GPL](LICENSE.GPL) and [LICENSE.LGPL](LICENSE.LGPL) for details.

## Contributing

### Upstream First
For general improvements, bug fixes, and features that benefit all sox users, please contribute directly to the parent [sox_ng project](https://codeberg.org/sox_ng/sox_ng). This fork tracks sox_ng and will inherit those improvements.

### Fork-Specific Contributions
For contributions specific to extreme filtering capabilities, we welcome:
- Research and documentation on perceptual effects of extreme filters
- Optimizations for large-scale DFT operations
- Memory efficiency improvements for multi-million tap filters
- Benchmarking data and performance analysis
- Use case documentation from scientific or measurement applications

The goal is to maintain this as a focused, experimental platform while ensuring general improvements flow through the sox_ng ecosystem.

## Acknowledgments

- Original SoX team for the excellent foundation; Måns Rullgård, especially, for creating his DSD-enabled SoX fork
- [sox_ng maintainers](https://codeberg.org/sox_ng/sox_ng) for all the work that goes into enabling an actively maintained fork
- Nix for providing the foundation for reproducible build environments, Flox for making this just a _bit_ more accessible.
