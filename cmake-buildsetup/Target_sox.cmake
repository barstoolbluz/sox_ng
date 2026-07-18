# CMAKE file for executable SoX to be called when building SoX_ng

SET(targetName "SoX")
SET(targetVersion ${CMAKE_PROJECT_VERSION})

#============================================================

#--------------------
# --- directories ---
#--------------------

SET(soXIncludeDirectoryList ${GLOB_projectRootDirectory})

#---------------------
# --- source files ---
#---------------------

SET(srcFileStemList
    sox_ng
)

UTIL_List_constructFromOther(soXSourceFileList
                             srcFileStemList
                             "${soxSrcDirectory}/" ".c")

#--------------------
# --- definitions ---
#--------------------

SET(soXCompileDefinitionList
    SOX_IMPORT
)

#===============
# === Target ===
#===============

ADD_EXECUTABLE(${targetName}
               ${soXSourceFileList})
SET_TARGET_PROPERTIES(${targetName} PROPERTIES
                      OUTPUT_NAME "sox_ng")
                  
ADD_DEPENDENCIES(${targetName} libSoX)

TARGET_INCLUDE_DIRECTORIES(${targetName} PUBLIC
                           ${soXIncludeDirectoryList})

TARGET_COMPILE_DEFINITIONS(${targetName} PUBLIC
                           ${soXCompileDefinitionList})

TARGET_LINK_LIBRARIES(${targetName} PUBLIC libSoX)

COMPILER_addSpecificFlags(${targetName} TRUE)
