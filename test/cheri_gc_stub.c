/* Sandbox side (libc_cheri side). */

#include "cheri_gc.h"

int	cheri_invoke(struct cheri_object gc_object,
	    register_t methodnum, register_t a1,
	    __capability void *c3)
	    __attribute__((cheri_ccall));

register_t cheri_gc_methodnum_alloc = CHERI_GC_METHOD_ALLOC_C;
int
cheri_gc_alloc_c(struct cheri_object gc_object,
    __capability void * __capability *out_ptr,
    size_t sz)
{

	return (cheri_invoke(gc_object, cheri_gc_methodnum_alloc, sz, out_ptr));
}

register_t cheri_gc_methodnum_revoke = CHERI_GC_METHOD_REVOKE_C;
int
cheri_gc_revoke_c(struct cheri_object gc_object,
    __capability void *ptr)
{

	return (cheri_invoke(gc_object, cheri_gc_methodnum_revoke, 0, ptr));
}

register_t cheri_gc_methodnum_reuse = CHERI_GC_METHOD_REUSE_C;
int
cheri_gc_reuse_c(struct cheri_object gc_object,
    __capability void *ptr)
{

	return (cheri_invoke(gc_object, cheri_gc_methodnum_reuse, 0, ptr));
}
