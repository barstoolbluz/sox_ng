/* libSoX effect: tremolo  (c) 2007 robs@users.sourceforge.net
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

static int getopts(sox_effect_t * effp, int argc, char * * argv)
{
  double speed, depth = 40;
  char dummy;     /* To check for extraneous chars. */
  char offset[100];
  char * args[] = {0, "sine", "fmod", 0, 0, "25"};

  if (argc < 2 || argc > 3) return lsx_usage(effp);
  if (sscanf(argv[1], "%lf %c", &speed, &dummy) != 1 || speed < 0) {
    lsx_fail("speed `%s' must be a number >= 0", argv[1]);
    return SOX_EOF;
  }
  if ((argc > 2 && sscanf(argv[2], "%lf %c", &depth, &dummy) != 1) ||
      depth <= 0 || depth > 100) {
    lsx_fail("depth `%s' must be a positive number up to 100", argv[2]);
    return SOX_EOF;
  }
  args[0] = argv[0];
  args[3] = argv[1];
  sprintf(offset, "%g", 100 - depth / 2);
  args[4] = offset;
  return lsx_synth_effect_fn()->getopts(effp, (int)array_length(args), args);
}

sox_effect_handler_t const * lsx_tremolo_effect_fn(void)
{
  static char const usage[] = "speed [depth]";
  static char const * const extra_usage[] = {
    "OPTION  RANGE  DEFAULT  DESCRIPTION",
    "speed   0-      none    Frequency of the tremolo",
    "depth   0-100    40     Depth of the tremolo as a percentage",
    NULL
  };
  static sox_effect_handler_t handler;
  handler = *lsx_synth_effect_fn();
  handler.name = "tremolo";
  handler.usage = usage;
  handler.extra_usage = extra_usage;
  handler.getopts = getopts;
  return &handler;
}
