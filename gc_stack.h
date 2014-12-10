#ifndef _GC_STACK_H_
#define _GC_STACK_H_

#include <stdlib.h>

#include "gc_cheri.h"

struct gc_stack {
	_gc_cap void * _gc_cap	*data;
};

int	gc_stack_init(_gc_cap struct gc_stack *_stack, size_t _sz);
int	gc_stack_push(_gc_cap struct gc_stack *_stack, _gc_cap void *_obj);
int	gc_stack_pop(_gc_cap struct gc_stack *_stack,
	    _gc_cap void * _gc_cap *_obj);

#endif /* !_GC_STACK_H_ */
