# Automatic Configuration Support (emulating autoconf)

# include several CMake support libraries
INCLUDE(CheckFunctionExists)
INCLUDE(CheckIncludeFile)
INCLUDE(CheckLibraryExists)
INCLUDE(CMakePushCheckState)
INCLUDE(CheckSourceCompiles)
INCLUDE(CheckSymbolExists)
INCLUDE(CheckTypeSize)
INCLUDE(TestBigEndian)

#=========================
#=== Macro Definitions ===
#=========================

MACRO(ACONF_checkForLibrary
      resultVariableName headerFileName
      libraryNameList functionNameList)
    # checks whether header named <headerFileName> exists, and whether
    # all libraries in <libraryNameList> exist and contain the
    # corresponding function from <functionNameList>; if this
    # succeeds, <variable> is set to TRUE

    SET(listA "${libraryNameList}")
    SET(listB "${functionNameList}")
    UNSET(isOkay CACHE)
    CHECK_INCLUDE_FILE(${headerFileName} isOkay)

    FOREACH(libraryName functionName IN ZIP_LISTS listA listB)
    # FOREACH(libraryName functionName
    #         IN ZIP_LISTS libraryNameList functionNameList)
        IF(isOkay)
            UNSET(isOkay CACHE)
            CHECK_LIBRARY_EXISTS(${libraryName} ${functionName} "" isOkay)
        ENDIF()
    ENDFOREACH()        

    UNSET(${resultVariableName} CACHE)

    IF(${isOkay})
        SET(${resultVariableName} TRUE)
    ENDIF()
ENDMACRO(ACONF_checkForLibrary)

#--------------------

MACRO(ACONF_checkIncludeFileNameList
      fileNameListVariable)
    # traverses all file names in variable named
    # <fileNameListVariable> and sets HAVE_XXX_H variable accordingly
    # when include file is found in include path

    FOREACH(fileName ${${fileNameListVariable}})
        STRING(REPLACE "." "_" variableSuffix ${fileName})
        STRING(REPLACE "/" "_" variableSuffix ${variableSuffix})
        STRING(TOUPPER ${variableSuffix} variableSuffix)
        SET(configurationVariable "HAVE_${variableSuffix}")

        IF(NOT ("${fileName}" STREQUAL "inttypes.h"))
            CHECK_INCLUDE_FILE(${fileName} ${configurationVariable})
        ELSE()
            # do a specific check to take care of dummy inttypes.h
            # files
            CHECK_SYMBOL_EXISTS("PRIu32"
                                ${fileName} ${configurationVariable})
        ENDIF()
    ENDFOREACH()
ENDMACRO(ACONF_checkIncludeFileNameList)

#--------------------

MACRO(ACONF_checkFunctionNameList
      functionNameListVariable)
    # traverses all function names in variable named
    # <functionNameListVariable> and sets HAVE_XXX variable
    # accordingly when function is found

    FOREACH(functionName ${${functionNameListVariable}})
        STRING(TOUPPER ${functionName} variableSuffix)
        SET(configurationVariable "HAVE_${variableSuffix}")
        CHECK_FUNCTION_EXISTS(${functionName} ${configurationVariable})
    ENDFOREACH()
ENDMACRO(ACONF_checkFunctionNameList)

#--------------------

MACRO(ACONF_checkEnumSymbolNameList
      headerFilePath enumSymbolNameListVariable)
    # traverses all enumeration symbol names in variable named
    # <enumSymbolNameListVariable> and sets HAVE_XXX variable
    # accordingly when enumeration symbol is found in file named
    # <headerFilePath>

    FOREACH(enumSymbolName ${${enumSymbolNameListVariable}})
        STRING(TOUPPER ${enumSymbolName} variableSuffix)
        SET(configurationVariable "HAVE_${variableSuffix}")

        # TODO: implement real check (check_symbol_exists does not
        # work for enums), for now assume that value is defined
        SET(${configurationVariable} TRUE)
        # END_TODO
    ENDFOREACH()
ENDMACRO(ACONF_checkEnumSymbolNameList)

#--------------------

MACRO(ACONF_collectTypeNameSizesForList
      typeNameListVariable)
    # traverses all type names in variable named
    # <typeNameListVariable> and sets SIZEOF_XXX variable accordingly
    # when type size is found

    FOREACH(typeName ${${typeNameListVariable}})
        STRING(REPLACE "*" "_P" variableSuffix ${typeName})
        STRING(REPLACE " " "_" variableSuffix ${variableSuffix})
        STRING(TOUPPER ${variableSuffix} variableSuffix)
        SET(haveConfigurationVariable "HAVE_${variableSuffix}")
        SET(sizeConfigurationVariable "SIZEOF_${variableSuffix}")

        CHECK_TYPE_SIZE("${typeName}" ${sizeConfigurationVariable})

        IF("${${sizeConfigurationVariable}}" STRGREATER "")
            SET(${haveConfigurationVariable} TRUE)
        ENDIF()
    ENDFOREACH()
ENDMACRO(ACONF_collectTypeNameSizesForList)

#--------------------

MACRO(ACONF_generateConfigurationFile
      libraryShortName)
    # generate configuration file config.h from
    # libXXX-config.h_(cmake) and stores it in
    # temporaryXXXIncludeDirectory

    STRING(TOLOWER ${libraryShortName} lowercasedLibraryShortName)
    SET(libraryName "lib${lowercasedLibraryShortName}")
    SET(configurationTemplateName
        "${buildSetupTemplateDirectory}/${libraryName}-config.h_(cmake)")
    ACONF_generateConfigurationFileFromTemplate(
        ${configurationTemplateName}
        "config.h"
        ${libraryShortName})
ENDMACRO(ACONF_generateConfigurationFile)

#--------------------

MACRO(ACONF_generateConfigurationFileFromTemplate
      templateFileName destinationFileName libraryShortName)
    # generate configuration file named <destinationFileName> (located
    # in temporary include directory) from template file named
    # <templateFileName> and stores it in temporaryXXXIncludeDirectory

    SET(includeDirectoryVariableName
        "temporary${libraryShortName}IncludeDirectory")
    ACONF_temporaryIncludeDirectory(${includeDirectoryVariableName}
                                    ${libraryShortName})
    SET(destinationFilePath
        "${${includeDirectoryVariableName}}/${destinationFileName}")
    CONFIGURE_FILE(${templateFileName} ${destinationFilePath} @ONLY)
ENDMACRO(ACONF_generateConfigurationFileFromTemplate)

#--------------------

MACRO(ACONF_setPackageVariables)
    # sets standard package variables to data from <targetName> and
    # <targetVersion>

    SET(PACKAGE          "${targetName}")
    SET(PACKAGE_NAME     "${targetName}")
    SET(PACKAGE_STRING   "${targetName}-${targetVersion}")
    SET(PACKAGE_VERSION  "${targetVersion}")
    SET(VERSION          "${targetVersion}")
    SET(HOST_TRIPLET
        ${CMAKE_SYSTEM_PROCESSOR}-xxx-${CMAKE_SYSTEM_NAME})
ENDMACRO(ACONF_setPackageVariables)

#--------------------

MACRO(ACONF_temporaryIncludeDirectory
      destinationVariableName libraryShortName)
    # sets variable named <destinationVariableName> to temporary
    # include directory for library named <libraryShortName>

    SET(${destinationVariableName}
        ${CMAKE_BINARY_DIR}/lib${libraryShortName}_include)
ENDMACRO(ACONF_temporaryIncludeDirectory)
