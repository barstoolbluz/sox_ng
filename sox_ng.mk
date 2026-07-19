# This file is part of MXE. See LICENSE.md for licensing information.

PKG             := sox_ng
$(PKG)_WEBSITE  := https://codeberg.org/sox_ng/sox_ng
$(PKG)_DESCR    := SoX
$(PKG)_IGNORE   :=
$(PKG)_VERSION  := 14.6.1
$(PKG)_CHECKSUM := bb03126de6b3ce0049801466f067097eb73f8a2fc11e9239ed00744f42691145
$(PKG)_SUBDIR   := $(PKG)-$($(PKG)_VERSION)
$(PKG)_FILE     := $(PKG)-$($(PKG)_VERSION).tar.gz
$(PKG)_URL      := https://codeberg.org/sox_ng/$(PKG)/releases/download/$($(PKG)_SUBDIR)/$($(PKG)_FILE)
$(PKG)_DEPS     := cc fftw file flac lame libid3tag libltdl libmad libpng \
                   libsndfile opencore-amr opusfile speex speexdsp twolame \
		   vorbis wavpack

define $(PKG)_UPDATE
     echo 'TODO: write update script for $(PKG).' >&2
     echo $($(PKG)_VERSION)
endef

define $(PKG)_BUILD
    # set pkg-config cflags and libs
    $(SED) -i 's,^\(Cflags:.*\),\1 -fopenmp,' '$(1)/sox_ng.pc.in'
    $(SED) -i '/Libs.private/d'               '$(1)/sox_ng.pc.in'
    echo Libs.private: @MAGIC_LIBS@ \
        `grep sox_ng_LDADD '$(1)/src/optional-fmts.am' | \
         $(SED) 's, sox_ng_LDADD += ,,g' | tr -d '\n'` @LIBS@ >>'$(1)/sox_ng.pc.in'

    cd '$(1)' && ./configure \
        --host='$(TARGET)' \
        --prefix='$(PREFIX)/$(TARGET)' \
        --build="`config.guess`" \
        --disable-shared \
        --enable-static \
        --disable-debug \
        --with-libltdl \
	CFLAGS='-fno-pie -O2' \
	LDFLAGS='-fno-pie' \
        LIBS='-lshlwapi -lgnurx'

    $(MAKE) -C '$(1)' -j '$(JOBS)' bin_PROGRAMS= EXTRA_PROGRAMS=
    $(MAKE) -C '$(1)' -j 1 install
endef

$(PKG)_BUILD_SHARED =
