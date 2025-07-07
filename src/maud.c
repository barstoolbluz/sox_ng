/* libSoX MAUD file format handler, by Lutz Vieweg 1993
 *
 * supports: mono and stereo, linear, a-law and u-law reading and writing
 *
 * an IFF format; description at http://lclevy.free.fr/amiga/MAUDINFO.TXT
 *
 * Copyright 1998-2006 Chris Bagwell and SoX Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained.
 * Lance Norskog And Sundry Contributors are not responsible for
 * the consequences of using this software.
 */

#include "sox_i.h"

/* Private data for MAUD file */
/* Writing files, the size is always rounded up to a multiple of 2
 * but when reading them with lsx_rawread(), this returns one sample more
 * than is right from a mono file with an odd number of samples
 * so we check the number of samples we have read and stop when we have
 * had enough.
 */
typedef struct {
        uint32_t nsamples;
        uint32_t nread;
} priv_t;

static int maudwriteheader(sox_format_t *);

/*
 * Do anything required before you start reading samples.
 * Read file header.
 *      Find out sampling rate,
 *      size and encoding of samples,
 *      mono/stereo/quad.
 */
static int startread(sox_format_t * ft)
{
        priv_t * p = (priv_t *) ft->priv;

        char buf[12];
        char *chunk_buf;

        unsigned short bitpersam;
        uint32_t nom;
        unsigned short denom;
        unsigned short chaninf;

        uint32_t chunksize;
        uint32_t trash32;
        uint16_t trash16;
        int rc;

	static const char read_error_msg[] = "file is truncated in %.4s chunk";
#define read_error() { \
	lsx_fail_errno(ft, EOF, read_error_msg, buf); \
	return (SOX_EOF); \
}

        /* Needed for rawread() */
        rc = lsx_rawstartread(ft);
        if (rc)
            return rc;

        /* read FORM chunk */
        if (lsx_reads(ft, buf, (size_t)4) == SOX_EOF)
	    read_error();
	if (strncmp(buf, "FORM", (size_t)4) != 0)
        {
                lsx_fail_errno(ft,SOX_EHDR,"header does not begin with magic word `FORM'");
                return (SOX_EOF);
        }

        if (lsx_readdw(ft, &trash32) || /* totalsize */
            lsx_reads(ft, buf, (size_t)4))
		read_error();
	if (strncmp(buf, "MAUD", (size_t)4) != 0)
        {
                lsx_fail_errno(ft,SOX_EHDR,"`FORM' chunk does not specify `MAUD' as type");
                return(SOX_EOF);
        }

        /* read chunks until 'BODY' (or end) */

        while (lsx_reads(ft, buf, (size_t)4) == SOX_SUCCESS && strncmp(buf,"MDAT",(size_t)4) != 0) {

                /*
                buf[4] = 0;
                lsx_debug("chunk %s",buf);
                */

                if (strncmp(buf,"MHDR",(size_t)4) == 0) {

                        if (lsx_readdw(ft, &chunksize))
			    read_error();
                        if (chunksize != 8*4)
                        {
                            lsx_fail_errno(ft,SOX_EHDR,"MHDR chunk has bad size");
                            return(SOX_EOF);
                        }

                        /* number of samples stored in MDAT */
                        if (lsx_readdw(ft, &(p->nsamples)) ||
                            /* number of bits per sample as stored in MDAT */
                            lsx_readw(ft, &bitpersam) ||
                            /* number of bits per sample after decompression */
                            lsx_readw(ft, &trash16) ||
			    /* clock source frequency */
                            lsx_readdw(ft, &nom) ||
			    /* clock divide */
                            lsx_readw(ft, &denom))
			    read_error();
                        if (denom == 0)
                        {
                            lsx_fail_errno(ft,SOX_EHDR,"frequency denominator == 0, failed");
                            return (SOX_EOF);
                        }

                        ft->signal.rate = nom / denom;

                        if (lsx_readw(ft, &chaninf)) /* channel information */
			    read_error();
                        switch (chaninf) {
                        case 0:
                                ft->signal.channels = 1;
                                break;
                        case 1:
                                ft->signal.channels = 2;
                                break;
                        default:
                                lsx_fail_errno(ft,SOX_EFMT,"unsupported number of channels in file");
                                return (SOX_EOF);
                        }

			/* number of channels (mono: 1, stereo: 2, ...) */
                        if (lsx_readw(ft, &chaninf))
			        return(SOX_EOF);
                        if (chaninf != ft->signal.channels)
                        {
                                lsx_fail_errno(ft,SOX_EFMT,"unsupported number of channels in file");
                                return(SOX_EOF);
                        }

                        if (lsx_readw(ft, &chaninf) || /* compression type */
                            lsx_readdw(ft, &trash32) || /* rest of chunk, unused yet */
                            lsx_readdw(ft, &trash32) ||
                            lsx_readdw(ft, &trash32))
			        return(SOX_EOF);

                        if (bitpersam == 8 && chaninf == 0) {
                                ft->encoding.bits_per_sample = 8;
                                ft->encoding.encoding = SOX_ENCODING_UNSIGNED;
                        }
                        else if (bitpersam == 8 && chaninf == 2) {
                                ft->encoding.bits_per_sample = 8;
                                ft->encoding.encoding = SOX_ENCODING_ALAW;
                        }
                        else if (bitpersam == 8 && chaninf == 3) {
                                ft->encoding.bits_per_sample = 8;
                                ft->encoding.encoding = SOX_ENCODING_ULAW;
                        }
                        else if (bitpersam == 16 && chaninf == 0) {
                                ft->encoding.bits_per_sample = 16;
                                ft->encoding.encoding = SOX_ENCODING_SIGN2;
                        }
                        else
                        {
                                lsx_fail_errno(ft,SOX_EFMT,"unsupported compression type detected");
                                return(SOX_EOF);
                        }

                        continue;
                }

                if (strncmp(buf,"ANNO",(size_t)4) == 0) {
                        if (lsx_readdw(ft, &chunksize))
			        return(SOX_EOF);
                        if (chunksize & 1)
                                chunksize++;
                        chunk_buf = lsx_malloc(chunksize + (size_t)1);
                        if (lsx_readbuf(ft, chunk_buf, (size_t)chunksize)
                            != chunksize)
                        {
                                lsx_fail_errno(ft,SOX_EOF,"unexpected EOF in ANNO header");
                                return(SOX_EOF);
                        }
                        chunk_buf[chunksize] = '\0';
                        lsx_debug("%s",chunk_buf);
                        free(chunk_buf);

                        continue;
                }

                /* some other kind of chunk */
                if (lsx_readdw(ft, &chunksize))
			return(SOX_EOF);
                if (chunksize & 1)
                        chunksize++;
                if (lsx_seeki(ft, (off_t)chunksize, SEEK_CUR))
			return(SOX_EOF);
                continue;

        }

        if (strncmp(buf,"MDAT",(size_t)4) != 0)
        {
            lsx_fail_errno(ft,SOX_EFMT,"MDAT chunk not found");
            return(SOX_EOF);
        }
        if (lsx_readdw(ft, &(p->nsamples)))
            return(SOX_EOF);
	p->nread = 0;
        return(SOX_SUCCESS);
}

static size_t read_samples(sox_format_t * ft, sox_sample_t * buf, size_t nsamp)
{
    priv_t * p = (priv_t *) ft->priv;
    uint32_t n_to_read;
    uint32_t nread;

    if (p->nsamples == 0x7f000000)
        /* Read partially-written files to the end */
        n_to_read = nsamp;
    else
        n_to_read = min(nsamp, p->nsamples - p->nread);
    nread = lsx_rawread(ft, buf, n_to_read);
    p->nread += nread;
    return nread;
}

static int startwrite(sox_format_t * ft)
{
        priv_t * p = (priv_t *) ft->priv;
        int rc;
	int status;

        /* Needed for rawwrite() */
        rc = lsx_rawstartwrite(ft);
        if (rc)
            return rc;

        /* If you have to seek around the output file */
        if (! ft->seekable)
        {
            lsx_fail_errno(ft,SOX_EOF,"output must be a file, not a pipe");
            return (SOX_EOF);
        }
        p->nsamples = 0x7f000000;
        status = maudwriteheader(ft);
        p->nsamples = 0;
        return (status);
}

static size_t write_samples(sox_format_t * ft, const sox_sample_t *buf, size_t len)
{
        priv_t * p = (priv_t *) ft->priv;

        p->nsamples += len;

        return lsx_rawwrite(ft, buf, len);
}

static int stopwrite(sox_format_t * ft)
{
        /* All samples are already written out. */

        priv_t *p = (priv_t*)ft->priv;
        uint32_t mdat_size; /* MDAT chunk size */
        mdat_size = p->nsamples * (ft->encoding.bits_per_sample >> 3);
        lsx_padbytes(ft, (size_t) (mdat_size%2));

        if (lsx_seeki(ft, (off_t)0, 0) != 0)
        {
            lsx_fail_errno(ft,errno,"can't rewind output file to rewrite header");
            return(SOX_EOF);
        }

        return(maudwriteheader(ft));
}

static const char write_error_msg[] = "write error";
#define write_error() { \
	lsx_fail_errno(ft, SOX_EOF, write_error_msg); \
	return SOX_EOF; \
}

#define MAUDHEADERSIZE (4+(4+4+32)+(4+4+19+1)+(4+4))
static int maudwriteheader(sox_format_t * ft)
{
        priv_t * p = (priv_t *) ft->priv;
        uint32_t mdat_size; /* MDAT chunk size */

        mdat_size = p->nsamples * (ft->encoding.bits_per_sample >> 3);

        if (lsx_writes(ft, "FORM") ||
            lsx_writedw(ft, MAUDHEADERSIZE + mdat_size + mdat_size%2) ||  /* size of file */
            lsx_writes(ft, "MAUD") ||     /* File type */

            lsx_writes(ft, "MHDR") ||
            lsx_writedw(ft,  8*4) ||      /* number of bytes to follow */
            lsx_writedw(ft, p->nsamples)) /* number of samples stored in MDAT */
	  write_error();

        switch (ft->encoding.encoding) {

        case SOX_ENCODING_UNSIGNED:
          if (lsx_writew(ft, 8) || /* number of bits per sample as stored in MDAT */
              lsx_writew(ft, 8)) /* number of bits per sample after decompression */
	    write_error();
          break;

        case SOX_ENCODING_SIGN2:
          if (lsx_writew(ft, 16) || /* number of bits per sample as stored in MDAT */
              lsx_writew(ft, 16)) /* number of bits per sample after decompression */
	    write_error();
          break;

        case SOX_ENCODING_ALAW:
        case SOX_ENCODING_ULAW:
          if (lsx_writew(ft, 8) || /* number of bits per sample as stored in MDAT */
              lsx_writew(ft, 16)) /* number of bits per sample after decompression */
	    write_error();
          break;

        default:
          break;
        }

        if (lsx_writedw(ft, (unsigned)(ft->signal.rate + .5)) || /* sample rate, Hz */
            lsx_writew(ft, (int) 1)) /* clock devide */
	  write_error();

        if (ft->signal.channels == 1) {
          if (lsx_writew(ft, 0) || /* channel information */
              lsx_writew(ft, 1)) /* number of channels (mono: 1, stereo: 2, ...) */
	    write_error();
        }
        else {
          if (lsx_writew(ft, 1) ||
              lsx_writew(ft, 2))
	    write_error();
        }

        switch (ft->encoding.encoding) {

        case SOX_ENCODING_UNSIGNED:
        case SOX_ENCODING_SIGN2:
          if (lsx_writew(ft, 0)) /* no compression */
	    write_error();
          break;

        case SOX_ENCODING_ULAW:
          if (lsx_writew(ft, 3))
	    write_error();
          break;

        case SOX_ENCODING_ALAW:
          if (lsx_writew(ft, 2))
	    write_error();
          break;

        default:
          break;
        }

        if (lsx_writedw(ft, 0) || /* reserved */
            lsx_writedw(ft, 0) || /* reserved */
            lsx_writedw(ft, 0) || /* reserved */

            lsx_writes(ft, "ANNO") ||
            lsx_writedw(ft, 19) || /* length of block */
            lsx_writes(ft, "file created by SoX") ||
            lsx_padbytes(ft, (size_t)1) ||

            lsx_writes(ft, "MDAT") ||
	    /* samples in file */
            lsx_writedw(ft, p->nsamples * (ft->encoding.bits_per_sample >> 3)))
	  write_error();

        return SOX_SUCCESS;
}

LSX_FORMAT_HANDLER(maud)
{
  static char const * const names[] = {"maud", NULL};
  static unsigned const write_encodings[] = {
    SOX_ENCODING_SIGN2, 16, 0,
    SOX_ENCODING_UNSIGNED, 8, 0,
    SOX_ENCODING_ULAW, 8, 0,
    SOX_ENCODING_ALAW, 8, 0,
    0};
  static sox_format_handler_t const handler = {SOX_LIB_VERSION_CODE,
    "Used with the ‘Toccata’ sound-card on the Amiga",
    names, SOX_FILE_BIG_END | SOX_FILE_MONO | SOX_FILE_STEREO,
    startread, read_samples, lsx_rawstopread,
    startwrite, write_samples, stopwrite,
    NULL, write_encodings, NULL, sizeof(priv_t)
  };
  return &handler;
}
