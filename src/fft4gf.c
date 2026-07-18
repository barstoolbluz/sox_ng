/* Single-precision float version of fft4g.c
 *
 * Copyright (c) 2025  Martin Guy <martineguy@gmail.com>
 *
 * "bend" would like to use this, and it seems to work, but it is not
 * currently included in src/Makefile.am as valgrind says it accesses
 * uninitialized memory https://codeberg.org/sox_ng/sox_ng/issues/790
 *
 * To test this, also remove the #if 0 in src/effects_i_dsp.c
 */

#define FFT4G_FLOAT 1

#include "fft4g.c"
