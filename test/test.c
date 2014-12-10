#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gc.h>
#include <gc_debug.h>

#include "framework.h"

testfn test_gc_init;
testfn test_gc_malloc;

struct tf_test	tests[] = {
	{.t_fn = test_gc_init, .t_desc = "gc initialization"},
	{.t_fn = test_gc_malloc, .t_desc = "gc malloc", .t_dofork = 1},
	{.t_fn = NULL},
};

int
main()
{
	struct tf_result result;

	memset(&result, 0, sizeof(result));
	tf_init();
	tf_runall(tests, &result);

	printf("%d successes %d failures %d total\n",
	    result.r_nsucc, result.r_nfail, result.r_nrun);

	return 0;
}

int
test_gc_init(struct tf_test *thiz)
{
	int rc;

	rc = gc_init();
	rc = (rc == 0) ? TF_SUCC : TF_FAIL;
	return (rc);
}

int
test_gc_malloc(struct tf_test *thiz)
{
	_gc_cap void *obj;
	int i, imin, imax, istep, imult;
	int j, jmin, jmax, jstep, jmult;

	/* Configurable. */
	imin = 512;
	imax = 10*1024*1024;
	istep = 0;
	imult = 2;
	#define JMIN 1
	#define JMAX imax/i
	#define JSTEP 1
	#define JMULT 1

	obj = gc_malloc(100);
	thiz->t_assert(obj != NULL);
	thiz->t_assert(gc_cheri_getlen(obj) >= 100);

	for (i = imin; i <= imax; i = (i+istep)*imult) {
		jmin = JMIN;
		jmax = JMAX;
		jstep = JSTEP;
		jmult = JMULT;
		for (j = jmin; j <= jmax; j = (j+jstep)*jmult) {
			thiz->t_pf("trying allocation of size %d%c\n",
			    SZFORMAT(i));
			obj = gc_malloc(i);
			thiz->t_assert(obj != NULL);
			thiz->t_assert(gc_cheri_getlen(obj) >= i);
		}
	}
	return (TF_SUCC);
}

