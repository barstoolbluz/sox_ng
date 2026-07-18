# CMAKE file for library libOpus to be called when building SoX_ng

SET(targetName "libOpus")
SET(targetVersion "1.5.2")

#================================================
#=== generate variables via autoconfiguration ===
#================================================

ACONF_setPackageVariables()

#-----

# define to enable non-Opus modes, e.g. 44.1 kHz & 2^n frames
SET(CUSTOM_MODES FALSE)

# define if API does not support floating-point
SET(DISABLE_FLOAT_API FALSE)

# define if assertions should be enabled
SET(ENABLE_ASSERTIONS FALSE)

# define to use Deep REDundancy (DRED)
SET(ENABLE_DRED FALSE)

# define if hardening run-time checks should be enabled
SET(ENABLE_HARDENING FALSE)

# define if you have a fast enough FPU
SET(FIXED_POINT FALSE)

# define if the encoder should make random decisions
SET(FUZZING FALSE)

# find processor architecture
SET(HAVE_SSE   GLOB_processorHasSSE)
SET(HAVE_SSE2  GLOB_processorHasSSE2)

IF(GLOB_systemProcessor     STREQUAL "i386")
    SET(OPUS_CPU_X86 TRUE)
ELSEIF(GLOB_systemProcessor STREQUAL "x86_64")
    SET(OPUS_CPU_X64 TRUE)
ELSEIF(GLOB_systemProcessor STREQUAL "arm")
    SET(OPUS_CPU_ARM TRUE)
ENDIF()

# define if you have the C99 variable-size arrays
SET(VAR_ARRAYS FALSE)

#------------------------------------------------------------
# define several configuration variables based on existing include
# files

SET(includeFileNameList
    "alloca.h"
)
    
ACONF_checkIncludeFileNameList(includeFileNameList)

IF(WINDOWS)
    # alloca.file will be emulated for Windows
    SET(HAVE_ALLOCA_H TRUE)
ENDIF()

SET(USE_ALLOCA HAVE_ALLOCA_H)

#------------------------------------------------------------
# define several configuration variables based on available
# library functions

SET(functionNameList
    "lrint" "lrintf"
)

ACONF_checkFunctionNameList(functionNameList)

#------------------------------------------------------------
# additional libraries

ACONF_checkForLibrary(HAVE_LIBM math.h m floor)

#===================
#=== build setup ===
#===================

#-----------------------------------
#--- generate configuration file ---
#-----------------------------------

ACONF_generateConfigurationFile(Opus)

#-------------------
#--- directories ---
#-------------------

IF(FIXED_POINT)
    SET(fileSuffix     _FIX)
    SET(arithmeticName fixed)
ELSE()
    SET(fileSuffix     _FLP)
    SET(arithmeticName float)
ENDIF()

SET(libOpusSrcDirectory
    ${LCONF_libOpusDirectory}/src)

SET(libOpusIncludeDirectoryList
    ${temporaryOpusIncludeDirectory}
    ${LCONF_libOpusDirectory}/celt
    ${LCONF_libOpusDirectory}/silk
    ${LCONF_libOpusDirectory}/silk/${arithmeticName}
    ${LCONF_libOpusDirectory}/include
)

#--------------------
#--- source files ---
#--------------------

# --- celt ---

SET(srcFileStemList
    bands celt celt_decoder celt_encoder celt_lpc cwrs entcode entdec
    entenc kiss_fft laplace mathops mdct modes pitch quant_bands rate
    vq
)

UTIL_List_constructFromOther(libOpusSourceFileList_celt
                             srcFileStemList
                             "${LCONF_libOpusDirectory}/celt/" ".c")


# --- silk ---

SET(srcFileStemList
    A2NLSF ana_filt_bank_1 biquad_alt bwexpander_32 bwexpander
    check_control_input CNG code_signs control_audio_bandwidth
    control_codec control_SNR debug dec_API decode_core decode_frame
    decode_indices decode_parameters decode_pitch decode_pulses
    decoder_set_fs enc_API encode_indices encode_pulses gain_quant
    HP_variable_cutoff init_decoder init_encoder inner_prod_aligned
    interpolate lin2log log2lin LPC_analysis_filter LPC_fit
    LPC_inv_pred_gain LP_variable_cutoff NLSF2A NLSF_decode
    NLSF_del_dec_quant NLSF_encode NLSF_stabilize NLSF_unpack NLSF_VQ
    NLSF_VQ_weights_laroia NSQ NSQ_del_dec pitch_est_tables PLC
    process_NLSFs quant_LTP_gains resampler resampler_down2_3
    resampler_down2 resampler_private_AR2 resampler_private_down_FIR
    resampler_private_IIR_FIR resampler_private_up2_HQ resampler_rom
    shell_coder sigm_Q15 sort stereo_decode_pred stereo_encode_pred
    stereo_find_predictor stereo_LR_to_MS stereo_MS_to_LR
    stereo_quant_pred sum_sqr_shift table_LSF_cos tables_gain
    tables_LTP tables_NLSF_CB_NB_MB tables_NLSF_CB_WB tables_other
    tables_pitch_lag tables_pulses_per_block VAD VQ_WMat_EC
)

UTIL_List_constructFromOther(libOpusSourceFileList_silk
                             srcFileStemList
                             "${LCONF_libOpusDirectory}/silk/" ".c")

# --- silk/${arithmeticName} ---

SET(srcFileStemList
    apply_sine_window autocorrelation burg_modified bwexpander
    corrMatrix encode_frame energy find_LPC find_LTP find_pitch_lags
    find_pred_coefs inner_product k2a LPC_analysis_filter
    LPC_inv_pred_gain LTP_analysis_filter LTP_scale_ctrl
    noise_shape_analysis pitch_analysis_core process_gains
    regularize_correlations residual_energy scale_copy_vector
    scale_vector schur sort warped_autocorrelation wrappers
)

SET(arithmeticDirectory
     "${LCONF_libOpusDirectory}/silk/${arithmeticName}")

UTIL_List_constructFromOther(libOpusSourceFileList_silkArith
                             srcFileStemList
                             "${arithmeticDirectory}/"
                             "${fileSuffix}.c")

# --- std ---

SET(srcFileStemList
    analysis extensions mapping_matrix mlp mlp_data opus opus_decoder
    opus_encoder opus_multistream opus_multistream_decoder
    opus_multistream_encoder opus_projection_decoder
    opus_projection_encoder repacketizer
)

UTIL_List_constructFromOther(libOpusSourceFileList_std
                             srcFileStemList
                             "${libOpusSrcDirectory}/" ".c")


SET(libOpusSourceFileList
    ${libOpusSourceFileList_celt}
    ${libOpusSourceFileList_silk}
    ${libOpusSourceFileList_silkArith}
    ${libOpusSourceFileList_std}
)
                             
#-------------------
#--- definitions ---
#-------------------

SET(libOpusCompileDefinitionList
    HAVE_CONFIG_H
)

#==============
#=== Target ===
#==============

IF(WINDOWS)
    # HACK: make malloc.h available as an include file alloca.h in
    # temporary include directory
    SET(allocaFileName "${temporaryOpusIncludeDirectory}/alloca.h")
    FILE(WRITE  ${allocaFileName} "#include <malloc.h>\n")
    FILE(APPEND ${allocaFileName} "#define alloca _malloca\n")
ENDIF()

LINKER_makeLibraryTarget(${targetName} TRUE)
ADD_DEPENDENCIES(SupportLibraries ${targetName})
