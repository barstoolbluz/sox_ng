# CMAKE file for library libSpeexDSP to be called when building SoX_ng

SET(targetName "libSpeexDSP")

SET(majorVersion 1)
SET(minorVersion 2)
SET(microVersion 1)
SET(targetVersion "${majorVersion}.${minorVersion}.${microVersion}")

#=================================================
# === generate variables via autoconfiguration ===
#=================================================

ACONF_setPackageVariables()

# define if building universal (internal helper macro)
SET(AC_APPLE_UNIVERSAL_BUILD FALSE)

# disable all parts of the API that are using floats
SET(DISABLE_FLOAT_API FALSE)

# disable VBR and VAD from the codec
SET(DISABLE_VBR FALSE)

# enable valgrind extra checks
SET(ENABLE_VALGRIND FALSE)

IF(DISABLE_FLOAT_API)
    # compile as fixed-point
    SET(FIXED_POINT TRUE)
    # debug fixed-point implementation
    SET(FIXED_DEBUG FALSE)
ELSE()
    # compile as floating-point
    SET(FLOATING_POINT TRUE)
ENDIF()

# version parts
SET(SPEEX_MAJOR_VERSION ${majorVersion})
SET(SPEEX_MINOR_VERSION ${minorVersion})
SET(SPEEX_MICRO_VERSION ${microVersion})
SET(SPEEX_EXTRA_VERSION "1")

# complete version string
SET(SPEEX_VERSION ${targetVersion})

# set to true if you have the ANSI C header files
SET(STDC_HEADERS TRUE)

# enable support for TI C55X DSP
SET(TI_C55X FALSE)

# enable NEON support
SET(USE_NEON FALSE)

# use variable-size arrays
SET(VAR_ARRAYS FALSE)

# --- find fast fourier transformation method

# use FFTW3 for FFT
SET(USE_GPL_FFTW3 FALSE)

# use Intel Math Kernel Library for FFT
SET(USE_INTEL_MKL FALSE)

# use KISS Fast Fourier Transform
SET(USE_KISS_FFT FALSE)

# use FFT from OggVorbis
SET(USE_SMALLFT TRUE)

#------------------------------------------------------------
# define configuration variables for endianness and type sizes of host
# machine

SET(typeNameList
    int int16_t int32_t long short uint16_t uint32_t u_int16_t
    u_int32_t
)

ACONF_collectTypeNameSizesForList(typeNameList)

#------------------------------------------------------------
# define several configuration variables based on host processor
# properties

CMAKE_HOST_SYSTEM_INFORMATION(RESULT HAS_SSE  QUERY HAS_SSE)
CMAKE_HOST_SYSTEM_INFORMATION(RESULT HAS_SSE2 QUERY HAS_SSE2)

#------------------------------------------------------------
# define several configuration variables based on existing include
# files

SET(includeFileNameList
    "dlfcn.h" "inttypes.h" "stdint.h" "stdio.h"
    "stdlib.h" "string.h" "strings.h" "sys/audioio.h"
    "sys/soundcard.h" "sys/stat.h" "sys/types.h" "unistd.h"
)

ACONF_checkIncludeFileNameList(includeFileNameList)

SET(USE_ALLOCA HAVE_ALLOCA)

#====================
# === build setup ===
#====================

#------------------------------------
#--- generate configuration files ---
#------------------------------------

SET(libraryShortName SpeexDSP)

# common config.h
ACONF_generateConfigurationFile(${libraryShortName}) 

# common speexdsp_config_types.h
SET(templateFileName
    "${LCONF_libSpeexDSPDirectory}/include/speex/speexdsp_config_types.h.in")

SET(INCLUDE_STDINT  "#include <stdint.h>")
SET(SIZE16          "int16_t")
SET(USIZE16         "uint16_t")
SET(SIZE32          "int32_t")
SET(USIZE32         "uint32_t")

ACONF_generateConfigurationFileFromTemplate(
       ${templateFileName} "speexdsp_config_types.h" ${libraryShortName})

#-------------------
#--- directories ---
#-------------------

SET(libSpeexDSPSrcDirectory
    ${LCONF_libSpeexDSPDirectory}/libspeexdsp)

SET(libSpeexDSPIncludeDirectoryList
    ${temporarySpeexDSPIncludeDirectory}
    ${LCONF_libSpeexDSPDirectory}/include
    ${libSpeexDSPSrcDirectory}
)

#---------------------
# --- source files ---
#---------------------

SET(srcFileStemList
    buffer fftwrap filterbank jitter kiss_fft kiss_fftr mdf
    preprocess resample scal smallft
)

UTIL_List_constructFromOther(libSpeexDSPSourceFileList
                             srcFileStemList
                             "${libSpeexDSPSrcDirectory}/" ".c")

#--------------------
# --- definitions ---
#--------------------

SET(libSpeexDSPCompileDefinitionList
    HAVE_CONFIG_H
)

#===============
# === Target ===
#===============

LINKER_makeLibraryTarget(${targetName} TRUE)
ADD_DEPENDENCIES(SupportLibraries ${targetName})
