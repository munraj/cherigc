#include <stdio.h> 
#include <string.h> 
#include <gc_debug.h>
#include "cheri_gc.h"
#include "test_sb.h"

#include <cheri/cheri_class.h>
#include <cheri/cheri_type.h>
struct sandbox_object {
	CHERI_SYSTEM_OBJECT_FIELDS;
	struct sandbox_class	*sbo_sandbox_classp;
	void			*sbo_mem;
	register_t		 sbo_sandboxlen;
	register_t		 sbo_heapbase;
	register_t		 sbo_heaplen;
	uint			 sbo_flags;	/* Sandbox flags. */

	/*
	 * The object's own code and data capabilities.
	 */
	struct cheri_object	 sbo_cheri_object_rtld;
	struct cheri_object	 sbo_cheri_object_invoke;

	/*
	 * System-object capabilities that can be passed to the object.
	 */
	struct cheri_object	 sbo_cheri_object_system;

	/*
	 * Sandbox statistics.
	 */
	struct sandbox_object_stat	*sbo_sandbox_object_statp;
};

int
sb_init(struct tf_test *thiz, struct sb *sbp,
    const char *path, size_t hpsz)
{
	int rc;

	sbp->sb_hpsz = hpsz;

	rc = sandbox_class_new(path, hpsz, &sbp->sb_cp);
	if (rc != 0) {
		thiz->t_pf("error: sandbox_class_new\n");
		return (1);
	}

	rc = sandbox_object_new(sbp->sb_cp, &sbp->sb_op);
	if (rc != 0) {
		thiz->t_pf("error: sandbox_object_new\n");
		return (1);
	}

	return (0);
}

int
sb_invoke(struct tf_test *thiz, struct sb *sbp,
    _gc_cap struct sb_param *sp)
{
	int rc;

	rc = sandbox_object_cinvoke(sbp->sb_op,
	    0, 0, 0, 0,
	    0, 0, 0, 0,
	    sp, NULL, NULL, NULL,
	    NULL, NULL, NULL, NULL);

	return (rc);
	(void)thiz;
}

int
test_sb(struct tf_test *thiz)
{
	struct sb sb;
	struct sb_param sp;
	__capability struct sb_param *spc;
	int rc;
	__capability void *cap;

	thiz->t_pf("create sandbox\n");
	rc = sb_init(thiz, &sb, SB_BIN, SB_HPSZ);
	if (rc != 0)
		return (rc);

	spc = cheri_ptr(&sp, sizeof(sp));
	memset(&sp, 0, sizeof(sp));

	rc = cheri_gc_new(CHERI_GC_METHOD_ALLOC_C |
	    CHERI_GC_METHOD_STATUS_C, &sp.sp_gc);
	if (rc != 0) {
		thiz->t_pf("error: cheri_gc_new\n");
		return (rc);
	}
	
	thiz->t_pf("sandbox codecap: %s\n", gc_cap_str(sb.sb_op->sbo_cheri_object_invoke.co_codecap));
	thiz->t_pf("sandbox datacap: %s\n", gc_cap_str(sb.sb_op->sbo_cheri_object_invoke.co_datacap));

	spc->sp_cap1 = cheri_ptrperm(&cap, sizeof(cap), CHERI_PERM_STORE_CAP);

	/* XXX: assume GC initialized. */
	
	thiz->t_pf("invoke sandbox\n");
	spc->sp_op = OP_INIT;
	rc = sb_invoke(thiz, &sb, spc);
	thiz->t_pf("return value from sandbox: %d\n", rc);
	
	/* In the sandbox, force p to the stack. */	
	thiz->t_pf("invoke sandbox\n");
	spc->sp_op = OP_TRY_USE;
	printf("note: spc is %s\n", gc_cap_str(spc));
	rc = sb_invoke(thiz, &sb, spc);
	thiz->t_pf("return value from sandbox: %d\n", rc);
	thiz->t_pf("cap: %s\n", gc_cap_str(cap));
	/* Check stack. */
	//struct gc_tags tg1 = gc_get_page_tags(gc_cheri_ptr(0x160c0c000ULL, 0x1000));
	//printf("lo: 0x%llx hi: 0x%llx\n", tg1.tg_lo, tg1.tg_hi);

	/* Do a collection. */
	//gc_extern_collect();
	
	// Revoke a cap.
	//gc_revoke(cap);
	//gc_extern_collect();
	gc_cmdln();
	
	thiz->t_pf("invoke sandbox\n");
	spc->sp_op = OP_TRY_USE;
	printf("note: spc is %s\n", gc_cap_str(spc));
	rc = sb_invoke(thiz, &sb, spc);
	thiz->t_pf("return value from sandbox: %d\n", rc);
	thiz->t_pf("cap: %s\n", gc_cap_str(cap));

	return (0);
}
