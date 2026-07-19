# CMAKE file for library libWavpack to be called when building SoX_ng

SET(targetName "libWavpack")
SET(targetVersion "5.7.0")

#====================
# === build setup ===
#====================

#-------------------
#--- directories ---
#-------------------

SET(libWavpackSrcDirectory
    ${LCONF_libWavpackDirectory}/src)

SET(libWavpackIncludeDirectoryList
    ${LCONF_libWavpackDirectory}/include
    ${libWavpackSrcDirectory}
)

#-------------------
#--- directories ---
#-------------------

SET(srcFileStemList
    common_utils decorr_utils entropy_utils extra1 extra2
    open_filename open_legacy open_raw open_utils pack pack_dns
    pack_dsd pack_floats pack_utils read_words tags tag_utils unpack
    unpack3 unpack3_open unpack3_seek unpack_dsd unpack_floats
    unpack_seek unpack_utils write_words
)

UTIL_List_constructFromOther(libWavpackSourceFileList
                             srcFileStemList
                             "${libWavpackSrcDirectory}/" ".c")

#--------------------
# --- definitions ---
#--------------------

SET(libWavpackCompileDefinitionList
)

#===============
# === Target ===
#===============

LINKER_makeLibraryTarget(${targetName} TRUE)
ADD_DEPENDENCIES(SupportLibraries ${targetName})
