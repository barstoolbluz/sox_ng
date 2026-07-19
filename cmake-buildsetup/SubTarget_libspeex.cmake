# CMAKE file for library libSpeex to be called when building SoX_ng

SET(targetName "libSpeex")

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

# use variable-size arrays
SET(VAR_ARRAYS FALSE)

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
    "alloca.h" "dlfcn.h" "getopt.h" "inttypes.h" "stdint.h" "stdio.h"
    "stdlib.h" "string.h" "strings.h" "sys/audioio.h"
    "sys/soundcard.h" "sys/stat.h" "sys/types.h" "unistd.h"
)

ACONF_checkIncludeFileNameList(includeFileNameList)

SET(USE_ALLOCA HAVE_ALLOCA)

#------------------------------------------------------------
# define several configuration variables based on available
# library functions

SET(functionNameList
    "getopt_long"
)

ACONF_checkFunctionNameList(functionNameList)

#====================
# === build setup ===
#====================

#------------------------------------
#--- generate configuration files ---
#------------------------------------

SET(libraryShortName Speex)

# common config.h
ACONF_generateConfigurationFile(${libraryShortName}) 

# common speex_config_types.h
SET(templateFileName
    "${LCONF_libSpeexDirectory}/include/speex/speex_config_types.h.in")
SET(INCLUDE_STDINT "#include <stdint.h>")
SET(SIZE16         "int16_t")
SET(USIZE16        "uint16_t")
SET(SIZE32         "int32_t")
SET(USIZE32        "uint32_t")

ACONF_generateConfigurationFileFromTemplate(
       ${templateFileName} "speex_config_types.h" ${libraryShortName})

#-------------------
#--- directories ---
#-------------------

SET(libSpeexSrcDirectory
    ${LCONF_libSpeexDirectory}/libspeex)

SET(libSpeexIncludeDirectoryList
    ${temporarySpeexIncludeDirectory}
    ${LCONF_libSpeexDirectory}/include
    ${libSpeexSrcDirectory}
)

#---------------------
# --- source files ---
#---------------------

SET(srcFileStemList
    bits cb_search exc_10_16_table exc_10_32_table exc_20_32_table
    exc_5_256_table exc_5_64_table exc_8_128_table filters gain_table
    gain_table_lbr hexc_10_32_table hexc_table high_lsp_tables lpc lsp
    lsp_tables_nb ltp modes modes_wb nb_celp quant_lsp sb_celp speex
    speex_callbacks speex_header stereo vbr vq window

)

# check for SpeexDSP and add files when not available
IF(NOT hasLocalLibSpeexDSP)
    LIST(APPEND srcFileStemList
         kiss_fft kiss_fftr smallft)
ENDIF()

UTIL_List_constructFromOther(libSpeexSourceFileList
                             srcFileStemList
                             "${libSpeexSrcDirectory}/" ".c")

#--------------------
# --- definitions ---
#--------------------

SET(libSpeexCompileDefinitionList
    HAVE_CONFIG_H
)

#===============
# === Target ===
#===============

LINKER_makeLibraryTarget(${targetName} TRUE)
ADD_DEPENDENCIES(SupportLibraries ${targetName})
