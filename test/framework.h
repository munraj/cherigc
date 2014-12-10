#ifndef _FRAMEWORK_H_
#define _FRAMEWORK_H_

typedef int	testfn(void *_thiz);

struct test {
	testfn		*t_fn;		/* main test function */
	const char	*t_desc;	/* textual description */
};

void		tf_init(void);
void		tf_runall(struct test *_test);
void		tf_run(struct test *_test);

/* Return values for testfn. */
#define	TF_SUCC		0		/* success */
#define	TF_ERROR	1		/* error */

#endif /* _FRAMEWORK_H_ */
