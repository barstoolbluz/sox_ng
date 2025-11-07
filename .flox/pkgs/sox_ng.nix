{ stdenv
, lib
, fetchFromGitHub
, autoconf
, automake
, libtool
, pkg-config
, zlib
, libsndfile
, flac
, lame
, libmad
, libvorbis
, libogg
, opusfile
, gsm
, wavpack
, libid3tag
, file
, ffmpeg-full
, alsa-lib
, libpulseaudio
, ladspa-sdk
, darwin
}:

stdenv.mkDerivation rec {
  pname = "sox_ng";
  version = "14.6.1.2-custom";

  # Source is the current repository
  src = ../..;

  nativeBuildInputs = [
    autoconf
    automake
    libtool
    pkg-config
  ];

  buildInputs = [
    # Core libraries
    zlib
    libsndfile
    libtool          # Provides libltdl

    # Audio codecs
    flac
    lame
    libmad
    libvorbis
    libogg
    opusfile
    gsm
    wavpack

    # Metadata & utilities
    libid3tag
    file             # Provides libmagic

    # FFmpeg
    ffmpeg-full

  ] ++ lib.optionals stdenv.isLinux [
    # Linux audio I/O
    alsa-lib
    libpulseaudio
    ladspa-sdk

  ] ++ lib.optionals stdenv.isDarwin [
    # Darwin frameworks
    darwin.apple_sdk_11_0.frameworks.CoreFoundation
    darwin.apple_sdk_11_0.frameworks.IOKit
  ];

  postPatch = ''
    # ========================================================================
    # PHASE 1: Large Sinc Filter Patches
    # ========================================================================

    echo "Applying large sinc filter support patches..."

    # 1. Increase FFT4G_MAX_SIZE in fft4g.h
    substituteInPlace src/fft4g.h \
      --replace '#define FFT4G_MAX_SIZE 262144' \
                '#define FFT4G_MAX_SIZE 1073741824'

    # 2. Increase ip array sizes in fft4g.c
    substituteInPlace src/fft4g.c \
      --replace 'int j, j1, k, k1, l, m, m2, ip[256];' \
                'int j, j1, k, k1, l, m, m2, ip[16384];'

    # 3. Increase sinc filter tap limits
    substituteInPlace src/sinc.c \
      --replace "GETOPT_LOCAL_NUMERIC(optstate, 'n', taps, 11, 32767)" \
                "GETOPT_LOCAL_NUMERIC(optstate, 'n', taps, 11, 1073741823)" \
      --replace "GETOPT_NUMERIC(optstate, 'n', num_taps[1], 11, 32767)" \
                "GETOPT_NUMERIC(optstate, 'n', num_taps[1], 11, 1073741823)" \
      --replace '*num_taps = range_limit(n, 11, 32767);' \
                '*num_taps = range_limit(n, 11, 1073741823);'

    echo "Large sinc filter patches applied successfully!"

    # ========================================================================
    # PHASE 2: sox_ng → sox Rebranding
    # ========================================================================

    echo "Renaming sox_ng to sox throughout the codebase..."

    # A. Build System Files
    substituteInPlace configure.ac \
      --replace 'sox_ng' 'sox'

    substituteInPlace CMakeLists.txt \
      --replace 'sox_ng' 'sox'

    substituteInPlace src/CMakeLists.txt \
      --replace 'sox_ng' 'sox'

    substituteInPlace Makefile.am \
      --replace 'sox_ng' 'sox' \
      --replace 'soxi_ng' 'soxi' \
      --replace 'soxformat_ng' 'soxformat' \
      --replace 'libsox_ng' 'libsox' \
      --replace 'play_ng' 'play' \
      --replace 'rec_ng' 'rec'

    substituteInPlace src/Makefile.am \
      --replace 'sox_ng' 'sox' \
      --replace 'libsox_ng' 'libsox'

    # Optional formats file (if exists)
    if [ -f src/optional-fmts.am ]; then
      substituteInPlace src/optional-fmts.am \
        --replace 'sox_ng' 'sox' \
        --replace 'libsox_ng' 'libsox'
    fi

    # pkg-config file (rename + patch)
    if [ -f sox_ng.pc.in ]; then
      mv sox_ng.pc.in sox.pc.in
      substituteInPlace sox.pc.in \
        --replace 'sox_ng' 'sox' \
        --replace 'libsox_ng' 'libsox'
    fi

    # B. Source File Renaming
    if [ -f src/sox_ng.h ]; then
      mv src/sox_ng.h src/sox.h
    fi
    if [ -f src/sox_ng.c ]; then
      mv src/sox_ng.c src/sox.c
    fi
    if [ -f src/libsox_ng.c ]; then
      mv src/libsox_ng.c src/libsox.c
    fi

    # Update all includes in source files
    find src -type f \( -name "*.c" -o -name "*.h" \) -exec \
      sed -i 's/sox_ng\.h/sox.h/g; s/libsox_ng/libsox/g' {} +

    # C. Documentation File Renaming
    if [ -f sox_ng.1 ]; then
      mv sox_ng.1 sox.1
      substituteInPlace sox.1 \
        --replace 'sox_ng' 'sox' \
        --replace 'play_ng' 'play' \
        --replace 'rec_ng' 'rec'
    fi

    if [ -f soxi_ng.1 ]; then
      mv soxi_ng.1 soxi.1
      substituteInPlace soxi.1 \
        --replace 'sox_ng' 'sox' \
        --replace 'soxi_ng' 'soxi'
    fi

    if [ -f soxformat_ng.7 ]; then
      mv soxformat_ng.7 soxformat.7
      substituteInPlace soxformat.7 \
        --replace 'sox_ng' 'sox' \
        --replace 'soxformat_ng' 'soxformat'
    fi

    if [ -f libsox_ng.3 ]; then
      mv libsox_ng.3 libsox.3
      substituteInPlace libsox.3 \
        --replace 'sox_ng' 'sox' \
        --replace 'libsox_ng' 'libsox'
    fi

    # D. Test Files
    find test -type f \( -name "*.sh" -o -name "*.test" \) -exec \
      sed -i 's/sox_ng/sox/g; s/soxi_ng/soxi/g; s/play_ng/play/g; s/rec_ng/rec/g' {} + \
      2>/dev/null || true

    # E. Build Scripts
    for script in mingwbuild osxbuild release.sh; do
      if [ -f "$script" ]; then
        substituteInPlace "$script" \
          --replace 'sox_ng' 'sox' \
          --replace 'soxi_ng' 'soxi' \
          --replace 'libsox_ng' 'libsox' \
          --replace 'play_ng' 'play' \
          --replace 'rec_ng' 'rec'
      fi
    done

    echo "Renaming complete!"

    # ========================================================================
    # PHASE 3: Platform-Specific Fixes
    # ========================================================================

    ${lib.optionalString stdenv.isDarwin ''
      echo "Applying Darwin-specific type compatibility fixes..."

      # Fix uint64_t → sox_uint64_t in offset contexts (defensive)
      find src -name "*.c" -exec grep -l "uint64_t.*offset" {} \; | while read file; do
        sed -i 's/uint64_t \([A-Z_]*\) offset/sox_uint64_t \1 offset/g' "$file" || true
      done

      # Fix lsx_rawseek signatures (defensive - may already be correct)
      sed -i 's/int lsx_rawseek(sox_format_t \* ft, uint64_t offset)/int lsx_rawseek(sox_format_t * ft, sox_uint64_t offset)/g' \
        src/sox_i.h src/raw.c 2>/dev/null || true

      # Fix function pointer types (defensive)
      find src -name "*.c" -exec grep -l "uint64_t" {} \; | while read file; do
        sed -i 's/(\(sox_format_t[^,]*\), uint64_t/(\1, sox_uint64_t/g' "$file" || true
      done

      echo "Darwin fixes applied!"
    ''}
  '';

  preConfigure = ''
    echo "Regenerating build system..."
    autoreconf -fiv
  '';

  configureFlags = [
    "--enable-shared"
    "--enable-static"
    "--with-ffmpeg"
  ];

  enableParallelBuilding = true;

  postInstall = ''
    echo "Fixing symlinks in $out/bin..."
    cd $out/bin

    # Remove any remaining *_ng symlinks and create correct ones
    for prog in play rec soxi; do
      rm -f ''${prog}_ng
      if [ ! -e $prog ]; then
        ln -s sox $prog
      fi
    done

    echo "Build complete!"
  '';

  meta = with lib; {
    description = "sox_ng with large sinc filter support (up to 1 billion taps)";
    longDescription = ''
      Custom build of sox_ng with enhanced sinc filter support and renamed
      to 'sox' for drop-in replacement of legacy SoX. Supports up to
      approximately 1 billion filter taps for extreme resampling scenarios.

      This build includes:
      - Large sinc filter patches (FFT size up to 2^30)
      - Full codec support via ffmpeg-full
      - DSD (Direct Stream Digital) audio support for high-resolution formats
      - Renamed binaries: sox, play, rec, soxi
    '';
    homepage = "https://github.com/barstoolbluz/sox_ng";
    license = with licenses; [ gpl2Plus lgpl21Plus ];
    platforms = platforms.unix;
    mainProgram = "sox";
  };
}
