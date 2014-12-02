#include "gc_stack.h"
#include "gc.h"
#include <stdio.h>

int gc_stack_init (__gc_capability gc_stack * stack, size_t sz)
{
	stack->data =  gc_alloc_internal(sz);
	return 0;
}

int gc_stack_push (__gc_capability gc_stack * stack, __gc_capability void * obj)
{
	if (gc_cheri_getoffset(stack->data) == gc_cheri_getlen(stack->data))
		return 1;
	*stack->data = obj;
	stack->data++;
	return 0;
}

int gc_stack_pop (__gc_capability gc_stack * stack, __gc_capability void * __gc_capability * obj)
{
	if (gc_cheri_getoffset(stack->data) == 0)
		return 1;
	stack->data--;
	/* XXX: poor compiler is doing a clc with -32 offset; force it to
   * use the newly stored cap
   */
	__asm__ __volatile__ ("":::"memory");
	*obj = *stack->data;
	return 0;
}

