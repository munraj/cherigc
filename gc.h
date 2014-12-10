/*-
 * Copyright (c) 2014 Munraj Vadera
 * All rights reserved.
 *
 * $FreeBSD$
 */
#ifndef _GC_H_
#define _GC_H_

#include <stdlib.h>

#include "gc_cheri.h"
#include "gc_stack.h"

struct gc_blk {
	_gc_cap struct gc_blk	*bk_next;	/* next block in the list */
	_gc_cap struct gc_blk	*bk_prev;	/* prev block in the list */
	size_t			 bk_objsz;	/* size of objects stored */
	uint64_t		 bk_marks;	/* mark bits for each object */
	uint64_t		 bk_free;	/* free bits for each object */
};

/*
 * Block table.
 *
 * btbls are used by the GC to compute address-to-object mappings
 * and check or set the status of objects (marked, used, free, etc).
 * Each btbl represents a "slab" or "region" of memory, optimized
 * for a certain size.
 *
 * Each block table contains a base address, a size, a bitmap
 * and a type. The base always points to a chunk of memory of size
 * nslots*slotsz. When the type is SMALL, every slotsz-sized chunk
 * has a block header (gc_blk). When the type is not SMALL, the entire
 * slab is contiguous data. The bitmap behaves differently in both
 * cases. For SMALL btbls, the bitmap only determines whether an
 * entire block of size slotsz is free or not. Mark bits are stored
 * in the gc_blk header itself. For not SMALL btbls, the bitmap
 * determines the individual mark status of slotsz-sized objects.
 *
 * As described below, the "bitmap" is actually a two-bit map, so that
 * each byte contains 4 entries. The bitmap will typically be of size
 * GC_PAGESZ, giving GC_PAGESZ*4 entries.
 *
 * Currently gc_state maintains only two btbls, one for small objects
 * (i.e. with block headers) and one for large objects. In the future,
 * gc_track() will use information given by libprocstat to create
 * btbls to track even "unmanaged" memory. This should be efficient
 * because the number of btbls needed is roughly linear in the
 * number of different virtual memory mappings: each contiguous paged
 * area can be managed by a single btbl (roughly speaking; it depends
 * on the slotsz, because nslots is usually fixed to GC_PAGESZ*4).
 * Typically it might be expected that each btbl tracks 1-64MB of
 * memory, giving an overhead of 0.39-0.006%.
 *
 * The btbl stores information for slotsz*nslots*4 many bytes of
 * data.
 *
 * flags & GC_BTBL_FLAG_SMALL:
 * The data blocks store small objects.
 * Two-bit entry for each page from the base.
 * 0b00: page free
 * 0b01: contains block header
 * 0b10: continuation data from some other block
 * 0b11: reserved
 *
 * Otherwise the data blocks store large objects.
 * Two-bit entry for each GC_BIGSZ sized object from the base.
 * 0b00: slot free
 * 0b01: slot used, unmarked
 * 0b10: continuation data from previous slot
 * 0b11: slot used, marked
 *
 */
struct gc_btbl {
	_gc_cap void	*bt_base;	/* pointer to first block */
	size_t		 bt_slotsz;	/* size of each block */
	size_t		 bt_nslots;	/* number of blocks */
	int		 bt_flags;	/* flags */
	_gc_cap uint8_t	*bt_map;	/* size: bt_nslots/4 */
};

/*
 * GC_BIGSZ
 * Size of a "big" block. Any allocation request from the client of
 * size >= GC_BIGSZ is allocated as one new chunk from the OS, and
 * stored in the big_heap list. See description above.
 *
 * GC_MINSZ
 * Size of the smallest reclaimable object that can be allocated. This
 * is set so that the mark bits can be stored in a single 64 bit
 * integer in the header. Allocations <GC_MINSZ are still allocated
 * from the relevant heap[] list, but the mark bits are only accurate
 * to a granularity of GC_MINSZ.
 *
 * GC_PAGESZ
 * The size of a page. This is the minimum unit of allocation used
 * when requesting memory from the operating system.
 *
 * GC_STACKSZ
 * The size of the mark stack in bytes. Should be a multiple of the
 * page size to avoid wasting memory. This means the stack can have
 * maximum of GC_STACKSZ/(sizeof _gc_cap void *) many entries
 * before it overflows.
 */ 
#define GC_LOG_MINSZ		7
#define GC_LOG_BIGSZ		10
#define GC_LOG_PAGESZ		12
#define GC_BIGSZ		((size_t)1 << GC_LOG_BIGSZ)
#define GC_MINSZ		((size_t)1 << GC_LOG_MINSZ)
#define GC_PAGESZ		((size_t)1 << GC_LOG_PAGESZ)
#define GC_PAGEMASK		(((uintptr_t)1 << GC_LOG_PAGESZ) - (uintptr_t)1)
#define GC_STACKSZ		(4*GC_PAGESZ)

#define GC_BLK_HDRSZ		(sizeof(struct gc_blk))
#define GC_BTBL_FREE		((uint8_t)0x00)
#define GC_BTBL_USED		((uint8_t)0x01)
#define GC_BTBL_CONT		((uint8_t)0x02)
#define GC_BTBL_USED_MARKED	((uint8_t)0x03)

#define GC_BTBL_FLAG_SMALL	0x00000001

#define GC_MS_NONE	0	/* not collecting */
#define GC_MS_MARK	1	/* marking */
#define GC_MS_SWEEP	2	/* sweeping */

struct gc_state {
	/* Small objects: allocated from pools, individual block headers. */
	_gc_cap struct gc_blk	*gs_heap[GC_LOG_BIGSZ];
	_gc_cap struct gc_blk	*gs_heap_free;
	struct gc_btbl		 gs_btbl_small;
	/* Large objects: allocated by bump-the-pointer, no block headers. */
	struct gc_btbl		 gs_btbl_big;
	/* Saved register and stack state; see gc_cheri.h. */
	_gc_cap void		*gs_regs[GC_NUM_SAVED_REGS];
	/* Points to gs_regs with correct bound. */
	_gc_cap void *_gc_cap	*gs_regs_c;
	/* Capability to the stack just as the collector is entered. */
	_gc_cap void		*gs_stack;
	/* Capability to stack bottom; initialized once only. */
	_gc_cap void		*gs_stack_bottom;
	/* Capability to static data bottom; initialized once only. */
	_gc_cap void		*gs_static_region;
	/* Collector mark/sweep state. */
	int			 gs_mark_state;
	/* Mark stack. */
	struct gc_stack		 gs_mark_stack;
	/* Sweep stack. */
	struct gc_stack		 gs_sweep_stack;
	/* Capability to mark stack with correct bound. */
	_gc_cap struct gc_stack	*gs_mark_stack_c;
	/* Capability to sweep stack with correct bound. */
	_gc_cap struct gc_stack	*gs_sweep_stack_c;
#ifdef GC_COLLECT_STATS
	/* Number of objects currently allocated (roughly). */
	size_t			 gs_nalloc;
	/* Number of bytes currently allocated (roughly). */
	size_t			 gs_nallocbytes;
	/* Number of objects marked. */
	size_t			 gs_nmark;
	/* Number of bytes marked. */
	size_t			 gs_nmarkbytes;
	/* Number of objects swept. */
	size_t			 gs_nsweep;
	/* Number of bytes swept. */
	size_t			 gs_nsweepbytes;
#endif /* GC_COLLECT_STATS */
};

extern _gc_cap struct gc_state	*gc_state_c;

/*
 * Round size to next multiple of alignment.
 * Currently set to 32 bytes, which is the alignment of capabilities.
 */
#define GC_ROUND_ALIGN(x) GC_ROUND32(x)

/*
 * Align pointer to previous multiple of alignment.
 * Currently set to 32 bytes, which is the alignment of capabilities.
 */
#define GC_ALIGN(x) GC_ALIGN32(x)

/* Round size to next multiple of 32 bytes. */
#define GC_ROUND32(x) (((x) + (size_t)31) & ~(size_t)31)

/* Align pointer to previous multiple of 32 bytes. */
#define GC_ALIGN32(x) ((void*)((uintptr_t)(x) & ~(uintptr_t)31))

/* Round size to next power of two. */
#define GC_ROUND_POW2(x) gc_round_pow2(x)

/* Round size to next multiple of GC_BIGSZ. */
#define GC_ROUND_BIGSZ(x) (((x) + GC_BIGSZ - 1) & ~(GC_BIGSZ - 1))

/* Calculate base-2 logarithm when x is known to be a power of two. */
#define GC_LOG2(x) gc_log2(x)

/* Calculate the first bit set in an integer. */
#define GC_FIRST_BIT(x) gc_first_bit(x)

int		 gc_init(void);
_gc_cap void	*gc_malloc(size_t _sz);
void		 gc_free(_gc_cap void *_p);
/*
 * Immediately revoke all access to the given capability.
 * This requires finding all outstanding references and invalidating
 * them before returning the object to the memory pool.
 */
void		 gc_revoke(_gc_cap void *_p);
/*
 * Eventually re-use the given capability.
 * This returns the capability to the memory pool only when the last
 * reference to it is deleted.
 */
void		 gc_reuse(_gc_cap void *_p);
_gc_cap void	*gc_alloc_internal(size_t _sz);
void		 gc_print_map(_gc_cap struct gc_btbl *_btbl);
size_t		 gc_round_pow2(size_t _x);
size_t		 gc_log2(size_t _x);
int		 gc_first_bit(uint64_t _x);
/* Initializes the given block table and allocates a map for it. */
void		 gc_alloc_btbl(_gc_cap struct gc_btbl *_btbl, size_t _slotsz,
		    size_t _nslots, int _flags);
/*
 * Allocates a free page from the given block table.
 * Returns non-zero iff error.
 */
int		 gc_alloc_free_page(_gc_cap struct gc_btbl *_btbl,
		    _gc_cap struct gc_blk **_out_blk, int _type);
const char	*binstr(uint8_t _b);
/*
 * Sets the contents of the map of the block table to the given
 * value between the start and end indices (inclusive).
 */
void		 gc_btbl_set_map(_gc_cap struct gc_btbl *_btbl,
		    int _start, int _end, uint8_t _v);
/*
 * Sets an object as marked.
 * Return values:
 * GC_OBJ_USED: object was used, and mark was set.
 * GC_OBJ_FREE: object is free, and mark was not set.
 * GC_OBJ_UNMANAGED: object is not managed by the GC.
 * GC_OBJ_ALREADY_MARKED: object is used and already marked.
 */
int		 gc_set_mark(_gc_cap void *_p);

/*
 * Finds the map index for a given object.
 * Returns non-zero iff the block is not managed by this table,
 * or the table is invalid.
 * If this returns an index, the block might still not be used.
 * Always check the type.
 */
int		 gc_get_btbl_indx(_gc_cap struct gc_btbl *_btbl,
		    size_t *_out_indx, uint8_t *_out_type, _gc_cap void *_p);
/*
 * Finds the block header for a given object.
 * The output index is the index of the object in the block, not the
 * index of the object in the block table.
 * Only works on block tables with the SMALL flag.
 *
 * Return values:
 * GC_OBJ_USED:	block used, valid block header supplied.
 * GC_OBJ_FREE: block free, block header will be invalid.
 * GC_OBJ_UNMANAGED: block unmanaged by this btbl (invalid address).
 * GC_INVALID_BTBL: wrong btbl for this operation (requires SMALL flag).
 */
int		 gc_get_block(_gc_cap struct gc_btbl *_btbl,
		    _gc_cap struct gc_blk **_out_blk, size_t *_out_indx,
		    _gc_cap void *_p);

/*
 * Returns the actual allocated object, given a pointer to its
 * interior, and the status of the object.
 * The status is one of the following:
 * GC_OBJ_USED: the object is managed by us, and currently in use.
 * GC_OBJ_FREE: the object is managed by us, and currently free, so
 *		could be allocated in the future (value of out_ptr
 *		indeterminate: undefined if block not allocated).
 * GC_OBJ_UNMANAGED: the object is unmanaged by the GC.
 */
int		 gc_get_obj(_gc_cap void *_p, _gc_cap void * _gc_cap *_out_p);

#define GC_OBJ_USED		0	/* managed by GC, currently in use */
#define GC_OBJ_FREE		1	/* managed by GC, currently free */
#define GC_OBJ_UNMANAGED	2	/* not managed by GC */
/* Managed by GC, in use, but already marked. */
#define GC_OBJ_ALREADY_MARKED	3
#define GC_INVALID_BTBL		4	/* invalid block table */

#endif /* !_GC_H_ */
