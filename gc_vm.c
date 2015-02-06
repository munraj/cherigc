#ifdef GC_USE_LIBPROCSTAT
#include <kvm.h>
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/user.h>
#include <libprocstat.h>
#include <unistd.h>
#endif

#include "gc.h"
#include "gc_debug.h"
#include "gc_vm.h"

int
gc_vm_tbl_alloc(_gc_cap struct gc_vm_tbl *vt, size_t sz)
{

	/* XXX: takes up entire page(s)... */
	vt->vt_ent = gc_alloc_internal(sizeof(*vt->vt_ent) * sz);
	if (vt->vt_ent == NULL)
		return (1);
	vt->vt_bt = gc_alloc_internal(sizeof(*vt->vt_bt) * sz);
	if (vt->vt_bt == NULL)
		return (1);
	vt->vt_sz = sz;
	vt->vt_nent = 0;
	vt->vt_bt_hp = gc_alloc_internal(GC_BT_HP_SZ);
	if (vt->vt_bt_hp == NULL)
		return (1);
	return (0);
}

int
gc_vm_tbl_update(_gc_cap struct gc_vm_tbl *vt)
{
#ifdef GC_USE_LIBPROCSTAT
	struct procstat *ps;
	struct kinfo_vmentry *kv;
	struct kinfo_proc *kp;
	unsigned cnt, i;

	ps = procstat_open_sysctl();
	if (ps == NULL)
		return (GC_ERROR);
	cnt = 0;
	kp = procstat_getprocs(ps, KERN_PROC_PID, getpid(), &cnt);
	if (kp == NULL)
		return (GC_ERROR);
	gc_debug("getprocs retrieved %u procs", cnt);
	kv = procstat_getvmmap(ps, kp, &cnt);
	if (kv == NULL)
		return (GC_ERROR);
	gc_debug("getvmmap retrieved %u entries", cnt);
	if (vt->vt_sz < cnt)
		return (GC_TOO_SMALL);
	vt->vt_nent = cnt;
	for (i = 0; i < vt->vt_nent; i++) {
		vt->vt_ent[i].ve_start = kv[i].kve_start;
		vt->vt_ent[i].ve_end = kv[i].kve_end;
		vt->vt_ent[i].ve_prot = kv[i].kve_protection;
		vt->vt_ent[i].ve_type = kv[i].kve_type;
		vt->vt_ent[i].ve_gctype = 0;
		gc_vm_tbl_track(vt, &vt->vt_ent[i]);
	}
	procstat_freevmmap(ps, kv);
	procstat_freeprocs(ps, kp);
	procstat_close(ps);
	return (GC_SUCC);
#else /* !GC_USE_LIBPROCSTAT */
	return (GC_ERROR);
#endif /* GC_USE_LIBPROCSTAT */
}

int
gc_vm_tbl_track(_gc_cap struct gc_vm_tbl *vt, _gc_cap struct gc_vm_ent *ve)
{
#ifdef GC_USE_LIBPROCSTAT
	size_t i;

	/*
	 * Create or find existing btbl to track this mapping.
	 *
	 * This allows us to track even unmanaged objects.
	 *
	 * First do an O(n) search through the array of btbls,
	 * using the current btbl as a hint (because updates
	 * tend not to reorder entries).
	 */
	
	if (gc_vm_tbl_bt_match(ve) == GC_SUCC)
		return (GC_SUCC);

	/* Find other existing. */
	for (i = 0; i < vt->vt_nent; i++) {
		ve->ve_bt = gc_cheri_incbase(vt->vt_bt,
		    i * sizeof(*vt->vt_bt));
		ve->ve_bt = gc_cheri_setlen(ve->ve_bt,
		    sizeof(*ve->ve_bt));
		if (gc_vm_tbl_bt_match(ve) == GC_SUCC)
			return (GC_SUCC);
	}

	/* Allocate from pool. */
	for (i = 0; i < vt->vt_sz; i++)
		if (!vt->vt_bt[i].bt_valid) {
			ve->ve_bt = gc_cheri_incbase(vt->vt_bt,
			    i * sizeof(*vt->vt_bt));
			ve->ve_bt = gc_cheri_setlen(ve->ve_bt,
			    sizeof(*ve->ve_bt));
			return (gc_vm_tbl_new_bt(vt, ve));
		}

	/* Fail; too many entries unmapped. */
	return (GC_ERROR);
#else /* !GC_USE_LIBPROCSTAT */
	return (GC_ERROR);
#endif /* GC_USE_LIBPROCSTAT */
}

int
gc_vm_tbl_new_bt(_gc_cap struct gc_vm_tbl *vt,
    _gc_cap struct gc_vm_ent *ve)
{
	uint64_t base, len;
	size_t npages, mapsz, tagsz;

	base = ve->ve_start;
	len = ve->ve_end - base;
	npages = len / GC_PAGESZ;
	/* Round up npages to next multiple of 2. */
	npages = (npages + (size_t)1) & ~(size_t)1;
	mapsz = npages / 2;
	tagsz = npages * sizeof(*ve->ve_bt->bt_tags);

	ve->ve_bt->bt_base = gc_cheri_ptr((void *)base, len);
	ve->ve_bt->bt_slotsz = GC_PAGESZ;
	ve->ve_bt->bt_nslots = npages;
	ve->ve_bt->bt_flags = 0;
	ve->ve_bt->bt_valid = 1;
	
	/* Allocate tags and map contiguously from pool. */
	if (gc_cheri_getlen(vt->vt_bt_hp) < mapsz + tagsz)
			return (GC_ERROR);
	ve->ve_bt->bt_map = gc_cheri_setlen(vt->vt_bt_hp, mapsz);
	vt->vt_bt_hp = gc_cheri_incbase(vt->vt_bt_hp, mapsz);
	ve->ve_bt->bt_tags = gc_cheri_setlen(vt->vt_bt_hp, tagsz);
	vt->vt_bt_hp = gc_cheri_incbase(vt->vt_bt_hp, tagsz);

	/* Set entire region as used and unmarked. */
	gc_btbl_set_map(ve->ve_bt, 0, npages - 1, GC_BTBL_USED);

	return (GC_SUCC);
}

int
gc_vm_tbl_bt_match(_gc_cap struct gc_vm_ent *ve)
{
	uint64_t start, end;

	if (ve->ve_bt == NULL)
		return (GC_ERROR);

	if (ve->ve_bt->bt_valid == 0)
		return (GC_ERROR);

	start = gc_cheri_getbase(ve->ve_bt->bt_base);
	end = start + gc_cheri_getlen(ve->ve_bt->bt_base);
	if (ve->ve_start == start && ve->ve_end == end)
		return (GC_SUCC);

	return (GC_ERROR);
}

_gc_cap struct gc_vm_ent *
gc_vm_tbl_find_btbl(_gc_cap struct gc_vm_tbl *vt, _gc_cap struct gc_btbl *bt)
{
	size_t i;
	_gc_cap struct gc_vm_ent *ve;
	uint64_t start, end;

	start = gc_cheri_getbase(bt->bt_base);
	end = start + gc_cheri_getlen(bt->bt_base);
	for (i = 0; i < vt->vt_nent; i++) {
		ve = &vt->vt_ent[i];
		gc_debug("search:[%llx,%llx] cur:[%llx,%llx]",
			start,end,ve->ve_start,ve->ve_end);
		if (ve->ve_start == start && ve->ve_end == end)
			return (ve);
	}

	return (NULL);
}

_gc_cap struct gc_vm_ent
*gc_vm_tbl_find(_gc_cap struct gc_vm_tbl *vt, uint64_t addr)
{
	size_t i;
	_gc_cap struct gc_vm_ent *ve;

	for (i = 0; i < vt->vt_nent; i++) {
		ve = &vt->vt_ent[i];
		if (addr >= ve->ve_start && addr < ve->ve_end)
			return (ve);
	}

	return (NULL);
}
