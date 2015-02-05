#include <string.h>

#include "gc_ts.h"

int
gc_cheri_get_ts(_gc_cap struct gc_ts *buf)
{

	return (sysarch(CHERI_GET_STACK, (void *)&buf->gts_cs));
}

int
gc_cheri_put_ts(_gc_cap struct gc_ts *buf)
{
	
	return (sysarch(CHERI_SET_STACK, (void *)&buf->gts_cs));
}
