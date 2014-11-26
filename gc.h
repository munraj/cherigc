#ifndef _GC_H_
#define _GC_H_

#include <stdint.h>
#include <stdlib.h>

#define __gc_capability __capability
#define gc_cheri_ptr		cheri_ptr
#define gc_cheri_getbase		cheri_getbase
#define gc_cheri_getoffset		cheri_getoffset
#define gc_cheri_getlen		cheri_getlen
#define gc_cheri_setlen		cheri_setlen

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

#define GC_INIT_HEAPSZ ((size_t) (64*1024))

/*
 * The collector deals with blocks of size GC_BIGSZ, requesting from
 * the operating system memory chunks of size GC_BIGSZ or bigger.
 *
 * Each block contains a metadata structure at the beginning (GC_blk),
 * followed by actual data. The metadata structure implements a
 * linked list.
 *
 * For allocation requests of size >= GC_BIGSZ, the heap_free list is
 * first checked for the requested size. If a free block large enough
 * is found, the block is allocated. The block is split if at least
 * GC_BIGSZ bytes will remain after splitting; otherwise, the entire
 * block is allocated. 
 *
 * The allocated block is added to the heap_big list and the remaining
 * free data, if any, is returned to the heap_free list.
 *
 * For smaller allocations, the size is rounded up to the nearest
 * power of two (say x) and is made from the gc_state->heap[x] block
 * list. The gc_blk header keeps track of how much space is free
 * in each gc_state->heap[x] block. If no block contains a free slot,
 * a new block is allocated from the heap_free list.
 *
 */

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
 */ 
#define GC_LOG_BIGSZ	 (13)
#define GC_BIGSZ	 		 ((size_t) (1 << GC_LOG_BIGSZ))
#define GC_LOG_MINSZ	 (GC_LOG_BIGSZ-5)
#define GC_MINSZ		   ((size_t) (1 << GC_LOG_MINSZ))

/*
 * Round size to next multiple of alignment.
 * Currently set to 32 bytes, which is the alignment of capabilities.
 */
#define GC_ROUND_ALIGN(x) GC_ROUND32(x)

/* Round size to next multiple of 32 bytes. */
#define GC_ROUND32(x) \
	(((x)+(size_t)31)&~(size_t)31)

/*
 * Round size to next power of two.
 */
#define GC_ROUND_POW2(x) gc_round_pow2(x)

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
   * The size of this block, always >= GC_BIGSZ.
	 * This header is included in the size count.
	 */
	size_t sz;

  /*
   * The size of objects stored in this block.
	 * Value undefined when block is on free list.
   */
	size_t objsz;

	/*
   * The mark bits for each object in this block.
	 * Just 1 or 0 when only one object is stored.
	 * Value undefined when block is on free list.
   */
	uint64_t marks;

	/*
   * The free bits for each object in this block.
	 * Value undefined when block is on free list.
	 */
	uint64_t free;
} gc_blk;
#define GC_BLK_HDRSZ GC_ROUND_ALIGN(sizeof(gc_blk))


struct gc_state_s
{
	__gc_capability gc_blk * heap[GC_LOG_BIGSZ];
	__gc_capability gc_blk * heap_free;
	__gc_capability gc_blk * heap_big;
};

extern __gc_capability struct gc_state_s * gc_state;

#define X_GC_LOG \
	X(GC_LOG_ERROR,	0, "error") \
	X(GC_LOG_WARN,	1, "warn") \
	X(GC_LOG_DEBUG,	2, "debug")
#define gc_error(...) gc_log(GC_LOG_ERROR, __FILE__, __LINE__, \
		__VA_ARGS__)
#define gc_warn(...) gc_log(GC_LOG_WARN, __FILE__, __LINE__, \
		__VA_ARGS__)
#define gc_debug(...) gc_log(GC_LOG_DEBUG, __FILE__, __LINE__, \
		__VA_ARGS__)
void gc_log (int severity, const char * file, int line,
		const char * format, ...);
const char * gc_log_severity_str (int severity);
const char * gc_cap_str (__gc_capability void * ptr);

enum gc_defines
{
#define X(cnst,value,...) \
	cnst=value,
X_GC_LOG
#undef X
};

#endif /* _GC_H_ */
