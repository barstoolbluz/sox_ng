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
}:

stdenv.mkDerivation rec {
  pname = "sox_ng";
  version = "14.8.0.1-custom";

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

  ];

  postPatch = ''
    # ========================================================================
    # PHASE 1: Large Sinc Filter Support — UPSTREAM as of sox_ng 14.7.x
    # ========================================================================
    # The large-sinc feature this fork used to patch in (up to ~1 billion taps,
    # larger FFT4G_MAX_SIZE) is now native in upstream. Instead of patching, we
    # ASSERT the capability is present so a future upstream regression fails the
    # build loudly rather than silently shipping 32767-tap filters.

    echo "Verifying upstream large sinc filter support..."

    grep -q '11, 1073741823' src/sinc.c \
      || { echo "ERROR: expected large-tap sinc limit (1073741823) missing from src/sinc.c"; exit 1; }

    # FFT4G_MAX_SIZE must be well above the old 262144 default (upstream: 1U << 31).
    grep -q 'FFT4G_MAX_SIZE' src/fft4g.h && ! grep -q 'FFT4G_MAX_SIZE 262144' src/fft4g.h \
      || { echo "ERROR: FFT4G_MAX_SIZE appears to have regressed to the small default in src/fft4g.h"; exit 1; }

    echo "Upstream large sinc filter support confirmed."

    # ========================================================================
    # PHASE 2: sox_ng → sox Rebranding — UPSTREAM via --enable-replace
    # ========================================================================
    # Upstream now ships a first-class `--enable-replace` configure flag (see
    # configureFlags) that installs compatibility symlinks: sox->sox_ng,
    # play/rec/soxi, libsox.{a,la,so}, sox.h, sox.pc, and all man pages. This
    # replaces the fragile brute-force mv/sed renaming the fork used to do, which
    # broke every time upstream restructured the build. No source edits here.

    # ========================================================================
    # PHASE 3: Platform-Specific Fixes (Darwin, defensive)
    # ========================================================================
    # Upstream already fixed lsx_rawseek to sox_uint64_t; the remaining sed
    # passes are defensive no-ops that can never fail the build.

    ${lib.optionalString stdenv.isDarwin ''
      echo "Applying Darwin-specific type compatibility fixes..."

      # Fix uint64_t → sox_uint64_t in offset contexts (defensive)
      find src -name "*.c" -exec grep -l "uint64_t.*offset" {} \; | while read file; do
        sed -i 's/uint64_t \([A-Z_]*\) offset/sox_uint64_t \1 offset/g' "$file" || true
      done

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
    # Install `sox`/`play`/`rec`/`soxi`, libsox.*, sox.h and sox.pc compatibility
    # symlinks alongside the sox_ng originals (upstream's supported rebranding
    # mechanism; replaces the fork's old brute-force renaming in postPatch).
    "--enable-replace"
  ];

  enableParallelBuilding = true;

  postInstall = ''
    # With --enable-replace, `make install` already creates the sox/play/rec/soxi
    # (and libsox.*, sox.h, sox.pc) compatibility symlinks pointing at the sox_ng
    # originals. As a cross-platform safety net — play/rec are only auto-linked
    # when an audio backend is enabled — ensure the core aliases exist, pointing
    # at the real sox_ng binary. Idempotent: never clobbers existing symlinks.
    cd "$out/bin"
    for alias in sox play rec soxi; do
      [ -e "$alias" ] || ln -s sox_ng "$alias"
    done

    echo "Build complete!"
  '';

  meta = with lib; {
    description = "sox_ng with large sinc filter support (up to 1 billion taps)";
    longDescription = ''
      Custom build of sox_ng exposed as 'sox' for drop-in replacement of
      legacy SoX. Supports up to approximately 1 billion filter taps for
      extreme resampling scenarios.

      This build includes:
      - Large sinc filter support (up to ~1 billion taps; native upstream
        since sox_ng 14.7.x, FFT4G_MAX_SIZE 1<<31)
      - Full codec support via ffmpeg-full
      - DSD (Direct Stream Digital) audio support for high-resolution formats
      - 'sox'/'play'/'rec'/'soxi' compatibility symlinks via --enable-replace
    '';
    homepage = "https://github.com/barstoolbluz/sox_ng";
    license = with licenses; [ gpl2Plus lgpl21Plus ];
    platforms = platforms.unix;
    mainProgram = "sox";
  };
}
