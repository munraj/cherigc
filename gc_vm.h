#ifndef _GC_VM_H_
#define _GC_VM_H_

#include <stdint.h>
#include <stdlib.h>

#include "gc_cheri.h"

struct gc_btbl;

/* A range of page mappings */
struct gc_vm_ent {
	uint64_t	ve_start;	/* start address */
	uint64_t	ve_end;		/* end address */
	uint32_t	ve_prot;	/* protection */
	uint32_t	ve_type;	/* KVME type */
	uint32_t	ve_gctype;	/* GC-specific type */
	_gc_cap struct gc_btbl	*ve_bt;		/* GC block table */
};

#define	GC_VE_PROT_RD		0x00000001UL
#define	GC_VE_PROT_WR		0x00000002UL
#define	GC_VE_PROT_EX		0x00000004UL

/*
 * Memory is managed by the GC.
 */
#define GC_VE_TYPE_MANAGED	0x00000001UL

/*
 * How much memory to allocate to vt_bt_hp (aka max total size of
 * block table map and tag bitmaps).
 */
#define	GC_BT_HP_SZ	1048576

struct gc_vm_tbl {
	/* Page mapping entries. */
	_gc_cap struct gc_vm_ent	*vt_ent;
	/* Number of allocated entries. */
	size_t				 vt_sz;
	/* Number of valid entries. */
	size_t				 vt_nent;
	/* Block table array. */
	_gc_cap struct gc_btbl		*vt_bt;
	/* Block table heap (for gc_vm_tbl_new_bt). */
	_gc_cap void			*vt_bt_hp;
};

/* Returns GC_SUCC, GC_ERROR or GC_TOO_SMALL. */
int	gc_vm_tbl_update(_gc_cap struct gc_vm_tbl *_vt);
int	gc_vm_tbl_alloc(_gc_cap struct gc_vm_tbl *_vt, size_t _sz);
_gc_cap struct gc_vm_ent	*gc_vm_tbl_find_btbl(
				    _gc_cap struct gc_vm_tbl *_vt,
				    _gc_cap struct gc_btbl *_bt);

_gc_cap struct gc_vm_ent	*gc_vm_tbl_find(
				    _gc_cap struct gc_vm_tbl *_vt,
				    uint64_t _addr);

/* Used internally. */
int	gc_vm_tbl_track(_gc_cap struct gc_vm_tbl *_vt,
	    _gc_cap struct gc_vm_ent *_ve);
int	gc_vm_tbl_new_bt(_gc_cap struct gc_vm_tbl *_vt,
	    _gc_cap struct gc_vm_ent *_ve);
int	gc_vm_tbl_bt_match(_gc_cap struct gc_vm_ent *_ve);

#endif /* !_GC_VM_H_ */
