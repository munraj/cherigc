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
	int i;
	for (i=0; i<GC_NUM_SAVED_REGS; i++)
		gc_debug("root: c%d: %s\n", 17+i, gc_cap_str(gc_state->regs_c[i]));
}

void
gc_resume_marking (void)
{
	//gc_mark_object();
}

void
gc_start_sweeping (void)
{
}

void
gc_resume_sweeping (void)
{
}
