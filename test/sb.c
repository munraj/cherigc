/* The actual sandbox. */

#include <machine/cheri.h>
#include <cheri/cheri_invoke.h>
#include <cheri/cheri_system.h>

int
invoke(void *param)
{
	const char *s;

	s = "hello from sandbox...\n";
	cheri_system_puts(s);

	return (5679);
}
