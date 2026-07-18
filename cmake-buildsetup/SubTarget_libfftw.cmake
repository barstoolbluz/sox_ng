# CMAKE file for library libfftw to be called when building SoX_ng

SET(targetName "libFFTW")
SET(targetVersion "3.3.10")

#==================
# === FUNCTIONS ===
#==================

MACRO(appendToSourceFileList stemListVariableName subdirectory)
    # appends to source file list all C sources stored in variable
    # named <stemListVariableName> located in <subdirectory>
    UTIL_List_appendOtherTransformed(libFFTWSourceFileList
                                     "${stemListVariableName}"
                                     "${subdirectory}/" ".c")
ENDMACRO()

#=================================================
# === generate variables via autoconfiguration ===
#=================================================

ACONF_setPackageVariables()

# define if the machine architecture "naturally" prefers fused
# multiply-add instructions
SET(ARCH_PREFERS_FMA FALSE)

# define to compile in single precision
# SET(BENCHFFT_SINGLE TRUE)
# SET(FFTW_SINGLE     TRUE)

# define to disable Fortran wrappers
SET(DISABLE_FORTRAN TRUE)

# define to enable generic (gcc) 128-bit SIMD optimizations
SET(HAVE_GENERIC_SIMD128 FALSE)

# define if you have POSIX threads libraries and header files
SET(HAVE_PTHREAD FALSE)

# set to true if you have the ANSI C header files
SET(STDC_HEADERS TRUE)

# set to true if SSE2 is enabled
SET(USE_SSE2 FALSE)

# use own aligned malloc for Windows
SET(WITH_OUR_MALLOC WINDOWS)

#------------------------------------------------------------
# define several configuration variables based on existing include
# files

SET(includeFileNameList
    "alloca.h" "altivec.h" "c_asm.h" "dlfcn.h" "fcntl.h" "fenv.h"
    "inttypes.h" "limits.h" "malloc.h" "memory.h" "stddef.h"
    "stdint.h" "stdlib.h" "string.h" "strings.h" "sys/stat.h"
    "sys/time.h" "sys/types.h" "unistd.h"
)

ACONF_checkIncludeFileNameList(includeFileNameList)

#------------------------------------------------------------
# define several configuration variables based on available
# library functions

SET(functionNameList
    "abort" "alloca" "BSDGettimeofday" "clock_gettime" "cosl"
    "_doprnt" "drand48" "gethrtime" "getpageize" "gettimeofday"
    "isnan" "mach_absolute_time" "memalign" "memmove" "memset"
    "posix_memalign" "read_real_time" "sinl" "snprintf" "sqrt"
    "sysctl" "tanl" "time_base_to_time" "vprintf"
    "_mm_free" "_mm_alloc" "_rtc"
)

ACONF_checkFunctionNameList(functionNameList)

#------------------------------------------------------------
# define configuration variables for endianness and type sizes of host
# machine

SET(typeNameList
    double float int hrtime_t long "long double" "long long"
    "ptrdiff_t" size_t "uintptr_t" "unsigned int" "unsigned long"
    "unsigned long long" "void*"
)

ACONF_collectTypeNameSizesForList(typeNameList)

TEST_BIG_ENDIAN(WORDS_BIGENDIAN)

IF(WORDS_BIGENDIAN)
    SET(CPU_IS_BIG_ENDIAN TRUE)
ELSE()
    SET(CPU_IS_LITTLE_ENDIAN TRUE)
ENDIF()

#------------------------------------------------------------
# find libraries

FIND_LIBRARY(LIBM_LIBRARY NAMES m)

IF(LIBM_LIBRARY)
    SET(HAVE_LIBM TRUE)
ENDIF()

IF(ENABLE_THREADS)
  FIND_PACKAGE(Threads)
ENDIF()

IF(Threads_FOUND)
  IF(CMAKE_USE_PTHREADS_INIT)
      SET(USING_POSIX_THREADS 1)
  ENDIF()

  SET(HAVE_THREADS TRUE)
ENDIF()

IF(ENABLE_OPENMP)
  FIND_PACKAGE(OpenMP)
ENDIF()

IF(OPENMP_FOUND)
    SET(HAVE_OPENMP TRUE)
ENDIF()

#====================
# === build setup ===
#====================

#-----------------------------------
#--- generate configuration file ---
#-----------------------------------

ACONF_generateConfigurationFile(FFTW) 

#-------------------
#--- directories ---
#-------------------

SET(libFFTWIncludeDirectoryList
    ${temporaryFFTWIncludeDirectory}
    ${LCONF_libFFTWDirectory}
)

#---------------------
# --- source files ---
#---------------------

# --- api ---
SET(subdirectory "${LCONF_libFFTWDirectory}/api")

SET(srcFileStemList
    "apiplan" "configure" "execute" "execute-dft" "execute-dft-c2r"
    "execute-dft-r2c" "execute-r2r" "execute-split-dft"
    "execute-split-dft-c2r" "execute-split-dft-r2c" "export-wisdom"
    "export-wisdom-to-file" "export-wisdom-to-string" "f77api" "flops"
    "forget-wisdom" "import-system-wisdom" "import-wisdom"
    "import-wisdom-from-file" "import-wisdom-from-string" "malloc"
    "mapflags" "map-r2r-kind" "mkprinter-file" "mkprinter-str"
    "mktensor-iodims64" "mktensor-iodims" "mktensor-rowmajor"
    "plan-dft-1d" "plan-dft-2d" "plan-dft-3d" "plan-dft"
    "plan-dft-c2r-1d" "plan-dft-c2r-2d" "plan-dft-c2r-3d"
    "plan-dft-c2r" "plan-dft-r2c-1d" "plan-dft-r2c-2d"
    "plan-dft-r2c-3d" "plan-dft-r2c" "plan-guru64-dft"
    "plan-guru64-dft-c2r" "plan-guru64-dft-r2c" "plan-guru64-r2r"
    "plan-guru64-split-dft" "plan-guru64-split-dft-c2r"
    "plan-guru64-split-dft-r2c" "plan-guru-dft" "plan-guru-dft-c2r"
    "plan-guru-dft-r2c" "plan-guru-r2r" "plan-guru-split-dft"
    "plan-guru-split-dft-c2r" "plan-guru-split-dft-r2c"
    "plan-many-dft" "plan-many-dft-c2r" "plan-many-dft-r2c"
    "plan-many-r2r" "plan-r2r-1d" "plan-r2r-2d" "plan-r2r-3d"
    "plan-r2r" "print-plan" "rdft2-pad" "the-planner" "version"
)

appendToSourceFileList(srcFileStemList "${subdirectory}")

# --- dft ---
SET(subdirectory "${LCONF_libFFTWDirectory}/dft")

SET(srcFileStemList
    "bluestein" "buffered" "conf" "ct" "dftw-direct" "dftw-directsq"
    "dftw-genericbuf" "dftw-generic" "direct" "generic" "indirect"
    "indirect-transpose" "kdft" "kdft-dif" "kdft-difsq" "kdft-dit"
    "nop" "plan" "problem" "rader" "rank-geq2" "solve" "vrank-geq1"
    "zero"
)

appendToSourceFileList(srcFileStemList "${subdirectory}")

# --- dft/scalar ---
SET(subdirectory "${LCONF_libFFTWDirectory}/dft/scalar")

SET(srcFileStemList
    "n" "t"
)

appendToSourceFileList(srcFileStemList "${subdirectory}")

# --- dft/scalar/codelets ---
SET(subdirectory "${LCONF_libFFTWDirectory}/dft/scalar/codelets")

SET(srcFileStemList
    "codlist" "n1_10" "n1_11" "n1_12" "n1_13" "n1_14" "n1_15" "n1_16"
    "n1_20" "n1_25" "n1_2" "n1_32" "n1_3" "n1_4" "n1_5" "n1_64" "n1_6"
    "n1_7" "n1_8" "n1_9" "q1_2" "q1_3" "q1_4" "q1_5" "q1_6" "q1_8"
    "t1_10" "t1_12" "t1_15" "t1_16" "t1_20" "t1_25" "t1_2" "t1_32"
    "t1_3" "t1_4" "t1_5" "t1_64" "t1_6" "t1_7" "t1_8" "t1_9" "t2_10"
    "t2_16" "t2_20" "t2_25" "t2_32" "t2_4" "t2_5" "t2_64" "t2_8"
)

appendToSourceFileList(srcFileStemList "${subdirectory}")

# --- dft/simd/common ---
SET(subdirectory "${LCONF_libFFTWDirectory}/dft/simd/common")

SET(srcFileStemList
    "codlist" "genus" "n1bv_10" "n1bv_11" "n1bv_128" "n1bv_12"
    "n1bv_13" "n1bv_14" "n1bv_15" "n1bv_16" "n1bv_20" "n1bv_25"
    "n1bv_2" "n1bv_32" "n1bv_3" "n1bv_4" "n1bv_5" "n1bv_64" "n1bv_6"
    "n1bv_7" "n1bv_8" "n1bv_9" "n1fv_10" "n1fv_11" "n1fv_128"
    "n1fv_12" "n1fv_13" "n1fv_14" "n1fv_15" "n1fv_16" "n1fv_20"
    "n1fv_25" "n1fv_2" "n1fv_32" "n1fv_3" "n1fv_4" "n1fv_5" "n1fv_64"
    "n1fv_6" "n1fv_7" "n1fv_8" "n1fv_9" "n2bv_10" "n2bv_12" "n2bv_14"
    "n2bv_16" "n2bv_20" "n2bv_2" "n2bv_32" "n2bv_4" "n2bv_64" "n2bv_6"
    "n2bv_8" "n2fv_10" "n2fv_12" "n2fv_14" "n2fv_16" "n2fv_20"
    "n2fv_2" "n2fv_32" "n2fv_4" "n2fv_64" "n2fv_6" "n2fv_8" "n2sv_16"
    "n2sv_32" "n2sv_4" "n2sv_64" "n2sv_8" "q1bv_2" "q1bv_4" "q1bv_5"
    "q1bv_8" "q1fv_2" "q1fv_4" "q1fv_5" "q1fv_8" "t1buv_10" "t1buv_2"
    "t1buv_3" "t1buv_4" "t1buv_5" "t1buv_6" "t1buv_7" "t1buv_8"
    "t1buv_9" "t1bv_10" "t1bv_12" "t1bv_15" "t1bv_16" "t1bv_20"
    "t1bv_25" "t1bv_2" "t1bv_32" "t1bv_3" "t1bv_4" "t1bv_5" "t1bv_64"
    "t1bv_6" "t1bv_7" "t1bv_8" "t1bv_9" "t1fuv_10" "t1fuv_2" "t1fuv_3"
    "t1fuv_4" "t1fuv_5" "t1fuv_6" "t1fuv_7" "t1fuv_8" "t1fuv_9"
    "t1fv_10" "t1fv_12" "t1fv_15" "t1fv_16" "t1fv_20" "t1fv_25"
    "t1fv_2" "t1fv_32" "t1fv_3" "t1fv_4" "t1fv_5" "t1fv_64" "t1fv_6"
    "t1fv_7" "t1fv_8" "t1fv_9" "t1sv_16" "t1sv_2" "t1sv_32" "t1sv_4"
    "t1sv_8" "t2bv_10" "t2bv_16" "t2bv_20" "t2bv_25" "t2bv_2"
    "t2bv_32" "t2bv_4" "t2bv_5" "t2bv_64" "t2bv_8" "t2fv_10" "t2fv_16"
    "t2fv_20" "t2fv_25" "t2fv_2" "t2fv_32" "t2fv_4" "t2fv_5" "t2fv_64"
    "t2fv_8" "t2sv_16" "t2sv_32" "t2sv_4" "t2sv_8" "t3bv_10" "t3bv_16"
    "t3bv_20" "t3bv_25" "t3bv_32" "t3bv_4" "t3bv_5" "t3bv_8" "t3fv_10"
    "t3fv_16" "t3fv_20" "t3fv_25" "t3fv_32" "t3fv_4" "t3fv_5" "t3fv_8"
)

## appendToSourceFileList(srcFileStemList "${subdirectory}")

# --- kernel ---
SET(subdirectory "${LCONF_libFFTWDirectory}/kernel")

SET(srcFileStemList
    "align" "alloc" "assert" "awake" "buffered" "cpy1d" "cpy2d"
    "cpy2d-pair" "ct" "debug" "extract-reim" "hash" "iabs" "kalloc"
    "md5-1" "md5" "minmax" "ops" "pickdim" "plan" "planner" "primes"
    "print" "problem" "rader" "scan" "solver" "solvtab" "stride"
    "tensor1" "tensor2" "tensor3" "tensor4" "tensor5" "tensor7"
    "tensor8" "tensor9" "tensor" "tile2d" "timer" "transpose" "trig"
    "twiddle"
)

appendToSourceFileList(srcFileStemList "${subdirectory}")

# --- rdft ---
SET(subdirectory "${LCONF_libFFTWDirectory}/rdft")

SET(srcFileStemList
    "buffered2" "buffered" "conf" "ct-hc2c" "ct-hc2c-direct"
    "dft-r2hc" "dht-r2hc" "dht-rader" "direct2" "direct-r2c"
    "direct-r2r" "generic" "hc2hc" "hc2hc-direct" "hc2hc-generic"
    "indirect" "khc2c" "khc2hc" "kr2c" "kr2r" "nop2" "nop" "plan2"
    "plan" "problem2" "problem" "rank0" "rank0-rdft2" "rank-geq2"
    "rank-geq2-rdft2" "rdft2-inplace-strides" "rdft2-rdft"
    "rdft2-strides" "rdft2-tensor-max-index" "rdft-dht" "solve2"
    "solve" "vrank3-transpose" "vrank-geq1" "vrank-geq1-rdft2"
)

appendToSourceFileList(srcFileStemList "${subdirectory}")

# --- rdft/scalar ---
SET(subdirectory "${LCONF_libFFTWDirectory}/rdft/scalar")

SET(srcFileStemList
    "hc2c" "hfb" "r2c" "r2r"
)

appendToSourceFileList(srcFileStemList "${subdirectory}")

# --- rdft/scalar/r2cb ---
SET(subdirectory "${LCONF_libFFTWDirectory}/rdft/scalar/r2cb")

SET(srcFileStemList
    "codlist" "hb_10" "hb_12" "hb_15" "hb_16" "hb_20" "hb2_16"
    "hb2_20" "hb2_25" "hb2_32" "hb2_4" "hb_25" "hb2_5" "hb2_8" "hb_2"
    "hb_32" "hb_3" "hb_4" "hb_5" "hb_64" "hb_6" "hb_7" "hb_8" "hb_9"
    "hc2cb_10" "hc2cb_12" "hc2cb_16" "hc2cb_20" "hc2cb2_16"
    "hc2cb2_20" "hc2cb2_32" "hc2cb2_4" "hc2cb2_8" "hc2cb_2" "hc2cb_32"
    "hc2cb_4" "hc2cb_6" "hc2cb_8" "hc2cbdft_10" "hc2cbdft_12"
    "hc2cbdft_16" "hc2cbdft_20" "hc2cbdft2_16" "hc2cbdft2_20"
    "hc2cbdft2_32" "hc2cbdft2_4" "hc2cbdft2_8" "hc2cbdft_2"
    "hc2cbdft_32" "hc2cbdft_4" "hc2cbdft_6" "hc2cbdft_8" "r2cb_10"
    "r2cb_11" "r2cb_128" "r2cb_12" "r2cb_13" "r2cb_14" "r2cb_15"
    "r2cb_16" "r2cb_20" "r2cb_25" "r2cb_2" "r2cb_32" "r2cb_3" "r2cb_4"
    "r2cb_5" "r2cb_64" "r2cb_6" "r2cb_7" "r2cb_8" "r2cb_9"
    "r2cbIII_10" "r2cbIII_12" "r2cbIII_15" "r2cbIII_16" "r2cbIII_20"
    "r2cbIII_25" "r2cbIII_2" "r2cbIII_32" "r2cbIII_3" "r2cbIII_4"
    "r2cbIII_5" "r2cbIII_64" "r2cbIII_6" "r2cbIII_7" "r2cbIII_8"
    "r2cbIII_9"
)

appendToSourceFileList(srcFileStemList "${subdirectory}")

# --- rdft/scalar/r2cf ---
SET(subdirectory "${LCONF_libFFTWDirectory}/rdft/scalar/r2cf")

SET(srcFileStemList
    "codlist" "hc2cf_10" "hc2cf_12" "hc2cf_16" "hc2cf_20" "hc2cf2_16"
    "hc2cf2_20" "hc2cf2_32" "hc2cf2_4" "hc2cf2_8" "hc2cf_2" "hc2cf_32"
    "hc2cf_4" "hc2cf_6" "hc2cf_8" "hc2cfdft_10" "hc2cfdft_12"
    "hc2cfdft_16" "hc2cfdft_20" "hc2cfdft2_16" "hc2cfdft2_20"
    "hc2cfdft2_32" "hc2cfdft2_4" "hc2cfdft2_8" "hc2cfdft_2"
    "hc2cfdft_32" "hc2cfdft_4" "hc2cfdft_6" "hc2cfdft_8" "hf_10"
    "hf_12" "hf_15" "hf_16" "hf_20" "hf2_16" "hf2_20" "hf2_25"
    "hf2_32" "hf2_4" "hf_25" "hf2_5" "hf2_8" "hf_2" "hf_32" "hf_3"
    "hf_4" "hf_5" "hf_64" "hf_6" "hf_7" "hf_8" "hf_9" "r2cf_10"
    "r2cf_11" "r2cf_128" "r2cf_12" "r2cf_13" "r2cf_14" "r2cf_15"
    "r2cf_16" "r2cf_20" "r2cf_25" "r2cf_2" "r2cf_32" "r2cf_3" "r2cf_4"
    "r2cf_5" "r2cf_64" "r2cf_6" "r2cf_7" "r2cf_8" "r2cf_9" "r2cfII_10"
    "r2cfII_12" "r2cfII_15" "r2cfII_16" "r2cfII_20" "r2cfII_25"
    "r2cfII_2" "r2cfII_32" "r2cfII_3" "r2cfII_4" "r2cfII_5"
    "r2cfII_64" "r2cfII_6" "r2cfII_7" "r2cfII_8" "r2cfII_9"
)

appendToSourceFileList(srcFileStemList "${subdirectory}")

# --- rdft/scalar/r2r ---
SET(subdirectory "${LCONF_libFFTWDirectory}/rdft/scalar/r2r")

SET(srcFileStemList
    "codlist" "e01_8" "e10_8"
)

appendToSourceFileList(srcFileStemList "${subdirectory}")

# --- reodft ---
SET(subdirectory "${LCONF_libFFTWDirectory}/reodft")

SET(srcFileStemList
    "conf" "redft00e-r2hc" "redft00e-r2hc-pad" "reodft00e-splitradix"
    "reodft010e-r2hc" "reodft11e-r2hc" "reodft11e-r2hc-odd"
    "reodft11e-radix2" "rodft00e-r2hc" "rodft00e-r2hc-pad"
)

appendToSourceFileList(srcFileStemList "${subdirectory}")

#--------------------
# --- definitions ---
#--------------------

SET(libFFTWCompileDefinitionList
    HAVE_CONFIG_H
)

#===============
# === Target ===
#===============

LINKER_makeLibraryTarget(${targetName} TRUE)
ADD_DEPENDENCIES(SupportLibraries ${targetName})
