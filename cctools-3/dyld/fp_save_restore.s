#ifdef __ppc__
/*
 * ppc_fp_save() and ppc_fp_restore() is passed a pointer to a buffer of 14
 * doubles into which the floating point registers are saved and restored.
 */
	.text
	.align 2
	.globl _ppc_fp_save
_ppc_fp_save:
	stfd	f0,0(r3)	; save all registers that could contain
	stfd	f1,8(r3)	;  parameters to the routine that is being
	stfd	f2,16(r3)	;  bound.
	stfd	f3,24(r3)
	stfd	f4,32(r3)
	stfd	f5,40(r3)
	stfd	f6,48(r3)
	stfd	f7,56(r3)
	stfd	f8,64(r3)
	stfd	f9,72(r3)
	stfd	f10,80(r3)
	stfd	f11,88(r3)
	stfd	f12,96(r3)
	stfd	f13,104(r3)
	blr

	.text
	.align 2
	.globl _ppc_fp_restore
_ppc_fp_restore:
	lfd	f0,0(r3)	; restore all registers that could contain
	lfd	f1,8(r3)	;  parameters to the routine that is being
	lfd	f2,16(r3)	;  bound.
	lfd	f3,24(r3)
	lfd	f4,32(r3)
	lfd	f5,40(r3)
	lfd	f6,48(r3)
	lfd	f7,56(r3)
	lfd	f8,64(r3)
	lfd	f9,72(r3)
	lfd	f10,80(r3)
	lfd	f11,88(r3)
	lfd	f12,96(r3)
	lfd	f13,104(r3)
	blr
	
/*
 * ppc_vec_save() and ppc_vec_restore() is passed a pointer to a buffer of 18
 * 128 bit vectors (16 byte aligned) into which the vector registers are saved
 * and restored.
 */
	.text
	.align 2
	.globl _ppc_vec_save
_ppc_vec_save:
	li	r4,0
	li	r5,16
	li	r6,32
	stvx	v2,r4,r3
	stvx	v3,r5,r3
	stvx	v4,r6,r3
	addi	r4,r4,48
	addi	r5,r5,48
	addi	r6,r6,48

	stvx	v5,r4,r3
	stvx	v6,r5,r3
	stvx	v7,r6,r3
	addi	r4,r4,48
	addi	r5,r5,48
	addi	r6,r6,48

	stvx	v8,r4,r3
	stvx	v9,r5,r3
	stvx	v10,r6,r3
	addi	r4,r4,48
	addi	r5,r5,48
	addi	r6,r6,48

	stvx	v11,r4,r3
	stvx	v12,r5,r3
	stvx	v13,r6,r3
	addi	r4,r4,48
	addi	r5,r5,48
	addi	r6,r6,48

	stvx	v14,r4,r3
	stvx	v15,r5,r3
	stvx	v16,r6,r3
	addi	r4,r4,48
	addi	r5,r5,48
	addi	r6,r6,48

	stvx	v17,r4,r3
	stvx	v18,r5,r3
	stvx	v19,r6,r3
	blr

	.text
	.align 2
	.globl _ppc_vec_restore
_ppc_vec_restore:
	li	r4,0
	li	r5,16
	li	r6,32
	lvx	v2,r4,r3
	lvx	v3,r5,r3
	lvx	v4,r6,r3
	addi	r4,r4,48
	addi	r5,r5,48
	addi	r6,r6,48

	lvx	v5,r4,r3
	lvx	v6,r5,r3
	lvx	v7,r6,r3
	addi	r4,r4,48
	addi	r5,r5,48
	addi	r6,r6,48

	lvx	v8,r4,r3
	lvx	v9,r5,r3
	lvx	v10,r6,r3
	addi	r4,r4,48
	addi	r5,r5,48
	addi	r6,r6,48

	lvx	v11,r4,r3
	lvx	v12,r5,r3
	lvx	v13,r6,r3
	addi	r4,r4,48
	addi	r5,r5,48
	addi	r6,r6,48

	lvx	v14,r4,r3
	lvx	v15,r5,r3
	lvx	v16,r6,r3
	addi	r4,r4,48
	addi	r5,r5,48
	addi	r6,r6,48

	stvx	v17,r4,r3
	stvx	v18,r5,r3
	stvx	v19,r6,r3
	blr
#endif /* __ppc__ */
