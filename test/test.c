#include <gc.h>
#include <stdio.h>

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
	return 0;
}

