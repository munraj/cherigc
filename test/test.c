#include <gc.h>
#include <gc_scan.h>
#include <stdio.h>

#include <machine/cheri.h>
#include <machine/cheric.h>

int main ()
{
  gc_init();
	printf("gc test\n");
  __gc_capability void * ptr = gc_malloc(100);
  __gc_capability void * a = gc_malloc(1000);
  __gc_capability void * b = gc_malloc(20000);
  __gc_capability void * c = gc_malloc(65536-100-1000-20000);
  __gc_capability void * d = gc_malloc(1);
  __gc_capability void * e = gc_malloc(0);
  printf("ptr: %s\n", gc_cap_str(ptr));
  printf("a: %s\n", gc_cap_str(a));
  printf("b: %s\n", gc_cap_str(b));
  printf("c: %s\n", gc_cap_str(c));
  printf("d: %s\n", gc_cap_str(d));
  printf("e: %s\n", gc_cap_str(e));
  struct
  {
    char stuff[32*20];
  } s;

#define ALIGN32(p) ((void*) (((uintptr_t)(p)+(uintptr_t)31)&~(uintptr_t)31))

  /* store a capability in a struct */
  __gc_capability void ** store = (void*)ALIGN32(&s);
  printf("store: %p, s: %p\n", store,&s);
  store[10] = a;

  size_t len = sizeof s - ((uintptr_t)store-(uintptr_t)&s);
  printf("len: %zu, sizeof s: %zu\n", len, sizeof s);
  gc_scan_region(gc_cheri_ptr(store, len));
	return 0;
}

