#ifndef _GC_COLLECT_H_
#define _GC_COLLECT_H_

#include "gc_cheri.h"
#include "gc_scan.h"

/*
 * Collect. Requires regs and stack to be saved.
 */
void	gc_collect(void);

void	gc_start_marking(void);
int	gc_is_unlimited(_gc_cap void *_obj);
void	gc_scan_tags(_gc_cap void *_obj, struct gc_tags _tags);
void	gc_scan_tags_64(_gc_cap void *_obj, uint64_t _tags);
void	gc_push_roots(void);
void	gc_resume_marking(void);
void	gc_start_sweeping(void);
void	gc_resume_sweeping(void);
void	gc_sweep_large_iter(_gc_cap struct gc_btbl *btbl, uint8_t *byte,
	    uint8_t type, void *addr, int j, int *freecont);
void	gc_sweep_small_iter(_gc_cap struct gc_btbl *btbl, uint8_t *byte,
	    uint8_t type, void *addr, int j);
void	gc_mark_children(_gc_cap void *obj,
	    _gc_cap struct gc_btbl *btbl, size_t big_indx,
	    _gc_cap struct gc_blk *blk, size_t sml_indx);
#endif /* !_GC_COLLECT_H */
