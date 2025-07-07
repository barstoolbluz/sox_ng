/* DCfilter.h */

extern void DCfilterInit(dolbyb_t *Param);
extern int64_t DCfilter(dolbyb_t *Param, int64_t InSamp, uint16_t Flt);
