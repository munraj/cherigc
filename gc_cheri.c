#include <sys/types.h>
#include <sys/sysctl.h>
#include <unistd.h>

#include <stdlib.h>

#include "gc_cheri.h"
#include "gc_debug.h"
#include "gc.h"

/*
 * Defined by FreeBSD.
 * This is the top of the data section.
 */
extern char	end;

jmp_buf		gc_jmp_buf;
void		(*gc_oldfn)(int);

/*
 * This uses an undocumented FreeBSD sysctl.
 * Boehm uses it too.
 */
_gc_cap void *
gc_get_stack_bottom(void)
{
	_gc_cap void *ret;
	int name[2];
	void * p;
	size_t lenp;

	name[0] = CTL_KERN;
	name[1] = KERN_USRSTACK;
	p = NULL;
	lenp = sizeof(p);
	if (sysctl(name, (sizeof(name))/(sizeof(name[0])),
	    &p, &lenp, NULL, 0)) {
		gc_error("failed to get stack bottom");
		return (NULL);
	}
	/* XXX: Length? */
	ret = (_gc_cap void *)p;
	gc_debug("found bottom of stack: %s", gc_cap_str(ret));
	return (ret);
}

void
gc_sigsegv_handler(int p)
{

	longjmp(gc_jmp_buf, 1);
}

/*
 * Determine bottom of data section.
 * Keep decrementing until we segfault.
 * Technically undefined behaviour, and non-portable.
 * We break out of infinite segfaulting by using setjmp/longjmp.
 */
_gc_cap void *
gc_get_static_region (void)
{
	_gc_cap void *ret;
	char *base;
	static char *good_base;
	static char *top;
	size_t len;
	int rc;
	char deref;

	rc = setjmp(gc_jmp_buf);
	if (!rc) {
		/* Direct return from setjmp. */
		top = GC_ALIGN(&end);
		gc_oldfn = signal(SIGSEGV, gc_sigsegv_handler);
		if (gc_oldfn == SIG_ERR) {
			gc_error("SIG_ERR registering handler");
			return (NULL);
		}
		base = top;
		good_base = NULL;
		while (base != NULL) {
			good_base = base;
			base -= sizeof(_gc_cap void *);
			deref = *base;
			/* Force dereference. */
			__asm__ __volatile__ (
				"move $1, %0" :: "r"(deref) : "memory", "$1"
			);
		}
	} else {
		/* Restore previous handler. */
		gc_oldfn = signal(SIGSEGV, gc_oldfn);
		if (gc_oldfn == SIG_ERR) {
			gc_error("SIG_ERR restoring gc_oldfn");
			return (NULL);
		}
		len = top - good_base;
		ret = gc_cheri_ptr(good_base, len);
		gc_debug("found bottom of static region: %s", gc_cap_str(ret));
		return ret;
	}
	/* Never segfaulted. */
	gc_error("decremented base to 0");
	return (NULL);
}
