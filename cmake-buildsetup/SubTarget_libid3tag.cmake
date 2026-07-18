# CMAKE file for library libId3tag to be called when building SoX_ng

SET(targetName "libId3tag")
SET(targetVersion "1.3.3")

#=================================================
# === generate variables via autoconfiguration ===
#=================================================

ACONF_setPackageVariables()

# define if you have the ANSI C header files
SET(STDC_HEADERS TRUE)

#------------------------------------------------------------
# define several configuration variables based on existing include
# files

SET(includeFileNameList
    "assert.h" "dlfcn.h" "inttypes.h" "memory.h" "stdint.h" "stdlib.h"
    "string.h" "strings.h" "sys/stat.h" "sys/types.h" "unistd.h"
)

ACONF_checkIncludeFileNameList(includeFileNameList)

#------------------------------------------------------------
# define several configuration variables based on available
# library functions
SET(functionNameList
    "ftruncate"
)

ACONF_checkFunctionNameList(functionNameList)

#------------------------------------------------------------

# build with libz support
SET(usesZLibrary FALSE)

IF("${LCONF_libZDirectory}" STREQUAL "")
    # find libz in standard library path
    CHECK_LIBRARY_EXISTS(z uncompress "" usesZLibrary)
ELSE()
    SET(usesZLibrary TRUE)
ENDIF()

SET(HAVE_LIB_Z ${usesZLibrary})

#====================
# === build setup ===
#====================

#-----------------------------------
#--- generate configuration file ---
#-----------------------------------

ACONF_generateConfigurationFile(Id3tag) 

#-------------------
#--- directories ---
#-------------------

SET(libId3tagSrcDirectory
    ${LCONF_libId3tagDirectory})

SET(libId3tagIncludeDirectoryList
    ${temporaryId3tagIncludeDirectory}
    ${libId3tagSrcDirectory}
)

IF(${usesZLibrary})
    LIST(APPEND libId3tagIncludeDirectoryList
         ${LCONF_libZDirectory})
ENDIF()

#---------------------
# --- source files ---
#---------------------

SET(srcFileStemList
    compat crc debug field file frame frametype genre latin1 parse
    render tag ucs4 utf16 utf8 util version
)

UTIL_List_constructFromOther(libId3tagSourceFileList
                             srcFileStemList
                             "${libId3tagSrcDirectory}/" ".c")

#--------------------
# --- definitions ---
#--------------------

SET(libId3tagCompileDefinitionList
    HAVE_CONFIG_H
)

#===============
# === Target ===
#===============

LINKER_makeLibraryTarget(${targetName} TRUE)
ADD_DEPENDENCIES(SupportLibraries ${targetName})
