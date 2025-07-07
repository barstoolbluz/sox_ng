# Copyright (C) 2006 Reuben Thomas <rrt@sc3d.org>
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation; either version 2 of the License, or (at your
# option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
# Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

## Process this file with automake to produce Makefile.in

RM = rm -f 

AM_CPPFLAGS = -DLADSPA_PATH="\"@LADSPA_PATH@\""
AM_CFLAGS = @WARN_CFLAGS@ #-Werror

if HAVE_LIBLTDL
# This is being used as a short cut to turn off versioning of ALL dynamic
# fmt libraries.  If any fmt ever needs to add a specific LDFLAGS
# then it will need to also add -avoid-version because AM_LDFLAGS
# is ignored when you specify a more specific one.
# We want to version libsox_ng and we are OK because they
# have a more specific LDFLAGS that includes -version-info.
AM_LDFLAGS = -avoid-version -module @APP_LDFLAGS@
AM_CPPFLAGS += -DPKGLIBDIR="\"$(pkglibdir)\""
endif
if HAVE_WIN32_LTDL
# Must match line above.
AM_LDFLAGS = -avoid-version -module
AM_CPPFLAGS += -DPKGLIBDIR="\"$(pkglibdir)\""
endif

# Pass flags from --enable-silent-libtool
LIBTOOL = @LIBTOOL@ @LIBTOOLFLAGS@

if HAVE_DISTRO
AM_CPPFLAGS += -DDISTRO="\"@DISTRO@\""
endif



#########################
# SoX - the application #
#########################

bin_PROGRAMS = sox_ng
EXTRA_PROGRAMS = example0 example1 example2 example3 example4 example5 example6 sox_sample_test
lib_LTLIBRARIES = libsox_ng.la
include_HEADERS = sox_ng.h
sox_ng_SOURCES = sox_ng.c
if HAVE_WIN32_GLOB
sox_ng_SOURCES += win32-glob.c win32-glob.h
endif
sox_ng_LDADD = libsox_ng.la
example0_SOURCES = example0.c
example1_SOURCES = example1.c
example2_SOURCES = example2.c
example3_SOURCES = example3.c
example4_SOURCES = example4.c
example5_SOURCES = example5.c
example6_SOURCES = example6.c
sox_sample_test_SOURCES = sox_sample_test.c sox_sample_test.h



#########################################################
# libsox_ng - file format, effects, and utility library #
#########################################################

# Format handlers and utils source
libsox_ng_la_SOURCES = adpcms.c adpcms.h aiff.c aiff.h cvsd.c cvsd.h cvsdfilt.h \
	  g711.c g711.h g721.c g723_24.c g723_40.c g72x.c g72x.h vox.c vox.h \
	  raw.c raw.h formats.c formats.h formats_i.c sox_i.h \
	  xmalloc.c xmalloc.h getopt.c util.c util.h libsox_ng.c libsox_i.c \
	  sox-fmt.c soxomp.h win32-unicode.c win32-unicode.h

# Effects source
libsox_ng_la_SOURCES += \
	band.h bend.c biquad.c biquad.h biquads.c chorus.c compand.c \
	compandt.c compandt.h contrast.c dcshift.c delay.c dft_filter.c \
	dft_filter.h dither.c dither.h dolbyb.c dop.c downsample.c earwax.c \
	echo.c echos.c effects.c effects.h effects_i.c effects_i_dsp.c \
	fade.c ffmpeg.c fft4g.c fft4g.h fifo.h fir.c firfit.c flanger.c gain.c \
	hilbert.c input.c ladspa.h ladspa.c loudness.c mcompand.c \
	mcompand_xover.h noiseprof.c noisered.c \
	noisered.h output.c overdrive.c pad.c phaser.c rate.c \
	rate_filters.h rate_half_fir.h rate_poly_fir0.h rate_poly_fir.h \
	remix.c repeat.c reverb.c reverse.c silence.c sinc.c \
	sdm.c sdm.h sdm_x86.h softvol.c softvol.h \
	speed.c speexdsp.c splice.c stat.c stats.c stretch.c swap.c \
	synth.c tempo.c tremolo.c trim.c upsample.c vad.c vol.c \
	ignore-warning.h
if HAVE_PNG
    libsox_ng_la_SOURCES += spectrogram.c
endif

# Libraries required by libsox_ng for file handlers, effects, or utils;
# regardless if libltdl is used or not.
libsox_ng_la_LIBADD = @PNG_LIBS@
if HAVE_MAGIC
libsox_ng_la_LIBADD += @MAGIC_LIBS@
endif
if HAVE_FFTW
libsox_ng_la_LIBADD += @FFTW_LIBS@
endif
if HAVE_SPEEXDSP
libsox_ng_la_LIBADD += @SPEEXDSP_LIBS@
endif

libsox_ng_la_LIBADD += @GOMP_LIBS@

libsox_ng_la_CFLAGS = @WARN_CFLAGS@
libsox_ng_la_LDFLAGS = @APP_LDFLAGS@ -version-info @SHLIB_VERSION@ \
  -export-symbols-regex '^(sox_.*|create_lpc10_(en|de)coder_state|lpc10_(en|de)code|lsx_(adjust_softvol|gsm_(create|destroy|encode|decode)|([cm]|re)alloc.*|check_read_params|(close|open)_dllibrary|(debug(_more|_most)?|fail|report|warn)_impl|eof|error|fail_errno|filelength|find_(enum_(text|value)|file_extension)|flush|clearerr|getopt(_init)?|id3(_read_tag|tagmap)|raw(read|write)|read(_b_buf|buf|chars)|unreadbuf|realloc|realloc_array|rewind|seeki|sigfigs3p?|sscanf|strcasecmp|strncasecmp|strdup|tell|unreadb|write(b|_b_buf|buf|s)))$$'

if HAVE_WIN32_LTDL
  libsox_ng_la_SOURCES += win32-ltdl.c win32-ltdl.h
endif

if HAVE_LIBLTDL
  libsox_ng_la_CFLAGS += $(LTDLINCL) 
  libsox_ng_la_LDFLAGS += $(LIBLTDL) 
endif



############################
# libsox_ng - File Formats #
############################

# Uncomment for bit-rot detection on linux
#libsox_ng_la_SOURCES += coreaudio.c sndio.c sunaudio.c
#libsox_ng_la_CFLAGS += -Ibit-rot

libsox_ng_la_SOURCES += raw-fmt.c s1-fmt.c s2-fmt.c s3-fmt.c \
  s4-fmt.c u1-fmt.c u2-fmt.c u3-fmt.c u4-fmt.c al-fmt.c la-fmt.c ul-fmt.c \
  lu-fmt.c 8svx.c aiff-fmt.c aifc-fmt.c au.c avr.c cdr.c cvsd-fmt.c \
  dvms-fmt.c dat.c hcom.c htk.c maud.c prc.c sf.c smp.c \
  sounder.c soundtool.c sphere.c tx16w.c voc.c vox-fmt.c ima-fmt.c adpcm.c adpcm.h \
  id3.c id3.h dsf.c dsdiff.c \
  ima_rw.c ima_rw.h wav.c wve.c xa.c nulfile.c f4-fmt.c f8-fmt.c gsrt.c nsp.c

libsox_ng_la_LIBADD += @DOLBYB_LIBS@ @LIBDOLBYB_LIBADD@
libsox_ng_la_LIBADD += @GSM_LIBS@ @LIBGSM_LIBADD@
libsox_ng_la_LIBADD += @LPC10_LIBS@ @LIBLPC10_LIBADD@

if HAVE_ID3TAG
  libsox_ng_la_LIBADD += @ID3_LIBS@
endif

pkglib_LTLIBRARIES =

include optional-fmts.am



# example programs will need same link options as sox does.
example0_LDADD = ${sox_ng_LDADD}
example1_LDADD = ${sox_ng_LDADD}
example2_LDADD = ${sox_ng_LDADD}
example3_LDADD = ${sox_ng_LDADD}
example4_LDADD = ${sox_ng_LDADD}
example5_LDADD = ${sox_ng_LDADD}
example6_LDADD = ${sox_ng_LDADD}

EXTRA_DIST = monkey.wav optional-fmts.am \
	     CMakeLists.txt soxconfig.h.cmake \
	     tests.sh testall.sh tests.bat testall.bat test-comments

all: sox_ng$(EXEEXT) play_ng$(EXEEXT) rec_ng$(EXEEXT) soxi_ng$(EXEEXT)

play_ng$(EXEEXT) rec_ng$(EXEEXT) soxi_ng$(EXEEXT): sox_ng$(EXEEXT)
	@if test "$(PLAYRECLINKS)" = "yes"; then	\
		test -f sox_ng$(EXEEXT) && ( $(RM) $@ && $(LN_S) sox_ng$(EXEEXT) $@ ) || $(LN_S) sox_ng $@; \
	fi

install-exec-hook:
	@if test "$(PLAYRECLINKS)" = "yes"; then	\
		cd $(DESTDIR)$(bindir); $(RM) play_ng$(EXEEXT) rec_ng$(EXEEXT); $(LN_S) sox_ng$(EXEEXT) play_ng$(EXEEXT); $(LN_S) sox_ng$(EXEEXT) rec_ng$(EXEEXT); \
	fi
	@if test "$(SYMLINKS)" = "yes"; then	\
		cd $(DESTDIR)$(bindir); $(RM) soxi_ng$(EXEEXT); $(LN_S) sox_ng$(EXEEXT) soxi_ng$(EXEEXT); \
	fi
	@if test "$(REPLACE)" = "yes"; then \
		cd $(DESTDIR)$(bindir); \
		$(RM) sox$(EXEEXT); $(LN_S) sox_ng$(EXEEXT) sox$(EXEEXT); \
		if test "$(PLAYRECLINKS)" = "yes"; then	\
			$(RM) play$(EXEEXT) rec$(EXEEXT); \
			$(LN_S) play_ng$(EXEEXT) play$(EXEEXT); \
			$(LN_S) rec_ng$(EXEEXT) rec$(EXEEXT); \
		fi; \
		if test "$(SYMLINKS)" = "yes"; then \
			$(RM) soxi$(EXEEXT); \
			$(LN_S) soxi_ng$(EXEEXT) soxi$(EXEEXT); \
		fi; \
		cd $(DESTDIR)$(includedir); \
		$(RM) sox.h; $(LN_S) sox_ng.h sox.h; \
		cd $(DESTDIR)$(libdir); \
		$(RM) libsox.a; $(LN_S) libsox_ng.a libsox.a; \
		$(RM) libsox.la; $(LN_S) libsox_ng.la libsox.la; \
		$(RM) libsox.so; $(LN_S) libsox_ng.so libsox.so; \
		if test "$(pkgconfigdir)"; then \
			cd $(DESTDIR)$(pkgconfigdir); \
			$(RM) "sox.pc"; $(LN_S) sox_ng.pc sox.pc; \
		fi; \
	fi

uninstall-hook:
	@if test "$(PLAYRECLINKS)" = "yes"; then	\
		cd $(DESTDIR)$(bindir); $(RM) play_ng$(EXTEXT) rec_ng$(EXEEXT); \
	fi
	@if test "$(SYMLINKS)" = "yes"; then	\
		cd $(DESTDIR)$(bindir); $(RM) soxi_ng$(EXEEXT); \
	fi
	@if test "$(REPLACE)" = "yes"; then \
		cd $(DESTDIR)$(bindir); \
		$(RM) sox$(EXEEXT); \
		if test "$(PLAYRECLINKS)" = "yes"; then	\
			$(RM) play$(EXEEXT) rec$(EXEEXT); \
		fi; \
		if test "$(SYMLINKS)" = "yes"; then \
			$(RM) soxi$(EXEEXT); \
		fi; \
		cd $(DESTDIR)$(includedir); $(RM) sox.h;\
		cd $(DESTDIR)$(libdir); \
		$(RM) libsox.a libsox.la libsox.so; \
		if test "$pkgconfigdir"; then \
			cd $(DESTDIR)$(pkfconfigdir); \
			$(RM) "sox.pc";\
		fi; \
	fi

clean-local:
	$(RM) play_ng$(EXEEXT) rec_ng$(EXEEXT) soxi_ng$(EXEEXT)
	$(RM) sox_sample_test$(EXEEXT)
	$(RM) example0$(EXEEXT) example1$(EXEEXT) example2$(EXEEXT) example3$(EXEEXT) example4$(EXEEXT) example5$(EXEEXT) example6$(EXEEXT)

distclean-local:

loc:
	sloccount \
	$(include_HEADERS) \
	$(sox_ng_SOURCES) \
	$(example0_SOURCES) \
	$(example1_SOURCES) \
	$(example2_SOURCES) \
	$(example3_SOURCES) \
	$(example4_SOURCES) \
	$(example5_SOURCES) \
	$(example6_SOURCES) \
	$(sox_sample_test_SOURCES) \
	$(libsox_ng_la_SOURCES)


# Ideally we would use the "check" target so that "make distcheck"
# would run the test suite, but an uninstalled libltdl build cannot
# currently load its formats and effects, so the checks would fail.
installcheck: sox_sample_test
	$(srcdir)/tests.sh --bindir=$(DESTDIR)${bindir} \
		--libdir=$(DESTDIR)${libdir} \
		--builddir=${builddir} --srcdir=${srcdir}
	$(srcdir)/testall.sh --bindir=$(DESTDIR)${bindir} \
		--libdir=$(DESTDIR)${libdir} \
		--srcdir=${srcdir}

check-am: sox_sample_test
	sh $(srcdir)/tests.sh
