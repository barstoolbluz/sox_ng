# CMAKE file for library libPng to be called when building SoX_ng

SET(targetName "libPng")
SET(targetVersion "1.6.44")

#=================================================
# === generate variables via autoconfiguration ===
#=================================================

ACONF_setPackageVariables()

# enable LOONGARCH LSX optimizations
SET(PNG_LOONGARCH_LSX_OPT FALSE)

# define if you have the ANSI C header files
SET(STDC_HEADERS TRUE)

# platform specific optimizations
IF("$CMAKE_HOST_PROCESSOR" STREQUAL "arm")
    SET(PNG_ARM_NEON_API_SUPPORTED TRUE)
    SET(PNG_ARM_NEON_CHECK_SUPPORTED TRUE)
    SET(PNG_ARM_NEON_OPT TRUE)
ELSEIF("$CMAKE_HOST_PROCESSOR" STREQUAL "amd64")
    SET(PNG_INTEL_SSE_OPT TRUE)
ELSEIF("$CMAKE_HOST_PROCESSOR" STREQUAL "mips")
    SET(PNG_MIPS_MMI_API_SUPPORTED TRUE)
    SET(PNG_MIPS_MMI_CHECK_SUPPORTED TRUE)
    SET(PNG_MIPS_MMI_OPT TRUE)
    SET(PNG_MIPS_MSA_API_SUPPORTED TRUE)
    SET(PNG_MIPS_MSA_CHECK_SUPPORTED TRUE)
    SET(PNG_MIPS_MSA_OPT TRUE)
ELSEIF("$CMAKE_HOST_PROCESSOR" STREQUAL "powerpc")
    SET(PNG_POWERPC_VSX_API_SUPPORTED TRUE)
    SET(PNG_POWERPC_VSX_CHECK_SUPPORTED TRUE)
    SET(PNG_POWERPC_VSX_OPT TRUE)
ENDIF()

#------------------------------------------------------------
# define several configuration variables based on existing include
# files

SET(includeFileNameList
    "dlfcn.h" "inttypes.h" "stdint.h" "stdio.h" "stdlib.h" "string.h"
    "strings.h" "sys/stat.h" "sys/types.h" "unistd.h"
)

ACONF_checkIncludeFileNameList(includeFileNameList)

#------------------------------------------------------------
# define several configuration variables based on available
# library functions

SET(functionNameList
    "feenableexcept" "pow"           
)

ACONF_checkFunctionNameList(functionNameList)

#------------------------------------------------------------
# define several configuration variables based on available
# libraries

ACONF_checkForLibrary(HAVE_LIBM math.h m pow)

# build with libz support
SET(usesZLibrary FALSE)

IF("${LCONF_libZDirectory}" STREQUAL "")
    # find libz in standard library path
    CHECK_LIBRARY_EXISTS(z uncompress "" usesZLibrary)
ELSE()
    SET(usesZLibrary TRUE)
ENDIF()

SET(HAVE_LIBZ ${usesZLibrary})

#====================
# === build setup ===
#====================

#-----------------------------------
#--- generate configuration file ---
#-----------------------------------

ACONF_generateConfigurationFile(Png) 

#-------------------
#--- directories ---
#-------------------

SET(libPngSrcDirectory
    ${LCONF_libPngDirectory})

SET(libPngIncludeDirectoryList
    ${temporaryPngIncludeDirectory}
    ${LCONF_libPngDirectory}/include
)

IF(${usesZLibrary})
    LIST(APPEND libPngIncludeDirectoryList
         ${LCONF_libZDirectory})
ENDIF()

#---------------------
# --- source files ---
#---------------------

SET(srcFileStemList
    png pngerror pngget pngmem pngpread pngread pngrio pngrtran
    pngrutil pngset pngtrans pngwio pngwrite pngwtran pngwutil
)

UTIL_List_constructFromOther(libPngSourceFileList
                             srcFileStemList
                             "${libPngSrcDirectory}/" ".c")

# add processor-specific sources
IF("$CMAKE_HOST_PROCESSOR" STREQUAL "arm")
    SET(libPngSrcDirectory_ARM ${libPngSrcDirectory}/arm)

    LIST(APPEND libPngSourceFileList
         ${libPngSrcDirectory_ARM}/arm_init.c
         ${libPngSrcDirectory_ARM}/filter_neon_intrinsics.c
         ${libPngSrcDirectory_ARM}/filter_neon.S
         ${libPngSrcDirectory_ARM}/palette_neon_intrinsics.c)
ELSEIF("$CMAKE_HOST_PROCESSOR" STREQUAL "amd64")
    SET(libPngSrcDirectory_INTEL ${libPngSrcDirectory}/intel)

    LIST(APPEND libPngSourceFileList
         ${libPngSrcDirectory_INTEL}/filter_sse2_intrinsics.c
         ${libPngSrcDirectory_INTEL}/intel_init.c)
ELSEIF("$CMAKE_HOST_PROCESSOR" STREQUAL "mips")
    SET(libPngSrcDirectory_MIPS ${libPngSrcDirectory}/mips)

    LIST(APPEND libPngSourceFileList
         ${libPngSrcDirectory_MIPS}/filter_mmi_inline_assembly.c
         ${libPngSrcDirectory_MIPS}/filter_msa_intrinsics.c
         ${libPngSrcDirectory_MIPS}/mips_init.c)
ELSEIF("$CMAKE_HOST_PROCESSOR" STREQUAL "powerpc")
    SET(libPngSrcDirectory_PPC ${libPngSrcDirectory}/powerpc)

    LIST(APPEND libPngSourceFileList
         ${libPngSrcDirectory_MIPS}/filter_vsx_intrinsics.c
         ${libPngSrcDirectory_MIPS}/powerpc_init.c)
ENDIF()

IF($PNG_LOONGARCH_LSX_OPT)
    SET(libPngSrcDirectory_LOON ${libPngSrcDirectory}/loongarch)

    LIST(APPEND libPngSourceFileList
         ${libPngSrcDirectory_LOON}/filter_lsx_intrinsics.c
         ${libPngSrcDirectory_LOON}/loongarch_init.c)
ENDIF()

#--------------------
# --- definitions ---
#--------------------

SET(libPngCompileDefinitionList
    HAVE_CONFIG_H
)

#===============
# === Target ===
#===============

LINKER_makeLibraryTarget(${targetName} TRUE)
ADD_DEPENDENCIES(SupportLibraries ${targetName})
