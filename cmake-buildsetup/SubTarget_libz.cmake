# CMAKE file for library libZ to be called when building SoX_ng

SET(targetName "libZ")
SET(targetVersion "1.3.1")

#====================
# === build setup ===
#====================

#-------------------
#--- directories ---
#-------------------

SET(libZSrcDirectory
    ${LCONF_libZDirectory})

SET(libZIncludeDirectoryList
    ${libZSrcDirectory}
)

#---------------------
# --- source files ---
#---------------------

SET(srcFileStemList
    adler32 compress crc32 deflate infback inffast inflate inftrees
    trees uncompr zutil
)

UTIL_List_constructFromOther(libZSourceFileList
                             srcFileStemList
                             "${libZSrcDirectory}/" ".c")

#--------------------
# --- definitions ---
#--------------------

SET(libZCompileDefinitionList
    Z_SOLO
)

#===============
# === Target ===
#===============

LINKER_makeLibraryTarget(${targetName} TRUE)
ADD_DEPENDENCIES(SupportLibraries ${targetName})
