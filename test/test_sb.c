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
    _gc_cap void *param)
{
	int rc;

	rc = sandbox_object_cinvoke(sbp->sb_op,
	    0, 0, 0, 0,
	    0, 0, 0, 0,
	    param, NULL, NULL, NULL,
	    NULL, NULL, NULL, NULL);

	return (rc);
	(void)thiz;
}

int
test_sb(struct tf_test *thiz)
{
	struct sb sb;
	int rc;

	thiz->t_pf("create sandbox\n");
	rc = sb_init(thiz, &sb, SB_BIN, SB_HPSZ);
	if (rc != 0)
		return (rc);

	thiz->t_pf("invoke sandbox\n");
	rc = sb_invoke(thiz, &sb, NULL);
	thiz->t_pf("return value from sandbox: %d\n", rc);

	return (0);
}
