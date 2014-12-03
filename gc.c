#include "gc.h"
#include "gc_debug.h"
#include "gc_stack.h"

#include <machine/cheri.h>
#include <machine/cheric.h>

#include <sys/mman.h>

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

__gc_capability struct gc_state_s * gc_state;

size_t
gc_round_pow2 (size_t x)
{
  x--;
  x |= x>>1;
  x |= x>>2;
  x |= x>>4;
  x |= x>>8;
  x |= x>>16;
  x++;
  return x;
}

size_t
gc_log2 (size_t x)
{
  /* assumes x is a power of 2 */
  size_t r;
  r = ((x & 0xAAAAAAAA) != 0);
  r |= ((x & 0xFFFF0000) != 0) << 4;
  r |= ((x & 0xFF00FF00) != 0) << 3;
  r |= ((x & 0xF0F0F0F0) != 0) << 2;
  r |= ((x & 0xCCCCCCCC) != 0) << 1;
  return r;
}

int
gc_first_bit (uint64_t x)
{
  int c;
  x = (x^(x-1)) >> 1; /* set trailing 0s to 1s and zero rest */
  for (c=0; x; c++)
    x>>=1;
  return c;
}

void
gc_alloc_mtbl (__gc_capability gc_mtbl * mtbl,
  size_t slotsz, size_t nslots, int flags)
{
  size_t sz;
  /* round up nslots to next multiple of 4 */
  nslots = (nslots+(size_t)3)&~(size_t)3;
  sz = nslots/4 + sizeof(gc_mtbl);
  memset((void*)mtbl, 0, sizeof(gc_mtbl));
  mtbl->base = gc_alloc_internal(slotsz*nslots);
  if (!mtbl->base)
    gc_error("gc_alloc_internal(%zu)", slotsz*nslots);
  mtbl->map = gc_alloc_internal(nslots/4);
  if (!mtbl->map)
  {
    /* XXX: todo: free mtbl->base */
    gc_error("gc_alloc_internal(%zu)", nslots/4);
  }
	memset((void*)mtbl->map, 0, nslots/4);
  mtbl->slotsz = slotsz;
  mtbl->nslots = nslots;
  mtbl->flags = flags;
  gc_debug("allocated mtbl map: %s", gc_cap_str(mtbl->map));
  gc_debug("allocated mtbl base: %s", gc_cap_str(mtbl->base));
}

void
gc_init (void)
{
  gc_debug("gc_init enter");
  gc_state = gc_alloc_internal(sizeof(struct gc_state_s));
  if (!gc_state)
    gc_error("gc_alloc_internal(%zu)", sizeof(struct gc_state_s));
  /*gc_state->heap = gc_alloc_internal(GC_INIT_HEAPSZ);
  if (!gc_state->heap)
    gc_error("gc_alloc_internal(%zu)", GC_INIT_HEAPSZ);
  gc_debug("heap: %s", gc_cap_str(gc_state->heap));*/
  memset((void*)gc_state, 0, sizeof(struct gc_state_s));
	gc_state->regs_c = gc_cheri_ptr((void*)&gc_state->regs,
		sizeof(gc_state->regs));

	gc_state->mark_state = GC_MS_NONE;

  gc_alloc_mtbl((__gc_capability gc_mtbl *) &gc_state->mtbl,
    GC_PAGESZ, 16384, GC_MTBL_FLAG_SMALL); /* 4096*16384 => 64MB heap */
  gc_alloc_mtbl((__gc_capability gc_mtbl *) &gc_state->mtbl_big,
    GC_BIGSZ, 16384, 0); /* 1024*16384 => 16MB heap */

  if (gc_stack_init(&gc_state->mark_stack, GC_STACKSZ))
    gc_error("gc_init_stack(%zu)", GC_STACKSZ);
	
	gc_state->mark_stack_c = gc_cheri_ptr((void*)&gc_state->mark_stack,
		sizeof(gc_state->mark_stack));

	gc_state->stack_bottom = gc_get_stack_bottom();
	gc_state->static_region = gc_get_static_region();

  gc_debug("gc_init success");
}

int
gc_alloc_free_page (__gc_capability gc_mtbl * mtbl,
  __gc_capability gc_blk ** out_blk, int type)
{
  int i, j;
  for (i=0; i<mtbl->nslots/4; i++)
  {
    uint8_t byte = mtbl->map[i];

    /* no; at least one page free */
    for (j=0; j<4; j++)
    {
      if (!(byte & 0xC0))
      {
        *out_blk = gc_cheri_incbase(mtbl->base,
          (i*4+j)*mtbl->slotsz);
        *out_blk = gc_cheri_setlen(*out_blk,
          mtbl->slotsz);
        mtbl->map[i] |= type << ((3-j)*2);
        return 0;
      }
      byte <<= 2;
    } 
  }
  return 1;
}

const char * BINSTR (uint8_t byte)
{
  static char c[9];
  c[0] = '0'+((byte>>7)&1);
  c[1] = '0'+((byte>>6)&1);
  c[2] = '0'+((byte>>5)&1);
  c[3] = '0'+((byte>>4)&1);
  c[4] = '0'+((byte>>3)&1);
  c[5] = '0'+((byte>>2)&1);
  c[6] = '0'+((byte>>1)&1);
  c[7] = '0'+((byte>>0)&1);
  c[8] = 0;
  return c;
}

void
gc_mtbl_set_map (__gc_capability gc_mtbl * mtbl,
  int start, int end, uint8_t value)
{
  uint8_t value4, mask;
  int i;
  value4 = (value << 6) | (value << 4) | (value << 2) | value;
  if (start/4 == end/4)
  {
    /* start/end at same byte; just set the required bits */
    mask = (uint8_t)0xFF >> ((start%4)*2);
    mask ^= (uint8_t)0xFF >> (((end%4)+1)*2);
    i = start/4;
    mtbl->map[i] = (mtbl->map[i] & ~mask) |
                   (value4 & mask);
  }
  else
  {
    mask = (uint8_t)0xFF >> ((start%4)*2);
    i = start/4;
    mtbl->map[i] = (mtbl->map[i] & ~mask) |
                   (value4 & mask);
    /* deal with bytes in between */
    for (i=start/4+1; i<=end/4-1; i++)
      mtbl->map[i] = value4;
    mask = ~((uint8_t)0xFF >> (((end%4)+1)*2));
    i = end/4;
    mtbl->map[i] = (mtbl->map[i] & ~mask) |
                   (value4 & mask);
  }
}

int
gc_set_mark (__gc_capability void * ptr)
{
  int error;
  __gc_capability gc_blk * blk;
  size_t indx;
  uint8_t byte;
  gc_debug("setting mark for object %s", gc_cap_str(ptr));
  /* try small region */
  error = gc_get_block(&gc_state->mtbl, &blk, &indx, ptr);
  if (!error)
  {
    blk->marks |= 1ULL << indx;
    gc_debug("set mark for small object at index %zu", indx);
  }
  else
  {
    /* try big region */
    error = gc_get_mtbl_indx(&gc_state->mtbl_big, &indx, ptr);
    if (error)
      return 1;
    byte = gc_state->mtbl_big.map[indx/4];
    byte |= GC_MTBL_USED_MARKED << ((3-(indx%4))*2);
    gc_debug("set mark for big object at index %zu", indx);
    gc_state->mtbl_big.map[indx/4] = byte;
  }
#ifdef GC_COLLECT_STATS
		gc_state->nmark++;
		gc_state->nmarkbytes += gc_cheri_getlen(gc_get_obj(ptr));
#endif /* GC_COLLECT_STATS */
  return 0;
}

int
gc_get_mtbl_indx (__gc_capability gc_mtbl * mtbl,
  size_t * indx,
  __gc_capability void * ptr)
{
  size_t logslotsz;
  uint8_t byte, type;
  if ((uintptr_t)ptr < (uintptr_t)gc_cheri_getbase(mtbl->base) ||
      (uintptr_t)ptr >= (uintptr_t)gc_cheri_getbase(mtbl->base) +
       gc_cheri_getlen(mtbl->base))
    return 1;
  logslotsz = GC_LOG2(mtbl->slotsz);
  *indx = ((uintptr_t)ptr - (uintptr_t)gc_cheri_getbase(mtbl->base)) >> logslotsz;
  while (1)
  {
    byte = mtbl->map[*indx/4];
    type = (byte >> ((3-(*indx%4))*2)) & 3;
    if (type == GC_MTBL_CONT)
    {
      /* block is continuation data; go to previous page */
      if (!*indx) return 1;
      (*indx)--;
    }
    else
      return 0;
  }
}

int
gc_get_block (__gc_capability gc_mtbl * mtbl,
  __gc_capability gc_blk ** out_blk,
  size_t * out_indx,
  __gc_capability void * ptr)
{
  /* obtain the block header for a pointer */
  size_t indx, logslotsz;
  uint8_t byte, type;
  uintptr_t mask;
  if (!(mtbl->flags & GC_MTBL_FLAG_SMALL))
    return 1;
  if (gc_get_mtbl_indx(mtbl, &indx, ptr))
    return 1;
  byte = mtbl->map[indx/4];
  type = (byte >> ((3-(indx%4))*2)) & 3;
  if (type == 0x1)
  {
    /* this page contains block header */
    logslotsz = GC_LOG2(mtbl->slotsz);
    mask = ((uintptr_t)1 << logslotsz) - (uintptr_t)1;
    *out_blk = cheri_ptr((void*)((uintptr_t)ptr & ~mask),
      mtbl->slotsz);
    *out_indx = ((uintptr_t)ptr - (uintptr_t)*out_blk) /
      (*out_blk)->objsz;
    return 0;
  }
  else if (type == GC_MTBL_FREE)
  {
    /* block doesn't exist */
    return 1;
  }
  else
  {
    /* impossible */
    return 1;
  }
  return 0;
}

int
gc_follow_free (__gc_capability gc_blk ** blk)
{
  for (; *blk; *blk=(*blk)->next)
    if ((*blk)->free)
      return 0;
  return 1;
}

void
gc_ins_blk (__gc_capability gc_blk * blk,
    __gc_capability gc_blk ** list)
{
  blk->prev = NULL;
  blk->next = *list;
  *list = blk;
}

__gc_capability void *
gc_malloc_entry (size_t sz);
__gc_capability void *
gc_malloc (size_t sz)
{
	size_t len;
	__capability void * c3;
	/*__asm__ __volatile__ (
		"clc $c16, %0, 0($c0)" : : "r"(gc_state->regs_c) : "memory"
	);*/
	__capability void * c16;
	len = (uintptr_t)gc_state->stack_bottom - (uintptr_t)GC_ALIGN(&len);
	gc_state->stack = gc_cheri_setlen(gc_state->stack_bottom, len);
	c16 = gc_state->regs_c;
	__asm__ __volatile__ (
		"cmove $c16, %0" : : "C"(c16) : "memory", "$c16"
	);
	GC_SAVE_REGS(16);
	c3 = gc_malloc_entry(sz);
	GC_RESTORE_REGS(16);
	GC_INVALIDATE_UNUSED_REGS;
	return c3;
}

__gc_capability void *
gc_malloc_entry (size_t sz)
{
  __gc_capability void * ptr;
  __gc_capability gc_blk * blk;
  int error, roundsz, logsz, hdrbits, indx;
  gc_debug("servicing allocation request of %zu bytes", sz);
  if (GC_ROUND_BIGSZ(sz) >= GC_BIGSZ)
  {
    roundsz = GC_ROUND_BIGSZ(sz);
    gc_debug("request %zu is big (rounded %zu)", sz, roundsz);
    /* allocate directly from the big heap */
    /* try to bump-the-pointer, but if this fails, then search
     * the map
     */
    if (gc_cheri_getoffset(gc_state->mtbl_big.base) + roundsz >
        gc_cheri_getlen(gc_state->mtbl_big.base))
    {
      /* out of memory, collect or search the map */
      gc_debug("couldn't bump the pointer");
      ptr = NULL;
    }
    else
    {
      ptr = gc_cheri_incbase(gc_state->mtbl_big.base,
        gc_cheri_getoffset(gc_state->mtbl_big.base));
      ptr = gc_cheri_setoffset(ptr, 0);
      ptr = gc_cheri_setlen(ptr, roundsz);
      gc_state->mtbl_big.base += roundsz;
      /* set relevant bits as unfree in table */
      indx = ((uintptr_t)ptr -
        (uintptr_t)gc_cheri_getbase(gc_state->mtbl_big.base)) >>
        GC_LOG_BIGSZ;
      gc_mtbl_set_map(&gc_state->mtbl_big,
        indx, indx, GC_MTBL_USED);
      if (roundsz/GC_BIGSZ>1)
        gc_mtbl_set_map(&gc_state->mtbl_big,
          indx+1, indx+roundsz/GC_BIGSZ-1, GC_MTBL_CONT);
    }
  }
  else if (sz < GC_MINSZ)
  {
    gc_debug("request %zu is too small", sz);
    ptr = NULL;
  }
  else
  {
    roundsz = GC_ROUND_POW2(sz);
    logsz = GC_LOG2(roundsz);
    gc_debug("request %zu is small (rounded %zu, log %zu)",
      sz, roundsz, logsz);
    blk = gc_state->heap[logsz];
    error = gc_follow_free(&blk); 
    if (!blk)
    {
      gc_debug("allocating new block");
      error = gc_alloc_free_page(&gc_state->mtbl, &blk, GC_MTBL_USED);
      if (error)
      {
        gc_error("out of memory");
        return NULL;
      }
      gc_debug("first free page: %s", gc_cap_str(blk));
      blk->objsz = roundsz;
      blk->marks = 0;
      blk->free = ((1ULL << (GC_PAGESZ / roundsz)) - 1ULL);
      /* account for the space taken up by the block header */
      hdrbits = (GC_BLK_HDRSZ + roundsz - 1) / roundsz;
      blk->free &= ~((1ULL << hdrbits) - 1ULL);
      gc_debug("free bits: 0x%llx, shifted: 0x%llx",
        blk->free, 1ULL<<(GC_PAGESZ/roundsz));
      gc_ins_blk(blk,
        (__gc_capability gc_blk **) &gc_state->heap[logsz]);
    }
    else
    {
      /*gc_debug("found free block: %s", gc_cap_str(blk));*/
    }
    indx = GC_FIRST_BIT(blk->free);
    blk->free &= ~(1ULL << indx);
    ptr = gc_cheri_incbase(blk, indx*roundsz);
    ptr = gc_cheri_setlen(ptr, sz);
  }
  gc_debug("returning %s", gc_cap_str(ptr));
	return ptr;
}

void
gc_rm_blk (__gc_capability gc_blk * blk,
    __gc_capability gc_blk ** list)
{
  /* special case: blk is head of list */
  if (blk == *list)
    *list = blk->next;
  if (blk->next)
    blk->next->prev = blk->prev;
  if (blk->prev)
    blk->prev->next = blk->next;
}

void
gc_free (__gc_capability void * ptr);

__gc_capability void *
gc_alloc_internal (size_t sz)
{
  gc_debug("internal allocator: request %zu bytes (mmap)", sz);
  void * ptr;
  ptr = mmap(NULL, sz, PROT_READ | PROT_WRITE,
      MAP_ANON, -1, 0);
  return ptr ? gc_cheri_ptr(ptr, sz) : NULL;
}

int
gc_get_obj (__gc_capability void * ptr,
	__gc_capability void * __gc_capability * out_ptr)
{
  int error, done;
  __gc_capability gc_blk * blk;
	void * base;
  size_t indx, len, i, j;
  uint8_t byte, type;
  /* try small region */
  error = gc_get_block(&gc_state->mtbl, &blk, &indx, ptr);
  if (!error)
	{
		base = (char*)blk + indx*blk->objsz;
		len = blk->objsz;
	}
  else
  {
    /* try big region */
    error = gc_get_mtbl_indx(&gc_state->mtbl_big, &indx, ptr);
    if (error)
      return 1;
		base = (char*)gc_cheri_getbase(gc_state->mtbl_big.base) +
			indx*GC_BIGSZ;
		/* determine length of big object */
		len = GC_BIGSZ;
		if (indx+1 < gc_state->mtbl_big.nslots)
		{
			indx++;
			for (i=indx/4, done=0; ;i++)
			{
				byte = gc_state->mtbl_big.map[i];
				for (j=(i==indx/4)?indx%4:0; j<4; j++)
				{
					type = (byte >> ((3-j)*2)) & 3;
					if (type == GC_MTBL_CONT)
						len += GC_BIGSZ;
					else
					{
						done = 1;
						break;
					}
				}
				if (done) break;
			}
		}
  }
	*out_ptr = gc_cheri_ptr(base, len);
  return 0;
}
