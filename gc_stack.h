#ifndef _GC_STACK_H_
#define _GC_STACK_H_

#include <stdlib.h>
#include "gc_cheri.h"

typedef struct
{
	__gc_capability void * __gc_capability * data;
} gc_stack;

int gc_stack_init (__gc_capability gc_stack * stack, size_t sz);
int gc_stack_push (__gc_capability gc_stack * stack,
	__gc_capability void * obj);
int gc_stack_pop (__gc_capability gc_stack * stack,
	__gc_capability void * __gc_capability * obj);

#endif /* _GC_STACK_H */
