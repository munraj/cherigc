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
			(void*)scan, (void*)ptr, (int)cheri_gettag(ptr));
	}
}

/*
 * TO CONSIDER:
 * Deutsch-Schorr-Waite pointer reversal: but you can use the offset field of the capability to remember how far you were (which pointer you were considering) in the node above!
 */
