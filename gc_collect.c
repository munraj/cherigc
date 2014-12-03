#include "gc_collect.h"
#include "gc_debug.h"
#include "gc.h"

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
			gc_debug("beginning a new collection\n");
#ifdef GC_COLLECT_STATS
			gc_state->nmark = 0;
			gc_state->nmarkbytes = 0;
			gc_state->nsweep = 0;
			gc_state->nsweepbytes = 0;
#endif /* GC_COLLECT_STATS */
			gc_start_marking();
			break;
	}
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
	int i, error;
	for (i=0; i<GC_NUM_SAVED_REGS; i++)
	{
		gc_debug("root: c%d: %s", 17+i, gc_cap_str(gc_state->regs_c[i]));
		if (!gc_cheri_gettag(gc_state->regs_c[i]))
			continue;
		/* ignore return value of gc_set_mark; always push */
		gc_set_mark_if_used(gc_state->regs_c[i]);
		error = gc_stack_push(gc_state->mark_stack_c,
			gc_state->regs_c[i]);
		if (error)
		{
			gc_error("mark stack overflow");
			return;
		}
	}
	/*
   * Push a capability to the stack to the mark stack.
   * This isn't marked as it's not an object that's been allocated
   * by the collector.
   */
	gc_debug("root: stack: %s", gc_cap_str(gc_state->stack));
	gc_stack_push(gc_state->mark_stack_c, gc_state->stack);
}

void
gc_scan_tags (__gc_capability void * obj, uint64_t tags)
{
	__gc_capability void * __gc_capability * child_ptr;
	for (child_ptr = obj; tags; tags>>=1, child_ptr++)
	{
		if (tags&1)
		{
			char a[50],b[50];
			strcpy(a, gc_cap_str(child_ptr));
			strcpy(b, gc_cap_str(*child_ptr));
			gc_debug("tag bit set: %s (*=) %s\n", a, b);
		}
	}
			/*parent = obj;
			unmanaged = gc_set_mark(parent[i]);
	int
	gc_try_mark_push (__gc_capability void * obj)
	{
		int unmanaged, error;
		unmanaged = gc_set_mark(gc_state->regs_c[i]);
		if (!unmanaged)
		{
			error = gc_stack_push(gc_state->mark_stack_c, obj);
			if (error) return 1;
		}
		return 0;
	}*/
}

void
gc_resume_marking (void)
{
	int empty, unmanaged;
	size_t i;
	__gc_capability void * obj;
	uint64_t tags, len;
	empty = gc_stack_pop(gc_state->mark_stack_c,
		gc_cheri_ptr(&obj, sizeof(__gc_capability void*)));
	if (empty)
	{
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
		gc_debug("popped off the mark stack, raw: %s\n", gc_cap_str(obj));
		unmanaged = gc_get_obj(obj, gc_cheri_ptr(&obj,
			sizeof(__gc_capability void *)));
		if (unmanaged)
		{
			/* XXX: object is unmanaged by the GC; use its native base and
			 * length (GROW cap in both directions to align - correct?) */
			obj = gc_cheri_ptr(GC_ALIGN(gc_cheri_getbase(obj)),
				GC_ROUND_ALIGN(gc_cheri_getlen(obj)));
			/* XXX: need to know if object is "marked" (been scanned before)
			 * to avoid cycles (could implement as perm bit in scan below;
		   * this would bound the number of scans of the same obj) */
		}
		gc_debug("popped off the mark stack and reconstructed: %s\n",
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
		tags >>= (GC_PAGESZ - (len%GC_PAGESZ));
		gc_scan_tags(obj, tags);
	}
}

void
gc_start_sweeping (void)
{
	gc_state->mark_state = GC_MS_SWEEP;
	gc_resume_sweeping();
}

void
gc_resume_sweeping (void)
{
}
