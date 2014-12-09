#include "gc.h"
#include "gc_cheri.h"
#include "gc_collect.h"
#include "gc_debug.h"

void
gc_collect(void)
{

	switch (gc_state->mark_state) {
	case GC_MS_MARK:
		gc_resume_marking();
		break;
	case GC_MS_SWEEP:
		gc_resume_sweeping();
		break;
	case GC_MS_NONE:
		gc_debug("beginning a new collection");
#ifdef GC_COLLECT_STATS
		gc_state->gs_nmark = 0;
		gc_state->gs_nmarkbytes = 0;
		gc_state->gs_nsweep = 0;
		gc_state->gs_nsweepbytes = 0;
#endif
		gc_start_marking();
		/* Because we're not incremental yet: */
		/*while (gc_state->mark_state != GC_MS_SWEEP)
			gc_resume_marking();
		while (gc_state->mark_state != GC_MS_NONE)
			gc_resume_sweeping();*/
		while (gc_state->gs_mark_state != GC_MS_NONE)
			gc_resume_marking();
		break;
	default:
		/* NOTREACHABLE */
	}
}

int
gc_is_unlimited(_gc_cap void * obj)
{

	/* Rough approximation. */
	return (gc_cheri_gettag(obj) && !gc_cheri_getbase(obj));
}

void
gc_start_marking(void)
{

	gc_state->gs_mark_state = GC_MS_MARK;
	gc_push_roots();
	gc_resume_marking();
}

/* Push roots to mark stack. */
void
gc_push_roots(void)
{
	int i, rc, error;

	gc_debug("push roots:");
	for (i = 0; i < GC_NUM_SAVED_REGS; i++)
	{
		gc_debug("root: c%d: %s", 17 + i,
		    gc_cap_str(gc_state_c->gs_regs_c[i]));
		if (!gc_cheri_gettag(gc_state_c->gs_regs_c[i]) ||
		    gc_is_unlimited(gc_state_c->gs_regs_c[i]))
			continue;
		rc = gc_set_mark(gc_state_c->gs_regs_c[i]);
		if (rc == GC_OBJ_FREE) {
			/*
			 * Root should not be pointing to this region;
			 * invalidate.
			 */
			gc_state_c->gs_regs_c[i] = gc_cheri_cleartag(
			    gc_state_c->gs_regs_c[i]);
		} else {
			/* Push whether managed or not. */
			error = gc_stack_push(gc_state_c->gs_mark_stack_c,
			    gc_state_c->gs_regs_c[i]);
			if (error != 0) {
				gc_error("mark stack overflow");
				return;
			}
		}
	}

	/*
	 * Push a capability to the stack to the mark stack.
	 * This isn't marked as it's not an object that's been allocated
	 * by the collector.
	 */
	gc_debug("root: stack: %s", gc_cap_str(gc_state_c->gs_stack));
	gc_stack_push(gc_state_c->gs_mark_stack_c, gc_state_c->gs_stack);
}


void
gc_scan_tags(_gc_cap void *obj, struct gc_tags tags)
{

	gc_scan_tags_64(obj, tags.tg_lo);
	gc_scan_tags_64(obj + GC_PAGESZ / 2, tags.tg_hi);
}

void
gc_scan_tags_64(_gc_cap void *parent, uint64_t tags)
{
	_gc_cap void * _gc_cap *child_ptr;
	_gc_cap void *obj;
	_gc_cap void *raw_obj;
	int rc, error;

	for (child_ptr = parent; tags; tags >>= 1, child_ptr++) {
		if (tags & 1) {
			raw_obj = *child_ptr;
			rc = gc_get_obj(raw_obj, gc_cheri_ptr(&obj,
			    sizeof(_gc_cap void *)));
			gc_debug("child: %s: rc=%d", gc_cap_str(raw_obj), rc);
			if (rc == GC_OBJ_FREE) {
				/* Immediately invalidate. */
				*child_ptr = gc_cheri_cleartag(raw_obj);
			} else if (rc == GC_OBJ_UNMANAGED) {
				/* XXX: "Mark" this. */
				error = gc_stack_push(
				    gc_state_c->gs_mark_stack_c, raw_obj);
				if (error != 0)
					gc_error("mark stack overflow");
			} else if (rc == GC_OBJ_USED) {
				rc = gc_set_mark(obj);
				if (rc != GC_OBJ_ALREADY_MARKED)
				{
					error = gc_stack_push(
					    gc_state_c->gs_mark_stack_c, obj);
					if (error != 0)
						gc_error("mark stack overflow");
				}
			} else {
				/* NOTREACHABLE */
			}
		}
	}
}

void
gc_resume_marking(void)
{
	int empty, rc;
	size_t i, off;
	_gc_cap void * obj;
	uint64_t len;
	struct gc_tags tags;

	if (gc_state_c->gs_mark_state == GC_MS_SWEEP) {
		gc_resume_sweeping();
		return;
	}
	empty = gc_stack_pop(gc_state->mark_stack_c,
	    gc_cheri_ptr(&obj, sizeof(_gc_cap void*)));
	if (empty) {
#ifdef GC_COLLECT_STATS
		gc_debug("mark phase complete (marked %zu/%zu object(s), "
		    "total %zu/%zu bytes)",
		    gc_state->nmark, gc_state->nalloc, gc_state->nmarkbytes,
		    gc_state->nallocbytes);
#endif
		gc_start_sweeping();
		return;
	} else {
		/*
		 * We assume that internal pointers may have been pushed to the
		 * stack, or even unmanaged objects.
		 * So on pop, we first reconstruct the actual object that was
		 * allocated by the collector, to ensure we consider all its
		 * children before marking it (in case future capabilities are a
		 * superset of this capability, and we thus skip it because it's
		 * marked).
		 * If the object wasn't allocated by us, we scan it anyway, but
		 * obviously can't set its mark bit. XXX: Unmanaged cyclic objects
		 * will throw the GC into an infinite loop...
		 */
		gc_debug("popped off the mark stack, raw: %s", gc_cap_str(obj));
		rc = gc_get_obj(obj, gc_cheri_ptr(&obj,
		    sizeof(_gc_cap void *)));
		if (rc == GC_OBJ_FREE) {
			/* NOTREACHABLE */
			/* Impossible: a free object is never pushed. */
			gc_error("impossible: gc_get_obj returned GC_OBJ_FREE");
		} else if (rc == GC_OBJ_UNMANAGED) {
			if (GC_ALIGN(gc_cheri_getbase(obj)) == NULL) {
				gc_debug("warning: popped pointer is near-NULL");
				return;
			}
			/*
			 * XXX: Object is unmanaged by the GC; use its native
			 * base and length (GROW cap in both directions to
			 * align - correct?).
			 */
			obj = gc_cheri_ptr(GC_ALIGN(gc_cheri_getbase(obj)),
			    GC_ROUND_ALIGN(gc_cheri_getlen(obj)));
			/*
			 * XXX: Need to know if object is "marked" (been scanned
			 * before) to avoid cycles (could implement as perm bit
			 * in scan below; this would bound the number of scans
			 * of the same obj).
			 */
		}
		gc_debug("popped off the mark stack and reconstructed: %s",
		    gc_cap_str(obj));
		/*
		 * Mark children of object.
		 * Each child's address is first constructed using knowledge of
		 * the tag bits of the parent.
		 * Tag bits are obtained for each page spanned by the parent.
		 */
		len = gc_cheri_getlen(obj);
		off = 0;
		for (i = 0; i < len / GC_PAGESZ; i++) {
			tags = gc_get_page_tags(
			    gc_cheri_setlen(gc_cheri_incbase(obj, off), GC_PAGESZ));
			gc_scan_tags(obj, tags);
			off += GC_PAGESZ;
		}
		obj += off;
		tags = gc_get_page_tags(obj);
		tags.tg_lo &= (1 << (len % GC_PAGESZ)) - 1;
		tags.tg_hi = 0;
		gc_scan_tags(obj, tags);
	}
}

void
gc_start_sweeping(void)
{
	_gc_cap void *ptr;
	int error;

	gc_debug("begin sweeping");
	gc_state_c->gs_mark_state = GC_MS_SWEEP;

	/* Push the btbls to consider on to the sweep stack. */
	ptr = &gc_state_c->gs_btbl_small;
	ptr = gc_cheri_incbase(ptr, gc_cheri_getoffset(ptr));
	ptr = gc_cheri_setoffset(ptr, 0);
	ptr = gc_cheri_setlen(ptr, sizeof(struct gc_btbl));
	error = gc_stack_push(gc_state_c->gs_sweep_stack_c, ptr);
	if (error != 0) {
		gc_error("sweep stack overflow");
		return;
	}
	ptr = &gc_state_c->gs_btbl_big;
	ptr = gc_cheri_incbase(ptr, gc_cheri_getoffset(ptr));
	ptr = gc_cheri_setoffset(ptr, 0);
	ptr = gc_cheri_setlen(ptr, sizeof(struct gc_btbl));
	error = gc_stack_push(gc_state->gs_sweep_stack_c, ptr);
	gc_debug("pushed %s\n",gc_cap_str(ptr));
	if (error != 0) {
		gc_error("sweep stack overflow");
		return;
	}

	gc_resume_sweeping();
}

void
gc_resume_sweeping(void)
{
	_gc_cap gc_btbl *btbl;
	_gc_cap gc_blk *blk;
	void *addr;
	uint64_t tmp;
	int empty, i, j, small, hdrbits, freecont;
	uint8_t byte, type, mask;

	empty = gc_stack_pop(gc_state_s->gs_sweep_stack_c, gc_cap_addr(&btbl));
	if (empty) {
		/* Collection complete. */
#ifdef GC_COLLECT_STATS
		gc_debug("sweep phase complete (swept %zu/%zu object(s), "
		    "total recovered %zu/%zu bytes)",
		    gc_state->gs_nsweep, gc_state->gs_nalloc,
		    gc_state->gs_nsweepbytes, gc_state->gs_nallocbytes);
#endif
		gc_state->gs_mark_state = GC_MS_NONE;
#ifdef GC_COLLECT_STATS
		gc_state->gs_nalloc -= gc_state->gs_nsweep;
		gc_state->gs_nallocbytes -= gc_state->gs_nsweepbytes;
#endif
		return;
	}
	small = btbl->bt_flags & GC_BTBL_FLAG_SMALL;
	/* Walk the btbl, making objects and entire blocks free. */
	freecont = 0;
	for (i = 0; i < btbl->nslots / 4; i++) {
		byte = btbl->map[i];
		for (j = 0; j < 4; j++) {
			type = (byte >> ((3 - j) * 2)) & 3;
			addr = (char*)gc_cheri_getbase(btbl->bt_base) +
					(4 * i + j)*btbl->bt_slotsz;
			if (!small) {
				if (type == GC_BTBL_CONT && freecont) {
					mask = ~(3 << ((3 - j) * 2));
					byte &= mask; 
#ifdef GC_COLLECT_STATS
					gc_state->nsweepbytes +=
					    btbl->bt_slotsz;
#endif
				} else if (type == GC_BTBL_USED) {
					/*
					 * Used but not marked; free entire
					 * block.
					 */
					mask = ~(3 << ((3-j)*2));
					byte &= mask; 
					/*
					 * Next iterations will free
					 * continuation data.
					 */
					freecont = 1;
#ifdef GC_COLLECT_STATS
					gc_state->nsweep++;
					gc_state->nsweepbytes += btbl->slotsz;
#endif
					gc_debug("swept entire large "
					    "block at address %p",
					    addr);
				} else if (type ==
				    GC_BTBL_USED_MARKED) {
					mask = ~(3 << ((3 - j) * 2));
					byte &= mask; 
					mask = (GC_BTBL_USED << ((3 - j) * 2));
					byte |= mask;
					freecont = 0;
				} else if (freecont) {
					freecont = 0;
				}
			} else {
				blk = gc_cheri_ptr(addr, GC_BLK_HDRSZ);
				/* Small btbl. */
				if (type == GC_BTBL_USED)
				{
					hdrbits = (GC_BLK_HDRSZ +
					    blk->bk_objsz - 1) /
					    blk->bk_objsz;
#ifdef GC_COLLECT_STATS
					/*
					 *  free  mark  NOR  meaning
					 *  0     0     1    swept (count)
					 *  0     1     0    alive (don't count)
					 *  1     0     0    don't care
					 *  1     1     0    "impossible"
					 */
					tmp = ~(blk->bk_free | blk->bk_marks);
					tmp &= ((1ULL << (GC_PAGESZ /
					    blk->objsz)) - 1ULL);
					tmp &= ~((1ULL << hdrbits) -
					    1ULL);
					for (; tmp; tmp >>= 1) {
						if (tmp & 1) {
							gc_state_c->gs_nsweep++;
							gc_state_c->
							    gs_nsweepbytes +=
							    blk->bk_objsz;
						}
					}
#endif
					if (!blk->marks) {
						/* Entire block free. */
						mask = ~(3 << ((3 - j) * 2));
						byte &= mask;
						gc_debug("swept entire block "
						    "storing objects of size "
						    "%zu at address %s",
						    blk->bk_objsz,
						    gc_cap_str(blk));
					} else {
						/*
						 * Make free all those things
						 * that aren't marked.
						 */
						blk->bk_marks = 0;
						blk->bk_free = ~blk->bk_marks;
						blk->bk_free &=
						    ((1ULL << (GC_PAGESZ /
						    blk->bk_objsz)) - 1ULL);
						/* Account for the space taken
						 * up by the block header.
						 */
						blk->bk_free &= ~((1ULL <<
						    hdrbits) - 1ULL);
						gc_debug("swept some objects "
						    "of size %zu in block %s",
						    blk->bk_objsz,
						    gc_cap_str(blk));
					}
				}
			}
		}
		btbl->bk_map[i] = byte;
	}
}
}
