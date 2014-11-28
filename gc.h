#ifndef _GC_H_
#define _GC_H_

#include <stdint.h>
#include <stdlib.h>

#define __gc_capability __capability
#define gc_cheri_getbase		cheri_getbase
#define gc_cheri_getlen		cheri_getlen
#define gc_cheri_getoffset		cheri_getoffset
#define gc_cheri_incbase		cheri_incbase
#define gc_cheri_ptr		cheri_ptr
#define gc_cheri_setlen		cheri_setlen
#define gc_cheri_setoffset		cheri_setoffset

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
#define GC_LOG_BIGSZ	 (10)
#define GC_BIGSZ	 		 ((size_t) (1 << GC_LOG_BIGSZ))
#define GC_LOG_MINSZ	 (GC_LOG_BIGSZ-6)
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


#define GC_LOG_PAGESZ			12
#define GC_PAGESZ					((size_t)1 << GC_LOG_PAGESZ)
#define GC_PAGEMASK				(((uintptr_t)1 << GC_LOG_PAGESZ)-(uintptr_t)1)

#define GC_MTBL_MAP_SZ		(GC_PAGESZ - sizeof(__gc_capability void*))
#define GC_PAGES_PER_MTBL	(GC_MTBL_MAP_SZ * (size_t)4)

/* master block table */
typedef struct gc_mtbl_s
{
	__gc_capability void * base;

	/*
   * When the data blocks store small objects:
   * Two-bit entry for each page from the base.
	 * 0b00: page free
	 * 0b01: contains block header
	 * 0b10: continuation data from some other block
	 * 0b11: reserved
	 * In this case, the mtbl stores information for
	 * GC_PAGESZ*GC_PAGESZ*4 many bytes of data.
	 * When GC_PAGESZ = 4kB, this is about 64MB.
   *
   * When the data blocks store large objects:
   * Two-bit entry for each GC_BIGSZ sized object from the base.
	 * 0b00: slot free
	 * 0b01: slot used, unmarked
	 * 0b10: continuation data from previous slot
	 * 0b11: slot used, marked
	 * In this case, the mtbl stores information for
	 * GC_BIGSZ*GC_PAGESZ*4 many bytes of data.
	 * When GC_BIGSZ = 1kB, and GC_PAGESZ = 4kB, this is about 16MB.
   */
	uint8_t map[GC_MTBL_MAP_SZ];
} gc_mtbl;
#define GC_MTBL_FREE	0x0
#define GC_MTBL_USED	0x1

struct gc_state_s
{

	/* small objects: allocated from pools,
	 * individual block headers */
	__gc_capability gc_blk * heap[GC_LOG_BIGSZ];
	__gc_capability gc_blk * heap_free;
	__gc_capability gc_mtbl * mtbl;

	/* large objects: allocated by bump-the-pointer,
	 * no block headers (one large mark/free bitmap) */
	__gc_capability gc_mtbl * mtbl_big;
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

void
gc_print_map (__gc_capability gc_mtbl * mtbl, size_t slotsz);

#endif /* _GC_H_ */
