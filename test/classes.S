#.set mips64
.set noreorder
#.set nobopt
.set noat
#include <cheri/cheri_class.h>

CHERI_CLASS_ASM(cheri_gc, cheri_gc_enter);
