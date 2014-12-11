#ifndef _FRAMEWORK_H_
#define _FRAMEWORK_H_

struct tf_test;

typedef int	testfn(struct tf_test *_thiz);
typedef void	testpf(struct tf_test *_thiz, const char *_fmt, ...);
typedef void	testexit(struct tf_test *_thiz, int _rc);
typedef void	testassert(struct tf_test *_thiz, int _cond,
		    const char *_scond, const char *_file, int _line);
typedef void	tf_sig_fn(int);

/*
 * Note: tests should actually invoke the functions without the `2' suffix,
 * because macro magic is used to pass the test as a parameter.
 */
struct tf_test {
	testfn		*t_fn;		/* main test function */
	const char	*t_desc;	/* textual description */
	/*
         * printf(3)-style function for use by the test to output diagnostic
	 * messages.
	 */
	testpf		*t_pf2;
	/*
	 * exit(3)-style function for use by the test to terminate a run.
	 */
	testexit	*t_exit2;
	/*
	 * assert(3)-style function for use by the test to verify conditions.
	 * The test should actually invoke tf_test.t_assert(), even though it
	 * doesn't exist.
	 */
	testassert	*t_assert2;
	/* Return code from t_fn or, if forked, from t_exit. */
	int		 t_rc;
#ifdef TF_FORK
	/* Fork a new process to run this test. */
	int		 t_dofork;
	/* The signal that caused the child to terminate, or zero. */
	int		 t_sig;
#endif
};
#define		t_assert(_cond)	t_assert2(thiz, (_cond), #_cond, \
				    __FILE__, __LINE__)
#define		t_pf(...)	t_pf2(thiz, __VA_ARGS__)
#define		t_exit(...)	t_exit2(thiz, __VA_ARGS__)

struct tf_result {
	int		 r_nrun;	/* number of tests actually run */
	int		 r_nfail;	/* number of tests that failed */
	int		 r_nsucc;	/* number of tests that succeeded */
};

struct tf_init {
	/* SIGINFO handler (for info via CTRL+T on the console). */
	tf_sig_fn	*i_siginfo_hnd;
};

void		tf_init(struct tf_init *_init);
void		tf_runall(struct tf_test *_test, struct tf_result *_result);
void		tf_run(struct tf_test *_test);

/* Default t_pf2. */
void		tf_printf2(struct tf_test *_test, const char *_fmt, ...);

/* Default t_exit2. */
void		tf_exit2(struct tf_test *_test, int _rc);

/* Default t_assert2. */
void		tf_assert2(struct tf_test *_test, int _cond, const char *_scond,
		    const char *_file, int _line);
#ifdef TF_FORK
/* Wrapper around fork(2). */
int		tf_fork(void);
int		tf_waitpid(int _pid, int *_status, int _options);
#endif

/* Return values for testfn. */
#define	TF_SUCC		0		/* success */
#define	TF_FAIL		1		/* failure */

#endif /* _FRAMEWORK_H_ */
