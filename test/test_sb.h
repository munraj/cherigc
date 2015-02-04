#ifndef _TEST_SB_H_
#define _TEST_SB_H_

/*
 * Test sandboxing and revocation.
 */

#include <cheri/sandbox.h>

#include <gc.h>

#include "framework.h"
#include "sb_param.h"

testfn		test_sb;

struct sb
{
	struct sandbox_class	*sb_cp;
	struct sandbox_object	*sb_op;
	size_t			sb_hpsz;
};

int	sb_init(struct tf_test *thiz, struct sb *sbp,
	    const char *path, size_t hpsz);
int	sb_invoke(struct tf_test *thiz, struct sb *sbp,
	    _gc_cap struct sb_param *sp);

#endif /* !_TEST_SB_H_ */
