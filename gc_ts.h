#ifndef _GC_TS_H_
#define _GC_TS_H_

#include <machine/sysarch.h>

#include "gc_cheri.h"

/*
 * Trusted stack.
 *
 * Opaque, but guaranteed to be accessible
 * by treating it as a capability store.
 */
struct gc_ts
{
	struct cheri_stack gts_cs;
};
int	gc_cheri_get_ts(_gc_cap struct gc_ts *_buf);
int	gc_cheri_put_ts(_gc_cap struct gc_ts *_buf);

#endif /* !_GC_TS_H_ */
