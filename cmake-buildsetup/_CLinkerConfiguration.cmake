# -*- coding: utf-8 -*-
#
# Local Settings for the C Linker and Library Manager in CMAKE

#################
### FUNCTIONS ###
#################

MACRO(LINKER_combineLibraryListIntoLib
      targetName libraryDirectoryPath libraryNameStemList)
    # combines static libraries in <libraryNameStemList> into a
    # combined library for <targetName>; all libraries are located in
    # <libraryDirectoryPath>

    SET(extension "${LINKER_staticLibExtension}")

    UTIL_List_constructFromOther(combinedLibraryPathList
                                 libraryNameStemList
                                 "${libraryDirectoryPath}/lib" ${extension})

    SET(libraryFileName
        "${libraryDirectoryPath}/${targetName}${extension}")
    SET(commandMessage "Combining libs into ${targetName}...")

    IF(WINDOWS)
        ADD_CUSTOM_TARGET(${targetName}
                          COMMAND lib
                                  /OUT:${libraryFileName}
                                  ${combinedLibraryPathList}
                          COMMAND_EXPAND_LISTS
                          COMMENT ${commandMessage})
    ELSE()
        ADD_CUSTOM_TARGET(${targetName}
                          COMMAND libtool
                                  --mode=link
                                  ${COMPILER_command}
                                  -static -o ${libraryFileName}
                                  ${combinedLibraryPathList}
                          COMMAND_EXPAND_LISTS
                          COMMENT ${commandMessage})
    ENDIF()
ENDMACRO(LINKER_combineLibraryListIntoLib)

#--------------------

MACRO(LINKER_getLibraryNameListForLibrary
      resultVariableName libraryShortName)
    # returns list of libraries for <libraryShortName> either from
    # PKG_CONFIG or by just lowercasing the short name

    STRING(TOLOWER ${libraryShortName} lowercasedLibraryShortName)

    # the default list of library names is just the lowercased name
    SET(${resultVariableName} "${lowercasedLibraryShortName}")

    IF(${PkgConfig_FOUND})
        # result might change based on package information
        SET(prefix ${libraryShortName})
        SET(pkgFoundVariableName "${prefix}_FOUND")
        SET(libFoundVariableName "${prefix}_LIBRARIES")
        PKG_CHECK_MODULES(${prefix} QUIET
                          ${lowercasedLibraryShortName})

        IF(${pkgFoundVariableName})
            SET(${resultVariableName} ${${libFoundVariableName}})
        ENDIF()
    ENDIF()
ENDMACRO(LINKER_getLibraryNameListForLibrary)

#--------------------

MACRO(LINKER_makeLibraryTarget
      targetName isObjectLibrary)
    # makes support library target named <targetName> based on
    # libXXXSourceFileList, libXXXCompileDefinitionList and
    # libXXXIncludeDirectoryList

    SET(sourceFileListName       "${targetName}SourceFileList")
    SET(definitionListName       "${targetName}CompileDefinitionList")
    SET(includeDirectoryListName "${targetName}IncludeDirectoryList")

    IF(NOT ${isObjectLibrary})
        ADD_LIBRARY(${targetName} STATIC ${${sourceFileListName}})
    ELSE()
        ADD_LIBRARY(${targetName} OBJECT ${${sourceFileListName}})
    ENDIF()

    TARGET_COMPILE_DEFINITIONS(${targetName} PUBLIC
                               ${${definitionListName}})
    TARGET_INCLUDE_DIRECTORIES(${targetName} PUBLIC
                               ${${includeDirectoryListName}})

    IF(NOT WINDOWS)
        # remove lib prefix because it is already contained in target
        # name
        SET_TARGET_PROPERTIES(${targetName} PROPERTIES PREFIX "")
    ENDIF()

    UTIL_Bool_setToInverse(warningsAreEnabled ${isObjectLibrary})
    COMPILER_addSpecificFlags(${targetName} ${warningsAreEnabled})
ENDMACRO(LINKER_makeLibraryTarget)

#============================================================

IF(WINDOWS)
    SET(LINKER_dynamicLibExtension ".dll")
    SET(LINKER_executableExtension ".exe")
    SET(LINKER_staticLibExtension  ".lib")
ELSEIF(MACOS)
    SET(LINKER_dynamicLibExtension ".dylib")
    SET(LINKER_executableExtension "")
    SET(LINKER_staticLibExtension  ".a")
ELSE()
    SET(LINKER_dynamicLibExtension ".so")
    SET(LINKER_executableExtension "")
    SET(LINKER_staticLibExtension  ".a")
ENDIF()

IF(WINDOWS)
    # omit warning for object library LNK4217
    SET(LINKER_cOptions_common "/IGNORE:4217")
ELSE()
    # warn about undefined symbols when linking
    IF(MACOS)
        # LIST(APPEND LINKER_cOptions_common
        #      -Wl,-undefined,error)
    ELSE()
        # LIST(APPEND LINKER_cOptions_common
        #      -Wl,-no-undefined)
    ENDIF()           
ENDIF()

SET(CMAKE_EXE_LINKER_FLAGS ${LINKER_cOptions_common}
    CACHE STRING "" FORCE)
SET(CMAKE_MODULE_LINKER_FLAGS ${LINKER_cOptions_common}
    CACHE STRING "" FORCE)
SET(CMAKE_SHARED_LINKER_FLAGS ${LINKER_cOptions_common}
    CACHE STRING "" FORCE)
SET(CMAKE_STATIC_LINKER_FLAGS ${LINKER_cOptions_common}
    CACHE STRING "" FORCE)
