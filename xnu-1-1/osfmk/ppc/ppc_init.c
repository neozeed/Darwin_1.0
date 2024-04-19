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

#include <debug.h>
#include <mach_kdb.h>
#include <mach_kdp.h>

#include <kern/misc_protos.h>
#include <kern/thread.h>
#include <kern/processor.h>
#include <machine/machine_routines.h>
#include <ppc/boot.h>
#include <ppc/proc_reg.h>
#include <ppc/misc_protos.h>
#include <ppc/pmap.h>
#include <ppc/new_screen.h>
#include <ppc/exception.h>
#include <ppc/Firmware.h>
#include <ppc/savearea.h>

#include <pexpert/pexpert.h>

extern const char version[];

extern unsigned int intstack_top_ss;	/* declared in start.s */
#if	MACH_KDP || MACH_KDB
extern unsigned int debstackptr;	/* declared in start.s */
extern unsigned int debstack_top_ss;	/* declared in start.s */
#endif	/* MACH_KDP || MACH_KDB */

unsigned int kernel_seg_regs[] = {
  KERNEL_SEG_REG0_VALUE,	/* 0 */
  KERNEL_SEG_REG0_VALUE + 1,	/* 1 */
  KERNEL_SEG_REG0_VALUE + 2,	/* 2 */
  SEG_REG_INVALID, /* 3 */
  SEG_REG_INVALID, /* 4 */
  KERNEL_SEG_REG5_VALUE, /* 5 - I/O segment */
  SEG_REG_INVALID, /* 6 */
  SEG_REG_INVALID, /* 7 */
  KERNEL_SEG_REG8_VALUE, /* 8-F are possible IO space */
  KERNEL_SEG_REG9_VALUE,
  KERNEL_SEG_REG10_VALUE,
  KERNEL_SEG_REG11_VALUE,
  KERNEL_SEG_REG12_VALUE,
  KERNEL_SEG_REG13_VALUE,
  KERNEL_SEG_REG14_VALUE, /* 14 - A/V video */
  KERNEL_SEG_REG15_VALUE /* 15 - NuBus etc */
};

extern void thandler(void);     /* trap handler */
extern void ihandler(void);     /* interrupt handler */
extern void shandler(void);     /* syscall handler */
extern void fpu_switch(void);   /* fp handler */
extern void vec_switch(void);   /* vector handler */
extern void atomic_switch_trap(void);   /* fast path atomic thread switch */

void (*exception_handlers[])(void) = {
	thandler,	/* 0x000   INVALID EXCEPTION */
	thandler,	/* 0x100   System reset */
	thandler,	/* 0x200   Machine check */
	thandler,	/* 0x300   Data access */
	thandler,	/* 0x400   Instruction access */
	ihandler,	/* 0x500   External interrupt */
	thandler,	/* 0x600   Alignment */
	thandler,	/* 0x700   fp exc, ill/priv instr, trap  */
	fpu_switch,	/* 0x800   Floating point disabled */
	ihandler,	/* 0x900   Decrementer */
	thandler,	/* 0xA00   I/O controller interface */
	thandler,	/* 0xB00   INVALID EXCEPTION */
	shandler,	/* 0xC00   System call exception */
	thandler,	/* 0xD00   Trace */
	thandler,	/* 0xE00   FP assist */
	thandler,	/* 0xF00   Performance monitor */
	vec_switch,	/* 0xF20   VMX */
	thandler,	/* 0x1000  INVALID EXCEPTION */
	thandler,	/* 0x1100  INVALID EXCEPTION */
	thandler,	/* 0x1200  INVALID EXCEPTION */
	thandler,	/* 0x1300  instruction breakpoint */
	thandler,	/* 0x1400  system management */
	thandler,	/* 0x1500  INVALID EXCEPTION */
	thandler,	/* 0x1600  Altivec Assist */
	thandler,	/* 0x1700  INVALID EXCEPTION */
	thandler,	/* 0x1800  INVALID EXCEPTION */
	thandler,	/* 0x1900  INVALID EXCEPTION */
	thandler,	/* 0x1A00  INVALID EXCEPTION */
	thandler,	/* 0x1B00  INVALID EXCEPTION */
	thandler,	/* 0x1C00  INVALID EXCEPTION */
	thandler,	/* 0x1D00  INVALID EXCEPTION */
	thandler,	/* 0x1E00  INVALID EXCEPTION */
	thandler,	/* 0x1F00  INVALID EXCEPTION */
	thandler,	/* 0x2000  Run Mode/Trace */
	ihandler,	/* 0x2100  Signal processor */
	thandler	/* 0x2200  Preemption */
};

/* 
 * per_proc_info is accessed with VM switched off via sprg0 
 * Note that we always get enough space for an extra cache line.  
 * That is because for performance, we need to align the struct 
 * on a cache boundary.  With the extra space, we can shift the 
 * array up to 31 bytes to align 
 */
unsigned char per_proc_area[NCPUS*sizeof(struct per_proc_info)+32] = {0};
struct per_proc_info *per_proc_info;

void ppc_init(boot_args *args)
{
	int i;
	unsigned long *src,*dst;
	char *str;
	unsigned long	addr, videoAddr;
	unsigned int	maxmem;
	bat_t		    bat;
	extern vm_offset_t static_memory_end;
	
	/*
	 * Setup per_proc info for first cpu.
 	 */
	 
	/* First align the per_proc_info to a cache line */
	per_proc_info = (struct per_proc_info *)(((unsigned int)
						  (&per_proc_area[0])+31)
						 &(-32));
	per_proc_info[0].cpu_number = 0;
	per_proc_info[0].cpu_flags = 0;
	per_proc_info[0].istackptr = 0;	/* we're on the interrupt stack */
	per_proc_info[0].intstack_top_ss = intstack_top_ss;
#if	MACH_KDP || MACH_KDB
	per_proc_info[0].debstackptr = debstackptr;
	per_proc_info[0].debstack_top_ss = debstack_top_ss;
#endif	/* MACH_KDP || MACH_KDB */
	per_proc_info[0].phys_exception_handlers =
		(vm_offset_t)&exception_handlers;
	per_proc_info[0].get_interrupts_enabled =
		fake_get_interrupts_enabled;
	per_proc_info[0].set_interrupts_enabled =
		fake_set_interrupts_enabled;
	per_proc_info[0].virt_per_proc_info = (unsigned int)
		&per_proc_info[0];
	per_proc_info[0].active_kloaded = (unsigned int)
		&active_kloaded[0];
	per_proc_info[0].cpu_data = (unsigned int)
		&cpu_data[0];
	per_proc_info[0].active_stacks = (unsigned int)
		&active_stacks[0];
	per_proc_info[0].need_ast = (unsigned int)
		&need_ast[0];
	per_proc_info[0].FPU_thread = 0;

	machine_slot[0].is_cpu = TRUE;

	mtsprg(0, (unsigned int)&per_proc_info[0]);	/* Point sprg 0 at it */

	cpu_init();

	/*
	 * Setup some processor related structures to satisfy funnels.
	 * Must be done before using unparallelized device drivers.
	 */
	processor_ptr[0] = &processor_array[0];
	master_cpu = 0;
	master_processor = cpu_to_processor(master_cpu);

	/* Set up segment registers as VM through space 0 */
	for (i=0; i<=15; i++) {
	  isync();
	  mtsrin((KERNEL_SEG_REG0_VALUE | (i << 20)), i * 0x10000000);
	  sync();
	}

	static_memory_end = round_page(args->topOfKernelData);;
        /* Get platform expert set up */
	PE_init_platform(FALSE, args);


	/* This is how the BATs get configured */
	/* IBAT[0] maps Segment 0 1:1 */
	/* DBAT[0] maps Segment 0 1:1 */
	/* DBAT[2] maps the I/O Segment 1:1 */
	/* DBAT[3] maps the Video Segment 1:1 */

	/* If v_baseAddr is non zero, use DBAT3 to map the video segment */
	videoAddr = args->Video.v_baseAddr & 0xF0000000;
	if (videoAddr) {
	  /* start off specifying 1-1 mapping of video seg */
	  bat.upper.word	     = videoAddr;
	  bat.lower.word	     = videoAddr;
	  
	  bat.upper.bits.bl    = 0x7ff;	/* size = 256M */
	  bat.upper.bits.vs    = 1;
	  bat.upper.bits.vp    = 0;
	  
	  bat.lower.bits.wimg  = PTE_WIMG_IO;
	  bat.lower.bits.pp    = 2;	/* read/write access */
	  
	  sync();isync();
	  mtdbatu(3, BAT_INVALID); /* invalidate old mapping */
	  mtdbatl(3, bat.lower.word);
	  mtdbatu(3, bat.upper.word);
	  sync();isync();
	}
	
	/* Use DBAT2 to map the io segment */
	addr = get_io_base_addr() & 0xF0000000;
	if (addr != videoAddr) {
	  /* start off specifying 1-1 mapping of io seg */
	  bat.upper.word	     = addr;
	  bat.lower.word	     = addr;
	  
	  bat.upper.bits.bl    = 0x7ff;	/* size = 256M */
	  bat.upper.bits.vs    = 1;
	  bat.upper.bits.vp    = 0;
	  
	  bat.lower.bits.wimg  = PTE_WIMG_IO;
	  bat.lower.bits.pp    = 2;	/* read/write access */
	  
	  sync();isync();
	  mtdbatu(2, BAT_INVALID); /* invalidate old mapping */
	  mtdbatl(2, bat.lower.word);
	  mtdbatu(2, bat.upper.word);
	  sync();isync();
	}

#if 0
	GratefulDebInit((bootBumbleC *)&(args->Video));	/* Initialize the GratefulDeb debugger */
#endif

	/* setup debugging output if one has been chosen */
	PE_init_kprintf(FALSE);
	kprintf("kprintf initialized\n");

	/* create the console for verbose or pretty mode */
	PE_create_console();

	/* setup console output */
	PE_init_printf(FALSE);

#if DEBUG
	printf("\n\n\nThis program was compiled using gcc %d.%d for powerpc\n",
	       __GNUC__,__GNUC_MINOR__);

	/* Processor version information */
	{       
		unsigned int pvr;
		__asm__ ("mfpvr %0" : "=r" (pvr));
		printf("processor version register : 0x%08x\n",pvr);
	}
	for (i = 0; i < kMaxDRAMBanks; i++) {
		if (args->PhysicalDRAM[i].size) 
			printf("DRAM at 0x%08x size 0x%08x\n",
			       args->PhysicalDRAM[i].base,
			       args->PhysicalDRAM[i].size);
	}
#endif /* DEBUG */

	/*   
	 * VM initialization, after this we're using page tables...
	 */
	if (!PE_parse_boot_arg("maxmem", &maxmem))
		maxmem=0;
	else
		maxmem = maxmem * (1024 * 1024);

	ppc_vm_init(maxmem, args);

	PE_init_platform(TRUE, args);

	machine_startup(args);
}

ppc_init_cpu(
	struct per_proc_info *proc_info)
{
	int i;
	unsigned int gph;
	savectl *sctl;							/* Savearea controls */

	mtsprg(0, (unsigned int)proc_info);		/* Point sprg 0 at the per_proc_area */
	
	gph = save_get_phys();					/* Get a savearea (physical addressing) */
	mtsprg(1, gph);							/* Set physical address of savearea */
	
	cpu_init();

	/* Set up segment registers as VM through space 0 */
	for (i=0; i<=15; i++) {
	  isync();
	  mtsrin((KERNEL_SEG_REG0_VALUE | (i << 20)), i * 0x10000000);
	  sync();
	}

	ppc_vm_cpu_init(proc_info);

	slave_main();
}
