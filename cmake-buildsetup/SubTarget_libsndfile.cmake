# CMAKE file for library libSndfile to be called when building SoX_ng

SET(targetName "libSndfile")
SET(targetVersion "1.2.2")

#=================================================
# === generate variables via autoconfiguration ===
#=================================================

ACONF_setPackageVariables()

# define if building universal (internal helper macro)
SET(AC_APPLE_UNIVERSAL_BUILD FALSE)

# set to true if the compiler is GNU GCC
SET(COMPILER_IS_GCC ${COMPILER_isGCC})

# host processor clips on positive or negative float to int conversion
SET(CPU_CLIPS_NEGATIVE FALSE)
SET(CPU_CLIPS_POSITIVE FALSE)

# set to true to enable experimental code
SET(ENABLE_EXPERIMENTAL_CODE FALSE)

# set to 1 if flac, ogg, vorbis, and opus are available
SET(HAVE_EXTERNAL_XIPH_LIBS FALSE)

# will be set to 1 if lame, mpg123 mpeg support is available
SET(HAVE_MPEG FALSE)

# set to true if you have libsqlite3
SET(HAVE_SQLITE3 FALSE)

IF(CMAKE_SYSTEM_NAME STREQUAL OpenBSD)
    SET(OS_IS_OPENBSD TRUE)
ELSEIF(WINDOWS)
    SET(OS_IS_WIN32 TRUE)
    SET(USE_WINDOWS_API TRUE)
    SET(WIN32_TARGET_DLL FALSE)
ENDIF()

# set to true if you have the ANSI C header files
SET(STDC_HEADERS TRUE)

# set to true if SSE2 is enabled
SET(USE_SSE2 FALSE)

#------------------------------------------------------------
# define several configuration variables based on existing include
# files

SET(includeFileNameList
    "assert.h" "alsa/asoundlib.h" "byteswap.h" "direct.h" "dlfcn.h"
    "endian.h" "immintrin.h" "inttypes.h" "io.h" "locale.h"
    "minix/config.h" "memory.h" "sndio.h" "stdbool.h" "stdint.h"
    "stdio.h" "stdlib.h" "string.h" "strings.h" "sys/stat.h"
    "sys/time.h" "sys/types.h" "sys/wait.h" "unistd.h" "wchar.h"
)

ACONF_checkIncludeFileNameList(includeFileNameList)

SET(HAVE_ALSA ${HAVE_ALSA_ASOUNDLIB_H})

#------------------------------------------------------------
# define several configuration variables based on available
# library functions

SET(functionNameList
    "calloc" "ceil" "floor" "fmod" "free" "fstat" "fsync" "ftruncate"
    "getpageize" "gettimeofday" "gmtime" "gmtime_r" "localtime"
    "localtime_r" "lrint" "lrintf" "lseek" "lseek64" "malloc" "mmap"
    "open" "pipe" "read" "realloc" "setlocale" "snprintf" "vsnprintf"
    "waitpid" "write"
)

ACONF_checkFunctionNameList(functionNameList)

CHECK_FUNCTION_EXISTS("S_IRGRP" HAVE_DECL_S_IRGRP)

#------------------------------------------------------------
# define configuration variables for endianness and type sizes of host
# machine

SET(typeNameList
    double float int int64_t long "long long" off_t short size_t
    ssize_t "void*" wchar_t
)

ACONF_collectTypeNameSizesForList(typeNameList)

TEST_BIG_ENDIAN(WORDS_BIGENDIAN)

#------------------------------------------------------------

IF(WORDS_BIGENDIAN)
    SET(CPU_IS_BIG_ENDIAN TRUE)
ELSE()
    SET(CPU_IS_LITTLE_ENDIAN TRUE)
ENDIF()

#====================
# === build setup ===
#====================

#-----------------------------------
#--- generate configuration file ---
#-----------------------------------

ACONF_generateConfigurationFile(Sndfile) 

#-------------------
#--- directories ---
#-------------------

SET(libSndfileSrcDirectory
    ${LCONF_libSndfileDirectory}/src)

SET(libSndfileIncludeDirectoryList
    ${temporarySndfileIncludeDirectory}
    ${LCONF_libSndfileDirectory}/include
    ${libSndfileSrcDirectory}
    ${libSndfileSrcDirectory}/ALAC
    ${libSndfileSrcDirectory}/G72x
)

IF(${usesZLibrary})
    LIST(APPEND libSndfileIncludeDirectoryList
         ${libZDirectory})
ENDIF()

#---------------------
# --- source files ---
#---------------------

SET(srcFileStemList
    ag_dec ag_enc ALACBitUtilities alac_decoder alac_encoder dp_dec
    dp_enc matrix_dec matrix_enc
)

UTIL_List_constructFromOther(libSndfileSourceFileList_ALAC
                             srcFileStemList
                             "${libSndfileSrcDirectory}/ALAC/" ".c")

SET(srcFileStemList
    g721 g723_16 g723_24 g723_40 g72x
)

UTIL_List_constructFromOther(libSndfileSourceFileList_G72x
                             srcFileStemList
                             "${libSndfileSrcDirectory}/G72x/" ".c")

SET(srcFileStemList
    add code decode gsm_create gsm_decode gsm_destroy gsm_encode
    gsm_option long_term lpc preprocess rpe short_term table
)

UTIL_List_constructFromOther(libSndfileSourceFileList_GSM610
                             srcFileStemList
                             "${libSndfileSrcDirectory}/GSM610/" ".c")

SET(srcFileStemList
    aiff alac alaw au audio_detect avr broadcast caf cart chanmap
    chunk command common dither double64 dwd dwvw file_io flac float32
    g72x gsm610 htk id3 ima_adpcm ima_oki_adpcm interleave ircam macos
    mat4 mat5 mpc2k mpeg mpeg_decode mpeg_l3_encode ms_adpcm nist
    nms_adpcm ogg ogg_opus ogg_pcm ogg_speex ogg_vcomment ogg_vorbis
    paf pcm pvf raw rf64 rx2 sd2 sds sndfile strings svx txw ulaw voc
    vox_adpcm w64 wav wavlike windows wve xi
)

UTIL_List_constructFromOther(libSndfileSourceFileList
                             srcFileStemList
                             "${libSndfileSrcDirectory}/" ".c")

LIST(APPEND libSndfileSourceFileList
     ${libSndfileSourceFileList_ALAC}
     ${libSndfileSourceFileList_G72x}
     ${libSndfileSourceFileList_GSM610})

#--------------------
# --- definitions ---
#--------------------

SET(libSndfileCompileDefinitionList
    HAVE_CONFIG_H
)

# add missing formats from inttypes.h (if inttypes.h is not available)
LIST(APPEND libSndfileCompileDefinitionList
     PRIi32="d"
     PRIu32="d"
)

IF(MACOS)
    # HACK: ensure that "io.h" file is available as an include file in
    # temporary sndfile directory
    SET(uioFileName "${CMAKE_OSX_SYSROOT}/usr/include/sys/uio.h")
    SET(ioFileName "${temporarySndfileIncludeDirectory}/io.h")
    FILE(COPY_FILE ${uioFileName} ${ioFileName})
ENDIF()

#===============
# === Target ===
#===============

LINKER_makeLibraryTarget(${targetName} TRUE)
ADD_DEPENDENCIES(SupportLibraries ${targetName})
