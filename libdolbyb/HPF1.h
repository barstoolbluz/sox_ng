/* HPF1.h */

#include "Param.h"

/* Amend to allow separate filters for high and low stage */
#define HPF1NumberOfFilters  ParamMaxChnl

extern void HPF1Init(dolbyb_t *Param);
extern int64_t HPF1Check(dolbyb_t *Param, int64_t InSamp, uint16_t Chn);
extern int64_t HPF1(dolbyb_t *Param, int64_t InSamp, uint16_t Chn);
extern void HPF1Update(dolbyb_t *Param, uint16_t Chn);
