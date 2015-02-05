#ifndef _GC_CHERI_H_
#define _GC_CHERI_H_

#include <stdint.h>
#include <machine/cheri.h>
#include <machine/cheric.h>
#include <setjmp.h>
#include <signal.h>

#define	_gc_cap			__capability
#define	gc_cheri_getbase(x)	((uint64_t)cheri_getbase(x))
#define	gc_cheri_getlen(x)	((uint64_t)cheri_getlen(x))
#define	gc_cheri_getoffset(x)	((uint64_t)cheri_getoffset(x))
#define	gc_cheri_gettype(x)	((uint64_t)cheri_gettype(x))
#define	gc_cheri_gettag(x)	((int)cheri_gettag(x))
#define	gc_cheri_getsealed(x)	((int)cheri_getsealed(x))
#define	gc_cheri_incbase	cheri_incbase
#define	gc_cheri_ptr		cheri_ptr
#define	gc_cheri_setlen		cheri_setlen
#define	gc_cheri_setoffset	cheri_setoffset
#define	gc_cheri_cleartag	cheri_cleartag
#define	gc_cheri_seal		cheri_seal
#define	gc_cheri_unseal		cheri_unseal

#define	gc_cap_addr(x)		(gc_cheri_ptr((void*)(x), \
				    sizeof(_gc_cap void *)))

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
 *
 * XXX: now, we also DO save+restore+scan C1-C16, because the compiler doesn't
 * save caller save regs to the stack.
 */

#define	GC_INVALIDATE_UNUSED_REGS		\
	/*GC_INVALIDATE_REG(1);			\
	GC_INVALIDATE_REG(2);			\
	GC_INVALIDATE_REG(4);			\
	GC_INVALIDATE_REG(5);			\
	GC_INVALIDATE_REG(6);			\
	GC_INVALIDATE_REG(7);			\
	GC_INVALIDATE_REG(8);			\
	GC_INVALIDATE_REG(9);			\
	GC_INVALIDATE_REG(10);			\
	GC_INVALIDATE_REG(11);			\
	GC_INVALIDATE_REG(12);			\
	GC_INVALIDATE_REG(13);			\
	GC_INVALIDATE_REG(14);			\
	GC_INVALIDATE_REG(15);			\
	GC_INVALIDATE_REG(16);*/		\
	do {} while (0)

#define	GC_NUM_SAVED_REGS	25

#define	GC_SAVE_REGS(buf)			\
	GC_SAVE_REG(17, buf, 0);		\
	GC_SAVE_REG(18, buf, 32);		\
	GC_SAVE_REG(19, buf, 64);		\
	GC_SAVE_REG(20, buf, 96);		\
	GC_SAVE_REG(21, buf, 128);		\
	GC_SAVE_REG(22, buf, 160);		\
	GC_SAVE_REG(23, buf, 192);		\
	GC_SAVE_REG(24, buf, 224);		\
	GC_SAVE_REG(25, buf, 256);		\
	GC_SAVE_REG(26, buf, 288);		\
	GC_SAVE_REG(1, buf, 320);		\
	GC_SAVE_REG(2, buf, 352);		\
	GC_SAVE_REG(4, buf, 384);		\
	GC_SAVE_REG(5, buf, 416);		\
	GC_SAVE_REG(6, buf, 448);		\
	GC_SAVE_REG(7, buf, 480);		\
	GC_SAVE_REG(8, buf, 512);		\
	GC_SAVE_REG(9, buf, 544);		\
	GC_SAVE_REG(10, buf, 576);		\
	GC_SAVE_REG(11, buf, 608);		\
	GC_SAVE_REG(12, buf, 640);		\
	GC_SAVE_REG(13, buf, 672);		\
	GC_SAVE_REG(14, buf, 704);		\
	GC_SAVE_REG(15, buf, 736);		\
	GC_SAVE_REG(16, buf, 768);		\

#define	GC_RESTORE_REGS(buf)			\
	GC_RESTORE_REG(17, buf, 0);		\
	GC_RESTORE_REG(18, buf, 32);		\
	GC_RESTORE_REG(19, buf, 64);		\
	GC_RESTORE_REG(20, buf, 96);		\
	GC_RESTORE_REG(21, buf, 128);		\
	GC_RESTORE_REG(22, buf, 160);		\
	GC_RESTORE_REG(23, buf, 192);		\
	GC_RESTORE_REG(24, buf, 224);		\
	GC_RESTORE_REG(25, buf, 256);		\
	GC_RESTORE_REG(26, buf, 288);		\
	GC_RESTORE_REG(1, buf, 320);		\
	GC_RESTORE_REG(2, buf, 352);		\
	GC_RESTORE_REG(4, buf, 384);		\
	GC_RESTORE_REG(5, buf, 416);		\
	GC_RESTORE_REG(6, buf, 448);		\
	GC_RESTORE_REG(7, buf, 480);		\
	GC_RESTORE_REG(8, buf, 512);		\
	GC_RESTORE_REG(9, buf, 544);		\
	GC_RESTORE_REG(10, buf, 576);		\
	GC_RESTORE_REG(11, buf, 608);		\
	GC_RESTORE_REG(12, buf, 640);		\
	GC_RESTORE_REG(13, buf, 672);		\
	GC_RESTORE_REG(14, buf, 704);		\
	GC_RESTORE_REG(15, buf, 736);		\
	GC_RESTORE_REG(16, buf, 768);		\

#define	GC_SAVE_REG(indx, buf, offset)		\
	__asm__ __volatile__ (			\
		"csc $c" #indx ", $zero, " #offset "($c" #buf ")" : : : \
	    "memory"				\
	)

#define	GC_RESTORE_REG(indx,buf,offset)		\
	__asm__ __volatile__ (			\
		"clc $c" #indx ", $zero, " #offset "($c" #buf ")" : : : \
	    "memory" \
	)

#define	GC_INVALIDATE_REG(indx)			\
	__asm__ __volatile__ (			\
		"ccleartag $c" #indx ", $c" #indx : : : "memory"	\
	)

_gc_cap void	*gc_get_stack(void);
_gc_cap void	*gc_get_stack_top(void);
_gc_cap void	*gc_get_stack_bottom(void);
_gc_cap void	*gc_get_static_region(void);
void		 gc_sigsegv_handler(int _p);

extern jmp_buf	gc_jmp_buf;
extern void	(*gc_oldfn)(int);

_gc_cap void	*gc_unseal(_gc_cap void *_obj);


#endif /* !_GC_CHERI_H_ */
