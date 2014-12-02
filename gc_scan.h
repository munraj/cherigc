#ifndef _GC_SCAN_H_
#define _GC_SCAN_H_

#include "gc_cheri.h"

void gc_scan_region (__gc_capability void * region);

#endif /* _GC_SCAN_H_ */
