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
 *	This file is used to maintain the virtual to real mappings for a PowerPC machine.
 *	The code herein is primarily used to bridge between the pmap layer and the hardware layer.
 *	Currently, some of the function of this module is contained within pmap.c.  We may want to move
 *	all of this into it (or most anyway) for the sake of performance.  We shall see as we write it.
 *
 *	We also depend upon the structure of the phys_entry control block.  We do put some processor 
 *	specific stuff in there.
 *
 */

#include <cpus.h>
#include <debug.h>
#include <mach_kgdb.h>
#include <mach_vm_debug.h>
#include <db_machine_commands.h>

#include <kern/thread.h>
#include <kern/thread_act.h>
#include <mach/vm_attributes.h>
#include <mach/vm_param.h>
#include <vm/vm_kern.h>
#include <vm/vm_map.h>
#include <vm/vm_page.h>
#include <kern/spl.h>

#include <kern/misc_protos.h>
#include <ppc/misc_protos.h>
#include <ppc/proc_reg.h>

#include <vm/pmap.h>
#include <ppc/pmap.h>
#include <ppc/pmap_internals.h>
#include <ppc/mem.h>

#include <ppc/new_screen.h>
#include <ppc/Firmware.h>
#include <ppc/mappings.h>
#include <ppc/miscserv.h>
#include <ddb/db_output.h>

#define PERFTIMES 0

#if PERFTIMES && DEBUG
#define debugLog2(a, b, c) dbgLog2(a, b, c)
#else
#define debugLog2(a, b, c)
#endif

autogenblok		*AutoGenList;									/* Anchor for the autogeb list */
space_t			curr_spaceID = 0;								/* Highest used space ID */
unsigned int	incrVSID = 0;									/* VSID increment value */
unsigned int	mappingdeb0 = 0;						
extern unsigned int	hash_table_size;						
extern unsigned int	debsave0;						
extern unsigned int dbdbdb;										/* (TEST/DEBUG) */
extern vm_offset_t mem_size;
/*
 *	ppc_prot translates from the mach representation of protections to the PPC version.
 *	Calculation of it like this saves a memory reference - and maybe a couple of microseconds.
 *	It eliminates the used of this table.
 *	unsigned char ppc_prot[8] = { 0, 3, 2, 2, 3, 3, 2, 2 };
 */

#define ppc_prot(p) ((0xAFAC >> (p << 1)) & 3)

/*
 *		mapping_space();
 *			This function is called to generate an address space ID. This space ID must be unique within
 *			the system.  For the PowerPC, it is used to build the VSID.  We build a VSID in the following
 *			way:  space ID << 4 | segment.  Since a VSID is 24 bits, and out of that, we reserve the last
 *			4, so, we can have 2^20 (2M) unique IDs.  Each pmap has a unique space ID, so we should be able
 *			to have 2M pmaps at a time, which we couldn't, we'd run out of memory way before then.  The 
 *			problem is that only a certain number of pmaps are kept in a free list and if that is full,
 *			they are release.  This causes us to lose track of what space IDs are free to be reused.
 *			We can do 4 things: 1) not worry about it, 2) keep all free pmaps, 3) rebuild all mappings
 *			when the space ID wraps, or 4) scan the list of pmaps and find a free one.
 *
 *			Yet another consideration is the hardware use of the VSID.  It is used as part of the hash
 *			calculation for virtual address lookup.  An improperly chosen value could potentially cause
 *			too many hashes to hit the same bucket, causing PTEG overflows.  The actual hash function
 *			is (page index XOR vsid) mod number of ptegs. For a 32MB machine, using the suggested
 *			hash table size, there are 2^12 (8192) PTEGs.  Remember, though, that the bottom 4 bits
 *			are reserved for the segment number, which means that we really have 2^(12-4) 512 space IDs
 *			before we start hashing to the same buckets with the same vaddrs. Also, within a space ID,
 *			every 8192 pages (32MB) within a segment will hash to the same bucket.  That's 8 collisions
 *			per segment.  So, a scan of every page for 256MB would fill 32 PTEGs completely, but
 *			with no overflow.  I don't think that this is a problem.
 *
 *			There may be a problem with the space ID, though. A new space ID is generate (mainly) 
 *			whenever there is a fork.  There shouldn't really be any problem because (for a 32MB
 *			machine) we can have 512 pmaps and still not have hash collisions for the same address.
 *			The potential problem, though, is if we get long-term pmaps that have space IDs that are
 *			the same modulo 512.  We can reduce this problem by having the segment number be bits
 *			0-3 of the space ID rather than 20-23.  Doing this means that, in effect, corresponding
 *			vaddrs in different segments hash to the same PTEG. While this is somewhat of a problem,
 *			I don't think that it is as signifigant as the other, so, I'll make the space ID
 *			with segment first.
 *
 *			The final, and biggest problem is the wrap, which will happen every 2^20 space IDs.
 *			While this is a problem that should only happen in periods counted in weeks, it can and
 *			will happen.  This is assuming a monotonically increasing space ID. If we were to search
 *			for an inactive space ID, there could not be a wrap until there was 2^20 concurrent space IDs.
 *			That's pretty unlikely to happen.  There couldn't be enough storage to support a million tasks.
 *			Another potential solution is to monitor for the wrap, and reorganize the space IDs when it happens.
 *			This is rather severe, and would have user-perceivable performance impact.  It would be necessary
 *			to quiese all other processors, invalidate and purge the entire hash table, and then to reassign
 *			all space IDs in active pmaps.  It may be better to amortize the problem by keeping pmaps in sorted
 *			order and keeping track of the lowest unused space ID.  I'll think on this one and do a panic
 *			until I got it sussed.
 */

space_t mapping_space(void) {									/* Generate a unique space ID */

	register space_t	currSID, nextSID;					
	
	while(1) {													/* Keep trying until something happens */
		currSID=curr_spaceID;									/* Get a copy of the current ID */
		if(!(nextSID = ((currSID + incrVSID) & SID_MAX))) {		/* Get the next one and check if we wrapped */
			panic("Address space ID wrapped;  Temporarily fatal system error.  Add more code here...\n");	/* Die */
		}
		if(hw_compare_and_store(currSID, nextSID, &curr_spaceID)) {	/* Have we found a good one yet? */
			debugLog2(0, nextSID, 0);							/* Log mapping_space call */
			return (nextSID);									/* Yeah, return it... */
		}														/* save the new  and exit if not, */
	}
}


/*
 *		mapping_init();
 *			Do anything that needs to be done before the mapping system can be used.
 *			Hash table must be initialized before we call this.
 *
 *			Calculate the SID increment.  Currently we use size^(1/2) + size^(1/4) + 1;
 */

void mapping_init(void) {

	unsigned int tmp;
	
	__asm__ volatile("cntlzw %0, %1" : "=r" (tmp) : "r" (hash_table_size));	/* Get number of leading 0s */

	incrVSID = 1 << ((32 - tmp + 1) >> 1);						/* Get ceiling of sqrt of table size */
	incrVSID |= 1 << ((32 - tmp + 1) >> 2);						/* Get ceiling of quadroot of table size */
	incrVSID |= 1;												/* Set bit and add 1 */
	return;

}


/*
 *		mapping_remove(space_t space, vm_offset_t va);
 *			Given a space ID and virtual address, this routine finds the mapping and removes if from
 *			both its PTEG hash list and the physical entry list.  The mapping block will be added to
 *			the free list.  If the free list threshold is reached, garbage collection will happen.
 *			We also kick back a return code to say whether or not we had one to remove.
 *
 *			We have a strict ordering here:  the mapping must be removed from the PTEG hash list before
 *			it can be removed from the physical entry list.  This allows us to get by with only the PTEG
 *			hash lock at page fault time. The physical entry lock must be held while we remove the mapping 
 *			from both lists. The PTEG lock is one of the lowest level locks.  No PTE fault, interruptions,
 *			losing control, getting other locks, etc., are allowed when you hold it. You do, and you die.
 *			It's just that simple!
 *
 *			When the phys_entry lock is held, the mappings chained to that one are guaranteed to stay around.
 *			However, a mapping's order on the PTEG hash chain is not.  The interrupt handler uses the PTEG
 *			lock to control the hash cahin and may move the position of the mapping for MRU calculations.
 *
 *			Note that mappings do not need to point to a physical entry. When they don't, it indicates 
 *			the mapping is outside of physical memory and usually refers to a memory mapped device of
 *			some sort.  Naturally, we can't lock what we don't have, so the phys entry lock and unlock
 *			routines return normally, but don't do anything.
 */

boolean_t mapping_remove(space_t space, vm_offset_t va) {		/* Remove a single mapping for this VADDR 
																   Returns TRUE if a mapping was found to remove */

	mapping		*mp, *mpv;
	spl_t 		s;
	
	debugLog2(1, va, space);									/* start mapping_remove */
	s=splhigh();												/* Don't bother me */
	
	mp = hw_lock_phys_vir(space, va);							/* Lock the physical entry for this mapping */

	if(!mp) {													/* Did we find one? */
		splx(s);												/* Restore the interrupt level */
		debugLog2(2, 0, 0);										/* end mapping_remove */
		return FALSE;											/* Didn't find any, return FALSE... */
	}
	if((unsigned int)mp&1) {									/* Did we timeout? */
		panic("mapping_remove: timeout locking physical entry\n");	/* Yeah, scream about it! */
		splx(s);												/* Restore the interrupt level */
		return FALSE;											/* Bad hair day, return FALSE... */
	}
	
	mpv = hw_cpv(mp);											/* Get virtual address of mapping */
	if(mpv->pmap) {												/* Check for a pmap */
#if DEBUG
		if(hw_atomic_sub(&mpv->pmap->stats.resident_count, 1) < 0) panic("pmap resident count went negative\n");
#else
		(void)hw_atomic_sub(&mpv->pmap->stats.resident_count, 1);	/* Decrement the resident page count */
#endif
	}
	
	hw_rem_map(mp);												/* Remove the corresponding mapping */
	
	if(mpv->physent)hw_unlock_bit((unsigned int *)&mpv->physent->phys_link, PHYS_LOCK);	/* Unlock physical entry associated with mapping */
	
	splx(s);													/* Was there something you needed? */
		
	mapping_free(mpv);											/* Add mapping to the free list */
	debugLog2(2, 1, 0);											/* end mapping_remove */
	return TRUE;												/* Tell them we did it */
}

/*
 *		mapping_purge(struct phys_entry *pp) - release all mappings for this physent to the free list 
 *
 *		This guy releases any mappings that exist for a physical page.
 *		We get the lock on the phys_entry, and hold it through out this whole routine.
 *		That way, no one can change the queue out from underneath us.  We keep fetching
 *		the physents mapping anchor until it is null, then we're done.  
 *
 *		For each mapping, we call the remove routine to remove it from the PTEG hash list and 
 *		decriment the pmap's residency count.  Then we release the mapping back to the free list.
 *
 */
 
void mapping_purge(struct phys_entry *pp) {						/* Remove all mappings for this physent */

	mapping		*mp, *mpv;
	spl_t 		s;
		
	s=splhigh();												/* Don't bother me */
	debugLog2(3, pp->pte1, 0);									/* start mapping_purge */
	
	if(!hw_lock_bit((unsigned int *)&pp->phys_link, PHYS_LOCK, LockTimeOut)) {		/* Lock the physical entry */
		panic("\nmapping_purge: Timeout attempting to lock physical entry at %08X: %08X %08X\n", 
			pp, pp->phys_link, pp->pte1);	/* Complain about timeout */
	}
	
	while(mp = (mapping *)((unsigned int)pp->phys_link & ~PHYS_FLAGS)) {	/* Keep going so long as there's another */

		mpv = hw_cpv(mp);										/* Get the virtual address */
		if(mpv->pmap) {											/* See if there is a pmap to worry about */
#if DEBUG
			if(hw_atomic_sub(&mpv->pmap->stats.resident_count, 1) < 0) panic("pmap resident count went negative\n");
#else
			(void)hw_atomic_sub(&mpv->pmap->stats.resident_count, 1);	/* Decrement the resident page count */
#endif
		}
	
		hw_rem_map(mp);											/* Remove the mapping */
		mapping_free(mpv);										/* Add mapping to the free list */
	}
		
	hw_unlock_bit((unsigned int *)&pp->phys_link, PHYS_LOCK);	/* We're done, unlock the physical entry */
	
	debugLog2(4, pp->pte1, 0);									/* end mapping_purge */
	splx(s);													/* Was there something you needed? */
	return;														/* Tell them we did it */
}


/*
 *		mapping_make(pmap, space, pp, va, spa, prot, attr) - map a virtual address to a real one 
 *
 *		This routine takes the given parameters, builds a mapping block, and queues it into the 
 *		correct lists.
 *		
 *		The pmap and pp parameters can be null.  This allows us to make a mapping that is not
 *		associated with any physical page or pmap.  We may need this for certain I/O areas.
 *
 *		If the pmap is null, we don't diddle with it, i.e., the residency counts.
 *		If the phys_entry address is null, we neither lock or chain into it.
 */
 
mapping *mapping_make(pmap_t pmap, space_t space, struct phys_entry *pp, vm_offset_t va, vm_offset_t pa, vm_prot_t prot, int attr) {	/* Make an address mapping */

	register mapping *mp, *mpv;
	spl_t 		s;
	
	debugLog2(5, va, pa);										/* start mapping_purge */
	mpv = mapping_alloc();										/* Get a spare mapping block */
	
	mpv->pmap = pmap;											/* Initialize the pmap pointer */
	mpv->physent = pp;											/* Initialize the pointer to the physical entry */
	mpv->PTEr = ((unsigned int)pa & ~(PAGE_SIZE - 1)) | attr<<3 | ppc_prot(prot);	/* Build the real portion of the PTE */
	mpv->PTEv = (((unsigned int)va >> 1) & 0x78000000) | (space << 7) | (((unsigned int)va >> 22) & 0x0000003F);	/* Build the VSID */

	s=splhigh();												/* Don't bother from now on */
	
	mp = hw_cvp(mpv);											/* Get the physical address of this */

	if(pp) {													/* Is there a physical entry? */
		if(!hw_lock_bit((unsigned int *)&pp->phys_link, PHYS_LOCK, LockTimeOut)) {	/* Lock the physical entry */
			panic("\nmapping_make: Timeout attempting to lock physical entry at %08X: %08X %08X\n", 
				pp, pp->phys_link, pp->pte1);					/* Complain about timeout */
		}
		
		mpv->next = (mapping *)((unsigned int)pp->phys_link & ~PHYS_FLAGS);		/* Move the old anchor to the new mappings forward */
		pp->phys_link = (mapping *)((unsigned int)mp | (unsigned int)pp->phys_link & PHYS_FLAGS);	/* Point the anchor at us.  Now we're on the list (keep the flags) */
	}
	
	hw_add_map(mp, space, va);									/* Stick it on the PTEG hash list */
	
	if(pmap) {													/* If there is a pmap, adjust the residency count */
		(void)hw_atomic_add(&mpv->pmap->stats.resident_count, 1);	/* Increment the resident page count */
	}
	
	if(pp)hw_unlock_bit((unsigned int *)&pp->phys_link, PHYS_LOCK);	/* If we have one, unlock the physical entry */
		
	splx(s);													/* Ok for interruptions now */
	debugLog2(6, space, prot);									/* end mapping_purge */
	return mpv;													/* Leave... */
}


/*
 *		mapping_remap(space_t space, vm_offset_t from, vm_offset_t to) - remap a physical address to a different virtual address
 *
 *		This routine takes the given parameters, finds the mapping for the "from" address
 *		and removes it.  Then it modifies the virtual address and adds it back.
 *		
 *		It returns failure if the "from" address is not mapped.
 */
 
int	mapping_remap(space_t space, vm_offset_t from, vm_offset_t to) {	/* Remap an address mapping */

	register mapping *mp, *mpv;
	spl_t 		s;
	
	s=splhigh();												/* Don't bother from now on */
	
	mp = hw_lock_phys_vir(space, from);							/* Lock the physical entry for this mapping */
	debugLog2(7, from, to);										/* start remap */
	if(!mp) {													/* Did we find one? */
		splx(s);												/* Restore the interrupt level */
		return FALSE;											/* Didn't find source, return FALSE... */
	}
	if((unsigned int)mp&1) {									/* Did we timeout? */
		panic("mapping_remap: timeout locking physical entry\n");	/* Yeah, scream about it! */
		splx(s);												/* Restore the interrupt level */
		return FALSE;											/* Bad hair day, return FALSE... */
	}
	
	hw_rem_map(mp);												/* Remove from the old address. Removes from both PTEG and physent lists */	

	mpv = hw_cpv(mp);											/* Convert to virtual address */

	mpv->PTEv = (((unsigned int)to >> 1) & 0x78000000) | 
		(space << 7) | (((unsigned int)to >> 22) & 0x0000003F);	/* Rebuild the VSID */
	
	if(mpv->physent) {											/* Is there a physical entry? */		
		mpv->next = (mapping *)((unsigned int)mpv->physent->phys_link & ~PHYS_FLAGS);	/* Move the old anchor to the new mappings forward */
		mpv->physent->phys_link = (mapping *)((unsigned int)mp | ((unsigned int)mpv->physent->phys_link & PHYS_FLAGS));	/* Point the anchor at us.  Now we're back on the list (keep the flags) */
	}
	
	hw_add_map(mp, space, to);									/* Map it to the new one */
	
	if(mp->physent)hw_unlock_bit((unsigned int *)&mpv->physent->phys_link, PHYS_LOCK);	/* Unlock physical entry associated with mapping */
		
	debugLog2(8, space, 0);										/* end remap */
	splx(s);													/* Ok for interruptions now */
	return TRUE;												/* Leave... */
}


/*
 *		void mapping_protect(phys_entry *pp, vm_prot_t prot, boolean_t locked) - change the protection of a physical page
 *
 *		This routine takes a physical entry and runs through all mappings attached to it and changes
 *		the protection.  If there are PTEs associated with the mappings, they will be invalidated before
 *		the protection is changed.  We don't try to save the PTE.  We won't worry about the LRU calculations
 *		either (I don't think, maybe I'll change my mind later).  There is no limitation on changes, e.g., 
 *		higher to lower, lower to higher.
 *
 *		If locked is true, the physent is already locked and should not be unlocked at exit.
 *
 *		Interruptions should be disabled at entry.
 */

void mapping_protect(struct phys_entry *pp, vm_prot_t prot, boolean_t locked) {	/* Change protection of all mappings to page */

	debugLog2(9, pp->pte1, prot);								/* end remap */
	if(!locked) {												/* Do we need to lock the physent? */
		if(!hw_lock_bit((unsigned int *)&pp->phys_link, PHYS_LOCK, LockTimeOut)) {	/* Lock the physical entry */
			panic("\nmapping_protect: Timeout attempting to lock physical entry at %08X: %08X %08X\n", 
				pp, pp->phys_link, pp->pte1);					/* Complain about timeout */
		}
	}	

	hw_prot(pp, ppc_prot(prot));								/* Go set the protection on this physical page */

	if(!locked) hw_unlock_bit((unsigned int *)&pp->phys_link, PHYS_LOCK);		/* We're done, unlock the physical entry */
	debugLog2(10, pp->pte1, 0);									/* end remap */
	
	return;														/* Leave... */
}

/*
 *		mapping_phys_attr(struct phys_entry *pp, vm_prot_t prot, unsigned int wimg) Sets the default physical page attributes
 *
 *		This routine takes a physical entry and sets the physical attributes.  There can be no mappings
 *		associated with this page when we do it.
 */

void mapping_phys_attr(struct phys_entry *pp, vm_prot_t prot, unsigned int wimg) {	/* Sets the default physical page attributes */

	debugLog2(11, pp->pte1, prot);								/* end remap */

	if(!hw_lock_bit((unsigned int *)&pp->phys_link, PHYS_LOCK, LockTimeOut)) {	/* Lock the physical entry */
		panic("\nmapping_phys_attr: Timeout attempting to lock physical entry at %08X: %08X %08X\n", 
			pp, pp->phys_link, pp->pte1);						/* Complain about timeout */
	}

//	if(pp->phys_link) panic("\nmapping_phys_attr: attempt to change default attributes when mappings exist!\n");

	hw_phys_attr(pp, ppc_prot(prot), wimg);						/* Go set the default WIMG and protection */

	hw_unlock_bit((unsigned int *)&pp->phys_link, PHYS_LOCK);	/* We're done, unlock the physical entry */
	debugLog2(12, pp->pte1, wimg);								/* end remap */
	
	return;														/* Leave... */
}

/*
 *		void mapping_invall(phys_entry *pp) - invalidates all ptes associated with a page
 *
 *		This routine takes a physical entry and runs through all mappings attached to it and invalidates
 *		any PTEs it finds.
 *
 *		Interruptions must be disabled and the physical entry locked at entry.
 */

void mapping_invall(struct phys_entry *pp) {					/* Clear all PTEs pointing to a physical page */

	hw_inv_all(pp);												/* Go set the change bit of a physical page */
	
	return;														/* Leave... */
}


/*
 *		void mapping_clr_mod(phys_entry *pp) - clears the change bit of a physical page
 *
 *		This routine takes a physical entry and runs through all mappings attached to it and turns
 *		off the change bit.  If there are PTEs associated with the mappings, they will be invalidated before
 *		the change bit is changed.  We don't try to save the PTE.  We won't worry about the LRU calculations
 *		either (I don't think, maybe I'll change my mind later).
 *
 *		Interruptions must be disabled and the physical entry locked at entry.
 */

void mapping_clr_mod(struct phys_entry *pp) {					/* Clears the change bit of a physical page */

	hw_clr_mod(pp);												/* Go clear the change bit of a physical page */
	return;														/* Leave... */
}


/*
 *		void mapping_set_mod(phys_entry *pp) - set the change bit of a physical page
 *
 *		This routine takes a physical entry and runs through all mappings attached to it and turns
 *		on the change bit.  If there are PTEs associated with the mappings, they will be invalidated before
 *		the change bit is changed.  We don't try to save the PTE.  We won't worry about the LRU calculations
 *		either (I don't think, maybe I'll change my mind later).
 *
 *		Interruptions must be disabled and the physical entry locked at entry.
 */

void mapping_set_mod(struct phys_entry *pp) {					/* Sets the change bit of a physical page */

	hw_set_mod(pp);												/* Go set the change bit of a physical page */
	return;														/* Leave... */
}


/*
 *		void mapping_clr_ref(struct phys_entry *pp) - clears the reference bit of a physical page
 *
 *		This routine takes a physical entry and runs through all mappings attached to it and turns
 *		off the reference bit.  If there are PTEs associated with the mappings, they will be invalidated before
 *		the reference bit is changed.  We don't try to save the PTE.  We won't worry about the LRU calculations
 *		either (I don't think, maybe I'll change my mind later).
 *
 *		Interruptions must be disabled at entry.
 */

void mapping_clr_ref(struct phys_entry *pp) {					/* Clears the reference bit of a physical page */

	mapping	*mp;

	debugLog2(13, pp->pte1, 0);									/* end remap */
	if(!hw_lock_bit((unsigned int *)&pp->phys_link, PHYS_LOCK, LockTimeOut)) {	/* Lock the physical entry for this mapping */
		panic("Lock timeout getting lock on physical entry\n");	/* Just die... */
	}
	hw_clr_ref(pp);												/* Go clear the reference bit of a physical page */
	hw_unlock_bit((unsigned int *)&pp->phys_link, PHYS_LOCK);	/* Unlock physical entry */
	debugLog2(14, pp->pte1, 0);									/* end remap */
	return;														/* Leave... */
}


/*
 *		void mapping_set_ref(phys_entry *pp) - set the reference bit of a physical page
 *
 *		This routine takes a physical entry and runs through all mappings attached to it and turns
 *		on the reference bit.  If there are PTEs associated with the mappings, they will be invalidated before
 *		the reference bit is changed.  We don't try to save the PTE.  We won't worry about the LRU calculations
 *		either (I don't think, maybe I'll change my mind later).
 *
 *		Interruptions must be disabled and the physical entry locked at entry.
 */

void mapping_set_ref(struct phys_entry *pp) {					/* Sets the reference bit of a physical page */

	hw_set_ref(pp);												/* Go set the reference bit of a physical page */
	return;														/* Leave... */
}


/*
 *		void mapping_tst_mod(phys_entry *pp) - test the change bit of a physical page
 *
 *		This routine takes a physical entry and runs through all mappings attached to it and tests
 *		the changed bit.  If there are PTEs associated with the mappings, they will be invalidated before
 *		the changed bit is tested.  We don't try to save the PTE.  We won't worry about the LRU calculations
 *		either (I don't think, maybe I'll change my mind later).
 *
 *		Interruptions must be disabled and the physical entry locked at entry.
 */

boolean_t mapping_tst_mod(struct phys_entry *pp) {				/* Tests the change bit of a physical page */

	return(hw_tst_mod(pp));										/* Go test the change bit of a physical page */
}


/*
 *		void mapping_tst_ref(phys_entry *pp) - tests the reference bit of a physical page
 *
 *		This routine takes a physical entry and runs through all mappings attached to it and tests
 *		the reference bit.  If there are PTEs associated with the mappings, they will be invalidated before
 *		the reference bit is changed.  We don't try to save the PTE.  We won't worry about the LRU calculations
 *		either (I don't think, maybe I'll change my mind later).
 *
 *		Interruptions must be disabled and the physical entry locked at entry.
 */

boolean_t mapping_tst_ref(struct phys_entry *pp) {				/* Tests the reference bit of a physical page */

	return(hw_tst_ref(pp));										/* Go test the reference bit of a physical page */
}


/*
 *		void mapping_phys_init(physent, wimg) - fills in the default processor dependent areas of the phys ent
 *
 *		Currently, this sets the default word 1 of the PTE.  The only bits set are the WIMG bits
 */

void mapping_phys_init(struct phys_entry *pp, unsigned int pa, unsigned int wimg) {		/* Initializes hw specific storage attributes */

	pp->pte1 = (pa & -PAGE_SIZE) | ((wimg << 3) & 0x00000078);	/* Set the WIMG and phys addr in the default PTE1 */

	return;														/* Leave... */
}


/*
 *		mapping_adjust(void) - Releases free mapping blocks and/or allocates new ones 
 *
 *		This routine frees any mapping blocks queued to mapCtl.mapcrel. It also checks
 *		the number of free mappings remaining, and if below a threshold, replenishes them.
 *		The list will be replenshed from mapCtl.mapcrel if there are enough.  Otherwise,
 *		a new one is allocated.
 *
 *		This routine allocates and/or memory and must be called from a safe place. 
 *		Currently, vm_pageout_scan is the safest place. We insure that the 
 */

thread_call_t				mapping_adjust_call;
static thread_call_data_t	mapping_adjust_call_data;

void mapping_adjust(void) {										/* Adjust free mappings */

	kern_return_t	retr;
	mappingblok	*mb, *mbn;
	spl_t			s;
	int				allocsize, i;

	if(mapCtl.mapcmin <= MAPPERBLOK) {							/* Do the first time only */
		mapCtl.mapcmin = mem_size >> 12;						/* Make sure we have enough for all of physical memory */
#if DEBUG
		kprintf("mapping_adjust: minimum entries rqrd = %08X\n", mapCtl.mapcmin);
		kprintf("mapping_adjust: free = %08X; in use = %08X; release = %08X\n",
		  mapCtl.mapcfree, mapCtl.mapcinuse, mapCtl.mapcreln);
#endif
	}

	s = splhigh();												/* Don't bother from now on */
	if(!hw_lock_to((hw_lock_t)&mapCtl.mapclock, LockTimeOut)) {	/* Lock the control header */ 
		panic("mapping_adjust - timeout getting control lock (1)\n");	/* Tell all and die */
	}
	
	if (mapping_adjust_call == NULL) {
		thread_call_setup(&mapping_adjust_call_data, mapping_adjust, NULL);
		mapping_adjust_call = &mapping_adjust_call_data;
	}

	while(1) {													/* Keep going until we've got enough */
		
		allocsize = mapCtl.mapcmin - mapCtl.mapcfree;			/* Figure out how much we need */
		if(allocsize < 1) break;								/* Leave if we have all we need */
		
		if((unsigned int)(mbn = mapCtl.mapcrel)) {				/* Can we rescue a free one? */
			mapCtl.mapcrel = mbn->nextblok;						/* Dequeue it */
			mapCtl.mapcreln--;									/* Back off the count */
			allocsize = MAPPERBLOK;								/* Show we allocated one block */			
		}
		else {													/* No free ones, try to get it */
			
			allocsize = (allocsize + MAPPERBLOK - 1) / MAPPERBLOK;	/* Get the number of pages we need */
			if(allocsize > (mapCtl.mapcfree / 2)) allocsize = (mapCtl.mapcfree / 2);	/* Don't try for anything that we can't comfortably map */
			
			hw_lock_unlock((hw_lock_t)&mapCtl.mapclock);		/* Unlock our stuff */
			splx(s);											/* Restore 'rupts */

			for(; allocsize > 0; allocsize >>= 1) {				/* Try allocating in descending halves */ 
				retr = kmem_alloc_wired(kernel_map, (vm_offset_t *)&mbn, PAGE_SIZE * allocsize);	/* Find a virtual address to use */
				if((retr != KERN_SUCCESS) && (allocsize == 1)) {	/* Did we find any memory at all? */
					panic("Whoops...  Not a bit of wired memory left for anyone\n");
				}
				if(retr == KERN_SUCCESS) break;					/* We got some memory, bail out... */
			}
		
			allocsize = allocsize * MAPPERBLOK;					/* Convert pages to number of maps allocated */
			s = splhigh();										/* Don't bother from now on */
			if(!hw_lock_to((hw_lock_t)&mapCtl.mapclock, LockTimeOut)) {	/* Lock the control header */ 
				panic("mapping_adjust - timeout getting control lock (2)\n");	/* Tell all and die */
			}
		}
		for(; allocsize > 0; allocsize -= MAPPERBLOK) {			/* Release one block at a time */
			mapping_free_init((vm_offset_t)mbn, 0, 1);			/* Initialize a non-permanent block */
			mbn = (mappingblok *)((unsigned int)mbn + PAGE_SIZE);	/* Point to the next slot */
		}
	}

	if(mapCtl.mapcholdoff) {									/* Should we hold off this release? */
		mapCtl.mapcrecurse = 0;									/* We are done now */
		hw_lock_unlock((hw_lock_t)&mapCtl.mapclock);			/* Unlock our stuff */
		splx(s);												/* Restore 'rupts */
		return;													/* Return... */
	}

	mbn = mapCtl.mapcrel;										/* Get first pending release block */
	mapCtl.mapcrel = 0;											/* Dequeue them */
	mapCtl.mapcreln = 0;										/* Set count to 0 */

	hw_lock_unlock((hw_lock_t)&mapCtl.mapclock);				/* Unlock our stuff */
	splx(s);													/* Restore 'rupts */

	while((unsigned int)mbn) {									/* Toss 'em all */
		mb = mbn->nextblok;										/* Get the next */
		kmem_free(kernel_map, (vm_offset_t) mbn, PAGE_SIZE);	/* Release this mapping block */
		mbn = mb;												/* Chain to the next */
	}

	__asm__ volatile("sync");									/* Make sure all is well */
	mapCtl.mapcrecurse = 0;										/* We are done now */
	return;
}

/*
 *		mapping_free(mapping *mp) - release a mapping to the free list 
 *
 *		This routine takes a mapping and adds it to the free list.
 *		If this mapping make the block non-empty, we queue it to the free block list.
 *		NOTE: we might want to queue it to the end to keep quelch the pathalogical
 *		case when we get a mapping and free it repeatedly causing the block to chain and unchain.
 *		If this release fills a block and we are above the threshold, we release the block
 */

void mapping_free(struct mapping *mp) {							/* Release a mapping */

	mappingblok	*mb, *mbn;
	spl_t			s;
	unsigned int	full, mindx;

	mindx = ((unsigned int)mp & (PAGE_SIZE - 1)) >> 5;			/* Get index to mapping */
	mb = (mappingblok *)((unsigned int)mp & -PAGE_SIZE);		/* Point to the mapping block */

	s = splhigh();												/* Don't bother from now on */
	if(!hw_lock_to((hw_lock_t)&mapCtl.mapclock, LockTimeOut)) {	/* Lock the control header */ 
		panic("mapping_free - timeout getting control lock\n");	/* Tell all and die */
	}
	
	full = !(mb->mapblokfree[0] | mb->mapblokfree[1] | mb->mapblokfree[2] | mb->mapblokfree[3]);	/* See if full now */ 
	mb->mapblokfree[mindx >> 5] |= (0x80000000 >> (mindx & 31));	/* Flip on the free bit */
	
	if(full) {													/* If it was full before this: */
		mb->nextblok = mapCtl.mapcnext;							/* Move head of list to us */
		mapCtl.mapcnext = mb;									/* Chain us to the head of the list */
	}

	mapCtl.mapcfree++;											/* Bump free count */
	mapCtl.mapcinuse--;											/* Decriment in use count */
	
	mapCtl.mapcfreec++;											/* Count total calls */

	if(mapCtl.mapcfree > mapCtl.mapcmin) {						/* Should we consider releasing this? */
		if(((mb->mapblokfree[0] | 0x80000000) & mb->mapblokfree[1] & mb->mapblokfree[2] & mb->mapblokfree[3]) 
		   == 0xFFFFFFFF) {										/* See if empty now */ 

			if(mapCtl.mapcnext == mb) {							/* Are we first on the list? */
				mapCtl.mapcnext = mb->nextblok;					/* Unchain us */
				if(!((unsigned int)mapCtl.mapcnext)) mapCtl.mapclast = 0;	/* If last, remove last */
			}
			else {												/* We're not first */
				for(mbn = mapCtl.mapcnext; mbn != 0; mbn = mbn->nextblok) {	/* Search for our block */
					if(mbn->nextblok == mb) break;				/* Is the next one our's? */
				}
				if(!mbn) panic("mapping_free: attempt to release mapping block (%08X) not on list\n", mp);
				mbn->nextblok = mb->nextblok;					/* Dequeue us */
				if(mapCtl.mapclast == mb) mapCtl.mapclast = mbn;	/* If last, make our predecessor last */
			}
			
			if(mb->mapblokflags & mbPerm) {						/* Is this permanently assigned? */
				mb->nextblok = mapCtl.mapcnext;					/* Move chain head to us */
				mapCtl.mapcnext = mb;							/* Chain us to the head */
				if(!((unsigned int)mb->nextblok)) mapCtl.mapclast = mb;	/* If last, make us so */
			}
			else {
				mapCtl.mapcfree -= MAPPERBLOK;					/* Remove the block from the free count */
				mapCtl.mapcreln++;								/* Count on release list */
				mb->nextblok = mapCtl.mapcrel;					/* Move pointer */
				mapCtl.mapcrel = mb;							/* Chain us in front */
			}
		}
	}

	if(mapCtl.mapcreln > MAPFRTHRSH) {							/* Do we have way too many releasable mappings? */
		if(hw_compare_and_store(0, 1, &mapCtl.mapcrecurse)) {	/* Make sure we aren't recursing */
			thread_call_enter(mapping_adjust_call);			/* Go toss some */
		}
	}
	hw_lock_unlock((hw_lock_t)&mapCtl.mapclock);				/* Unlock our stuff */
	splx(s);													/* Restore 'rupts */

	return;														/* Bye, dude... */
}


/*
 *		mapping_alloc(void) - obtain a mapping from the free list 
 *
 *		This routine takes a mapping off of the free list and returns it's address.
 *
 *		We do this by finding a free entry in the first block and allocating it.
 *		If this allocation empties the block, we remove it from the free list.
 *		If this allocation drops the total number of free entries below a threshold,
 *		we allocate a new block.
 *
 */

mapping *mapping_alloc(void) {									/* Obtain a mapping */

	register mapping *mp;
	mappingblok	*mb, *mbn;
	spl_t			s;
	int				mindx;
	kern_return_t	retr;

	s = splhigh();												/* Don't bother from now on */
	if(!hw_lock_to((hw_lock_t)&mapCtl.mapclock, LockTimeOut)) {	/* Lock the control header */ 
		panic("mapping_alloc - timeout getting control lock\n");	/* Tell all and die */
	}

	if(!(mb = mapCtl.mapcnext)) {								/* Get the first block entry */
		panic("mapping_alloc - free mappings exhausted\n");		/* Whine and moan */
	}
	

	if(!(mindx = mapalc(mb))) {									/* Allocate a slot */
		panic("mapping_alloc - empty mapping block detected at %08X\n", mb);	/* Not allowed to find none */
	}
	
	if(mindx < 0) {												/* Did we just take the last one */
		mindx = -mindx;											/* Make positive */
		mapCtl.mapcnext = mb->nextblok;							/* Remove us from the list */
		if(!((unsigned int)mapCtl.mapcnext)) mapCtl.mapclast = 0;	/* Removed the last one */
	}
	
	mapCtl.mapcfree--;											/* Decrement free count */
	mapCtl.mapcinuse++;											/* Bump in use count */
	
	mapCtl.mapcallocc++;										/* Count total calls */

/*
 *	Note: in the following code, we will attempt to rescue blocks only one at a time.
 *	Eventually, after a few more mapping_alloc calls, we will catch up.  If there are none
 *	rescueable, we will kick the misc scan who will allocate some for us.  We only do this
 *	if we haven't already done it.
 *	For early boot, we are set up to only rescue one block at a time.  This is because we prime
 *	the release list with as much as we need until threads start.
 */
	if(mapCtl.mapcfree < mapCtl.mapcmin) {						/* See if we need to replenish */
		if(mbn = mapCtl.mapcrel) {								/* Try to rescue a block from impending doom */
			mapCtl.mapcrel = mbn->nextblok;						/* Pop the queue */
			mapCtl.mapcreln--;									/* Back off the count */
			mapping_free_init((vm_offset_t)mbn, 0, 1);			/* Initialize a non-permanent block */
		}
		else {													/* We need to replenish */
			if(hw_compare_and_store(0, 1, &mapCtl.mapcrecurse)) {	/* Make sure we aren't recursing */
				thread_call_enter(mapping_adjust_call);		/* Go allocate some more */
			}
		}
	}

	hw_lock_unlock((hw_lock_t)&mapCtl.mapclock);				/* Unlock our stuff */
	splx(s);													/* Restore 'rupts */
	
	mp = &((mapping *)mb)[mindx];								/* Point to the allocated mapping */
    __asm__ volatile("dcbz 0,%0" : : "r" (mp));					/* Clean it up */
	return mp;													/* Send it back... */
}


/*
 *		void mapping_free_init(mb, perm) - Adds a block of storage to the free mapping list
 *
 *		The mapping block is a page size area on a page boundary.  It contains 1 header and 127
 *		mappings.  This call adds and initializes a block for use.
 *	
 *		The header contains a chain link, bit maps, a virtual to real translation mask, and
 *		some statistics. Bit maps map each slot on the page (bit 0 is not used because it 
 *		corresponds to the header).  The translation mask is the XOR of the virtual and real
 *		addresses (needless to say, the block must be wired).
 *
 *		We handle these mappings the same way as saveareas: the block is only on the chain so
 *		long as there are free entries in it.
 *
 *		Empty blocks are garbage collected when there are at least mapCtl.mapcmin pages worth of free 
 *		mappings. Blocks marked PERM won't ever be released.
 *
 *		If perm is negative, the mapping is initialized, but immediately queued to the mapCtl.mapcrel
 *		list.  We do this only at start up time. This is done because we only allocate blocks 
 *		in the pageout scan and it doesn't start up until after we run out of the initial mappings.
 *		Therefore, we need to preallocate a bunch, but we don't want them to be permanent.  If we put
 *		them on the release queue, the allocate routine will rescue them.  Then when the
 *		pageout scan starts, all extra ones will be released.
 *
 */


void mapping_free_init(vm_offset_t mbl, int perm, boolean_t locked) {		
															/* Set's start and end of a block of mappings
															   perm indicates if the block can be released 
															   or goes straight to the release queue .
															   locked indicates if the lock is held already */
														   
	mappingblok	*mb;
	spl_t		s;
	int			i;
	unsigned int	raddr;

	mb = (mappingblok *)mbl;								/* Start of area */
	
	
	if(perm >= 0) {											/* See if we need to initialize the block */
		if(perm) {
			raddr = (unsigned int)mbl;						/* Perm means V=R */
			mb->mapblokflags = mbPerm;						/* Set perm */
		}
		else {
			raddr = kvtophys(mbl);							/* Get real address */
			mb->mapblokflags = 0;							/* Set not perm */
		}
		
		mb->mapblokvrswap = raddr ^ (unsigned int)mbl;		/* Form translation mask */
		
		mb->mapblokfree[0] = 0x7FFFFFFF;					/* Set first 32 (minus 1) free */
		mb->mapblokfree[1] = 0xFFFFFFFF;					/* Set next 32 free */
		mb->mapblokfree[2] = 0xFFFFFFFF;					/* Set next 32 free */
		mb->mapblokfree[3] = 0xFFFFFFFF;					/* Set next 32 free */
	}
	
	s = splhigh();											/* Don't bother from now on */
	if(!locked) {											/* Do we need the lock? */
		if(!hw_lock_to((hw_lock_t)&mapCtl.mapclock, LockTimeOut)) {		/* Lock the control header */ 
			panic("mapping_free_init - timeout getting control lock\n");	/* Tell all and die */
		}
	}
	
	if(perm < 0) {											/* Direct to release queue? */
		mb->nextblok = mapCtl.mapcrel;						/* Move forward pointer */
		mapCtl.mapcrel = mb;								/* Queue us on in */
		mapCtl.mapcreln++;									/* Count the free block */
	}
	else {													/* Add to the free list */
		
		mb->nextblok = 0;									/* We always add to the end */
		mapCtl.mapcfree += MAPPERBLOK;						/* Bump count */
		
		if(!((unsigned int)mapCtl.mapcnext)) {				/* First entry on list? */
			mapCtl.mapcnext = mapCtl.mapclast = mb;			/* Chain to us */
		}
		else {												/* We are not the first */
			mapCtl.mapclast->nextblok = mb;					/* Point the last to us */
			mapCtl.mapclast = mb;							/* We are now last */
		}
	}
		
	if(!locked) {											/* Do we need to unlock? */
		hw_lock_unlock((hw_lock_t)&mapCtl.mapclock);		/* Unlock our stuff */
	}
		splx(s);											/* Restore 'rupts */
	return;													/* All done, leave... */
}


/*
 *		void mapping_prealloc(unsigned int) - Preallocates mapppings for large request
 *	
 *		No locks can be held, because we allocate memory here.
 *		This routine needs a corresponding mapping_relpre call to remove the
 *		hold off flag so that the adjust routine will free the extra mapping
 *		blocks on the release list.  I don't like this, but I don't know
 *		how else to do this for now...
 *
 */

void mapping_prealloc(unsigned int size) {					/* Preallocates mapppings for large request */

	int	nmapb, i;
	kern_return_t	retr;
	mappingblok	*mbn;
	spl_t		s;

	s = splhigh();											/* Don't bother from now on */
	if(!hw_lock_to((hw_lock_t)&mapCtl.mapclock, LockTimeOut)) {		/* Lock the control header */ 
		panic("mapping_prealloc - timeout getting control lock\n");	/* Tell all and die */
	}

	nmapb = (size >> 12) + mapCtl.mapcmin;					/* Get number of entries needed for this and the minimum */
	
	mapCtl.mapcholdoff++;									/* Bump the hold off count */
	
	if((nmapb = (nmapb - mapCtl.mapcfree)) <= 0) {			/* Do we already have enough? */
		hw_lock_unlock((hw_lock_t)&mapCtl.mapclock);		/* Unlock our stuff */
		splx(s);											/* Restore 'rupts */
		return;
	}

	nmapb = (nmapb + MAPPERBLOK - 1) / MAPPERBLOK;			/* Get number of blocks to get */
	
	hw_lock_unlock((hw_lock_t)&mapCtl.mapclock);			/* Unlock our stuff */
	splx(s);												/* Restore 'rupts */
	
	for(i = 0; i < nmapb; i++) {							/* Allocate 'em all */
		retr = kmem_alloc_wired(kernel_map, (vm_offset_t *)&mbn, PAGE_SIZE);	/* Find a virtual address to use */
		if(retr != KERN_SUCCESS) {							/* Did we get some memory? */
			panic("Whoops...  Not a bit of wired memory left for anyone\n");
		}
		mapping_free_init((vm_offset_t)mbn, -1, 0);			/* Initialize on to the release queue */
	}
}

/*
 *		void mapping_relpre(void) - Releases preallocation release hold off
 *	
 *		This routine removes the
 *		hold off flag so that the adjust routine will free the extra mapping
 *		blocks on the release list.  I don't like this, but I don't know
 *		how else to do this for now...
 *
 */

void mapping_relpre(void) {									/* Releases release hold off */

	spl_t		s;

	s = splhigh();											/* Don't bother from now on */
	if(!hw_lock_to((hw_lock_t)&mapCtl.mapclock, LockTimeOut)) {		/* Lock the control header */ 
		panic("mapping_relpre - timeout getting control lock\n");	/* Tell all and die */
	}
	if(--mapCtl.mapcholdoff < 0) {							/* Back down the hold off count */
		panic("mapping_relpre: hold-off count went negative\n");
	}

	hw_lock_unlock((hw_lock_t)&mapCtl.mapclock);			/* Unlock our stuff */
	splx(s);												/* Restore 'rupts */
}

/*
 *		void mapping_free_prime(void) - Primes the mapping block release list
 *
 *		See mapping_free_init.
 *		No locks can be held, because we allocate memory here.
 *		One processor running only.
 *
 */

void mapping_free_prime(void) {									/* Primes the mapping block release list */

	int	nmapb, i;
	kern_return_t	retr;
	mappingblok	*mbn;
	
	nmapb = (mapCtl.mapcfree + mapCtl.mapcinuse + MAPPERBLOK - 1) / MAPPERBLOK;	/* Get permanent allocation */
	nmapb = nmapb * 4;											/* Get 4 times our initial allocation */

#if DEBUG
	kprintf("mapping_free_prime: free = %08X; in use = %08X; priming = %08X\n", 
	  mapCtl.mapcfree, mapCtl.mapcinuse, nmapb);
#endif
	
	for(i = 0; i < nmapb; i++) {								/* Allocate 'em all */
		retr = kmem_alloc_wired(kernel_map, (vm_offset_t *)&mbn, PAGE_SIZE);	/* Find a virtual address to use */
		if(retr != KERN_SUCCESS) {								/* Did we get some memory? */
			panic("Whoops...  Not a bit of wired memory left for anyone\n");
		}
		mapping_free_init((vm_offset_t)mbn, -1, 0);				/* Initialize onto release queue */
	}
}

/*
 *		vm_offset_t	mapping_p2v(space_t space, phys_entry *pp) - Finds first virtual mapping of a physical page in a space
 *
 *		Gets a lock on the physical entry.  Then it searches the list of attached mappings for one with
 *		the same space.  If it finds it, it returns the virtual address.
 */

vm_offset_t	mapping_p2v(space_t space, struct phys_entry *pp) {		/* Finds first virtual mapping of a physical page in a space */

	spl_t				s;
	register mapping	*mp, *mpv;
	vm_offset_t			va;

	if(!hw_lock_bit((unsigned int *)&pp->phys_link, PHYS_LOCK, LockTimeOut)) {	/* Try to get the lock on the physical entry */
		splx(s);											/* Restore 'rupts */
		panic("mapping_p2v: timeout getting lock on physent\n");			/* Arrrgghhhh! */
		return(0);											/* Should die before here */
	}
	
	va = 0;													/* Assume failure */
	
	for(mpv = hw_cpv(pp->phys_link); mpv; mp = hw_cpv(mp->next)) {	/* Scan 'em all */
		
		if(!(((mpv->PTEv >> 7) & 0x000FFFFF) == space)) continue;	/* Skip all the rest if this is not the right space... */ 
		
		va = ((((unsigned int)mpv->PTEhash & -64) << 5) ^ (space  << 12)) & 0x003FF000;	/* Backward hash to the wrapped VADDR */
		va = va | ((mpv->PTEv << 8) & 0xF0000000);			/* Move in the segment number */
		va = va | ((mpv->PTEv << 22) & 0x0FC00000);			/* Add in the API for the top of the address */
		break;												/* We're done now, pass virtual address back */
	}
	
	hw_unlock_bit((unsigned int *)&pp->phys_link, PHYS_LOCK);				/* Unlock the physical entry */
	splx(s);												/* Restore 'rupts */
	return(va);												/* Return the result or 0... */
}

/*
 *	kvtophys(addr)
 *
 *	Convert a kernel virtual address to a physical address
 */
vm_offset_t kvtophys(vm_offset_t va) {

	register mapping		*mp, *mpv;
	register vm_offset_t 	pa;
	spl_t				s;
	
	s=splhigh();											/* Don't bother from now on */
	mp = hw_lock_phys_vir(PPC_SID_KERNEL, va);				/* Find mapping and lock the physical entry for this mapping */

	if((unsigned int)mp&1) {								/* Did the lock on the phys entry time out? */
		splx(s);											/* Restore 'rupts */
		panic("kvtophys: timeout obtaining lock on physical entry (vaddr=%08X)\n", va);	/* Scream bloody murder! */
		return 0;
	}

	if(!mp) {												/* If it was not found, or no physical entry */
		splx(s);											/* Restore 'rupts */
		return 0;											/* Return 0 */
	}

	mpv = hw_cpv(mp);										/* Convert to virtual addressing */
	
	if(!mpv->physent) {										/* Was there a physical entry? */
		pa = (vm_offset_t)((mpv->PTEr & -PAGE_SIZE) | ((unsigned int)va & (PAGE_SIZE-1)));	/* Get physical address from physent */
	}
	else {
		pa = (vm_offset_t)((mpv->physent->pte1 & -PAGE_SIZE) | ((unsigned int)va & (PAGE_SIZE-1)));	/* Get physical address from physent */
		hw_unlock_bit((unsigned int *)&mpv->physent->phys_link, PHYS_LOCK);	/* Unlock the physical entry */
	}
	
	splx(s);												/* Restore 'rupts */
	return pa;												/* Return the physical address... */
}

/*
 *	phystokv(addr)
 *
 *	Convert a physical address to a kernel virtual address if
 *	there is a mapping, otherwise return NULL
 */

vm_offset_t phystokv(vm_offset_t pa) {

	struct phys_entry	*pp;
	vm_offset_t			va;

	pp = pmap_find_physentry(pa);							/* Find the physical entry */
	if (PHYS_NULL == pp) {
		return (vm_offset_t)NULL;							/* If none, return null */
	}
	if(!(va=mapping_p2v(PPC_SID_KERNEL, pp))) {
		return 0;											/* Can't find it, return 0... */
	}
	return (va | (pa & (PAGE_SIZE-1)));						/* Build and return VADDR... */

}

/*
 *		Enters translations into the autogen maps.
 *
 *		Not implemented yet, just stubbed out...
 */
 
boolean_t	autogen_map(space_t space, vm_offset_t va, vm_offset_t spa, vm_offset_t epa, vm_prot_t prot, int attr) { /* Build an autogen area */

		return(0);										/* Just return false for now */
}


/*
 *		Dumps out the mapping stuff associated with a virtual address
 */
void dumpaddr(space_t space, vm_offset_t va) {

	mapping		*mp, *mpv;
	vm_offset_t	pa;
	spl_t 		s;

	s=splhigh();											/* Don't bother me */

	mp = hw_lock_phys_vir(space, va);						/* Lock the physical entry for this mapping */
	if(!mp) {												/* Did we find one? */
		splx(s);											/* Restore the interrupt level */
		printf("dumpaddr: virtual address (%08X) not mapped\n", va);	
		return;												/* Didn't find any, return FALSE... */
	}
	if((unsigned int)mp&1) {								/* Did we timeout? */
		panic("dumpaddr: timeout locking physical entry for virtual address (%08X)\n", va);	/* Yeah, scream about it! */
		splx(s);											/* Restore the interrupt level */
		return;												/* Bad hair day, return FALSE... */
	}
	printf("dumpaddr: space=%08X; vaddr=%08X\n", space, va);	/* Say what address were dumping */
	mpv = hw_cpv(mp);										/* Get virtual address of mapping */
	dumpmapping(mpv);
	if(mpv->physent) {
		dumppca(mpv);
		hw_unlock_bit((unsigned int *)&mpv->physent->phys_link, PHYS_LOCK);	/* Unlock physical entry associated with mapping */
	}
	splx(s);												/* Was there something you needed? */
	return;													/* Tell them we did it */
}



/*
 *		Prints out a mapping control block
 *
 */
 
void dumpmapping(struct mapping *mp) { 						/* Dump out a mapping */

	printf("Dump of mapping block: %08X\n", mp);			/* Header */
	printf("                 next: %08X\n", mp->next);                 
	printf("             hashnext: %08X\n", mp->hashnext);                 
	printf("              PTEhash: %08X\n", mp->PTEhash);                 
	printf("               PTEent: %08X\n", mp->PTEent);                 
	printf("              physent: %08X\n", mp->physent);                 
	printf("                 PTEv: %08X\n", mp->PTEv);                 
	printf("                 PTEr: %08X\n", mp->PTEr);                 
	printf("                 pmap: %08X\n", mp->pmap);
	
	if(mp->physent) {									/* Print physent if it exists */
		printf("Associated physical entry: %08X %08X\n", mp->physent->phys_link, mp->physent->pte1);
	}
	else {
		printf("Associated physical entry: none\n");
	}
	
	dumppca(mp);										/* Dump out the PCA information */
	
	return;
}

/*
 *		Prints out a PTEG control area
 *
 */
 
void dumppca(struct mapping *mp) { 						/* PCA */

	PCA				*pca;
	unsigned int	*pteg;
	
	pca = (PCA *)((unsigned int)mp->PTEhash&-64);		/* Back up to the start of the PCA */
	pteg=(unsigned int *)((unsigned int)pca-(((hash_table_base&0x0000FFFF)+1)<<16));
	printf(" Dump of PCA: %08X\n", pca);		/* Header */
	printf("     PCAlock: %08X\n", pca->PCAlock);                 
	printf("     PCAallo: %08X\n", pca->flgs.PCAallo);                 
	printf("     PCAhash: %08X %08X %08X %08X\n", pca->PCAhash[0], pca->PCAhash[1], pca->PCAhash[2], pca->PCAhash[3]);                 
	printf("              %08X %08X %08X %08X\n", pca->PCAhash[4], pca->PCAhash[5], pca->PCAhash[6], pca->PCAhash[7]);                 
	printf("Dump of PTEG: %08X\n", pteg);		/* Header */
	printf("              %08X %08X %08X %08X\n", pteg[0], pteg[1], pteg[2], pteg[3]);                 
	printf("              %08X %08X %08X %08X\n", pteg[4], pteg[5], pteg[6], pteg[7]);                 
	printf("              %08X %08X %08X %08X\n", pteg[8], pteg[9], pteg[10], pteg[11]);                 
	printf("              %08X %08X %08X %08X\n", pteg[12], pteg[13], pteg[14], pteg[15]);                 
	return;
}

/*
 *		Dumps starting with a physical entry
 */
 
void dumpphys(struct phys_entry *pp) { 						/* Dump from physent */

	mapping			*mp;
	PCA				*pca;
	unsigned int	*pteg;

	printf("Dump from physical entry %08X: %08X %08X\n", pp, pp->phys_link, pp->pte1);
	mp = hw_cpv(pp->phys_link);
	while(mp) {
		dumpmapping(mp);
		dumppca(mp);
		mp = hw_cpv(mp->next);
	}
	
	return;
}

/*
 *		void ignore_zero_fault(boolean_t) - Sets up to ignore or honor any fault on 
 *		page 0 access for the current thread.
 *
 *		If parameter is TRUE, faults are ignored
 *		If parameter is FALSE, faults are honored
 *
 */

void ignore_zero_fault(boolean_t type) {		/* Sets up to ignore or honor any fault on page 0 access for the current thread */

	if(type) current_act()->mact.specFlags |= ignoreZeroFault;	/* Ignore faults on page 0 */
	else     current_act()->mact.specFlags &= ~ignoreZeroFault;	/* Honor faults on page 0 */
	
	return;												/* Return the result or 0... */
}








