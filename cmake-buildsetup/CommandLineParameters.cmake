# command line parameter handling for SoX_ng CMake build

##############
### MACROS ###
##############

MACRO(CLP_setFromCommandLine
      destinationVariableName commandLineVariableName defaultValue)
    # sets variable named <destinationVariableName> from value of
    # command line variable named <commandLineVariableName> if set or
    # otherwise to <defaultValue>

    IF(NOT DEFINED ${commandLineVariableName})
        SET(${destinationVariableName} ${defaultValue})
    ELSE()
        SET(commandLineValue ${${commandLineVariableName}})
        SET(${destinationVariableName} ${commandLineValue})
    ENDIF()
ENDMACRO(CLP_setFromCommandLine)

#============================================================

# set flag to write a summary of relevant build variables
CLP_setFromCommandLine(CLP_Debug_listBuildVariables
                       list_build_variable_values FALSE)

# set flag whether to make any symlinks to sox_ng
CLP_setFromCommandLine(CLP_disableSymlinks disable_symlinks FALSE)

# set flag whether to use dynamic version of amrnb library (instead of
# static)
CLP_setFromCommandLine(CLP_enableDlAmrnb enable_dl_amrnb FALSE)

# set flag whether to use dynamic version of amrwb library (instead of
# static)
CLP_setFromCommandLine(CLP_enableDlAmrwb enable_dl_amrwb FALSE)

# set flag whether to use dynamic version of lame library (instead of
# static)
CLP_setFromCommandLine(CLP_enableDlLame enable_dl_lame FALSE)

# set flag whether to use dynamic version of mad library (instead of
# static)
CLP_setFromCommandLine(CLP_enableDlMad enable_dl_mad FALSE)

# set flag whether to use dynamic library version of sndfile library
# (instead of static)
CLP_setFromCommandLine(CLP_enableDlSndfile enable_dl_sndfile FALSE)

# set flag whether to use dynamic library version of speexdsp library
# (instead of static)
CLP_setFromCommandLine(CLP_enableDlSpeexDSP enable_dl_speexdsp FALSE)

# set flag whether to use dynamic library version of twolame library
# (instead of static)
CLP_setFromCommandLine(CLP_enableDlTwoLame enable_dl_twolame FALSE)

# make links from "sox" to "sox_ng" and so on
CLP_setFromCommandLine(CLP_enableReplace enable_replace FALSE)

# the target path for the generated exe files;
# (default: $install_prefix_path/bin)
CLP_setFromCommandLine(CLP_installBinPath install_bin_path
                       ${CMAKE_INSTALL_PREFIX}/bin)

# the target path for the generated documentation files;
# (default: $install_prefix_path/doc)
CLP_setFromCommandLine(CLP_installDocPath install_doc_path
                       ${CMAKE_INSTALL_PREFIX}/doc)

# the target path for the generated lib files; (default:
# $install_prefix_path/lib)
CLP_setFromCommandLine(CLP_installLibPath install_lib_path
                       ${CMAKE_INSTALL_PREFIX}/lib)

# the target path for the generated man files; (default:
# $install_prefix_path/man)
CLP_setFromCommandLine(CLP_installManPath install_man_path
                       ${CMAKE_INSTALL_PREFIX}/man)

# set flag whether to prefer curl to wget
CLP_setFromCommandLine(CLP_withCurl with_curl FALSE)

# set default to loading optional formats dynamically
CLP_setFromCommandLine(CLP_withDynDefault with_dyn_default FALSE)

# set flag whether to use ffmpeg to decode otherwise unsupported
# formats
CLP_setFromCommandLine(CLP_withFFmpeg with_ffmpeg FALSE)

# default search path for LADSPA plugins
CLP_setFromCommandLine(CLP_withLadspaPath with_ladspa_path
                       "${CMAKE_INSTALL_FULL_LIBDIR}/ladspa")

# location to install .pc files or "no" to disable
# (default=$(libdir)/pkgconfig)
CLP_setFromCommandLine(CLP_withPkgConfigDir with_pkgconfigdir
                       "${CMAKE_INSTALL_FULL_LIBDIR}/pkgconfig")

# set flag whether to use dolbyb for external dynamic library support
CLP_setFromCommandLine(CLP_withoutDolbyB without_dolbyb FALSE)

# set flag whether to use EBU R128 library
CLP_setFromCommandLine(CLP_withoutEbuR128 without_ebur128 FALSE)

# set flag whether to use fftw library
CLP_setFromCommandLine(CLP_withoutFFTW without_fftw FALSE)

# set flag whether to use flac library (command line parameter not
# available in SoX_ng so far)
CLP_setFromCommandLine(CLP_withoutFlac without_flac FALSE)

# set flag whether to use gsm library (command line parameter not
# available in SoX_ng so far)
CLP_setFromCommandLine(CLP_withoutGSM without_gsm FALSE)

# set flag whether to use id3tag library
CLP_setFromCommandLine(CLP_withoutId3tag without_id3tag FALSE)

# set flag whether to use ladspa library
CLP_setFromCommandLine(CLP_withoutLadspa without_ladspa FALSE)

# set flag whether to use LAME (LAME Ain't an MP3 Encoder)
CLP_setFromCommandLine(CLP_withoutMP3Lame without_lame FALSE)

# set flag whether to use libltdl for external dynamic library support
CLP_setFromCommandLine(CLP_withoutLibltdl without_libltdl FALSE)

# set flag whether to use lpc10 library (command line parameter not
# available in SoX_ng so far)
CLP_setFromCommandLine(CLP_withoutLPC10 without_lpc10 FALSE)

# set flag whether to use MAD (MP3 Audio Decoder)
CLP_setFromCommandLine(CLP_withoutMad without_mad FALSE)

# set flag whether to use magic library
CLP_setFromCommandLine(CLP_withoutMagic without_magic FALSE)

# set flag whether to use ogg library
CLP_setFromCommandLine(CLP_withoutOgg without_ogg FALSE)
SET(CLP_withoutVorbis ${CLP_withoutOgg})

# set flag whether to use opus library
CLP_setFromCommandLine(CLP_withoutOpus without_opus FALSE)
SET(CLP_withoutOpusFile ${CLP_withoutOpus})

# set flag whether to use png library
CLP_setFromCommandLine(CLP_withoutPng without_png FALSE)

# set flag whether to use sndfile library (command line parameter not
# available in SoX_ng so far)
CLP_setFromCommandLine(CLP_withoutSndfile without_sndfile FALSE)

# set flag whether to use speexdsp library
CLP_setFromCommandLine(CLP_withoutSpeexDSP without_speexdsp FALSE)

# set flag whether to use twolame library
CLP_setFromCommandLine(CLP_withoutTwoLame without_twolame FALSE)

# set flag whether to use wavpack library (command line parameter not
# available in SoX_ng so far)
CLP_setFromCommandLine(CLP_withoutWavpack without_wavpack FALSE)

# set flag whether to use z library (command line parameter not
# available in SoX_ng so far)
CLP_setFromCommandLine(CLP_withoutZ without_z FALSE)

#------------------------------------------------------------

# append all command line variable names to relevant variable name
# list

UTIL_Debug_appendRelevantVariableNames(
    CLP_Debug_listBuildVariables
    CLP_disableSymlinks
    CLP_enableDlAmrnb
    CLP_enableDlAmrwb
    CLP_enableDlLame
    CLP_enableDlMad
    CLP_enableDlSndfile
    CLP_enableDlSpeexDSP
    CLP_enableDlTwoLame
    CLP_enableReplace
    CLP_installBinPath
    CLP_installDocPath
    CLP_installLibPath
    CLP_installManPath
    CLP_withCurl
    CLP_withDynDefault
    CLP_withFFmpeg
    CLP_withLadspaPath
    CLP_withPkgConfigDir
    CLP_withoutDolbyB
    CLP_withoutEbuR128
    CLP_withoutFFTW
    CLP_withoutFlac
    CLP_withoutGSM
    CLP_withoutId3tag
    CLP_withoutLadspa
    CLP_withoutMP3Lame
    CLP_withoutLibltdl
    CLP_withoutLPC10
    CLP_withoutMad
    CLP_withoutMagic
    CLP_withoutOgg
    CLP_withoutVorbis
    CLP_withoutOpus
    CLP_withoutOpusFile
    CLP_withoutPng
    CLP_withoutSndfile
    CLP_withoutSpeexDSP
    CLP_withoutTwoLame
    CLP_withoutWavpack
    CLP_withoutZ
)
