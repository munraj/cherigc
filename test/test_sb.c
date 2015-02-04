#include <string.h> 
#include "cheri_gc.h"
#include "test_sb.h"

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

	thiz->t_pf("create sandbox\n");
	rc = sb_init(thiz, &sb, SB_BIN, SB_HPSZ);
	if (rc != 0)
		return (rc);

	spc = cheri_ptr(&sp, sizeof(sp));
	memset(&sp, 0, sizeof(sp));

	rc = cheri_gc_new(CHERI_GC_METHOD_ALLOC_C, &sp.sp_gc);
	if (rc != 0) {
		thiz->t_pf("error: cheri_gc_new\n");
		return (rc);
	}

	/* XXX: assume GC initialized. */

	thiz->t_pf("invoke sandbox\n");
	spc->sp_op = OP_INIT;
	rc = sb_invoke(thiz, &sb, spc);
	thiz->t_pf("return value from sandbox: %d\n", rc);

	/* Do a collection. */
	gc_extern_collect();
	
	thiz->t_pf("invoke sandbox\n");
	thiz->t_pf("note: spc is %p\n", (void *)spc);
	thiz->t_pf("note: spc is %s\n", gc_cap_str(spc));
	spc->sp_op = OP_TRY_USE;
	rc = sb_invoke(thiz, &sb, spc);
	thiz->t_pf("return value from sandbox: %d\n", rc);

	return (0);
}
