/* HPF2SetVals.h */

#include "Param.h"

#define HPF2SetValsFltAttMux  1000000L
#define HPF2SetValsNumberOfFilters  ParamMaxChnl
#define HPF2SetValsTableSize  1000

extern char *HPF2SetValsInit(dolbyb_t *Param);
extern void HPF2SetVals(dolbyb_t *Param, uint16_t FltNum);
