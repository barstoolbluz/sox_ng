# Local Configuration for SoX_ng
# typically defines locations of supporting libraries

CMAKE_PATH(SET LCONF_allProjectsRootDirectory NORMALIZE
           ${CMAKE_CURRENT_SOURCE_DIR}/../..)

# the variables LCONF_<<xxx>>Directory define the locations of the
# library sources to be used by later build steps; if a directory is
# not set it is assumed that its includes and libraries are available
# via the standard system paths
CMAKE_PATH(SET LCONF_libFFTWDirectory NORMALIZE
           ${LCONF_allProjectsRootDirectory}/libfftw)
CMAKE_PATH(SET LCONF_libFlacDirectory NORMALIZE
           ${LCONF_allProjectsRootDirectory}/libflac)
CMAKE_PATH(SET LCONF_libId3tagDirectory NORMALIZE
           ${LCONF_allProjectsRootDirectory}/libid3tag)
CMAKE_PATH(SET LCONF_libMadDirectory NORMALIZE
           ${LCONF_allProjectsRootDirectory}/libmad)
CMAKE_PATH(SET LCONF_libMP3LameDirectory NORMALIZE
           ${LCONF_allProjectsRootDirectory}/libmp3lame)
CMAKE_PATH(SET LCONF_libOggDirectory NORMALIZE
           ${LCONF_allProjectsRootDirectory}/libogg)
CMAKE_PATH(SET LCONF_libOpusDirectory NORMALIZE
           ${LCONF_allProjectsRootDirectory}/libopus)
CMAKE_PATH(SET LCONF_libOpusfileDirectory NORMALIZE
           ${LCONF_allProjectsRootDirectory}/libopusfile)
CMAKE_PATH(SET LCONF_libPngDirectory NORMALIZE
           ${LCONF_allProjectsRootDirectory}/libpng)
CMAKE_PATH(SET LCONF_libSndfileDirectory NORMALIZE
           ${LCONF_allProjectsRootDirectory}/libsndfile)
CMAKE_PATH(SET LCONF_libSpeexDirectory NORMALIZE
           ${LCONF_allProjectsRootDirectory}/libspeex)
CMAKE_PATH(SET LCONF_libSpeexDSPDirectory NORMALIZE
           ${LCONF_allProjectsRootDirectory}/libspeexdsp)
CMAKE_PATH(SET LCONF_libVorbisDirectory NORMALIZE
           ${LCONF_allProjectsRootDirectory}/libvorbis)
CMAKE_PATH(SET LCONF_libWavpackDirectory NORMALIZE
           ${LCONF_allProjectsRootDirectory}/libwavpack)
CMAKE_PATH(SET LCONF_libZDirectory NORMALIZE
           ${LCONF_allProjectsRootDirectory}/libz)

#------------------------------------------------------------

# append all configuration variable names to relevant variable name
# list

UTIL_Debug_appendRelevantVariableNames(
    LCONF_libFFTWDirectory
    LCONF_libFlacDirectory
    LCONF_libId3tagDirectory
    LCONF_libMadDirectory
    LCONF_libMP3LameDirectory
    LCONF_libOggDirectory
    LCONF_libOpusDirectory
    LCONF_libOpusfileDirectory
    LCONF_libPngDirectory
    LCONF_libSndfileDirectory
    LCONF_libSpeexDirectory
    LCONF_libSpeexDSPDirectory
    LCONF_libVorbisDirectory
    LCONF_libWavpackDirectory
    LCONF_libZDirectory
)
