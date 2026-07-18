# CMAKE file for library MAD to be called when building SoX_ng

SET(targetName "libMad")
SET(targetVersion "0.15.1b")

#================================================
#=== generate variables via autoconfiguration ===
#================================================

ACONF_setPackageVariables()

# define to enable experimental code
SET(EXPERIMENTAL FALSE)

# define to optimize for accuracy over speed
SET(OPT_ACCURACY TRUE)

# define to optimize for speed over accuracy
SET(OPT_SPEED FALSE)

# define to enable a fast subband synthesis approximation optimization
SET(OPT_SSO FALSE)

# define to influence a strict interpretation of the ISO/IEC
# standards, even if this is in opposition with best accepted
# practices
SET(OPT_STRICT FALSE)

# define to true if you have the ANSI C header files
SET(STDC_HEADERS TRUE)

#------------------------------------------------------------
# define configuration variables for endianness and type sizes of host
# machine

SET(typeNameList
    int long "long long" "void*"
)

ACONF_collectTypeNameSizesForList(typeNameList)
TEST_BIG_ENDIAN(WORDS_BIGENDIAN)

#------------------------------------------------------------
# define several configuration variables based on existing include
# files

SET(includeFileNameList
    "assert.h" "dlfcn.h" "errno.h" "fcntl.h" "inttypes.h" "limits.h"
    "memory.h" "stdint.h" "stdlib.h" "string.h" "strings.h"
    "sys/stat.h" "sys/types.h" "sys/wait.h" "unistd.h"
)

ACONF_checkIncludeFileNameList(includeFileNameList)

#------------------------------------------------------------
# define several configuration variables based on available
# library functions

SET(functionNameList
    "fcntl" "fork" "pipe" "waitpid"
)

ACONF_checkFunctionNameList(functionNameList)

#------------------------------------------------------------

IF("$CMAKE_SYSTEM_PROCESSOR" STREQUAL "mips")
    SET(HAVE_MADD_ASM TRUE)
    SET(HAVE_MADD16_ASM TRUE)
ENDIF()

# define floating point mode
IF(CMAKE_SIZEOF_VOID_P EQUAL 8)
    SET(floatingPointMode "FPM_64BIT")
ELSEIF("${GLOB_systemProcessor}" STREQUAL "x86_64")
    SET(floatingPointMode "FPM_INTEL")
ELSEIF("${GLOB_systemProcessor}" STREQUAL "i386")
    SET(floatingPointMode "FPM_INTEL")
ELSEIF("${GLOB_systemProcessor}" STREQUAL "arm")
    SET(floatingPointMode "FPM_ARM")
ELSEIF("${GLOB_systemProcessor}" STREQUAL "mips")
    SET(floatingPointMode "FPM_MIPS")
ELSEIF("${GLOB_systemProcessor}" STREQUAL "powerpc")
    SET(floatingPointMode "FPM_PPC")
ELSEIF("${GLOB_systemProcessor}" STREQUAL "sparc")
    SET(floatingPointMode "FPM_SPARC")
ELSE()
    SET(floatingPointMode "FPM_DEFAULT")
ENDIF()

SET(floatingPointMode "FPM_DEFAULT")
SET(${floatingPointMode} TRUE)

#===================
#=== build setup ===
#===================

#-----------------------------------
#--- generate configuration file ---
#-----------------------------------

ACONF_generateConfigurationFile(Mad) 

#-------------------
#--- directories ---
#--------------------

SET(libMadSrcDirectory
    ${LCONF_libMadDirectory}
)

SET(libMadIncludeDirectoryList
    ${temporaryMadIncludeDirectory}
    ${libMadSrcDirectory}
)

#--------------------
#--- source files ---
#--------------------

SET(srcFileStemList
    bit decoder fixed frame huffman layer12 layer3 stream synth timer
    version
)

UTIL_List_constructFromOther(libMadSourceFileList
                             srcFileStemList
                             "${libMadSrcDirectory}/" ".c")

#-------------------
#--- definitions ---
#-------------------

SET(libMadCompileDefinitionList
    ${floatingPointMode} )

#==============
#=== Target ===
#==============

LINKER_makeLibraryTarget(${targetName} TRUE)
ADD_DEPENDENCIES(SupportLibraries ${targetName})
