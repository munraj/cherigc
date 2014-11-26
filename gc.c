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
  return c-1;
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
  gc_debug("gc_init success");
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
gc_ins_blk (__gc_capability gc_blk * blk,
    __gc_capability gc_blk ** list)
{
  blk->prev = NULL;
  blk->next = *list;
  *list = blk;
}

__gc_capability gc_blk *
gc_find_free (__gc_capability gc_blk * list, size_t sz)
{
  __gc_capability gc_blk * blk;
  for (blk=list; blk; blk=blk->next)
    if (blk->sz - GC_BLK_HDRSZ > sz)
      return blk;
  return NULL;
}

__gc_capability void *
gc_malloc (size_t sz)
{
  __gc_capability void * ptr;
  __gc_capability gc_blk * blk;
  __gc_capability gc_blk * free_blk;
  size_t remaining, roundsz, logsz;
  int indx;
  gc_debug("external allocator: %zu bytes requested",
    sz);
  if (sz >= GC_BIGSZ - GC_BLK_HDRSZ)
  {
    roundsz = GC_ROUND_ALIGN(sz);
    gc_debug("allocation size %zu is greater than GC_BIGSZ - GC_BLK_HDRSZ",
      sz);
    blk = gc_find_free(gc_state->heap_free, sz);
    if (!blk)
    {
      blk = gc_alloc_internal(roundsz + GC_BLK_HDRSZ);
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
    ptr = (__gc_capability void*) ((__gc_capability char *) blk +
      GC_BLK_HDRSZ);
    ptr = gc_cheri_setlen(ptr, sz);
  }
  else
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
        indx = GC_FIRST_BIT(blk->free);
        /* TODO: the rest of this...*/
      }
    }
  }
  gc_debug("external allocator: returning %s",
    gc_cap_str(ptr));
  return ptr;
}

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
