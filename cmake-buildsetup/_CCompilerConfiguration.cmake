# -*- coding: utf-8 -*-
#
# Local Settings for the C-Compiler in CMAKE
#

SET(COMPILER_isCLANG 0)
SET(COMPILER_isGCC   0)
SET(COMPILER_isMSVC  0)
SET(COMPILER_command "")

IF (CMAKE_C_COMPILER_ID MATCHES ".*Clang")
    SET(COMPILER_isCLANG 1)
    SET(COMPILER_command "clang")
ELSEIF(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    SET(COMPILER_isGCC   1)
    SET(COMPILER_command "gcc")
ELSEIF(CMAKE_C_COMPILER_ID STREQUAL "MSVC")
    SET(COMPILER_isMSVC  1)
    SET(COMPILER_command "cl")
ENDIF()

IF(COMPILER_isMSVC)
    SET(COMPILER_Warning_disablingPrefix /wd)
    SET(COMPILER_Warning_disableAll /W0)
    SET(COMPILER_Warning_maximumLevel /W4)
ELSE()
    SET(COMPILER_Warning_disablingPrefix -Wno-)
    SET(COMPILER_Warning_disableAll -w)
    SET(COMPILER_Warning_maximumLevel -Wall)
ENDIF()

# #################
# ### FUNCTIONS ###
# #################

FUNCTION(COMPILER_addSpecificFlags targetName warningsAreEnabled)
    # adds specific compiler flags to target <targetName> taking into
    # account whether warnings are enabled via <warningsAreEnabled>

    MESSAGE(STATUS
            "target " ${targetName}
            ": compiler warnings = " ${warningsAreEnabled})

    # definitions
    TARGET_COMPILE_DEFINITIONS(${targetName} PRIVATE
                               ${COMPILER_cDefinitions_common})

    TARGET_COMPILE_DEFINITIONS(${targetName} PRIVATE
                               $<IF:$<CONFIG:Release>,NDEBUG,DEBUG>)

    # options
    TARGET_COMPILE_OPTIONS(${targetName} PRIVATE
                           ${COMPILER_cOptions_common})

    IF(warningsAreEnabled)
        TARGET_COMPILE_OPTIONS(${targetName} PRIVATE
                               ${COMPILER_warningOptions})
    ELSE()
        TARGET_COMPILE_OPTIONS(${targetName} PRIVATE
                               ${COMPILER_Warning_disableAll})
    ENDIF()

    TARGET_COMPILE_OPTIONS(${targetName} PRIVATE
                           $<$<CONFIG:Debug>:${COMPILER_cOptions_debug}>)
    TARGET_COMPILE_OPTIONS(${targetName} PRIVATE
                           $<$<CONFIG:Release>:${COMPILER_cOptions_release}>)
    TARGET_COMPILE_OPTIONS(${targetName} PRIVATE
                           $<$<CONFIG:RelWithDebInfo>:${COMPILER_cOptions_relWithDebInfo}>)
ENDFUNCTION(COMPILER_addSpecificFlags)

#--------------------

FUNCTION(COMPILER_appendToCommonDefineClauses)
    # appends all define clauses common to all build configurations

    LIST(APPEND COMPILER_cDefinitions_common
        _LIB
        _UNICODE
        UNICODE
    )
    
    # --- add specific settings per platform ---
    IF(WINDOWS)
        LIST(APPEND COMPILER_cDefinitions_common
             _CRT_SECURE_NO_WARNINGS
             _WINDOWS
             _WINDLL
             WIN32
        )
    ENDIF()

    IF(MACOS)
        LIST(APPEND COMPILER_cDefinitions_common
             APPLE
        )
    ENDIF()

    IF(LINUX)
        LIST(APPEND COMPILER_cDefinitions_common
             LINUX=1
             UNIX
        )
    ENDIF()

    # --- combine defines into single list ---
    SET(COMPILER_cDefinitions_common
        ${COMPILER_cDefinitions_common} PARENT_SCOPE)
ENDFUNCTION(COMPILER_appendToCommonDefineClauses)

#--------------------

FUNCTION(COMPILER_setSupportedProgrammingLanguages)
    # defines programming languages supported for this build

    SET(CMAKE_C_STANDARD            90 PARENT_SCOPE)
    SET(CMAKE_C_STANDARD_REQUIRED True PARENT_SCOPE)

    IF(MACOS)
        ENABLE_LANGUAGE(OBJC)
    ENDIF()
ENDFUNCTION(COMPILER_setSupportedProgrammingLanguages)

#--------------------

FUNCTION(COMPILER_setCommonAndReleaseWarnings)
    # calculates warnings for common and release builds and returns
    # them as <COMPILER_warningOptions_common> and
    # <COMPILER_warningOptions_release>

    IF(COMPILER_isMSVC)
        # --- list of warning numbers to be ignored
        SET(COMPILER_ignoredWarningList_common
              4005 # macro redefinition
              4013 # undefined function, assuming extern int
              4068 # unknown pragma
              4100 # unreferenced formal parameter
              4101 # unreferenced local variable
              4125 # decimal digit terminates octal sequence
              4131 # old style declarator
              4189 # local variable initialized but not referenced
              4210 # function given file scope
              4232 # adress of dllimport not static
              4244 # loss of data for return conversion
              4245 # loss of data for return conversion
              4267 # loss of data for conversion
              4273 # inconsistent dll linkage
              4305 # truncation from double to const float
              4324 # structure padded due to alignment
              4456 # declaration hides previous declaration
              4477 # bad numeric format string
              4701 # potentially uninitialized local variable
              4702 # unreachable code
              4703 # potentially uninitialized local pointer
              4996 # deprecated POSIX name
              6001 # using uninitialized memory
              6031 # return value ignored
             28251 # inconsistent annotation
        )

        SET(COMPILER_ignoredWarningList_debug
        )

        SET(COMPILER_ignoredWarningList_release
              4723 # potential divide by 0
        )
    ELSE()
        # --- list of warnings to be ignored
        SET(COMPILER_ignoredWarningList_common
             address                 # impossible null pointer
             format                  # bad print format
             ignored-qualifiers      # const qualifier on functions
             unknown-pragmas         # unknown pragma
             unused-function         # unused function
        )

        SET(COMPILER_ignoredWarningList_release
             unused-variable         # remove warning for unused variable
        )

        IF(COMPILER_isGCC)
            LIST(APPEND COMPILER_ignoredWarningList_common
                 parentheses              # remove recommended parentheses
                 unused-but-set-variable  # unused variable that is set
            )
        ENDIF()

        IF(COMPILER_isCLANG)
            LIST(APPEND COMPILER_ignoredWarningList_common
                 c99-extensions                # C99 extensions
                 logical-op-parentheses        # recommended parentheses
                                               # in logical expressions
                 macro-redefined               # redefined macro
                 nan-infinity-disabled         # infinity macro
                 implicit-function-declaration # implicit functions
            )
        ENDIF()
    ENDIF()

    LIST(APPEND COMPILER_warningOptions_common
         ${COMPILER_Warning_maximumLevel})

    UTIL_List_appendOtherTransformed(COMPILER_warningOptions_common
                                     COMPILER_ignoredWarningList_common
                                     "${COMPILER_Warning_disablingPrefix}"
                                     "")

    UTIL_List_constructFromOther(COMPILER_warningOptions_release
                                 COMPILER_ignoredWarningList_release
                                 "${COMPILER_Warning_disablingPrefix}"
                                 "")

    SET(COMPILER_warningOptions_common
        ${COMPILER_warningOptions_common} PARENT_SCOPE)
    SET(COMPILER_warningOptions_release
        ${COMPILER_warningOptions_release} PARENT_SCOPE)
ENDFUNCTION(COMPILER_setCommonAndReleaseWarnings)

# ########################################

# --- set languages to C and Objective-C (for MacOS)
COMPILER_setSupportedProgrammingLanguages()

# --- append to settings from specific effect suite
COMPILER_appendToCommonDefineClauses()

# --- collect warnings
COMPILER_setCommonAndReleaseWarnings()

# --- define flags per compiler ---
IF(COMPILER_isMSVC)
    LIST(APPEND COMPILER_cOptions_common
         /bigobj              # increase number of addressable sections
         /diagnostics:column  # format of diagnostics message
         /EHsc                # exception handling: stack unwinding
         /Gd                  # cdecl calling convention
         /GS                  # buffers security check
         /MP                  # multi processor compilation
         /nologo              # suppress display of banner
         /permissive-         # set strict standard conformance
         /Zc:forScope         # standard conformance for scoping
         /Zc:inline           # remove unreferenced functions
         /Zc:preprocessor     # conforming preprocessor
         /Zc:wchar_t          # wchar is native
    )

    LIST(APPEND COMPILER_cOptions_release
         /Gw                  # global program optimization
         /O2                  # generate fast code
         /Qpar                # enables loop parallelization
    )

    LIST(APPEND COMPILER_cOptions_relWithDebInfo
         /Z7                  # debug information in file
         /Od                  # no optimization
    )

    LIST(APPEND COMPILER_cOptions_debug
         /Od                  # no optimization
         /Zi                  # debug information in database
    )
ELSE()
    LIST(APPEND COMPILER_cOptions_common
         -fvisibility=hidden          # default symbol visibility is
                                      # hidden
         -O0                          # no optimization
         -pedantic                    # set strict standard conformance
    )

    LIST(APPEND COMPILER_cOptions_release
         -O3                  # extreme optimization
    )

    LIST(APPEND COMPILER_cOptions_relWithDebInfo
         -Og                  # debugging compatible optimization
         -g                   # debug information in object files
    )

    LIST(APPEND COMPILER_cOptions_debug
         -Og                  # debugging compatible optimization
         -g                   # debug information in object files
    )
ENDIF()

LIST(APPEND COMPILER_warningOptions
     ${COMPILER_warningOptions_common}
     $<$<CONFIG:Debug>:${COMPILER_warningOptions_debug}>
     $<$<CONFIG:Release>:${COMPILER_warningOptions_release}>)
