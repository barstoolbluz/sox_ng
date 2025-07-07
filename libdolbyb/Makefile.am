if EXTERNAL_DOLBYB
EXTRA_DIST = \
     Calibrate.c DCfilter.c DiodeClip.c FindOutSmp.c HPF1.c HPF2.c \
     HPF2SetVals.c HPFAdj.c LPFAdj.c Param.c SetGate.c SidePath.c \
     libdolbyb.c \
     Calibrate.h DCfilter.h DiodeClip.h FindOutSmp.h HPF1.h HPF2.h \
     HPF2SetVals.h HPFAdj.h LPFAdj.h Mixers.h Param.h SetGate.h SidePath.h \
     dolbyb.h
else
noinst_LTLIBRARIES = libdolbyb.la
noinst_HEADERS = dolbyb.h
libdolbyb_la_SOURCES = \
     Calibrate.c DCfilter.c DiodeClip.c FindOutSmp.c HPF1.c HPF2.c \
     HPF2SetVals.c HPFAdj.c LPFAdj.c Param.c SetGate.c SidePath.c \
     libdolbyb.c \
     Calibrate.h DCfilter.h DiodeClip.h FindOutSmp.h HPF1.h HPF2.h \
     HPF2SetVals.h HPFAdj.h LPFAdj.h Mixers.h Param.h SetGate.h SidePath.h \
     dolbyb.h
AM_CPPFLAGS=-I../src
if HAVE_LIBLTDL
AM_LDFLAGS=-avoid-version -module
endif
endif
