#include <gc.h>
#include <gc_debug.h>
#include <gc_collect.h>
#include <gc_scan.h>
#include <stdio.h>

#include <machine/cheri.h>
#include <machine/cheric.h>

int main ()
{
	{
		gc_init();
		__gc_capability void * __capability * x, * y;
		gc_malloc(4*1048576);
		gc_malloc(4*1048576);
		gc_malloc(4*1048576);
		x = gc_malloc(GC_PAGESZ*3);
		//x = gc_cheri_ptr((uint64_t)x+50000000, 10);
		printf("allocated: %s\n", gc_cap_str(x));
		int rc;
		//rc = gc_set_mark(x);//, gc_cheri_ptr(&y, sizeof y));
		printf("rc is %d\n", rc);
		memset((void*)x, 0, gc_cheri_getlen(x));
		x[62] = gc_cheri_ptr(0x1,0x2);
		x[121] = gc_cheri_ptr(0x1,0x3);
		x[127] = gc_cheri_ptr(0x1,0x4);
		x[63] = gc_cheri_ptr(0x1,0x5);
		x[16] = gc_cheri_ptr(0x1,0x6);
		x[15] = gc_cheri_ptr(0x1,0x7);
		x[14] = gc_cheri_ptr(0x1,0x8);
		gc_malloc(71);
		gc_malloc(72);
		gc_malloc(73);
		gc_malloc(74);
		x[12] = gc_malloc(75)+8;
		int i = 53;
		x[13] = gc_cheri_ptr(&i, sizeof i);
		gc_tags tags;
		tags = gc_get_page_tags(x);
		printf("tags are 0x%llx 0x%llx\n", tags.hi, tags.lo);
		//gc_scan_tags(x, tags);
		printf("reconstructed: %s\n", gc_cap_str(y));
		gc_malloc(0);
		gc_print_map(&gc_state->mtbl_big);
		gc_collect();
		gc_print_map(&gc_state->mtbl_big);
		gc_print_map(&gc_state->mtbl);
		exit(1);
	}
/*
  printf("stack test\n");
  gc_stack stack;
  __gc_capability gc_stack * stackc =
    gc_cheri_ptr(&stack, sizeof stack);
  printf("init: %d\n", gc_stack_init(stackc, GC_PAGESZ));
  {
    __capability void * cap;
    __capability void * __capability * capc =
      gc_cheri_ptr((void*)&cap, sizeof cap);
    int i;
    for (i=0; i<50000; i++)
    {
      cap = gc_cheri_ptr((void*)0x123400+i, i);
      printf("push: %d\n", gc_stack_push(stackc, cap));
    }
    for (i=0; i<11; i++)
    {
      __asm__ __volatile__ ("daddu $2, $2, $zero");
      __asm__ __volatile__ ("daddu $2, $2, $zero");
      __asm__ __volatile__ ("daddu $2, $2, $zero");
      printf("pop: %d\n", gc_stack_pop(stackc, capc));
      printf("(cap): %s\n", gc_cap_str(cap));
      __asm__ __volatile__ ("daddu $3, $3, $zero");
      __asm__ __volatile__ ("daddu $3, $3, $zero");
      __asm__ __volatile__ ("daddu $3, $3, $zero");
    }
  }
  exit(1);
*/

  gc_init();
	//printf("gc test; sizeof state: %zu\n", sizeof *gc_state);
  __gc_capability void * ptr = gc_malloc(100);
  __gc_capability void * a = gc_malloc(107);
  __gc_capability void * b = gc_malloc(90);
  int i;
  for (i=0; i<15; i++)
  {
    __gc_capability void * x = gc_malloc(320000+i);
    //__gc_capability gc_blk * y=NULL;
    //size_t indx=0;
    //int error = gc_get_block(&gc_state->mtbl, &y, &indx, x);
    fprintf(stderr, "alloc: %s\n", gc_cap_str(x));
    //fprintf(stderr, "get_block: %d, %s, %zu\n", error, gc_cap_str(y),indx);
    /*gc_set_mark(x);
    fprintf(stderr, "small mtbl:\n");
    gc_print_map(&gc_state->mtbl);
    fprintf(stderr, "big mtbl:\n");
    gc_print_map(&gc_state->mtbl_big);*/
  }
  for (i=0; i<15; i++)
  {
    printf("alloc: %s\n", gc_cap_str(gc_malloc(100+i)));
    //printf("small mtbl:\n");
    //gc_print_map(&gc_state->mtbl);
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
  //gc_scan_region(gc_cheri_ptr(store, len));
	return 0;
}

