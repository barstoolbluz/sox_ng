/* Pulse Audio sound handler
 *
 * Copyright (C) 2008 Chris Bagwell (cbagwell@sprynet.com)
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

#include <pulse/simple.h>
#include <pulse/error.h>
#include <pulse/timeval.h>

#define pa_memzero(x,l) (memset((x), 0, (l)))
#define pa_zero(x) (pa_memzero(&(x), sizeof(x)))

typedef struct {
  pa_simple *pasp;
} priv_t;

/* The filename supplied to -t pulseaudio can be a space-separated list of
 * servers, an application name and/or a device name.
 * An application name is "app:whatever", a server name is anything else
 * that contains a colon and a device name doesn't contain a colon.
 *
 * Pointers to mallocked memory are stored in *server, *dev and *name
 * if those elements are present and the caller should free them if so;
 * The caller should set them to NULL beforehand.
 *
 * Returns SOX_SUCCESS or SOX_EOF.
 */
static int parse_pulse_filename(char *filename,
                                 char *volatile*volatile server, char **dev, char **name)
{
  char *argp;  /* Pointer into filename */
  char *arg;   /* An argument we are constructing */

  /* Split on spaces and switch on type of each element */
  argp = filename;
  while (*argp) {
    char *spacep;

    /* Skip initial spaces to eliminate multiple ones */
    while (*argp == ' ') argp++;
    if (!*argp) break; /* All done */

    /* App names can contain spaces and need to be last */
    if (!strncmp(argp, "app:", 4)) {
      if (*name) {
        lsx_fail("multiple application names in %s", filename);
        return SOX_EOF;
      }
      if (argp[4] == '\0') {
        /* "app:" means default; *name is already NULL */
        return SOX_SUCCESS;
      }
      *name = lsx_malloc(strlen(argp) - 4 + 1);
      strcpy(*name, argp + 4);
      return SOX_SUCCESS;
    }

    spacep = strchr(argp, ' ');
    if (spacep) {
      int len = spacep - argp;
      arg = lsx_malloc(len + 1);
      memcpy(arg, argp, len);
      arg[len] = '\0';
    } else {
      arg = lsx_malloc(strlen(argp) + 1);
      strcpy(arg, argp);
    }

    /* Now that arg is mallocked, it should either be assigned to
     * one of the return value pointers or freed.
     */

    /* Switch on server name/device name */
    if (!strncmp(arg, "app:", 4) && arg[4] != '\0') {
      if (*name) {
        lsx_fail("multiple application names in %s", filename);
        free(arg);
        return SOX_EOF;
      }
      *name = arg;
    } else if (strchr(arg, ':')) {
      /* A server name to set or append */
      if (*server == NULL)
        *server = arg;
      else {
        size_t oldlen = strlen(*server);
        *server = lsx_realloc(*server, oldlen + 1 + strlen(arg) + 1);
        (*server)[oldlen] = ' ';
        strcpy((*server) + oldlen + 1, arg);
        free(arg);
      }
    } else {
      /* No colon: a device name */
      if (*dev) {
        lsx_fail("multiple device names in %s", filename);
        free(arg);
        return SOX_EOF;
      }
      *dev = arg;
    }
    if (spacep) argp=spacep + 1;
    else break;
  }
  return SOX_SUCCESS;
}

static int setup(sox_format_t *ft, int is_input)
{
  priv_t *pa = (priv_t *)ft->priv;
  pa_stream_direction_t dir;
  char *server = NULL;  /* PulseAudio server list */
  char *dev = NULL;     /* PulseAudio device to use */
  char *name = NULL;    /* To set the PA application name */
  char *app_str;        /* The PA stream name, "record" or "playback" */
  pa_sample_spec spec;
  pa_channel_map map;
  pa_buffer_attr buffer_attr;
  int error;

  /* TODO: If user specified device of type "server:dev" then
   * break up and override server.
   */

  if (is_input)
  {
    dir = PA_STREAM_RECORD;
    app_str = "record";
  }
  else
  {
    dir = PA_STREAM_PLAYBACK;
    app_str = "playback";
  }

  if (parse_pulse_filename(ft->filename, &server, &dev, &name) != SOX_SUCCESS) {
    free(server); free(dev); free(name);
    return SOX_EOF;
  }

  if (dev && strncmp(dev, "default", (size_t)7) == 0) {
    free(dev);
    dev = NULL;
  }
  if (server && strncmp(server, "default", (size_t)7) == 0) {
    free(server);
    server = NULL;
  }

  /* If user doesn't specify, default to some reasonable values.
   * Since this is mainly for recording case, default to typical
   * 16-bit values to prevent saving larger files then average user
   * wants.  Power users can override to 32-bit if they wish.
   */
  if (ft->signal.channels == 0)
    ft->signal.channels = 2;
  if (ft->signal.rate == 0)
    ft->signal.rate = 44100;
  if (ft->encoding.bits_per_sample == 0)
  {
    ft->encoding.bits_per_sample = 16;
    ft->encoding.encoding = SOX_ENCODING_SIGN2;
  }

  spec.format = PA_SAMPLE_S32NE;
  spec.rate = ft->signal.rate;
  spec.channels = ft->signal.channels;

  /* Pulseaudio will introduce a 250ms buffer if no buffer_attr is set
   * https://github.com/pulseaudio/pulseaudio/blob/master/src/pulse/stream.c
   * unless the PULSE_LATENCY_MSEC environment variable is set.
   * Here we override this based on --input-buffer / --buffer
   * command-line arguments.
   */
  pa_zero(buffer_attr);
  buffer_attr.maxlength = (uint32_t) -1;
  buffer_attr.prebuf = (uint32_t) -1;
  if (is_input) {
    /* If the tlength/fragsize is not set, pulseaudio src/pulse/stream.c will
       attempt to use getenv("PULSE_LATENCY_MSEC") and fallback on 250ms what
       is likely to happen for the default pulseaudio input (--input-buffer defaults to 0)
       ToDo: Add a pacat/parec-like --latency-msec option?
    */
    buffer_attr.fragsize = sox_globals.input_bufsiz ? sox_globals.input_bufsiz : (uint32_t) -1;
    lsx_debug("INPUT cmd buffer size=%" PRIu64 ", pulseaudio buffer size=%u", (uint64_t)sox_globals.input_bufsiz, buffer_attr.fragsize);
  } else {
    buffer_attr.tlength = sox_globals.bufsiz ? sox_globals.bufsiz : (uint32_t) -1;
    lsx_debug("OUTPUT cmd buffer size=%" PRIu64 ", pulseaudio buffer size=%u",
              (uint64_t)sox_globals.bufsiz, buffer_attr.tlength);
  }

  pa_channel_map_init_auto(&map, spec.channels, PA_CHANNEL_MAP_ALSA);

  pa->pasp = pa_simple_new(server, name ? name : "SoX", dir, dev, app_str,
                           &spec, &map, &buffer_attr, &error);
  free(server); free(name); free(dev);

  if (pa->pasp == NULL)
  {
    lsx_fail_errno(ft, SOX_EPERM, "can not open audio device: %s", pa_strerror(error));
    return SOX_EOF;
  }

  /* TODO: Is it better to convert format/rates in SoX or in
   * always let Pulse Audio do it?  Since we don't know what
   * hardware prefers, assume it knows best and give it
   * what user specifies.
   */

  return SOX_SUCCESS;
}

static int startread_pulse(sox_format_t *ft)
{
    return setup(ft, 1);
}

static int stopread_pulse(sox_format_t * ft)
{
  priv_t *pa = (priv_t *)ft->priv;

  pa_simple_free(pa->pasp);

  return SOX_SUCCESS;
}

static size_t read_samples_pulse(sox_format_t *ft, sox_sample_t *buf, size_t nsamp)
{
  priv_t *pa = (priv_t *)ft->priv;
  size_t len;
  int rc, error;

  if (!nsamp)
    return 0;

  /* Pulse Audio buffer lengths are true buffer lengths and not
   * count of samples. */
  len = nsamp * sizeof(sox_sample_t);

  rc = pa_simple_read(pa->pasp, buf, len, &error);

  if (rc < 0)
  {
    lsx_fail_errno(ft, SOX_EPERM, "error reading from audio device: %s", pa_strerror(error));
    return SOX_EOF;
  }
  else
    return nsamp;
}

static int startwrite_pulse(sox_format_t * ft)
{
    return setup(ft, 0);
}

static size_t write_samples_pulse(sox_format_t *ft, const sox_sample_t *buf, size_t nsamp)
{
  priv_t *pa = (priv_t *)ft->priv;
  size_t len;
  int rc, error;

  /* Pulse Audio buffer lengths are true buffer lengths and not
   * count of samples. */
  len = nsamp * sizeof(sox_sample_t);

  rc = pa_simple_write(pa->pasp, buf, len, &error);

  if (rc < 0)
  {
    lsx_fail_errno(ft, SOX_EPERM, "error writing to audio device: %s", pa_strerror(error));
    return SOX_EOF;
  }

  return nsamp;
}

static int stopwrite_pulse(sox_format_t * ft)
{
  priv_t *pa = (priv_t *)ft->priv;
  int error;

  pa_simple_drain(pa->pasp, &error);
  pa_simple_free(pa->pasp);

  return SOX_SUCCESS;
}

LSX_FORMAT_HANDLER(pulseaudio)
{
  static char const *const names[] = { "pulseaudio", NULL };
  static unsigned const write_encodings[] = {
    SOX_ENCODING_SIGN2, 32, 0,
    0};
  static sox_format_handler_t const handler = {SOX_LIB_VERSION_CODE,
    "Pulse Audio client",
    names, SOX_FILE_DEVICE | SOX_FILE_NOSTDIO,
    startread_pulse, read_samples_pulse, stopread_pulse,
    startwrite_pulse, write_samples_pulse, stopwrite_pulse,
    NULL, write_encodings, NULL, sizeof(priv_t)
  };
  return &handler;
}
