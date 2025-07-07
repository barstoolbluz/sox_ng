/* SetGate.h */

#include "Param.h"

#define SetGateNumberOfFilters  ParamMaxChnl
#define SetGateTab1St     25
#define SetGateTab1Ed     1000
#define SetGateTab2St     1
#define SetGateTab2Ed     700

extern char *SetGateInit(dolbyb_t *Param);
extern void SetGate(dolbyb_t *Param, int64_t InSamp, uint16_t TabNum);
