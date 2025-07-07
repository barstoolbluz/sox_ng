# README.md

**SoX_ng** is a feature-enhanced fork of the venerable SoX (Sound eXchange) audio processing toolkit. While maintaining full compatibility with the original sox-14.4.2, this "next generation" fork adds significant new capabilities and addresses longstanding issues.

## Why SoX_ng?

The `SoX_ng` project imports, compares and refines bug fixes and
new work from the 50-odd software distributions that package SoX
and from the plethora of forks on github and elsewhere
and makes regular releases with a six-monthly cadence
for each of the micro (bug fixes) and minor (new features) releases.
A major release (non-backwards-compatible changes) is not planned.

The next micro release is scheduled for the 18th August 2025.<BR>
The next minor release is scheduled for the 18th November 2025.

## Key Features and Improvements over Original SoX

### 🎵 Native DSD (Direct Stream Digital) Support
- **New format support**: DSF (DSD Stream File), DSDIFF, and WSD files for high-resolution 1-bit audio
- **DSD over PCM (DoP)**: Transport DSD data over standard PCM interfaces
- **Sigma-Delta Modulation**: New `sdm` effect with multiple advanced modulators:
  - CLANS (Closed Loop Analysis of Noise Shapers) - optimized for stability
  - SDM (Standard Delta-Sigma Modulation) - conventional approach
  - CRFB (Cascade of Resonators with distributed FeedBack) - alternative design
- **Trellis Optimization**: High-quality encoding with configurable lookahead
- **Multiple DSD Rates**: DSD64 (2.8MHz), DSD128 (5.6MHz), DSD256 (11.3MHz), DSD512 (22.6MHz)

### 🛡️ Security and Stability
- **42 CVEs fixed** (2004-2023) with comprehensive test coverage
- **13 critical bugs resolved** that affected the original SoX
- Memory leak fixes and improved boundary checking
- Enhanced error handling throughout the codebase

### 🚀 Enhanced Capabilities
- **Extended format support**: 48+ additional audio/video formats via ffmpeg integration
- **Multi-threading support** for improved performance on modern systems
- **Reproducible builds** with deterministic output
- **NSP format** file reading capability

### 🔧 Better Maintenance
- Regular release schedule with predictable updates
- Systematic integration of fixes from 50+ downstream distributions
- Active consolidation of improvements from community forks
- Comprehensive test suite for regression prevention

## Terminology

`sox` means [sox.sf.net](http://sox.sf.net)<BR>
`SoX` means the Swiss Army Knife of command-line audio processing in any of its incarnations<BR>
`sox_ng` means the hard fork of `sox-14.4.2` aiming to sanitize `SoX`<BR>
`SoX_ng` means the project to maintain `sox_ng`

## How to get it

### Official Repository
`sox_ng` lives at
[codeberg.org/sox_ng/sox_ng](https://codeberg.org/sox_ng/sox_ng)
and is composed of a SoX code base, a wiki and an issue tracker.

### Flox-Enabled Fork
For a version with built-in Flox build support, you can also get `sox_ng` from:
[github.com/barstoolbluz/sox_ng](https://github.com/barstoolbluz/sox_ng)

This fork includes pre-configured Flox environments for easy building and installation.

### Releases

Download one of the
[release tarballs](https://codeberg.org/sox_ng/sox_ng/releases).

Extract it:
```
gzip -d < sox_ng-*.tar.gz | tar xf -
```
Build it:
```
cd sox_ng*
./configure
make
```
Install it:
```
make install
```

It installs as `sox_ng`, `sox_ng.h`, `libsox_ng.so` and so on
so that `sox` and `sox_ng` can coexist on the same system.
To make it work the same as the original `sox`, use
`./configure --enable-replace`

### Development branches

#### main

To fetch the latest version:
```
git clone https://codeberg.org/sox_ng/sox_ng
cd sox_ng
```
and to make local copies of the wiki and the issues:
```
git clone https://codeberg.org/sox_ng/sox_ng.wiki wiki
issues/getissues.sh
```

To compile it:
```
autoreconf -i
./configure
make
```
and to install it:
```
sudo make install
```
This installs it as `sox_ng`, `sox_ng.h`, `libsox_ng` and so on,
so that it can coexist with traditional `sox`. If you want it to work
the same as the original `sox`, use `./configure --enable-replace`
and if `ffmpeg` is installed add `--with-ffmpeg` to decode 48 more
audio and video formats.

## Building with Flox (Recommended)

The easiest way to build SoX_ng is using [Flox](https://flox.dev/), which provides a reproducible build environment with all necessary dependencies automatically managed.

### Prerequisites

1. Install Flox:
   ```bash
   curl -L https://flox.dev/install | sh
   ```

2. Clone the repository:
   ```bash
   # Option A: Clone from the Flox-enabled fork (recommended for Flox builds)
   git clone https://github.com/barstoolbluz/sox_ng
   cd sox_ng
   
   # Option B: Clone from the official repository
   git clone https://codeberg.org/sox_ng/sox_ng
   cd sox_ng
   ```

### Build Instructions

The repository includes a pre-configured Flox environment. Simply run:

```bash
# Activate the Flox environment
flox activate

# Build SoX_ng
flox build sox

# The built binary will be available at:
./result-sox/bin/sox_ng
```

The Flox environment automatically handles:
- Platform-specific dependencies (Linux/macOS)
- All required audio codec libraries (FLAC, LAME, Vorbis, Opus, etc.)
- Build tools (autoconf, automake, libtool)
- Audio I/O libraries (ALSA, PulseAudio on Linux; CoreAudio on macOS)

### Installing SoX_ng via Flox

#### Option 1: Local Installation

For a portable, self-contained installation:

```bash
./local-install  # Installs to $HOME/.local/sox_ng
```

Or to a custom location:
```bash
./local-install /path/to/installation
```

After installation, add to your PATH:
```bash
export PATH="$HOME/.local/sox_ng/bin:$PATH"
```

#### Option 2: Publish to Flox Catalog

You can publish SoX_ng to your private Flox Catalog for easy distribution:

```bash
# Ensure git is clean and pushed
git status  # Should show "working tree clean"
git push

# Publish to your catalog
flox publish -o <your-floxhub-handle> sox

# Then install anywhere with:
flox install <your-handle>/sox_ng
```

## Build dependencies (Traditional Method)

To compile a release tarball you will need `make`, `libtool`
and `gcc` or `clang` (`./configure CC=clang`)

To build the git repository you will also need `autoconf` and `automake`.

To enable all of SoX's optional modules you can install
`ladspa-sdk`
`lame`,
`libao`,
`libfftw3`,
`libflac`,
`libid3tag`,
`libmad`
`libogg`,
`libopus`,
`libopusfile`,
`libpng`,
`libsndfile`,
`libspeex`,
`libspeexdsp`,
`libvorbis`,
`opencore-amr`,
`twolame`,
`wavpack`.

### Debian, Ubuntu, Mint etc.
```
apt install gcc make libtool ladspa-sdk libao-dev libasound2-dev libfftw3-dev \
	libgsm1-dev libid3tag0-dev libltdl-dev libmad0-dev libmagic-dev \
	libmp3lame-dev libopencore-amrnb-dev libopencore-amrwb-dev \
	libopusfile-dev libpng-dev libpulse-dev libsamplerate0-dev \
	libsndfile1-dev libspeex-dev libspeexdsp-dev libtwolame-dev \
	libvorbis-dev libwavpack-dev
```
and to run `issues/getissues.sh` and the `makehtml.sh` scripts you will need
```
apt-get install jq libtext-multimarkdown-perl
```

### Fedora, Red Hat, CentOS etc.
```
yum install gcc make libtool \
	alsa-lib-devel fftw-devel file-devel flac-devel gsm-devel \
	ladspa-devel lame-devel libao-devel libcaca-devel libid3tag-devel \
	libmad-devel libpng-devel libsamplerate-devel libsndfile-devel \
	libtool-ltdl-devel libvorbis-devel opencore-amr-devel \
	opusfile-devel pulseaudio-libs-devel speex-devel speexdsp-devel \
	twolame-devel wavpack-devel
```
and to run `issues/getissues.sh` and the `makehtml.sh` scripts you will need
```
yum install jq multimarkdown
```

## Accessibility

You can edit and commit to the code and the wiki using Codeberg's web interface
or from the command-line.
The command-line is the only way to add images and attachments to the wiki.

The issues can be downloaded as well as uploaded replacing all remote content
but this risks overwriting changes made via the web interface so for now
it is recommended to edit them on the Codeberg web site.

## Using DSD Features

### Basic DSD Encoding

Convert PCM to DSD64:
```bash
sox_ng input.wav output.dsf rate -v 2822400 sdm -f clans-8
```

Convert PCM to DSD128:
```bash
sox_ng input.wav output.dsf rate -v 5644800 sdm -f clans-7
```

### Advanced Encoding with Trellis Optimization

For higher quality encoding:
```bash
# High-quality encoding with trellis optimization
sox_ng input.wav output.dsf rate -v 2822400 sdm -f clans-8 -t 32 -n 32
```

Trellis parameters:
- `-t N`: Lookahead depth (1-64)
- `-n N`: Number of trellis nodes (1-32+)
- `-l N`: Trellis latency in samples (optional)

### Recommended Settings by DSD Rate

| DSD Rate | Sample Rate | Recommended Modulator | Alternative |
|----------|-------------|----------------------|-------------|
| DSD64    | 2.8224 MHz  | CLANS-8             | SDM-8       |
| DSD128   | 5.6448 MHz  | CLANS-7             | SDM-7       |
| DSD256   | 11.2896 MHz | CLANS-6             | SDM-6       |
| DSD512   | 22.5792 MHz | CLANS-5             | SDM-5       |

### Format Conversions

DSD to PCM:
```bash
sox_ng input.dsf output.wav rate -v 96000 gain +2.5
```

DSF to DSDIFF:
```bash
sox_ng input.dsf output.dff
```

DSD over PCM (DoP):
```bash
sox_ng input.dsf output.dop
```

### Signal Level Standards

DSD uses different level references than PCM:
- **0dBDSD**: 50% modulation, equivalent to -6dBFS PCM
- **+3.1dBDSD**: SACD peak limit (71.43% modulation, -2.9dBFS PCM)

## Community

### Mailing list

* [sox-ng@groups.io](https://groups.io/g/sox-ng)<BR>
  To subscribe, go there or send an email to
  [sox-ng+subscribe@groups.io](mailto:sox-ng+subscribe@groups.io)

### Private email

* [sox_ng@fastmail.com](mailto:sox_ng@fastmail.com)

### Finances

* [`SoX_ng`'s financial accounts](Accounting) are public and
* [Bounties](Bounties) can be offered for specific issues to be resolved

## README and PDFs

To generate SoX's original README, run `./README.sh` or `make README`

To generate the PDF version of the documentation, `make pdf`
