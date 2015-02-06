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
#include "gc_scan.h"
#include "gc_stack.h"
#include "gc_ts.h"
#include "gc_vm.h"

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
 * Note that the tags are indexed by page, whereas the map is
 * indexed by slot.
 *
 * flags & GC_BTBL_FLAG_SMALL:
 * The data blocks store small objects.
 * Four-bit entry for each page from the base.
 * 0b0000: page free
 * 0b0001: contains block header
 * 0b0010: continuation data from some other block
 * 0b0011 - 0b1110: reserved
 * 0b1111: object not managed by GC
 *
 * Otherwise the data blocks store large objects.
 * Four-bit entry for each GC_BIGSZ sized object from the base.
 * 0b0000: slot free
 * 0b0001: slot used, unmarked
 * 0b0010: continuation data from previous slot
 * 0b0011: slot used, marked
 * 0bx1xx: as above, but with revoke flag set
 * 0b1111: object not managed by GC
 *
 *
 */
struct gc_btbl {
	_gc_cap void	*bt_base;	/* pointer to first block */
	size_t		 bt_slotsz;	/* size of each block */
	size_t		 bt_nslots;	/* number of blocks */
	int		 bt_flags;	/* flags */
	_gc_cap uint8_t	*bt_map;	/* size: bt_nslots/4 */
	_gc_cap struct gc_tags	*bt_tags;	/* array of tags for each page */
	int		 bt_valid;	/* used by gc_vm.c */
};

/* Construct an index into the map. */
#define	GC_BTBL_MKINDX(i, j)	((i) * 2 + (j))

/*
 * Break down an index into a byte index for the map (MAPINDX) and a bit index
 * within the byte (BYTINDX).
 */
#define	GC_BTBL_MAPINDX(idx)	((idx) / 2)
#define GC_BTBL_BYTINDX(idx)	((1 - ((idx) % 2)) * 4)

/* Get and set the btbl type of an index into a byte in the map. */
#define GC_BTBL_GETTYPE(byte, j)	(((byte) >> GC_BTBL_BYTINDX(j)) & 15)
#define GC_BTBL_SETTYPE(byte, j, type)	do {				\
		    (byte) &= ~(uint8_t)(15 << GC_BTBL_BYTINDX(j));	\
		    (byte) |= (type) << GC_BTBL_BYTINDX(j);		\
	    } while (0)

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
 * when requesting memory from the operating system. It is also the
 * size used to manage tag bits: the gc_tags structure stores tags for
 * blocks of size GC_PAGESZ.
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

/*
 * The btbl map has entries of the form 0byyxx.
 * The xx part is the 2-bit "type", accessed by GC_BTBL_TYPE_MASK.
 * The yy part is the 2-bit "flags", i.e. individual maskable bits.
 */
#define GC_BTBL_FREE		((uint8_t)0x0)	/* "type" is free */
#define GC_BTBL_USED		((uint8_t)0x1)	/* "type" is used */
#define GC_BTBL_CONT		((uint8_t)0x2)	/* "type" is cont */
#define GC_BTBL_USED_MARKED	((uint8_t)0x3)	/* "type" is used+marked */
#define GC_BTBL_TYPE_MASK	((uint8_t)0x3)
#define GC_BTBL_UNMANAGED	((uint8_t)0xF)	/* obj unmanaged */
#define GC_BTBL_REVOKED_MASK	((uint8_t)0x4)	/* obj revoked flag */

/*
 * The objects stored in this btbl are small: that is, small enough
 * so that it makes sense to track them with a header for each block.
 */
#define GC_BTBL_FLAG_SMALL	0x00000001

/*
 * The objects stored in this btbl are managed by the collector. That
 * is, the garbage collector has sole control over allocating objects
 * in the btbl and is free to read from and write to their memory.
 */
#define GC_BTBL_FLAG_MANAGED	0x00000002

#define GC_MS_NONE	0	/* not collecting */
#define GC_MS_MARK	1	/* marking */
#define GC_MS_SWEEP	2	/* sweeping */

struct gc_state {

	/*
	 * Whenever this is set, any call to gc_log will cause it to invoke
	 * gc_cmdln after printing the diagnostic message. This provides
	 * the command line with an ability to crudely "step" the garbage
	 * collector.
	 */
	int			 gs_enter_cmdln_on_log;

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
	/* Trusted stack buffer. */
	struct gc_ts		 gs_gts;
	/* Capability to trusted stack buffer with correct bound. */
	_gc_cap struct gc_ts	*gs_gts_c;
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
	/* Table of memory mappings. */
	struct gc_vm_tbl	 gs_vt;
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
	/* Total number of collections */
	size_t			 gs_ntcollect;
	/* Total number of allocation requests of each small size */
	size_t			 gs_ntalloc[GC_LOG_BIGSZ];
	/* Total number of allocation requests of large sizes */
	size_t			 gs_ntbigalloc;
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

/* Round up to next multiple of GC_PAGESZ. */
#define GC_ROUND_PAGESZ(x) (((x) + (size_t)GC_PAGESZ - 1) & ~((size_t)GC_PAGESZ - 1))

/* Round down to next multiple of GC_PAGESZ. */
#define GC_ALIGN_PAGESZ(x) ((x) & ~GC_PAGEMASK)

/* Calculate base-2 logarithm when x is known to be a power of two. */
#define GC_LOG2(x) gc_log2(x)

/* Calculate the first bit set in an integer. */
#define GC_FIRST_BIT(x) gc_first_bit(x)

/* Convert block table slot index into page index. */
#define	GC_SLOT_IDX_TO_PAGE_IDX(btbl, indx)			\
	(((indx) * (btbl)->bt_slotsz) / GC_PAGESZ)

/* Convert block table slot index into offset within page. */
#define	GC_SLOT_IDX_TO_PAGE_OFF(btbl, indx)			\
	(((indx) * (btbl)->bt_slotsz) % GC_PAGESZ)

/* Tag granularity (<=> size of a capability). */
#define	GC_TAG_GRAN 32

int	gc_ty_is_cont(uint8_t ty);
uint8_t	gc_ty_set_cont(uint8_t ty);
int	gc_ty_is_free(uint8_t ty);
uint8_t	gc_ty_set_free(uint8_t ty);
int	gc_ty_is_used(uint8_t ty);
uint8_t	gc_ty_set_used(uint8_t ty);
int	gc_ty_is_marked(uint8_t ty);
uint8_t	gc_ty_set_marked(uint8_t ty);
int	gc_ty_is_revoked(uint8_t ty);
uint8_t	gc_ty_set_revoked(uint8_t ty);
int	gc_ty_is_unmanaged(uint8_t ty);
uint8_t	gc_ty_set_unmanaged(uint8_t ty);

int		 gc_init(void);

/*
 * Force collection. Saves regs, stack and calls gc_collect.
 */
void		 gc_extern_collect(void);

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
 * Allocates a free block from the given block table.
 * Returns non-zero iff error.
 */
int		 gc_alloc_free_blk(_gc_cap struct gc_btbl *_btbl,
		    _gc_cap struct gc_blk **_out_blk, int _type);
/*
 * Searches the block table to find the requested number of free blocks. If the
 * requested number are found, they are allocated and the function
 * returns 0. Otherwise, the block table is left unmodified, and the
 * function returns 1.
 */
int		 gc_alloc_free_blks(_gc_cap struct gc_btbl *_btbl,
		    _gc_cap struct gc_blk **_out_blk, int _len);
const char	*binstr(uint8_t _b);
/*
 * Sets the contents of the map of the block table to the given
 * value between the start and end indices (inclusive).
 */
void		 gc_btbl_set_map(_gc_cap struct gc_btbl *_btbl,
		    int _start, int _end, uint8_t _v);
/*
 * Sets an object as marked, and returns the *original* type of the
 * object before the mark was set (this can be used to check if the
 * object was marked already).
 *
 * For SMALL btbls, the type is "emulated" to match these
 * requirements. For example, GC_BTBL_USED_MARKED means what it does
 * for big btbls.
 *
 * e.g.:
 * GC_BTBL_USED: object was used, and mark was set.
 * GC_BTBL_FREE: object is free, and mark was not set.
 * GC_BTBL_UNMANAGED: object is not managed by the GC.
 * GC_BTBL_USED_MARKED: object is used and already marked.
 */
int		 gc_set_mark(_gc_cap void *_p);

/*
 * Used internally by gc_set_mark. Like gc_set_mark, they all
 * *attempt* to set the mark, and return GC_BTBL_UNMANAGED if
 * it fails for the particular given bt.
 *
 * gc_set_mark_bt is the main wrapper.
 * gc_set_mark_big assumes the SMALL flag is not set.
 * gc_set_mark_small assumes the SMALL flag is set.
 */
int		 gc_set_mark_bt(_gc_cap void *_p, _gc_cap struct gc_btbl *_bt);
int		 gc_set_mark_big(_gc_cap void *_p, _gc_cap struct gc_btbl *_bt);
int		 gc_set_mark_small(_gc_cap void *_p, _gc_cap struct gc_btbl *_bt);

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
 * The object's map field is returned when possible. Otherwise,
 * GC_INVALID_BTBL is returned.
 *
 * e.g.:
 * GC_BTBL_USED: block used, valid block header supplied.
 * GC_BTBL_FREE: block free, block header will be invalid.
 * GC_BTBL_UNMANAGED: block unmanaged by this btbl (invalid address).
 * GC_INVALID_BTBL: wrong btbl for this operation (requires SMALL flag).
 */
int		 gc_get_block(_gc_cap struct gc_btbl *_btbl,
		    _gc_cap struct gc_blk **_out_blk,
		    size_t *_out_sml_indx, size_t *_out_big_indx,
		    _gc_cap void *_p);

/*
 * Returns the actual allocated object, given a pointer to its
 * interior, and the status of the object.
 * The status is one of the GC_BTBL type flags indicating the status
 * of the object, but not necessarily its *actual* type (e.g. for
 * GC_BTBL_UNMANAGED, the object might not even have a btbl entry).
 * e.g.:
 * GC_BTBL_USED: the object is managed by us, and currently in use.
 * GC_BTBL_FREE: the object is managed by us, and currently free, so
 *		could be allocated in the future (value of out_ptr
 *		indeterminate: undefined if block not allocated).
 * GC_BTBL_UNMANAGED: the object is unmanaged by the GC.
 *
 * When the type is GC_BTBL_USED or GC_BTBL_FREE:
 * - the object's block table is returned in *_out_btbl.
 * - the object's index in the block table is returned in *_out_big_indx.
 * - for SMALL btbls, the object's index in its block is returned in _out_sml_indx.
 *
 * When the status is GC_BTBL_UNMANAGED, these values are undefined.
 */
int		 gc_get_obj(_gc_cap void *_p,
		    _gc_cap void * _gc_cap *_out_p,
		    _gc_cap struct gc_btbl * _gc_cap *_out_btbl,
		    _gc_cap size_t *_out_big_indx,
		    _gc_cap struct gc_blk * _gc_cap *_out_blk,
		    _gc_cap size_t *_out_sml_indx);

/* Like gc_set_mark_*. */
int		 gc_get_obj_bt(_gc_cap void *ptr,
		    _gc_cap struct gc_btbl *bt,
		    _gc_cap void * _gc_cap *out_ptr,
		    _gc_cap size_t *out_big_indx,
		    _gc_cap struct gc_blk * _gc_cap *out_blk,
		    _gc_cap size_t *out_sml_indx);
int		 gc_get_obj_small(_gc_cap void *ptr,
		    _gc_cap struct gc_btbl *bt,
		    _gc_cap void * _gc_cap *out_ptr,
		    _gc_cap size_t *out_big_indx,
		    _gc_cap struct gc_blk * _gc_cap *out_blk,
		    _gc_cap size_t *out_sml_indx);
int		 gc_get_obj_big(_gc_cap void *ptr,
		    _gc_cap struct gc_btbl *bt,
		    _gc_cap void * _gc_cap *out_ptr,
		    _gc_cap size_t *out_big_indx);

/* Managed by GC, in use, but already marked. */
#define GC_INVALID_BTBL		0x1000000F	/* invalid block table */
#define GC_TOO_SMALL		5	/* size too small */
#define GC_SUCC			0	/* success */
#define GC_ERROR		1	/* failure */

/*
 * Obtain the tags for a page by checking the block tables, and
 * updating them if necessary.
 * The length of the given capability is ignored, but the offset
 * is considered.
 */
struct gc_tags	 gc_get_or_update_tags(_gc_cap struct gc_btbl *_btbl,
		    size_t _page_indx);

#endif /* !_GC_H_ */
