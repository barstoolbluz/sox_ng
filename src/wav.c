/* libSoX microsoft's WAVE sound format handler
 *
 * Copyright (C) 1998-2006 Chris Bagwell (cbagwell@sprynet.com)
 * Copyright (C) 1997 Graeme W. Gill
 * Copyright (C) 1992 Rick Richardson
 * Copyright (C) 1991 Lance Norskog And Sundry Contributors
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or (at
 * your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

 /*
  * Info for format tags can be found at:
  *   http://www-mmsp.ece.mcgill.ca/Documents/AudioFormats/WAVE/WAVE.html
  */

#include "sox_i.h"


#include "ima_rw.h"
#include "adpcm.h"
#ifdef EXTERNAL_GSM

#ifdef HAVE_GSM_GSM_H
#include <gsm/gsm.h>
#else
#include <gsm.h>
#endif

#else
#include "../libgsm/gsm.h"
#endif

/* Magic length written when its not possible to write valid lengths.
 * This can be either because of non-seekable output or because
 * the length can not be represented by the 32-bits used in WAV files.
 * When magic length is detected on inputs, disable any length
 * logic.
 */
#define MS_UNSPEC 0x7ffff000

#include "wav-formats.h"

/* To allow padding to samplesPerBlock. Works, but currently never true. */
static const size_t pad_nsamps = sox_false;

/* Private data for .wav file */
typedef struct {
    /* samples/channel reading: starts at total count and decremented  */
    /* writing: starts at 0 and counts samples written */
    uint64_t  numSamples;    
    size_t    dataLength;           /* Needed for ADPCM writing */
    unsigned short formatTag;       /* What type of encoding file is in use */
    unsigned short samplesPerBlock;
    unsigned short blockAlign;
    size_t dataStart;               /* Needed for seeking */
    char           * comment;
    int ignoreSize;                 /* ignoreSize allows us to process 32-bit
                                     * WAV files that are greater then 2Gb
                                     * and can't be represented by
                                     * the 32-bit size field. */

    /* The following are used by *ADPCM wav files */
    unsigned short nCoefs;          /* ADPCM: number of coef sets */
    short         *lsx_ms_adpcm_i_coefs;  /* ADPCM: coef sets */
    void          *ms_adpcm_data;   /* Private data of adpcm decoder */
    unsigned char *packet;          /* Temporary buffer for packets */
    short         *samples;         /* Interleaved samples buffer */
    short         *samplePtr;       /* Pointer to current sample  */
    short         *sampleTop;       /* End of samples-buffer */
    unsigned short blockSamplesRemaining;  /* Samples remaining per channel */
    int            state[16];       /* Step-size info for *ADPCM writes */

    /* The following are used by GSM 6.10 wav files */
    gsm            gsmhandle;
    gsm_signal     *gsmsample;
    int            gsmindex;
    size_t         gsmbytecount;    /* Count of bytes written to data block */
    sox_bool       isRF64;          /* True if file being read is a RF64 */
    uint64_t       ds64_dataSize;   /* Size of data chunk from ds64 header */

    /* WAV write-side diagnostics.  A seekable output can write the header
     * twice, so keep warning state in the format instance rather than
     * emitting duplicate advisories from wavwritehdr(). */
    sox_bool       warnedSamplesUnspec;
    sox_bool       warnedDataUnspec;
    sox_bool       firstHeaderDataUnspec;
} priv_t;

static char *wav_format_str(unsigned wFormatTag);

static int wavwritehdr(sox_format_t *, int);

static const char write_error_msg[] = "write error";
#define write_error() { \
    lsx_fail_errno(ft, SOX_EOF, write_error_msg); \
    return SOX_EOF; \
}

static void wav_warn_once(sox_bool * warned, char const * message)
{
    if (!*warned) {
        lsx_warn("%s", message);
        *warned = sox_true;
    }
}


/****************************************************************************/
/* IMA ADPCM Support Functions Section                                      */
/****************************************************************************/

/*
 *
 * ImaAdpcmReadBlock - Grab and decode complete block of samples
 *
 */
static unsigned short  ImaAdpcmReadBlock(sox_format_t * ft)
{
    priv_t *       wav = (priv_t *) ft->priv;
    size_t bytesRead;
    int samplesThisBlock;

    /* Pull in the packet and check the header */
    bytesRead = lsx_readbuf(ft, wav->packet, (size_t)wav->blockAlign);
    samplesThisBlock = wav->samplesPerBlock;
    if (bytesRead < wav->blockAlign)
    {
        /* If it looks like a valid header is around then try and */
        /* work with partial blocks.  Specs say it should be null */
        /* padded but I guess this is better than trailing quiet. */
        samplesThisBlock = lsx_ima_samples_in((size_t)0, (size_t)ft->signal.channels, bytesRead, (size_t) 0);
        if (samplesThisBlock == 0 || samplesThisBlock > wav->samplesPerBlock)
        {
            lsx_warn("premature EOF on input file");
            return 0;
        }
    }

    wav->samplePtr = wav->samples;

    /* For a full block, the following should be true: */
    /* wav->samplesPerBlock = blockAlign - 8byte header + 1 sample in header */
    lsx_ima_block_expand_i(ft->signal.channels, wav->packet, wav->samples, samplesThisBlock);
    return samplesThisBlock;

}

/****************************************************************************/
/* MS ADPCM Support Functions Section                                       */
/****************************************************************************/

/*
 *
 * AdpcmReadBlock - Grab and decode complete block of samples
 *
 */
static unsigned short  AdpcmReadBlock(sox_format_t * ft)
{
    priv_t *       wav = (priv_t *) ft->priv;
    size_t bytesRead;
    int samplesThisBlock;
    const char *errmsg;

    /* Pull in the packet and check the header */
    bytesRead = lsx_readbuf(ft, wav->packet, (size_t) wav->blockAlign);
    samplesThisBlock = wav->samplesPerBlock;
    if (bytesRead < wav->blockAlign)
    {
        /* If it looks like a valid header is around then try and */
        /* work with partial blocks.  Specs say it should be null */
        /* padded but I guess this is better than trailing quiet. */
        samplesThisBlock = lsx_ms_adpcm_samples_in((size_t)0, (size_t)ft->signal.channels, bytesRead, (size_t)0);
        if (samplesThisBlock == 0 || samplesThisBlock > wav->samplesPerBlock)
        {
            lsx_warn("premature EOF on input file");
            return 0;
        }
    }

    errmsg = lsx_ms_adpcm_block_expand_i(wav->ms_adpcm_data, ft->signal.channels, wav->nCoefs, wav->lsx_ms_adpcm_i_coefs, wav->packet, wav->samples, samplesThisBlock);

    if (errmsg)
        lsx_warn("%s", errmsg);

    return samplesThisBlock;
}

/****************************************************************************/
/* Common ADPCM Write Function                                              */
/****************************************************************************/

static int xxxAdpcmWriteBlock(sox_format_t * ft)
{
    priv_t * wav = (priv_t *) ft->priv;
    size_t chans, ct;
    short *p;

    chans = ft->signal.channels;
    p = wav->samplePtr;
    ct = p - wav->samples;
    if (ct>=chans) {
        /* zero-fill samples if needed to complete block */
        for (p = wav->samplePtr; p < wav->sampleTop; p++) *p=0;
        /* compress the samples to wav->packet */
        if (wav->formatTag == WAVE_FORMAT_ADPCM) {
            lsx_ms_adpcm_block_mash_i((unsigned) chans, wav->samples, wav->samplesPerBlock, wav->state, wav->packet, wav->blockAlign);
        }else{ /* WAVE_FORMAT_IMA_ADPCM */
            lsx_ima_block_mash_i((unsigned) chans, wav->samples, wav->samplesPerBlock, wav->state, wav->packet, 9);
        }
        /* write the compressed packet */
        if (lsx_writebuf(ft, wav->packet, (size_t) wav->blockAlign) != wav->blockAlign)
            write_error();
        /* update lengths and samplePtr */
        wav->dataLength += wav->blockAlign;
        if (pad_nsamps)
          wav->numSamples += wav->samplesPerBlock;
        else
          wav->numSamples += ct/chans;
        wav->samplePtr = wav->samples;
    }
    return (SOX_SUCCESS);
}

/****************************************************************************/
/* WAV GSM6.10 support functions                                            */
/****************************************************************************/
/* create the gsm object, malloc buffer for 160*2 samples */
static int wavgsminit(sox_format_t * ft)
{
    int valueP=1;
    priv_t *       wav = (priv_t *) ft->priv;
    wav->gsmbytecount=0;
    wav->gsmhandle=gsm_create();
    if (!wav->gsmhandle)
    {
        lsx_fail_errno(ft,SOX_EOF,"cannot create GSM object");
        return (SOX_EOF);
    }

    if(gsm_option(wav->gsmhandle,GSM_OPT_WAV49,&valueP) == -1){
        lsx_fail_errno(ft,SOX_EOF,"error setting gsm_option for WAV49 format. Recompile gsm library with -DWAV49 option and relink sox");
        return (SOX_EOF);
    }

    lsx_valloc(wav->gsmsample, 160*2);
    wav->gsmindex=0;
    return (SOX_SUCCESS);
}

/*destroy the gsm object and free the buffer */
static void wavgsmdestroy(sox_format_t * ft)
{
    priv_t *       wav = (priv_t *) ft->priv;
    gsm_destroy(wav->gsmhandle);
    free(wav->gsmsample);
}

static size_t wavgsmread(sox_format_t * ft, sox_sample_t *buf, size_t len)
{
    priv_t *       wav = (priv_t *) ft->priv;
    size_t done=0;
    int bytes;
    gsm_byte    frame[65];

    ft->sox_errno = SOX_SUCCESS;

  /* copy out any samples left from the last call */
    while(wav->gsmindex && (wav->gsmindex<160*2) && (done < len))
        buf[done++]=SOX_SIGNED_16BIT_TO_SAMPLE(wav->gsmsample[wav->gsmindex++],);

  /* read and decode loop, possibly leaving some samples in wav->gsmsample */
    while (done < len) {
        wav->gsmindex=0;
        bytes = lsx_readbuf(ft, frame, (size_t)65);
        if (bytes <=0)
            return done;
        if (bytes<65) {
            lsx_warn("invalid GSM frame size: %d bytes",bytes);
            return done;
        }
        /* decode the long 33 byte half */
        if(gsm_decode(wav->gsmhandle,frame, wav->gsmsample)<0)
        {
            lsx_fail_errno(ft,SOX_EOF,"error during gsm decode");
            return 0;
        }
        /* decode the short 32 byte half */
        if(gsm_decode(wav->gsmhandle,frame+33, wav->gsmsample+160)<0)
        {
            lsx_fail_errno(ft,SOX_EOF,"error during gsm decode");
            return 0;
        }

        while ((wav->gsmindex <160*2) && (done < len)){
            buf[done++]=SOX_SIGNED_16BIT_TO_SAMPLE(wav->gsmsample[(wav->gsmindex)++],);
        }
    }

    return done;
}

static int wavgsmflush(sox_format_t * ft)
{
    gsm_byte    frame[65];
    priv_t *       wav = (priv_t *) ft->priv;

    /* zero fill as needed */
    while(wav->gsmindex<160*2)
        wav->gsmsample[wav->gsmindex++]=0;

    /*encode the even half short (32 byte) frame */
    gsm_encode(wav->gsmhandle, wav->gsmsample, frame);
    /*encode the odd half long (33 byte) frame */
    gsm_encode(wav->gsmhandle, wav->gsmsample+160, frame+32);
    if (lsx_writebuf(ft, frame, (size_t) 65) != 65)
        write_error();
    wav->gsmbytecount += 65;

    wav->gsmindex = 0;
    return (SOX_SUCCESS);
}

static size_t wavgsmwrite(sox_format_t * ft, const sox_sample_t *buf, size_t len)
{
    priv_t * wav = (priv_t *) ft->priv;
    size_t done = 0;
    int rc;

    ft->sox_errno = SOX_SUCCESS;

    while (done < len) {
        SOX_SAMPLE_LOCALS;
        while ((wav->gsmindex < 160*2) && (done < len))
            wav->gsmsample[(wav->gsmindex)++] =
                SOX_SAMPLE_TO_SIGNED_16BIT(buf[done++], ft->clips);

        if (wav->gsmindex < 160*2)
            break;

        rc = wavgsmflush(ft);
        if (rc)
            return 0;
    }
    return done;

}

static int wavgsmstopwrite(sox_format_t * ft)
{
    priv_t *       wav = (priv_t *) ft->priv;

    ft->sox_errno = SOX_SUCCESS;

    if (wav->gsmindex)
        if (wavgsmflush(ft))
            write_error();

    /* Add a pad byte if amount of written bytes is not even. */
    if (wav->gsmbytecount && wav->gsmbytecount % 2){
        if(lsx_writeb(ft, 0))
            write_error();
	wav->gsmbytecount += 1;
    }

    wavgsmdestroy(ft);

    return SOX_SUCCESS;
}

/****************************************************************************/
/* General Sox WAV file code                                                */
/****************************************************************************/

static int sndfile_workaround(uint64_t *len, sox_format_t *ft) {
    char magic[5];
    off_t here;

    here = lsx_tell(ft);

    lsx_debug("Attempting work around for bad ds64 length bug");

    /* Seek to last four bytes of chunk, assuming size is correct. */
    if (lsx_seeki(ft, (off_t)(*len)-4, SEEK_CUR) != SOX_SUCCESS)
    {
        lsx_fail_errno(ft, SOX_EHDR, "WAV chunk appears to have invalid size %" PRIu64 ".", *len);
        return SOX_EOF;
    }

    /* Get the last four bytes to see if it is an "fmt " chunk */
    if (lsx_reads(ft, magic, (size_t)4) == SOX_EOF)
    {
        lsx_fail_errno(ft,SOX_EHDR, "WAV chunk appears to have invalid size %" PRIu64 ".", *len);
        return SOX_EOF;
    }

    /* Seek back to where we were, which won't work if you're piping */
    if (lsx_seeki(ft, here, SEEK_SET)!=SOX_SUCCESS)
    {
        lsx_fail_errno(ft,SOX_EHDR, "cannot seek backwards to work around possible broken header");
        return SOX_EOF;
    }
    if (memcmp(magic, "fmt ", (size_t)4)==0)
    {
        /* If the last four bytes were "fmt ", len is almost certainly four bytes too big. */
        lsx_debug("File had libsndfile bug, working around tell=%" PRId64, (uint64_t)lsx_tell(ft));
        *len -= 4;
    }
    return SOX_SUCCESS;
}

static int findChunk(sox_format_t * ft, const char *Label, uint64_t *len)
{
    char magic[5];
    priv_t *wav = (priv_t *) ft->priv;
    uint32_t len_tmp;

    lsx_debug("Searching for %2x %2x %2x %2x", Label[0], Label[1], Label[2], Label[3]);
    for (;;)
    {
        if (lsx_reads(ft, magic, (size_t)4) == SOX_EOF)
        {
            lsx_fail_errno(ft, SOX_EHDR, "file is missing the %s chunk",
                          Label);
            return SOX_EOF;
        }
        lsx_debug("WAV Chunk %s", magic);
        if (lsx_readdw(ft, &len_tmp) == SOX_EOF)
        {
            lsx_fail_errno(ft, SOX_EHDR, "%s chunk is too short",
                          magic);
            return SOX_EOF;
        }

        if (len_tmp == 0xffffffff && wav->isRF64==sox_true)
        {
            /* Chunk length should come from ds64 header */
            if (memcmp(magic, "data", (size_t)4)==0)
            {
                *len = wav->ds64_dataSize;
            }
            else
            {
                lsx_fail_errno(ft, SOX_EHDR, "cannot yet read block sizes of arbitrary RF64 chunks, cannot find chunk '%s'", Label);
                return SOX_EOF;
            }
        }
        else {
            *len = len_tmp;
        }

        /* Work around for a bug in libsndfile
         * https://github.com/erikd/libsndfile/commit/7fa1c57c37844a9d44642ea35e6638238b8af19e#src/rf64.c
           The ds64 chunk should be 0x1c bytes, not 0x20.
         */
        if ((*len) == 0x20 && memcmp(Label, "ds64", (size_t)4)==0)
        {
            int fail;
            if ((fail = sndfile_workaround(len, ft)) != SOX_SUCCESS) {
                return fail;
            }
        }

        if (memcmp(Label, magic, (size_t)4) == 0)
            break; /* Found the given chunk */

        /* Chunks are required to be word aligned. */
        if ((*len) % 2) (*len)++;

        /* skip to next chunk */
        if (!*len || lsx_seeki(ft, (off_t)(*len), SEEK_CUR) != SOX_SUCCESS)
        {
            lsx_fail_errno(ft,SOX_EHDR,
                          "chunk appears to have invalid size %" PRIu64 ".", *len);
            return SOX_EOF;
        }
    }
    return SOX_SUCCESS;
}


static int wavfail(sox_format_t * ft)
{
  static struct subtype_name {
    uint16_t subtype;
    char const *name;
  } subtype_names[] = {
    /* References:
     * https://datatracker.ietf.org/doc/html/rfc2361#appendix-A
     * https://py-waveinfo.readthedocs.io/en/stable/ref_enums
     * https://www.recordingblogs.com/wiki/format-chunk-of-a-wave-file
     * https://codec.kyiv.ua/audio.html
     */
    { WAVE_FORMAT_VSELP,             "Compaq VSELP" },
    { WAVE_FORMAT_IBM_CVSD,          "IBM CVSD" },
    { WAVE_FORMAT_DTS,               "Microsoft DTS" },
    { WAVE_FORMAT_DRM,               "DRM" },
    { WAVE_FORMAT_WMAVOICE9,         "WMA 9 Speech" },
    { WAVE_FORMAT_WMAVOICE10,        "WMA 10 Speech" },
    { WAVE_FORMAT_OKI_ADPCM,         "OKI-ADPCM" },
    { WAVE_FORMAT_MEDIASPACE_ADPCM,  "Videologic Mediaspace ADPCM" },
    { WAVE_FORMAT_SIERRA_ADPCM,      "Sierra ADPCM" },
    { WAVE_FORMAT_G723_ADPCM,        "Antex G.723 ADPCM" },
    { WAVE_FORMAT_DIGISTD,           "DSP Solutions DIGISTD" },
    { WAVE_FORMAT_DIGIFIX,           "DSP Solutions DIGIFIX" },
    { WAVE_FORMAT_DIALOGIC_OKI_ADPCM,"Dialogic OKI ADPCM" },
    { WAVE_FORMAT_MEDIAVISION_ADPCM, "Media Vision ADPCM" },
    { WAVE_FORMAT_CU_CODEC,          "HP CU" },
    { WAVE_FORMAT_HP_DYN_VOICE,      "HP Dynamic Voice" },
    { WAVE_FORMAT_YAMAHA_ADPCM,      "Yamaha ADPCM" },
    { WAVE_FORMAT_SONARC,            "SONARC Speech Compression" },
    { WAVE_FORMAT_DSPGROUP_TRUESPEECH,"DSP Group True Speech" },
    { WAVE_FORMAT_ECHOSC1,           "Echo Speech" },
    { WAVE_FORMAT_AUDIOFILE_AF36,    "Virtual Music Audiofile AF36" },
    { WAVE_FORMAT_APTX,              "Audio Processing Tech." },
    { WAVE_FORMAT_AUDIOFILE_AF10,    "Virtual Music Audiofile AF10" },
    { WAVE_FORMAT_PROSODY_1612,      "Aculab Prosody 1612" },
    { WAVE_FORMAT_LRC, "Merging Tech. LRC" },
    { WAVE_FORMAT_DOLBY_AC2,         "Dolby AC2" },
    { WAVE_FORMAT_MSNAUDIO,          "MSN Audio" },
    { WAVE_FORMAT_ANTEX_ADPCME,      "Antex ADPCME" },
    { WAVE_FORMAT_CONTROL_RES_VQLPC, "Control Resources VQLPC" },
    { WAVE_FORMAT_DIGIREAL,          "DSP Solutions DIGIREAL" },
    { WAVE_FORMAT_DIGIADPCM,         "DSP Solutions DIGIADPCM" },
    { WAVE_FORMAT_CONTROL_RES_CR10,  "Control Resources CR10" },
    { WAVE_FORMAT_NMS_VBXADPCM,      "Natural MiscroSystems VBX ADPCM" },
    { WAVE_FORMAT_CS_IMAADPCM,       "Crystal Semiconductors IMA ADPCM" },
    { WAVE_FORMAT_ECHOSC3,           "Echo Speech SC3" },
    { WAVE_FORMAT_ROCKWELL_ADPCM,    "Rockwell ADPCM" },
    { WAVE_FORMAT_ROCKWELL_DIGITALK, "Rockwell DIGITALK" },
    { WAVE_FORMAT_XEBEC,             "Xebec Multimedia" },
    { WAVE_FORMAT_G721_ADPCM,        "Antex G.721 ADPCM" },
    { WAVE_FORMAT_G728_CELP,         "Antex G.728 CELP" },
    { WAVE_FORMAT_MSG723,            "Microsoft G.723" },
    { WAVE_FORMAT_INTEL_G723_1,      "IBM AVC ADPCM" },
    { WAVE_FORMAT_INTEL_G729,        "Intel G.729" },
    { WAVE_FORMAT_SHARP_G726,        "Sharp G.726" },
    { WAVE_FORMAT_MPEG,              "Microsoft MPEG" },
    { WAVE_FORMAT_RT23,              "RT32 or PAC" },
    { WAVE_FORMAT_RT24,              "InSoft RT24" },
    { WAVE_FORMAT_PAC,               "InSoft PAC" },
    { WAVE_FORMAT_MPEGLAYER3,        "MP3" },
    { WAVE_FORMAT_LUCENT_G723,       "Lucent G.723" },
    { WAVE_FORMAT_CIRRUS,            "Cirrus Logic" },
    { WAVE_FORMAT_ESPCM,             "ESS Tech. PCM" },
    { WAVE_FORMAT_VOXWARE,           "Voxware Inc." },
    { WAVE_FORMAT_CANOPUS_ATRAC,     "Canopus ATRAC" },
    { WAVE_FORMAT_G726_ADPCM,        "APICOM G.726 ADPCM" },
    { WAVE_FORMAT_G722_ADPCM,        "APICOM G.722 ADPCM" },
    { WAVE_FORMAT_DSAT,              "Microsoft DSAT" },
    { WAVE_FORMAT_DSAT_DISPLAY,      "Microsoft DSAT-DISPLAY" },
    { WAVE_FORMAT_VOXWARE_BYTE_ALIGNED, "Voxware Byte Aligned" },
    { WAVE_FORMAT_VOXWARE_AC8,       "Voxware AC8" },
    { WAVE_FORMAT_VOXWARE_AC10,      "Voxware AC10" },
    { WAVE_FORMAT_VOXWARE_AC16,      "Voxware AC16" },
    { WAVE_FORMAT_VOXWARE_AC20,      "Voxware AC20" },
    { WAVE_FORMAT_VOXWARE_RT24,      "Voxware RT24 MetaVoice" },
    { WAVE_FORMAT_VOXWARE_RT29,      "Voxware RT29 MetaSound" },
    { WAVE_FORMAT_VOXWARE_RT29HW,    "Voxware RT29HW" },
    { WAVE_FORMAT_VOXWARE_VR12,      "Voxware VR12" },
    { WAVE_FORMAT_VOXWARE_VR18,      "Voxware VR18" },
    { WAVE_FORMAT_VOXWARE_TQ40,      "Voxware TQ40" },
    { WAVE_FORMAT_VOXWARE_SC3,       "Voxware SC3" },
    { WAVE_FORMAT_VOXWARE_SC3_1,     "Voxware SC3.1" },
    { WAVE_FORMAT_SOFTSOUND,         "Soundsoft" },
    { WAVE_FORMAT_VOXWARE_TQ60,      "Voxware TQ60" },
    { WAVE_FORMAT_MSRT24,            "Microsoft MSRT24" },
    { WAVE_FORMAT_G729A,             "AT&T G.729a" },
    { WAVE_FORMAT_MVI_MVI2,          "Motion Pixels MVI-MVI2" },
    { WAVE_FORMAT_DF_G726,           "DataFusion G.726" },
    { WAVE_FORMAT_DF_GSM610,         "DataFusion GSM610" },
    { WAVE_FORMAT_ISIAUDIO,          "Iterated Systems Audio" },
    { WAVE_FORMAT_ONLIVE,            "OnLive" },
    { WAVE_FORMAT_MULTITUDE_FT_SX20, "Multitude FT SX20" },
    { WAVE_FORMAT_INFOCOM_ITS_G721_ADPCM, "Infocom ITS A/SG.721 ADPCM" },
    { WAVE_FORMAT_CONVEDIA_G729,     "Convedia G.729" },
    { WAVE_FORMAT_CONGRUENCY,        "Congruency Inc." },
    { WAVE_FORMAT_SBC24,             "Siemens SBC24" },
    { WAVE_FORMAT_DOLBY_AC3_SPDIF,   "Sonic Foundry Dolby AC3 S/PDIF" },
    { WAVE_FORMAT_MEDIASONIC_G723,   "Mediasonic G.723" },
    { WAVE_FORMAT_PROSODY_8KBPS,     "Aculab Prosody 8kbps" },
    { WAVE_FORMAT_ZYXEL_ADPCM,       "ZyXEL ADPCM" },
    { WAVE_FORMAT_PHILIPS_LPCBB,     "Philips LPCBB" },
    { WAVE_FORMAT_PACKED,            "Studer Professional Audio Packed" },
    { WAVE_FORMAT_MALDEN_PHONYTALK,  "Malden PhonyTalk" },
    { WAVE_FORMAT_RACAL_RECORDER_GSM,"Racal Recorder GSM" },
    { WAVE_FORMAT_RACAL_RECORDER_G720_A, "Racal Recorder G.720a" },
    { WAVE_FORMAT_RACAL_RECORDER_G723_1, "Racal Recorder G.723.1" },
    { WAVE_FORMAT_RACAL_RECORDER_TETRA_ACELP, "Racal Recorder Tetra ACELP" },
    { WAVE_FORMAT_NEC_AAC,           "NEC AAC" },
    { WAVE_FORMAT_RAW_AAC1,          "Raw AAC1" },
    { WAVE_FORMAT_RHETOREX_ADPCM,    "Rhetorex ADPCM" },
    { WAVE_FORMAT_IRAT,              "BeCubed Software IRAT" },
    { WAVE_FORMAT_VIVO_G723,         "Vivo G.723" },
    { WAVE_FORMAT_VIVO_SIREN,        "Vivo Siren" },
    { WAVE_FORMAT_PHILIPS_CELP,      "Philips Speech Processing CELP" },
    { WAVE_FORMAT_PHILIPS_GRUNDIG,   "Philips Speech Processing GRUNDIG" },
    { WAVE_FORMAT_DIGITAL_G723,      "Digital G.723" },
    { WAVE_FORMAT_SANYO_LD_ADPCM,    "Sanyo LD ADPCM" },
    { WAVE_FORMAT_SIPROLAB_ACEPLNET, "Sipro Lab ACEPLNET" },
    { WAVE_FORMAT_SIPROLAB_ACELP4800,"Sipro Lab ACELP4800" },
    { WAVE_FORMAT_SIPROLAB_ACELP8V3, "Sipro Lab ACELP8V3" },
    { WAVE_FORMAT_SIPROLAB_G729,     "Sipro Lab G.729" },
    { WAVE_FORMAT_SIPROLAB_G729A,    "Sipro Lab G.729a" },
    { WAVE_FORMAT_SIPROLAB_KELVIN,   "Sipro Lab KELVIN" },
    { WAVE_FORMAT_VOICEAGE_AMR,      "VoiceAge AMR" },
    { WAVE_FORMAT_G726ADPCM,         "Dictaphone G.726 ADPCM" },
    { WAVE_FORMAT_DICTAPHONE_CELP68, "Dictaphone CELP68" },
    { WAVE_FORMAT_DICTAPHONE_CELP54, "Dictaphone CELP54" },
    { WAVE_FORMAT_QUALCOMM_PUREVOICE,"Qualcomm Purevoice" },
    { WAVE_FORMAT_QUALCOMM_HALFRATE, "Qualcomm Halfrate" },
    { WAVE_FORMAT_TUBGSM,            "Ring Zero Systems TUBGSM" },
    { WAVE_FORMAT_MSAUDIO1,          "Microsoft Audio 1" },
    { WAVE_FORMAT_WMAUDIO2,          "Windows Media Audio V2" },
    { WAVE_FORMAT_WMAUDIO3,          "Windows Media Audio Professional V9" },
    { WAVE_FORMAT_WMAUDIO_LOSSLESS,  "Windows Media Audio Lossless V9" },
    { WAVE_FORMAT_WMASPDIF,          "WMA Pro over S(PDIF" },
    { WAVE_FORMAT_UNISYS_NAP_ADPCM,  "Unisys NAP ADPCM" },
    { WAVE_FORMAT_UNISYS_NAP_ULAW,   "Unisys NAP ULAW" },
    { WAVE_FORMAT_UNISYS_NAP_ALAW,   "Unisys NAP ALAW" },
    { WAVE_FORMAT_UNISYS_NAP_16K,    "Unisys NAP 16K" },
    { WAVE_FORMAT_SYCOM_ACM_SYC008,  "Sycom ACM SYC008" },
    { WAVE_FORMAT_SYCOM_ACM_SYC701_G726L, "Sycom ACM SYC701 G.726L" },
    { WAVE_FORMAT_SYCOM_ACM_SYC701_CELP54, "Sycom ACM CELP54" },
    { WAVE_FORMAT_SYCOM_ACM_SYC701_CELP68, "Sycom ACM CELP68" },
    { WAVE_FORMAT_KNOWLEDGE_ADVENTURE_ADPCM, "Knowledge Adventure ADPCM" },
    { WAVE_FORMAT_FRAUNHOFER_IIS_MPEG2_AAC, "Fraunhofer IIS MPEG2 AAC" },
    { WAVE_FORMAT_DTS_DS,            "Digital Theatre Systems DS" },
    { WAVE_FORMAT_CREATIVE_ADPCM,    "Creative Labs ADPCM" },
    { WAVE_FORMAT_CREATIVE_FASTSPEECH8, "Creative Labs FastSpeech 8" },
    { WAVE_FORMAT_CREATIVE_FASTSPEECH10, "Creative Labs FastSpeech 10" },
    { WAVE_FORMAT_UHER_ADPCM,        "Uher ADPCM" },
    { WAVE_FORMAT_ULEAD_DV_AUDIO,    "Ulead DV ACM" },
    { WAVE_FORMAT_ULEAD_DV_AUDIO_1,  "Ulead DV ACM" },
    { WAVE_FORMAT_QUARTERDECK,       "Quarterdeck" },
    { WAVE_FORMAT_ILINK_VC,          "I-Link VC" },
    { WAVE_FORMAT_RAW_SPORT,         "Aureal Semiconductor Raw Sport" },
    { WAVE_FORMAT_ESST_AC3,          "ESST AC3" },
    { WAVE_FORMAT_GENERIC_PASSTHRU,  "Generic Passthru" },
    { WAVE_FORMAT_IPI_HSX,           "Interactive Products HSX" },
    { WAVE_FORMAT_IPI_RPELP,         "Interactive Products RPELP" },
    { WAVE_FORMAT_CS2,               "Consistent CS2" },
    { WAVE_FORMAT_SONY_SCX,          "Sony SCX" },
    { WAVE_FORMAT_SONY_SCY,          "Sony SCY" },
    { WAVE_FORMAT_SONY_ATRAC3,       "Sony ATRAC3" },
    { WAVE_FORMAT_SONY_SPC,          "Sony SPC" },
    { WAVE_FORMAT_TELUM_AUDIO,       "Telum Audio" },
    { WAVE_FORMAT_TELUM_IA_AUDIO,    "Telum IA Audio" },
    { WAVE_FORMAT_NORCOM_VOICE_SYSTEMS_ADPCM, "Norcom Voice Systems ADPCM" },
    { WAVE_FORMAT_FM_TOWNS_SND,      "Fujitsu FM Towns SND" },
    { WAVE_FORMAT_MICRONAS,          "Micronas Semiconductors Development" },
    { WAVE_FORMAT_MICRONAS_CELP833,  "Micronas Semiconductors CELP833" },
    { WAVE_FORMAT_BTV_DIGITAL,       "Brooktree Digital" },
    { WAVE_FORMAT_INTEL_MUSIC_CODER, "Intel Music Coder" },
    { WAVE_FORMAT_INDEO_AUDIO,       "Ligos Indeo Audio" },
    { WAVE_FORMAT_QDESIGN_MUSIC,     "QDesign Music" },
    { WAVE_FORMAT_ON2_VP7_AUDIO,     "On2 VP7" },
    { WAVE_FORMAT_ON2_VP6_AUDIO,     "On2 VP6" },
    { WAVE_FORMAT_VME_VMPCM,         "AT&T VME VMPCM" },
    { WAVE_FORMAT_TPC,               "AT&T TPC" },
    { WAVE_FORMAT_YMPEG,             "YMPEG" },
    { WAVE_FORMAT_LIGHTWAVE_LOSSLESS,"ClearJump LightWave Lossless" },
    { WAVE_FORMAT_OLIGSM,            "Olivetti GSM" },
    { WAVE_FORMAT_OLIADPCM,          "Olivetti ADPCM" },
    { WAVE_FORMAT_OLICELP,           "Olivetti CELP" },
    { WAVE_FORMAT_OLISBC,            "Olivetti SBC" },
    { WAVE_FORMAT_OLIOPR,            "Olivetti OPR" },
    { WAVE_FORMAT_LH_CODEC,          "Lernout & Hauspie" },
    { WAVE_FORMAT_LH_CODEC_CELP,     "Lernout & Hauspie CELP" },
    { WAVE_FORMAT_LH_CODEC_SBC8,     "Lernout & Hauspie SBC8" },
    { WAVE_FORMAT_LH_CODEC_SBC12,    "Lernout & Hauspie SBC12" },
    { WAVE_FORMAT_LH_CODEC_SBC16,    "Lernout & Hauspie SBC16" },
    { WAVE_FORMAT_NORRIS,            "Norris Comm. Inc." },
    { WAVE_FORMAT_ISIAUDIO_2,        "Iterated Systems Audio 2" },
    { WAVE_FORMAT_SOUNDSPACE_MUSICOMPRESS, "AT&T Soundspace Music Compression" },
    { WAVE_FORMAT_MPEG_ADTS_AAC,     "MPEG ADTS AAC" },
    { WAVE_FORMAT_MPEG_RAW_AAC,      "MPEG Raw AAC" },
    { WAVE_FORMAT_MPEG_LOAS,         "MPEG LOAS" },
    { WAVE_FORMAT_NOKIA_MPEG_ADTS_AAC, "Nokia MPEG ADTS AAC" },
    { WAVE_FORMAT_NOKIA_MPEG_RAW_AAC,"Nokia MPEG RAW AAC" },
    { WAVE_FORMAT_VODAFONE_MPEG_ADTS_AAC, "Vodafone MPEG ADTS_AAC" },
    { WAVE_FORMAT_VODAFONE_MPEG_RAW_AAC, "Vodafone MPEG RAW AAC" },
    { WAVE_FORMAT_MPEG_HEAAC,        "MPEG HEAAC" },
    { WAVE_FORMAT_VOXWARE_RT24_SPEECH, "Voxware ToolVox RT24 Speech codec" },
    { WAVE_FORMAT_LUCENT_AX24000P,   "Lucent AX24000P" },
    { WAVE_FORMAT_SONICFOUNDRY_LOSSLESS, "Sonic Foundry LOSSLESS" },
    { WAVE_FORMAT_INNINGS_TELECOM_ADPCM, "Innings Telecom ADPCM" },
    { WAVE_FORMAT_LUCENT_SX8300P,    "Lucent SX8300P speech codec" },
    { WAVE_FORMAT_LUCENT_SX5363S,    "Lucent SX5363S G.723 compliant codec" },
    { WAVE_FORMAT_CUSEEME,           "CU-SeeMe Digitalk" },
    { WAVE_FORMAT_NTCSOFT_ALF2CM_ACM,"NTC Soft ALF2CM ACM" },
    { WAVE_FORMAT_DVM,               "FAST Multimedia DVM" },
    { WAVE_FORMAT_DTS2,              "Dolby Digital Theatre System" },
    { WAVE_FORMAT_RA_14,             "RealAudio 1/2 14.4" },
    { WAVE_FORMAT_RA_28,             "RealAudio 1/2 28.8" },
    { WAVE_FORMAT_RA_G2,             "RealAudio 2/8 Cook (low bitrate)" },
    { WAVE_FORMAT_RA_DNET,           "RealAudio 3/4/5 DNET" },
    { WAVE_FORMAT_RA_RAAC,           "RealAudio 10 AAC (RAAC)" },
    { WAVE_FORMAT_RA_RACP,           "RealAudio 10 AAC+ (RACP)" },
    { WAVE_FORMAT_FFMPEG_SONIC,      "FFmpeg Sonic" },
    { WAVE_FORMAT_MAKEAVIS,          "AviSynth" },
    { WAVE_FORMAT_DIVIO_MPEG4_AAC,   "Divio MPEG-4 AAC" },
    { WAVE_FORMAT_NOKIA_ADAPTIVE_MULTIRATE, "Nokia adaptive multirate" },
    { WAVE_FORMAT_DIVIO_G726,        "Divio G.726" },
    { WAVE_FORMAT_LEAD_SPEECH,       "LEAD Speech" },
    { WAVE_FORMAT_FFMPEG_ADPCM,      "FFmpeg ADPCM" },
    { WAVE_FORMAT_LEAD_VORBIS,       "LEAD Vorbis" },
    { WAVE_FORMAT_WAVPACK_AUDIO,     "WavPack" },
    { WAVE_FORMAT_OGG_VORBIS_MODE_1, "Ogg Vorbis" },
    { WAVE_FORMAT_OGG_VORBIS_MODE_2, "Ogg Vorbis" },
    { WAVE_FORMAT_OGG_VORBIS_MODE_3, "Ogg Vorbis" },
    { WAVE_FORMAT_OGG_VORBIS_MODE_1_PLUS, "Ogg Vorbis" },
    { WAVE_FORMAT_OGG_VORBIS_MODE_2_PLUS, "Ogg Vorbis" },
    { WAVE_FORMAT_OGG_VORBIS_MODE_3_PLUS, "Ogg Vorbis" },
    { WAVE_FORMAT_ALAC,              "ALAC" },
    { WAVE_FORMAT_3COM_NBX,          "3Com NBX" },
    { WAVE_FORMAT_OPUS,              "Opus" },
    { WAVE_FORMAT_FAAD_AAC,          "FAAD AAC" },
    { WAVE_FORMAT_AMR_NB,            "AMR (narrow band)" },
    { WAVE_FORMAT_AMR_WB,            "AMR (wide band)" },
    { WAVE_FORMAT_AMR_WP,            "AMR (adaptive multi-rate)" },
    { WAVE_FORMAT_GSM_AMR_CBR,       "GSM-AMR CBR" },
    { WAVE_FORMAT_GSM_AMR_VBR_SID,   "GSM-AMR VBR with SID)" },
    { WAVE_FORMAT_COMVERSE_INFOSYS_G723_1, "Comverse Infosys G.723.1" },
    { WAVE_FORMAT_COMVERSE_INFOSYS_AVQSBC, "Comverse Infosys AVQSBC" },
    { WAVE_FORMAT_COMVERSE_INFOSYS_SBC, "Comverse Infosys OLDSBC" },
    { WAVE_FORMAT_SYMBOL_G729_A,     "Symbol Technologies G.729a" },
    { WAVE_FORMAT_VOICEAGE_AMR_WB,   "Voiceage AMR WB" },
    { WAVE_FORMAT_INGENIENT_G726,    "Ingenient Technologies G.726" },
    { WAVE_FORMAT_MPEG4_AAC,         "ISO/MPEG4 AAC" },
    { WAVE_FORMAT_ENCORE_G726,       "Encore Software G.726" },
    { WAVE_FORMAT_ZOLL_ASAO,         "Zoll ASAO" },
    { WAVE_FORMAT_SPEEX_VOICE,       "Xiph Speex" },
    { WAVE_FORMAT_VIANIX_MASC,       "Vianix MASC" },
    { WAVE_FORMAT_WM9_SPECTRUM_ANALYZER, "WM9 Spectrum Analyzer" },
    { WAVE_FORMAT_WMF_SPECTRUM_ANAYZER, "WMF Spectrum Anayzer" },
    { WAVE_FORMAT_GSM_610,           "GSM610" },
    { WAVE_FORMAT_GSM_620,           "GSM620" },
    { WAVE_FORMAT_GSM_660,           "GSM660" },
    { WAVE_FORMAT_GSM_690,           "GSM690" },
    { WAVE_FORMAT_GSM_ADAPTIVE_MULTIRATE_WB, "GSM Adaptive Multirate WB" },
    { WAVE_FORMAT_POLYCOM_G722,      "Polycom G.722" },
    { WAVE_FORMAT_POLYCOM_G728,      "Polycom G.728" },
    { WAVE_FORMAT_POLYCOM_G729_A,    "Polycom G.729a" },
    { WAVE_FORMAT_POLYCOM_SIREN,     "Polycom Siren" },
    { WAVE_FORMAT_GLOBAL_IP_ILBC,    "Global IP ILBC" },
    { WAVE_FORMAT_RADIOTIME_TIME_SHIFT_RADIO, "Radiotime time shift radio" },
    { WAVE_FORMAT_NICE_ACA,          "NICE ACA" },
    { WAVE_FORMAT_NICE_ADPCM,        "NICE ADPCM" },
    { WAVE_FORMAT_VOCORD_G721,       "Vocord G.721" },
    { WAVE_FORMAT_VOCORD_G726,       "Vocord G.726" },
    { WAVE_FORMAT_VOCORD_G722_1,     "Vocord G.722.1" },
    { WAVE_FORMAT_VOCORD_G728,       "Vocord G.728" },
    { WAVE_FORMAT_VOCORD_G729,       "Vocord G.729" },
    { WAVE_FORMAT_VOCORD_G729_A,     "Vocord G.729a" },
    { WAVE_FORMAT_VOCORD_G723_1,     "VOCORD G.723.1" },
    { WAVE_FORMAT_VOCORD_LBC,        "VOCORD LBC" },
    { WAVE_FORMAT_NICE_G728,         "NICE_G.728" },
    { WAVE_FORMAT_FRACE_TELECOM_G729,"France Telecom G.729a" },
    { WAVE_FORMAT_CODIAN,            "Codian" },
    { WAVE_FORMAT_DOLBY_AC4,         "Dolby AC4" },
    { WAVE_FORMAT_DFAC,              "DebugMode FrameServer ACM" },
    { WAVE_FORMAT_FLAC,              "FLAC" },
    { 0, NULL }
  };
  struct subtype_name *snp;
  unsigned short subtype = ((priv_t *)ft->priv)->formatTag;
  char const *name = "Unknown";

  for (snp=subtype_names; snp->subtype != 0; snp++) {
    if (snp->subtype == subtype) {
      name = snp->name;
      break;
    }
  }

  lsx_fail_errno(ft, SOX_EHDR, "file encoding 0x%04x (%s) is not supported", subtype, name);
  return SOX_EOF;
}

static const char read_error_msg[] = "file is truncated";
#define read_error() { \
    lsx_fail_errno(ft, SOX_EOF, read_error_msg); \
    return SOX_EOF; \
}

/*
 * Do anything required before you start reading samples.
 * Read file header.
 *      Find out sampling rate,
 *      size and encoding of samples,
 *      mono/stereo/quad.
 */
static int startread_wav(sox_format_t * ft)
{
    priv_t *       wav = (priv_t *) ft->priv;
    char        magic[5];
    uint64_t    len;

    /* wave file characteristics */
    uint64_t      qwRiffLength;
    uint32_t      dwRiffLength_tmp;
    unsigned short wChannels;       /* number of channels */
    uint32_t      dwSamplesPerSecond; /* samples per second per channel */
    uint32_t      dwAvgBytesPerSec;/* estimate of bytes per second needed */
    uint16_t wBitsPerSample = 0;  /* bits per sample */
    uint32_t wFmtSize;
    uint16_t wExtSize = 0;    /* extended field for non-PCM */

    uint64_t      qwDataLength;    /* length of sound data in bytes */
    size_t    bytesPerBlock = 0;
    int    bytespersample;          /* bytes per sample (per channel */
    char text[256];
    uint32_t      dwLoopPos;

    ft->sox_errno = SOX_SUCCESS;
    wav->ignoreSize = ft->signal.length == SOX_IGNORE_LENGTH;

    if (lsx_reads(ft, magic, (size_t)4) == SOX_EOF)
	read_error();
    if ((strncmp("RIFF", magic, (size_t)4) != 0 &&
         strncmp("RIFX", magic, (size_t)4) != 0 &&
	 strncmp("RF64", magic, (size_t)4)!=0 ))
    {
        lsx_fail_errno(ft,SOX_EHDR,"RIFF header not found");
        return SOX_EOF;
    }

    /* RIFX is a Big-endian RIFF */
    if (strncmp("RIFX", magic, (size_t)4) == 0)
    {
        lsx_debug("Found RIFX header");
        ft->encoding.reverse_bytes = MACHINE_IS_LITTLEENDIAN;
    }
    else ft->encoding.reverse_bytes = MACHINE_IS_BIGENDIAN;

    if (strncmp("RF64", magic, (size_t)4) == 0)
    {
        wav->isRF64 = sox_true;
    }
    else
    {
        wav->isRF64 = sox_false;
    }

    if (lsx_readdw(ft, &dwRiffLength_tmp)) {
        lsx_fail_errno(ft,SOX_EHDR,"header not found");
        return SOX_EOF;
    }
    qwRiffLength = dwRiffLength_tmp;

    if (lsx_reads(ft, magic, (size_t)4) == SOX_EOF || strncmp("WAVE", magic, (size_t)4))
    {
        lsx_fail_errno(ft,SOX_EHDR,"header not found");
        return SOX_EOF;
    }

    if (wav->isRF64 && findChunk(ft, "ds64", &len) != SOX_EOF) {
        lsx_debug("Found ds64 header");

        if (dwRiffLength_tmp==0xffffffff)
        {
            if (lsx_readqw(ft, &qwRiffLength))
                read_error();
        }
        else
        {
            if (lsx_skipbytes(ft, (size_t)8))
                read_error();
        }
        if (lsx_readqw(ft, &wav->ds64_dataSize) ||
            lsx_skipbytes(ft, (size_t)len-16))
                read_error();
    }

    /* Now look for the format chunk */
    if (findChunk(ft, "fmt ", &len) == SOX_EOF)
    {
        lsx_fail_errno(ft,SOX_EHDR,"fmt chunk not found");
        return SOX_EOF;
    }
    wFmtSize = len;

    if (wFmtSize < 16)
    {
        lsx_fail_errno(ft,SOX_EHDR,"fmt chunk is too short");
        return SOX_EOF;
    }

    if (lsx_readw(ft, &(wav->formatTag)) ||
        lsx_readw(ft, &wChannels) ||
        lsx_readdw(ft, &dwSamplesPerSecond) ||
        lsx_readdw(ft, &dwAvgBytesPerSec) || /* Average bytes/second */
        lsx_readw(ft, &(wav->blockAlign)) || /* Block align */
        lsx_readw(ft, &wBitsPerSample))      /* bits per sample per channel */
        read_error();
    len -= 16;

    if (wav->formatTag == WAVE_FORMAT_EXTENSIBLE)
    {
      uint16_t extensionSize;
      uint16_t numberOfValidBits;
      uint32_t speakerPositionMask;
      uint16_t subFormatTag;
      uint8_t dummyByte;
      int i;

      if (wFmtSize < 18)
      {
        lsx_fail_errno(ft,SOX_EHDR,"fmt chunk is too short");
        return SOX_EOF;
      }
      if (lsx_readw(ft, &extensionSize))
        read_error();
      len -= 2;
      if (extensionSize < 22)
      {
        lsx_fail_errno(ft,SOX_EHDR,"fmt chunk is too short");
        return SOX_EOF;
      }
      if (lsx_readw(ft, &numberOfValidBits) ||
          lsx_readdw(ft, &speakerPositionMask) ||
          lsx_readw(ft, &subFormatTag))
        read_error();
      for (i = 0; i < 14; ++i)
        if (lsx_readb(ft, &dummyByte))
          read_error();
      len -= 22;
      if (numberOfValidBits > wBitsPerSample)
      {
        lsx_fail_errno(ft,SOX_EHDR,"number of valid bits exceeds the number of bits per sample");
        return SOX_EOF;
      }
      wav->formatTag = subFormatTag;
      lsx_report("EXTENSIBLE");
    }

    switch (wav->formatTag)
    {
    case WAVE_FORMAT_UNKNOWN:
        lsx_fail_errno(ft,SOX_EHDR,"file is in unsupported Microsoft Official Unknown format");
        return SOX_EOF;

    case WAVE_FORMAT_PCM:
        /* Default (-1) depends on sample size.  Set that later on. */
        if (ft->encoding.encoding != SOX_ENCODING_UNKNOWN && ft->encoding.encoding != SOX_ENCODING_UNSIGNED &&
            ft->encoding.encoding != SOX_ENCODING_SIGN2)
            lsx_report("user options are overriding the encoding in the file header");
        break;

    case WAVE_FORMAT_ADPCM:
        if (ft->encoding.encoding == SOX_ENCODING_UNKNOWN || ft->encoding.encoding == SOX_ENCODING_MS_ADPCM)
            ft->encoding.encoding = SOX_ENCODING_MS_ADPCM;
        else
            lsx_report("user options are overriding the encoding in the file header");
        break;

    case WAVE_FORMAT_IEEE_FLOAT:
        if (ft->encoding.encoding == SOX_ENCODING_UNKNOWN || ft->encoding.encoding == SOX_ENCODING_FLOAT)
            ft->encoding.encoding = SOX_ENCODING_FLOAT;
        else
            lsx_report("user options are overriding the encoding in the file header");
        break;

    case WAVE_FORMAT_ALAW:
        if (ft->encoding.encoding == SOX_ENCODING_UNKNOWN || ft->encoding.encoding == SOX_ENCODING_ALAW)
            ft->encoding.encoding = SOX_ENCODING_ALAW;
        else
            lsx_report("user options are overriding the encoding in the file header");
        break;

    case WAVE_FORMAT_MULAW:
        if (ft->encoding.encoding == SOX_ENCODING_UNKNOWN || ft->encoding.encoding == SOX_ENCODING_ULAW)
            ft->encoding.encoding = SOX_ENCODING_ULAW;
        else
            lsx_report("user options are overriding the encoding in the file header");
        break;

    case WAVE_FORMAT_IMA_ADPCM:
        if (ft->encoding.encoding == SOX_ENCODING_UNKNOWN || ft->encoding.encoding == SOX_ENCODING_IMA_ADPCM)
            ft->encoding.encoding = SOX_ENCODING_IMA_ADPCM;
        else
            lsx_report("user options are overriding the encoding in the file header");
        break;

    case WAVE_FORMAT_GSM610:
        if (ft->encoding.encoding == SOX_ENCODING_UNKNOWN || ft->encoding.encoding == SOX_ENCODING_GSM )
            ft->encoding.encoding = SOX_ENCODING_GSM;
        else
            lsx_report("user options are overriding the encoding in the file header");
        break;

    default:
        return wavfail(ft);
    }

    /* User options take precedence */
    if (ft->signal.channels == 0 || ft->signal.channels == wChannels)
        ft->signal.channels = wChannels;
    else
        lsx_report("user options are overriding the number of channels in the file header");

    if (ft->signal.channels == 0) {
        lsx_fail_errno(ft, SOX_EHDR, "channel count is zero");
        return SOX_EOF;
    }

    if (ft->signal.rate == 0 || ft->signal.rate == dwSamplesPerSecond)
        ft->signal.rate = dwSamplesPerSecond;
    else
        lsx_report("user options are overriding the rate in the file header");


    wav->lsx_ms_adpcm_i_coefs = NULL;
    wav->packet = NULL;
    wav->samples = NULL;

    /* non-PCM formats except alaw and mulaw formats have extended fmt chunk.
     * Check for those cases.
     */
    if (wav->formatTag != WAVE_FORMAT_PCM &&
        wav->formatTag != WAVE_FORMAT_ALAW &&
        wav->formatTag != WAVE_FORMAT_MULAW) {
        if (len >= 2) {
            if (lsx_readw(ft, &wExtSize))
                read_error();
            len -= 2;
        } else {
            lsx_warn("wave header missing extended part of fmt chunk");
        }
    }

    if (wExtSize > len)
    {
        lsx_fail_errno(ft,SOX_EOF,"header error: wExtSize inconsistent with wFmtLen");
        return SOX_EOF;
    }

    switch (wav->formatTag)
    {
    case WAVE_FORMAT_ADPCM:
        if (wExtSize < 4)
        {
            lsx_fail_errno(ft,SOX_EOF,"format[%s]: expects wExtSize >= %d",
                        wav_format_str(wav->formatTag), 4);
            return SOX_EOF;
        }

        if (wBitsPerSample != 4)
        {
            lsx_fail_errno(ft,SOX_EOF,"can only handle 4-bit MS ADPCM");
            return SOX_EOF;
        }

        if (lsx_readw(ft, &(wav->samplesPerBlock)))
            read_error();
        bytesPerBlock = lsx_ms_adpcm_bytes_per_block((size_t) ft->signal.channels, (size_t) wav->samplesPerBlock);
        if (bytesPerBlock != wav->blockAlign)
        {
            lsx_fail_errno(ft,SOX_EOF,"format[%s]: samplesPerBlock(%d) incompatible with blockAlign(%d)",
                wav_format_str(wav->formatTag), wav->samplesPerBlock, wav->blockAlign);
            return SOX_EOF;
        }

        if (lsx_readw(ft, &(wav->nCoefs)))
            read_error();
        if (wav->nCoefs < 7 || wav->nCoefs > 0x100) {
            lsx_fail_errno(ft,SOX_EOF,"ADPCM file nCoefs (%.4hx) makes no sense", wav->nCoefs);
            return SOX_EOF;
        }
        wav->packet = lsx_malloc((size_t)wav->blockAlign);

        len -= 4;

        if (wExtSize < 4 + 4*wav->nCoefs)
        {
            lsx_fail_errno(ft,SOX_EOF,"header error: wExtSize(%d) too small for nCoefs(%d)", wExtSize, wav->nCoefs);
            return SOX_EOF;
        }

        lsx_valloc(wav->samples, wChannels * wav->samplesPerBlock);

        /* nCoefs, lsx_ms_adpcm_i_coefs used by adpcm.c */
        lsx_valloc(wav->lsx_ms_adpcm_i_coefs, wav->nCoefs * 2);
        wav->ms_adpcm_data = lsx_ms_adpcm_alloc(wChannels);
        {
            int i, errct=0;
            for (i=0; len>=2 && i < 2*wav->nCoefs; i++) {
                if (lsx_readsw(ft, &(wav->lsx_ms_adpcm_i_coefs[i])))
                    read_error();
                len -= 2;
                if (i<14) errct += (wav->lsx_ms_adpcm_i_coefs[i] != lsx_ms_adpcm_i_coef[i/2][i%2]);
                /* lsx_debug("lsx_ms_adpcm_i_coefs[%2d] %4d",i,wav->lsx_ms_adpcm_i_coefs[i]); */
            }
            if (errct) lsx_warn("base lsx_ms_adpcm_i_coefs differ in %d/14 positions",errct);
        }

        bytespersample = 2;  /* AFTER de-compression */
        break;

    case WAVE_FORMAT_IMA_ADPCM:
        if (wExtSize < 2)
        {
            lsx_fail_errno(ft,SOX_EOF,"format[%s]: expects wExtSize >= %d",
                    wav_format_str(wav->formatTag), 2);
            return SOX_EOF;
        }

        if (wBitsPerSample != 4)
        {
            lsx_fail_errno(ft,SOX_EOF,"can only handle 4-bit IMA ADPCM");
            return SOX_EOF;
        }

        if (lsx_readw(ft, &(wav->samplesPerBlock)))
            read_error();
        bytesPerBlock = lsx_ima_bytes_per_block((size_t) ft->signal.channels, (size_t) wav->samplesPerBlock);
        if (bytesPerBlock != wav->blockAlign || wav->samplesPerBlock%8 != 1)
        {
            lsx_fail_errno(ft,SOX_EOF,"format[%s]: samplesPerBlock(%d) incompatible with blockAlign(%d)",
                wav_format_str(wav->formatTag), wav->samplesPerBlock, wav->blockAlign);
            return SOX_EOF;
        }

        wav->packet = lsx_malloc((size_t)wav->blockAlign);
        len -= 2;

        lsx_valloc(wav->samples, wChannels * wav->samplesPerBlock);

        bytespersample = 2;  /* AFTER de-compression */
        break;

    /* GSM formats have extended fmt chunk.  Check for those cases. */
    case WAVE_FORMAT_GSM610:
        if (wExtSize < 2)
        {
            lsx_fail_errno(ft,SOX_EOF,"format[%s]: expects wExtSize >= %d",
                    wav_format_str(wav->formatTag), 2);
            return SOX_EOF;
        }
        if (lsx_readw(ft, &wav->samplesPerBlock))
            read_error();
        bytesPerBlock = 65;
        if (wav->blockAlign != 65)
        {
            lsx_fail_errno(ft,SOX_EOF,"format[%s]: expects blockAlign(%d) = %d",
                    wav_format_str(wav->formatTag), wav->blockAlign, 65);
            return SOX_EOF;
        }
        if (wav->samplesPerBlock != 320)
        {
            lsx_fail_errno(ft,SOX_EOF,"format[%s]: expects samplesPerBlock(%d) = %d",
                    wav_format_str(wav->formatTag), wav->samplesPerBlock, 320);
            return SOX_EOF;
        }
        bytespersample = 2;  /* AFTER de-compression */
        len -= 2;
        break;

    default:
      bytespersample = (wBitsPerSample + 7)/8;

    }

    /* User options take precedence */
    if (!ft->encoding.bits_per_sample || ft->encoding.bits_per_sample == wBitsPerSample)
      ft->encoding.bits_per_sample = wBitsPerSample;
    else
      lsx_warn("user options overriding size in header");

    /* Now we have enough information to set default encodings. */
    switch (bytespersample)
    {
    case 1:
      if (ft->encoding.encoding == SOX_ENCODING_UNKNOWN)
        ft->encoding.encoding = SOX_ENCODING_UNSIGNED;
      break;

    case 2: case 3: case 4:
      if (ft->encoding.encoding == SOX_ENCODING_UNKNOWN)
        ft->encoding.encoding = SOX_ENCODING_SIGN2;
      break;

    case 8:
      if (ft->encoding.encoding == SOX_ENCODING_UNKNOWN)
        ft->encoding.encoding = SOX_ENCODING_FLOAT;
      break;

    default:
      lsx_fail_errno(ft,SOX_EFMT,"don't understand size");
      return SOX_EOF;
    }

    /* Skip anything left over from fmt chunk */
    if (lsx_seeki(ft, (off_t)len, SEEK_CUR))
        read_error();

    /* for non-PCM formats, there's a 'fact' chunk before
     * the upcoming 'data' chunk */

    /* Now look for the wave data chunk */
    if (findChunk(ft, "data", &len) == SOX_EOF)
    {
        lsx_fail_errno(ft, SOX_EOF, "could not find data chunk");
        return SOX_EOF;
    }

    /* ds64 size will have been applied in findChunk */
    qwDataLength = len;
    /* XXX - does MS_UNSPEC apply to RF64 files? */
    if (qwDataLength == 0x7FFFFFFF ||  /* LAME */
        qwDataLength == 0xFFFFFFFF ||  /* FFMPEG and madplay */
        qwDataLength == MS_UNSPEC) {   /* SoX only, apparently */
      wav->ignoreSize = 1;
      /* This is to be expected when reading from a pipe */
      if (ft->seekable)
        lsx_warn("data length is unspecified; taking it from the file length");
    }


    /* Data starts here */
    wav->dataStart = lsx_tell(ft);
    ft->data_start = wav->dataStart;

    switch (wav->formatTag)
    {

    case WAVE_FORMAT_ADPCM:
        wav->numSamples =
            lsx_ms_adpcm_samples_in((size_t)qwDataLength, (size_t)ft->signal.channels,
                           (size_t)wav->blockAlign, (size_t)wav->samplesPerBlock);
        lsx_debug_more("datalen %" PRIu64 ", numSamples %lu",qwDataLength, (unsigned long)wav->numSamples);
        wav->blockSamplesRemaining = 0;        /* Samples left in buffer */
        ft->signal.length = wav->numSamples*ft->signal.channels;
        break;

    case WAVE_FORMAT_IMA_ADPCM:
        /* Compute easiest part of number of samples.  For every block, there
           are samplesPerBlock samples to read. */
        wav->numSamples =
            lsx_ima_samples_in((size_t)qwDataLength, (size_t)ft->signal.channels,
                         (size_t)wav->blockAlign, (size_t)wav->samplesPerBlock);
        lsx_debug_more("datalen %" PRIu64 ", numSamples %lu",qwDataLength, (unsigned long)wav->numSamples);
        wav->blockSamplesRemaining = 0;        /* Samples left in buffer */
        lsx_ima_init_table();
        ft->signal.length = wav->numSamples*ft->signal.channels;
        break;

    case WAVE_FORMAT_GSM610:
        wav->numSamples = ((qwDataLength / wav->blockAlign) * wav->samplesPerBlock);
        wavgsminit(ft);
        ft->signal.length = wav->numSamples*ft->signal.channels;
        break;

    default:
        if (ft->encoding.bits_per_sample == 0)
        {
            lsx_fail_errno(ft, SOX_EHDR, "bits per sample is zero");
            return SOX_EOF;
        }
        wav->numSamples = div_bits(qwDataLength, ft->encoding.bits_per_sample) / ft->signal.channels;
        ft->signal.length = wav->numSamples * ft->signal.channels;
    }
     
    /* When ignoring size, reset length so that output files do
     * not mistakenly depend on it.
     */
    if (wav->ignoreSize)
      ft->signal.length = SOX_UNSPEC;

    lsx_debug("Reading Wave file: %s format, %d channel%s, %d samp/sec",
           wav_format_str(wav->formatTag), ft->signal.channels,
           wChannels == 1 ? "" : "s", dwSamplesPerSecond);
    lsx_debug("        %d byte/sec, %d block align, %d bits/samp, %" PRIu64 " data bytes",
           dwAvgBytesPerSec, wav->blockAlign, wBitsPerSample, qwDataLength);

    /* Can also report extended fmt information */
    switch (wav->formatTag)
    {
        case WAVE_FORMAT_ADPCM:
            lsx_debug("        %d Extsize, %d Samps/block, %lu bytes/block %d Num Coefs, %lu Samps/chan",
                      wExtSize,wav->samplesPerBlock,
                      (unsigned long)bytesPerBlock,wav->nCoefs,
                      (unsigned long)wav->numSamples);
            break;

        case WAVE_FORMAT_IMA_ADPCM:
            lsx_debug("        %d Extsize, %d Samps/block, %lu bytes/block %lu Samps/chan",
                      wExtSize, wav->samplesPerBlock, 
                      (unsigned long)bytesPerBlock,
                      (unsigned long)wav->numSamples);
            break;

        case WAVE_FORMAT_GSM610:
            lsx_debug("GSM .wav: %d Extsize, %d Samps/block, %lu Samples/chan",
                      wExtSize, wav->samplesPerBlock, 
                      (unsigned long)wav->numSamples);
            break;

        default:
            lsx_debug("        %lu Samps/chans", 
                      (unsigned long)wav->numSamples);
    }

    /* Horrible way to find Cool Edit marker points. Taken from Quake source*/
    ft->oob.loops[0].start = SOX_IGNORE_LENGTH;
    if(ft->seekable){
        /*Got this from the quake source.  I think it 32bit aligns the chunks
         * doubt any machine writing Cool Edit Chunks writes them at an odd
         * offset */
        len = (len + 1) & ~1u;
        if (lsx_seeki(ft, (off_t)len, SEEK_CUR) == SOX_SUCCESS &&
            findChunk(ft, "LIST", &len) != SOX_EOF)
        {
            wav->comment = lsx_malloc((size_t)256);
            /* Initialize comment to a NULL string */
            wav->comment[0] = 0;
            while(!lsx_eof(ft))
            {
                if (lsx_reads(ft,magic,(size_t)4) == SOX_EOF)
                    break;

                /* First look for type fields for LIST Chunk and
                 * skip those if found.  Since a LIST is a list
                 * of Chunks, treat the remaining data as Chunks
                 * again.
                 */
                if (strncmp(magic, "INFO", (size_t)4) == 0)
                {
                    /*Skip*/
                    lsx_debug("Type INFO");
                }
                else if (strncmp(magic, "adtl", (size_t)4) == 0)
                {
                    /* Skip */
                    lsx_debug("Type adtl");
                }
                else
                {
                    uint32_t len_tmp;
                    if (lsx_readdw(ft,&len_tmp) == SOX_EOF)
                        read_error();
                    len = len_tmp;
                    if (strncmp(magic,"ICRD",(size_t)4) == 0)
                    {
                        lsx_debug("Chunk ICRD");
                        if (len > 254)
                        {
                            lsx_warn("possible buffer overflow hack attack (ICRD)!");
                            break;
                        }
                        /* Ignore a final ICRD chunk whose length is longer
                         * than the rest of the file
                         */
                        if (lsx_reads(ft,text, (size_t)len))
                        {
                            lsx_warn("truncated ICRD chunk at end of file");
                            break;
			}
                        if (strlen(wav->comment) + strlen(text) < 254)
                        {
                            if (wav->comment[0] != 0)
                                strcat(wav->comment,"\n");

                            strcat(wav->comment,text);
                        }
                        if (strlen(text) < len)
                           if (lsx_seeki(ft, (off_t)(len - strlen(text)), SEEK_CUR))
			    read_error();
                    }
                    else if (strncmp(magic,"ISFT",(size_t)4) == 0)
                    {
                        lsx_debug("Chunk ISFT");
                        if (len > 254)
                        {
                            lsx_warn("possible buffer overflow hack attack (ISFT)!");
                            break;
                        }
                        /* WAV files exist that end with an ISFT chunk
                         * whose length is longer than the rest of the file
                         */
                        if (lsx_reads(ft,text, (size_t)len))
                        {
                            lsx_warn("truncated ISFT chunk at end of file");
                            break;
			}
                        if (strlen(wav->comment) + strlen(text) < 254)
                        {
                            if (wav->comment[0] != 0)
                                strcat(wav->comment,"\n");

                            strcat(wav->comment,text);
                        }
                        if (strlen(text) < len)
                            if (lsx_seeki(ft, (off_t)(len - strlen(text)), SEEK_CUR))
			        read_error();
                    }
                    else if (strncmp(magic,"cue ",(size_t)4) == 0)
                    {
                        lsx_debug("Chunk cue ");
                        if (lsx_seeki(ft,(off_t)(len-4),SEEK_CUR) ||
                            lsx_readdw(ft,&dwLoopPos))
			    read_error();
                        ft->oob.loops[0].start = dwLoopPos;
                    }
                    else if (strncmp(magic,"ltxt",(size_t)4) == 0)
                    {
                        lsx_debug("Chunk ltxt");
                        if (lsx_readdw(ft,&dwLoopPos))
			    read_error();
                        ft->oob.loops[0].length = dwLoopPos - ft->oob.loops[0].start;
                        if (len > 4)
                            if (lsx_seeki(ft, (off_t)(len - 4), SEEK_CUR))
			        read_error();
                    }
                    else
                    {
                        lsx_debug("Attempting to seek beyond unsupported chunk `%c%c%c%c' of length %" PRIu64 " bytes", magic[0], magic[1], magic[2], magic[3], len);
                        len = (len + 1) & ~1u;
                        if (lsx_seeki(ft, (off_t)len, SEEK_CUR))
			    read_error();
                    }
                }
            }
        }
        lsx_clearerr(ft);
        if (lsx_seeki(ft,(off_t)wav->dataStart,SEEK_SET))
	    read_error();
    }
    return lsx_rawstartread(ft);
}


/*
 * Read up to len samples from file.
 * Convert to signed longs.
 * Place in buf[].
 * Return number of samples read.
 */

static size_t read_samples_wav(sox_format_t * ft, sox_sample_t *buf, size_t len)
{
        priv_t *   wav = (priv_t *) ft->priv;
        size_t done;

        ft->sox_errno = SOX_SUCCESS;

        /* If file is in ADPCM encoding then read in multiple blocks else */
        /* read as much as possible and return quickly. */
        switch (ft->encoding.encoding)
        {
        case SOX_ENCODING_IMA_ADPCM:
        case SOX_ENCODING_MS_ADPCM:

            if (!wav->ignoreSize && len > (wav->numSamples*ft->signal.channels))
                len = (wav->numSamples*ft->signal.channels);

            done = 0;
            while (done < len) { /* Still want data? */
                /* See if need to read more from disk */
                if (wav->blockSamplesRemaining == 0) {
                    if (wav->formatTag == WAVE_FORMAT_IMA_ADPCM)
                        wav->blockSamplesRemaining = ImaAdpcmReadBlock(ft);
                    else
                        wav->blockSamplesRemaining = AdpcmReadBlock(ft);
                    if (wav->blockSamplesRemaining == 0)
                    {
                        /* Don't try to read any more samples */
                        wav->numSamples = 0;
                        return done;
                    }
                    wav->samplePtr = wav->samples;
                }

                /* Copy interleaved data into buf, converting to sox_sample_t */
                {
                    short *p, *top;
                    size_t ct;
                    ct = len-done;
                    if (ct > (wav->blockSamplesRemaining*ft->signal.channels))
                        ct = (wav->blockSamplesRemaining*ft->signal.channels);

                    done += ct;
                    wav->blockSamplesRemaining -= (ct/ft->signal.channels);
                    p = wav->samplePtr;
                    top = p+ct;
                    /* Output is already signed */
                    while (p<top)
                        *buf++ = SOX_SIGNED_16BIT_TO_SAMPLE((*p++),);

                    wav->samplePtr = p;
                }
            }
            /* "done" for ADPCM equals total data processed and not
             * total samples procesed.  The only way to take care of that
             * is to return here and not fall thru.
             */
            wav->numSamples -= (done / ft->signal.channels);
            return done;
            break;

        case SOX_ENCODING_GSM:
            if (!wav->ignoreSize && len > wav->numSamples*ft->signal.channels)
                len = (wav->numSamples*ft->signal.channels);

            done = wavgsmread(ft, buf, len);
            if (done == 0 && wav->numSamples != 0 && !wav->ignoreSize)
                lsx_warn("premature EOF on input file");
        break;

        default: /* assume PCM or float encoding */
            if (!wav->ignoreSize && len > wav->numSamples*ft->signal.channels)
                len = (wav->numSamples*ft->signal.channels);

            done = lsx_rawread(ft, buf, len);
            /* If software thinks there are more samples but I/O */
            /* says otherwise, let the user know about this.     */
            if (done == 0 && wav->numSamples != 0 && !wav->ignoreSize)
                lsx_warn("premature EOF on input file");
        }

        /* Only return buffers that contain a totally playable
         * amount of audio.
         */
        done -= done % ft->signal.channels;
        if (done/ft->signal.channels > wav->numSamples)
            wav->numSamples = 0;
        else
            wav->numSamples -= (done/ft->signal.channels);
        return done;
}

/*
 * Do anything required when you stop reading samples.
 * Don't close input file!
 */
static int stopread_wav(sox_format_t * ft)
{
    priv_t *       wav = (priv_t *) ft->priv;

    ft->sox_errno = SOX_SUCCESS;

    free(wav->packet);
    free(wav->samples);
    free(wav->lsx_ms_adpcm_i_coefs);
    free(wav->ms_adpcm_data);
    free(wav->comment);
    wav->comment = NULL;

    switch (ft->encoding.encoding)
    {
    case SOX_ENCODING_GSM:
        wavgsmdestroy(ft);
        break;
    case SOX_ENCODING_IMA_ADPCM:
    case SOX_ENCODING_MS_ADPCM:
        break;
    default:
        break;
    }
    return SOX_SUCCESS;
}

static int startwrite_wav(sox_format_t * ft)
{
    priv_t * wav = (priv_t *) ft->priv;
    int rc;

    ft->sox_errno = SOX_SUCCESS;

    if (ft->encoding.encoding != SOX_ENCODING_MS_ADPCM &&
        ft->encoding.encoding != SOX_ENCODING_IMA_ADPCM &&
        ft->encoding.encoding != SOX_ENCODING_GSM)
    {
        rc = lsx_rawstartwrite(ft);
        if (rc)
            return rc;
    }

    wav->numSamples = 0;
    wav->dataLength = 0;
    wav->warnedSamplesUnspec = sox_false;
    wav->warnedDataUnspec = sox_false;
    wav->firstHeaderDataUnspec = sox_false;
    if (!ft->signal.length && !ft->seekable)
        lsx_warn("length in output header will be wrong since can't seek to fix it");

    rc = wavwritehdr(ft, 0);  /* also calculates various wav->* info */
    if (rc != 0)
        return rc;

    wav->packet = NULL;
    wav->samples = NULL;
    wav->lsx_ms_adpcm_i_coefs = NULL;
    switch (wav->formatTag)
    {
        size_t ch, sbsize;

        case WAVE_FORMAT_IMA_ADPCM:
            lsx_ima_init_table();
        /* intentional case fallthru! */
	    goto wave_format_adpcm;
        case WAVE_FORMAT_ADPCM:
wave_format_adpcm:
            /* #channels already range-checked for overflow in wavwritehdr() */
            for (ch=0; ch<ft->signal.channels; ch++)
                wav->state[ch] = 0;
            sbsize = ft->signal.channels * wav->samplesPerBlock;
            wav->packet = lsx_malloc((size_t)wav->blockAlign);
            lsx_valloc(wav->samples, sbsize);
            wav->sampleTop = wav->samples + sbsize;
            wav->samplePtr = wav->samples;
            break;

        case WAVE_FORMAT_GSM610:
            return wavgsminit(ft);

        default:
            break;
    }
    return SOX_SUCCESS;
}

/* wavwritehdr:  write .wav headers as follows:

bytes      variable      description
0  - 3     'RIFF'/'RIFX' Little/Big-endian
4  - 7     wRiffLength   length of file minus the 8 byte riff header
8  - 11    'WAVE'
12 - 15    'fmt '
16 - 19    wFmtSize       length of format chunk minus 8 byte header
20 - 21    wFormatTag     identifies PCM, ULAW etc
22 - 23    wChannels
24 - 27    dwSamplesPerSecond  samples per second per channel
28 - 31    dwAvgBytesPerSec    non-trivial for compressed formats
32 - 33    wBlockAlign         basic block size
34 - 35    wBitsPerSample      non-trivial for compressed formats

PCM formats then go straight to the data chunk:
36 - 39    'data'
40 - 43     dwDataLength   length of data chunk minus 8 byte header
44 - (dwDataLength + 43)   the data
(+ a padding byte if dwDataLength is odd)

non-PCM formats must write an extended format chunk and a fact chunk:

ULAW, ALAW formats:
36 - 37    wExtSize = 0  the length of the format extension
38 - 41    'fact'
42 - 45    dwFactSize = 4  length of the fact chunk minus 8 byte header
46 - 49    dwSamplesWritten   actual number of samples written out
50 - 53    'data'
54 - 57     dwDataLength  length of data chunk minus 8 byte header
58 - (dwDataLength + 57)  the data
(+ a padding byte if dwDataLength is odd)


GSM6.10  format:
36 - 37    wExtSize = 2 the length in bytes of the format-dependent extension
38 - 39    320           number of samples per  block
40 - 43    'fact'
44 - 47    dwFactSize = 4  length of the fact chunk minus 8 byte header
48 - 51    dwSamplesWritten   actual number of samples written out
52 - 55    'data'
56 - 59     dwDataLength  length of data chunk minus 8 byte header
60 - (dwDataLength + 59)  the data (including a padding byte, if necessary,
                            so dwDataLength is always even)


note that header contains (up to) 3 separate ways of describing the
length of the file, all derived here from the number of (input)
samples wav->numSamples in a way that is non-trivial for the blocked
and padded compressed formats:

wRiffLength -      (riff header) the length of the file, minus 8
dwSamplesWritten - (fact header) the number of samples written (after padding
                   to a complete block eg for GSM)
dwDataLength     - (data chunk header) the number of (valid) data bytes written

*/

static int wavwritehdr(sox_format_t * ft, int second_header)
{
    priv_t *       wav = (priv_t *) ft->priv;

    /* variables written to wav file header */
    /* RIFF header */
    uint32_t wRiffLength ;  /* length of file after 8 byte riff header */
    /* fmt chunk */
    uint16_t wFmtSize = 16;       /* size field of the fmt chunk */
    uint16_t wFormatTag = 0;      /* data format */
    uint16_t wChannels;           /* number of channels */
    uint32_t dwSamplesPerSecond;  /* samples per second per channel*/
    uint32_t dwAvgBytesPerSec=0;  /* estimate of bytes per second needed */
    uint16_t wBlockAlign=0;       /* byte alignment of a basic sample block */
    uint16_t wBitsPerSample=0;    /* bits per sample */
    /* fmt chunk extension (not PCM) */
    uint16_t wExtSize=0;          /* extra bytes in the format extension */
    uint16_t wSamplesPerBlock;    /* samples per channel per block */
    /* wSamplesPerBlock and other things may go into format extension */

    /* fact chunk (not PCM) */
    uint32_t dwFactSize=4;        /* length of the fact chunk */
    uint64_t dwSamplesWritten=0;  /* windows doesnt seem to use this*/

    /* data chunk */
    uint64_t  dwDataLength; /* length of sound data in bytes */
    /* end of variables written to header */

    /* internal variables, intermediate values etc */
    int bytespersample; /* (uncompressed) bytes per sample (per channel) */
    uint64_t blocksWritten = 0;
    uint64_t riffLength64;
    sox_bool isExtensible = sox_false;    /* WAVE_FORMAT_EXTENSIBLE? */
    sox_bool writesFact;

    if (ft->signal.channels > UINT16_MAX) {
        lsx_fail_errno(ft, SOX_EOF, "too many channels (%u)",
                       ft->signal.channels);
        return SOX_EOF;
    }

    dwSamplesPerSecond = ft->signal.rate;
    wChannels = ft->signal.channels;
    wBitsPerSample = ft->encoding.bits_per_sample;
    wSamplesPerBlock = 1;       /* common default for PCM data */

    switch (ft->encoding.encoding)
    {
        case SOX_ENCODING_UNSIGNED:
        case SOX_ENCODING_SIGN2:
            wFormatTag = WAVE_FORMAT_PCM;
            bytespersample = (wBitsPerSample + 7)/8;
            wBlockAlign = wChannels * bytespersample;
            break;
        case SOX_ENCODING_FLOAT:
            wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
            bytespersample = (wBitsPerSample + 7)/8;
            wBlockAlign = wChannels * bytespersample;
            break;
        case SOX_ENCODING_ALAW:
            wFormatTag = WAVE_FORMAT_ALAW;
            wBlockAlign = wChannels;
            break;
        case SOX_ENCODING_ULAW:
            wFormatTag = WAVE_FORMAT_MULAW;
            wBlockAlign = wChannels;
            break;
        case SOX_ENCODING_IMA_ADPCM:
            if (wChannels>16)
            {
                lsx_fail_errno(ft,SOX_EOF,"channels(%d) must be <= 16",wChannels);
                return SOX_EOF;
            }
            wFormatTag = WAVE_FORMAT_IMA_ADPCM;
            wBlockAlign = wChannels * 256; /* reasonable default */
            wBitsPerSample = 4;
            wExtSize = 2;
            wSamplesPerBlock = lsx_ima_samples_in((size_t) 0, (size_t) wChannels, (size_t) wBlockAlign, (size_t) 0);
            break;
        case SOX_ENCODING_MS_ADPCM:
            if (wChannels>16)
            {
                lsx_fail_errno(ft,SOX_EOF,"channels(%d) must be <= 16",wChannels);
                return SOX_EOF;
            }
            wFormatTag = WAVE_FORMAT_ADPCM;
            wBlockAlign = ft->signal.rate / 11008;
            wBlockAlign = max(wBlockAlign, 1) * wChannels * 256;
            wBitsPerSample = 4;
            wExtSize = 4+4*7;      /* Ext fmt data length */
            wSamplesPerBlock = lsx_ms_adpcm_samples_in((size_t) 0, (size_t) wChannels, (size_t) wBlockAlign, (size_t) 0);
            break;
        case SOX_ENCODING_GSM:
            if (wChannels!=1)
            {
                lsx_report("overriding GSM audio from %d channel to 1",wChannels);
                if (!second_header)
                  ft->signal.length /= max(1, ft->signal.channels);
                wChannels = ft->signal.channels = 1;
            }
            wFormatTag = WAVE_FORMAT_GSM610;
            /* dwAvgBytesPerSec = 1625*(dwSamplesPerSecond/8000.)+0.5; */
            wBlockAlign=65;
            wBitsPerSample=0;  /* not representable as int   */
            wExtSize=2;        /* length of format extension */
            wSamplesPerBlock = 320;
            break;
        default:
                break;
    }
    wav->formatTag = wFormatTag;
    wav->blockAlign = wBlockAlign;
    wav->samplesPerBlock = wSamplesPerBlock;

    /* When creating header, use length hint given by input file.  If no
     * hint then write default value.  Also, use default value even
     * on header update if more then 32-bit length needs to be written.
     */
    if (!second_header && !ft->signal.length) {
        /* adjust for blockAlign */
        blocksWritten = MS_UNSPEC/wBlockAlign;
        dwDataLength = blocksWritten * wBlockAlign;
        dwSamplesWritten = blocksWritten * wSamplesPerBlock;
    } else {    /* fixup with real length */
        dwSamplesWritten = 
            second_header? wav->numSamples : ft->signal.length / wChannels;
        blocksWritten = (dwSamplesWritten+wSamplesPerBlock-1)/wSamplesPerBlock;
        dwDataLength = blocksWritten * wBlockAlign;
    }

    if (wFormatTag == WAVE_FORMAT_GSM610)
        if (dwDataLength & 1) dwDataLength++; /* round up to even */

    if (wFormatTag == WAVE_FORMAT_PCM && (wBitsPerSample > 16 || wChannels > 2)
        && strcmp(ft->filetype, "wavpcm")) {
      isExtensible = sox_true;
      wFmtSize += 2 + 22;
    }
    else if (wFormatTag != WAVE_FORMAT_PCM)
        wFmtSize += 2+wExtSize; /* plus ExtData */

    writesFact = isExtensible || wFormatTag != WAVE_FORMAT_PCM;

    /* The fact sample count is independent of the RIFF/data sizes.  Clamp it
     * only when this layout actually writes a fact chunk; PCM has no field to
     * overflow and therefore needs no sample-count advisory. */
    if (writesFact && dwSamplesWritten > UINT32_MAX) {
        if (second_header || !ft->seekable)
            wav_warn_once(&wav->warnedSamplesUnspec,
                "sample count exceeds WAV fact chunk's 32-bit field: "
                "writing unspecified sample count");
        dwSamplesWritten = MS_UNSPEC;
    }

    /* Derive the RIFF ChunkSize in 64 bits.  The data length can still fit in
     * its own 32-bit field while data + container overhead does not; assigning
     * the old expression directly to wRiffLength wrapped in that final window
     * below 4 GiB.  In either overflow case, use SoX's read-to-EOF sentinel for
     * data and recompute an internally consistent RIFF size.  This sacrifices
     * an exact data length for those boundary files, but avoids a contradictory
     * header in which the data chunk extends beyond the declared RIFF extent. */
    riffLength64 = 4u + (8u + wFmtSize) +
        (8u + dwDataLength + dwDataLength % 2u);
    if (writesFact)
        riffLength64 += 8u + dwFactSize;

    if (riffLength64 > UINT32_MAX) {
        if (second_header || !ft->seekable)
            wav_warn_once(&wav->warnedDataUnspec,
                "RIFF or data length exceeds WAV's 32-bit size fields: "
                "writing unspecified data length");
        else
            wav->firstHeaderDataUnspec = sox_true;

        dwDataLength = MS_UNSPEC;
        riffLength64 = 4u + (8u + wFmtSize) +
            (8u + dwDataLength + dwDataLength % 2u);
        if (writesFact)
            riffLength64 += 8u + dwFactSize;
    }

    wRiffLength = (uint32_t)riffLength64;

    /* dwAvgBytesPerSec <-- this is BEFORE compression, isn't it? guess not. */
    /* Round before dividing so that txw's 33333.3 doesn't become
     * 33333 with 66667 byte rate.
     */
    dwAvgBytesPerSec = (wBlockAlign*lrint(ft->signal.rate)) / wSamplesPerBlock;

    /* figured out header info, so write it */

    /* If user specified opposite swap than we think, assume they are
     * asking to write a RIFX file.
     */
    if (ft->encoding.reverse_bytes == MACHINE_IS_LITTLEENDIAN)
    {
        if (!second_header)
            lsx_report("requested to swap bytes so writing RIFX header");
        if (lsx_writes(ft, "RIFX"))
	    write_error();
    }
    else
        if (lsx_writes(ft, "RIFF"))
	    write_error();

    if (lsx_writedw(ft, wRiffLength) ||
        lsx_writes(ft, "WAVE") ||
        lsx_writes(ft, "fmt ") ||
        lsx_writedw(ft, wFmtSize) ||
        lsx_writew(ft, isExtensible ? WAVE_FORMAT_EXTENSIBLE : wFormatTag) ||
        lsx_writew(ft, wChannels) ||
        lsx_writedw(ft, dwSamplesPerSecond) ||
        lsx_writedw(ft, dwAvgBytesPerSec) ||
        lsx_writew(ft, wBlockAlign) ||
        lsx_writew(ft, wBitsPerSample)) /* end info common to all fmts */
	    write_error();

    if (isExtensible) {
      uint32_t dwChannelMask=0;  /* unassigned speaker mapping by default */
      static unsigned char const guids[][14] = {
        {'\x00','\x00','\x00','\x00','\x10','\x00','\x80','\x00','\x00','\xAA','\x00','\x38','\x9B','\x71'},  /* wav */
        {'\x00','\x00','\x21','\x07','\xd3','\x11','\x86','\x44','\xc8','\xc1','\xca','\x00','\x00','\x00'}}; /* amb */

      /* if not amb, assume most likely channel masks from number of channels; not
       * ideal solution, but will make files playable in many/most situations
       */
      if (strcmp(ft->filetype, "amb")) {
        if      (wChannels == 1) dwChannelMask = 0x4;     /* 1 channel (mono) = FC */
        else if (wChannels == 2) dwChannelMask = 0x3;     /* 2 channels (stereo) = FL, FR */
        else if (wChannels == 4) dwChannelMask = 0x33;    /* 4 channels (quad) = FL, FR, BL, BR */
        else if (wChannels == 6) dwChannelMask = 0x3F;    /* 6 channels (5.1) = FL, FR, FC, LF, BL, BR */
        else if (wChannels == 8) dwChannelMask = 0x63F;   /* 8 channels (7.1) = FL, FR, FC, LF, BL, BR, SL, SR */
      }
 
      if (lsx_writew(ft, 22) ||
          lsx_writew(ft, wBitsPerSample) || /* No padding in container */
          lsx_writedw(ft, dwChannelMask) || /* Speaker mapping is something reasonable */
          lsx_writew(ft, wFormatTag) ||
          lsx_writebuf(ft, guids[!strcmp(ft->filetype, "amb")], (size_t)14) != 14)
	      write_error();
    }
    else
    /* if not PCM, we need to write out wExtSize even if wExtSize=0 */
    if (wFormatTag != WAVE_FORMAT_PCM)
        if (lsx_writew(ft,wExtSize))
	    write_error();

    switch (wFormatTag)
    {
        int i;
        case WAVE_FORMAT_IMA_ADPCM:
        if (lsx_writew(ft, wSamplesPerBlock))
	    write_error();
        break;
        case WAVE_FORMAT_ADPCM:
        if (lsx_writew(ft, wSamplesPerBlock) ||
            lsx_writew(ft, 7)) /* nCoefs */
	        write_error();
        for (i=0; i<7; i++) {
            if (lsx_writew(ft, (uint16_t)(lsx_ms_adpcm_i_coef[i][0])) ||
                lsx_writew(ft, (uint16_t)(lsx_ms_adpcm_i_coef[i][1])))
	            write_error();
        }
        break;
        case WAVE_FORMAT_GSM610:
        if (lsx_writew(ft, wSamplesPerBlock))
	    write_error();
        break;
        default:
        break;
    }

    /* Oversized RIFF/data/fact sizes are clamped above, before the serialized
     * 32-bit fields are written, for both first and seek-back headers. */

    /* if not PCM, write the 'fact' chunk */
    if (writesFact){
        if (lsx_writes(ft, "fact") ||
            lsx_writedw(ft,dwFactSize) ||
            lsx_writedw(ft,(uint32_t)dwSamplesWritten))
	        write_error();
    }

    if (lsx_writes(ft, "data") ||
        lsx_writedw(ft, (uint32_t)dwDataLength))     /* data chunk size */
	    write_error();

    if (!second_header) {
        lsx_debug("Writing Wave file: %s format, %d channel%s, %d samp/sec",
                wav_format_str(wFormatTag), wChannels,
                wChannels == 1 ? "" : "s", dwSamplesPerSecond);
        lsx_debug("        %d byte/sec, %d block align, %d bits/samp",
                dwAvgBytesPerSec, wBlockAlign, wBitsPerSample);
    } else {
        lsx_debug("Finished writing Wave file, %u data bytes %lu samples",
                (uint32_t)dwDataLength, (unsigned long)wav->numSamples);
        if (wFormatTag == WAVE_FORMAT_GSM610){
            lsx_debug("GSM6.10 format: %u blocks %u padded samples %u padded data bytes",
                    (uint32_t)blocksWritten, (uint32_t)dwSamplesWritten, (uint32_t)dwDataLength);
            if (wav->gsmbytecount != dwDataLength)
                lsx_warn("help ! internal inconsistency - data_written %u gsmbytecount %lu",
                        (uint32_t)dwDataLength, (unsigned long)wav->gsmbytecount);

        }
    }
    return SOX_SUCCESS;
}

static size_t write_samples_wav(sox_format_t * ft, const sox_sample_t *buf, size_t len)
{
        priv_t *   wav = (priv_t *) ft->priv;
        ptrdiff_t total_len = len;

        ft->sox_errno = SOX_SUCCESS;

        switch (wav->formatTag)
        {
        case WAVE_FORMAT_IMA_ADPCM:
        case WAVE_FORMAT_ADPCM:
            while (len>0) {
                short *p = wav->samplePtr;
                short *top = wav->sampleTop;

                if (top>p+len) top = p+len;
                len -= top-p; /* update residual len */
                while (p < top)
                   *p++ = (*buf++) >> 16;

                wav->samplePtr = p;
                if (p == wav->sampleTop)
                    xxxAdpcmWriteBlock(ft);

            }
            return total_len - len;
            break;

        case WAVE_FORMAT_GSM610:
            len = wavgsmwrite(ft, buf, len);
            wav->numSamples += (len/ft->signal.channels);
            return len;
            break;

        default:
            len = lsx_rawwrite(ft, buf, len);
            wav->numSamples += (len/ft->signal.channels);
            return len;
        }
}

static int stopwrite_wav(sox_format_t * ft)
{
        priv_t *   wav = (priv_t *) ft->priv;

        ft->sox_errno = SOX_SUCCESS;


        /* Call this to flush out any remaining data. */
        switch (wav->formatTag)
        {
        case WAVE_FORMAT_IMA_ADPCM:
        case WAVE_FORMAT_ADPCM:
            if (xxxAdpcmWriteBlock(ft))
		return SOX_EOF;
            break;
        case WAVE_FORMAT_GSM610:
            if (wavgsmstopwrite(ft))
		return SOX_EOF;
            break;
        }

        /* Add a pad byte if the number of data bytes is odd.
           See wavwritehdr() above for the calculation. */
        if (wav->formatTag != WAVE_FORMAT_GSM610)
          lsx_padbytes(ft, (size_t)((wav->numSamples + wav->samplesPerBlock - 1)/wav->samplesPerBlock*wav->blockAlign) % 2);

        free(wav->packet);
        free(wav->samples);
        free(wav->lsx_ms_adpcm_i_coefs);

        /* All samples are already written out. */
        /* If file header needs fixing up, for example it needs the */
        /* the number of samples in a field, seek back and write them here. */
        if (ft->signal.length && wav->numSamples <= 0xffffffff &&
            wav->numSamples == ft->signal.length) {
          /* This fast path leaves the first header in place.  If that header
           * had to use MS_UNSPEC because the RIFF size overflowed while the
           * sample count still fit, emit the advisory here; suppressing first-
           * header warnings previously made this successful path silent. */
          if (wav->firstHeaderDataUnspec)
            wav_warn_once(&wav->warnedDataUnspec,
                "RIFF or data length exceeds WAV's 32-bit size fields: "
                "writing unspecified data length");
          return SOX_SUCCESS;
        }
        if (!ft->seekable)
          return SOX_EOF;

        /* When using open_memstream(), seeking back and closing truncates
         * the buffer to the new offset and fseek(SEEK_END) doesn't work either
         * so remember the actual length, rewrite the header and then seek back
         * to where we were.
         */
        { off_t o = ftell(ft->fp);
          int result;

          if (lsx_seeki(ft, (off_t)0, SEEK_SET) != 0)
          {
                lsx_fail_errno(ft,SOX_EOF,"can't rewind output file to rewrite header");
                return SOX_EOF;
          }
          result = wavwritehdr(ft, 1);

          fseek(ft->fp, o, SEEK_SET);

          return result;
        }
}

/*
 * Return a string corresponding to the wave format type.
 */
static char *wav_format_str(unsigned wFormatTag)
{
        switch (wFormatTag)
        {
                case WAVE_FORMAT_UNKNOWN:
                        return "Microsoft Official Unknown";
                case WAVE_FORMAT_PCM:
                        return "Microsoft PCM";
                case WAVE_FORMAT_ADPCM:
                        return "Microsoft ADPCM";
                case WAVE_FORMAT_IEEE_FLOAT:
                       return "IEEE Float";
                case WAVE_FORMAT_ALAW:
                        return "Microsoft A-law";
                case WAVE_FORMAT_MULAW:
                        return "Microsoft U-law";
                case WAVE_FORMAT_OKI_ADPCM:
                        return "OKI ADPCM format.";
                case WAVE_FORMAT_IMA_ADPCM:
                        return "IMA ADPCM";
                case WAVE_FORMAT_DIGISTD:
                        return "Digistd format.";
                case WAVE_FORMAT_DIGIFIX:
                        return "Digifix format.";
                case WAVE_FORMAT_DOLBY_AC2:
                        return "Dolby AC2";
                case WAVE_FORMAT_GSM610:
                        return "GSM 6.10";
                case WAVE_FORMAT_ROCKWELL_ADPCM:
                        return "Rockwell ADPCM";
                case WAVE_FORMAT_ROCKWELL_DIGITALK:
                        return "Rockwell DIGITALK";
                case WAVE_FORMAT_G721_ADPCM:
                        return "G.721 ADPCM";
                case WAVE_FORMAT_G728_CELP:
                        return "G.728 CELP";
                case WAVE_FORMAT_MPEG:
                        return "MPEG";
                case WAVE_FORMAT_MPEGLAYER3:
                        return "MPEG Layer 3";
                case WAVE_FORMAT_G726_ADPCM:
                        return "G.726 ADPCM";
                case WAVE_FORMAT_G722_ADPCM:
                        return "G.722 ADPCM";
                default:
                        return "Unknown";
        }
}

static int seek_wav(sox_format_t * ft, sox_uint64_t offset)
{
  priv_t *   wav = (priv_t *) ft->priv;

  if (ft->encoding.bits_per_sample & 7)
    lsx_fail_errno(ft, SOX_ENOTSUP, "seeking not supported with this encoding");
  else if (wav->formatTag == WAVE_FORMAT_GSM610) {
    int alignment;
    size_t gsmoff;

    /* rounding bytes to blockAlign so that we
     * don't have to decode partial block. */
    gsmoff = offset * wav->blockAlign / wav->samplesPerBlock +
             wav->blockAlign * ft->signal.channels / 2;
    gsmoff -= gsmoff % (wav->blockAlign * ft->signal.channels);

    ft->sox_errno = lsx_seeki(ft, (off_t)(gsmoff + wav->dataStart), SEEK_SET);
    if (ft->sox_errno == SOX_SUCCESS) {
      /* offset is in samples */
      uint64_t new_offset = offset;
      alignment = offset % wav->samplesPerBlock;
      if (alignment != 0)
          new_offset += (wav->samplesPerBlock - alignment);
      wav->numSamples = ft->signal.length - (new_offset / ft->signal.channels);
    }
  } else {
    double wide_sample = offset - (offset % ft->signal.channels);
    double to_d = wide_sample * ft->encoding.bits_per_sample / 8;
    off_t to = to_d;
    ft->sox_errno = (to != to_d)? SOX_EOF : lsx_seeki(ft, (off_t)wav->dataStart + (off_t)to, SEEK_SET);
    if (ft->sox_errno == SOX_SUCCESS)
      wav->numSamples -= (size_t)wide_sample / ft->signal.channels;
  }

  return ft->sox_errno;
}

LSX_FORMAT_HANDLER(wav)
{
  static char const * const names[] = {"wav", "wavpcm", "amb", NULL};
  static unsigned const write_encodings[] = {
    SOX_ENCODING_SIGN2, 16, 24, 32, 0,
    SOX_ENCODING_UNSIGNED, 8, 0,
    SOX_ENCODING_ULAW, 8, 0,
    SOX_ENCODING_ALAW, 8, 0,
    SOX_ENCODING_GSM, 0,
    SOX_ENCODING_MS_ADPCM, 4, 0,
    SOX_ENCODING_IMA_ADPCM, 4, 0,
    SOX_ENCODING_FLOAT, 32, 64, 0,
    0};
  static sox_format_handler_t const handler = {SOX_LIB_VERSION_CODE,
    "Microsoft audio format", names, SOX_FILE_LIT_END,
    startread_wav, read_samples_wav, stopread_wav,
    startwrite_wav, write_samples_wav, stopwrite_wav,
    seek_wav, write_encodings, NULL, sizeof(priv_t)
  };
  return &handler;
}
