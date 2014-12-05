#include "gc_collect.h"
#include "gc_debug.h"
#include "gc.h"
#include "gc_cheri.h"

void
gc_collect (void)
{
	switch (gc_state->mark_state)
	{
		case GC_MS_MARK:
			gc_resume_marking();
			break;
		case GC_MS_SWEEP:
			gc_resume_sweeping();
			break;
		case GC_MS_NONE:
			gc_debug("beginning a new collection");
#ifdef GC_COLLECT_STATS
			gc_state->nmark = 0;
			gc_state->nmarkbytes = 0;
			gc_state->nsweep = 0;
			gc_state->nsweepbytes = 0;
#endif /* GC_COLLECT_STATS */
			gc_start_marking();
			/* because we're not incremental yet: */
			/*while (gc_state->mark_state != GC_MS_SWEEP)
				gc_resume_marking();
			while (gc_state->mark_state != GC_MS_NONE)
				gc_resume_sweeping();*/
			while (gc_state->mark_state != GC_MS_NONE)
				gc_resume_marking();
			break;
	}
}

int
gc_is_unlimited (__gc_capability void * obj)
{
	/* rough approximation */
	return gc_cheri_gettag(obj) && !gc_cheri_getbase(obj);
}

void
gc_start_marking (void)
{
	gc_state->mark_state = GC_MS_MARK;
	gc_push_roots();
	gc_resume_marking();
}

void
gc_push_roots (void)
{
	/* push roots to mark stack */
	gc_debug("push roots:");
	int i, rc, error;
	for (i=0; i<GC_NUM_SAVED_REGS; i++)
	{
		gc_debug("root: c%d: %s", 17+i, gc_cap_str(gc_state->regs_c[i]));
		if (!gc_cheri_gettag(gc_state->regs_c[i]) ||
				gc_is_unlimited(gc_state->regs_c[i]))
			continue;
		rc = gc_set_mark(gc_state->regs_c[i]);
		if (rc == GC_OBJ_FREE)
		{
			/* root should not be pointing to this region; invalidate */
			gc_state->regs_c[i] = gc_cheri_cleartag(gc_state->regs_c[i]);
		}
		else
		{
			/* push whether managed or not */
			error = gc_stack_push(gc_state->mark_stack_c,
				gc_state->regs_c[i]);
			if (error)
			{
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
	/*gc_debug("root: stack: %s", gc_cap_str(gc_state->stack));
	gc_stack_push(gc_state->mark_stack_c, gc_state->stack);*/
}


void
gc_scan_tags (__gc_capability void * obj, gc_tags tags)
{
	gc_scan_tags_64(obj, tags.lo);
	gc_scan_tags_64(obj+GC_PAGESZ/2, tags.hi);
}

void
gc_scan_tags_64 (__gc_capability void * parent, uint64_t tags)
{
	__gc_capability void * __gc_capability * child_ptr;
	__gc_capability void * obj;
	__gc_capability void * raw_obj;
	int rc, error;
	for (child_ptr = parent; tags; tags>>=1, child_ptr++)
	{
		if (tags&1)
		{
			raw_obj = *child_ptr;
			rc = gc_get_obj(raw_obj,
				gc_cheri_ptr(&obj, sizeof(__gc_capability void *)));
			gc_debug("child: %s: rc=%d", gc_cap_str(raw_obj), rc);
			if (rc == GC_OBJ_FREE)
			{
				/* immediately invalidate */
				*child_ptr = gc_cheri_cleartag(raw_obj);
			}
			else if (rc == GC_OBJ_UNMANAGED)
			{
				/* XXX: "mark" this */
				error = gc_stack_push(gc_state->mark_stack_c, raw_obj);
				if (error)
					gc_error("mark stack overflow");
			}
			else /* GC_OBJ_USED */
			{
				rc = gc_set_mark(obj);
				if (rc != GC_OBJ_ALREADY_MARKED)
				{
					error = gc_stack_push(gc_state->mark_stack_c, obj);
					if (error)
						gc_error("mark stack overflow");
				}
			}
		}
	}
}

void
gc_resume_marking (void)
{
	int empty, rc;
	size_t i;
	__gc_capability void * obj;
	uint64_t len;
	gc_tags tags;
	if (gc_state->mark_state == GC_MS_SWEEP)
	{
		gc_resume_sweeping();
		return;
	}
	empty = gc_stack_pop(gc_state->mark_stack_c,
		gc_cheri_ptr(&obj, sizeof(__gc_capability void*)));
	if (empty)
	{
#ifdef GC_COLLECT_STATS
		gc_debug("mark phase complete (marked %zu/%zu object(s), total %zu/%zu bytes)",
			gc_state->nmark, gc_state->nalloc, gc_state->nmarkbytes, gc_state->nallocbytes);
#endif /* GC_COLLECT_STATS */
		gc_start_sweeping();
		return;
	}
	else
	{
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
			sizeof(__gc_capability void *)));
		if (rc == GC_OBJ_FREE)
		{
			/* impossible: a free object is never pushed */
			gc_error("impossible: gc_get_obj returned GC_OBJ_FREE");
		}
		else if (rc == GC_OBJ_UNMANAGED)
		{
			if (!GC_ALIGN(gc_cheri_getbase(obj)))
			{
				gc_debug("warning: popped pointer is near-NULL");
				return;
			}
			/* XXX: object is unmanaged by the GC; use its native base and
			 * length (GROW cap in both directions to align - correct?) */
			obj = gc_cheri_ptr(GC_ALIGN(gc_cheri_getbase(obj)),
				GC_ROUND_ALIGN(gc_cheri_getlen(obj)));
			/* XXX: need to know if object is "marked" (been scanned before)
			 * to avoid cycles (could implement as perm bit in scan below;
		   * this would bound the number of scans of the same obj) */
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
		for (i=0; i<len/GC_PAGESZ; i++)
		{
			tags = gc_get_page_tags(obj);
			gc_scan_tags(obj, tags);
			obj += GC_PAGESZ;
		}
		tags = gc_get_page_tags(obj);
		tags.lo &= (1 << (len%GC_PAGESZ)) - 1;
		tags.hi = 0;
		gc_scan_tags(obj, tags);
	}
}

void
gc_start_sweeping (void)
{
	__gc_capability void * ptr;
	int error;
	gc_debug("begin sweeping");
	gc_state->mark_state = GC_MS_SWEEP;
	/* push the mtbls to consider on to the sweep stack */
	ptr = &gc_state->mtbl;
	ptr = gc_cheri_incbase(ptr, gc_cheri_getoffset(ptr));
	ptr = gc_cheri_setoffset(ptr, 0);
	ptr = gc_cheri_setlen(ptr, sizeof(gc_mtbl));
	error = gc_stack_push(gc_state->sweep_stack_c, ptr);
	if (error)
	{
		gc_error("sweep stack overflow");
		return;
	}
	ptr = &gc_state->mtbl_big;
	ptr = gc_cheri_incbase(ptr, gc_cheri_getoffset(ptr));
	ptr = gc_cheri_setoffset(ptr, 0);
	ptr = gc_cheri_setlen(ptr, sizeof(gc_mtbl));
	error = gc_stack_push(gc_state->sweep_stack_c, ptr);
		gc_debug("pushed %s\n",gc_cap_str(ptr));
	if (error)
	{
		gc_error("sweep stack overflow");
		return;
	}
	gc_resume_sweeping();
}

void
gc_resume_sweeping (void)
{
	int empty, i, j, small, hdrbits, freecont;
	__gc_capability gc_mtbl * mtbl;
	__gc_capability gc_blk * blk;
	uint8_t byte, type, mask;
	void * addr;
	uint64_t tmp;
	empty = gc_stack_pop(gc_state->sweep_stack_c, gc_cap_addr(&mtbl));
	if (empty)
	{
		/* collection complete: */
#ifdef GC_COLLECT_STATS
			gc_debug("sweep phase complete (swept %zu/%zu object(s), total recovered %zu/%zu bytes)",
				gc_state->nsweep, gc_state->nalloc, gc_state->nsweepbytes, gc_state->nallocbytes);
#endif /* GC_COLLECT_STATS */
		gc_state->mark_state = GC_MS_NONE;
#ifdef GC_COLLECT_STATS
		gc_state->nalloc -= gc_state->nsweep;
		gc_state->nallocbytes -= gc_state->nsweepbytes;
#endif /* GC_COLLECT_STATS */
		return;
	}
	else
	{
		gc_debug("mtbl is %s\n", gc_cap_str(mtbl));
		small = mtbl->flags & GC_MTBL_FLAG_SMALL;
		gc_debug("type: %d\n", small);
		/* walk the mtbl, making objects and entire blocks free */
		freecont = 0;
		for (i=0; i<mtbl->nslots/4; i++)
		{
			byte = mtbl->map[i];
			for (j=0; j<4; j++)
			{
				type = (byte >> ((3-j)*2)) & 3;
				addr = (char*)gc_cheri_getbase(mtbl->base) +
						(4*i+j)*mtbl->slotsz;
				if (!small)
				{
					if (type == GC_MTBL_CONT && freecont)
					{
						mask = ~(3 << ((3-j)*2));
						byte &= mask; 
#ifdef GC_COLLECT_STATS
						gc_state->nsweepbytes += GC_BIGSZ;
#endif /* GC_COLLECT_STATS */
					}
					else if (type == GC_MTBL_USED)
					{
						/* used but not marked; free entire block */
						mask = ~(3 << ((3-j)*2));
						byte &= mask; 
						/* next iterations will free continuation data */
						freecont = 1;
#ifdef GC_COLLECT_STATS
						gc_state->nsweep++;
						gc_state->nsweepbytes += GC_BIGSZ;
#endif /* GC_COLLECT_STATS */
						gc_debug("swept entire large block at address %p", addr);
					}
					else if (type == GC_MTBL_USED_MARKED)
					{
						mask = ~(3 << ((3-j)*2));
						byte &= mask; 
						mask = (GC_MTBL_USED << ((3-j)*2));
						byte |= mask;
						freecont = 0;
					}
					else if (freecont)
					{
						freecont = 0;
					}
				}
				else
				{
					blk = gc_cheri_ptr(addr, GC_BLK_HDRSZ);
					/* small mtbl */
					if (type == GC_MTBL_USED)
					{
						hdrbits = (GC_BLK_HDRSZ + blk->objsz - 1) / blk->objsz;
#ifdef GC_COLLECT_STATS
						/*
						 *		free		mark		NOR		meaning
						 *		 0			 0			 1			swept (count)
						 *		 0			 1			 0			alive (don't count)
						 *		 1			 0			 0			don't care
						 *		 1			 1			 0			"impossible"
						 */
						tmp = ~(blk->free | blk->marks);
						tmp &= ((1ULL << (GC_PAGESZ / blk->objsz)) - 1ULL);
						tmp &= ~((1ULL << hdrbits) - 1ULL);
						for (;tmp;tmp>>=1)
						{
							if (tmp&1)
							{
								gc_state->nsweep++;
								gc_state->nsweepbytes += blk->objsz;
							}
						}
#endif /* GC_COLLECT_STATS */
						if (!blk->marks)
						{
							/* entire block free */
							mask = ~(3 << ((3-j)*2));
							byte &= mask; 
							gc_debug("swept entire block storing objects of size %zu at address %s", blk->objsz, gc_cap_str(blk));
						}
						else
						{
							/* make free all those things that aren't marked */
							blk->marks = 0;
							blk->free = ~blk->marks;
							blk->free &= ((1ULL << (GC_PAGESZ / blk->objsz)) - 1ULL);
							/* account for the space taken up by the block header */
							blk->free &= ~((1ULL << hdrbits) - 1ULL);
							gc_debug("swept some objects of size %zu in block %s", blk->objsz, gc_cap_str(blk));
						}
					}
				}
			}
			mtbl->map[i] = byte;
		}
	}
}
