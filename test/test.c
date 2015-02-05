#ifdef GC_USE_LIBPROCSTAT
#include <kvm.h>
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/user.h>
#include <libprocstat.h>
#include <unistd.h>
#endif /* GC_USE_LIBPROCSTAT */

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gc.h>
#include <gc_cmdln.h>
#include <gc_debug.h>

#include "framework.h"
#include "test_sb.h"

tf_sig_fn	siginfo_hnd;
testfn		test_gc_init;
#ifdef GC_USE_LIBPROCSTAT
testfn		test_procstat;
#endif
testfn		test_gc_malloc;
testfn		test_ll;
testfn		test_store;

struct tf_test	tests[] = {
	{.t_fn = test_gc_init, .t_desc = "gc initialization"},
#ifdef GC_USE_LIBPROCSTAT
	/*{.t_fn = test_procstat, .t_desc = "libprocstat", .t_dofork = 0},*/
#endif
	//{.t_fn = test_ll, .t_desc = "linked list", .t_dofork = 0},
	//{.t_fn = test_store, .t_desc = "ptr store", .t_dofork = 0},
	/*{.t_fn = test_gc_malloc, .t_desc = "gc malloc", .t_dofork = 0},*/
	{.t_fn = test_sb, .t_desc = "sandboxing", .t_dofork = 0},
	{.t_fn = NULL},
};


struct node;
struct node {
	_gc_cap struct node	*p;
	_gc_cap struct node	*n;
	uint8_t			 v[1];
};

void
siginfo_hnd(int sig)
{

	gc_print_siginfo_status();
	gc_cmdln();
}

int
main()
{
	struct tf_init init;
	struct tf_result result;

	memset(&result, 0, sizeof(result));
	init.i_siginfo_hnd = &siginfo_hnd;
	tf_init(&init);
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
	imax = 10 * 1024 * 1024;
	istep = 0;
	imult = 2;
	#define JMIN 1
	#define JMAX 100
	#define JSTEP 1
	#define JMULT 1

	obj = gc_malloc(100);
	thiz->t_assert(obj != NULL);
	thiz->t_assert(gc_cheri_getlen(obj) >= 100);

	for (i = imin; i <= imax; i = (i + istep) * imult) {
		jmin = JMIN;
		jmax = JMAX;
		jstep = JSTEP;
		jmult = JMULT;
		for (j = jmin; j <= jmax; j = (j + jstep) * jmult) {
			thiz->t_pf("trying allocation of size %d%c\n",
			    SZFORMAT(i));
			obj = gc_malloc(i);
			thiz->t_assert(obj != NULL);
			thiz->t_assert(gc_cheri_getlen(obj) >= i);
		}
	}
	return (TF_SUCC);
}

#ifdef GC_USE_LIBPROCSTAT
int
test_procstat(struct tf_test *thiz)
{
	struct procstat *ps;
	struct kinfo_vmentry *kv;
	struct kinfo_proc *kp;
	unsigned cnt, i;

	ps = procstat_open_sysctl();
	thiz->t_assert(ps != NULL);
	cnt = 0;
	kp = procstat_getprocs(ps, KERN_PROC_PID, getpid(), &cnt);
	thiz->t_assert(kp != NULL);
	thiz->t_pf("getprocs retrieved %u procs\n", cnt);
	kv = procstat_getvmmap(ps, kp, &cnt);
	thiz->t_assert(kv != NULL);
	for (i = 0; i < cnt; i++) {
		thiz->t_pf("[%u] type: %d, "
		    "start: 0x%"PRIx64", end: 0x%"PRIx64" "
		    "(sz: %"PRIu64"%c), prot: 0x%x\n",
		    i, kv[i].kve_type, kv[i].kve_start, kv[i].kve_end,
		    SZFORMAT(kv[i].kve_end - kv[i].kve_start),
		    kv[i].kve_protection);
	}
	procstat_freevmmap(ps, kv);
	procstat_freeprocs(ps, kp);
	procstat_close(ps);

	return (TF_SUCC);
}
#endif /* GC_USE_LIBPROCSTAT */

int
test_ll(struct tf_test *thiz)
{
#define	LLHASH(i,j,t,p) (			\
	    (uint8_t)(((i)+(j)+(t)+(p))>>(j))	\
	)

#define	ALLOCATE_JUNK do {				\
	    junk = gc_malloc(junksz);			\
	    thiz->t_assert(junk != NULL);		\
	    for (tmp = 0; tmp < junksz / 4; tmp++)	\
		junk[tmp] = junkfill;			\
	} while (0)

	_gc_cap struct node * _gc_cap *np;
	_gc_cap struct node *hd, *p, *t;
	_gc_cap uint32_t *junk;
	size_t nsz, junksz, tmp;
	int nmax, i, j, junkn;
	uint32_t junkfill;
	uint8_t h;

	/* Configurable */
	nmax = 100;
	nsz = 201;
	junksz = 200;
	junkfill = 0x0BADDEAD;
	junkn = 3;

	if (nsz < sizeof(struct node))
		nsz = sizeof(struct node);

	/* Align junk size to 4 bytes */
	junksz &= ~(size_t)3;

	/* Allocate LL. */
	np = gc_cheri_ptr(&hd, sizeof(hd));
	p = NULL;
	for (i = 0; i < nmax; i++) {
		for (j = 0; j < junkn; j++)
			ALLOCATE_JUNK;
		*np = gc_malloc(nsz);
		t = *np;
		thiz->t_assert(t != NULL);
		ALLOCATE_JUNK;
		thiz->t_assert(t != NULL);
		thiz->t_assert(gc_cheri_getlen(t) >= nsz);
		t->p = p;
		p = t;
		np = &t->n;

		for (j = 0; j < nsz - sizeof(struct node); j++) {
			t->v[j] = LLHASH(i, j,
			    (int)(uintptr_t)t, (int)(uintptr_t)t->p);
		}
		ALLOCATE_JUNK;
	}

	/* Check LL. */
	p = NULL;
	t = hd;
	for (i = 0; i < nmax; i++) {
		thiz->t_pf("checking linked list node %d\n", i);
		thiz->t_assert(t != NULL);
		thiz->t_pf("current: %p, actual prev: %p, stored prev: %p, stored next: %p\n",
		   (void *)t, (void *)p, (void *)t->p, (void *)t->n);
		thiz->t_assert(t->p == p);
		for (j = 0; j < nsz - sizeof(struct node); j++) {
			h = LLHASH(i, j,
			    (int)(uintptr_t)t, (int)(uintptr_t)t->p);
			if (t->v[j] != h)
				thiz->t_pf("position %d: stored hash: 0x%x; actual hash: 0x%x\n", j, t->v[j], h);
			thiz->t_assert(t->v[j] == h);
		}
		p = t;
		t = t->n;
	}

	return (TF_SUCC);
}

int
test_store(struct tf_test *thiz)
{
	_gc_cap struct node *n;
	size_t i;

	n = gc_malloc(2004);
	n->p = gc_cheri_ptr((void *)0x5678, 0x7890);
	for (i=0; i<2004-sizeof(*n); i++)
		n->v[i] = 99;
	gc_malloc(2005);
	gc_malloc(2005);
	gc_malloc(2005);
	gc_malloc(2005);
	gc_malloc(2005);
	gc_malloc(2005);
	gc_malloc(2005);
	gc_malloc(2005);
	gc_malloc(2005);
	gc_malloc(2005);
	gc_malloc(2005);
	gc_malloc(2005);
	gc_malloc(2005);
	gc_malloc(2005);
	n->n = gc_malloc(2006);
	for (i=0; i<2004-sizeof(*n); i++)
		n->n->v[i] = 22;
	printf("n: %s\n", gc_cap_str(n));
	printf("n->n: %s\n", gc_cap_str(n->n));
	gc_malloc(2005);
	gc_malloc(2005);
	gc_malloc(2005);
	gc_malloc(2005);
	gc_malloc(2005);
	gc_malloc(2005);
	gc_malloc(2005);
	gc_malloc(2005);
	gc_malloc(2005);
	gc_malloc(2005);
	gc_malloc(2005);
	gc_malloc(2005);
	//printf("addr: 0x%llx\n", &n); /* force n to stack */

	/*for (i = 0; i < 1000; i++)
	{
		n->n = gc_malloc(254);
		n = n->n;
	}*/
	gc_extern_collect();
	printf("n: %s\n", gc_cap_str(n));
	gc_extern_collect();
	printf("n: %s\n", gc_cap_str(n));
	gc_extern_collect();

	printf("n: %s\n", gc_cap_str(n));
	printf("n->n: %s\n", gc_cap_str(n->n));
	printf("n->p: %s\n", gc_cap_str(n->p));
	for (i=0; i<2004-sizeof(*n); i++)
		if (n->v[i] != 99) printf("n->v[%zu] is not 99!\n", i);
	for (i=0; i<2004-sizeof(*n); i++)
		if (n->n->v[i] != 22) printf("n->n->v[%zu] = 0x%x is not 22!\n", i, n->n->v[i]);
	return (TF_SUCC);
}
