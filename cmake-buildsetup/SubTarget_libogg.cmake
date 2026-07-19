# CMAKE file for library libOgg to be called when building SoX_ng

SET(targetName "libOgg")
SET(targetVersion "1.3.5")

#================================================
#=== generate variables via autoconfiguration ===
#================================================

ACONF_setPackageVariables()

# define if you build with CRC
SET(DISABLE_CRC FALSE)

# define if you have the ANSI C header files
SET(STDC_HEADERS TRUE)

#------------------------------------------------------------
# define several configuration variables based on existing include
# files

SET(includeFileNameList
    "dlfcn.h" "inttypes.h" "memory.h" "stdint.h" "stdlib.h"
    "string.h" "strings.h" "sys/stat.h" "sys/types.h" "unistd.h"
)
    
ACONF_checkIncludeFileNameList(includeFileNameList)

#------------------------------------------------------------
# define configuration variables for type sizes of host machine

SET(typeNameList
    int int16_t int32_t int64_t long "long long" short uint16_t
    uint32_t uint64_t u_int16_t u_int32_t
)

ACONF_collectTypeNameSizesForList(typeNameList)

#===================
#=== build setup ===
#===================

#-----------------------------------
#--- generate configuration file ---
#-----------------------------------

ACONF_generateConfigurationFile(Ogg)

#-----------------------------------------------
# derived variables for types configuration file

UTIL_Bool_setToZeroOrOne(INCLUDE_INTTYPES_H   HAVE_INTTYPES_H)
UTIL_Bool_setToZeroOrOne(INCLUDE_STDINT_H     HAVE_STDINT_H)
UTIL_Bool_setToZeroOrOne(INCLUDE_SYS_TYPES_H  HAVE_STDINT_H)
SET(SIZE16  int16_t)
SET(SIZE32  int32_t)
SET(SIZE64  int64_t)
SET(USIZE16 uint16_t)
SET(USIZE32 uint32_t)
SET(USIZE64 uint64_t)

ACONF_generateConfigurationFileFromTemplate(
    "${LCONF_libOggDirectory}/include/ogg/config_types.h.in"
    "ogg/config_types.h"
    "Ogg"
)

#-------------------
#--- directories ---
#-------------------

SET(libOggSrcDirectory
    ${LCONF_libOggDirectory}/src)

SET(libOggIncludeDirectoryList
    ${temporaryOggIncludeDirectory}
    ${LCONF_libOggDirectory}/include
)

#--------------------
#--- source files ---
#--------------------

SET(srcFileStemList
    bitwise framing
)

UTIL_List_constructFromOther(libOggSourceFileList
                             srcFileStemList
                             "${libOggSrcDirectory}/" ".c")

#-------------------
#--- definitions ---
#-------------------

SET(libOggCompileDefinitionList
    HAVE_CONFIG_H
    LIBOGG_EXPORTS
)

#==============
#=== Target ===
#==============

LINKER_makeLibraryTarget(${targetName} TRUE)
ADD_DEPENDENCIES(SupportLibraries ${targetName})
