#ifndef _GC_COLLECT_H_
#define _GC_COLLECT_H_

void
gc_collect (void);


/* ===========Begin Mark============ */
void
gc_start_marking (void);

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
