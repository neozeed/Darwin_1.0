/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * @OSF_COPYRIGHT@
 */

#ifndef	_PPC_ASM_H_
#define	_PPC_ASM_H_

#define	__ASMNL__	@
#define STRINGD .ascii

#ifdef ASSEMBLER


#define br0 0

#define ARG0 r3
#define ARG1 r4
#define ARG2 r5
#define ARG3 r6
#define ARG4 r7
#define ARG5 r8
#define ARG6 r9
#define ARG7 r10

#define tmp0	r0	/* Temporary GPR remapping (603e specific) */
#define tmp1	r1
#define tmp2	r2
#define tmp3	r3

/* SPR registers */

#define mq		0		/* MQ register for 601 emulation */
#define rtcu	4		/* RTCU - upper word of RTC for 601 emulation */
#define rtcl	5		/* RTCL - lower word of RTC for 601 emulation */
#define dsisr	18
#define ppcDAR	19
#define ppcdar	19
#define dar		19
#define SDR1	25
#define sdr1	25
#define srr0	26
#define srr1	27
#define vrsave	256		/* Vector Register save */
#define sprg0	272
#define sprg1	273
#define sprg2	274
#define sprg3	275
#define pvr		287

#define IBAT0U	528
#define IBAT0L	529
#define IBAT1U	530
#define IBAT1L	531
#define IBAT2U	532
#define IBAT2L	533
#define IBAT3U	534
#define IBAT3L	535
#define ibat0u	528
#define ibat0l	529
#define ibat1u	530
#define ibat1l	531
#define ibat2u	532
#define ibat2l	533
#define ibat3u	534
#define ibat3l	535

#define DBAT0U	536
#define DBAT0L	537
#define DBAT1U	538
#define DBAT1L	539
#define DBAT2U	540
#define DBAT2L	541
#define DBAT3U	542
#define DBAT3L	543
#define dbat0u	536
#define dbat0l	537
#define dbat1u	538
#define dbat1l	539
#define dbat2u	540
#define dbat2l	541
#define dbat3u	542
#define dbat3l	543

#define ummcr2	928		/* Performance monitor control */
#define ubamr	935		/* Performance monitor mask */
#define ummcr0	936		/* Performance monitor control */
#define upmc1	937		/* Performance monitor counter */
#define upmc2	938		/* Performance monitor counter */
#define usia	939		/* User sampled instruction address */
#define ummcr1	940		/* Performance monitor control */
#define upmc3	941		/* Performance monitor counter */
#define upmc4	942		/* Performance monitor counter */
#define usda	943		/* User sampled data address */
#define mmcr2	944		/* Performance monitor control */
#define bamr	951		/* Performance monitor mask */
#define mmcr0	952
#define pmc1	953
#define	pmc2	954
#define	sia		955
#define	mmcr1	956
#define	pmc3	957
#define	pmc4	958
#define	sda		959		/* Sampled data address */
#define dmiss	976		/* ea that missed */
#define dcmp	977		/* compare value for the va that missed */
#define hash1	978		/* pointer to first hash pteg */
#define	hash2	979		/* pointer to second hash pteg */
#define imiss	980		/* ea that missed */
#define icmp	981		/* compare value for the va that missed */
#define rpa		982		/* required physical address register */

#define HID0	1008	/* Checkstop and misc enables */
#define hid0	1008	/* Checkstop and misc enables */
#define HID1	1009	/* Clock configuration */
#define hid1	1009	/* Clock configuration */
#define iabr	1010	/* Instruction address breakpoint register */
#define dabr	1013	/* Data address breakpoint register */
#define msscr0	1014	/* Memory subsystem control */
#define msscr1	1015	/* Memory subsystem debug */
#define l2cr	1017	/* L2 Cache control */
#define ictc	1019	/* I-cache throttling control */
#define thrm1	1020	/* Thermal management 1 */
#define thrm2	1021	/* Thermal management 2 */
#define thrm3	1022	/* Thermal management 3 */
#define pir		1023	/* Processor ID Register */

;	hid0 bits
#define emcp	0
#define emcpm	0x80000000
#define dbp		1
#define dbpm	0x40000000
#define eba		2
#define ebam	0x20000000
#define ebd		3
#define ebdm	0x10000000
#define sbclk	4
#define sbclkm	0x08000000
#define eclk	6
#define eclkm	0x02000000
#define par		7
#define parm	0x01000000
#define doze	8
#define dozem	0x00800000
#define nap		9
#define napm	0x00400000
#define sleep	10
#define sleepm	0x00200000
#define dpm		11
#define dpmm	0x00100000
#define riseg	12
#define risegm	0x00080000
#define eiec	13
#define eiecm	0x00040000
#define nhr		15
#define nhrm	0x00010000
#define ice		16
#define icem	0x00008000
#define dce		17
#define dcem	0x00004000
#define ilock	18
#define ilockm	0x00002000
#define dlock	19
#define dlockm	0x00001000
#define icfi	20
#define icfim	0x00000800
#define dcfi	21
#define dcfim	0x00000400
#define spd		22
#define spdm	0x00000200
#define sge		24
#define sgem	0x00000080
#define dcfa	25
#define dcfam	0x00000040
#define btic	26
#define bticm	0x00000020
#define abe		28
#define abem	0x00000008
#define bht		29
#define bhtm	0x00000004
#define nopdst	30
#define nopdstm	0x00000002
#define nopti	31
#define noptim	0x00000001

;	msscr0 bits
#define shden	0
#define shdenm	0x80000000
#define shden3	1
#define shdenm3	0x40000000
#define l1intvs	2	
#define l1intve	4	
#define l1intvb	0x38000000	
#define l2intvs	5	
#define l2intve	7	
#define l2intvb	0x07000000	
#define dl1hwf	8
#define dl1hwfm	0x00800000
#define dbsiz	9
#define dbsizm	0x00400000
#define emode	10
#define emodem	0x00200000
#define abgd	11
#define abgdm	0x00100000
#define tfsts	24
#define tfste	25
#define tfstm	0x000000C0

;	msscr1 bits
#define cqd		15
#define cqdm	0x00010000
#define csqs	1
#define csqe	2
#define csqm	0x60000000

;	srr1 bits
#define icmck	1
#define icmckm	0x40000000
#define dcmck	2
#define dcmckm	0x20000000
#define l2mck	3
#define l2mckm	0x10000000
#define tlbmck	4
#define tlbmckm	0x08000000
#define brmck	5
#define brmckm	0x04000000
#define othmck	10
#define othmckm	0x00200000
#define l2dpmck	11
#define l2dpmckm	0x00100000
#define mcpmck	12
#define mcpmckm	0x00080000
#define teamck	13
#define teamckm	0x00040000
#define dpmck	14
#define dpmckm	0x00020000
#define apmck	15
#define apmckm	0x00010000

#define cr0_lt	0
#define cr0_gt	1
#define cr0_eq	2
#define cr0_so	3
#define cr0_un	3
#define cr1_lt	4
#define cr1_gt	5
#define cr1_eq	6
#define cr1_so	7
#define cr1_un	7
#define cr2_lt	8
#define cr2_gt	9
#define cr2_eq	10
#define cr2_so	11
#define cr2_un	11
#define cr3_lt	12
#define cr3_gt	13
#define cr3_eq	14
#define cr3_so	15
#define cr3_un	15
#define cr4_lt	16
#define cr4_gt	17
#define cr4_eq	18
#define cr4_so	19
#define cr4_un	19
#define cr5_lt	20
#define cr5_gt	21
#define cr5_eq	22
#define cr5_so	23
#define cr5_un	23
#define cr6_lt	24
#define cr6_gt	25
#define cr6_eq	26
#define cr6_so	27
#define cr6_un	27
#define cr7_lt	28
#define cr7_gt	29
#define cr7_eq	30
#define cr7_so	31
#define cr7_un	31

/*
 * Macros to access high and low word values of an address
 */

#define	HIGH_CADDR(x)	ha16(x)
#define	HIGH_ADDR(x)	hi16(x)
#define	LOW_ADDR(x)	lo16(x)

#endif	/* ASSEMBLER */

/* Tags are placed before Immediately Following Code (IFC) for the debugger
 * to be able to deduce where to find various registers when backtracing
 * 
 * We only define the values as we use them, see SVR4 ABI PowerPc Supplement
 * for more details (defined in ELF spec).
 */

#define TAG_NO_FRAME_USED 0x00000000

/* (should use genassym to get these offsets) */

#define FM_BACKPTR 0
#define	FM_CR_SAVE 4
#define FM_LR_SAVE 8 /* MacOSX is NOT following the ABI at the moment.. */
#define FM_SIZE    64   /* minimum frame contents, backptr and LR save. Make sure it is quadaligned */
#define FM_ARG0	   56
#define FM_ALIGN(l) ((l+15)&-16)

#define	FM_ELF_ARG0		8
#define	FM_MACHO_ARG0		56
#define	MACHO_SYSCALL_BEGIN	0x2000
#define	PK_SYSCALL_BEGIN	0x7000


/* redzone is the area under the stack pointer which must be preserved
 * when taking a trap, interrupt etc.
 */
#define FM_REDZONE 224				/* is ((32-14+1)*4) */

#define COPYIN_ARG0_OFFSET FM_ARG0

#ifdef	MACH_KERNEL
#include <mach_kdb.h>
#else	/* MACH_KERNEL */
#define MACH_KDB 0
#endif	/* MACH_KERNEL */

#define BREAKPOINT_TRAP tw	4,r4,r4

/* There is another definition of ALIGN for .c sources */
#ifndef __LANGUAGE_ASSEMBLY
#define ALIGN 4
#endif /* __LANGUAGE_ASSEMBLY */

#ifndef FALIGN
#define FALIGN 4 /* Align functions on words for now. Cachelines is better */
#endif

#define LB(x,n) n
#if	__STDC__
#define	LCL(x)	L ## x
#define EXT(x) _ ## x
#define LEXT(x) _ ## x ## :
#define LBc(x,n) n ## :
#define LBb(x,n) n ## b
#define LBf(x,n) n ## f
#else /* __STDC__ */
#define LCL(x) L/**/x
#define EXT(x) _/**/x
#define LEXT(x) _/**/x/**/:
#define LBc(x,n) n/**/:
#define LBb(x,n) n/**/b
#define LBf(x,n) n/**/f
#endif /* __STDC__ */

#define String	.asciz
#define Value	.word
#define Times(a,b) (a*b)
#define Divide(a,b) (a/b)

#define data16	.byte 0x66
#define addr16	.byte 0x67

#if !GPROF
#define MCOUNT
#endif /* GPROF */

#define ELF_FUNC(x)
#define ELF_DATA(x)
#define ELF_SIZE(x,s)

#define	Entry(x,tag)	.text@.align FALIGN@ .globl EXT(x)@ LEXT(x)
#define	ENTRY(x,tag)	Entry(x,tag)@MCOUNT
#define	ENTRY2(x,y,tag)	.text@ .align FALIGN@ .globl EXT(x)@ .globl EXT(y)@ \
			LEXT(x)@ LEXT(y) @\
			MCOUNT
#if __STDC__
#define	ASENTRY(x) 	.globl x @ .align FALIGN; x ## @ MCOUNT
#else
#define	ASENTRY(x) 	.globl x @ .align FALIGN; x @ MCOUNT
#endif /* __STDC__ */
#define	DATA(x)		.globl EXT(x) @ .align ALIGN @ LEXT(x)


#define End(x)		ELF_SIZE(x,.-x)
#define END(x)		End(EXT(x))
#define ENDDATA(x)	END(x)
#define Enddata(x)	End(x)

/* These defines are here for .c files that wish to reference global symbols
 * within __asm__ statements. 
 */
#define CC_SYM_PREFIX "_"

#endif /* _PPC_ASM_H_ */
