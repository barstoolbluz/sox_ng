# CMAKE file for library libVorbis to be called when building SoX_ng

SET(targetName "libVorbis")
SET(targetVersion "1.3.7")

# === consistency checks ===

IF(NOT hasLocalLibOgg)
    MESSAGE(STATUS
            "cannot build ${targetName},"
            "required ogg directory is not defined")
    SET(hasLocalLibVorbis FALSE)
    RETURN()
ENDIF()

#=================================================
# === generate variables via autoconfiguration ===
#=================================================

ACONF_setPackageVariables()

# set to true if you have the ANSI C header files
SET(STDC_HEADERS TRUE)

#------------------------------------------------------------
# define several configuration variables based on existing include
# files

SET(includeFileNameList
    "alloca.h" "dlfcn.h" "inttypes.h" "memory.h" "stdint.h" "stdio.h"
    "stdlib.h" "string.h" "strings.h" "sys/stat.h" "sys/types.h"
    "unistd.h"
)

ACONF_checkIncludeFileNameList(includeFileNameList)

#------------------------------------------------------------
# define several configuration variables based on available
# library functions

SET(functionNameList
    "alloca"
)

ACONF_checkFunctionNameList(functionNameList)

SET(C_ALLOCA HAVE_ALLOCA)

#====================
# === build setup ===
#====================

#-----------------------------------
#--- generate configuration file ---
#-----------------------------------

ACONF_generateConfigurationFile(Vorbis) 

#-------------------
#--- directories ---
#-------------------

ACONF_temporaryIncludeDirectory(temporaryOggIncludeDirectory Ogg)               

SET(libVorbisSrcDirectory
    ${LCONF_libVorbisDirectory}/lib)

SET(libVorbisIncludeDirectoryList
    ${temporaryOggIncludeDirectory}
    ${temporaryVorbisIncludeDirectory}
    ${LCONF_libVorbisDirectory}/include
    ${libVorbisSrcDirectory}
    ${LCONF_libOggDirectory}/include
)

#---------------------
# --- source files ---
#---------------------

SET(srcFileStemList
    analysis bitrate block codebook envelope floor0 floor1 info lookup
    lpc lsp mapping0 mdct psy registry res0 sharedbook smallft
    synthesis vorbisenc vorbisfile window
)

UTIL_List_constructFromOther(libVorbisSourceFileList
                             srcFileStemList
                             "${libVorbisSrcDirectory}/" ".c")

#--------------------
# --- definitions ---
#--------------------

SET(libVorbisCompileDefinitionList
    HAVE_CONFIG_H
)

#================
# === Target ===
#================

LINKER_makeLibraryTarget(${targetName} TRUE)
ADD_DEPENDENCIES(SupportLibraries ${targetName})
