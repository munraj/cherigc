#ifndef _GC_SCAN_H_
#define _GC_SCAN_H_

#include "gc_cheri.h"

void gc_scan_region (__gc_capability void * region);

typedef struct gc_tags_s
{
	uint64_t lo, hi;
} gc_tags;

gc_tags
gc_get_page_tags (__gc_capability void * page);

#endif /* _GC_SCAN_H_ */
