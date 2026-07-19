# Building SoX_ng with CMake

sox_ng can be built with CMake by using the files in the
`cmake-buildsetup` directory.

By this, CMake can produce Microsoft Build files (for Visual Studio),
make files or files for other build platforms.

**Not that this CMake production line is not intended to replace the
existing autoconf based build (which also takes care of
cross-compilation and also exotic platforms). Instead it is a variant
for directly supporting MS Visual Studio, XCode and Linux builds with
the CMake supported generators (like MSBuild, make, ninja etc.).**

The previously provided msvc files for Windows as well as the
CMakeLists.txt files in several directories were removed from SoX_ng,
because CMake produces those build files from a single configuration
setup and also delivers equivalent files but with a more modern
syntax.

## Requirements

You have to have CMake in a current version (&gt;4.0) available on
your machine.

## Setup

You have to install the sox_ng project from the
[Codeberg][Codeberg_SoX] repository into some directory.

Ideally the following additional projects are installed alongside
sox_ng.

  - [fftw][libfftw]
  - [flac][libflac]
  - [libid3tag][libid3tag]
  - [libmad][libmad]
  - [libmp3lame][libmp3lame]
  - [libogg][libogg]
  - [libopus][libopus]
  - [libopusfile][libopusfile]
  - [libpng][libpng]
  - [libsndfile][libsndfile]
  - [libspeex][libspeex]
  - [libspeexdsp][libspeexdsp]
  - [libvorbis][libvorbis]
  - [libwavpack][libwavpack]
  - [libz][libz]

Hence the recommended directory structure is as follows (also with
some minor renaming of the library directories):

  - «code_directory»
    - libfftw
    - libflac
    - libid3tag
    - libmad
    - libmp3lame
    - libogg
    - libopus
    - libpng
    - libsndfile
    - libspeex
    - libspeexdsp
    - libvorbis
    - libwavpack
    - libz
    - sox_ng
      - cmake-buildsetup
      - ...

If the directory structure differs from that, you can adapt the file
`LocalSettings.cmake` in the `cmake-buildsetup` directory.  There
the paths to the subprojects are defined.

If some support library is missing, the corresponding path should be
set to empty.  Normally then that functionality is left out of SoX_ng.
But the build configuration process tries to find installed libraries
on the host system and probes them for specific functionality.  If
successful, that installed library is used instead of the missing
library source.

Libraries provides in source form are preferred to installed
libraries.


## Processing

CMake does an out-of-source build in a special build directory and
does not at all alter the source directories.

### Generation of the Build Files

The build files are to be generated in some build directory
which has to be created.  So the process for those is as
follows:

  1. Create the build directory e.g. by:

     mkdir ~/ng_build

  2. Edit the file `cmake-buildsetup/LocalSettings.cmake`
     to reflect the local environment.

  3. For MacOS, ensure that the enviroment variable
     `SDKROOT` is set accordingly, e.g. to

     /Library/Developer/CommandLineTools/SDKs/MacOSX-version.sdk

  4. Generate the build files:

     - If you have a multi-config build system (like e.g. msbuild or
       ninja), you can just generate the build files:

       cmake -S path_to_sox_ng/cmake-buildsetup -B build_directory

     - If you have a single-config build system (like e.g. make), you
       must specify the build type (Debug or Release), you generate
       the build files by specifying the build type:

       cmake -S path_to_sox_ng/cmake-buildsetup \
	     -B build_directory \
	     -DCMAKE_BUILD_TYPE=build_type

The setup can be changed by overriding default settings in the cmake
step.  Note that some configuration variables for the standard build
process are missing; **the current CMake setup only allows for a build
of libSoX and SoX with static libraries coming from explicit source
directories alongside SoX_ng.**

The following variables can be set with `-Dwhatever=true` or `-Dwhatever=value`:

<SMALL font-size: x-small>

| **Variable** | **Description** | **Default Value** |
|:-------------|:----------------|:------------------|
| **disable_symlinks** | set flag whether to make any symlinks to sox_ng | FALSE |
| **enable_dl_amrnb** | set flag whether to use dynamic version of amrnb library (instead of static) | FALSE |
| **enable_dl_amrwb** | set flag whether to use dynamic version of amrwb library (instead of static) | FALSE |
| **enable_dl_lame** | set flag whether to use dynamic version of lame library (instead of static) | FALSE |
| **enable_dl_mad** | set flag whether to use dynamic version of mad library (instead of static) | FALSE |
| **enable_dl_sndfile** | set flag whether to use dynamic library version of sndfile library (instead of static) | FALSE |
| **enable_dl_speexdsp** | set flag whether to use dynamic library version of speexdsp library (instead of static) | FALSE |
| **enable_dl_twolame** | set flag whether to use dynamic library version of twolame library (instead of static) | FALSE |
| **enable_replace** | make links from "sox" to "sox_ng" and so on | FALSE |
| **install_bin_path** | the target path for the generated exe files | $CMAKE_INSTALL_PREFIX/bin |
| **install_lib_path** | the target path for the generated lib files | $CMAKE_INSTALL_PREFIX/lib |
| **install_doc_path** | the target path for the generated documentation files | $CMAKE_INSTALL_PREFIX/doc |
| **install_man_path** | the target path for the generated man files | $CMAKE_INSTALL_PREFIX/man |
| **list_build_variable_values** | shows values of relevant build variables in CMake build log | FALSE |
| **with_curl** | set flag whether to prefer curl to wget | FALSE |
| **with_dyn_default** | set default to loading optional formats dynamically | FALSE |
| **with_ffmpeg** | set flag whether to use ffmpeg to decode otherwise unsupported formats | FALSE |
| **with_ladspa_path** | default search path for LADSPA plugins | ${libdir}/ladspa |
| **with_pkgconfigdir** | location to install .pc files or "no" to disable | ${libdir}/pkgconfig |
| **without_dolbyb** | set flag whether to omit the dolbyb library | FALSE |
| **without_ebur128** | set flag whether to omit the EBU R128 library | FALSE |
| **without_fftw** | set flag whether to omit the fftw library | FALSE |
| **without_flac** | set flag whether to omit the flac library (command line parameter not available in SoX_ng so far) | FALSE |
| **without_gsm** | set flag whether to omit the gsm library (command line parameter not available in SoX_ng so far) | FALSE |
| **without_id3tag** | set flag whether to omit the id3tag library | FALSE |
| **without_ladspa** | set flag whether to omit the ladspa library | FALSE |
| **without_lame** | set flag whether to omit the LAME library (LAME Ain't an MP3 Encoder) | FALSE |
| **without_libltdl** | set flag whether to omit the libltdl library | FALSE |
| **without_lpc10** | set flag whether to omit the lpc10 library (command line parameter not available in SoX_ng so far) | FALSE |
| **without_mad** | set flag whether to omit the MAD (MP3 Audio Decoder) library | FALSE |
| **without_magic** | set flag whether to omit the magic library | FALSE |
| **without_opus** | set flag whether to omit the opus and opusfile libraries | FALSE |
| **without_png** | set flag whether to omit the png library | FALSE |
| **without_sndfile** | set flag whether to omit the sndfile library (command line parameter not available in SoX_ng so far) | FALSE |
| **without_speexdsp** | set flag whether to omit the speexdsp library | FALSE |
| **without_twolame** | set flag whether to omit the twolame library | FALSE |
| **without_wavpack** | set flag whether to omit the wavpack library (command line parameter not available in SoX_ng so far) | FALSE |
| **without_z** | set flag whether to omit the z library (command line parameter not available in SoX_ng so far) | FALSE |

</SMALL>


### Build

The build process is initiated by the command

    cmake --build build_directory

If you intend to build a specific target, the command is

    cmake --build build_directory --target target

where target is one of the following:

  - **ALL:** the complete build
      - **SupportTools:** the supporting libraries
	- **libFFTW:** the fftw library
	- **libFlac:** the flac library
	- **libId3tag:** the libid3tag library
	- **libMad:** the libmad library
	- **libMp3Lame:** the libmp3lame library
	- **libOgg:** the libogg library
	- **libOpus:** the libopus library
	- **libOpusfile:** the libopusfile library
	- **libPng:** the libpng library
	- **libSndfile:** the libsndfile library
	- **libSpeex:** the libspeex library
	- **libSpeexDSP:** the libspeexdsp library
	- **libVorbis:** the libvorbis library
	- **libWavpack:** the libwavpack library
	- **libZ:** the libz library
      - **libSoX:** the SoX_ng library
      - **libSoXDocumentation:** the doxygen documentation
	for the libSoX library
      - **SoX:** the SoX_ng executable
      - **SoXExamples:** the example programs for SoX
	- **SoXExample_0**
	- **...**
	- **SoXExample_6**


### Installation

The installation process is initiated by the command

`cmake --install «build_directory»`

After that

  - the static SoX library `libsox_ng` will be in the library
    directory,

  - the doxygen library documentation for SoX will be in the
    documentation directory,

  - the executable `sox_ng` will be in the binary directory
    (together with `soxi_ng`, `play_ng` and `rec_ng`),

  - if the configuration variable `enable_replace` is set, the
    executables `sox`, `soxi`, `play` and `rec` will also be in the
    binary directory and the SoX library can also be accessed as
    `libsox`,

  - the test executables `sox_ng_example1` to `sox_ng_example6` will
    be in the binary directory, and

  - if available, the man pages will be in the manual directory.

Note that you have to have write access to the destination
directories. If those are system directories, the installation
command must be run with administrator rights (e.g. by
specifying `sudo cmake ...`).

## Acknowledgements

SOX_NG.EXE included in this package makes use of the following
projects.

  - SoX_ng - [https://codeberg.org/sox_ng/sox_ng][Codeberg_SoX]
  - CMake - [https://cmake.org][CMake]
  - FFMPEG - [https://ffmpeg.org][ffmpeg]
  - FFTW - [http://fftw.org][libfftw]
  - FLAC - [https://github.com/xiph/flac][libflac]
  - LADSPA - [http://www.ladspa.org][ladspa]
  - LAME - [https://lame.sourceforge.io][libmp3lame]
  - libid3tag - [https://codeberg.org/tenacityteam/libid3tag][libid3tag]
  - libmad - [https://www.underbit.com/products/mad][libmad]
  - libsndfile - [https://github.com/libsndfile/libsndfile][libsndfile]
  - Ogg - [https://www.xiph.org/ogg][libogg]
  - Opus - [https://www.xiph.org/opus][libopus]
  - Opusfile - [https://www.xiph.org/opusfile][libopusfile]
  - PNG - [http://www.libpng.org/pub/png][libpng]
  - speex - [https://github.com/xiph/speex][libspeex]
  - speexdsp - [https://github.com/xiph/speexdsp][libspeexdsp]
  - Vorbis - [http://www.vorbis.com][libvorbis]
  - WavPack - [https://github.com/dbry/WavPack][libwavpack]
  - ZLib - [https://www.zlib.net][libz]

<!-- -------------------- -->

[CMake]: https://cmake.org
[Codeberg_SoX]: https://codeberg.org/sox_ng/sox_ng
[ffmpeg]: https://ffmpeg.org
[ladspa]: http://www.ladspa.org
[libfftw]: http://fftw.org
[libflac]: https://github.com/xiph/flac
[libid3tag]: https://codeberg.org/tenacityteam/libid3tag
[libmad]: https://www.underbit.com/products/mad
[libmp3lame]: https://lame.sourceforge.io
[libogg]: https://www.xiph.org/ogg
[libopus]: https://www.xiph.org/opus
[libopusfile]: https://www.xiph.org/opusfile
[libpng]: http://www.libpng.org/pub/png
[libsndfile]: https://github.com/libsndfile/libsndfile
[libspeex]: https://github.com/xiph/speex
[libspeexdsp]: https://github.com/xiph/speexdsp
[libvorbis]: http://www.vorbis.com
[libwavpack]: https://github.com/dbry/WavPack
[libz]: https://www.zlib.net
