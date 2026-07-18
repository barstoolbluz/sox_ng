# -*- coding: utf-8 -*-
#
# Global Constants and functions in CMAKE

#################
### CONSTANTS ###
#################

SET(WINDOWS ${WIN32})
SET(MACOS   ${APPLE})

IF(UNIX AND NOT APPLE)
    SET(LINUX 1)
ELSE()
    SET(LINUX 0)
ENDIF()

IF(WINDOWS)
    SET(GLOB_batchFileExtension "bat")
    SET(GLOB_nullDeviceName "NUL")
ELSEIF(LINUX)
    SET(GLOB_batchFileExtension "sh")
    SET(GLOB_nullDeviceName "/dev/null")
ELSE()
    SET(GLOB_batchFileExtension "sh")
    SET(GLOB_nullDeviceName "/dev/null")
ENDIF()

SET(GLOB_platformName
    ${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR})
MESSAGE(STATUS "platformName = " ${GLOB_platformName})

# define standard name of processor
SET(GLOB_systemProcessor ${CMAKE_SYSTEM_PROCESSOR})

IF(GLOB_systemProcessor MATCHES "386$")
    SET(GLOB_systemProcessor "i386")
ELSEIF(GLOB_systemProcessor STREQUAL "AMD64"
   OR GLOB_systemProcessor STREQUAL "X86_64")
    SET(GLOB_systemProcessor "x86_64")
ELSEIF(GLOB_systemProcessor STREQUAL "ARM"
   OR GLOB_systemProcessor STREQUAL "ARM64")
    SET(GLOB_systemProcessor "arm")
ELSEIF(GLOB_systemProcessor STREQUAL "MIPS")
    SET(GLOB_systemProcessor "mips")
ELSEIF(GLOB_systemProcessor STREQUAL "POWERPC")
    SET(GLOB_systemProcessor "powerpc")
ELSEIF(GLOB_systemProcessor STREQUAL "POWERPC64")
    SET(GLOB_systemProcessor "powerpc64")
ELSEIF(GLOB_systemProcessor STREQUAL "SPARC")
    SET(GLOB_systemProcessor "sparc")
ENDIF()

CMAKE_HOST_SYSTEM_INFORMATION(RESULT GLOB_processorHasSSE
                              QUERY HAS_SSE)

CMAKE_HOST_SYSTEM_INFORMATION(RESULT GLOB_processorHasSSE2
                              QUERY HAS_SSE2)

#-------------------
#--- directories ---
#-------------------

# the subdirectory for the configuration used
CMAKE_PATH(SET GLOB_buildDirectory NORMALIZE
           ${CMAKE_CURRENT_BINARY_DIR})

IF(GENERATOR_IS_MULTI_CONFIG)
    CMAKE_PATH(SET GLOB_configurationBuildDirectory NORMALIZE
               "${GLOB_buildDirectory}/$<CONFIG>")
ELSE()
    CMAKE_PATH(SET GLOB_configurationBuildDirectory NORMALIZE
               "${GLOB_buildDirectory}/${CMAKE_BUILD_TYPE}")
ENDIF()

IF("${GLOB_projectRootDirectory}" STREQUAL "")
    # no definition found, set it to relative directory
    CMAKE_PATH(SET GLOB_projectRootDirectory NORMALIZE
               ${CMAKE_CURRENT_SOURCE_DIR}/..)
ENDIF()
