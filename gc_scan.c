#include "gc_scan.h"
#include "gc_debug.h"

void gc_scan_region (__gc_capability void * region)
{
	gc_debug("scanning region %s", gc_cap_str(region));
	__gc_capability void ** scan;
	__gc_capability void * ptr;
	for (scan = (__gc_capability void **) region;
		(void*)scan < (void*)region + gc_cheri_getlen(region);
		scan++)
	{
		ptr = *scan;
		gc_debug("scan: %p value: %p tag: %d",
			(void*)scan, (void*)ptr, gc_cheri_gettag(ptr));
	}
}

uint64_t
gc_get_page_tags (__gc_capability void * page)
{
	__gc_capability void * __gc_capability * scan;
	uint64_t tags, mask;
	tags = 0;
	mask = 1ULL;
	for (scan = gc_cheri_ptr((void*)page, GC_PAGESZ);
			 gc_cheri_getoffset(scan) < GC_PAGESZ;
			 scan++, mask <<= 1)
	{
		if (gc_cheri_gettag(*scan))
			tags |= mask;
	}
	return tags;
}

/*
 * TO CONSIDER:
 * Deutsch-Schorr-Waite pointer reversal: but you can use the offset field of the capability to remember how far you were (which pointer you were considering) in the node above!
 */
