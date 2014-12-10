#ifdef TF_FORK
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#endif

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "framework.h"

void
tf_init(void)
{
}

void
tf_runall(struct tf_test *test, struct tf_result *result)
{

	while (test->t_fn != NULL) {
		tf_run(test);
		if (test->t_rc == TF_SUCC)
			result->r_nsucc++;
		else
			result->r_nfail++;
		result->r_nrun++;
		test++;
	}
}

void
tf_run(struct tf_test *test)
{
#ifdef TF_FORK
	int pid, status, rc;
#endif

	if (test->t_pf2 == NULL)
		test->t_pf2 = &tf_printf2;
	if (test->t_exit2 == NULL)
		test->t_exit2 = &tf_exit2;
	if (test->t_assert2 == NULL)
		test->t_assert2 = &tf_assert2;
#ifdef TF_FORK
	test->t_sig = 0;
	if (test->t_dofork) {
		pid = tf_fork();
		if (pid == -1) {
			fprintf(stderr, "error: tf_fork\n");
			exit(1);
		}
		if (pid == 0) {
			/* Child. */
			_Exit(test->t_fn(test));
		}
		else {
			/* Parent. */
			rc = tf_waitpid(pid, &status, 0);
			if (rc == -1) {
				fprintf(stderr, "error: tf_waitpid\n");
				exit(1);
			}
			if (WIFEXITED(status)) {
				rc = WEXITSTATUS(status);
			} else if (WIFSIGNALED(status)) {
				rc = TF_FAIL;
				test->t_sig = WTERMSIG(status);
			} else {
				fprintf(stderr, "note: child neither exited nor signaled\n");
				rc = TF_FAIL;
			}
			test->t_rc = rc;
		}
		fprintf(stderr, "test %s: (forked) %s (rc=%d, sig=%d)\n",
		    test->t_desc, test->t_rc == TF_SUCC ? "SUCC" : "FAIL",
		    test->t_rc, test->t_sig);
		return;
	}
#endif
	test->t_rc = test->t_fn(test);
	fprintf(stderr, "test %s: %s (rc=%d)\n", test->t_desc,
	    test->t_rc == TF_SUCC ? "SUCC" : "FAIL", test->t_rc);
}

void
tf_printf2(struct tf_test *test, const char *fmt, ...)
{
	va_list vl;

	va_start(vl, fmt);
	vprintf(fmt, vl);
	va_end(vl);
}

void
tf_exit2(struct tf_test *test, int rc)
{

	fprintf(stderr, "test %s: fatal\n", test->t_desc);
#ifdef TF_FORK
	if (test->t_dofork) {
		fflush(stdout);
		fflush(stderr);
		_Exit(rc);
	}
#endif
	exit(rc);
}

void
tf_assert2(struct tf_test *thiz, int cond, const char * scond,
    const char *file, int line)
{

	if (cond) return;
	fprintf(stderr, "test `%s' (%s:%d): assertion failed: %s\n",
	    thiz->t_desc, file, line, scond);
	thiz->t_exit(TF_FAIL);
}

#ifdef TF_FORK
int
tf_fork(void)
{

	return (fork());
}
int
tf_waitpid(int pid, int *status, int options)
{

	return (waitpid(pid, status, options));
}
#endif
