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
	return (0);
}

int
gc_vm_tbl_get(_gc_cap struct gc_vm_tbl *vt)
{
#ifdef GC_USE_LIBPROCSTAT
	struct procstat *ps;
	struct kinfo_vmentry *kv;
	struct kinfo_proc *kp;
	unsigned cnt, i;

	ps = procstat_open_sysctl();
	if (ps == NULL)
		return (1);
	cnt = 0;
	kp = procstat_getprocs(ps, KERN_PROC_PID, getpid(), &cnt);
	if (kp == NULL)
		return (1);
	gc_debug("getprocs retrieved %u procs\n", cnt);
	kv = procstat_getvmmap(ps, kp, &cnt);
	if (kv == NULL)
		return (1);
	gc_debug("getvmmap retrieved %u entries\n", cnt);
	if (gc_vm_tbl_alloc(vt, cnt) != 0)
		return (1);
	for (i = 0; i < vt->vt_sz; i++) {
		vt->vt_ent[i].ve_start = kv[i].kve_start;
		vt->vt_ent[i].ve_end = kv[i].kve_end;
		vt->vt_ent[i].ve_prot = kv[i].kve_protection;
		vt->vt_ent[i].ve_type = 0;
	}
	procstat_freevmmap(ps, kv);
	procstat_freeprocs(ps, kp);
	procstat_close(ps);
	return (0);
#else /* !GC_USE_LIBPROCSTAT */
	return (1);
#endif /* GC_USE_LIBPROCSTAT */
}

