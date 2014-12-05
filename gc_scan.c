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

gc_tags
gc_get_page_tags (__gc_capability void * page)
{
	__gc_capability void * __gc_capability * scan;
	scan = gc_cheri_ptr(
		gc_cheri_getbase(page) + gc_cheri_getoffset(page),
		/*GC_PAGESZ*//*can't do this!*/
		gc_cheri_getlen(page));
	uint64_t mask;
	gc_tags tags;
	uint64_t * tagp;
	tags.lo = 0;
	tags.hi = 0;
	mask = 1ULL;
	tagp = &tags.lo;
	for (;
			 gc_cheri_getoffset(scan) < gc_cheri_getlen(page);
			 scan++, mask <<= 1)
	{
		gc_debug("scan: %s\n", gc_cap_str(scan));
		if (!mask)
		{
			mask = 1ULL;
			tagp = &tags.hi;
		}
		if (gc_cheri_gettag(*scan))
			*tagp |= mask;
	}
	return tags;
}

/*
 * TO CONSIDER:
 * Deutsch-Schorr-Waite pointer reversal: but you can use the offset field of the capability to remember how far you were (which pointer you were considering) in the node above!
 */
