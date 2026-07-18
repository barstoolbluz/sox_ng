if EXTERNAL_EBUR128
EXTRA_DIST = ebur128.c ebur128.h queue.h CMakeLists.txt
else
noinst_LTLIBRARIES = libebur128.la
noinst_HEADERS = ebur128.h
libebur128_la_SOURCES = ebur128.c ebur128.h queue.h
if HAVE_LIBLTDL
AM_LDFLAGS=-avoid-version -module
endif
endif
