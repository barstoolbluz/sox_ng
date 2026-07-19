/* soxcopy: copy one audio file to another... a simple format converter
 *
 * Copyright (c) 2026 Martin Guy <martinwguy@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for details.
 *
 * For brevity, we know that SOX_SUCCESS == 0, don't bother freeing anything
 * and assume that sox_find_effect("input") or ("output") can't fail.
 */

#include "sox_ng.h"
#include <stdlib.h> /* for main() */
#include <stdio.h>  /* for fprintf(stderr) */

/* Only show failure messages */
static void show_error(unsigned level, const char *filename,
                       const char *format, va_list ap)
{ if (level == 1) { vfprintf(stderr, format, ap); putc('\n', stderr); } }

int main(int argc, char * argv[])
{
  char *infile, *outfile;                /* Input and output file names */
  sox_format_t *in, *out;                /* Of the input and output files */
  sox_signalinfo_t insignal, outsignal;  /* Of each effect */
  sox_effects_chain_t *chain;
  sox_effect_t * effect;

  if (argc != 3) {
    fprintf(stderr, "usage: soxcopy input output\n");
    exit(1);
  }
  infile = argv[1]; outfile = argv[2];

  if (sox_init()) exit(1);
  sox_globals.output_message_handler = show_error;
  sox_format_init();
  chain = sox_create_effects_chain(&in->encoding, &out->encoding);

  /* Open the files first so we know supported sample rates and channels */
  in = sox_open_read(infile, NULL, NULL, NULL);
  if (!in) exit(1);
  insignal = in->signal;
  out = sox_open_write(outfile, &out->signal, NULL, NULL, NULL, NULL);
  if (!out) exit(1);
  outsignal = out->signal;

  effect = sox_create_effect(sox_find_effect("input"));
  sox_effect_options(effect, 1, (char **)&in);
  sox_add_effect(chain, effect, &in->signal, &insignal);
  free(effect);

  if (insignal.rate != outsignal.rate) {
    char arg[16];
    char *argv[] = { arg };
    fprintf(stderr, "Converting from %gHz to %gHz\n",
            insignal.rate, outsignal.rate);
    effect = sox_create_effect(sox_find_effect("rate"));
    sprintf(arg, "%g", out->signal.rate);
    sox_effect_options(effect, 1, argv);
    sox_add_effect(chain, effect, &insignal, &outsignal);
    free(effect);
    insignal = outsignal;  /* For the next effect */
  }

  if (insignal.channels != outsignal.channels) {
    char arg[16];
    char *argv[] = { arg };
    fprintf(stderr, "Converting from %d channels to %d channels\n",
            insignal.channels, outsignal.channels);
    effect = sox_create_effect(sox_find_effect("channels"));
    sprintf(arg, "%u", out->signal.channels);
    sox_effect_options(effect, 1, argv);
    sox_add_effect(chain, effect, &insignal, &outsignal);
    free(effect);
    insignal = outsignal;  /* For the next effect */
  }

  effect = sox_create_effect(sox_find_effect("output"));
  sox_add_effect(chain, effect, &insignal, &out->signal);
  sox_effect_options(effect, 1, (char **)&out);
  free(effect);

  sox_flow_effects(chain, NULL, NULL);

  sox_close(out);
  sox_quit();
  return 0;
}
