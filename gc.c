#include "gc.h"

#include <machine/cheri.h>
#include <machine/cheric.h>

#include <sys/mman.h>

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

__gc_capability struct gc_state_s * gc_state;

void gc_init (void)
{
  gc_debug("gc_init enter");
  gc_state = gc_alloc_internal(sizeof(struct gc_state_s));
  if (!gc_state)
    gc_error("gc_alloc_internal(%zu)", sizeof(struct gc_state_s));
  gc_state->heap = gc_alloc_internal(GC_INIT_HEAPSZ);
  if (!gc_state->heap)
    gc_error("gc_alloc_internal(%zu)", GC_INIT_HEAPSZ);
  gc_debug("heap: %s", gc_cap_str(gc_state->heap));
  gc_debug("gc_init success");
}

__gc_capability void * gc_malloc (size_t sz)
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
}

void gc_free (__gc_capability void * ptr);

const char * gc_log_severity_str (int severity)
{
  static char s[10];
  if (0) {}
#define X(cnst,value,str,...) \
  else if (severity == cnst) return strcpy(s, str);
  X_GC_LOG
#undef X
  else return strcpy(s, "");
}

const char * gc_cap_str (__gc_capability void * ptr)
{
  static char s[50];
  snprintf(s, sizeof s, "[b=%p o=%zu l=%zu]",
    (void*)gc_cheri_getbase(ptr),
    gc_cheri_getoffset(ptr),
    gc_cheri_getlen(ptr));
  return s;
}

void gc_log (int severity, const char * file, int line,
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

__gc_capability void * gc_alloc_internal (size_t sz)
{
  gc_debug("internal allocator: request %zu bytes (mmap)", sz);
  void * ptr;
  ptr = mmap(NULL, sz, PROT_READ | PROT_WRITE,
      MAP_ANON, -1, 0);
  return ptr ? gc_cheri_ptr(ptr, sz) : NULL;
}
