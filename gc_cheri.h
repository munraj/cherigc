#ifndef _GC_CHERI_H_
#define _GC_CHERI_H_

#include <stdint.h>
#include <machine/cheri.h>
#include <machine/cheric.h>

#define __gc_capability __capability
#define gc_cheri_getbase(x)		((uint64_t)cheri_getbase(x))
#define gc_cheri_getlen(x)		((uint64_t)cheri_getlen(x))
#define gc_cheri_getoffset(x)	((uint64_t)cheri_getoffset(x))
#define gc_cheri_gettag(x)		((int)cheri_gettag(x))
#define gc_cheri_incbase		cheri_incbase
#define gc_cheri_ptr		cheri_ptr
#define gc_cheri_setlen		cheri_setlen
#define gc_cheri_setoffset		cheri_setoffset
#define gc_cheri_cleartag		cheri_cleartag

/*
 * When the GC is entered:
 * C17 - C24 are saved, because they are callee-save.
 * C25 is also saved (because unknown whether to preserve or not).
 * C26 (IDC) is also saved (because unknown whether to preserve or not).
 *
 * When the GC returns:
 * C1 - C23 are invalidated, because they are caller-save, and to prevent the leakage
 * of GC information and/or collected objects.
 * C17 - C24 are restored (possibly invalidated).
 * C25 is restored (possibly invalidated).
 * C26 is restored (possibly invalidated).
 *
 * Therefore the GC only scans objects referenced by, and possibly invalidates, the
 * capabilities in C17-C24, C25, C26 (recursively).
 */

#define GC_INVALIDATE_UNUSED_REGS \
	GC_INVALIDATE_REG(1); \
	GC_INVALIDATE_REG(2); \
	GC_INVALIDATE_REG(4); \
	GC_INVALIDATE_REG(5); \
	GC_INVALIDATE_REG(6); \
	GC_INVALIDATE_REG(7); \
	GC_INVALIDATE_REG(8); \
	GC_INVALIDATE_REG(9); \
	GC_INVALIDATE_REG(10); \
	GC_INVALIDATE_REG(11); \
	GC_INVALIDATE_REG(12); \
	GC_INVALIDATE_REG(13); \
	GC_INVALIDATE_REG(14); \
	GC_INVALIDATE_REG(15); \
	GC_INVALIDATE_REG(16);

#define GC_NUM_SAVED_REGS	10

#define GC_SAVE_REGS(buf) \
	GC_SAVE_REG(17, buf, 0); \
	GC_SAVE_REG(18, buf, 32); \
	GC_SAVE_REG(19, buf, 64); \
	GC_SAVE_REG(20, buf, 96); \
	GC_SAVE_REG(21, buf, 128); \
	GC_SAVE_REG(22, buf, 160); \
	GC_SAVE_REG(23, buf, 192); \
	GC_SAVE_REG(24, buf, 224); \
	GC_SAVE_REG(25, buf, 256); \
	GC_SAVE_REG(26, buf, 288);

#define GC_RESTORE_REGS(buf) \
	GC_RESTORE_REG(17, buf, 0); \
	GC_RESTORE_REG(18, buf, 32); \
	GC_RESTORE_REG(19, buf, 64); \
	GC_RESTORE_REG(20, buf, 96); \
	GC_RESTORE_REG(21, buf, 128); \
	GC_RESTORE_REG(22, buf, 160); \
	GC_RESTORE_REG(23, buf, 192); \
	GC_RESTORE_REG(24, buf, 224); \
	GC_RESTORE_REG(25, buf, 256); \
	GC_RESTORE_REG(26, buf, 288);

#define GC_SAVE_REG(indx,buf,offset) \
	__asm__ __volatile__ \
	( \
		"csc $c" #indx ", $zero, " #offset "($c" #buf ")" : : : "memory" \
	)

#define GC_RESTORE_REG(indx,buf,offset) \
	__asm__ __volatile__ \
	( \
		"clc $c" #indx ", $zero, " #offset "($c" #buf ")" : : : "memory" \
	)

#define GC_INVALIDATE_REG(indx) \
	__asm__ __volatile__ \
	( \
		"ccleartag $c" #indx ", $c" #indx : : : "memory" \
	)

__gc_capability void *
gc_get_stack (void);

__gc_capability void *
gc_get_stack_top (void);

__gc_capability void *
gc_get_stack_bottom (void);

__gc_capability void *
gc_get_static_region (void);

#endif /* _GC_CHERI_H_ */
