/* synth: "sox synth" in C using libsox.
 *
 * "synth args" is the same as "sox -n -d synth args"
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
 */

#include "sox_ng.h"
#include <stdlib.h>
#include <stdio.h>

int main(int argc, char * argv[])
{
  sox_format_t *in, *out;
  static sox_signalinfo_t signal;
  sox_effects_chain_t *chain;
  sox_effect_t * effect;
  int i;

  if (sox_init()) exit(1);
  sox_format_init();
  chain = sox_create_effects_chain(&in->encoding, &out->encoding);

  /* Open the output first so we know the sample rate the audio device wants
   * so we can run the whole effects chain at that rate. */
  signal.rate = 48000; /* Suggest 48k; it may write something different */
  signal.channels = 1;
  out = sox_open_write("default", &signal, NULL, "alsa", NULL, NULL);
  if (!out) return 1;

  in = sox_open_read("", &signal, NULL, "null");
  if (!in) return 1;
  effect = sox_create_effect(sox_find_effect("input"));
  sox_effect_options(effect, 1, (char **)&in);
  sox_add_effect(chain, effect, &signal, &signal);
  free(effect);

  effect = sox_create_effect(sox_find_effect("synth"));
  if (sox_effect_options(effect, argc - 1, argv + 1))
    /* Command-line syntax error will bail here */
    return 1;
  sox_add_effect(chain, effect, &signal, &signal);
  free(effect);

  effect = sox_create_effect(sox_find_effect("output"));
  sox_effect_options(effect, 1, (char **)&out);
  sox_add_effect(chain, effect, &signal, &signal);
  free(effect);

  sox_flow_effects(chain, NULL, NULL);

  sox_close(out);
  sox_delete_effects_chain(chain);
  sox_quit();
  return 0;
}
