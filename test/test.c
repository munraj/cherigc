#include "framework.h"
int	test_gc_init(void *thiz);
struct test	tf_tests[] = {
	{.t_fn = test_gc_init, .t_desc = "gc initialization"},
};

int
main()
{
}

int
test_gc_init(void *thiz)
{
	return TF_SUCC;
}

