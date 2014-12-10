#include "gc_stack.h"
#include "gc.h"
#include <stdio.h>

int
gc_stack_init(_gc_cap struct gc_stack *stack, size_t sz)
{

	stack->data = gc_alloc_internal(sz);
	return (0);
}

int
gc_stack_push(_gc_cap struct gc_stack *stack, _gc_cap void *obj)
{

	if (gc_cheri_getoffset(stack->data) == gc_cheri_getlen(stack->data))
		return (1);
	*stack->data = obj;
	stack->data++;
	return (0);
}

int
gc_stack_pop(_gc_cap struct gc_stack *stack, _gc_cap void * _gc_cap *obj)
{

	if (gc_cheri_getoffset(stack->data) == 0)
		return (1);
	stack->data--;
	/* XXX: Poor compiler is doing a clc with -32 offset; force it to use
	 * the newly stored cap.
	 */
	__asm__ __volatile__ ("":::"memory");
	*obj = *stack->data;
	return (0);
}

