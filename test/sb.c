/* The actual sandbox. */

#include <machine/cheri.h>
#include <machine/cheric.h>
#include <cheri/cheri_invoke.h>
#include <cheri/cheri_system.h>

#include "sb_param.h"
#include "cheri_gc.h"

int	 printf(const char *, ...);
int	 sprintf(char *, const char *, ...);
char	*strcpy(char *, const char *);
#define	NULL		((void *)0)

int	 test_ll(struct sb_param *sp);

struct node {
	struct node	*p;
	struct node	*n;
	uint8_t			 v[1];
};
static struct node *hd;
static int nmax;
static size_t nsz;
#define PRINTF		printf
#define MALLOC(sz)	GC_MALLOC(sp->sp_gc, (sz))
#define ASSERT(cond, arg)					\
	if (!(cond)) {					\
		PRINTF("Assert failed: `%s':", #cond);	\
		PRINTF arg;				\
		PRINTF(" (line %d)\n", __LINE__);	\
		return (-1);				\
	}
#define	LLHASH(i,j,t,p) (				\
	    (uint8_t)(((i)+(j)+(t)+(p))>>(j))		\
	)

#define	ALLOCATE_JUNK do {				\
	    junk = MALLOC(junksz);			\
	    ASSERT(junk != NULL, (""));			\
	    for (tmp = 0; tmp < junksz / 4; tmp++)	\
		junk[tmp] = junkfill;			\
	} while (0)

unsigned long long
CTOPTR(void *p)
{
	unsigned long long rp;

	rp = cheri_getbase(p) + cheri_getoffset(p);
	return (rp);
}


const char *
pstr(void *p)
{
	static char s[50];

	if (p == (void *)0)
		return (strcpy(s, "null cap"));

	sprintf(s, "b=0x%zx o=0x%zx l=0x%zx t=%d",
	    (size_t)cheri_getbase(p),
	    (size_t)cheri_getoffset(p),
	    (size_t)cheri_getlen(p),
	    (int)cheri_gettag(p));

	return (s);
}

int
try_use(struct sb_param *sp)
{
	struct node *p, *t;
	int i, j;
	uint8_t h;

	/*printf("&p is %s\n", pstr(&p));
	printf("p is %s\n", pstr(p));
	printf("p status: %d\n", cheri_gc_status_c(sp->sp_gc, p));*/

	/* Check LL. */
	p = NULL;
	t = hd;
	for (i = 0; i < nmax; i++) {
		PRINTF("checking linked list node %d\n", i);
		ASSERT(t != NULL, (""));
		PRINTF("current: %llx, actual prev: %llx, stored prev: %llx, stored next: %llx\n",
		   CTOPTR(t), CTOPTR(p), CTOPTR(t->p), CTOPTR(t->n));
		ASSERT(t->p == p, (""));
		for (j = 0; j < nsz - sizeof(struct node); j++) {
			h = LLHASH(i, j,
			    (int)(uintptr_t)t, (int)(uintptr_t)t->p);
			if (t->v[j] != h)
				PRINTF("position %d: stored hash: 0x%x; actual hash: 0x%x\n", j, t->v[j], h);
			ASSERT(t->v[j] == h, (""));
		}
		p = t;
		t = t->n;
	}
	return (0);
}

int
init(struct sb_param *sp)
{
	int rc;

	/*rc = cheri_gc_alloc_c(sp->sp_gc, &p, 500);
	printf("result: %d\n", rc);
	printf("p: %s\n", pstr(p));
	printf("p status: %d\n", cheri_gc_status_c(sp->sp_gc, p));*/

	rc = test_ll(sp);
	return (rc);
}

int
invoke(struct sb_param *sp)
{

	printf("sandbox invoked\n");
	printf("...with operation %d\n", sp->sp_op);

	switch (sp->sp_op) {
	case OP_INIT:
		return (init(sp));
	case OP_TRY_USE:
		return (try_use(sp));
	default:
		return (-1);
	}
}

void *
GC_MALLOC(struct cheri_object gc, size_t sz)
{
	void *p;
	int rc;

	rc = cheri_gc_alloc_c(gc, &p, sz);
	if (rc != 0)
		return (NULL);

	return (p);
}

int
test_ll(struct sb_param *sp)
{
	struct node **np;
	struct node *p, *t;
	uint32_t *junk;
	size_t junksz, tmp;
	int i, j, junkn;
	uint32_t junkfill;

	/* Configurable */
	nmax = 10;
	nsz = 201;
	junksz = 200;
	junkfill = 0x0BADDEAD;
	junkn = 2;

	if (nsz < sizeof(struct node))
		nsz = sizeof(struct node);

	/* Align junk size to 4 bytes */
	junksz &= ~(size_t)3;

	/* Allocate LL. */
	np = &hd;
	p = NULL;
	for (i = 0; i < nmax; i++) {
		for (j = 0; j < junkn; j++)
			ALLOCATE_JUNK;
		*np = MALLOC(nsz);
		t = *np;
		ASSERT(t != NULL, ("i is %d\n", i));

		if (i == nmax / 2) {
			PRINTF("** storing special cap %s for later revocation\n", pstr(*np));
			*sp->sp_cap1 = *np;
		}

		ALLOCATE_JUNK;
		ASSERT(t != NULL, (""));
		ASSERT(cheri_getlen(t) >= nsz, (""));
		t->p = p;
		p = t;
		np = &t->n;

		for (j = 0; j < nsz - sizeof(struct node); j++) {
			t->v[j] = LLHASH(i, j,
			    (int)(uintptr_t)t, (int)(uintptr_t)t->p);
		}
		ALLOCATE_JUNK;
	}

	return (try_use(sp));
}
