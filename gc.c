#include <sys/mman.h>

#include <machine/cheri.h>
#include <machine/cheric.h>

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "gc.h"
#include "gc_collect.h"
#include "gc_debug.h"
#include "gc_stack.h"

_gc_cap void		*gc_malloc_entry(size_t sz);

_gc_cap struct gc_state	*gc_state_c;

int
gc_ty_is_cont(uint8_t ty)
{

	return (!gc_ty_is_unmanaged(ty) &&
	    (ty & GC_BTBL_TYPE_MASK) == GC_BTBL_CONT);
}

uint8_t
gc_ty_set_cont(uint8_t ty)
{

	ty &= ~GC_BTBL_TYPE_MASK;
	ty |= GC_BTBL_CONT;
	return (ty);
}


int
gc_ty_is_free(uint8_t ty)
{

	return (!gc_ty_is_unmanaged(ty) &&
	    (ty & GC_BTBL_TYPE_MASK) == GC_BTBL_FREE);
}

uint8_t
gc_ty_set_free(uint8_t ty)
{

	ty &= ~GC_BTBL_TYPE_MASK;
	ty |= GC_BTBL_FREE;
	return (ty);
}

int
gc_ty_is_used(uint8_t ty)
{

	return (!gc_ty_is_unmanaged(ty) &&
	    (ty & GC_BTBL_TYPE_MASK) == GC_BTBL_USED);
}

uint8_t
gc_ty_set_used(uint8_t ty)
{

	ty &= ~GC_BTBL_TYPE_MASK;
	ty |= GC_BTBL_USED;
	return (ty);
}

int
gc_ty_is_marked(uint8_t ty)
{

	return (!gc_ty_is_unmanaged(ty) &&
	    (ty & GC_BTBL_TYPE_MASK) == GC_BTBL_USED_MARKED);
}

uint8_t
gc_ty_set_marked(uint8_t ty)
{

	ty &= ~GC_BTBL_TYPE_MASK;
	ty |= GC_BTBL_USED_MARKED;
	return (ty);
}

int
gc_ty_is_revoked(uint8_t ty)
{

	return (!gc_ty_is_unmanaged(ty) &&
	    (ty & GC_BTBL_REVOKED_MASK));
}

uint8_t
gc_ty_set_revoked(uint8_t ty)
{

	ty |= GC_BTBL_REVOKED_MASK;
	return (ty);
}

int
gc_ty_is_unmanaged(uint8_t ty)
{

	return (ty == GC_BTBL_UNMANAGED);
}

uint8_t
gc_ty_set_unmanaged(uint8_t ty)
{

	return (GC_BTBL_UNMANAGED);
}

size_t
gc_round_pow2(size_t x)
{

	x--;
	x |= x >> 1;
	x |= x >> 2;
	x |= x >> 4;
	x |= x >> 8;
	x |= x >> 16;
	x++;
	return (x);
}

/* XXX: Assumes x is a power of 2. */
size_t
gc_log2(size_t x)
{
	size_t r;

	r = ((x & 0xAAAAAAAA) != 0);
	r |= ((x & 0xFFFF0000) != 0) << 4;
	r |= ((x & 0xFF00FF00) != 0) << 3;
	r |= ((x & 0xF0F0F0F0) != 0) << 2;
	r |= ((x & 0xCCCCCCCC) != 0) << 1;
	return (r);
}

int
gc_first_bit(uint64_t x)
{
	int c;

	x = (x ^ (x - 1)) >> 1; /* set trailing 0s to 1s and zero rest */
	for (c = 0; x != 0; c++)
		x >>= 1;
	return (c);
}

void
gc_alloc_btbl(_gc_cap struct gc_btbl *btbl, size_t slotsz, size_t nslots,
    int flags)
{
	size_t sz;
	size_t memsz;
	size_t mapsz;
	size_t tagsz;
	size_t npages;

	/* Round up nslots to next multiple of 2. */
	nslots = (nslots + (size_t)1) & ~(size_t)1;
	sz = nslots / 2 + sizeof(struct gc_btbl);

	memset((void *)btbl, 0, sizeof(struct gc_btbl));

	memsz = slotsz * nslots;
	mapsz = nslots / 2;
	npages = memsz / GC_PAGESZ;
	tagsz = npages * sizeof(*btbl->bt_tags);

	btbl->bt_base = gc_alloc_internal(memsz);
	if (btbl->bt_base == NULL)
		gc_error("gc_alloc_internal(%zu)", memsz);
	gc_fill_free_mem(btbl->bt_base);

	/* Contiguously allocate map and tags. */
	btbl->bt_map = gc_alloc_internal(mapsz + tagsz);
	if (btbl->bt_map == NULL) {
		/* XXX: TODO: Free btbl->base. */
		gc_error("gc_alloc_internal(%zu)", mapsz + tagsz);
	}
	memset((void *)btbl->bt_map, 0, mapsz + tagsz);
	btbl->bt_tags = gc_cheri_incbase(
	    btbl->bt_map, mapsz);
	btbl->bt_map = gc_cheri_ptr(
	    (void *)btbl->bt_map, mapsz);

	btbl->bt_slotsz = slotsz;
	btbl->bt_nslots = nslots;
	btbl->bt_flags = flags;
	btbl->bt_valid = 1;

	gc_debug("allocated a block table with %zu slots of size %zu each",
	    nslots, slotsz);
	gc_debug("allocated btbl map: %s", gc_cap_str(btbl->bt_map));
	gc_debug("allocated btbl tags: %s", gc_cap_str(btbl->bt_tags));
	gc_debug("allocated btbl base: %s", gc_cap_str(btbl->bt_base));
}

int
gc_init(void)
{
	/*_gc_cap struct gc_vm_ent *ve;*/

	gc_debug_indent_level = 0;

	gc_debug("gc_init enter");
	gc_state_c = gc_alloc_internal(sizeof(struct gc_state));
	if (gc_state_c == NULL) {
		gc_error("gc_alloc_internal(%zu)", sizeof(struct gc_state));
		return (1);
	}

	memset((void *)gc_state_c, 0, sizeof(struct gc_state));
	gc_state_c->gs_regs_c = gc_cheri_ptr((void *)&gc_state_c->gs_regs,
	    sizeof(gc_state_c->gs_regs));
	gc_state_c->gs_gts_c = gc_cheri_ptr((void *)&gc_state_c->gs_gts,
	    sizeof(gc_state_c->gs_gts));
	gc_state_c->gs_mark_state = GC_MS_NONE;

	/* 4096*16384 => 64MB heap. */
	/* XXX: 4096*10 => 40kB heap. */
	gc_alloc_btbl((_gc_cap struct gc_btbl *)&gc_state_c->gs_btbl_small,
	    GC_PAGESZ, 10/*16384*/, GC_BTBL_FLAG_SMALL | GC_BTBL_FLAG_MANAGED);
	/* 1024*16384 => 16MB heap. */
	/* XXX: 1024*100 => 100kB heap. */
	gc_alloc_btbl((_gc_cap struct gc_btbl *)&gc_state_c->gs_btbl_big,
	    GC_BIGSZ, 100/*16384*/,  GC_BTBL_FLAG_MANAGED);

	if (gc_stack_init(&gc_state_c->gs_mark_stack, GC_STACKSZ) != 0) {
		gc_error("gc_init_stack(%zu)", GC_STACKSZ);
		return (1);
	}
	gc_state_c->gs_mark_stack_c = gc_cheri_ptr(
	    (void *)&gc_state_c->gs_mark_stack,
	    sizeof(gc_state_c->gs_mark_stack));

	if (gc_stack_init(&gc_state_c->gs_sweep_stack, GC_PAGESZ) != 0) {
		gc_error("gc_init_stack(%zu)", GC_PAGESZ);
		return (1);
	}
	gc_state_c->gs_sweep_stack_c = gc_cheri_ptr(
	    (void *)&gc_state_c->gs_sweep_stack,
	    sizeof(gc_state_c->gs_sweep_stack));

	gc_state_c->gs_stack_bottom = gc_get_stack_bottom();
	gc_state_c->gs_static_region = gc_get_static_region();

	if (gc_vm_tbl_alloc(&gc_state_c->gs_vt,
	    GC_PAGESZ / sizeof(struct gc_vm_ent)) != 0) {
		gc_error("gc_vm_tbl_alloc");
		return (1);
	}

	/* Get memory mapping information (typically from libprocstat). */
	if (gc_vm_tbl_update(&gc_state_c->gs_vt) != GC_SUCC) {
		gc_error("gc_vm_tbl_update");
		return (1);
	}

	/*ve = gc_vm_tbl_find_btbl(&gc_state_c->gs_vt, &gc_state_c->gs_btbl_big);
	if (ve == NULL) {
		gc_error("no mapping for big btbl");
		return (1);
	}
	ve->ve_gctype |= GC_VE_TYPE_MANAGED;
	
	ve = gc_vm_tbl_find_btbl(&gc_state_c->gs_vt,
	    &gc_state_c->gs_btbl_small);
	if (ve == NULL) {
		gc_error("no mapping for small btbl");
		return (1);
	}
	ve->ve_gctype |= GC_VE_TYPE_MANAGED;*/

	gc_print_vm_tbl(&gc_state_c->gs_vt);
	//gc_cmdln();
	

	gc_debug("gc_init success");
	return (0);
}

int
gc_alloc_free_blk(_gc_cap struct gc_btbl *btbl,
    _gc_cap struct gc_blk **out_blk, int type)
{
	int i, j, idx;
	uint8_t byte;

	for (i = 0; i < btbl->bt_nslots / 2; i++) {
		byte = btbl->bt_map[i];
		for (j = 0; j < 2; j++) {
			idx = GC_BTBL_MKINDX(i, j);
			if (GC_BTBL_GETTYPE(byte, j) == GC_BTBL_FREE) {
				*out_blk = gc_cheri_incbase(btbl->bt_base,
				    idx * btbl->bt_slotsz);
				*out_blk = gc_cheri_setlen(*out_blk,
					btbl->bt_slotsz);
				GC_BTBL_SETTYPE(byte, j, type);
				btbl->bt_map[i] = byte;
				return (0);
			}
		}
	}
	return (1);
}

int
gc_alloc_free_blks(_gc_cap struct gc_btbl *btbl,
    _gc_cap struct gc_blk **out_blk, int len)
{
	int i, j, idx, fi, fj, fidx, nblk;
	uint8_t byte;

	nblk = (len + btbl->bt_slotsz - 1) / btbl->bt_slotsz;
	fi = -1;
	for (i = 0; i < btbl->bt_nslots / 2; i++) {
		byte = btbl->bt_map[i];
		for (j = 0; j < 2; j++) {
			idx = GC_BTBL_MKINDX(i, j);
			if (GC_BTBL_GETTYPE(byte, j) == GC_BTBL_FREE) {
				if (fi == -1) {
					fi = i;
					fj = j;
					fidx = GC_BTBL_MKINDX(fi, fj);
				} else {
					if (idx - fidx == nblk - 1) {
						/* Have enough free blocks. */
						*out_blk = gc_cheri_incbase(
						    btbl->bt_base,
						    fidx * btbl->bt_slotsz);
						*out_blk = gc_cheri_setlen(
						    *out_blk, nblk * btbl->bt_slotsz);
						/*
						 * Go back and set all blocks as
						 * allocated.
						 */
						if (idx > fidx) 
							gc_btbl_set_map(btbl,
							    fidx + 1, idx,
							    GC_BTBL_CONT);
						gc_btbl_set_map(btbl,
						    fidx, fidx,
						    GC_BTBL_USED);
						return (0);
					}
				}
			} else {
				/* Start search again. */
				fi = -1;
			}
		}
	}
	return (1);
}

const char *
binstr(uint8_t b)
{
	static char c[9];

	c[0] = '0' + ((b >> 7) & 1);
	c[1] = '0' + ((b >> 6) & 1);
	c[2] = '0' + ((b >> 5) & 1);
	c[3] = '0' + ((b >> 4) & 1);
	c[4] = '0' + ((b >> 3) & 1);
	c[5] = '0' + ((b >> 2) & 1);
	c[6] = '0' + ((b >> 1) & 1);
	c[7] = '0' + ((b >> 0) & 1);
	c[8] = '\0';
	return (c);
}

void
gc_btbl_set_map(_gc_cap struct gc_btbl *btbl, int start, int end, uint8_t value)
{
	uint8_t value2, mask;
	int i;

	value2 = (value << 4) | value;
	if (start / 2 == end / 2) {
		/* Start/end at same byte; just set the required bits. */
		mask = (uint8_t)0xFF >> ((start % 2) * 4);
		mask ^= (uint8_t)0xFF >> (((end % 2) + 1) * 4);
		i = start / 2;
		btbl->bt_map[i] = (btbl->bt_map[i] & ~mask) | (value2 & mask);
	} else {
		mask = (uint8_t)0xFF >> ((start % 2) * 4);
		i = start / 2;
		btbl->bt_map[i] = (btbl->bt_map[i] & ~mask) | (value2 & mask);
		/* Deal with bytes in between. */
		for (i = start / 2 + 1; i <= end / 2 - 1; i++)
			btbl->bt_map[i] = value2;
		mask = ~((uint8_t)0xFF >> (((end % 2) + 1) * 4));
		i = end / 2;
		btbl->bt_map[i] = (btbl->bt_map[i] & ~mask) | (value2 & mask);
	}
}

int
gc_set_mark_big(_gc_cap void *ptr, _gc_cap struct gc_btbl *bt)
{
	size_t indx;
	int rc;
	uint8_t byte, type;

	rc = gc_get_btbl_indx(bt, &indx, &type, ptr);
	if (rc != 0)
		return (GC_BTBL_UNMANAGED);
	else if (gc_ty_is_revoked(type))
		/* do something */;
	else if (gc_ty_is_free(type))
		return (type);
	else if (gc_ty_is_marked(type))
		return (type);
	else if (gc_ty_is_used(type))
	{
		byte = bt->bt_map[GC_BTBL_MAPINDX(indx)];
		GC_BTBL_SETTYPE(byte, indx, gc_ty_set_marked(type));
		bt->bt_map[GC_BTBL_MAPINDX(indx)] = byte;
#ifdef GC_COLLECT_STATS
		if (bt->bt_flags & GC_BTBL_FLAG_MANAGED) {
			gc_state_c->gs_nmark++;
			gc_get_obj(ptr, gc_cheri_ptr(&ptr, sizeof(_gc_cap void *)), NULL, NULL, NULL, NULL);
			gc_state_c->gs_nmarkbytes += gc_cheri_getlen(ptr);
		}
#endif
		gc_debug("set mark for big object at index %zu", indx);
		return (type);
	}
	GC_NOTREACHABLE_ERROR();
	return (-1);
}

int
gc_set_mark_small(_gc_cap void *ptr, _gc_cap struct gc_btbl *bt)
{
	_gc_cap struct gc_blk *blk;
	size_t indx;
	int rc;
	uint8_t type;

	rc = gc_get_block(bt, &blk, &indx, NULL, ptr);
	type = rc;
	if (gc_ty_is_unmanaged(rc)) {
		return (GC_BTBL_UNMANAGED);
	} else if (gc_ty_is_free(type)) {
		/* Entire block is free. */
		return (type);
	} else if (gc_ty_is_used(type)) {
		/* Construction of emulated btbl type. */
		if (((blk->bk_free >> indx) & 1) != 0)
			return (gc_ty_set_free(type)); /* free; don't mark */
		if (((blk->bk_marks >> indx) & 1) != 0)
			return (gc_ty_set_marked(type)); /* already marked */
		blk->bk_marks |= 1ULL << indx;
#ifdef GC_COLLECT_STATS
		if (bt->bt_flags & GC_BTBL_FLAG_MANAGED) {
			gc_state_c->gs_nmark++;
			gc_state_c->gs_nmarkbytes += blk->bk_objsz;
		}
#endif
		gc_debug("set mark for small object at index %zu", indx);
		return (type);
	}
	GC_NOTREACHABLE_ERROR();
	return (-1);
}

int
gc_set_mark_bt(_gc_cap void *ptr, _gc_cap struct gc_btbl *bt)
{

	if (bt->bt_flags & GC_BTBL_FLAG_SMALL)
		return (gc_set_mark_small(ptr, bt));
	else
		return (gc_set_mark_big(ptr, bt));
}

int
gc_set_mark(_gc_cap void *ptr)
{
	int rc;
	_gc_cap struct gc_vm_ent *ve;
	uint64_t base;

	ptr = gc_cheri_setoffset(ptr, 0); /* sanitize */

	/* Try small region. */
	rc = gc_set_mark_small(ptr, &gc_state_c->gs_btbl_small);
	if (!gc_ty_is_unmanaged(rc))
		return (rc);

	/* Try big region. */
	rc = gc_set_mark_big(ptr, &gc_state_c->gs_btbl_big);
	if (!gc_ty_is_unmanaged(rc))
		return (rc);

	/*
	 * In neither; must be unmanaged by the GC, but potentially
	 * trackable in the VM mappings, so we try those.
	 */
	gc_debug("note: pointer %s is in neither big nor small region", gc_cap_str(ptr));
	base = gc_cheri_getbase(ptr);
	ve = gc_vm_tbl_find(&gc_state_c->gs_vt, base);
	if (ve == NULL)
		return (GC_BTBL_UNMANAGED);
	gc_debug("note: found a btbl for it: %s", gc_cap_str(ve->ve_bt));
	return (gc_set_mark_bt(ptr, ve->ve_bt));
}

int
gc_get_btbl_indx(_gc_cap struct gc_btbl *btbl, size_t *out_indx,
    uint8_t *out_type, _gc_cap void *ptr)
{
	size_t logslotsz;
	uint8_t byte, type;
	uintptr_t iptr;
	uintptr_t ibase;
	size_t len;

	iptr = (uintptr_t)gc_cheri_getbase(ptr);
	ibase = (uintptr_t)gc_cheri_getbase(btbl->bt_base);
	len = gc_cheri_getlen(btbl->bt_base);

	if (iptr < ibase || iptr >= ibase + len)
		return (1);

	logslotsz = GC_LOG2(btbl->bt_slotsz);
	*out_indx = (iptr - ibase) >> logslotsz;

	for (;;) {
		byte = btbl->bt_map[GC_BTBL_MAPINDX(*out_indx)];
		type = GC_BTBL_GETTYPE(byte, *out_indx);
		if (gc_ty_is_cont(type)) {
			/* Block is continuation data; go to previous block. */
			if (*out_indx == 0)
				return (1);
			(*out_indx)--;
		} else {
			*out_type = type;
			return (0);
		}
	}
}

int
gc_get_block(_gc_cap struct gc_btbl *btbl, _gc_cap struct gc_blk **out_blk,
    size_t *out_sml_indx, size_t *out_big_indx, _gc_cap void *ptr)
{
	size_t indx, logslotsz;
	uintptr_t mask;
	uint8_t type;

	if (!(btbl->bt_flags & GC_BTBL_FLAG_SMALL))
		return (GC_INVALID_BTBL);
	if (gc_get_btbl_indx(btbl, &indx, &type, ptr) != 0)
		return (GC_BTBL_UNMANAGED);
	if (out_big_indx != NULL)
		*out_big_indx = indx;

	if (gc_ty_is_used(type)) {
		/* This block contains block header. */
		logslotsz = GC_LOG2(btbl->bt_slotsz);
		mask = ((uintptr_t)1 << logslotsz) - (uintptr_t)1;
		*out_blk = gc_cheri_ptr((void*)((uintptr_t)ptr & ~mask),
		    btbl->bt_slotsz);
		*out_sml_indx = ((uintptr_t)ptr - (uintptr_t)*out_blk) /
		    (*out_blk)->bk_objsz;
		return (type);
	} else if (gc_ty_is_free(type)) {
		/* Block doesn't exist. */
		return (type);
	}

	GC_NOTREACHABLE_ERROR();
	return (-1);
}

int
gc_follow_free(_gc_cap struct gc_blk **blk)
{

	for (; *blk != NULL; *blk = (*blk)->bk_next)
		if ((*blk)->bk_free != (uint64_t)0) /* at least one bit free */
			return (0);
	return (1);
}

void
gc_ins_blk(_gc_cap struct gc_blk *blk, _gc_cap struct gc_blk **list)
{

	blk->bk_prev = NULL;
	blk->bk_next = *list;
	if (*list != NULL)
		(*list)->bk_prev = blk;
	*list = blk;
}

void
gc_extern_collect(void)
{
	size_t len; /* XXX: first var */
	_gc_cap void *c16;

	len = (uintptr_t)gc_state_c->gs_stack_bottom -
	    (uintptr_t)GC_ALIGN(&len);
	gc_state_c->gs_stack = gc_cheri_ptr(GC_ALIGN(&len), len);
	c16 = gc_state_c->gs_regs_c;
	__asm__ __volatile__ (
		"cmove $c16, %0" : : "C"(c16) : "memory", "$c16"
	);
	GC_SAVE_REGS(16);
	gc_collect();
	GC_RESTORE_REGS(16);
	GC_INVALIDATE_UNUSED_REGS;
}

_gc_cap void *
gc_malloc(size_t sz)
{
	size_t len; /* XXX: first var */
	_gc_cap void *c3;
	_gc_cap void *c16;

	len = (uintptr_t)gc_state_c->gs_stack_bottom -
	    (uintptr_t)GC_ALIGN(&len);
	gc_state_c->gs_stack = gc_cheri_ptr(GC_ALIGN(&len), len);
	c16 = gc_state_c->gs_regs_c;
	__asm__ __volatile__ (
		"cmove $c16, %0" : : "C"(c16) : "memory", "$c16"
	);
	GC_SAVE_REGS(16);
	c3 = gc_malloc_entry(sz);
	GC_RESTORE_REGS(16);
	GC_INVALIDATE_UNUSED_REGS;
	return (c3);
}

_gc_cap void *
gc_malloc_entry(size_t sz)
{
	_gc_cap struct gc_blk *blk;
	_gc_cap void *hp;
	_gc_cap void *ptr;
	uint64_t off;
	int error, roundsz, logsz, hdrbits, indx;
	int collected;
	collected = 0;
retry:

	gc_debug("servicing allocation request of %zu bytes", sz);
	if (sz < GC_MINSZ) {
		gc_debug("request %zu is too small, rounding up to %zu",
		    sz, GC_MINSZ);
		sz = GC_MINSZ;
	}
	if (GC_ROUND_POW2(sz) >= GC_BIGSZ) {
		roundsz = GC_ROUND_BIGSZ(sz);
#ifdef GC_COLLECT_STATS
		gc_state_c->gs_ntbigalloc++;
#endif
		gc_debug("request %zu is big (rounded %zu)", sz, roundsz);
		/*
		 * Allocate directly from the big heap. Try to bump-the-pointer,
		 * but if this fails, then search the map.
		 */
		hp = gc_state_c->gs_btbl_big.bt_base;
		if (gc_cheri_getoffset(hp) + roundsz > gc_cheri_getlen(hp)) {
			/* Out of memory, TODO: collect or search the map. */
			gc_debug("couldn't bump the pointer; searching for free blocks");
			error = gc_alloc_free_blks(&gc_state_c->gs_btbl_big,
			    &blk, roundsz);
			if (error != 0) {
				if (collected) {
					gc_error("out of memory");
					return (NULL);
				} else {
					gc_debug("OOM, collecting...");
					gc_collect();
					collected = 1;
					goto retry;
				}
			}
			gc_debug("found free blocks starting at %s", gc_cap_str(blk));
			ptr = blk;
		} else {
			off = gc_cheri_getoffset(hp);
			indx = off / gc_state_c->gs_btbl_big.bt_slotsz;
			ptr = gc_cheri_incbase(hp, off);
			gc_state_c->gs_btbl_big.bt_base += roundsz;
			/* Set relevant bits as unfree in table. */
			gc_btbl_set_map(&gc_state_c->gs_btbl_big, indx, indx,
			    GC_BTBL_USED);
			if (roundsz > GC_BIGSZ)
				gc_btbl_set_map(&gc_state_c->gs_btbl_big,
				    indx + 1, indx + roundsz / GC_BIGSZ - 1,
				    GC_BTBL_CONT);
		}
		ptr = gc_cheri_setoffset(ptr, 0);
		ptr = gc_cheri_setlen(ptr, sz);
		gc_fill_used_mem(ptr, roundsz);
	} else {
		roundsz = GC_ROUND_POW2(sz);
		logsz = GC_LOG2(roundsz);
#ifdef GC_COLLECT_STATS
		gc_state_c->gs_ntalloc[logsz]++;
#endif
		gc_debug("request %zu is small (rounded %zu, log %zu)",
		    sz, roundsz, logsz);
		blk = gc_state_c->gs_heap[logsz];
		error = gc_follow_free(&blk); 
		if (error != 0) {
			gc_debug("allocating new block");
			error = gc_alloc_free_blk(&gc_state_c->gs_btbl_small,
			    &blk, GC_BTBL_USED);
			if (error != 0) {
				if (collected) {
					gc_error("out of memory");
					return (NULL);
				} else {
					gc_debug("OOM, collecting...");
					gc_collect();
					collected = 1;
					goto retry;
				}
			}
			gc_debug("first free block: %s", gc_cap_str(blk));
			blk->bk_objsz = roundsz;
			blk->bk_marks = 0;
			blk->bk_free = ((1ULL << (GC_PAGESZ / roundsz)) - 1ULL);
			/*
			 * Account for the space taken up by the block
			 * header.
			 */
			hdrbits = (GC_BLK_HDRSZ + roundsz - 1) / roundsz;
			blk->bk_free &= ~((1ULL << hdrbits) - 1ULL);
			gc_debug("free bits: 0x%llx, shifted: 0x%llx",
			    blk->bk_free, 1ULL << (GC_PAGESZ / roundsz));
			gc_ins_blk(blk,
			    (_gc_cap struct gc_blk **)
			    &gc_state_c->gs_heap[logsz]);
		}
		indx = GC_FIRST_BIT(blk->bk_free);
		blk->bk_free &= ~(1ULL << indx);
		ptr = gc_cheri_incbase(blk, indx * roundsz);
		ptr = gc_cheri_setlen(ptr, sz);
		gc_fill_used_mem(ptr, roundsz);
	}
#ifdef GC_COLLECT_STATS
	if (ptr != NULL) {
		gc_state_c->gs_nalloc++;
		gc_state_c->gs_nallocbytes += roundsz;
	}
#endif
	gc_debug("returning %s", gc_cap_str(ptr));
	return (ptr);
}

void
gc_rm_blk(_gc_cap struct gc_blk *blk, _gc_cap struct gc_blk **list)
{

	if (blk == *list)
		*list = blk->bk_next; /* special case: blk is head of list */
	if (blk->bk_next != NULL)
		blk->bk_next->bk_prev = blk->bk_prev;
	if (blk->bk_prev != NULL)
		blk->bk_prev->bk_next = blk->bk_next;
}

void
gc_free(_gc_cap void *ptr)
{

	gc_error("unimplemented: gc_free");
}

void
gc_revoke(_gc_cap void *ptr)
{

	gc_error("unimplemented: gc_revoke");
}

void
gc_reuse(_gc_cap void *ptr)
{

	gc_error("unimplemented: gc_reuse");
}

_gc_cap void *
gc_alloc_internal(size_t sz)
{
	void *ptr;

	gc_debug("internal allocator: request %zu bytes (mmap)", sz);
	ptr = mmap(NULL, sz, PROT_READ | PROT_WRITE, MAP_ANON, -1, 0);
	return (ptr != NULL ? gc_cheri_ptr(ptr, sz) : NULL);
}

int
gc_get_obj_big(_gc_cap void *ptr,
    _gc_cap struct gc_btbl *bt,
    _gc_cap void * _gc_cap *out_ptr,
    _gc_cap size_t *out_big_indx)
{
	int rc;
	void *base;
	size_t indx, len, i, j;
	uint8_t byte, type, jtype;

	rc = gc_get_btbl_indx(bt, &indx, &type, ptr);
	if (out_big_indx != NULL)
		*out_big_indx = indx;
	if (rc != 0)
		return (GC_BTBL_UNMANAGED);

	gc_debug("gc_get_obj_big: indx is %zu\n", indx);

	base = (char *)gc_cheri_getbase(bt->bt_base) + indx * GC_BIGSZ;
	len = GC_BIGSZ;
	if (gc_ty_is_free(type)) {
		*out_ptr = gc_cheri_ptr(base, len);
		return (type);
	} else if (gc_ty_is_used(type) || gc_ty_is_marked(type)) {
		/* Determine length of big object. */
		indx++;
		for (i = GC_BTBL_MAPINDX(indx);
		    i < bt->bt_nslots / 2; i++) {
			byte = bt->bt_map[i];
			for (j = (i == GC_BTBL_MAPINDX(indx)) ? indx % 2 : 0;
			    j < 2; j++) {
				jtype = GC_BTBL_GETTYPE(byte, j);
				if (gc_ty_is_cont(jtype))
					len += GC_BIGSZ;
				else {
					*out_ptr =
					    gc_cheri_ptr(base, len);
					return (type);
				}
			}
		}
		/* Reached end of table. */
		*out_ptr = gc_cheri_ptr(base, len);
		return (type);
	}
	GC_NOTREACHABLE_ERROR();
	return (-1);
}

int
gc_get_obj_small(_gc_cap void *ptr,
    _gc_cap struct gc_btbl *bt,
    _gc_cap void * _gc_cap *out_ptr,
    _gc_cap size_t *out_big_indx,
    _gc_cap struct gc_blk * _gc_cap *out_blk,
    _gc_cap size_t *out_sml_indx)
{
	int rc;
	_gc_cap struct gc_blk *blk;
	void *base;
	size_t indx, big_indx, len;

	rc = gc_get_block(bt, &blk, &indx, &big_indx, ptr);
	if (out_big_indx != NULL)
		*out_big_indx = big_indx;
	if (out_blk != NULL)
		*out_blk = blk;
	if (out_sml_indx != NULL)
		*out_sml_indx = indx;

	if (gc_ty_is_unmanaged(rc))
		return (GC_BTBL_UNMANAGED);

	base = (char *)blk + indx * blk->bk_objsz;
	len = blk->bk_objsz;
	if (out_ptr != NULL)
		*out_ptr = gc_cheri_ptr(base, len);

	if (gc_ty_is_free(rc)) {
		return (rc); 
	} else if (gc_ty_is_used(rc)) {
		if (((blk->bk_free >> indx) & 1) != 0)
			return (gc_ty_set_free(rc));
		if (((blk->bk_marks >> indx) & 1) != 0)
			return (gc_ty_set_marked(rc));
		return (rc);
	}
	GC_NOTREACHABLE_ERROR();
	return (-1);
}

int
gc_get_obj_bt(_gc_cap void *ptr,
    _gc_cap struct gc_btbl *bt,
    _gc_cap void * _gc_cap *out_ptr,
    _gc_cap size_t *out_big_indx,
    _gc_cap struct gc_blk * _gc_cap *out_blk,
    _gc_cap size_t *out_sml_indx)
{

	ptr = gc_cheri_setoffset(ptr, 0); /* sanitize */
	if (bt->bt_flags & GC_BTBL_FLAG_SMALL)
		return (gc_get_obj_small(ptr, bt, out_ptr, out_big_indx,
		    out_blk, out_sml_indx));
	else
		return (gc_get_obj_big(ptr, bt, out_ptr, out_big_indx));
}

int
gc_get_obj(_gc_cap void *ptr,
    _gc_cap void * _gc_cap *out_ptr,
    _gc_cap struct gc_btbl * _gc_cap *out_btbl,
    _gc_cap size_t *out_big_indx,
    _gc_cap struct gc_blk * _gc_cap *out_blk,
    _gc_cap size_t *out_sml_indx)
{
	int rc;
	_gc_cap struct gc_vm_ent *ve;
	uint64_t base;

	ptr = gc_cheri_setoffset(ptr, 0); /* sanitize */

	if (out_btbl != NULL)
		*out_btbl = NULL;

	/* Try small region. */
	rc = gc_get_obj_small(ptr, &gc_state_c->gs_btbl_small,
	    out_ptr, out_big_indx, out_blk, out_sml_indx);
	if (out_btbl != NULL)
		*out_btbl = &gc_state_c->gs_btbl_small;
	if (!gc_ty_is_unmanaged(rc))
		return (rc);

	/* Try big region. */
	rc = gc_get_obj_big(ptr, &gc_state_c->gs_btbl_big,
	    out_ptr, out_big_indx);
	if (out_btbl != NULL)
		*out_btbl = &gc_state_c->gs_btbl_big;
	if (!gc_ty_is_unmanaged(rc))
		return (rc);

	gc_debug("gc_get_obj: pointer %s is in neither big nor small region", gc_cap_str(ptr));
	/* Don't do this because not sure of size of object. */
	/*
	 * In neither; must be unmanaged by the GC, but potentially
	 * trackable in the VM mappings, so we try those.
	 */
	/*
	base = gc_cheri_getbase(ptr);
	ve = gc_vm_tbl_find(&gc_state_c->gs_vt, base);
	if (ve == NULL)
		return (GC_BTBL_UNMANAGED);
	gc_debug("note: found a VM ent for it: " GC_DEBUG_VE_FMT, GC_DEBUG_VE_PRI(ve));
	gc_debug("note: found a btbl for it: %s", gc_cap_str(ve->ve_bt));
	rc = gc_get_obj_bt(ptr, ve->ve_bt, out_ptr, out_big_indx,
	    out_blk, out_sml_indx);
	if (out_btbl != NULL)
		*out_btbl = ve->ve_bt;
	return (rc);*/
	(void)ve;
	(void)base;
	return (GC_BTBL_UNMANAGED);

}

struct gc_tags
gc_get_or_update_tags(_gc_cap struct gc_btbl *btbl, size_t page_indx)
{
	_gc_cap void *page;

	if (!btbl->bt_tags[page_indx].tg_v) {
		/* Construct page capability. */
		page = gc_cheri_incbase(btbl->bt_base, page_indx * GC_PAGESZ);
		page = gc_cheri_setlen(page, GC_PAGESZ);
		page = gc_cheri_setoffset(page, 0);
		btbl->bt_tags[page_indx] = gc_get_page_tags(page);
	}

	return (btbl->bt_tags[page_indx]);
}
