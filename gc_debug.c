#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "gc_debug.h"
#include "gc_cmdln.h"

int	gc_debug_indent_level;

void
gc_debug_indent(int incr)
{

	gc_debug_indent_level += incr;
}

const char *
gc_log_severity_str(int severity)
{
	static char s[10];

	if (0)
		;
#define	X(cnst, value, str, ...)			\
	else if (severity == cnst)			\
		return (strcpy(s, str));
	X_GC_LOG
#undef	X
	else
		return (strcpy(s, ""));
}

const char *
gc_cap_str(_gc_cap void *ptr)
{
	static char s[50];

	snprintf(s, sizeof(s), "[b=%p o=%zu l=0x%zx t=%d]",
	    (void*)gc_cheri_getbase(ptr), gc_cheri_getoffset(ptr),
	    gc_cheri_getlen(ptr), gc_cheri_gettag(ptr));
	return (s);
}

void
gc_log(int severity, const char *file, int line, const char *format, ...)
{
	va_list vl;
	int i;

	va_start(vl, format);
	fprintf(stderr, "gc:%s:%d: %s: ", file, line, gc_log_severity_str(severity));
	for (i = 0; i < gc_debug_indent_level; i++)
		fprintf(stderr, "%s", GC_DEBUG_INDENT_STR);
	vfprintf(stderr, format, vl);
	fprintf(stderr, "\n");
	va_end(vl);

	if (gc_state_c != NULL && gc_state_c->gs_enter_cmdln_on_log)
		gc_cmdln();
}

void
gc_print_map(_gc_cap struct gc_btbl * btbl)
{
	int i, j;
	uint64_t addr;
	uint64_t prev_cont_addr;
	uint64_t prev_addr;
	uint8_t byte;

	gc_debug("btbl base: %s\n", gc_cap_str(btbl->bt_base));
	prev_cont_addr = 0;
	prev_addr = 0;
	for (i = 0; i < btbl->bt_nslots / 4; i++) {
		byte = btbl->bt_map[i];
		for (j=0; j<4; j++) {
			addr = (uint64_t)gc_cheri_getbase(btbl->bt_base) +
			    (4 * i + j) * btbl->bt_slotsz;
			if (prev_cont_addr && (byte >> 6) != GC_BTBL_CONT) {
				if (prev_cont_addr == prev_addr)
					gc_debug("0x%llx    continuation data",
					    prev_cont_addr);
				else
					gc_debug("0x%llx-0x%llx  (continuation data)",
					    prev_cont_addr, prev_addr);
				prev_cont_addr = 0;
			}
			switch (byte >> 6) {
			case 0x1:
				if (btbl->bt_flags & GC_BTBL_FLAG_SMALL)
					gc_debug("0x%llx    block header",
					    addr);
				else
					gc_debug("0x%llx    used unmarked",
					    addr);
				break;
			case 0x2:
				if (prev_cont_addr == 0)
					prev_cont_addr = addr;
				break;
			case 0x3:
				if (btbl->bt_flags & GC_BTBL_FLAG_SMALL)
					gc_debug("0x%llx    reserved", addr);
				else
					gc_debug("0x%llx    used marked", addr);
				break;
			}
			byte <<= 2;
			prev_addr = addr;
		}
	}
	if (prev_cont_addr) {
		if (prev_cont_addr == prev_addr)
			gc_debug("0x%llx    continuation data", prev_cont_addr);
		else
			gc_debug("0x%llx-0x%llx  (continuation data)",
			    prev_cont_addr, prev_addr);
	}
}

const char *
gc_ve_prot_str(uint32_t prot)
{
	static char s[4];

	snprintf(s, sizeof(s), "%c%c%c",
	    (prot & GC_VE_PROT_RD) ? 'r' : '-',
	    (prot & GC_VE_PROT_WR) ? 'w' : '-',
	    (prot & GC_VE_PROT_EX) ? 'x' : '-');
	return (s);
}

void
gc_print_vm_tbl(_gc_cap struct gc_vm_tbl *vt)
{
	size_t i;
	_gc_cap struct gc_vm_ent *ve;

	gc_debug("vm table: %zu active entries (space for %zu)",
	    vt->vt_nent, vt->vt_sz);

	for (i = 0; i < vt->vt_nent; i++) {
		ve = &vt->vt_ent[i];
		gc_debug(GC_DEBUG_VE_FMT, GC_DEBUG_VE_PRI(ve));
	}
}

void
gc_print_siginfo_status(void)
{
	size_t btbls_sz, btblb_sz;

	btbls_sz = gc_cheri_getlen(gc_state_c->gs_btbl_small.bt_base);
	btblb_sz = gc_cheri_getlen(gc_state_c->gs_btbl_big.bt_base);

	printf(
	    "[gc] alloc=%zu allocb=%zu%c mk=%zu mkb=%zu%c swp=%zu swpb=%zu%c\n"
	    "[gc] btbls: [sz=%zu%c] btblb: [sz=%zu%c]\n"
	    "[gc] ntcollect=%zu\n",
	    gc_state_c->gs_nalloc, SZFORMAT(gc_state_c->gs_nallocbytes),
	    gc_state_c->gs_nmark, SZFORMAT(gc_state_c->gs_nmarkbytes),
	    gc_state_c->gs_nsweep, SZFORMAT(gc_state_c->gs_nsweepbytes),
	    SZFORMAT(btbls_sz), SZFORMAT(btblb_sz),
	    gc_state_c->gs_ntcollect);
}

void
gc_fill(_gc_cap void *obj, uint32_t magic)
{
	_gc_cap uint32_t *ip;
	_gc_cap uint8_t *cp;
	size_t len, rem, i;

	cp = gc_cheri_setoffset(obj, 0);
	ip = gc_cheri_setoffset(obj, 0);
	len = gc_cheri_getlen(ip) / sizeof(uint32_t);
	rem = gc_cheri_getlen(ip) % sizeof(uint32_t);

	for (i = 0; i < len; i++)
		ip[i] = magic;
	for (i = 0; i < rem; i++)
		cp[len * sizeof(uint32_t) + i] =
		    (magic >> (24 - (8 * i))) & 0xFF;
}

void
gc_fill_used_mem(_gc_cap void *obj, size_t roundsz)
{

	gc_debug("fill with INIT_USE: %s", gc_cap_str(obj));
	gc_fill(obj, GC_MAGIC_INIT_USE);
	obj = gc_cheri_ptr((void*)(gc_cheri_getbase(obj) +
	    gc_cheri_getlen(obj)), roundsz - gc_cheri_getlen(obj));
	gc_debug("fill with INIT_INTERNAL: %s", gc_cap_str(obj));
	gc_fill(obj, GC_MAGIC_INIT_INTERNAL);
}

void
gc_fill_free_mem(_gc_cap void *obj)
{
	gc_debug("fill with INIT_FREE: %s", gc_cap_str(obj));
	gc_fill(obj, GC_MAGIC_INIT_FREE);
}
