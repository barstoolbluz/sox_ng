# CMAKE file for library FLAC to be called when building SoX_ng

SET(targetName "libFlac")
SET(targetVersion "1.3.3")

IF("${libFlacUsesOgg}" STREQUAL "")
    SET(libFlacUsesOgg FALSE)
ENDIF()

# === consistency checks ===

IF("${libFlacUsesOgg}" AND NOT hasLocalLibOgg)
    MESSAGE(STATUS
            "cannot build ${targetName},"
            "required ogg directory is not defined")
    SET(hasLocalLibFlac FALSE)
    RETURN()
ENDIF()

#=================================================
# === generate variables via autoconfiguration ===
#=================================================

ACONF_setPackageVariables()

# disable examples
SET(EXAMPLES FALSE)

# align data blocks
SET(FLAC__ALIGN_MALLOC_DATA TRUE)

# enable ogg linkage
SET(FLAC__HAS_OGG ${libFlacUsesOgg})

# no vararray support
SET(HAVE_CXX_VARARRAYS FALSE)

# set to true if you have the ANSI C header files
SET(STDC_HEADERS TRUE)

#------------------------------------------------------------
# define several configuration variables based on existing include
# files

SET(includeFileNameList
    "byteswap.h" "cpuid.h" "dlfcn.h" "inttypes.h" "langinfo.h"
    "memory.h" "stdint.h" "stdlib.h" "string.h" "strings.h"
    "sys/ioctl.h" "sys/param.h" "sys/stat.h" "sys/types"
    "termio.h" "unistd.h" "x86intrin.h"
)

ACONF_checkIncludeFileNameList(includeFileNameList)

#------------------------------------------------------------
# define several configuration variables based on available
# library functions

SET(functionNameList
    "fseeko" "getopt_long" "iconv"
)

ACONF_checkFunctionNameList(functionNameList)

#------------------------------------------------------------
# define configuration variables for endianness and type sizes of host
# machine

SET(typeNameList
    socklen_t "void*"
)

ACONF_collectTypeNameSizesForList(typeNameList)

TEST_BIG_ENDIAN(WORDS_BIGENDIAN)

#------------------------------------------------------------

IF(WORDS_BIGENDIAN)
    SET(CPU_IS_BIG_ENDIAN TRUE)
ELSE()
    SET(CPU_IS_LITTLE_ENDIAN TRUE)
ENDIF()


IF(GLOB_systemProcessor STREQUAL "x86_64")
    SET(FLAC__CPU_X86_64 TRUE)
ELSEIF(GLOB_systemProcessor STREQUAL "i386")
    SET(FLAC__CPU_IA32 TRUE)
ELSEIF(GLOB_systemProcessor STREQUAL "powerpc")
    SET(FLAC__CPU_PPC TRUE)
ELSEIF(GLOB_systemProcessor STREQUAL "powerpc64")
    SET(FLAC__CPU_PPC TRUE)
    SET(FLAC__CPU_PPC64 TRUE)
ENDIF()

SET(FLAC__HAS_X86INTRIN ${HAVE_X86INTRIN_H})
SET(HAVE_SOCKLEN_T SIZEOF_SOCKLEN_T)

IF(WINDOWS)
    SET(OS_IS_WINDOWS TRUE)
ELSEIF(APPLE)
    SET(FLAC__SYS_DARWIN TRUE)
ELSEIF(LINUX)
    SET(FLAC__SYS_LINUX TRUE)
ENDIF()

#ACONF_checkForLibrary(HAVE_LROUND math.h m lround)
SET(HAVE_LROUND TRUE)

#====================
# === build setup ===
#====================

#-----------------------------------
#--- generate configuration file ---
#-----------------------------------

ACONF_generateConfigurationFile(Flac)

#--------------------
# --- directories ---
#--------------------

SET(libFlacSrcDirectory
    ${LCONF_libFlacDirectory}/src/libFLAC)

SET(libFlacIncludeDirectoryList
    ${temporaryFlacIncludeDirectory}
    ${libFlacSrcDirectory}/include
    ${LCONF_libFlacDirectory}/include
)

IF(${libFlacUsesOgg})
    ACONF_temporaryIncludeDirectory(temporaryOggIncludeDirectory Ogg)
    LIST(APPEND libFlacIncludeDirectoryList
         ${temporaryOggIncludeDirectory}
         ${LCONF_libOggDirectory}/include)
ENDIF()

#---------------------
# --- source files ---
#---------------------

SET(srcFileStemList
    bitmath bitreader bitwriter cpu crc fixed fixed_intrin_sse2
    fixed_intrin_ssse3 float format lpc lpc_intrin_avx2 lpc_intrin_sse
    lpc_intrin_sse2 lpc_intrin_sse41 md5 memory metadata_iterators
    metadata_object ogg_decoder_aspect ogg_encoder_aspect ogg_helper
    ogg_mapping stream_decoder stream_encoder stream_encoder_framing
    stream_encoder_intrin_avx2 stream_encoder_intrin_sse2
    stream_encoder_intrin_ssse3 window
)

IF(WINDOWS)
    LIST(APPEND srcFileStemList windows_unicode_filenames)
ENDIF()

UTIL_List_constructFromOther(libFlacSourceFileList
                             srcFileStemList
                             "${libFlacSrcDirectory}/" ".c")

#--------------------
# --- definitions ---
#--------------------

SET(libFlacCompileDefinitionList
    FLAC__NO_DLL
    FLAC__OVERFLOW_DETECT
    HAVE_CONFIG_H
    PACKAGE_VERSION="${targetVersion}"
)

IF(MACOS)
    LIST(APPEND libFlacCompileDefinitionList
         PRId64="ld"
    )
ENDIF()

#===============
# === Target ===
#===============

LINKER_makeLibraryTarget(${targetName} TRUE)
ADD_DEPENDENCIES(SupportLibraries ${targetName})
