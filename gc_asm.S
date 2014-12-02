#pragma clang diagnostic push
//#pragma clang diagnostic ignored "-Wall"
#define GC_ASM_ONLY
.macro GC_SAVE_REG indx, buf, offset
	csc $c\indx, $zero, \offset(\buf)
.endm
.macro GC_RESTORE_REG indx, buf, offset
	clc $c\indx, $zero, \offset(\buf)
.endm
.macro GC_INVALIDATE_REG indx
	ccleartag $c\indx, $c\indx
.endm
.macro GC_INVALIDATE_UNUSED_REGS buf
	GC_INVALIDATE_REG 1
	GC_INVALIDATE_REG 2
	GC_INVALIDATE_REG 4
	GC_INVALIDATE_REG 5
	GC_INVALIDATE_REG 6
	GC_INVALIDATE_REG 7
	GC_INVALIDATE_REG 8
	GC_INVALIDATE_REG 9
	GC_INVALIDATE_REG 10
	GC_INVALIDATE_REG 11
	GC_INVALIDATE_REG 12
	GC_INVALIDATE_REG 13
	GC_INVALIDATE_REG 14
	GC_INVALIDATE_REG 15
	GC_INVALIDATE_REG 16
.endm
.macro GC_SAVE_REGS buf
	GC_SAVE_REG 17, \buf, 0
	GC_SAVE_REG 18, \buf, 32
	GC_SAVE_REG 19, \buf, 64
	GC_SAVE_REG 20, \buf, 96
	GC_SAVE_REG 21, \buf, 128
	GC_SAVE_REG 22, \buf, 192
	GC_SAVE_REG 23, \buf, 256
	GC_SAVE_REG 24, \buf, 320
	GC_SAVE_REG 25, \buf, 384
	GC_SAVE_REG 26, \buf, 448
.endm
.macro GC_RESTORE_REGS buf
	GC_RESTORE_REG 17, \buf, 0
	GC_RESTORE_REG 18, \buf, 32
	GC_RESTORE_REG 19, \buf, 64
	GC_RESTORE_REG 20, \buf, 96
	GC_RESTORE_REG 21, \buf, 128
	GC_RESTORE_REG 22, \buf, 192
	GC_RESTORE_REG 23, \buf, 256
	GC_RESTORE_REG 24, \buf, 320
	GC_RESTORE_REG 25, \buf, 384
	GC_RESTORE_REG 26, \buf, 448
.endm
	.text
	.global gc_save
	.ent gc_save
gc_save:
	#daddiu $sp, $sp, -32
	#sd $ra, 24($sp)

	la $t0, gc_regs($gp)
	clc $c16, $t0, 0($c0)
	GC_SAVE_REGS $c16
	#jal GC_ENTRY_POINT(gc_malloc)
	#nop
	#GC_RESTORE_REGS $c16
	#GC_INVALIDATE_UNUSED_REGS

	#ld $ra, 24($sp)
	#daddiu $sp, $sp, 32
	jr $ra
	nop
	#j GC_ENTRY_POINT(gc_malloc)
	#nop
	.global gc_restore
	.ent gc_restore
gc_restore:
	la $t0, gc_regs($gp)
	clc $c16, $t0, 0($c0)
	GC_RESTORE_REGS $c16
	GC_INVALIDATE_UNUSED_REGS
	jr $ra
	nop
#pragma clang diagnostic pop
