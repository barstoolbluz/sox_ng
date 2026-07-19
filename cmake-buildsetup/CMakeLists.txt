#####################################
# CMAKE file for the SoX_ng project #
#                                   #
# Dr. Thomas Tensi, 2026-02         #
#####################################

CMAKE_MINIMUM_REQUIRED(VERSION 4.0)

PROJECT(SoX_ng
        VERSION 14.7.0
        LANGUAGES C)

# =============================
# === program configuration ===
# =============================

INCLUDE(GNUInstallDirs)

# global includes
INCLUDE(_UtilityFunctions.cmake)
INCLUDE(_GlobalConfiguration.cmake)

# --- define the settings for the C compiler ---
INCLUDE(_CCompilerConfiguration.cmake)

# --- define the settings for the linker and librarian ---
INCLUDE(_CLinkerConfiguration.cmake)

# --- define support for automatic configuration (autoconf emulation)
INCLUDE(_AutomaticConfiguration.cmake)

# project specific includes

# --- process the command line parameters ---
INCLUDE(CommandLineParameters.cmake)

# --- do some local configuration ---
INCLUDE(LocalSettings.cmake)

# --- doxygen (for the developer documentation) ---
FIND_PACKAGE(Doxygen)

# ===================
# === directories ===
# ===================

CMAKE_PATH(SET buildSetupDirectory NORMALIZE
           ${CMAKE_CURRENT_SOURCE_DIR})

CMAKE_PATH(SET buildSetupTemplateDirectory NORMALIZE
           ${buildSetupDirectory}/templates)

# --- source directories ---
CMAKE_PATH(SET soxSrcDirectory NORMALIZE
           ${GLOB_projectRootDirectory}/src)

# --- test file directory ---
CMAKE_PATH(SET testFileDirectory NORMALIZE
           ${GLOB_projectRootDirectory}/test)

# --- add dynamic linkage library for MacOS and Unix ---
IF(NOT WINDOWS)
    LINK_LIBRARIES(${CMAKE_DL_LIBS})
ENDIF(NOT WINDOWS)

# --- enable PkgConfig for MacOS and Unix ---
IF(NOT WINDOWS)
    FIND_PACKAGE(PkgConfig) 
ENDIF(NOT WINDOWS)

#############
# FUNCTIONS #
#############

MACRO(_installSymbolicLink symbolicLinkPath filePath)
    # installs symlink from <symbolicLinkPath> to <filePath>
    
    INSTALL(CODE "EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -E create_symlink \"${filePath}\" \"${symbolicLinkPath}\")")
    INSTALL(CODE "MESSAGE(\"-- Created symlink: ${symbolicLinkPath} -> ${filePath}\")")
ENDMACRO(_installSymbolicLink)

#--------------------

MACRO(appendNameConditionally variableName name)
    # checks hasLocalLibXXX for <name> and, if set, adds <name> to
    # variable with name <variableName>

    SET(conditionVariableName "hasLocalLib${name}")

    IF(${conditionVariableName})
        LIST(APPEND ${variableName} ${name})
    ENDIF()
ENDMACRO(appendNameConditionally)

#--------------------

MACRO(installManPage name manChapter)
    # installs man page for <name> into chapter <manChapter>

    CMAKE_PATH(SET fileName NORMALIZE "${name}_ng.${manChapter}")
    CMAKE_PATH(SET filePath NORMALIZE
               "${GLOB_projectRootDirectory}/${fileName}")
    CMAKE_PATH(SET directory NORMALIZE
               "${CLP_installManPath}/man${manChapter}")
    INSTALL(FILES ${filePath} DESTINATION ${directory})

    IF(CLP_enableReplace)
        CMAKE_PATH(SET newName NORMALIZE "${name}.${manChapter}")
        SET(symbolicLinkPath "${directory}/${newName}")
        SET(filePath         "${directory}/${fileName}")
        _installSymbolicLink(${symbolicLinkPath} ${filePath})
    ENDIF()
ENDMACRO(installManPage)

#--------------------

MACRO(installSymbolicLink kind newName oldName)
    # installs symlink from file <newName> to file <oldName> where
    # <kind> gives the kind of file (exe, lib, man)

    IF("${kind}" STREQUAL "exe")
        SET(directory ${CLP_installBinPath}) 
        SET(extension ${LINKER_executableExtension})
    ELSEIF("${kind}" STREQUAL "lib")
        SET(directory ${CLP_installLibPath}) 
        SET(extension ${LINKER_staticLibraryExtension})
    ENDIF()

    SET(symbolicLinkPath "${directory}/${newName}${extension}")
    SET(filePath         "${directory}/${oldName}${extension}")
    _installSymbolicLink(${symbolicLinkPath} ${filePath})
ENDMACRO(installSymbolicLink)

#--------------------

MACRO(makeDoxygenDocumentation)
    # generates doxygen documentation

    CMAKE_PATH(SET doxygenConfigurationFile NORMALIZE
               ${GLOB_projectRootDirectory}/Doxyfile)
    SET(doxygenFileList
        "${soxSrcDirectory}
        ${GLOB_projectRootDirectory}/Doxymain.md")
    DOXYGEN_ADD_DOCS(libSoXDocumentation
                     ${doxygenFileList}
                     ALL
                     WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
                     COMMENT "Generating API documentation for libSoX"
                     CONFIG_FILE ${doxygenConfigurationFile})
ENDMACRO(makeDoxygenDocumentation)

#--------------------

FUNCTION(processSubtargetConditionally
         targetName parentTargetName languageStandard)
    # includes CMake file for <targetName> when associated library
    # directory is set; <parentTargetName> gives the name of the
    # parent target and <languageStandard> gives the C language
    # standard

    MESSAGE("=== setting up ${targetName} ===")
    STRING(TOLOWER ${targetName} lowercasedTargetName)

    SET(includeFileName "SubTarget_${lowercasedTargetName}.cmake")
    INCLUDE(${includeFileName})
    STRING(SUBSTRING "${targetName}" 3 -1 libraryName)
    UTIL_Target_setFolder(${targetName} ${folderName_supportLibs})
    SET_TARGET_PROPERTIES(${targetName} PROPERTIES
                          C_STANDARD ${languageStandard}
                          C_STANDARD_REQUIRED FALSE)
ENDFUNCTION(processSubtargetConditionally)

###########
# TARGETS #
###########

#----------------
# common settings
#----------------

SET(folderName_examples    "Examples")
SET(folderName_supportLibs "SupportLibs")

# ============================
# === supporting libraries ===
# ============================

SET(supportLibsRootTargetName "SupportLibraries")

ADD_CUSTOM_TARGET(${supportLibsRootTargetName})
UTIL_Target_setFolder(${supportLibsRootTargetName} ${folderName_supportLibs})

# --- process all libraries

SET(libraryShortNameList
    "FFTW" "Flac" "Id3tag" "Mad" "MP3Lame" "Ogg" "Opus" "Opusfile"
    "Png" "Sndfile" "Speex" "SpeexDSP" "Vorbis" "Wavpack" "Z"
)

# --- do consistency checks ---

FOREACH(shortName ${libraryShortNameList})
    SET(configurationVariableName "hasLocalLib${shortName}")
    SET(directoryVariableName "LCONF_lib${shortName}Directory")
    SET(exclusionVariableName "CLP_without${shortName}")
    SET(targetName "lib${shortName}")

    SET(${configurationVariableName} FALSE)
    UTIL_Debug_appendRelevantVariableNames(${configurationVariableName})
    SET(messagePrefix "cannot build ${targetName},")

    IF(${exclusionVariableName})
        MESSAGE(STATUS "${messagePrefix} excluded from build")
    ELSEIF("${${directoryVariableName}}" STRGREATER "")
        SET(${configurationVariableName} TRUE)
    ELSE()
        MESSAGE(STATUS "${messagePrefix} directory not defined")
    ENDIF()
ENDFOREACH()

SET(libFlacUsesOgg TRUE)

# --- traverse the libraries
FOREACH(shortName ${libraryShortNameList})
    SET(configurationVariableName "hasLocalLib${shortName}")

    IF(${configurationVariableName})
        IF("${shortName}" STREQUAL "Mad")
            # Mad is not conforming to C99
            SET(languageStandard 90)
        ELSE()
            SET(languageStandard 99)
        ENDIF()

        processSubtargetConditionally("lib${shortName}"
                                      ${supportLibsRootTargetName}
                                      ${languageStandard})
    ENDIF()
ENDFOREACH()

IF(libFlacUsesOgg AND hasLocalLibFlac)
    ADD_DEPENDENCIES(libFlac libOgg)
ENDIF()

# --- collect active libraries
SET(activeLibraryNameList )

FOREACH(shortName ${libraryShortNameList})
    appendNameConditionally(activeLibraryNameList ${shortName})
ENDFOREACH()

# ======================
# === LibSoX library ===
# ======================

INCLUDE(Target_libsox.cmake)

# ======================
# === SoX executable ===
# ======================

INCLUDE(Target_sox.cmake)

# ====================
# === SoX examples ===
# ====================

INCLUDE(Target_soxExamples.cmake)

# =====================
# DOXYGEN documentation
# =====================

IF(NOT Doxygen_FOUND)
    MESSAGE("Doxygen needs to be installed to generate the documentation")
ELSE()
    makeDoxygenDocumentation()
ENDIF()

################
# INSTALLATION #
################

# === install SoX library ===
INSTALL(TARGETS libSoX DESTINATION ${CLP_installLibPath})

# === install SoX binaries ===
INSTALL(TARGETS SoX DESTINATION ${CLP_installBinPath})
installSymbolicLink(exe soxi_ng sox_ng)
installSymbolicLink(exe play_ng sox_ng)
installSymbolicLink(exe rec_ng  sox_ng)

# === install SoX example binaries ===
FOREACH(fileIndex ${soxExampleFileIndexList})
    SET(exampleTargetName "SoXExamples_${fileIndex}")
    INSTALL(TARGETS ${exampleTargetName} DESTINATION ${CLP_installBinPath})
ENDFOREACH()

# === install renamings of SoX library and binaries ===
IF(CLP_enableReplace)
    installSymbolicLink(lib libsox libsox_ng)
    installSymbolicLink(exe sox  sox_ng)
    installSymbolicLink(exe soxi sox_ng)
    installSymbolicLink(exe play sox_ng)
    installSymbolicLink(exe rec  sox_ng)

    FOREACH(fileIndex ${soxExampleFileIndexList})
        SET(executableFileName "sox_ng_example${fileIndex}")
        SET(linkFileName       "sox_example${fileIndex}")
        installSymbolicLink(exe
                            ${linkFileName}
                            ${executableFileName})
    ENDFOREACH()
ENDIF()

# === install the man pages ===
installManPage(libsoxeffect 3)
installManPage(libsoxformat 3)
installManPage(libsox       3)
installManPage(soxeffect    7)
installManPage(soxformat    7)
installManPage(soxi         1)
installManPage(sox          1)

# === install the doxygen documentation (if any) ===
IF(Doxygen_FOUND)
    CMAKE_PATH(SET doxygenBuildDirectory NORMALIZE
               ${CMAKE_CURRENT_BINARY_DIR}/doc/html)
    CMAKE_PATH(SET soxDoxygenDirectory NORMALIZE
               "${CLP_installDocPath}/sox_doxygen_doc")
    INSTALL(DIRECTORY ${doxygenBuildDirectory}
            DESTINATION ${soxDoxygenDirectory})
ENDIF()

#------------------------------------------------
# write relevant variables to STDOUT if specified
#------------------------------------------------

IF(CLP_Debug_listBuildVariables)
    LIST(SORT relevantVariableNameList CASE INSENSITIVE)
    UTIL_List_showContents("${relevantVariableNameList}")
ENDIF()
