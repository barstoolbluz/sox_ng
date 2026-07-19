# CMAKE file for library libSoX to be called when building SoX_ng

SET(targetName "libSoX")
SET(targetVersion ${CMAKE_PROJECT_VERSION})

###############
## FUNCTIONS ##
###############

MACRO(appendConditionally 
      resultVariableName conditionVariableName st)
    # appends string <st> to <resultVariable> when variable named
    # <conditionVariableName> is set

    UTIL_List_appendConditionally(${resultVariableName}
                                  ${conditionVariableName}
                                  "${st}")
ENDMACRO(appendConditionally)

#--------------------

MACRO(appendConditionallyEmbeddedInclude
      libraryShortName)
    # appends include directory for library named <libraryShortName>
    # to <libSoXIncludeDirectoryList> if embedded version shall be
    # used

    SET(configurationVariableName_local "hasLocalLib${libraryShortName}")
    SET(includeDirectory "${lib${libraryShortName}Directory}")
    appendConditionally(libSoXIncludeDirectoryList
                        "${configurationVariableName_local}"
                        "${includeDirectory}")
ENDMACRO(appendConditionallyEmbeddedInclude)

#--------------------

MACRO(appendConditionallyExplicitInclude
      libraryShortName subdirectory)
    # appends include directory for library named <libraryShortName> to
    # <libSoXIncludeDirectoryList> if explicitly installed version for
    # the SoX_ng build shall be used

    SET(configurationVariableName_local "hasLocalLib${libraryShortName}")
    SET(includeDirectory "${LCONF_lib${libraryShortName}Directory}")

    MESSAGE("configVariable_local = '${configurationVariableName_local}'"
            ", contents = '${${configurationVariableName_local}}'"
            ", includeDir = '${includeDirectory}'")

    IF(DEFINED ${configurationVariableName_local})
        IF(${${configurationVariableName_local}})
            IF(NOT "${subdirectory}" STREQUAL "")
                SET(includeDirectory ${includeDirectory}/${subdirectory})
            ENDIF()

            ACONF_temporaryIncludeDirectory(temporaryIncludeDirectory
                                            ${libraryShortName})               
            SET(directoryList
                ${includeDirectory}
                ${temporaryIncludeDirectory})

            LIST(APPEND libSoXIncludeDirectoryList ${directoryList})
        ENDIF()
    ENDIF()
ENDMACRO(appendConditionallyExplicitInclude)

#--------------------

MACRO(appendConditionallyExternalLibraries
      targetName libraryShortNameList)
    # appends all libraries from <libraryShortNameList> to libraries
    # of <targetName> if explicit library is selected

    FOREACH(libraryShortName ${libraryShortNameList})
        STRING(TOUPPER ${libraryShortName} uppercasedLibraryShortName)
        SET(configurationVariableName_external
            "EXTERNAL_${uppercasedLibraryShortName}")

        IF(DEFINED ${configurationVariableName_external})
            LINKER_getLibraryNameListForLibrary(libNameList
                                                ${libraryShortName})
            TARGET_LINK_LIBRARIES(${targetName} PUBLIC ${libNameList})
        ENDIF()
    ENDFOREACH()
ENDMACRO(appendConditionallyExternalLibraries)

#--------------------

MACRO(appendConditionallyName
      listVariableName libraryShortName overrideConditionSuffix)
    # appends lowercased <libraryShortName> to variable named
    # <listVariableName> when HAVE_XXX is set

    STRING(TOUPPER ${libraryShortName} uppercasedLibraryName)
    STRING(TOLOWER ${libraryShortName} lowercasedLibraryName)

    IF("${overrideConditionSuffix}" STRGREATER "")
        SET(conditionVariableName "HAVE_${overrideConditionSuffix}")
    ELSE()
        SET(conditionVariableName "HAVE_${uppercasedLibraryName}")
    ENDIF()

    appendConditionally(${listVariableName} ${conditionVariableName}
                        ${lowercasedLibraryName})
ENDMACRO(appendConditionallyName)

#--------------------

MACRO(_checkForLibraryExternal
      configurationVariableName_external configurationVariableName_have
      libraryShortName headerFileName functionName)
    # checks for library named <libraryShortName> by checking whether
    # library is external and setting corresponding
    # <configurationVariableName_external>,
    # <configurationVariableName_have>; <headerFileName> gives the
    # name of the header file to be checked, <libraryShortName> the
    # library name for the library check, <functionName> the name of
    # the function to be checked

    ACONF_checkForLibrary("${configurationVariableName_external}"
                          "${headerFileName}"
                          "${libraryShortName}"
                          "${functionName}")
    SET(${configurationVariableName_have}
        ${${configurationVariableName_external}})
ENDMACRO(_checkForLibraryExternal)

#--------------------

MACRO(checkForLibraryExternalVsEmbedded
      libraryShortName functionSuffix directoryPrefix)
    # checks for library named <libraryShortName> whether to use a system
    # provided version or the embedded version within SoX_ng; this is
    # done by checking whether the exclusion variable is unset, and if
    # not, then checking whether library is external and setting
    # corresponding EXTERNAL_xxx variable, HAVE_xxx variable and
    # hasLocalLibXXX variable; <functionSuffix> gives the suffix of
    # the function to be checked and <directoryPrefix> the directory
    # name of the header file

    STRING(TOUPPER ${libraryShortName} uppercasedLibraryName)
    SET(configurationVariableName_have "HAVE_${uppercasedLibraryName}")
    SET(configurationVariableName_local "hasLocalLib${libraryShortName}")
    SET(exclusionVariableName "CLP_without${libraryShortName}")

    IF(DEFINED ${exclusionVariable} AND ${${exclusionVariable}})
        # skip: library is excluded
    ELSEIF(${${configurationVariableName_local}})
        SET(${configurationVariableName_have} TRUE)
    ELSE()
        STRING(TOLOWER ${libraryShortName} lowercasedLibraryName)
        SET(configurationVariableName_external
            "EXTERNAL_${uppercasedLibraryName}")
        SET(headerFileName
            "${directoryPrefix}${lowercasedLibraryName}.h")
        SET(functionName "${lowercasedLibraryName}_${functionSuffix}")

        _checkForLibraryExternal(
            ${configurationVariableName_external}
            ${configurationVariableName_have}
            ${lowercasedLibraryName}
            ${headerFileName}
            ${functionName})
    ENDIF()
ENDMACRO(checkForLibraryExternalVsEmbedded)

#--------------------

MACRO(checkForLibraryExternalVsExplicit
      libraryShortName headerFileName libraryShortNameForCheck
      functionName)
    # checks hasLocalLibXXX variable for library named <libraryShortName>;
    # if not set, analyze externally provided version by checking
    # whether the exclusion variable is unset, and if not, then
    # checking whether library is external and setting corresponding
    # EXTERNAL_xxx and HAVE_xxx variables; <headerFileName> gives the
    # name of the header file to be checked, <libraryShortNameForCheck>
    # gives the library name for the library check, <functionName> the
    # name of the function to be checked and <sourceFileName> the name
    # of the source file to be added to list of source files

    STRING(TOUPPER ${libraryShortName} uppercasedLibraryName)
    SET(configurationVariableName_have "HAVE_${uppercasedLibraryName}")
    SET(configurationVariableName_local "hasLocalLib${libraryShortName}")
    SET(exclusionVariableName "CLP_without${libraryShortName}")

    IF(DEFINED ${exclusionVariable} AND ${${exclusionVariable}})
        # skip: library is excluded
    ELSEIF(${${configurationVariableName_local}})
        SET(${configurationVariableName_have} TRUE)
    ELSE()
        SET(configurationVariableName_external
            "EXTERNAL_${uppercasedLibraryName}")

        _checkForLibraryExternal(
            ${configurationVariableName_external}
            ${configurationVariableName_have}
            ${libraryShortNameForCheck}
            ${headerFileName}
            ${functionName})
    ENDIF()
ENDMACRO(checkForLibraryExternalVsExplicit)

#--------------------

MACRO(copyAndTransformOpusHeaderFile
      isTransformed)
    # copies opusfile.h header file from its include path to the
    # temporary SoX include directory; if <isTransformed> is set,
    # includes of opus headers in the file are prefixed with "opus/"

    SET(headerFileName "opus/opusfile.h")

    IF(hasLocalLibOpus)
        CMAKE_PATH(SET originalFilePath NORMALIZE
                   "${LCONF_libOpusfileDirectory}/include/opusfile.h")
    ELSE()
        FIND_FILE(originalFilePath "${headerFileName}")
    ENDIF()

    IF(NOT ${isTransformed})
        FILE(COPY ${originalFilePath}
             DESTINATION "${temporarySoXIncludeDirectory}/opus")
    ELSE()
        CMAKE_PATH(SET destinationFilePath NORMALIZE
                   "${temporarySoXIncludeDirectory}/${headerFileName}")
        FILE(READ ${originalFilePath} originalData)
        STRING(REPLACE "opus_multistream.h" "opus/opus_multistream.h"
                   transformedData "${originalData}")
        FILE(WRITE ${destinationFilePath} "${transformedData}")
    ENDIF()
ENDMACRO(copyAndTransformOpusHeaderFile)

############################################################

# === local directories ===

CMAKE_PATH(SET libDolbyBDirectory NORMALIZE
           ${GLOB_projectRootDirectory}/libdolbyb)

CMAKE_PATH(SET libEbuR128Directory NORMALIZE
           ${GLOB_projectRootDirectory}/libebur128)

CMAKE_PATH(SET libGSMDirectory NORMALIZE
           ${GLOB_projectRootDirectory}/libgsm)

CMAKE_PATH(SET libLPC10Directory NORMALIZE
           ${GLOB_projectRootDirectory}/lpc10)

CMAKE_PATH(SET temporarySoXIncludeDirectory NORMALIZE
           ${CMAKE_BINARY_DIR}/${targetName}_include)

#=================================================
# === generate variables via autoconfiguration ===
#=================================================

ACONF_setPackageVariables()

#------------------------------------------------------------
# define several configuration variables based on existing include
# files

SET(includeFileNameList
    "audioio.h" "byteswap.h" "conio.h" "fcntl.h" "fenv.h" "fftw3.h"
    "glob.h" "gsm/gsm.h" "inttypes.h" "io.h" "ltdl.h" "libpng/png.h"
    "machine/soundcard.h" "memory.h" "stdint.h" "string.h" "strings.h"
    "sun/audioio.h" "sys/audioio.h" "sys/ioctl.h" "sys/select.h"
    "sys/soundcard.h" "sys/stat.h" "sys/time.h" "sys/timeb.h"
    "sys/types.h" "sys/utsname.h" "termios.h" "unistd.h"
)

ACONF_checkIncludeFileNameList(includeFileNameList)

IF(NOT WIN32)
    SET(HAVE_LIBLTDL ${HAVE_LTDL_H})
ELSE()
    SET(HAVE_WIN32_GLOB_H ${HAVE_GLOB_H})

    IF(NOT CLP_withoutLibltdl)
        CHECK_INCLUDE_FILE("ltdl.h" HAVE_WIN32_LTDL_H)
    ENDIF()
ENDIF()

#------------------------------------------------------------
# define several configuration variables based on available
# library functions

SET(functionNameList
    "aligned_alloc" "fmemopen" "fork" "fseeko" "gettimeofday"
    "memalign" "mkstemp" "popen" "posix_memalign" "sigaction"
    "strcasecmp" "strdup" "strrstr" "vsnprintf"
)

ACONF_checkFunctionNameList(functionNameList)

# set variable for endianness of host machine
TEST_BIG_ENDIAN(WORDS_BIGENDIAN)

#==============================
# complex library checks
#==============================

#-------------------------------
# external vs embedded libraries:
#
# check for library provided by system and - when not found -
# reference embedded library
# -------------------------------

# DolbyB
SET(hasLocalLibDolbyB TRUE)
checkForLibraryExternalVsEmbedded(DolbyB init "")
SET(HAVE_DOLBYB_H   ${HAVE_DOLBYB})

# EbuR128
SET(hasLocalLibEbuR128 TRUE)
checkForLibraryExternalVsEmbedded(EbuR128 init "")
SET(HAVE_EBUR128_H  ${HAVE_EBUR128})

# GSM
SET(hasLocalLibGSM TRUE)
checkForLibraryExternalVsEmbedded(GSM create "gsm/")
SET(HAVE_GSM_GSM_H  ${HAVE_GSM})

# LPC10
SET(hasLocalLibLPC10 TRUE)
checkForLibraryExternalVsEmbedded(LPC10 create "lpc10/")

#-------------------------------
# external vs explicit libraries:
#
# check for library provided by system and - when not found -
# reference library installed explicitly for SoX_ng
#-------------------------------

# --- FFTW
checkForLibraryExternalVsExplicit(FFTW "fftw3.h" fftw3 fftw_execute)
SET(HAVE_FFTW3_H ${HAVE_FFTW})

# --- Flac
checkForLibraryExternalVsExplicit(Flac "FLAC/all.h" FLAC
                                  FLAC__stream_encoder_new)

# --- Id3tag
checkForLibraryExternalVsExplicit(Id3tag "id3tag.h" id3tag id3_file_open)
SET(HAVE_LAME_ID3TAG  ${HAVE_ID3TAG})

SET(USING_ID3TAG ${HAVE_ID3TAG})

# --- Mad
checkForLibraryExternalVsExplicit(Mad "mad.h" mad mad_stream_buffer)
SET(HAVE_MAD_H ${HAVE_MAD})

# --- Opus
checkForLibraryExternalVsExplicit(Opus "opus/opus.h" opus opus_decoder_create)

# --- Opusfile
IF(NOT CLP_withoutOpus)
    checkForLibraryExternalVsExplicit(Opusfile "opus/opusfile.h"
                                      opusfile op_open_callbacks)

    IF(EXTERNAL_OPUS)
        # HACK: make a local copy of opusfile.h referencing
        # "opus/opus_multistream.h" instead of bad
        # "<opus_multistream.h>"
        copyAndTransformOpusHeaderFile(TRUE)
    ENDIF()

    IF(hasLocalLibOpusfile)
        # HACK: make a local copy of opusfile.h into opus/ subdirectory
        copyAndTransformOpusHeaderFile(FALSE)
    ENDIF()
ENDIF()

# --- Sndfile
checkForLibraryExternalVsExplicit(Sndfile "sndfile.h" sndfile
                                  sf_open_virtual)

# assume that sndfile (when it exists) has the appropriate version
SET(HAVE_SNDFILE_1_0_18  ${HAVE_SNDFILE})

# --- Speex
checkForLibraryExternalVsExplicit(Speex "speex/speex_preprocess.h"
                                  speex speex_preprocess_run)

# --- SpeexDSP
checkForLibraryExternalVsExplicit(SpeexDSP "speex/speex_preprocess.h"
                                  speexdsp speex_preprocess_run)

# --- Wavpack
checkForLibraryExternalVsExplicit(Wavpack "wavpack/wavpack.h" wavpack
                                  WavpackGetSampleRate)
SET(HAVE_WAVPACK_H  ${HAVE_WAVPACK})

# --- Z
checkForLibraryExternalVsExplicit(Z "zlib.h" z zlibVersion)

# --- Png (depending on Z)
IF(HAVE_Z)
    checkForLibraryExternalVsExplicit(Png "png.h" png png_set_rows)
    # SET(HAVE_LIBPNG_PNG_H  ${HAVE_PNG})
ENDIF()

#-----
#-----

# Lame

IF(NOT CLP_withoutMP3Lame)
    IF(hasLocalLibMP3Lame)
        SET(HAVE_LAME TRUE)
        SET(HAVE_LAME_H TRUE)
    ELSE()
        ACONF_checkForLibrary(HAVE_LAME_LAME_H lame/lame.h
                              mp3lame lame_get_lametag_frame)

        IF(NOT HAVE_LAME_LAME_H)
            ACONF_checkForLibrary(HAVE_LAME_LAME_H lame.h
                                  mp3lame lame_get_lametag_frame)
        ENDIF()

        SET(HAVE_LAME ${HAVE_LAME_LAME_H})
    ENDIF()
ENDIF()

#-----

# Ogg/Vorbis

IF(NOT CLP_withoutOggVorbis)
    IF(hasLocalLibOgg AND hasLocalLibVorbis)
        SET(HAVE_OGG_VORBIS TRUE)
    ELSE()
        SET(functionList
            "ogg_stream_flush;vorbis_analysis_headerout;ov_clear;vorbis_encode_init_vbr")
        ACONF_checkForLibrary(HAVE_OGG_VORBIS vorbis/codec.h
                              "ogg;vorbis;vorbisfile;vorbisenc"
                              "${functionList}")
    ENDIF()
ENDIF()

#-----------------------------------
# scan for system provided libraries
#-----------------------------------

ACONF_checkForLibrary(HAVE_AMRNB amrnb/sp_dec.h amrnb Decoder_Interface_init)
ACONF_checkForLibrary(HAVE_AMRWB amrwb/dec.h amrwb D_IF_init)
ACONF_checkForLibrary(HAVE_AO ao/ao.h ao ao_play)
ACONF_checkForLibrary(NEED_LIBM math.h m pow)

IF(NOT CLP_withoutMagic)
    ACONF_checkForLibrary(HAVE_MAGIC magic.h magic magic_open)
ENDIF()

IF(NOT CLP_withoutLadspa)
    CHECK_INCLUDE_FILE("ladspa.h" HAVE_LADSPA_H)
ENDIF()

IF(NEED_LIBM)
    ACONF_checkForLibrary(HAVE_LRINT math.h m lrint)
ELSE()
    CHECK_FUNCTION_EXISTS("lrint" HAVE_LRINT)
ENDIF()

IF(NOT CLP_withoutTwoLame)
    ACONF_checkForLibrary(HAVE_TWOLAME_H twolame.h twolame twolame_init)
ENDIF()

#-----------------------------------
# scan for platform audio libraries
#-----------------------------------

IF(WINDOWS)
    ACONF_checkForLibrary(HAVE_WAVEAUDIO mmsystem.h winmm waveInGetDevCapsA)
ELSEIF(MACOS)
    FIND_LIBRARY(COREAUDIO_LIBRARY CoreAudio)
    MARK_AS_ADVANCED(COREAUDIO_LIBRARY)

    IF(COREAUDIO_LIBRARY)
        SET(HAVE_COREAUDIO TRUE)
    ENDIF(COREAUDIO_LIBRARY)
ELSE()
    # some Unix system
    ACONF_checkForLibrary(HAVE_ALSA alsa/asoundlib.h asound snd_pcm_open)
    ACONF_checkForLibrary(HAVE_SNDIO sndio.h sndio sio_open)
    ACONF_checkForLibrary(HAVE_PULSEAUDIO pulse/simple.h
                          "pulse-simple;pulse"
                          "pa_simple_new;pa_strerror")

    ACONF_checkForLibrary(HAVE_SUN_AUDIOIO_H sun/audioio.h c ioctl)

    IF(NOT HAVE_SUN_AUDIOIO_H)
        ACONF_checkForLibrary(HAVE_SYS_AUDIOIO_H sys/audioio.h c ioctl)
    ENDIF()

    ACONF_checkForLibrary(HAVE_SYS_SOUNDCARD_H sys/soundcard.h
                          ossaudio _oss_ioctl)
    ACONF_checkForLibrary(HAVE_MACHINE_SOUNDCARD_H machine/soundcard.h
                          ossaudio _oss_ioctl)

    IF(NOT HAVE_ALSA)
        IF(HAVE_SYS_SOUNDCARD_H OR HAVE_MACHINE_SOUNDCARD_H)
            SET(HAVE_OSS TRUE)
        ELSEIF(HAVE_SUN_AUDIOIO_H OR HAVE_SYS_AUDIOIO_H)
            SET(HAVE_SUN_AUDIO TRUE)
        ENDIF()
    ENDIF()
ENDIF()

#--------------------------
# update dependent settings
#--------------------------

IF(HAVE_LAME_LAME_H OR HAVE_MAD_H)
    SET(HAVE_MP3 TRUE)
ENDIF()

SET(CMAKE_REQUIRED_LIBRARIES mp3lame m)
CHECK_FUNCTION_EXISTS("id3tag_set_fieldvalue" HAVE_LAME_ID3TAG)

IF(hasLocalLibSndfile)
    # define several configuration variables based on existing symbols
    # in sndfile.h

    SET(enumSymbolNameList
        SF_FORMAT_AVR SF_FORMAT_SD2 SF_FORMAT_CAF SF_FORMAT_FLAC
        SF_FORMAT_WVE SF_FORMAT_OGG SF_FORMAT_MPC2K SF_FORMAT_MPEG
        SFC_SET_SCALE_INT_FLOAT_WRITE
    )

    SET(libSndfileHeaderFileName
        "${LCONF_libSndfileDirectory}/include/sndfile.h")

    ACONF_checkEnumSymbolNameList(${libSndfileHeaderFileName}
                                  enumSymbolNameList)
ENDIF()

#--------------------------------
# collect all include directories
#--------------------------------

SET(libSoXIncludeDirectoryList
    ${temporarySoXIncludeDirectory}
    ${soxSrcDirectory}
)

# append includes for all embedded projects where SoX depends upon
appendConditionallyEmbeddedInclude(DolbyB)
appendConditionallyEmbeddedInclude(EbuR128)
appendConditionallyEmbeddedInclude(GSM)
appendConditionallyEmbeddedInclude(LPC10)

# append includes for all explicitly parallel projects where SoX
# depends upon
appendConditionallyExplicitInclude(FFTW      "api")
appendConditionallyExplicitInclude(Flac      include)
appendConditionallyExplicitInclude(Id3tag    "")
appendConditionallyExplicitInclude(Mad       "")
appendConditionallyExplicitInclude(MP3Lame   include)
appendConditionallyExplicitInclude(Ogg       include)
appendConditionallyExplicitInclude(Opus      include)
appendConditionallyExplicitInclude(Opusfile  include)
appendConditionallyExplicitInclude(Png       "")
appendConditionallyExplicitInclude(Sndfile   include)
appendConditionallyExplicitInclude(Speex     include)
appendConditionallyExplicitInclude(SpeexDSP  include)
appendConditionallyExplicitInclude(Vorbis    include)
appendConditionallyExplicitInclude(Wavpack   include)
appendConditionallyExplicitInclude(Z         "")

#--------------------
# collect platforms
#--------------------

SET(libSoXPlatformList )

appendConditionallyName(libSoXPlatformList AO          "")
appendConditionallyName(libSoXPlatformList ALSA        "")
appendConditionallyName(libSoXPlatformList COREAUDIO   "")
appendConditionallyName(libSoXPlatformList OPUS        "")
# appendConditionallyName(libSoXPlatformList OSS         "")
appendConditionallyName(libSoXPlatformList PULSEAUDIO  "")
appendConditionallyName(libSoXPlatformList SNDIO       "")
appendConditionallyName(libSoXPlatformList SUNAUDIO    "")
appendConditionallyName(libSoXPlatformList WAVEAUDIO   "")

UTIL_List_constructFromOther(libSoXPlatformSourceFileList
                             libSoXPlatformList
                             "${soxSrcDirectory}/" ".c")

# ---------------------------------------------------------
# collect all HAVE_XXX variables and EXTERNAL_XXX variables
# for debugging
# ---------------------------------------------------------

IF(CLP_Debug_listBuildVariables)
    SET(platformNameList
        AO ALSA COREAUDIO OPUS PULSEAUDIO SNDIO SUNAUDIO WAVEAUDIO)

    SET(embeddedLibraryNameList
        DOLBYB EBUR128 GSM LPC10)

    SET(explicitLibraryNameList
        FFTW FLAC ID3TAG MAD OPUS OPUSFILE PNG SNDFILE
        SPEEX SPEEXDSP WAVPACK Z
    )

    SET(libraryNameList
        ${embeddedLibraryNameList}
        ${explicitLibraryNameList}
    )

    SET(haveVariableNameStemList
        AMRNB AMRWB MP3 LAME
        ${platformNameList}
        ${libraryNameList}
    )

    UTIL_List_constructFromOther(haveVariableNameList
                                 haveVariableNameStemList
                                 "HAVE_" "")

    UTIL_List_constructFromOther(externalVariableNameList
                                 libraryNameList
                                 "EXTERNAL_" "")

    UTIL_Debug_appendRelevantVariableNames(${haveVariableNameList}
                                           ${externalVariableNameList}
                                           HAVE_OGG_VORBIS)
ENDIF()

#====================
# === build setup ===
#====================

#-----------------------------------
#--- generate configuration file ---
#-----------------------------------

# put soxconfig.h into src directory instead of a build local
# directory (due to references from the embedded libraries)

CONFIGURE_FILE("${buildSetupTemplateDirectory}/libsox-config.h_(cmake)"
               ${soxSrcDirectory}/soxconfig.h
               @ONLY)

#---------------------
# --- source files ---
#---------------------

#..............................
#... libDolbyB source files ...
#..............................

SET(srcFileStemList
    Calibrate DCfilter DiodeClip FindOutSmp HPF1 HPF2 HPF2SetVals
    HPFAdj LPFAdj libdolbyb Param SetGate SidePath
)

UTIL_List_constructFromOther(libDolbyBSourceFileList
                             srcFileStemList
                             "${libDolbyBDirectory}/" ".c")

#...............................
#... libEBUR128 source files ...
#...............................

SET(srcFileStemList
    ebur128
)

UTIL_List_constructFromOther(libEbuR128SourceFileList
                             srcFileStemList
                             "${libEbuR128Directory}/" ".c")

#...........................
#... libGSM source files ...
#...........................

SET(srcFileStemList
    add code decode gsm_create gsm_decode gsm_destroy gsm_encode
    gsm_option long_term lpc preprocess rpe short_term table
)

UTIL_List_constructFromOther(libGSMSourceFileList
                             srcFileStemList
                             "${libGSMDirectory}/" ".c")

#..........................
#... lpc10 source files ...
#..........................

SET(srcFileStemList
    analys bsynz chanwr dcbias decode deemp difmag dyptrk encode
    energy f2clib ham84 hp100 invert irc2pc ivfilt lpcdec lpcenc
    lpcini lpfilt median mload onset pitsyn placea placev preemp
    prepro random rcchk synths tbdm voicin vparms
)

UTIL_List_constructFromOther(libLPC10SourceFileList
                             srcFileStemList
                             "${libLPC10Directory}/" ".c")

#...........................
#... LibSoX source files ...
#...........................

# the SoX effects
SET(srcFileStemList
    bend biquad biquads centercut chorus compand compandt contrast
    dcshift delay dft_filter dither dolbyb dop downsample earwax echo
    echos fade fft4g fir firfit flanger gain hilbert input ladspa
    loudness mcompand noiseprof noisered output overdrive pad phaser
    rate remix repeat reverb reverse saturation sdm silence sinc
    skeleff softvol speed speexdsp splice stat stats
    stretch swap synth tempo tremolo trim upsample vad vol
)

# add additional effect files
appendConditionallyName(srcFileStemList spectrogram "HAVE_PNG")

UTIL_List_constructFromOther(libSoXEffectsSourceFileList
                             srcFileStemList
                             "${soxSrcDirectory}/" ".c")

#--------------------

# the SoX formats
SET(srcFileStemList
    8svx adpcm adpcms aifc-fmt aiff aiff-fmt al-fmt au avr cdr cvsd
    cvsd-fmt dat dsdiff dsf dvms-fmt f4-fmt f8-fmt ffmpeg g711 g721
    g723_24 g723_40 g72x gsm gsrt hcom htk ima-fmt ima_rw la-fmt
    lpc10 lu-fmt maud mp3 mp3-mad mp3-lame mp3-twolame nulfile nsp prc
    raw raw-fmt s1-fmt s2-fmt s3-fmt s4-fmt sdm sf skelform smp sounder
    soundtool sox-fmt sphere tx16w u1-fmt u2-fmt u3-fmt u4-fmt ul-fmt
    voc vox vox-fmt wav wve xa
)

# add additional format files
appendConditionallyName(srcFileStemList id3     "ID3TAG")
appendConditionallyName(srcFileStemList flac    "")
appendConditionallyName(srcFileStemList sndfile "")
appendConditionallyName(srcFileStemList vorbis  "OGG_VORBIS")
appendConditionallyName(srcFileStemList wavpack "")

UTIL_List_constructFromOther(libSoXFormatsSourceFileList
                             srcFileStemList
                             "${soxSrcDirectory}/" ".c")

#--------------------

# the SoX glue code
SET(srcFileStemList
    effects formats_i libsox_i effects_i effects_i_dsp getopt keymap
    util fifo formats libsox_ng xmalloc
)

appendConditionally(srcFileStemList WINDOWS
                    "win32-glob;win32-ltdl;win32-unicode")

UTIL_List_constructFromOther(libSoXGlueSourceFileList
                             srcFileStemList
                             "${soxSrcDirectory}/" ".c")

#--------------------

SET(libSoXSourceFileList
    ${libSoXEffectsSourceFileList}
    ${libSoXFormatsSourceFileList}
    ${libSoXGlueSourceFileList}
    ${libSoXPlatformSourceFileList}
)

# add all embedded libraries to source file list
LIST(APPEND libSoXSourceFileList
     ${libDolbyBSourceFileList}
     ${libEbuR128SourceFileList}
     ${libGSMSourceFileList}
     ${libLPC10SourceFileList})

#--------------------
# --- definitions ---
#--------------------

SET(libSoXCompileDefinitionList
    HAVE_CONFIG_H
)

# add missing formats from inttypes.h (if inttypes.h is not available)
LIST(APPEND libSoXCompileDefinitionList
     PRIi64="ld")

IF(NOT WINDOWS)
    LIST(APPEND libSoXCompileDefinitionList
         LADSPA_PATH="${CLP_withLadspaPath}")
    LIST(APPEND libSoXCompileDefinitionList
         PKGLIBDIR="${CLP_withPkgConfigDir}")
ENDIF()

#===============
# === Target ===
#===============

# the libSoX library without supporting libraries included
LINKER_makeLibraryTarget(${targetName} FALSE)
SET_TARGET_PROPERTIES(${targetName} PROPERTIES
                      OUTPUT_NAME "libsox_ng")

# add supporting object libraries
SET(libraryNameStemList ${activeLibraryNameList})

UTIL_List_constructFromOther(libraryObjectList
                             libraryNameStemList
                             "$<TARGET_OBJECTS:lib" ">")

TARGET_LINK_LIBRARIES(${targetName} PUBLIC ${libraryObjectList})

# add required system libraries (if any)
SET(platformLibraryList )

IF(LINUX)
    appendConditionally(platformLibraryList HAVE_AO          "ao")
    appendConditionally(platformLibraryList HAVE_ALSA        "asound")
    appendConditionally(platformLibraryList HAVE_LIBLTDL     "ltdl")
    appendConditionally(platformLibraryList NEED_LIBM        "m")
    appendConditionally(platformLibraryList HAVE_MAGIC       "magic")
    appendConditionally(platformLibraryList HAVE_OPUS        "opusfile")
    appendConditionally(platformLibraryList HAVE_OSS         "ossaudio")
    appendConditionally(platformLibraryList HAVE_PULSEAUDIO  "pulse;pulse-simple")
    appendConditionally(platformLibraryList HAVE_SNDIO       "sndio")
    appendConditionally(platformLibraryList HAVE_SUNAUDIO    "sunaudio")
ELSEIF(MACOS)
    appendConditionally(platformLibraryList HAVE_COREAUDIO
                        "-framework CoreAudio")
ENDIF()

TARGET_LINK_LIBRARIES(${targetName} PUBLIC ${platformLibraryList})

# add external libraries
SET(externalLibraryNameList
    FFTW Flac Id3tag Mad MP3Lame Ogg Opus Opusfile Png Sndfile Speex
    SpeexDSP Vorbis Wavpack Z
)

appendConditionallyExternalLibraries(${targetName}
                                     "${externalLibraryNameList}")

# libSoX depends on all support libraries
ADD_DEPENDENCIES(${targetName} SupportLibraries)
