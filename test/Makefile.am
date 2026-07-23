EXTRA_DIST = samples.c \
  BUG-libsndfile-float-level/README \
  BUG-libsndfile-float-level/run \
  BUG-libsndfile-float-level/sox-f32-minus20.caf \
  BUG-libsndfile-float-level/sox-f32-minus20.w64 \
  BUG-libsndfile-float-level/sox-f64-minus20.caf \
  BUG-libsndfile-float-level/sox-f64-minus20.w64

EXTRA_PROGRAMS = wav_size_state_test
wav_size_state_test_SOURCES = wav-size-state.c
wav_size_state_test_CPPFLAGS = -I$(top_srcdir)/src

CLEANFILES = samples$(EXEEXT) samples.o

samples$(EXEEXT): samples.c
	$(CC) -o samples$(EXEEXT) samples.c

check-am: samples$(EXEEXT) wav_size_state_test$(EXEEXT)
	sox=../../src/sox_ng \
	wav_size_state_test='$(abs_builddir)/wav_size_state_test$(EXEEXT)' \
	sh $(srcdir)/check.sh -n

dist-hook:
	rm -f $(distdir)/samples$(EXEEXT) $(distdir)/samples.o
	rm -rf $(distdir)/.deps
