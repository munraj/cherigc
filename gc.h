#ifndef _GC_H_
#define _GC_H_

#include <stdlib.h>

#define __gc_capability __capability
#define gc_cheri_ptr		cheri_ptr
#define gc_cheri_getbase		cheri_getbase
#define gc_cheri_getoffset		cheri_getoffset
#define gc_cheri_getlen		cheri_getlen

void gc_init (void);
__gc_capability void * gc_malloc (size_t sz);
void gc_free (__gc_capability void * ptr);
__gc_capability void * gc_alloc_internal (size_t sz);

struct gc_state_s
{
	__gc_capability void * heap;
};

extern __gc_capability struct gc_state_s * gc_state;

#define GC_INIT_HEAPSZ ((size_t) (64*1024))

#define X_GC_LOG \
	X(GC_LOG_ERROR,	0, "error") \
	X(GC_LOG_WARN,	1, "warn") \
	X(GC_LOG_DEBUG,	2, "debug")
#define gc_error(...) gc_log(GC_LOG_ERROR, __FILE__, __LINE__, \
		__VA_ARGS__)
#define gc_warn(...) gc_log(GC_LOG_WARN, __FILE__, __LINE__, \
		__VA_ARGS__)
#define gc_debug(...) gc_log(GC_LOG_DEBUG, __FILE__, __LINE__, \
		__VA_ARGS__)
void gc_log (int severity, const char * file, int line,
		const char * format, ...);
const char * gc_log_severity_str (int severity);
const char * gc_cap_str (__gc_capability void * ptr);

enum gc_defines
{
#define X(cnst,value,...) \
	cnst=value,
X_GC_LOG
#undef X
};

#endif /* _GC_H_ */
