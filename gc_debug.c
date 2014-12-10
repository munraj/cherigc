#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "gc_debug.h"

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

	va_start(vl, format);
	fprintf(stderr, "gc:%s:%d: %s: ", file, line, gc_log_severity_str(severity));
	vfprintf(stderr, format, vl);
	fprintf(stderr, "\n");
	va_end(vl);
}

void
gc_print_map(_gc_cap struct gc_btbl * btbl)
{
	int i, j;
	uint64_t addr;
	uint64_t prev_cont_addr;
	uint64_t prev_addr;
	uint8_t base, byte;

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
