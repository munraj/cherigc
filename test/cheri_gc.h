#ifndef _CHERI_GC_H_
#define _CHERI_GC_H_

/* Ambient side (libcheri side). */

#include <machine/cheri.h>

int	cheri_gc_new(int perm, struct cheri_object *cop);
void	cheri_gc_setperm(int perm, struct cheri_object cop);
void	cheri_gc_revoke(struct cheri_object cop);
void	cheri_gc_destroy(struct cheri_object cop);

#define	CHERI_GC_METHOD_ALLOC_C		1
#define	CHERI_GC_METHOD_REVOKE_C	2
#define	CHERI_GC_METHOD_REUSE_C		4

/*
 * Sandbox side (libc_cheri side).
 * Return <0 on error and 0 on success.
 */
int	cheri_gc_alloc_c(struct cheri_object gc_object,
	    __capability void * __capability *out_ptr,
	    size_t sz);
int	cheri_gc_revoke_c(struct cheri_object gc_object,
	    __capability void *ptr);
int	cheri_gc_reuse_c(struct cheri_object gc_object,
	    __capability void *ptr);

#endif /* !_CHERI_GC_H_ */
