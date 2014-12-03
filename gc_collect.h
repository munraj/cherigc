#ifndef _GC_COLLECT_H_
#define _GC_COLLECT_H_

#include "gc_cheri.h"
#include "gc_scan.h"

void
gc_collect (void);

/* ===========Begin Mark============ */
void
gc_start_marking (void);

int
gc_is_unlimited (__gc_capability void * obj);

void
gc_scan_tags (__gc_capability void * obj, gc_tags tags);

void
gc_scan_tags_64 (__gc_capability void * obj, uint64_t tags);

void
gc_push_roots (void);

void
gc_resume_marking (void);
/* ===========End Mark============ */

/* ===========Begin Sweep============ */
void
gc_start_sweeping (void);

void
gc_resume_sweeping (void);
/* ===========End Sweep============ */

#endif /* _GC_COLLECT_H */
