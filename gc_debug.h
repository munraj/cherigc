#ifndef _GC_DEBUG_H_
#define _GC_DEBUG_H_

#include "gc.h"
#include "gc_cheri.h"
#include "gc_vm.h"

#define	X_GC_LOG							\
	X(GC_LOG_ERROR,	0, "error")					\
	X(GC_LOG_WARN,	1, "warn")					\
	X(GC_LOG_DEBUG,	2, "debug")
#define	gc_error(...) gc_log(GC_LOG_ERROR, __FILE__, __LINE__, __VA_ARGS__)
#define	gc_warn(...) gc_log(GC_LOG_WARN, __FILE__, __LINE__, __VA_ARGS__)
#define	gc_debug(...) gc_log(GC_LOG_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define GC_NOTREACHABLE_ERROR() gc_error("NOTREACHABLE")

enum gc_debug_defines {
#define	X(cnst, value, ...) cnst=value,
	X_GC_LOG
#undef	X
};

extern int	 gc_debug_indent_level;
#define	GC_DEBUG_INDENT_STR	"\t>>> "

void		 gc_debug_indent(int incr);
void		 gc_log(int _severity, const char *_file, int _line,
		    const char *_format, ...);
const char	*gc_log_severity_str(int _severity);
const char	*gc_cap_str(_gc_cap void *_ptr);
/* Prints the map of a block table, without outputting the free blocks. */
void		 gc_print_map(_gc_cap struct gc_btbl *_btbl);
const char	*gc_ve_prot_str(uint32_t prot);
/* Prints memory map info. */
void		 gc_print_vm_tbl(_gc_cap struct gc_vm_tbl *_vt);
void		 gc_print_siginfo_status(void);

/* Initializes the memory for this object with a magic pattern, in
 * order to aid debugging of memory corruption.
 *
 * It is assumed that the length of _obj specifies how much of the
 * object is actually to be used by a client, and that _roundsz is the
 * actual size of the object as allocated internally, so that the
 * difference can be filled with a different pattern.
 */
void		 gc_fill_used_mem(_gc_cap void *_obj, size_t _roundsz);
void		 gc_fill_free_mem(_gc_cap void *_obj);
void		 gc_fill(_gc_cap void * _obj, uint32_t _magic);

#define	GC_MAGIC_INIT_USE	0xA110CA7D
#define	GC_MAGIC_INIT_INTERNAL	0x5CAFF01D
#define	GC_MAGIC_INIT_FREE	0xDE1E7ED0

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

/*
 * Use this to print VM entries.
 */
#define GC_DEBUG_VE_FMT						\
		"0x%" PRIx64 "-0x%" PRIx64 ": p=%s sz=%3zu%c t=0x%x gt=0x%x bt=%p"
#define GC_DEBUG_VE_PRI(ve)					\
		    (ve)->ve_start, (ve)->ve_end,		\
		    gc_ve_prot_str((ve)->ve_prot),		\
		    SZFORMAT((ve)->ve_end - (ve)->ve_start),	\
		    (ve)->ve_type, (ve)->ve_gctype,		\
		    (void *)(ve)->ve_bt

#endif /* !_GC_DEBUG_H_ */
