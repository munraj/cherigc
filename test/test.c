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
  __gc_capability void * a = gc_malloc(107);
  __gc_capability void * b = gc_malloc(90);
  int i;
  for (i=0; i<15; i++)
  {
    __gc_capability void * x = gc_malloc(320000+i);
    __gc_capability gc_blk * y=NULL;
    size_t indx=0;
    int error = gc_get_block(&gc_state->mtbl, &y, &indx, x);
    fprintf(stderr, "alloc: %s\n", gc_cap_str(x));
    fprintf(stderr, "get_block: %d, %s, %zu\n", error, gc_cap_str(y),indx);
    gc_set_mark(x);
    fprintf(stderr, "small mtbl:\n");
    gc_print_map(&gc_state->mtbl);
    fprintf(stderr, "big mtbl:\n");
    gc_print_map(&gc_state->mtbl_big);
  }
  for (i=0; i<15; i++)
  {
    printf("alloc: %s\n", gc_cap_str(gc_malloc(100+i)));
    printf("small mtbl:\n");
    gc_print_map(&gc_state->mtbl);
    //fprintf(stderr, "big mtbl:\n");
    //gc_print_map(&gc_state->mtbl_big);
  }
  __gc_capability void * c = gc_malloc(2909);
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

