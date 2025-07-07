/* Mixers.h */

/* Was (I * 18 + S * 15) / 18 */
#define MixersEncode(InSamp, SidSmp) (((InSamp) * 6 + (SidSmp) * 5) / 6)
#define MixersDecode(InSamp, SidSmp) (((InSamp) * 6 - (SidSmp) * 5) / 6)
