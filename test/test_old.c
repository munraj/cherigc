#include <gc.h>
#include <gc_debug.h>
#include <gc_collect.h>
#include <gc_scan.h>

#include <machine/cheri.h>
#include <machine/cheric.h>

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#ifdef GC_USE_LIBPROCSTAT
#include <kvm.h>
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/user.h>
#include <libprocstat.h>
#include <unistd.h>
#endif /* GC_USE_LIBPROCSTAT */

typedef struct {
	int v[100];
	_gc_cap void * p[10];
} S;

int
main()
{
	gc_init();
#ifdef GC_USE_LIBPROCSTAT
	struct procstat * ps;
	struct kinfo_vmentry * kv;
	struct kinfo_proc * kp;
	unsigned cnt, i;
	ps = procstat_open_sysctl();
	if (!ps) {
		gc_error("procstat_open\n");
		return 1;
	}
	cnt = 0;
	kp = procstat_getprocs(ps, KERN_PROC_PID, getpid(), &cnt);
	if (!kp) {
		gc_error("procstat_getprocs\n");
		return 1;
	}
	printf("getprocs retrieved %u procs\n", cnt);
	kv = procstat_getvmmap(ps, kp, &cnt);
	if (!kv) {
		gc_error("procstat_getvmmap\n");
		return 1;
	}
	for (i=0; i<cnt; i++) {
		printf("[%u] type: %d, start: 0x%"PRIx64", end: 0x%"PRIx64" (sz: %"PRIu64"%c), prot: 0x%x\n",
			i, kv[i].kve_type, kv[i].kve_start, kv[i].kve_end, SZFORMAT(kv[i].kve_end - kv[i].kve_start), kv[i].kve_protection);
	}
	procstat_freevmmap(ps, kv);
	procstat_freeprocs(ps, kp);
	procstat_close(ps);
	exit(0);
#endif /* GC_USE_LIBPROCSTAT */
	_gc_cap S * x;
	x = gc_malloc(sizeof *x);
	x->p[0] = x;
	/* set the stack top because we're calling gc_collect outside of the GC */
	void * local;
	uintptr_t localp = (uintptr_t)GC_ALIGN(&local);
	gc_state_c->gs_stack =
		gc_cheri_ptr((void*)localp, (uintptr_t)gc_state_c->gs_stack_bottom-localp);
	printf("stack is %s\n", gc_cap_str(gc_state_c->gs_stack));
	gc_collect();
	printf("x is %s\n", gc_cap_str(x));
	//printf("&x is %s\n", gc_cap_str(gc_cap_addr(&x)));
	return 0;
}

int old_main ()
{
	{
		gc_init();
		_gc_cap void * __capability * x, * y;
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
		x[62] = gc_cheri_ptr((void*)0x1,0x2);
		x[121] = gc_cheri_ptr((void*)0x1,0x3);
		x[127] = gc_cheri_ptr((void*)0x1,0x4);
		x[63] = gc_cheri_ptr((void*)0x1,0x5);
		x[16] = gc_cheri_ptr((void*)0x1,0x6);
		x[15] = gc_cheri_ptr((void*)0x1,0x7);
		x[14] = gc_cheri_ptr((void*)0x1,0x8);
		struct s
		{
			int i;
			__capability void * value;
		};
		__capability struct s * z;
		z=gc_malloc(sizeof *z);
		z->value = z;
		gc_malloc(72);
		gc_malloc(73);
		gc_malloc(74);
		x[12] = gc_malloc(75)+8;
		int i = 53;
		x[13] = gc_cheri_ptr(&i, sizeof i);
		struct gc_tags tags;
		tags = gc_get_page_tags(x);
		printf("tags are 0x%"PRIx64" 0x%"PRIx64"\n", tags.tg_hi, tags.tg_lo);
		//gc_scan_tags(x, tags);
		printf("reconstructed: %s\n", gc_cap_str(y));
		gc_malloc(0);
		gc_print_map(&gc_state_c->gs_btbl_big);
		gc_collect();
		gc_print_map(&gc_state_c->gs_btbl_big);
		gc_print_map(&gc_state_c->gs_btbl_small);
		exit(1);
	}
/*
  printf("stack test\n");
  gc_stack stack;
  _gc_cap gc_stack * stackc =
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
  _gc_cap void * ptr = gc_malloc(100);
  _gc_cap void * a = gc_malloc(107);
  _gc_cap void * b = gc_malloc(90);
  int i;
  for (i=0; i<15; i++)
  {
    _gc_cap void * x = gc_malloc(320000+i);
    //_gc_cap gc_blk * y=NULL;
    //size_t indx=0;
    //int error = gc_get_block(&gc_state_c->btbl, &y, &indx, x);
    fprintf(stderr, "alloc: %s\n", gc_cap_str(x));
    //fprintf(stderr, "get_block: %d, %s, %zu\n", error, gc_cap_str(y),indx);
    /*gc_set_mark(x);
    fprintf(stderr, "small btbl:\n");
    gc_print_map(&gc_state_c->btbl);
    fprintf(stderr, "big btbl:\n");
    gc_print_map(&gc_state_c->btbl_big);*/
  }
  for (i=0; i<15; i++)
  {
    printf("alloc: %s\n", gc_cap_str(gc_malloc(100+i)));
    //printf("small btbl:\n");
    //gc_print_map(&gc_state_c->btbl);
    //fprintf(stderr, "big btbl:\n");
    //gc_print_map(&gc_state_c->btbl_big);
  }
  _gc_cap void * c = gc_malloc(2909);
  _gc_cap void * d = gc_malloc(1);
  _gc_cap void * e = gc_malloc(0);
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
  _gc_cap void ** store = (void*)ALIGN32(&s);
  printf("store: %p, s: %p\n", store,&s);
  store[10] = a;

  size_t len = sizeof s - ((uintptr_t)store-(uintptr_t)&s);
  printf("len: %zu, sizeof s: %zu\n", len, sizeof s);
  //gc_scan_region(gc_cheri_ptr(store, len));
	return 0;
}

