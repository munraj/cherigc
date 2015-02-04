/* The actual sandbox. */

#include <machine/cheri.h>
#include <machine/cheric.h>
#include <cheri/cheri_invoke.h>
#include <cheri/cheri_system.h>

#include "sb_param.h"
#include "cheri_gc.h"

void *p;

const char *
pstr(void *p)
{
	char s[50];

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
	char *s;

	printf("p is %s\n", pstr(p));
	return (0);
}

int
init(struct sb_param *sp)
{
	int rc;

	rc = cheri_gc_alloc_c(sp->sp_gc, &p, 500);
	printf("result: %d\n", rc);
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
