/* Ambient side (libcheri side). */

#include <cheri/cheri_class.h>
#include <cheri/cheri_type.h>

#include <gc.h>
#include <gc_debug.h>

#include "cheri_gc.h"

CHERI_CLASS_DECL(cheri_gc);

static __capability void	*cheri_gc_type;

struct cheri_gc {
	CHERI_SYSTEM_OBJECT_FIELDS;	/* saved c0 */
	int cg_perm;	/* method number permissions */
};

static __attribute__ ((constructor)) void
cheri_gc_init(void)
{

	cheri_gc_type = cheri_type_alloc();
}

int
cheri_gc_new(int perm, struct cheri_object *cop)
{
	struct cheri_gc *cgp;

	cgp = calloc(1, sizeof(*cgp));
	if (cgp == NULL) {
		return (-1);
	}
	CHERI_SYSTEM_OBJECT_INIT(cgp);	/* store c0 */
	cgp->cg_perm = perm;

	cop->co_codecap = cheri_setoffset(cheri_getpcc(),
	    (register_t)CHERI_CLASS_ENTRY(cheri_gc));
	cop->co_codecap = cheri_seal(cop->co_codecap, cheri_gc_type);

	cop->co_datacap = cheri_ptrperm(cgp, sizeof(*cgp),
	    CHERI_PERM_GLOBAL | CHERI_PERM_LOAD |
	    CHERI_PERM_LOAD_CAP | CHERI_PERM_STORE |
	    CHERI_PERM_STORE_CAP);
	cop->co_datacap = cheri_seal(cop->co_datacap, cheri_gc_type);

	return (0);
}

void
cheri_gc_setperm(int perm, struct cheri_object co)
{
	__capability struct cheri_gc *cgp;

	cgp = cheri_unseal(co.co_datacap, cheri_gc_type);
	cgp->cg_perm = perm;
}

void
cheri_gc_revoke(struct cheri_object co)
{

	cheri_gc_setperm(0, co);
}

void
cheri_gc_destroy(struct cheri_object co)
{
	__capability struct cheri_gc *cgp;

	cgp = cheri_unseal(co.co_datacap, cheri_gc_type);
	free((void *)cgp);
}

static int
_cheri_gc_alloc_c(__capability void * __capability *out_ptr, size_t sz)
{
	__capability struct cheri_gc *cgp;

	/* Check permission to allocate hasn't been revoked. */
	cgp = cheri_getidc();
	if (!(cgp->cg_perm & CHERI_GC_METHOD_ALLOC_C)) {
		return (-1);
	}

	/* Check output pointer is not NULL and has the correct size. */
	if (out_ptr == NULL ||
	    cheri_getlen(out_ptr) < sizeof(*out_ptr)) {
		return (-1);
	}

	/* Forward to GC. */
	*out_ptr = gc_malloc(sz);
	return (0);
}

static int
_cheri_gc_revoke_c(__capability void *ptr)
{
	__capability struct cheri_gc *cgp;

	/* Check permission to revoke hasn't been revoked. */
	cgp = cheri_getidc();
	if (!(cgp->cg_perm & CHERI_GC_METHOD_REVOKE_C)) {
		return (-1);
	}

	/* Forward to GC. */
	gc_revoke(ptr);
	return (0);
}

static int
_cheri_gc_reuse_c(__capability void *ptr)
{
	__capability struct cheri_gc *cgp;

	/* Check permission to reuse hasn't been revoked. */
	cgp = cheri_getidc();
	if (!(cgp->cg_perm & CHERI_GC_METHOD_REUSE_C)) {
		return (-1);
	}

	/* Forward to GC. */
	gc_reuse(ptr);
	return (0);
}

int
cheri_gc_enter(register_t methodnum, register_t a1,
    struct cheri_object co, __capability void *c3) __attribute__((cheri_ccall))
{

	switch (methodnum) {
	case CHERI_GC_METHOD_ALLOC_C:
		return (_cheri_gc_alloc_c(c3, a1));
	case CHERI_GC_METHOD_REVOKE_C:
		return (_cheri_gc_revoke_c(c3));
	case CHERI_GC_METHOD_REUSE_C:
		return (_cheri_gc_reuse_c(c3));
	default:
		return (-1);
	}
}
