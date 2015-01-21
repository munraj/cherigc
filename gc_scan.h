#ifndef _GC_SCAN_H_
#define _GC_SCAN_H_

#include "gc_cheri.h"

struct gc_tags
{
	uint64_t	tg_lo;	/* tags for low 2048 bytes */
	uint64_t	tg_hi;	/* tags for high 2048 bytes */
	int		tg_v;	/* non-zero => tags valid */
};

void		gc_scan_region(_gc_cap void *region);
struct gc_tags	gc_get_page_tags(_gc_cap void *page);

#endif /* !_GC_SCAN_H_ */
