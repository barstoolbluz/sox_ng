# CMAKE file for library LAME to be called when building SoX_ng

SET(targetName "libMP3Lame")
SET(targetVersion "3.100")

#=================================================
# === generate variables via autoconfiguration ===
#=================================================

ACONF_setPackageVariables()

# define if building universal (internal helper macro)
SET(AC_APPLE_UNIVERSAL_BUILD FALSE)

# define to one of `_getb67', `GETB67', `getb67' for Cray-2 and
# Cray-YMP systems. This function is required for `alloca.c' support
# on those systems.
SET(CRAY_STACKSEG_END FALSE)

# allow to compute a more accurate replaygain value
SET(DECODE_ON_THE_FLY TRUE)

# faster log implementation with less but enough precision
SET(USE_FAST_LOG TRUE)

# double is faster than float on Alpha
IF("$CMAKE_HOST_PROCESSOR" STREQUAL "alpha")
    SET(FLOAT TRUE)
ENDIF()

# build with mpglib support
SET(HAVE_MPGLIB TRUE)

# define if you have the ANSI C header files
SET(STDC_HEADERS TRUE)

#------------------------------------------------------------
# define configuration variables for endianness and type sizes of host
# machine

SET(typeNameList
    double float int long "long double" "long long" short
    "unsigned int" "unsigned long" "unsigned long long" "unsigned short"
)

ACONF_collectTypeNameSizesForList(typeNameList)

SET(HAVE_LONG_DOUBLE ${SIZEOF_LONG_DOUBLE})

IF(${SIZEOF_DOUBLE} LESS ${SIZEOF_LONG_DOUBLE})
    SET(HAVE_LONG_DOUBLE_WIDER TRUE)
ENDIF()

TEST_BIG_ENDIAN(WORDS_BIGENDIAN)

#------------------------------------------------------------
# define several configuration variables based on existing include
# files

SET(includeFileNameList
    "alloca.h" "dlfcn.h" "efence.h" "errno.h" "fcntl.h" "inttypes.h"
    "limits.h" "linux/soundcard.h" "memory.h" "ncurses/termcap.h"
    "stdint.h" "stdlib.h" "string.h" "strings.h" "sys/soundcard.h"
    "sys/stat.h" "sys/time.h" "sys/types.h" "termcap.h" "unistd.h"
)

ACONF_checkIncludeFileNameList(includeFileNameList)

SET(C_ALLOCA ${HAVE_ALLOCA_H})
SET(HAVE_TERMCAP ${HAVE_ALLOCA_H})

# find xmm intrinsics via "xmmintrin.h", but do not rely on it for
# MSVC
IF(COMPILER_isMSVC)
    # CHECK_SYMBOL_EXISTS("_mm_malloc" "malloc.h" HAVE_XMMINTRIN_H)
ELSE()
    ACONF_checkIncludeFileNameList("xmmintrin.h")
ENDIF()

#------------------------------------------------------------
# define several configuration variables based on available
# library functions

SET(functionNameList
    "alloca" "gettimeofday" "iconv" "memcpy" "socket" "strchr"
    "strtol"
)

ACONF_checkFunctionNameList(functionNameList)

# build with sndfile support
SET(usesSndFileLibrary FALSE)

IF("${libSndfileDirectory}" STREQUAL "")
    # find sndfile.h in standard include path
    CHECK_INCLUDE_FILE("sndfile.h" usesSndFileLibrary)
ELSE()
    SET(usesSndFileLibrary TRUE)
ENDIF()

SET(LIBSNDFILE ${usesSndFileLibrary})

#====================
# === build setup ===
#====================

#-----------------------------------
#--- generate configuration file ---
#-----------------------------------

ACONF_generateConfigurationFile(MP3Lame) 

#-------------------
#--- directories ---
#-------------------

SET(libMP3LameSrcDirectory
    ${LCONF_libMP3LameDirectory}/libmp3lame)

SET(libMpgLibSrcDirectory
    ${LCONF_libMP3LameDirectory}/mpglib)

SET(libMP3LameIncludeDirectoryList
    ${temporaryMP3LameIncludeDirectory}
    ${LCONF_libMP3LameDirectory}
    ${LCONF_libMP3LameDirectory}/include
    ${libMpgLibSrcDirectory}
    ${libMP3LameSrcDirectory}
)

IF(${usesSndFileLibrary})
    LIST(APPEND libMP3LameIncludeDirectoryList
         ${libSndfileDirectory}/src)
ENDIF()

#---------------------
# --- source files ---
#---------------------

SET(srcFileStemList
    bitstream encoder fft gain_analysis id3tag lame mpglib_interface
    newmdct presets psymodel quantize quantize_pvt reservoir set_get
    tables takehiro util vbrquantize VbrTag version
)

UTIL_List_constructFromOther(libMP3LameSourceFileList
                             srcFileStemList
                             "${libMP3LameSrcDirectory}/" ".c")


SET(srcFileStemList
    common dct64_i386 decode_i386 interface layer1 layer2 layer3
    tabinit
)

UTIL_List_constructFromOther(libMpgLibSourceFileList
                             srcFileStemList
                             "${libMpgLibSrcDirectory}/" ".c")

LIST(APPEND libMP3LameSourceFileList ${libMpgLibSourceFileList})

#--------------------
# --- definitions ---
#--------------------

SET(libMP3LameCompileDefinitionList
    HAVE_CONFIG_H
)

#===============
# === Target ===
#===============

LINKER_makeLibraryTarget(${targetName} TRUE)
ADD_DEPENDENCIES(SupportLibraries ${targetName})
