#ifndef _GC_H_
#define _GC_H_

#include <stdlib.h>
#include "gc_cheri.h"
#include "gc_stack.h"

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
 * maximum of GC_STACKSZ/(sizeof __gc_capability void *) many entries
 * before it overflows.
 */ 
#define GC_LOG_BIGSZ	 (10)
#define GC_BIGSZ	 		 ((size_t) (1 << GC_LOG_BIGSZ))
#define GC_LOG_MINSZ	 (GC_LOG_BIGSZ-6)
#define GC_MINSZ		   ((size_t) (1 << GC_LOG_MINSZ))

#define GC_LOG_PAGESZ	 12
#define GC_PAGESZ			 ((size_t)1 << GC_LOG_PAGESZ)
#define GC_PAGEMASK		 (((uintptr_t)1 << GC_LOG_PAGESZ)-(uintptr_t)1)

#define GC_STACKSZ		 (4*GC_PAGESZ)

void gc_init (void);

__gc_capability void * gc_malloc (size_t sz);

void gc_free (__gc_capability void * ptr);

/*
 * Immediately revoke all access to the given capability.
 * This requires finding all outstanding references and invalidating
 * them before returning the object to the memory pool.
 */
void gc_revoke (__gc_capability void * ptr);

/*
 * Eventually re-use the given capability.
 * This returns the capability to the memory pool only when the last
 * reference to it is deleted.
 */
void gc_reuse (__gc_capability void * ptr);

__gc_capability void * gc_alloc_internal (size_t sz);

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
#define GC_ROUND32(x) \
	(((x)+(size_t)31)&~(size_t)31)

/* Align pointer to previous multiple of 32 bytes. */
#define GC_ALIGN32(x) \
	((void*)((uintptr_t)(x)&~(uintptr_t)31))

/*
 * Round size to next power of two.
 */
#define GC_ROUND_POW2(x) gc_round_pow2(x)

/*
 * Round size to next multiple of GC_BIGSZ.
 */
#define GC_ROUND_BIGSZ(x) \
	(((x)+GC_BIGSZ-1)&~(GC_BIGSZ-1))

/*
 * Calculate base-2 logarithm when x is known to be a power of two.
 */
#define GC_LOG2(x) gc_log2(x)

/*
 * Calculate the first bit set in an integer.
 */
#define GC_FIRST_BIT(x) gc_first_bit(x)

typedef struct gc_blk_s
{
	/*
   * The next and previous blocks in the list.
   */
	__gc_capability struct gc_blk_s * next, * prev;

  /*
   * The size of objects stored in this block.
   */
	size_t objsz;

	/*
   * The mark bits for each object in this block.
	 * Just 1 or 0 when only one object is stored.
   */
	uint64_t marks;

	/*
	 * The free bits for each object in this block.
	 */
	uint64_t free;
} gc_blk;
#define GC_BLK_HDRSZ sizeof(gc_blk)

/* master block table */
typedef struct gc_mtbl_s
{
	__gc_capability void * base;
	size_t slotsz;
	size_t nslots;
	int flags;
	/*
	 * The mtbl stores information for slotsz*nslots*4 many bytes of
	 * data.
 	 *
	 * flags & GC_MTBL_FLAG_SMALL:
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
	__gc_capability uint8_t * map; /* size: nslots/4 */
} gc_mtbl;
#define GC_MTBL_FREE	((uint8_t)0x0)
#define GC_MTBL_USED	((uint8_t)0x1)
#define GC_MTBL_CONT	((uint8_t)0x2)
#define GC_MTBL_USED_MARKED	((uint8_t)0x3)

#define GC_MTBL_FLAG_SMALL 0x1

struct gc_state_s
{

	/*
   * Small objects: allocated from pools, individual block headers.
   */
	__gc_capability gc_blk * heap[GC_LOG_BIGSZ];
	__gc_capability gc_blk * heap_free;
	gc_mtbl mtbl;

	/*
   * Large objects: allocated by bump-the-pointer, no block headers
	 * (one large mark/free bitmap).
   */
	gc_mtbl mtbl_big;

	/*
   * Saved register and stack state.
	 * See gc_cheri.h.
   */
	__gc_capability void * regs[GC_NUM_SAVED_REGS];

	/* points to regs with correct bound */
	__gc_capability void * __gc_capability * regs_c;

	/*
   * Capability to the stack just as the collector is entered.
	 */
	__gc_capability void * stack;

	/* initialized once only */
	__gc_capability void * stack_bottom;
	__gc_capability void * static_region;

	int mark_state;
	gc_stack mark_stack;
	
	/* pointer to stack with correct bound */
	__gc_capability gc_stack * mark_stack_c;

#ifdef GC_COLLECT_STATS
	/* number of objects marked */
	size_t nmark;

	/* number of bytes marked */
	size_t nmarkbytes;
	
	/* number of objects swept */
	size_t nsweep;

	/* number of bytes swept */
	size_t nsweepbytes;
#endif /* GC_COLLECT_STATS */
};

/* not collecting */
#define GC_MS_NONE  0

/* marking */
#define GC_MS_MARK  1

/* sweeping */
#define GC_MS_SWEEP 2

extern __gc_capability struct gc_state_s * gc_state;

void
gc_print_map (__gc_capability gc_mtbl * mtbl);

size_t
gc_round_pow2 (size_t x);

size_t
gc_log2 (size_t x);

int
gc_first_bit (uint64_t x);

/*
 * Initializes the given master block table and allocates a map for
 * it.
 */
void
gc_alloc_mtbl (__gc_capability gc_mtbl * mtbl,
  size_t slotsz, size_t nslots, int flags);

/*
 * Allocates a free page from the given master block table.
 * Returns non-zero iff error.
 */
int
gc_alloc_free_page (__gc_capability gc_mtbl * mtbl,
  __gc_capability gc_blk ** out_blk, int type);

const char * BINSTR (uint8_t byte);

/*
 * Sets the contents of the map of the master block table to the given
 * value between the start and end indices (inclusive).
 */
void
gc_mtbl_set_map (__gc_capability gc_mtbl * mtbl,
  int start, int end, uint8_t value);

/*
 * Sets an object as marked.
 * Returns non-zero iff error.
 */
int
gc_set_mark (__gc_capability void * ptr);

/*
 * Finds the map index for a given object.
 * Returns non-zero iff error.
 */
int
gc_get_mtbl_indx (__gc_capability gc_mtbl * mtbl,
  size_t * indx,
  __gc_capability void * ptr);

/*
 * Finds the block header for a given object.
 * The output index is the index of the object in the block, not the
 * index of the object in the master block table.
 * Only works on master block tables with the SMALL flag.
 * Returns non-zero iff error.
 */
int
gc_get_block (__gc_capability gc_mtbl * mtbl,
  __gc_capability gc_blk ** out_blk,
  size_t * out_indx,
  __gc_capability void * ptr);

/*
 * Returns the actual allocated object, given a pointer to its
 * interior.
 * Returns non-zero on error (i.e., the object is unmanaged by us).
 */
int
gc_get_obj (__gc_capability void * ptr,
	__gc_capability void * __gc_capability * out_ptr);

#endif /* _GC_H_ */
