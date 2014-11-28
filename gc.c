#include "gc.h"

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

  gc_state->mtbl = gc_alloc_internal(GC_PAGESZ);
  if (!gc_state->mtbl)
    gc_error("gc_alloc_internal(%zu)", GC_PAGESZ);
  memset((void*)gc_state->mtbl, 0, GC_PAGESZ);
  gc_state->mtbl->base = gc_alloc_internal(GC_PAGESZ*GC_PAGES_PER_MTBL);
  if (!gc_state->mtbl->base)
    gc_error("gc_alloc_internal(%zu)", GC_PAGESZ*GC_PAGES_PER_MTBL);
  gc_debug("mtbl: %s", gc_cap_str(gc_state->mtbl));
  gc_debug("base: %s", gc_cap_str(gc_state->mtbl->base));

  gc_debug("gc_init success");
}

int
gc_alloc_free_page (__gc_capability gc_mtbl * mtbl,
  __gc_capability gc_blk ** out_blk, int type)
{
  int i, j;
  for (i=0; i<GC_MTBL_MAP_SZ; i++)
  {
    uint8_t byte = mtbl->map[i];

    /* no; at least one page free */
    for (j=0; j<4; j++)
    {
      if (!(byte & 0xC0))
      {
        *out_blk = gc_cheri_incbase(mtbl->base,
          (i*4+j)*GC_PAGESZ);
        *out_blk = gc_cheri_setlen(*out_blk,
          GC_PAGESZ);
        mtbl->map[i] |= type << (3-j)*2;
        return 0;
      }
      byte <<= 2;
    } 
  }
  return 1;
}

int
gc_get_block (__gc_capability gc_mtbl * mtbl,
  __gc_capability gc_blk ** out_blk,
  __gc_capability void * ptr)
{
  /* obtain the block header for a pointer */
  size_t indx;
  uint8_t byte, type;
  if ((uintptr_t)ptr < (uintptr_t)mtbl->base ||
      (uintptr_t)ptr > (uintptr_t)mtbl->base +
        GC_PAGESZ*GC_PAGES_PER_MTBL)
  {
    return 1;
  }
  indx = ((uintptr_t)ptr - (uintptr_t)mtbl->base) >> GC_LOG_PAGESZ;
  while (1)
  {
    byte = mtbl->map[indx/4];
    type = (byte >> ((3-(indx%4))*2)) & 3;
    gc_debug("for pointer %s, indx=%zu, type=0x%x\n",
      gc_cap_str(ptr), indx, type);
    if (type == 0x1)
    {
      /* this page contains block header */
      
      *out_blk = cheri_ptr((uintptr_t)ptr & ~GC_PAGEMASK, GC_PAGESZ);
      return 0;
    }
    else if (type == 0x0 || type == 0x2)
    {
      /* block doesn't exist */
      return 1;
    }
    else if (type == 0x3)
    {
      /* block is continuation data; go to previous page */
      if (!indx) return 1;
      indx--;
    }
  }
  return 0;
}

void
gc_print_map (__gc_capability gc_mtbl * mtbl)
{
  int i, j;
  for (i=0; i<GC_MTBL_MAP_SZ; i++)
  {
    uint8_t byte = mtbl->map[i];
    for (j=0; j<4; j++)
    {
      uint64_t addr = (uint64_t)(uintptr_t)mtbl->base +
          (4*i+j)*GC_PAGESZ;
      switch (byte >> 6)
      {
        case 0x1:
          gc_debug("0x%llx    block header", addr);
          break;
        case 0x3:
          gc_debug("0x%llx    continuation data", addr);
          break;
      }
      byte <<= 2;
    } 
  }
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
gc_malloc (size_t sz)
{
  __gc_capability void * ptr;
  __gc_capability gc_blk * blk;
  int error, roundsz, logsz, hdrbits, indx;
  gc_debug("servicing allocation request of %zu bytes", sz);
  if (sz >= GC_BIGSZ)
  {
    gc_debug("request %zu is big", sz);
    ptr = NULL;
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
      error = gc_alloc_free_page(gc_state->mtbl, &blk, 0x1);
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
      gc_debug("found free block: %s", gc_cap_str(blk));
    }
    indx = GC_FIRST_BIT(blk->free);
    gc_debug("first free index: %d", indx);
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

#if 0
__gc_capability gc_blk *
gc_find_free_old (__gc_capability gc_blk * list, size_t sz)
{
  __gc_capability gc_blk * blk;
  for (blk=list; blk; blk=blk->next)
    if (blk->sz >= sz)
      return blk;
  return NULL;
}

__gc_capability void *
gc_malloc_old2 (size_t sz)
{
  __gc_capability void * ptr;
  __gc_capability gc_blk * blk;
  __gc_capability gc_blk * free_blk;
  size_t remaining, roundsz, logsz;
  int indx, hdrbits;
  ptr = NULL;
  gc_debug("external allocator: %zu bytes requested",
    sz);
  if (sz >= GC_BIGSZ)
  {
    roundsz = GC_ROUND_ALIGN(sz);
    gc_debug("allocation size %zu is greater than GC_BIGSZ",
      sz);
    blk = gc_find_free(gc_state->heap_free, sz);
    if (!blk)
    {
      /* allocate new block */
      blk = gc_alloc_internal(roundsz + GC_BLK_HDRSZ);
      if (!blk)
      {
        gc_error("gc_alloc_internal(%zu) failed", GC_BIGSZ);
        return NULL;
      }
      blk->sz = roundsz + GC_BLK_HDRSZ;
      gc_debug("allocated new block: %s", gc_cap_str(blk));
    }
    else
    {
      gc_debug("found free block: %s", gc_cap_str(blk));
      gc_rm_blk(blk,
        (__gc_capability gc_blk **) &gc_state->heap_free);
    }
    blk->objsz = sz;
    blk->marks = 0; 
    blk->free = 0;
    gc_ins_blk(blk,
      (__gc_capability gc_blk **) &gc_state->heap_big);
    remaining = blk->sz - roundsz - GC_BLK_HDRSZ;
    if (remaining >= GC_BIGSZ)
    {
      /* lots of free space in this block, split it */
      gc_debug("remaining space %zu is greater than GC_BIGSZ",
        remaining);
      free_blk = (__gc_capability void*) (
        (__gc_capability char *) blk +
        GC_BLK_HDRSZ + roundsz);
      free_blk->sz = remaining;
      gc_ins_blk(free_blk,
        (__gc_capability gc_blk **) &gc_state->heap_free);
    }
    ptr = gc_cheri_incbase(blk, GC_BLK_HDRSZ);
    ptr = gc_cheri_setlen(ptr, sz);
  }
  else if (GC_ROUND_POW2(sz) >= GC_MINSZ)
  {
    roundsz = GC_ROUND_POW2(sz);
    logsz = GC_LOG2(roundsz);
    gc_debug("allocation size %zu (rounded: %zu, log: %zu) is small",
      sz, roundsz, logsz);
    /* find free space in existing block */
    for (blk=gc_state->heap[logsz]; blk; blk=blk->next)
    {
      if (blk->free)
      {
        gc_debug("found free block: %s", gc_cap_str(blk));
        break;
      }
    }
    if (!blk)
    {
      /* allocate a new block */
      blk = gc_alloc_internal(GC_BIGSZ);
      if (!blk)
      {
        gc_error("gc_alloc_internal(%zu) failed", GC_BIGSZ + GC_BLK_HDRSZ);
        return NULL;
      }
      blk->sz = GC_BIGSZ + GC_BLK_HDRSZ;
      gc_debug("allocated new block: %s", gc_cap_str(blk));
      blk->objsz = roundsz;
      blk->marks = 0; 
      blk->free = ((1ULL << (GC_BIGSZ / roundsz)) - 1ULL);
      /* account for the space taken up by the block header */
      //hdrbits = (GC_BLK_HDRSZ + roundsz - 1) / roundsz;
      //blk->free &= ~((1ULL << hdrbits) - 1ULL);
      gc_debug("free bits: 0x%llx, shifted: 0x%llx", blk->free, 1ULL<<(GC_BIGSZ/roundsz));
      gc_ins_blk(blk,
        (__gc_capability gc_blk **) &gc_state->heap[logsz]);
    }
    indx = GC_FIRST_BIT(blk->free);
    gc_debug("first free index: %d", indx);
    blk->free &= ~(1ULL << indx);
    ptr = gc_cheri_incbase(blk, indx*roundsz);
    ptr = gc_cheri_setlen(ptr, sz);
  }
  else
  {
    gc_error("external allocator: unhandled size: %zu < GC_MINSZ (%zu)\n",
      sz, GC_MINSZ);
  }
  gc_debug("external allocator: returning %s",
    gc_cap_str(ptr));
  return ptr;
}
#endif /* 0 */

/*__gc_capability void *
gc_malloc_old (size_t sz)
{
  __gc_capability void * ptr;
  if (sz > gc_cheri_getlen(gc_state->heap) -
           gc_cheri_getoffset(gc_state->heap))
  {
    gc_debug("external allocator: request %zu bytes: too big (max %zu)",
      sz, gc_cheri_getlen(gc_state->heap) -
      gc_cheri_getoffset(gc_state->heap));
    return NULL;
  }
  ptr = gc_cheri_ptr((void*)gc_cheri_getbase(gc_state->heap) +
      gc_cheri_getoffset(gc_state->heap),
      sz);
  gc_state->heap += sz;
  gc_debug("heap: %s", gc_cap_str(gc_state->heap));
  return ptr;
}*/

void
gc_free (__gc_capability void * ptr);

const char *
gc_log_severity_str (int severity)
{
  static char s[10];
  if (0) {}
#define X(cnst,value,str,...) \
  else if (severity == cnst) return strcpy(s, str);
  X_GC_LOG
#undef X
  else return strcpy(s, "");
}

const char *
gc_cap_str (__gc_capability void * ptr)
{
  static char s[50];
  snprintf(s, sizeof s, "[b=%p o=%zu l=%zu]",
    (void*)gc_cheri_getbase(ptr),
    gc_cheri_getoffset(ptr),
    gc_cheri_getlen(ptr));
  return s;
}

void
gc_log (int severity, const char * file, int line,
    const char * format, ...)
{
  va_list vl;
  va_start(vl, format);
  fprintf(stderr, "gc:%s:%d: %s: ", file, line,
    gc_log_severity_str(severity));
  vfprintf(stderr, format, vl);
  fprintf(stderr, "\n");
  va_end(vl);
}

__gc_capability void *
gc_alloc_internal (size_t sz)
{
  gc_debug("internal allocator: request %zu bytes (mmap)", sz);
  void * ptr;
  ptr = mmap(NULL, sz, PROT_READ | PROT_WRITE,
      MAP_ANON, -1, 0);
  return ptr ? gc_cheri_ptr(ptr, sz) : NULL;
}
