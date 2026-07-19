# CMAKE file for all the executable SoX examples to be called when
# building SoX_ng

SET(targetName "SoXExamples")
SET(targetVersion ${CMAKE_PROJECT_VERSION})

#============================================================

#--------------------
# --- directories ---
#--------------------

SET(soXExamplesIncludeDirectoryList ${GLOB_projectRootDirectory})

#================
# === Targets ===
#================

ADD_CUSTOM_TARGET(${targetName})
UTIL_Target_setFolder(${targetName} ${folderName_examples})

SET(soxExampleFileIndexList "0;1;2;3;4;5;6")

FOREACH(fileIndex ${soxExampleFileIndexList})
    SET(exampleTargetName "SoXExamples_${fileIndex}")
    SET(mainFileName "${soxSrcDirectory}/example${fileIndex}.c")
    ADD_EXECUTABLE(${exampleTargetName} ${mainFileName})
    ADD_DEPENDENCIES(${exampleTargetName} libSoX)
    TARGET_INCLUDE_DIRECTORIES(${exampleTargetName} PUBLIC
                               ${soXExamplesIncludeDirectoryList})
    TARGET_LINK_LIBRARIES(${exampleTargetName} PUBLIC libSoX)
    COMPILER_addSpecificFlags(${exampleTargetName} TRUE)
    ADD_DEPENDENCIES(${targetName} ${exampleTargetName})

    SET_TARGET_PROPERTIES(${exampleTargetName} PROPERTIES
                          OUTPUT_NAME "sox_ng_example${fileIndex}")
    UTIL_Target_setFolder(${exampleTargetName} ${folderName_examples})
ENDFOREACH()
