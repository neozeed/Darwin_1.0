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
 * @OSF_FREE_COPYRIGHT@
 */
/*
 * @APPLE_FREE_COPYRIGHT@
 */

/*
 *	Author: Bill Angell, Apple
 *	Date:	6/97
 *
 * exceptions and certain C functions write into a trace table which
 * can be examined via the machine 'lt' command under kdb
 */


#include <string.h>			/* For strcpy() */
#include <mach/boolean.h>
#include <machine/db_machdep.h>

#include <ddb/db_access.h>
#include <ddb/db_lex.h>
#include <ddb/db_output.h>
#include <ddb/db_command.h>
#include <ddb/db_sym.h>
#include <ddb/db_task_thread.h>
#include <ddb/db_command.h>		/* For db_option() */
#include <ddb/db_examine.h>
#include <ddb/db_expr.h>
#include <kern/thread.h>
#include <kern/task.h>
#include <mach/vm_param.h>
#include <ppc/Firmware.h>
#include <ppc/low_trace.h>
#include <ppc/db_low_trace.h>
#include <ppc/mappings.h>
#include <ppc/pmap.h>
#include <ppc/mem.h>
#include <ppc/pmap_internals.h>
#include <ppc/savearea.h>

void db_dumpphys(struct phys_entry *pp); 						/* Dump from physent */
void db_dumppca(struct mapping *mp); 						/* PCA */
void db_dumpmapping(struct mapping *mp); 					/* Dump out a mapping */

db_addr_t	db_low_trace_prev = 0;

/*
 *		Print out the low level trace table:
 *
 *		Displays the entry and 15 before it in newest to oldest order
 *		
 *		lt [entaddr]
 
 *		If entaddr is omitted, it starts with the most current
 *		If entaddr = 0, it starts with the most current and does the whole table
 */
void db_low_trace(db_expr_t addr, int have_addr, db_expr_t count, char * modif) {

	int		c, i;
	unsigned int tempx, cnt;
	unsigned int xbuf[8];
	unsigned int xTraceCurr, xTraceStart, xTraceEnd, cxltr, xxltr;
	db_addr_t	next_addr;
	LowTraceRecord xltr;
	unsigned char cmark;
	
	cnt = 16;													/* Default to 16 entries */
	
	ReadReal(((unsigned int)&traceCurr)&0x00000FFF, &xbuf[0]);	/* Get trace pointers (it's been moved to page 0) */
	xTraceCurr=xbuf[0];											/* Transfer current pointer */
	xTraceStart=xbuf[1];										/* Transfer start of table */
	xTraceEnd=xbuf[2];											/* Transfer end of table */
	
	if(addr == -1) cnt = 0x7FFFFFFF;							/* Max the count */

	if(!addr || (addr == -1)) {
		addr=xTraceCurr-sizeof(LowTraceRecord);					/* Start at the newest */
		if((unsigned int)addr<xTraceStart) addr=xTraceEnd-sizeof(LowTraceRecord);	/* Wrap low back to high */
	}
	
	if((unsigned int)addr<xTraceStart||(unsigned int)addr>=xTraceEnd) {	/* In the table? */
		db_printf("address not in low memory trace table\n");	/* Tell the fool */
		return;													/* Leave... */
	}

	if((unsigned int)addr&0x0000003F) {							/* Proper alignment? */
		db_printf("address not aligned on trace entry boundary (0x40)\n");	/* Tell 'em */
		return;													/* Leave... */
	}
	
	xxltr=(unsigned int)addr;									/* Set the start */
	cxltr=((xTraceCurr==xTraceStart ? xTraceEnd : xTraceCurr)-sizeof(LowTraceRecord));	/* Get address of newest entry */

	db_low_trace_prev = addr;									/* Starting point */

	for(i=0; i < cnt; i++) {									/* Dump the 16 (or all) entries */
	
		ReadReal(xxltr, (unsigned int *)&xltr);					/* Get the first half */
		ReadReal(xxltr+32, &(((unsigned int *)&xltr)[8]));		/* Get the second half */
		
		db_printf("\n%s%08X  %1X  %08X %08X - %04X\n", (xxltr!=cxltr ? " " : "*"), 
			xxltr,
			xltr.LTR_cpu, xltr.LTR_timeHi, xltr.LTR_timeLo, 
			(xltr.LTR_excpt&0x8000 ? 0xFFFF : xltr.LTR_excpt*64));	/* Print the first line */
		db_printf("              %08X %08X %08X %08X %08X %08X %08X\n",
			xltr.LTR_cr, xltr.LTR_srr0, xltr.LTR_srr1, xltr.LTR_dar, xltr.LTR_save, xltr.LTR_lr, xltr.LTR_ctr);
		db_printf("              %08X %08X %08X %08X %08X %08X\n",
			xltr.LTR_r0, xltr.LTR_r1, xltr.LTR_r2, xltr.LTR_r3, xltr.LTR_r4, xltr.LTR_r5);
	
		if((cnt != 16) && (xxltr == xTraceCurr)) break;			/* If whole table dump, exit when we hit start again... */

		xxltr-=sizeof(LowTraceRecord);							/* Back it on up */
		if(xxltr<xTraceStart)
			xxltr=(xTraceEnd-sizeof(LowTraceRecord));			/* Wrap low back to high */
	
	}
	db_next = (db_expr_t)(xxltr);
	return;
}


/*
 *		Print out 256 bytes
 *
 *		
 *		dl [entaddr]
 */
void db_display_long(db_expr_t addr, int have_addr, db_expr_t count, char * modif) {

	int				i;

	for(i=0; i<8; i++) {									/* Print 256 bytes */
		db_printf("%08X   %08X %08X %08X %08X  %08X %08X %08X %08X\n", addr,	/* Print a line */
			((unsigned long *)addr)[0], ((unsigned long *)addr)[1], ((unsigned long *)addr)[2], ((unsigned long *)addr)[3], 
			((unsigned long *)addr)[4], ((unsigned long *)addr)[5], ((unsigned long *)addr)[6], ((unsigned long *)addr)[7]);
		addr=(db_expr_t)((unsigned int)addr+0x00000020);	/* Point to next address */
	}
	db_next = addr;


}

/*
 *		Print out 256 bytes of real storage
 *
 *		Displays the entry and 15 before it in newest to oldest order
 *		
 *		dr [entaddr]
 */
void db_display_real(db_expr_t addr, int have_addr, db_expr_t count, char * modif) {

	int				i;
	unsigned int xbuf[8];

	for(i=0; i<8; i++) {									/* Print 256 bytes */
		ReadReal((unsigned int)addr, &xbuf[0]);				/* Get the real storage data */
		db_printf("%08X   %08X %08X %08X %08X  %08X %08X %08X %08X\n", addr,	/* Print a line */
			xbuf[0], xbuf[1], xbuf[2], xbuf[3], 
			xbuf[4], xbuf[5], xbuf[6], xbuf[7]);
		addr=(db_expr_t)((unsigned int)addr+0x00000020);	/* Point to next address */
	}
	db_next = addr;


}

unsigned int	dvspace = 0;

/*
 *		Print out virtual to real translation information
 *
 *		
 *		dm vaddr [space] (defaults to last entered) 
 */
void db_display_mappings(db_expr_t addr, int have_addr, db_expr_t count, char * modif) {

	int				i;
	unsigned int	xspace;

	mapping		*mp, *mpv;
	vm_offset_t	pa;
	
	if (db_expression(&xspace)) dvspace = xspace;			/* Get the space or set default */
	
	db_printf("mapping information for %08X in space %08X:\n", addr, dvspace);
	mp = hw_lock_phys_vir(dvspace, addr);					/* Lock the physical entry for this mapping */
	if(!mp) {												/* Did we find one? */
		db_printf("Not mapped\n");	
		return;												/* Didn't find any, return FALSE... */
	}
	if((unsigned int)mp&1) {								/* Did we timeout? */
		db_printf("Timeout locking physical entry for virtual address (%08X)\n", addr);	/* Yeah, scream about it! */
		return;												/* Bad hair day, return FALSE... */
	}
	printf("dumpaddr: space=%08X; vaddr=%08X\n", dvspace, addr);	/* Say what address were dumping */
	mpv = hw_cpv(mp);										/* Get virtual address of mapping */
	dumpmapping(mpv);
	if(mpv->physent) {
		hw_unlock_bit((unsigned int *)&mpv->physent->phys_link, PHYS_LOCK);	/* Unlock physical entry associated with mapping */
	}
	return;													/* Tell them we did it */


}

/*
 *		Prints out a mapping control block
 *
 */
 
void db_dumpmapping(struct mapping *mp) { 					/* Dump out a mapping */

	db_printf("Dump of mapping block: %08X\n", mp);			/* Header */
	db_printf("                 next: %08X\n", mp->next);                 
	db_printf("             hashnext: %08X\n", mp->hashnext);                 
	db_printf("              PTEhash: %08X\n", mp->PTEhash);                 
	db_printf("               PTEent: %08X\n", mp->PTEent);                 
	db_printf("              physent: %08X\n", mp->physent);                 
	db_printf("                 PTEv: %08X\n", mp->PTEv);                 
	db_printf("                 PTEr: %08X\n", mp->PTEr);                 
	db_printf("                 pmap: %08X\n", mp->pmap);
	
	if(mp->physent) {									/* Print physent if it exists */
		db_printf("Associated physical entry: %08X %08X\n", mp->physent->phys_link, mp->physent->pte1);
	}
	else {
		db_printf("Associated physical entry: none\n");
	}
	
	db_dumppca(mp);										/* Dump out the PCA information */
	
	return;
}

/*
 *		Prints out a PTEG control area
 *
 */
 
void db_dumppca(struct mapping *mp) { 						/* PCA */

	PCA				*pca;
	unsigned int	*pteg;
	
	pca = (PCA *)((unsigned int)mp->PTEhash&-64);		/* Back up to the start of the PCA */
	pteg=(unsigned int *)((unsigned int)pca-(((hash_table_base&0x0000FFFF)+1)<<16));
	db_printf(" Dump of PCA: %08X\n", pca);		/* Header */
	db_printf("     PCAlock: %08X\n", pca->PCAlock);                 
	db_printf("     PCAallo: %08X\n", pca->flgs.PCAallo);                 
	db_printf("     PCAhash: %08X %08X %08X %08X\n", pca->PCAhash[0], pca->PCAhash[1], pca->PCAhash[2], pca->PCAhash[3]);                 
	db_printf("              %08X %08X %08X %08X\n", pca->PCAhash[4], pca->PCAhash[5], pca->PCAhash[6], pca->PCAhash[7]);                 
	db_printf("Dump of PTEG: %08X\n", pteg);		/* Header */
	db_printf("              %08X %08X %08X %08X\n", pteg[0], pteg[1], pteg[2], pteg[3]);                 
	db_printf("              %08X %08X %08X %08X\n", pteg[4], pteg[5], pteg[6], pteg[7]);                 
	db_printf("              %08X %08X %08X %08X\n", pteg[8], pteg[9], pteg[10], pteg[11]);                 
	db_printf("              %08X %08X %08X %08X\n", pteg[12], pteg[13], pteg[14], pteg[15]);                 
	return;
}

/*
 *		Dumps starting with a physical entry
 */
 
void db_dumpphys(struct phys_entry *pp) { 						/* Dump from physent */

	mapping			*mp;
	PCA				*pca;
	unsigned int	*pteg;

	db_printf("Dump from physical entry %08X: %08X %08X\n", pp, pp->phys_link, pp->pte1);
	mp = hw_cpv(pp->phys_link);
	while(mp) {
		db_dumpmapping(mp);
		db_dumppca(mp);
		mp = hw_cpv(mp->next);
	}
	
	return;
}


/*
 *		Print out 256 bytes of virtual storage
 *
 *		
 *		dv [entaddr] [space]
 *		address must be on 32-byte boundary.  It will be rounded down if not
 */
void db_display_virtual(db_expr_t addr, int have_addr, db_expr_t count, char * modif) {

	int				i, size, lines, rlines;
	unsigned int 	xbuf[8];
	unsigned int	xspace;

	mapping		*mp, *mpv;
	vm_offset_t	pa;
	
	if (db_expression(&xspace)) dvspace = xspace;			/* Get the space or set default */
	
	addr&=-32;
	
	size = 4096 - (addr & 0x00000FFF);						/* Bytes left on page */
	lines = size / 32;										/* Number of lines in first or only part */
	if(lines > 8) lines = 8;
	rlines = 8 - lines;
	if(rlines < 0) lines = 0;
	
	db_printf("Dumping %08X (space=%08X); ", addr, dvspace);
	mp = hw_lock_phys_vir(dvspace, addr);					/* Lock the physical entry for this mapping */
	if(!mp) {												/* Did we find one? */
		db_printf("Not mapped\n");	
		return;												/* Didn't find any, return FALSE... */
	}
	if((unsigned int)mp&1) {								/* Did we timeout? */
		db_printf("Timeout locking physical entry for virtual address (%08X)\n", addr);	/* Yeah, scream about it! */
		return;												/* Bad hair day, return FALSE... */
	}
	mpv = hw_cpv(mp);										/* Get virtual address of mapping */
	if(!mpv->physent) {										/* Was there a physical entry? */
		pa = (vm_offset_t)((mpv->PTEr & -PAGE_SIZE) | ((unsigned int)addr & (PAGE_SIZE-1)));	/* Get physical address from physent */
	}
	else {
		pa = (vm_offset_t)((mpv->physent->pte1 & -PAGE_SIZE) | ((unsigned int)addr & (PAGE_SIZE-1)));	/* Get physical address from physent */
		hw_unlock_bit((unsigned int *)&mpv->physent->phys_link, PHYS_LOCK);		/* Unlock the physical entry */
	}
	db_printf("phys=%08X\n", pa);
	for(i=0; i<lines; i++) {								/* Print n bytes */
		ReadReal((unsigned int)pa, &xbuf[0]);				/* Get the real storage data */
		db_printf("%08X   %08X %08X %08X %08X  %08X %08X %08X %08X\n", addr,	/* Print a line */
			xbuf[0], xbuf[1], xbuf[2], xbuf[3], 
			xbuf[4], xbuf[5], xbuf[6], xbuf[7]);
		addr=(db_expr_t)((unsigned int)addr+0x00000020);	/* Point to next address */
		pa=(unsigned int)pa+0x00000020;						/* Point to next address */
	}
	db_next = addr;
	
	if(!rlines) return;
	
	db_printf("Dumping %08X (space=%08X); ", addr, dvspace);
	mp = hw_lock_phys_vir(dvspace, addr);					/* Lock the physical entry for this mapping */
	if(!mp) {												/* Did we find one? */
		db_printf("Not mapped\n");	
		return;												/* Didn't find any, return FALSE... */
	}
	if((unsigned int)mp&1) {								/* Did we timeout? */
		db_printf("Timeout locking physical entry for virtual address (%08X)\n", addr);	/* Yeah, scream about it! */
		return;												/* Bad hair day, return FALSE... */
	}
	mpv = hw_cpv(mp);										/* Get virtual address of mapping */
	if(!mpv->physent) {										/* Was there a physical entry? */
		pa = (vm_offset_t)((mpv->PTEr & -PAGE_SIZE) | ((unsigned int)addr & (PAGE_SIZE-1)));	/* Get physical address from physent */
	}
	else {
		pa = (vm_offset_t)((mpv->physent->pte1 & -PAGE_SIZE) | ((unsigned int)addr & (PAGE_SIZE-1)));	/* Get physical address from physent */
		hw_unlock_bit((unsigned int *)&mp->physent->phys_link, PHYS_LOCK);	/* Unlock the physical entry */
	}
	db_printf("phys=%08X\n", pa);
	for(i=0; i<rlines; i++) {								/* Print n bytes */
		ReadReal((unsigned int)pa, &xbuf[0]);				/* Get the real storage data */
		db_printf("%08X   %08X %08X %08X %08X  %08X %08X %08X %08X\n", addr,	/* Print a line */
			xbuf[0], xbuf[1], xbuf[2], xbuf[3], 
			xbuf[4], xbuf[5], xbuf[6], xbuf[7]);
		addr=(db_expr_t)((unsigned int)addr+0x00000020);	/* Point to next address */
		pa=(unsigned int)pa+0x00000020;						/* Point to next address */
	}
	db_next = addr;


}


/*
 *		Print out savearea stuff
 *
 *		
 *		ds 
 */

#define chainmax 32

void db_display_save(db_expr_t addr, int have_addr, db_expr_t count, char * modif) {

	int				i, j, totsaves, tottasks, taskact, chainsize;
	processor_set_t	pset;
	task_t			task;
	thread_act_t	act;
	savearea		*save;
	
	tottasks = 0;
	totsaves = 0;
	
	for(pset = (processor_set_t)all_psets.next; pset != (processor_set_t)&all_psets; pset = (processor_set_t)pset->all_psets.next) {	/* Go through PSETs */
		for(task = (task_t)pset->tasks.next; task != (task_t)&pset->tasks.next; task = (task_t)task->pset_tasks.next) {	/* Go through the tasks */
			taskact = 0;								/* Reset activation count */
			db_printf("\nTask %4d @%08X:\n", tottasks, task);	/* Show where we're at */
			for(act = (thread_act_t)task->thr_acts.next; act != (thread_act_t)&task->thr_acts; act = (thread_act_t)act->thr_acts.next) {	/* Go through activations */
				db_printf("   Act %4d @%08X - p: %08X  fp: %08X  fl: %08X  fc: %d  vp: %08X  vl: %08X  vp: %d\n",
					taskact, act, act->mact.pcb, act->mact.FPU_pcb, act->mact.FPU_lvl, act->mact.FPU_cpu, 		
					act->mact.VMX_pcb, act->mact.VMX_lvl, act->mact.VMX_cpu);
					
					
				save = (savearea *)act->mact.pcb; 		/* Set the start of the normal chain */
				chainsize = 0;
				while(save) {							/* Do them all */
					totsaves++;							/* Count savearea */
					db_printf("      Norm %08X: %08X %08X - tot = %d\n", save, save->save_srr0, save->save_srr1, totsaves);
					save = save->save_prev;				/* Next one */
					if(chainsize++ > chainmax) {		/* See if we might be in a loop */
						db_printf("      Chain terminated by count (%d) before %08X\n", chainmax, save);
						break;
					}
				}
			
				save = (savearea *)act->mact.FPU_pcb; 	/* Set the start of the floating point chain */
				chainsize = 0;
				while(save) {							/* Do them all */
					if(!(save->save_flags & SAVattach)) totsaves++;	/* Count savearea only if not a normal one also */
					db_printf("      FPU  %08X: %08X - tot = %d\n", save, save->save_level_fp, totsaves);
					save = save->save_prev_float;		/* Next one */
					if(chainsize++ > chainmax) {		/* See if we might be in a loop */
						db_printf("      Chain terminated by count (%d) before %08X\n", chainmax, save);
						break;
					}
				}
			
				save = (savearea *)act->mact.VMX_pcb; 	/* Set the start of the floating point chain */
				chainsize = 0;
				while(save) {							/* Do them all */
					if(!(save->save_flags & (SAVattach | SAVfpuvalid))) totsaves++;	/* Count savearea only if not a normal one also */
					db_printf("      Vec  %08X: %08X - tot = %d\n", save, save->save_level_vec, totsaves);
					save = save->save_prev_vector;		/* Next one */
					if(chainsize++ > chainmax) {		/* See if we might be in a loop */
						db_printf("      Chain terminated by count (%d) before %08X\n", chainmax, save);
						break;
					}
				}
				taskact++;
			}
			tottasks++;
		}
	}
	
	db_printf("Total saveareas accounted for: %d\n", totsaves);
	return;
}

/*
 *		Print out extra registers
 *
 *		
 *		dx 
 */

extern unsigned int dbfloats[33][2];
extern unsigned int dbvecs[33][4];
extern unsigned int dbspecrs[40];

void db_display_xregs(db_expr_t addr, int have_addr, db_expr_t count, char * modif) {

	int				i, j, pents;

	stSpecrs(dbspecrs);										/* Save special registers */
	db_printf("PIR:  %08X\n", dbspecrs[0]);
	db_printf("PVR:  %08X\n", dbspecrs[1]);
	db_printf("SDR1: %08X\n", dbspecrs[22]);
	db_printf("DBAT: %08X %08X  %08X %08X\n", dbspecrs[2], dbspecrs[3], dbspecrs[4], dbspecrs[5]);
	db_printf("      %08X %08X  %08X %08X\n", dbspecrs[6], dbspecrs[7], dbspecrs[8], dbspecrs[9]);
	db_printf("IBAT: %08X %08X  %08X %08X\n", dbspecrs[10], dbspecrs[11], dbspecrs[12], dbspecrs[13]);
	db_printf("      %08X %08X  %08X %08X\n", dbspecrs[14], dbspecrs[15], dbspecrs[16], dbspecrs[17]);
	db_printf("SPRG: %08X %08X  %08X %08X\n", dbspecrs[18], dbspecrs[19], dbspecrs[20], dbspecrs[21]);
	for(i = 0; i < 16; i += 8) {							/* Print 8 at a time */
		db_printf("SR%02d: %08X %08X %08X %08X %08X %08X %08X %08X\n", i,
			dbspecrs[23+i], dbspecrs[24+i], dbspecrs[25+i], dbspecrs[26+i], 
			dbspecrs[27+i], dbspecrs[28+i], dbspecrs[29+i], dbspecrs[30+i]); 
	}
	
	db_printf("\n");


	stFloat(dbfloats);										/* Save floating point registers */
	for(i = 0; i < 32; i += 4) {							/* Print 4 at a time */
		db_printf("F%02d: %08X %08X  %08X %08X  %08X %08X  %08X %08X\n", i,
			dbfloats[i][0], dbfloats[i][1], dbfloats[i+1][0], dbfloats[i+1][1], 
			dbfloats[i+2][0], dbfloats[i+2][1], dbfloats[i+3][0], dbfloats[i+3][1]); 
	}
	db_printf("FCR: %08X %08X\n", dbfloats[32][0], dbfloats[32][1]);	/* Print FSCR */
	
	if(!stVectors(dbvecs)) return;							/* Return if not Altivec capable */
	
	for(i = 0; i < 32; i += 2) {							/* Print 2 at a time */
		db_printf("V%02d: %08X %08X %08X %08X  %08X %08X %08X %08X\n", i,
			dbvecs[i][0], dbvecs[i][1], dbvecs[i][2], dbvecs[i][3], 
			dbvecs[i+1][0], dbvecs[i+1][1], dbvecs[i+1][2], dbvecs[i+1][3]); 
	}
	db_printf("VCR: %08X %08X %08X %08X\n", dbvecs[32][0], dbvecs[32][1], dbvecs[32][2], dbvecs[32][3]);	/* Print VSCR */

	return;													/* Tell them we did it */


}


/*
 *		Print out physical to vm system stuff
 *
 *		
 *		dp 
 */
void db_display_phys(db_expr_t addr, int have_addr, db_expr_t count, char * modif) {

#if 0
	int				i, j, pents;
	mapping		*mp;
	vm_offset_t	pa;
	struct phys_entry *pp;
	
	for (i = 0; i < pmap_mem_regions_count; i++) {			/* Step through the phys tables */
		for(j = 0; j < (pmap_mem_regions[i].end - pmap_mem_regions[i].start); j ++) {	/* Step through phys ents */
			pa = pmap_mem_regions[i].start + (j << 12);		/* Get paddr */
			pp = &pmap_mem_regions[i].phys_table[j];		/* Get phys_ent */
			db_printf(
		
		}
	}
		db_printf("pa=%08X\n", 
		if (pa < pmap_mem_regions[i].start)
			continue;
		if (pa >= pmap_mem_regions[i].end)
			return PHYS_NULL;
		
		entry = &pmap_mem_regions[i].phys_table[(pa - pmap_mem_regions[i].start) >> PPC_PGSHIFT];
		return entry;
	}



	mp = hw_lock_phys_vir(dvspace, addr);					/* Lock the physical entry for this mapping */
	if(!mp) {												/* Did we find one? */
		db_printf("Not mapped\n");	
		return;												/* Didn't find any, return FALSE... */
	}
	if((unsigned int)mp&1) {								/* Did we timeout? */
		db_printf("Timeout locking physical entry for virtual address (%08X)\n", addr);	/* Yeah, scream about it! */
		return;												/* Bad hair day, return FALSE... */
	}
	printf("dumpaddr: space=%08X; vaddr=%08X\n", dvspace, addr);	/* Say what address were dumping */
	dumpmapping(mp);
	if(mp->physent) {
		db_dumppca(mp);
		hw_unlock_bit(&mp->physent->pte1, PHYS_LOCK);		/* Unlock physical entry associated with mapping */
	}

#endif

	return;													/* Tell them we did it */


}



void Dumbo(void);
void Dumbo(void){
}
