#include "gc_cheri.h"
#include "gc_debug.h"
#include "gc.h"
#include <stdlib.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/sysctl.h>
__gc_capability void *
gc_get_stack_bottom (void)
{
	/*
   * This uses an undocumented FreeBSD sysctl.
   * Boehm uses it too.
   */
	int name[2];
	void * p;
	size_t lenp;
	__gc_capability void * ret;
	name[0] = CTL_KERN;
	name[1] = KERN_USRSTACK;
	p = NULL;
	lenp = sizeof p;
	if (sysctl(name, (sizeof name)/(sizeof name[0]),
				&p, &lenp, NULL, 0))
	{
		gc_error("failed to get stack bottom");
		return NULL;
	}
	/* XXX: length? */
	ret = (__gc_capability void *)p;
	gc_debug("found bottom of stack: %s", gc_cap_str(ret));
	return ret;
}

/*
 * Defined by FreeBSD.
 * This is the top of the data section.
 */
extern char end;

/*
 * Determine bottom of data section.
 * Keep decrementing until we segfault.
 * Technically undefined behaviour, and non-portable.
 * We break out of infinite segfaulting by using setjmp/longjmp.
 */
#include <signal.h>
#include <setjmp.h>
static jmp_buf gc_jmp_buf;

static void
gc_sigsegv_handler (int p)
{
	longjmp(gc_jmp_buf, 1);
}

__gc_capability void *
gc_get_static_region (void)
{
	int rc;
	char * base;
	char deref;
	size_t len;
	static void (*oldfn)(int);
	static char * good_base;
	static char * top;
	__gc_capability void * ret;

	rc = setjmp(gc_jmp_buf);
	if (!rc) /* direct return from setjmp */
	{
		top = GC_ALIGN(&end);
		oldfn = signal(SIGSEGV, gc_sigsegv_handler);
		if (oldfn == SIG_ERR)
		{
			gc_error("SIG_ERR registering handler");
			return NULL;
		}
		base = top;
		good_base = NULL;
		while (base)
		{
			good_base = base;
			base -= sizeof(__gc_capability void *);
			deref = *base;
			/* force dereference */
			__asm__ __volatile__ (
				"move $1, %0" :: "r"(deref) : "memory", "$1"
			);
		}
	}
	else
	{
		/* restore previous handler */
		oldfn = signal(SIGSEGV, oldfn);
		if (oldfn == SIG_ERR)
		{
			gc_error("SIG_ERR restoring oldfn");
			return NULL;
		}
		len = top - good_base;
		ret = gc_cheri_ptr(good_base, len);
		gc_debug("found bottom of static region: %s", gc_cap_str(ret));
		return ret;
	}
	/* never segfaulted */
	gc_error("decremented base to 0");
	return NULL;
}
