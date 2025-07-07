noinst_PROGRAMS = samples
samples_SOURCES = samples.c

CLEANFILES = samples samples.o

all: samples

check-am:
	sox=../../src/sox_ng sh $(srcdir)/check.sh -n

dist-hook:
	$(RM) -rf $(distdir)/.deps
