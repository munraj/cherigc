#ifndef _GC_COLLECT_H_
#define _GC_COLLECT_H_

#include "gc_cheri.h"
#include "gc_scan.h"

void	gc_collect(void);
void	gc_start_marking(void);
int	gc_is_unlimited(_gc_cap void *obj);
void	gc_scan_tags(_gc_cap void *obj, struct gc_tags tags);
void	gc_scan_tags_64(_gc_cap void *obj, uint64_t tags);
void	gc_push_roots(void);
void	gc_resume_marking(void);
void	gc_start_sweeping(void);
void	gc_resume_sweeping(void);

#endif /* !_GC_COLLECT_H */
