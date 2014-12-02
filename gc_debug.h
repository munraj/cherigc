#ifndef _GC_DEBUG_H_
#define _GC_DEBUG_H_

#include "gc_cheri.h"
#include "gc.h"

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

enum gc_debug_defines
{
#define X(cnst,value,...) \
	cnst=value,
X_GC_LOG
#undef X
};

/*
 * Prints the map of a master block table, without outputting the free
 * blocks.
 */
void
gc_print_map (__gc_capability gc_mtbl * mtbl);

#endif /* _GC_DEBUG_H_ */
