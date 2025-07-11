{
  description = "sox-sinc: sox_ng with large sinc filter support";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs {
          inherit system;
          config = {
            allowUnfree = true;
          };
        };
        
        # Build ffmpeg-full with unfree codecs
        ffmpeg-unfree = pkgs.ffmpeg-full.override {
          withUnfree = true;
        };
        
        # Extract the build script from the manifest
        buildScript = pkgs.writeScript "build-sox-sinc" ''
          #!${pkgs.bash}/bin/bash
          set -euo pipefail
          
          # Apply large sinc filter patch
          echo "Applying large sinc filter support patch..."
          
          # 1. Increase FFT4G_MAX_SIZE in fft4g.h
          echo "Patching fft4g.h to increase FFT4G_MAX_SIZE..."
          sed -i 's/#define FFT4G_MAX_SIZE 262144/#define FFT4G_MAX_SIZE 1073741824/' src/fft4g.h
          
          # 2. Increase ip array sizes in fft4g.c
          echo "Patching fft4g.c to increase ip array sizes..."
          sed -i 's/int j, j1, k, k1, l, m, m2, ip\[256\];/int j, j1, k, k1, l, m, m2, ip[16384];/' src/fft4g.c
          
          # 3. Increase sinc filter tap limits
          echo "Patching sinc.c to increase tap limits..."
          sed -i "s/GETOPT_NUMERIC(optstate, 'n', num_taps\[1\], 11, 32767)/GETOPT_NUMERIC(optstate, 'n', num_taps[1], 11, 1073741823)/" src/sinc.c
          sed -i 's/\*num_taps = range_limit(n, 11, 32767);/*num_taps = range_limit(n, 11, 1073741823);/' src/sinc.c
          
          echo "Large sinc filter patch applied successfully!"
          
          # Now apply the standard sox_ng -> sox renaming
          sed -i 's/sox_ng/sox/g' configure.ac
          
          sed -i 's/sox_ng/sox/g' CMakeLists.txt
          sed -i 's/sox_ng/sox/g' src/CMakeLists.txt
          
          sed -i 's/sox_ng/sox/g' Makefile.am
          sed -i 's/soxi_ng/soxi/g' Makefile.am
          sed -i 's/soxformat_ng/soxformat/g' Makefile.am
          sed -i 's/libsox_ng/libsox/g' Makefile.am
          sed -i 's/play_ng/play/g' Makefile.am
          sed -i 's/rec_ng/rec/g' Makefile.am
          sed -i 's/sox_ng/sox/g' src/Makefile.am
          sed -i 's/libsox_ng/libsox/g' src/Makefile.am
          
          if [ -f src/optional-fmts.am ]; then
              sed -i 's/sox_ng/sox/g' src/optional-fmts.am
              sed -i 's/libsox_ng/libsox/g' src/optional-fmts.am
          fi
          
          sed -i 's/libsox_i\.c/libsox_i.c/g' src/Makefile.am
          sed -i 's/libsox_i\.c/libsox_i.c/g' src/optional-fmts.am 2>/dev/null || true
          
          if [ -f sox_ng.pc.in ]; then
              mv sox_ng.pc.in sox.pc.in
              sed -i 's/sox_ng/sox/g' sox.pc.in
              sed -i 's/libsox_ng/libsox/g' sox.pc.in
          fi
          
          if [ -f src/sox_ng.h ]; then
              mv src/sox_ng.h src/sox.h
          fi
          if [ -f src/sox_ng.c ]; then
              mv src/sox_ng.c src/sox.c
          fi
          if [ -f src/libsox_ng.c ]; then
              mv src/libsox_ng.c src/libsox.c
          fi
          
          find src -name "*.c" -o -name "*.h" | xargs sed -i 's/sox_ng\.h/sox.h/g'
          find src -name "*.c" -o -name "*.h" | xargs sed -i 's/libsox_ng/libsox/g'
          find src -name "*.c" -o -name "*.h" | xargs sed -i 's/"libsox\.h"/"libsox.h"/g'
          find src -name "*.c" -o -name "*.h" | xargs sed -i 's/<libsox\.h>/<libsox.h>/g'
          find src -name "*.c" -o -name "*.h" | xargs sed -i 's/libsox_i/libsox_i/g'
          
          if [ -f sox_ng.1 ]; then
              mv sox_ng.1 sox.1
              sed -i 's/sox_ng/sox/g' sox.1
              sed -i 's/play_ng/play/g' sox.1
              sed -i 's/rec_ng/rec/g' sox.1
          fi
          if [ -f soxi_ng.1 ]; then
              mv soxi_ng.1 soxi.1
              sed -i 's/sox_ng/sox/g' soxi.1
              sed -i 's/soxi_ng/soxi/g' soxi.1
          fi
          if [ -f soxformat_ng.7 ]; then
              mv soxformat_ng.7 soxformat.7
              sed -i 's/sox_ng/sox/g' soxformat.7
              sed -i 's/soxformat_ng/soxformat/g' soxformat.7
          fi
          if [ -f libsox_ng.3 ]; then
              mv libsox_ng.3 libsox.3
              sed -i 's/sox_ng/sox/g' libsox.3
              sed -i 's/libsox_ng/libsox/g' libsox.3
          fi
          
          find test -name "*.sh" -o -name "*.test" | xargs sed -i 's/sox_ng/sox/g' 2>/dev/null || true
          find test -name "*.sh" -o -name "*.test" | xargs sed -i 's/soxi_ng/soxi/g' 2>/dev/null || true
          find test -name "*.sh" -o -name "*.test" | xargs sed -i 's/play_ng/play/g' 2>/dev/null || true
          find test -name "*.sh" -o -name "*.test" | xargs sed -i 's/rec_ng/rec/g' 2>/dev/null || true
          
          for script in mingwbuild osxbuild release.sh; do
              if [ -f "$script" ]; then
                  sed -i 's/sox_ng/sox/g' "$script"
                  sed -i 's/soxi_ng/soxi/g' "$script"
                  sed -i 's/libsox_ng/libsox/g' "$script"
                  sed -i 's/play_ng/play/g' "$script"
                  sed -i 's/rec_ng/rec/g' "$script"
              fi
          done
          
          if ! grep -q "math.h" src/sox_sample_test.h; then
              echo "Adding math.h include to src/sox_sample_test.h"
              sed -i '/^#include <assert.h>$/a #include <math.h>' src/sox_sample_test.h
          else
              echo "math.h include already present"
          fi
        '';

        sox-sinc = pkgs.stdenv.mkDerivation rec {
          pname = "sox-sinc";
          version = "14.6.0-sinc";
          
          src = ./.;
          
          nativeBuildInputs = with pkgs; [
            autoconf
            automake
            libtool
            pkg-config
          ];
          
          buildInputs = with pkgs; [
            zlib
            libsndfile
            libtool  # for libltdl
            flac
            lame
            libmad
            libvorbis
            libogg
            opusfile
            gsm
            wavpack
            libid3tag
            file  # for libmagic
            ffmpeg-unfree  # ffmpeg-full with unfree codecs
          ] ++ lib.optionals stdenv.isLinux [
            alsa-lib
            libpulseaudio
            ladspa-sdk
          ] ++ lib.optionals stdenv.isDarwin [
            darwin.apple_sdk.frameworks.CoreFoundation
            darwin.apple_sdk.frameworks.IOKit
            darwin.apple_sdk.frameworks.CoreAudio
          ];
          
          preConfigure = ''
            # Apply patches
            ${buildScript}
            
            # Run autoreconf
            autoreconf -fiv
          '';
          
          configureFlags = [
            "--with-ffmpeg"
          ];
          
          postInstall = ''
            # Create compatibility symlinks
            cd $out/bin
            for prog in play rec soxi; do
              if [ -L "''${prog}_ng" ]; then
                rm "''${prog}_ng"
                ln -s sox $prog
              fi
            done
          '';
          
          meta = with pkgs.lib; {
            description = "sox_ng with support for extremely large sinc filters and full ffmpeg codec support";
            longDescription = ''
              A specialized build of sox_ng that removes the standard limitation
              on sinc filter lengths, supporting up to approximately 1 billion taps
              for experimental and research purposes. Includes ffmpeg-full with
              unfree codecs for maximum format support.
              
              Note: Building requires --impure flag due to unfree codecs:
              nix build --impure
            '';
            homepage = "https://github.com/barstoolbluz/sox_ng";
            license = with licenses; [ gpl2Plus lgpl21Plus ];
            platforms = platforms.unix;
            maintainers = with maintainers; [barstoolbluz];
          };
        };
      in
      {
        packages = {
          default = sox-sinc;
          sox-sinc = sox-sinc;
        };
        
        apps.default = {
          type = "app";
          program = "${sox-sinc}/bin/sox";
        };
        
        devShells.default = pkgs.mkShell {
          buildInputs = [ sox-sinc ] ++ sox-sinc.nativeBuildInputs ++ sox-sinc.buildInputs;
        };
      });
}
