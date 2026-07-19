EXTRA_DIST = samples.c

CLEANFILES = samples$(EXEEXT) samples.o

samples$(EXEEXT): samples.c
	$(CC) -o samples$(EXEEXT) samples.c

check-am: samples$(EXEEXT)
	sox=../../src/sox_ng sh $(srcdir)/check.sh -n

dist-hook:
	rm -f $(distdir)/samples$(EXEEXT) $(distdir)/samples.o
	rm -rf $(distdir)/.deps
