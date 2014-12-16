#ifndef _GC_VM_H_
#define _GC_VM_H_

#include <stdint.h>
#include <stdlib.h>

#include "gc_cheri.h"

/* A range of page mappings */
struct gc_vm_ent {
	uint64_t	ve_start;	/* start address */
	uint64_t	ve_end;		/* end address */
	uint32_t	ve_prot;	/* protection */
	uint32_t	ve_type;	/* GC-specific type */
};

#define	GC_VE_PROT_RD		0x00000001UL
#define	GC_VE_PROT_WR		0x00000002UL
#define	GC_VE_PROT_EX		0x00000004UL

/* Memory is managed by the GC. */
#define GC_VE_TYPE_MANAGED	0x00000001UL

struct gc_vm_tbl {
	/* Page mapping entries. */
	_gc_cap struct gc_vm_ent	*vt_ent;
	/* Number of entries. */
	size_t				 vt_sz;
};

int	gc_vm_tbl_get(_gc_cap struct gc_vm_tbl *_vt);
int	gc_vm_tbl_alloc(_gc_cap struct gc_vm_tbl *_vt, size_t _sz);

#endif /* !_GC_VM_H_ */
