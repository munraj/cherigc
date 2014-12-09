#ifndef _GC_DEBUG_H_
#define _GC_DEBUG_H_

#include "gc_cheri.h"
#include "gc.h"

#define	X_GC_LOG							\
	X(GC_LOG_ERROR,	0, "error")					\
	X(GC_LOG_WARN,	1, "warn")					\
	X(GC_LOG_DEBUG,	2, "debug")
#define	gc_error(...) gc_log(GC_LOG_ERROR, __FILE__, __LINE__, __VA_ARGS__)
#define	gc_warn(...) gc_log(GC_LOG_WARN, __FILE__, __LINE__, __VA_ARGS__)
#define	gc_debug(...) gc_log(GC_LOG_DEBUG, __FILE__, __LINE__, __VA_ARGS__)

enum gc_debug_defines {
#define	X(cnst, value, ...) cnst=value,
	X_GC_LOG
#undef	X
};

void		 gc_log(int _severity, const char *_file, int _line,
		    const char *_format, ...);
const char	*gc_log_severity_str(int _severity);
const char	*gc_cap_str(_gc_cap void *_ptr);
/* Prints the map of a block table, without outputting the free blocks. */
void		 gc_print_map(_gc_cap struct gc_btbl *btbl);

#define	SZFORMAT(n) SZDIV(n), SZPRE(n)

#define	SZPRE(n) (							\
  (n) >= (1024 * 1024 * 1024) ? 'G' :					\
  (n) >= (1024 * 1024) ? 'M' :						\
  (n) >= 1024 ? 'k' :							\
  ' '									\
)

#define	SZDIV(n) (							\
  (n) >= (1024 * 1024 * 1024) ? (n) / (1024 * 1024 * 1024) :		\
  (n) >= (1024 * 1024) ? (n) / (1024 * 1024) :				\
  (n) >= 1024 ? (n) / 1024 :						\
  (n)									\
)

#endif /* !_GC_DEBUG_H_ */
