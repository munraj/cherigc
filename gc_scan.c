#include "gc_scan.h"
#include "gc_debug.h"

void
gc_scan_region(_gc_cap void *region)
{
	_gc_cap void *ptr;
	_gc_cap void **scan;

	gc_debug("scanning region %s", gc_cap_str(region));
	for (scan = (_gc_cap void **)region;
	    (void*)scan < (void*)region + gc_cheri_getlen(region); scan++) {
		ptr = *scan;
	}
}

struct gc_tags
gc_get_page_tags(_gc_cap void *page)
{
	_gc_cap void * _gc_cap *scan;
	struct gc_tags tags;
	uint64_t * tagp;
	uint64_t mask;
	size_t len;

	/* assert(gc_cheri_getoffset(page) == 0) */
	/* assert(gc_cheri_getlen(page) == GC_PAGESZ) */

	scan = (_gc_cap void * _gc_cap *)page;

	/*scan = gc_cheri_ptr((void*)(gc_cheri_getbase(page) +
	    gc_cheri_getoffset(page)),
	    gc_cheri_getlen(page));*/
	tags.tg_lo = 0;
	tags.tg_hi = 0;
	tags.tg_v = 1;
	mask = 1ULL;
	tagp = &tags.tg_lo;
	len = gc_cheri_getlen(page);
	for (; gc_cheri_getoffset(scan) < len; scan++, mask <<= 1) {
		if (!mask) {
			mask = 1ULL;
			tagp = &tags.tg_hi;
		}
		if (gc_cheri_gettag(*scan))
			*tagp |= mask;
	}
	return (tags);
}
