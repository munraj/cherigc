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
	vt->vt_sz = sz;
	vt->vt_nent = 0;
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
gc_vm_tbl_track_all(_gc_cap struct gc_vm_tbl *vt)
{
#ifdef GC_USE_LIBPROCSTAT
	/*
	 * Create or update btbls to track each mapping,
	 * with page granularity.
	 * This allows us to track even unmanaged objects.
	 */
	
#else /* !GC_USE_LIBPROCSTAT */
	return (GC_ERROR);
#endif /* GC_USE_LIBPROCSTAT */
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
