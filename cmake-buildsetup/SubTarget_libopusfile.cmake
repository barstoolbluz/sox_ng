# CMAKE file for library libOpusfile to be called when building SoX_ng

SET(targetName "libOpusfile")
SET(targetVersion "0.12")

# === consistency checks ===

IF(NOT hasLocalLibOpus)
    MESSAGE(STATUS
            "cannot build ${targetName},"
            "required opus directory is not defined")
    SET(hasLocalLibOpusfile FALSE)
    RETURN()
ENDIF()

#================================================
#=== generate variables via autoconfiguration ===
#================================================

ACONF_setPackageVariables()

#-----

# define if assertions should be enabled
SET(ENABLE_ASSERTIONS FALSE)

# define if you want to have a non-floating point API
SET(OP_DISABLE_FLOAT_API TRUE)

# define to enable HTTP support
SET(OP_ENABLE_HTTP FALSE)

# define if fixed point should be used instead of float
SET(OP_FIXED_POINT FALSE)

#------------------------------------------------------------
# define several configuration variables based on available
# library functions

SET(functionNameList
    "clock_gettime" "lrintf"
)

ACONF_checkFunctionNameList(functionNameList)

SET(OP_HAVE_CLOCK_GETTIME  HAVE_CLOCK_GETTIME)
SET(OP_HAVE_LRINTF         HAVE_LRINTF)

#===================
#=== build setup ===
#===================

#-----------------------------------
#--- generate configuration file ---
#-----------------------------------

ACONF_generateConfigurationFile(Opusfile)

#-------------------
#--- directories ---
#-------------------

SET(libOpusfileSrcDirectory
    ${LCONF_libOpusfileDirectory}/src)

SET(libOpusfileIncludeDirectoryList
    ${LCONF_libOpusfileDirectory}/include
    ${temporaryOpusfileIncludeDirectory}
    ${LCONF_libOggDirectory}/include
    ${LCONF_libOpusDirectory}/include
)

#--------------------
#--- source files ---
#--------------------

SET(srcFileStemList
    http info internal opusfile stream wincerts
)

UTIL_List_constructFromOther(libOpusfileSourceFileList
                             srcFileStemList
                             "${libOpusfileSrcDirectory}/" ".c")

#-------------------
#--- definitions ---
#-------------------

SET(libOpusfileCompileDefinitionList
    HAVE_CONFIG_H
)

#==============
#=== Target ===
#==============

LINKER_makeLibraryTarget(${targetName} TRUE)
ADD_DEPENDENCIES(SupportLibraries ${targetName})
