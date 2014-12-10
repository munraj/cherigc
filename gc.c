#include <sys/mman.h>

#include <machine/cheri.h>
#include <machine/cheric.h>

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "gc.h"
#include "gc_debug.h"
#include "gc_stack.h"

_gc_cap void		*gc_malloc_entry(size_t sz);

_gc_cap struct gc_state	*gc_state_c;

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

	/* Round up nslots to next multiple of 4. */
	nslots = (nslots + (size_t)3) & ~(size_t)3;
	sz = nslots / 4 + sizeof(struct gc_btbl);

	memset((void *)btbl, 0, sizeof(struct gc_btbl));

	btbl->bt_base = gc_alloc_internal(slotsz * nslots);
	if (btbl->bt_base == NULL)
		gc_error("gc_alloc_internal(%zu)", slotsz * nslots);

	btbl->bt_map = gc_alloc_internal(nslots / 4);
	if (btbl->bt_map == NULL) {
		/* XXX: TODO: Free btbl->base. */
		gc_error("gc_alloc_internal(%zu)", nslots / 4);
	}
	memset((void *)btbl->bt_map, 0, nslots / 4);

	btbl->bt_slotsz = slotsz;
	btbl->bt_nslots = nslots;
	btbl->bt_flags = flags;

	gc_debug("allocated btbl map: %s", gc_cap_str(btbl->bt_map));
	gc_debug("allocated btbl base: %s", gc_cap_str(btbl->bt_base));
}

void
gc_init(void)
{

	gc_debug("gc_init enter");
	gc_state_c = gc_alloc_internal(sizeof(struct gc_state));
	if (gc_state_c == NULL)
		gc_error("gc_alloc_internal(%zu)", sizeof(struct gc_state));

	memset((void *)gc_state_c, 0, sizeof(struct gc_state));
	gc_state_c->gs_regs_c = gc_cheri_ptr((void *)&gc_state_c->gs_regs,
	    sizeof(gc_state_c->gs_regs));
	gc_state_c->gs_mark_state = GC_MS_NONE;

	/* 4096*16384 => 64MB heap. */
	gc_alloc_btbl((_gc_cap struct gc_btbl *)&gc_state_c->gs_btbl_small,
	    GC_PAGESZ, 16384, GC_BTBL_FLAG_SMALL);
	/* 1024*16384 => 16MB heap. */
	gc_alloc_btbl((_gc_cap struct gc_btbl *)&gc_state_c->gs_btbl_big,
	    GC_BIGSZ, 16384, 0);

	if (gc_stack_init(&gc_state_c->gs_mark_stack, GC_STACKSZ) != 0)
		gc_error("gc_init_stack(%zu)", GC_STACKSZ);
	gc_state_c->gs_mark_stack_c = gc_cheri_ptr(
	    (void *)&gc_state_c->gs_mark_stack,
	    sizeof(gc_state_c->gs_mark_stack));

	if (gc_stack_init(&gc_state_c->gs_sweep_stack, GC_PAGESZ) != 0)
		gc_error("gc_init_stack(%zu)", GC_PAGESZ);
	gc_state_c->gs_sweep_stack_c = gc_cheri_ptr(
	    (void *)&gc_state_c->gs_sweep_stack,
	    sizeof(gc_state_c->gs_sweep_stack));

	gc_state_c->gs_stack_bottom = gc_get_stack_bottom();
	gc_state_c->gs_static_region = gc_get_static_region();

	gc_debug("gc_init success");
}

int
gc_alloc_free_page(_gc_cap struct gc_btbl *btbl,
    _gc_cap struct gc_blk **out_blk, int type)
{
	int i, j;
	uint8_t byte;

	for (i = 0; i < btbl->bt_nslots / 4; i++) {
		byte = btbl->bt_map[i];
		for (j = 0; j < 4; j++) {
			if (!(byte & 0xC0)) {
				*out_blk = gc_cheri_incbase(btbl->bt_base,
				    (i * 4 + j) * btbl->bt_slotsz);
				*out_blk = gc_cheri_setlen(*out_blk,
					btbl->bt_slotsz);
				btbl->bt_map[i] |= type << ((3 - j) * 2);
				return (0);
			}
			byte <<= 2;
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
	uint8_t value4, mask;
	int i;

	value4 = (value << 6) | (value << 4) | (value << 2) | value;
	if (start / 4 == end / 4) {
		/* Start/end at same byte; just set the required bits. */
		mask = (uint8_t)0xFF >> ((start % 4) * 2);
		mask ^= (uint8_t)0xFF >> (((end % 4) + 1) * 2);
		i = start / 4;
		btbl->bt_map[i] = (btbl->bt_map[i] & ~mask) | (value4 & mask);
	} else {
		mask = (uint8_t)0xFF >> ((start % 4) * 2);
		i = start / 4;
		btbl->bt_map[i] = (btbl->bt_map[i] & ~mask) | (value4 & mask);
		/* Deal with bytes in between. */
		for (i = start / 4 + 1; i <= end / 4 - 1; i++)
			btbl->bt_map[i] = value4;
		mask = ~((uint8_t)0xFF >> (((end % 4) + 1) * 2));
		i = end / 4;
		btbl->bt_map[i] = (btbl->bt_map[i] & ~mask) | (value4 & mask);
	}
}

int
gc_set_mark(_gc_cap void *ptr)
{
	_gc_cap struct gc_blk *blk;
	size_t indx;
	int rc;
	uint8_t byte, type;

	gc_debug("gc_set_mark: finding object %s", gc_cap_str(ptr));
	ptr = gc_cheri_setoffset(ptr, 0); /* sanitize */

	/* Try small region. */
	rc = gc_get_block(&gc_state_c->gs_btbl_small, &blk, &indx, ptr);
	gc_debug("gc_set_mark: small rc: %d, indx=%zu", rc, indx);
	if (rc == GC_OBJ_FREE)
		return (GC_OBJ_FREE);
	else if (rc == GC_OBJ_USED) {
		if (((blk->bk_free >> indx) & 1) != 0)
			return (GC_OBJ_FREE);
		if (((blk->bk_marks >> indx) & 1) != 0)
			return (GC_OBJ_ALREADY_MARKED);
		blk->bk_marks |= 1ULL << indx;
#ifdef GC_COLLECT_STATS
		gc_state_c->gs_nmark++;
		gc_state_c->gs_nmarkbytes += blk->bk_objsz;
#endif
		gc_debug("set mark for small object at index %zu", indx);
		return (GC_OBJ_USED);
	} else if (rc == GC_OBJ_UNMANAGED) {
		/* Try big region. */
		rc = gc_get_btbl_indx(&gc_state_c->gs_btbl_big, &indx, &type,
		    ptr);
		gc_debug("gc_set_mark: big rc: %d, type: %d", rc, type);
		if (rc)
			return (GC_OBJ_UNMANAGED);
		if (type == GC_BTBL_FREE)
			return (GC_OBJ_FREE);
		else if (type == GC_BTBL_USED_MARKED)
			return (GC_OBJ_ALREADY_MARKED);
		else if (type == GC_BTBL_USED)
			byte = gc_state_c->gs_btbl_big.bt_map[indx / 4];
		else {
			/* NOTREACHABLE */
		}
		/*
		 * XXX: only because USED_MARKED is 0b11 does this
		 * work.
		 */
		byte |= GC_BTBL_USED_MARKED << ((3 - (indx % 4)) * 2);
		gc_state_c->gs_btbl_big.bt_map[indx / 4] = byte;
#ifdef GC_COLLECT_STATS
		gc_state_c->gs_nmark++;
		gc_get_obj(ptr, gc_cheri_ptr(&ptr, sizeof(_gc_cap void *)));
		gc_state_c->gs_nmarkbytes += gc_cheri_getlen(ptr);
#endif
		gc_debug("set mark for big object at index %zu", indx);
		return (GC_OBJ_USED);
	} else {
		/* NOTREACHABLE */
	}
	GC_NOTREACHABLE_ERROR();
	return (-1);
}

int
gc_get_btbl_indx(_gc_cap struct gc_btbl *btbl, size_t *out_indx,
    uint8_t *out_type, _gc_cap void *ptr)
{
	size_t logslotsz;
	uint8_t byte, type;

	if ((uintptr_t)ptr < (uintptr_t)gc_cheri_getbase(btbl->bt_base) ||
	    (uintptr_t)ptr >= (uintptr_t)gc_cheri_getbase(btbl->bt_base) +
	    gc_cheri_getlen(btbl->bt_base))
		return (1);
	logslotsz = GC_LOG2(btbl->bt_slotsz);
	*out_indx = ((uintptr_t)ptr -
	    (uintptr_t)gc_cheri_getbase(btbl->bt_base)) >> logslotsz;
	for (;;) {
		byte = btbl->bt_map[*out_indx / 4];
		type = (byte >> ((3 - (*out_indx % 4)) * 2)) & 3;
		if (type == GC_BTBL_CONT) {
			/* Block is continuation data; go to previous page. */
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
    size_t *out_indx, _gc_cap void *ptr)
{
	size_t indx, logslotsz;
	uintptr_t mask;
	uint8_t type;

	if (!(btbl->bt_flags & GC_BTBL_FLAG_SMALL))
		return (GC_INVALID_BTBL);
	if (gc_get_btbl_indx(btbl, &indx, &type, ptr) != 0)
		return (GC_OBJ_UNMANAGED);
	if (type == GC_BTBL_USED) {
		/* This page contains block header. */
		logslotsz = GC_LOG2(btbl->bt_slotsz);
		mask = ((uintptr_t)1 << logslotsz) - (uintptr_t)1;
		*out_blk = cheri_ptr((void*)((uintptr_t)ptr & ~mask),
		    btbl->bt_slotsz);
		*out_indx = ((uintptr_t)ptr - (uintptr_t)*out_blk) /
		    (*out_blk)->bk_objsz;
		return (GC_OBJ_USED);
	} else if (type == GC_BTBL_FREE) {
		/* Block doesn't exist. */
		return (GC_OBJ_FREE);
	} else {
		/* NOTREACHABLE */
	}
	GC_NOTREACHABLE_ERROR();
	return (-1);
}

int
gc_follow_free(_gc_cap struct gc_blk **blk)
{

	for (; *blk != NULL; *blk = (*blk)->bk_next) {
		if ((*blk)->bk_free)
			return (0);
	}
	return (1);
}

void
gc_ins_blk(_gc_cap struct gc_blk *blk, _gc_cap struct gc_blk **list)
{

	blk->bk_prev = NULL;
	blk->bk_next = *list;
	*list = blk;
}

_gc_cap void *
gc_malloc(size_t sz)
{
	size_t len; /* XXX: first var */
	_gc_cap void *c3;
	_gc_cap void *c16;

	len = (uintptr_t)gc_state_c->gs_stack_bottom -
	    (uintptr_t)GC_ALIGN(&len);
	gc_state_c->gs_stack = gc_cheri_setlen(gc_state_c->gs_stack_bottom,
	    len);
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
	_gc_cap void *ptr;
	int error, roundsz, logsz, hdrbits, indx;

	gc_debug("servicing allocation request of %zu bytes", sz);
	if (sz < GC_MINSZ) {
		gc_debug("request %zu is too small, rounding up to %zu",
		    sz, GC_MINSZ);
		sz = GC_MINSZ;
	}
	if (GC_ROUND_POW2(sz) >= GC_BIGSZ) {
		roundsz = GC_ROUND_BIGSZ(sz);
		gc_debug("request %zu is big (rounded %zu)", sz, roundsz);
		/*
		 * Allocate directly from the big heap. Try to bump-the-pointer,
		 * but if this fails, then search the map.
		 */
		if (gc_cheri_getoffset(gc_state_c->gs_btbl_big.bt_base) +
		    roundsz > gc_cheri_getlen(
		    gc_state_c->gs_btbl_big.bt_base)) {
			/* Out of memory, TODO: collect or search the map. */
			gc_debug("couldn't bump the pointer");
			ptr = NULL;
		} else {
			ptr = gc_cheri_incbase(gc_state_c->gs_btbl_big.bt_base,
			    gc_cheri_getoffset(
			    gc_state_c->gs_btbl_big.bt_base));
			ptr = gc_cheri_setoffset(ptr, 0);
			ptr = gc_cheri_setlen(ptr, roundsz);
			gc_state_c->gs_btbl_big.bt_base += roundsz;
			/* Set relevant bits as unfree in table. */
			indx = ((uintptr_t)ptr - (uintptr_t)gc_cheri_getbase(
			    gc_state_c->gs_btbl_big.bt_base)) >> GC_LOG_BIGSZ;
			gc_btbl_set_map(&gc_state_c->gs_btbl_big, indx, indx,
			    GC_BTBL_USED);
			if (roundsz / GC_BIGSZ > 1)
				gc_btbl_set_map(&gc_state_c->gs_btbl_big,
				    indx + 1, indx + roundsz / GC_BIGSZ - 1,
				    GC_BTBL_CONT);
		}
	} else {
		roundsz = GC_ROUND_POW2(sz);
		logsz = GC_LOG2(roundsz);
		gc_debug("request %zu is small (rounded %zu, log %zu)",
		    sz, roundsz, logsz);
		blk = gc_state_c->gs_heap[logsz];
		error = gc_follow_free(&blk); 
		if (error != 0) {
			gc_debug("allocating new block");
			error = gc_alloc_free_page(&gc_state_c->gs_btbl_small,
			    &blk, GC_BTBL_USED);
			if (error != 0) {
				gc_error("out of memory");
				return (NULL);
			}
			gc_debug("first free page: %s", gc_cap_str(blk));
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
	}
#ifdef GC_COLLECT_STATS
	if (ptr) {
		gc_state_c->gs_nalloc++;
		gc_state_c->gs_nallocbytes += gc_cheri_getlen(ptr);
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

_gc_cap void *
gc_alloc_internal(size_t sz)
{
	void *ptr;

	gc_debug("internal allocator: request %zu bytes (mmap)", sz);
	ptr = mmap(NULL, sz, PROT_READ | PROT_WRITE, MAP_ANON, -1, 0);
	return (ptr != NULL ? gc_cheri_ptr(ptr, sz) : NULL);
}

int
gc_get_obj(_gc_cap void *ptr, _gc_cap void * _gc_cap *out_ptr)
{
	int rc;
	_gc_cap struct gc_blk *blk;
	void * base;
	size_t indx, len, i, j;
	uint8_t byte, type;

	ptr = gc_cheri_setoffset(ptr, 0); /* sanitize */
	/* Try small region */
	rc = gc_get_block(&gc_state_c->gs_btbl_small, &blk, &indx, ptr);
	if (rc == GC_OBJ_FREE)
		return (GC_OBJ_FREE);
	if (rc == GC_OBJ_USED) {
		if (((blk->bk_free >> indx) & 1) != 0)
			return (GC_OBJ_FREE);
		base = (char*)blk + indx * blk->bk_objsz;
		len = blk->bk_objsz;
		*out_ptr = gc_cheri_ptr(base, len);
		return (GC_OBJ_USED);
	} else if (rc == GC_OBJ_UNMANAGED) {
		/* Try big region. */
		rc = gc_get_btbl_indx(&gc_state_c->gs_btbl_big, &indx, &type,
		    ptr);
		if (rc != 0)
			return (GC_OBJ_UNMANAGED);
		base = (char*)gc_cheri_getbase(
		    gc_state_c->gs_btbl_big.bt_base) + indx * GC_BIGSZ;
		len = GC_BIGSZ;
		if (type == GC_BTBL_FREE) {
			*out_ptr = gc_cheri_ptr(base, len);
			return (GC_OBJ_FREE);
		} else if (type == GC_BTBL_USED ||
		    type == GC_BTBL_USED_MARKED) {
			/* Determine length of big object. */
			indx++;
			for (i = indx / 4;
			    i < gc_state_c->gs_btbl_big.bt_nslots / 4; i++) {
				byte = gc_state_c->gs_btbl_big.bt_map[i];
				for (j = (i == indx / 4) ? indx % 4 : 0;
				    j < 4; j++) {
					type = (byte >> ((3 - j) * 2)) & 3;
					if (type == GC_BTBL_CONT)
						len += GC_BIGSZ;
					else {
						*out_ptr =
						    gc_cheri_ptr(base, len);
						return (GC_OBJ_USED);
					}
				}
			}
			/* Reached end of table. */
			*out_ptr = gc_cheri_ptr(base, len);
			return GC_OBJ_USED;
		} else {
			/* NOTREACHABLE */
		}
	} else {
		/* NOTREACHABLE */
	}
	GC_NOTREACHABLE_ERROR();
	return (-1);
}
