#ifndef _GC_SCAN_H_
#define _GC_SCAN_H_

#include "gc_cheri.h"

void gc_scan_region (__gc_capability void * region);

uint64_t
gc_get_page_tags (__gc_capability void * page);

#endif /* _GC_SCAN_H_ */
