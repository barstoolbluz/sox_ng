/* keymap.c: SoX keypress-to-effect-parameter-change functions
 *
 * Copyright (c) 2025 Martin Guy <martinwguy@gmail.com>
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 2
 * as published by the Free Software Foundation.
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

#include <ctype.h>   /* for isdigit() */

/* Routines to remember, to apply and to forget keymaps.
 *
 * The string values are mallocked memory which we are responsible for freeing.
 * The effect name may be "synth2" meaning "only tweak the second synth effect
 * in the effects chain" and a field name like "decay2" means "only change the
 * "decay" field of the second delay/decay pair of the effect.
 */
void
sox_keymap_add(char *key, char *effect, char *field, char op, double step)
{
  sox_keymap_t *keymaps;
  unsigned keymap_count = sox_globals.keymap_count;

  lsx_revalloc(sox_globals.keymaps, sox_globals.keymap_count + 1);
  keymaps = sox_globals.keymaps;
  keymaps[keymap_count].key = key;
  keymaps[keymap_count].effect = effect;
  keymaps[keymap_count].field = field;
  keymaps[keymap_count].op = op;
  keymaps[keymap_count].step = step;
  sox_globals.keymap_count++;
}

/*
 * Tells you whether a named key or named effect.field is mapped
 */
sox_bool
sox_is_keymapped(char *key)
{
  sox_keymap_t *keymaps = sox_globals.keymaps;
  char *effect = NULL, *field = NULL;
  /* "." is a key name, not a null effect.field */
  char *dot = strcmp(key, ".") ? strchr(key, '.') : NULL;
  unsigned i;

  if (dot) {
    effect = key;
    field = dot + 1;
  }

  for (i=0; i < sox_globals.keymap_count; i++) {
    sox_keymap_t *keymap = keymaps + i;
    /* Does a key name match? */
    if (!strcmp(key, keymap->key)) {
      return sox_true;
    }
    /* Do the effect and field names match? */
    if (dot && !strncmp(effect, keymap->effect, dot - key)
            && !strcmp(field, keymap->field)) {
      return sox_true;
    }
  }
  return sox_false;
}

int
sox_keymap_apply(sox_effects_chain_t *effects_chain, char *key)
{
  unsigned i;
  sox_bool found_key = sox_false;
  sox_bool found_effect = sox_false;
  sox_keymap_t *keymaps = sox_globals.keymaps;

  for (i=0; i < sox_globals.keymap_count; i++) {
    if (!strcmp(key, keymaps[i].key)) {
      sox_effect_t **e;
      size_t n;
      /* This is the Nth occurrence in the chain of
       * an effect with this name */
      unsigned occurrence = 0;

      found_key = sox_true;

      /* Find the named effect in the effects chain */
      for (n=0, e=effects_chain->effects;
           n < effects_chain->length;
           n++, e++) {
        sox_effect_t *effp = (*e);
        unsigned namelen = strlen(effp->handler.name);
        char  *effect   = keymaps[i].effect;
        char  *field    = keymaps[i].field;
        char   op       = keymaps[i].op;
        double step     = keymaps[i].step;

        /* Match keymap of "synth2" etc. to effect name "synth" */
        if (!strncmp(effect, effp->handler.name, namelen)) {
          occurrence++;

          /* Does this binding apply to all invocations of the effect */
          if (effect[namelen] == '\0' ||
              /* ...or just to the Nth invocation of the effect? */
              (unsigned)atoi(effect+namelen) == occurrence) {
            char *valuestr = effp->handler.get(effp, field);
            double value;
            char *result;

            found_effect = sox_true;
            if (!valuestr) {
              lsx_warn("can't get the current value of %s.%s",
                       effect, field);
              return SOX_ENOEFFECT;
            }
            value = lsx_strtod(valuestr, NULL);
            switch (op) {
            case '+': value += step; break;
            case '-': value -= step; break;
            case '*': value *= step; break;
            case '/': value /= step; break;
            case '=': value  = step; break;
            }
            /* Reuse the string from handler.get() as it's mallocked[16]
             * and it's ours now. */
            sprintf(valuestr, "%g", value);
            result = effp->handler.set(effp, field, valuestr);
            if (!result) {
              lsx_warn("failed to set %s.%s to %s", effect, field, valuestr);
            } else {
              lsx_report("set %s.%s to %s", effect, field, result);
              free(result);
            }
            free(valuestr);
          }
        }
      }
    }
  }
  if (!found_key) {
    lsx_warn("key `%s' doesn't do anything", key);
    return SOX_ENOKEYMAP;
  }
  if (!found_effect) {
    lsx_warn("effect not found in the chain");
    return SOX_ENOEFFECT;
  }
  return SOX_SUCCESS;
}

void
sox_keymap_free(void)
{
  sox_keymap_t *keymaps = sox_globals.keymaps;
  unsigned i;

  for (i=0; i < sox_globals.keymap_count; i++) {
    free(keymaps[i].key);
    free(keymaps[i].effect);
    free(keymaps[i].field);
  }
  free(keymaps);
  sox_globals.keymaps = NULL;
  sox_globals.keymap_count = 0;
}
