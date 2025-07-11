/* Amiga 8SVX format handler
 *
 * Copyright (C) February 1992 W V Neisius
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

#include "sox_i.h"

#define BUFLEN 512

/* Private data used by writer */
typedef struct{
  uint32_t nsamples;
  uint32_t left;
  off_t ch0_pos;
  sox_uint8_t buf[4][BUFLEN];
  FILE* tmp[4];
} priv_t;

static int svxwriteheader(sox_format_t *, size_t);

/*======================================================================*/
/*                         8SVXSTARTREAD                                */
/*======================================================================*/

static int startread(sox_format_t * ft)
{
        priv_t * p = (priv_t * ) ft->priv;

        char buf[12];
        char *chunk_buf;

        uint32_t totalsize;
        uint32_t chunksize;

        uint32_t channels;
        unsigned short rate;

        static const char read_error_msg[] = "file is truncated in %.4s chunk";
#define read_error() { \
        lsx_fail_errno(ft, SOX_EOF, read_error_msg, buf); \
        return(SOX_EOF); \
}

        if (! ft->seekable)
        {
                lsx_fail_errno(ft,SOX_EINVAL,"input file must be a file, not a pipe");
                return (SOX_EOF);
        }
        rate = 0;
        channels = 1;

        /* read FORM chunk */
        if (lsx_reads(ft, buf, (size_t)4))
                read_error();
        if (strncmp(buf, "FORM", (size_t)4) != 0)
        {
                lsx_fail_errno(ft, SOX_EHDR, "header did not begin with magic word `FORM'");
                return(SOX_EOF);
        }
        if (lsx_readdw(ft, &totalsize) || lsx_reads(ft, buf, (size_t)4))
                read_error();
        if (strncmp(buf, "8SVX", (size_t)4) != 0) {
                lsx_fail_errno(ft, SOX_EHDR, "'FORM' chunk does not specify `8SVX' as type");
                return(SOX_EOF);
        }

        /* read chunks until 'BODY' (or end) */
        while (lsx_reads(ft, buf, (size_t)4) == SOX_SUCCESS &&
               strncmp(buf,"BODY",(size_t)4) != 0) {
                if (strncmp(buf,"VHDR",(size_t)4) == 0) {
                        if (lsx_readdw(ft, &chunksize))
                                read_error();
                        if (chunksize != 20)
                        {
                                lsx_fail_errno(ft, SOX_EHDR, "VHDR chunk has bad size");
                                return(SOX_EOF);
                        }
                        if (lsx_seeki(ft,(off_t)12,SEEK_CUR) ||
                            lsx_readw(ft, &rate) ||
                            lsx_seeki(ft,(off_t)1,SEEK_CUR) ||
                            lsx_readbuf(ft, buf,(size_t)1) != 1)
                                read_error();
                        if (buf[0] != 0)
                        {
                                lsx_fail_errno(ft, SOX_EFMT, "unsupported data compression");
                                return(SOX_EOF);
                        }
                        if (lsx_seeki(ft,(off_t)4,SEEK_CUR))
                                read_error();
                        continue;
                }

                if (strncmp(buf,"ANNO",(size_t)4) == 0) {
                        if (lsx_readdw(ft, &chunksize))
                                read_error();
                        if (chunksize & 1)
                                chunksize++;
                        chunk_buf = lsx_malloc(chunksize + (size_t)2);
                        if (lsx_readbuf(ft, chunk_buf,(size_t)chunksize)
                                        != chunksize)
                                read_error();
                        chunk_buf[chunksize] = '\0';
                        lsx_debug("%s",chunk_buf);
                        free(chunk_buf);

                        continue;
                }

                if (strncmp(buf,"NAME",(size_t)4) == 0) {
                        if (lsx_readdw(ft, &chunksize))
                                read_error();
                        if (chunksize & 1)
                                chunksize++;
                        chunk_buf = lsx_malloc(chunksize + (size_t)1);
                        if (lsx_readbuf(ft, chunk_buf,(size_t)chunksize)
                                        != chunksize)
                                read_error();
                        chunk_buf[chunksize] = '\0';
                        lsx_debug("%s",chunk_buf);
                        free(chunk_buf);

                        continue;
                }

                if (strncmp(buf,"CHAN",(size_t)4) == 0) {
                        if (lsx_readdw(ft, &chunksize))
                                read_error();
                        if (chunksize != 4)
                        {
                                lsx_fail_errno(ft, SOX_EHDR, "CHAN chunk size is not 4");
                                return(SOX_EOF);
                        }
                        if (lsx_readdw(ft, &channels))
                                read_error();
                        channels = (channels & 0x01) +
                                        ((channels & 0x02) >> 1) +
                                        ((channels & 0x04) >> 2) +
                                        ((channels & 0x08) >> 3);

                        continue;
                }

                /* some other kind of chunk */
                if (lsx_readdw(ft, &chunksize))
                        read_error();
                if (chunksize & 1)
                        chunksize++;
                if (lsx_seeki(ft,(off_t)chunksize,SEEK_CUR))
                        read_error();
                continue;

        }

        if (rate == 0)
        {
                lsx_fail_errno(ft, SOX_EHDR, "invalid sample rate");
                return(SOX_EOF);
        }
        if (strncmp(buf,"BODY",(size_t)4) != 0)
        {
                lsx_fail_errno(ft, SOX_EHDR, "BODY chunk not found");
                return(SOX_EOF);
        }
        if (lsx_readdw(ft, &(p->nsamples)))
                read_error();
        p->left = p->nsamples;
        p->ch0_pos = lsx_tell(ft);

        ft->signal.length = p->nsamples;
        ft->signal.channels = channels;
        ft->signal.rate = rate;
        ft->encoding.encoding = SOX_ENCODING_SIGN2;
        ft->encoding.bits_per_sample = 8;

        return(SOX_SUCCESS);
}

/*======================================================================*/
/*                         8SVXREAD                                     */
/*======================================================================*/
static size_t read_samples(sox_format_t * ft, sox_sample_t *buf, size_t nsamp)
{
    size_t done = 0;

    priv_t * p = (priv_t * ) ft->priv;
    size_t frames = nsamp / ft->signal.channels;
    unsigned width = p->nsamples / ft->signal.channels;

    if (p->left < frames)
        frames = p->left;

    while (done != frames) {
        size_t chunk = frames - done;
        size_t i;
        unsigned ch;

        if (chunk > BUFLEN)
            chunk = BUFLEN;

        for (ch = 0; ch != ft->signal.channels; ch++) {
            if (lsx_seeki(ft, p->ch0_pos + ch * width, SEEK_SET) ||
                chunk != lsx_readbuf(ft, p->buf[ch], chunk))
                return done * ft->signal.channels;
        }

        for (i = 0; i != chunk; i++) {
            for (ch = 0; ch != ft->signal.channels; ch++) {
                /* scale signed up to long's range */
                *buf++ = SOX_SIGNED_8BIT_TO_SAMPLE(p->buf[ch][i], dummy);
            }
        }

        done += chunk;
        p->left -= chunk * ft->signal.channels;
        p->ch0_pos += chunk;
    }
    return done * ft->signal.channels;
}

/*======================================================================*/
/*                         8SVXSTARTWRITE                               */
/*======================================================================*/
static int startwrite(sox_format_t * ft)
{
        priv_t * p = (priv_t * ) ft->priv;
        size_t i;

        /* open channel output files */
        for (i = 0; i < ft->signal.channels; i++) {
                if ((p->tmp[i] = lsx_tmpfile()) == NULL)
                {
                        lsx_fail_errno(ft,errno,"can't open channel output file");
                        return(SOX_EOF);
                }
        }

        p->nsamples = 0;
        return(SOX_SUCCESS);
}

/*======================================================================*/
/*                         8SVXWRITE                                    */
/*======================================================================*/

static size_t write_samples(sox_format_t * ft, const sox_sample_t *buf, size_t len)
{
        priv_t * p = (priv_t * ) ft->priv;
        SOX_SAMPLE_LOCALS;

        unsigned char datum;
        size_t done = 0, i;

        p->nsamples += len;

        while(done < len) {
                for (i = 0; i < ft->signal.channels; i++) {
                        datum = SOX_SAMPLE_TO_SIGNED_8BIT(*buf++, ft->clips);
                        putc(datum, p->tmp[i]);
                }
                done += ft->signal.channels;
        }
        return (done);
}

/*======================================================================*/
/*                         8SVXSTOPWRITE                                */
/*======================================================================*/

static int stopwrite(sox_format_t * ft)
{
        priv_t * p = (priv_t * ) ft->priv;

        size_t i, len;
        char svxbuf[512];

        if (svxwriteheader(ft, (size_t) p->nsamples))
            return SOX_EOF;

        /* append all channel pieces to channel 0 */
        /* close temp files */
        for (i = 0; i < ft->signal.channels; i++) {
                if (fseeko(p->tmp[i], (off_t)0, 0))
                {
                        lsx_fail_errno (ft,errno,"can't rewind channel output file %lu",(unsigned long)i);
                        return(SOX_EOF);
                }
                while (!feof(p->tmp[i])) {
                        len = fread(svxbuf, (size_t) 1, (size_t) 512, p->tmp[i]);
                        if (lsx_writebuf(ft, svxbuf, len) != len) {
                          lsx_fail_errno (ft,errno,"can't write channel output file %lu",(unsigned long)i);
                          return SOX_EOF;
                        }
                }
                fclose (p->tmp[i]);
        }

        /* add a pad byte if BODY size is odd */
        if(p->nsamples % 2 != 0)
            if (lsx_writeb(ft, '\0'))
                return SOX_EOF;

        return(SOX_SUCCESS);
}

/*======================================================================*/
/*                         8SVXWRITEHEADER                              */
/*======================================================================*/
#define SVXHEADERSIZE 100
static int svxwriteheader(sox_format_t * ft, size_t nsamples)
{
        size_t formsize =  nsamples + SVXHEADERSIZE - 8;

        /* FORM size must be even */
        if(formsize % 2 != 0) formsize++;

        if (lsx_writes(ft, "FORM") ||
            lsx_writedw(ft, (unsigned) formsize) ||  /* size of file */
            lsx_writes(ft, "8SVX") || /* File type */

            lsx_writes(ft, "VHDR") ||
            lsx_writedw(ft, 20) || /* number of bytes to follow */
            lsx_writedw(ft, (unsigned) nsamples/ft->signal.channels) ||  /* samples, 1-shot */
            lsx_writedw(ft, 0) ||  /* samples, repeat */
            lsx_writedw(ft, 0) ||  /* samples per repeat cycle */
            lsx_writew(ft, min(65535, (unsigned)(ft->signal.rate + .5))) ||
            lsx_writeb(ft,1) || /* number of octabes */
            lsx_writeb(ft,0) || /* data compression (none) */
            lsx_writew(ft,1) || lsx_writew(ft,0) || /* volume */

            lsx_writes(ft, "ANNO") ||
            lsx_writedw(ft, 32) || /* length of block */
            lsx_writes(ft, "File created by Sound Exchange  ") ||

            lsx_writes(ft, "CHAN") ||
            lsx_writedw(ft, 4) ||
            lsx_writedw(ft, (ft->signal.channels == 2) ? 6u :
                       (ft->signal.channels == 4) ? 15u : 2u) ||

            lsx_writes(ft, "BODY") ||
            lsx_writedw(ft, (unsigned) nsamples)) /* samples in file */
                return SOX_EOF;

        return SOX_SUCCESS;
}

LSX_FORMAT_HANDLER(svx)
{
  static char const * const names[] = {"8svx", NULL};
  static unsigned const write_encodings[] = {SOX_ENCODING_SIGN2, 8, 0, 0};
  static sox_format_handler_t const handler = {SOX_LIB_VERSION_CODE,
    "Amiga audio format (a subformat of the Interchange File Format)",
    names, SOX_FILE_BIG_END|SOX_FILE_MONO|SOX_FILE_STEREO|SOX_FILE_QUAD,
    startread, read_samples, NULL,
    startwrite, write_samples, stopwrite,
    NULL, write_encodings, NULL, sizeof(priv_t)
  };
  return &handler;
}
