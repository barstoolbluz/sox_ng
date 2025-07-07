/* SidePath.h */

extern char *SidePathInit(dolbyb_t *Param);
extern int64_t SidePathCheck(dolbyb_t *Param, int64_t InSamp, uint16_t Chn);
extern int64_t SidePath(dolbyb_t *Param, int64_t InSamp, uint16_t Chn);
extern int64_t SidePathUpdate(dolbyb_t *Param, uint16_t Chn);
