/* HPF2.h */

extern void HPF2Init(dolbyb_t *Param);
extern int64_t HPF2Check(dolbyb_t *Param, int64_t InSamp, uint16_t Chn);
extern int64_t HPF2(dolbyb_t *Param, int64_t InSamp, uint16_t Chn);
extern void HPF2Update(dolbyb_t *Param, uint16_t Chn);
