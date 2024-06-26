/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.1 (the "License").  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON- INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * Copyright (c) 1993 NeXT Computer, Inc.
 *
 * Architecture computing functions.
 *
 * HISTORY
 *
 * 11 April 1997
 *	Update m98k to ppc and removed the never supported architectures (mips,
 *	and vax).  Apple Computer, Inc.
 *
 * 4 February 1993 Lennart Lovstrand <lennart@next.com>
 *	Redesigned to use NXArchInfo based names and signatures.
 *
 * Originally written at NeXT, Inc.
 *
 */

#import <stdio.h>
#import <stdlib.h>
#import <string.h>
#import <limits.h>

#import <mach/mach.h>
#import "stuff/openstep_mach.h"
#import <mach-o/fat.h>
#import <mach-o/arch.h>

/* The array of all currently know architecture flags (terminated with an entry
 * with all zeros).  Pointer to this returned with NXGetAllArchInfos().
 */
static const NXArchInfo ArchInfoTable[] = {
    /* architecture families */
    {"hppa",   CPU_TYPE_HPPA,	 CPU_SUBTYPE_HPPA_ALL,	   NX_BigEndian,
	 "HP-PA"},
    {"i386",   CPU_TYPE_I386,    CPU_SUBTYPE_I386_ALL,	   NX_LittleEndian,
	 "Intel 80x86"},
    {"i860",   CPU_TYPE_I860,    CPU_SUBTYPE_I860_ALL,     NX_BigEndian,
	 "Intel 860"},
    {"m68k",   CPU_TYPE_MC680x0, CPU_SUBTYPE_MC680x0_ALL,  NX_BigEndian,
	 "Motorola 68K"},
    {"m88k",   CPU_TYPE_MC88000, CPU_SUBTYPE_MC88000_ALL,  NX_BigEndian,
	 "Motorola 88K"},
    {"ppc",    CPU_TYPE_POWERPC, CPU_SUBTYPE_POWERPC_ALL,  NX_BigEndian,
	 "PowerPC"},
    {"sparc",  CPU_TYPE_SPARC,   CPU_SUBTYPE_SPARC_ALL,	   NX_BigEndian,
	 "SPARC"},
    {"any",    CPU_TYPE_ANY,     CPU_SUBTYPE_MULTIPLE,     NX_UnknownByteOrder,
	 "Architecture Independent"},
    /* specific architecture implementations */
    {"hppa7100LC", CPU_TYPE_HPPA, CPU_SUBTYPE_HPPA_7100LC, NX_BigEndian,
	 "HP-PA 7100LC"},
    {"m68030", CPU_TYPE_MC680x0, CPU_SUBTYPE_MC68030_ONLY, NX_BigEndian,
	 "Motorola 68030"},
    {"m68040", CPU_TYPE_MC680x0, CPU_SUBTYPE_MC68040,	   NX_BigEndian,
	 "Motorola 68040"},
    {"i486",   CPU_TYPE_I386,    CPU_SUBTYPE_486,	   NX_LittleEndian,
	 "Intel 80486"},
    {"i486SX", CPU_TYPE_I386,    CPU_SUBTYPE_486SX,	   NX_LittleEndian,
	 "Intel 80486SX"},
    {"pentium",CPU_TYPE_I386,    CPU_SUBTYPE_PENT,	   NX_LittleEndian,
	 "Intel Pentium"}, /* same as 586 */
    {"i586",   CPU_TYPE_I386,    CPU_SUBTYPE_586,	   NX_LittleEndian,
	 "Intel 80586"},
    {"pentpro", CPU_TYPE_I386, CPU_SUBTYPE_PENTPRO,	   NX_LittleEndian,
	 "Intel Pentium Pro"},
    {"pentIIm3", CPU_TYPE_I386, CPU_SUBTYPE_PENTII_M3, NX_LittleEndian,
	 "Intel Pentium II Model 3" },
    {"pentIIm5", CPU_TYPE_I386, CPU_SUBTYPE_PENTII_M5, NX_LittleEndian,
	 "Intel Pentium II Model 5" },
    {"ppc601", CPU_TYPE_POWERPC, CPU_SUBTYPE_POWERPC_601,  NX_BigEndian,
	 "PowerPC 601" },
    {"ppc603", CPU_TYPE_POWERPC, CPU_SUBTYPE_POWERPC_603,  NX_BigEndian,
	 "PowerPC 603" },
    {"ppc603e",CPU_TYPE_POWERPC, CPU_SUBTYPE_POWERPC_603e, NX_BigEndian,
	 "PowerPC 603e" },
    {"ppc603ev",CPU_TYPE_POWERPC,CPU_SUBTYPE_POWERPC_603ev,NX_BigEndian,
	 "PowerPC 603ev" },
    {"ppc604", CPU_TYPE_POWERPC, CPU_SUBTYPE_POWERPC_604,  NX_BigEndian,
	 "PowerPC 604" },
    {"ppc604e",CPU_TYPE_POWERPC, CPU_SUBTYPE_POWERPC_604e, NX_BigEndian,
	 "PowerPC 604e" },
    {"ppc750", CPU_TYPE_POWERPC, CPU_SUBTYPE_POWERPC_750,  NX_BigEndian,
	 "PowerPC 750" },
    {"ppc7400",CPU_TYPE_POWERPC, CPU_SUBTYPE_POWERPC_7400,  NX_BigEndian,
	 "PowerPC 7400" },
    {"little", CPU_TYPE_ANY,     CPU_SUBTYPE_LITTLE_ENDIAN, NX_LittleEndian,
         "Little Endian"},
    {"big",    CPU_TYPE_ANY,     CPU_SUBTYPE_BIG_ENDIAN,   NX_BigEndian,
         "Big Endian"},
    {NULL,     0,		  0,			   0,
	 NULL}
};

/*
 * NXGetAllArchInfos() returns a pointer to an array of all currently know
 * architecture flags (terminated with an entry with all zeros).
 */
const
NXArchInfo *
NXGetAllArchInfos(void)
{
	return(ArchInfoTable);
}

/*
 * NXGetLocalArchInfo() returns the NXArchInfo matching the cputype and
 * cpusubtype of the local host.  NULL is returned if there is no matching
 * entry in the ArchInfoTable.
 */
const
NXArchInfo *
NXGetLocalArchInfo(void)
{
    struct host_basic_info hbi;
    kern_return_t ret;
    unsigned int count;

	count = HOST_BASIC_INFO_COUNT;
	ret = host_info(mach_host_self(), HOST_BASIC_INFO, (host_info_t)&hbi,
			&count);
	if(ret != KERN_SUCCESS)
	    return(NULL);

	/*
	 * There is a "bug" in the kernel for compatiblity that on
	 * an 030 machine host_info() returns cpusubtype
	 * CPU_SUBTYPE_MC680x0_ALL and not CPU_SUBTYPE_MC68030_ONLY.
	 */
	if(hbi.cpu_type == CPU_TYPE_MC680x0 &&
	   hbi.cpu_subtype == CPU_SUBTYPE_MC680x0_ALL)
	    hbi.cpu_subtype = CPU_SUBTYPE_MC68030_ONLY;

	return(NXGetArchInfoFromCpuType(hbi.cpu_type, hbi.cpu_subtype));
}

/*
 * NXGetArchInfoFromName() is passed an architecture name (like "m68k")
 * and returns the matching NXArchInfo struct, or NULL if none is found.
 */
const
NXArchInfo *
NXGetArchInfoFromName(
const char *name)
{
    const NXArchInfo *ai;

	for(ai = ArchInfoTable; ai->name != NULL; ai++)
	    if(strcmp(ai->name, name) == 0)
		return(ai);

	return(NULL);
}

/*
 * NXGetArchInfoFromName() is passed a cputype and cpusubtype and returns
 * the matching NXArchInfo struct, or NULL if none is found.  If the
 * cpusubtype is given as CPU_SUBTYPE_MULTIPLE, the first entry that
 * matches the given cputype is returned.  This is the NXArchInfo struct
 * describing the CPU "family".
 */
const
NXArchInfo *
NXGetArchInfoFromCpuType(
cpu_type_t cputype,
cpu_subtype_t cpusubtype)
{
    const NXArchInfo *ai;
    NXArchInfo *q;

    for(ai = ArchInfoTable; ai->name != NULL; ai++)
	if(ai->cputype == cputype &&
	   (cpusubtype == CPU_SUBTYPE_MULTIPLE ||
	    (ai->cpusubtype == cpusubtype)))
	    return(ai);

    if(cputype == CPU_TYPE_I386){
	q = malloc(sizeof(NXArchInfo));
	for(ai = ArchInfoTable; ai->name != NULL; ai++){
	    if(ai->cputype == cputype){
		*q = *ai;
		break;
	    }
	}
	q->cpusubtype = cpusubtype;
	q->description = malloc(sizeof("Intel family  model ") + 2 + 8);
	if(q->description == NULL)
	    return(NULL);
	sprintf((char *)q->description, "Intel family %u model %u", 
		CPU_SUBTYPE_INTEL_FAMILY(cpusubtype),
		CPU_SUBTYPE_INTEL_MODEL(cpusubtype));
	return((const NXArchInfo *)q);
    }
    else if(cputype == CPU_TYPE_POWERPC){
	q = malloc(sizeof(NXArchInfo));
	for(ai = ArchInfoTable; ai->name != NULL; ai++){
	    if(ai->cputype == cputype){
		*q = *ai;
		break;
	    }
	}
	q->cpusubtype = cpusubtype;
	q->description = malloc(sizeof("PowerPC cpusubtype ") + 10);
	if(q->description == NULL)
	    return(NULL);
	sprintf((char *)q->description, "PowerPC cpusubtype %u", cpusubtype);
	return((const NXArchInfo *)q);
    }

    return(NULL);
}

/*
 * NXFindBestFatArch() is passed a cputype and cpusubtype and a set of
 * fat_arch structs and selects the best one that matches (if any) and returns
 * a pointer to that fat_arch struct (or NULL).  The fat_arch structs must be
 * in the host byte order and correct such that the fat_archs really points to
 * enough memory for nfat_arch structs.  It is possible that this routine could
 * fail if new cputypes or cpusubtypes are added and an old version of this
 * routine is used.  But if there is an exact match between the cputype and
 * cpusubtype and one of the fat_arch structs this routine will always succeed.
 */
struct fat_arch *
NXFindBestFatArch(
cpu_type_t cputype,
cpu_subtype_t cpusubtype,
struct fat_arch *fat_archs,
unsigned long nfat_archs)
{
    unsigned long i, lowest_family, lowest_model, lowest_index;

	/*
	 * Look for the first exact match.
	 */
	for(i = 0; i < nfat_archs; i++){
	    if(fat_archs[i].cputype == cputype &&
	       fat_archs[i].cpusubtype == cpusubtype)
		return(fat_archs + i);
	}

	/*
	 * An exact match was not found so find the next best match which is
	 * cputype dependent.
	 */
	switch(cputype){
	case CPU_TYPE_I386:
	    switch(cpusubtype){
	    default:
		/*
		 * Intel cpusubtypes after the pentium (same as 586) are handled
		 * such that they require an exact match or they can use the
		 * pentium.  If that is not found call into the loop for the
		 * earilier subtypes.
		 */
		for(i = 0; i < nfat_archs; i++){
		    if(fat_archs[i].cputype != cputype)
			continue;
		    if(fat_archs[i].cpusubtype == CPU_SUBTYPE_PENT)
			return(fat_archs + i);
		}
	    case CPU_SUBTYPE_PENT:
	    case CPU_SUBTYPE_486SX:
		/*
		 * Since an exact match as not found look for the i486 else
		 * break into the loop to look for the i386_ALL.
		 */
		for(i = 0; i < nfat_archs; i++){
		    if(fat_archs[i].cputype != cputype)
			continue;
		    if(fat_archs[i].cpusubtype == CPU_SUBTYPE_486)
			return(fat_archs + i);
		}
		break;
	    case CPU_SUBTYPE_I386_ALL:
	    /* case CPU_SUBTYPE_I386: same as above */
	    case CPU_SUBTYPE_486:
		break;
	    }
	    for(i = 0; i < nfat_archs; i++){
		if(fat_archs[i].cputype != cputype)
		    continue;
		if(fat_archs[i].cpusubtype == CPU_SUBTYPE_I386_ALL)
		    return(fat_archs + i);
	    }

	    /*
	     * A match failed, promote as little as possible.
	     */
	    for(i = 0; i < nfat_archs; i++){
		if(fat_archs[i].cputype != cputype)
		    continue;
		if(fat_archs[i].cpusubtype == CPU_SUBTYPE_486)
		    return(fat_archs + i);
	    }
	    for(i = 0; i < nfat_archs; i++){
		if(fat_archs[i].cputype != cputype)
		    continue;
		if(fat_archs[i].cpusubtype == CPU_SUBTYPE_486SX)
		    return(fat_archs + i);
	    }
	    for(i = 0; i < nfat_archs; i++){
		if(fat_archs[i].cputype != cputype)
		    continue;
		if(fat_archs[i].cpusubtype == CPU_SUBTYPE_586)
		    return(fat_archs + i);
	    }
	    /*
	     * Now look for the lowest family and in that the lowest model.
	     */
	    lowest_family = CPU_SUBTYPE_INTEL_FAMILY_MAX + 1;
	    for(i = 0; i < nfat_archs; i++){
		if(fat_archs[i].cputype != cputype)
		    continue;
		if(CPU_SUBTYPE_INTEL_FAMILY(fat_archs[i].cpusubtype) <
		   lowest_family)
		    lowest_family = CPU_SUBTYPE_INTEL_FAMILY(
					fat_archs[i].cpusubtype);
	    }
	    /* if no intel cputypes found return NULL */
	    if(lowest_family == CPU_SUBTYPE_INTEL_FAMILY_MAX + 1)
		return(NULL);
	    lowest_model = ULONG_MAX;
	    lowest_index = -1;
	    for(i = 0; i < nfat_archs; i++){
		if(fat_archs[i].cputype != cputype)
		    continue;
		if(CPU_SUBTYPE_INTEL_FAMILY(fat_archs[i].cpusubtype) ==
		   lowest_family){
		    if(CPU_SUBTYPE_INTEL_MODEL(fat_archs[i].cpusubtype) <
		       lowest_model)
			lowest_index = i;
		}
	    }
	    return(fat_archs + lowest_index);
	case CPU_TYPE_MC680x0:
	    for(i = 0; i < nfat_archs; i++){
		if(fat_archs[i].cputype != cputype)
		    continue;
		if(fat_archs[i].cpusubtype == CPU_SUBTYPE_MC680x0_ALL)
		    return(fat_archs + i);
	    }
	    /*
	     * Try to promote if starting from CPU_SUBTYPE_MC680x0_ALL and
	     * favor the CPU_SUBTYPE_MC68040 over the CPU_SUBTYPE_MC68030_ONLY.
	     */
	    if(cpusubtype == CPU_SUBTYPE_MC680x0_ALL){
		for(i = 0; i < nfat_archs; i++){
		    if(fat_archs[i].cputype != cputype)
			continue;
		    if(fat_archs[i].cpusubtype == CPU_SUBTYPE_MC68040)
			return(fat_archs + i);
		}
		for(i = 0; i < nfat_archs; i++){
		    if(fat_archs[i].cputype != cputype)
			continue;
		    if(fat_archs[i].cpusubtype == CPU_SUBTYPE_MC68030_ONLY)
			return(fat_archs + i);
		}
	    }
	    break;
	case CPU_TYPE_POWERPC:
	    /*
	     * An exact match as not found.  So for all the PowerPC subtypes
	     * pick the subtype from the following order:
	     *	7400, 750, 604e, 604, 603ev, 603e, 603, ALL
	     * Note the 601 is NOT in the list above.  It is only picked via
	     * an exact match.
	     */
	    for(i = 0; i < nfat_archs; i++){
		if(fat_archs[i].cputype != cputype)
		    continue;
		if(fat_archs[i].cpusubtype == CPU_SUBTYPE_POWERPC_7400)
		    return(fat_archs + i);
	    }
	    for(i = 0; i < nfat_archs; i++){
		if(fat_archs[i].cputype != cputype)
		    continue;
		if(fat_archs[i].cpusubtype == CPU_SUBTYPE_POWERPC_750)
		    return(fat_archs + i);
	    }
	    for(i = 0; i < nfat_archs; i++){
		if(fat_archs[i].cputype != cputype)
		    continue;
		if(fat_archs[i].cpusubtype == CPU_SUBTYPE_POWERPC_604e)
		    return(fat_archs + i);
	    }
	    for(i = 0; i < nfat_archs; i++){
		if(fat_archs[i].cputype != cputype)
		    continue;
		if(fat_archs[i].cpusubtype == CPU_SUBTYPE_POWERPC_604)
		    return(fat_archs + i);
	    }
	    for(i = 0; i < nfat_archs; i++){
		if(fat_archs[i].cputype != cputype)
		    continue;
		if(fat_archs[i].cpusubtype == CPU_SUBTYPE_POWERPC_603ev)
		    return(fat_archs + i);
	    }
	    for(i = 0; i < nfat_archs; i++){
		if(fat_archs[i].cputype != cputype)
		    continue;
		if(fat_archs[i].cpusubtype == CPU_SUBTYPE_POWERPC_603e)
		    return(fat_archs + i);
	    }
	    for(i = 0; i < nfat_archs; i++){
		if(fat_archs[i].cputype != cputype)
		    continue;
		if(fat_archs[i].cpusubtype == CPU_SUBTYPE_POWERPC_603)
		    return(fat_archs + i);
	    }
	    for(i = 0; i < nfat_archs; i++){
		if(fat_archs[i].cputype != cputype)
		    continue;
		if(fat_archs[i].cpusubtype == CPU_SUBTYPE_POWERPC_ALL)
		    return(fat_archs + i);
	    }
	    break;
	case CPU_TYPE_MC88000:
	    for(i = 0; i < nfat_archs; i++){
		if(fat_archs[i].cputype != cputype)
		    continue;
		if(fat_archs[i].cpusubtype == CPU_SUBTYPE_MC88000_ALL)
		    return(fat_archs + i);
	    }
	    break;
	case CPU_TYPE_I860:
	    for(i = 0; i < nfat_archs; i++){
		if(fat_archs[i].cputype != cputype)
		    continue;
		if(fat_archs[i].cpusubtype == CPU_SUBTYPE_I860_ALL)
		    return(fat_archs + i);
	    }
	    break;
	case CPU_TYPE_HPPA:
	    for(i = 0; i < nfat_archs; i++){
		if(fat_archs[i].cputype != cputype)
		    continue;
		if(fat_archs[i].cpusubtype == CPU_SUBTYPE_HPPA_ALL)
		    return(fat_archs + i);
	    }
	    break;
	case CPU_TYPE_SPARC:
	    for(i = 0; i < nfat_archs; i++){
		if(fat_archs[i].cputype != cputype)
		    continue;
		if(fat_archs[i].cpusubtype == CPU_SUBTYPE_SPARC_ALL)
		    return(fat_archs + i);
	    }
	    break;
	default:
	    return(NULL);
	}
	return(NULL);
}

/*
 * NXCombineCpuSubtypes() returns the resulting cpusubtype when combining two
 * different cpusubtypes for the specified cputype.  If the two cpusubtypes
 * can't be combined (the specific subtypes are mutually exclusive) -1 is
 * returned indicating it is an error to combine them.  This can also fail and
 * return -1 if new cputypes or cpusubtypes are added and an old version of
 * this routine is used.  But if the cpusubtypes are the same they can always
 * be combined and this routine will return the cpusubtype pass in.
 */
cpu_subtype_t
NXCombineCpuSubtypes(
cpu_type_t cputype,
cpu_subtype_t cpusubtype1,
cpu_subtype_t cpusubtype2)
{
	if(cpusubtype1 == cpusubtype2)
	    return(cpusubtype1);

	switch(cputype){
	case CPU_TYPE_I386:
	    /*
	     * For compatiblity with pre-Rhapsody CR1 systems the subtypes that
	     * previously existed are handled the same as before.  So going in
	     * we know we don't have an exact match because of the test done
	     * at the beginning of the routine.
	     */
	    if((cpusubtype1 == CPU_SUBTYPE_I386_ALL ||
	        /* cpusubtype1 == CPU_SUBTYPE_386 || same as above */
	        cpusubtype1 == CPU_SUBTYPE_486 ||
	        cpusubtype1 == CPU_SUBTYPE_486SX ||
	        cpusubtype1 == CPU_SUBTYPE_586) &&
	       (cpusubtype2 == CPU_SUBTYPE_I386_ALL ||
	        /* cpusubtype2 == CPU_SUBTYPE_386 || same as above */
	        cpusubtype2 == CPU_SUBTYPE_486 ||
	        cpusubtype2 == CPU_SUBTYPE_486SX ||
	        cpusubtype2 == CPU_SUBTYPE_586)){
		/* return the highest subtype of the two */
		if(cpusubtype1 == CPU_SUBTYPE_586 ||
		   cpusubtype2 == CPU_SUBTYPE_586)
		    return(CPU_SUBTYPE_586);
		if(cpusubtype1 == CPU_SUBTYPE_486SX ||
		   cpusubtype2 == CPU_SUBTYPE_486SX)
		    return(CPU_SUBTYPE_486SX);
		if(cpusubtype1 == CPU_SUBTYPE_486 ||
		   cpusubtype2 == CPU_SUBTYPE_486)
		    return(CPU_SUBTYPE_486);
		break; /* logically can't get here */
	    }
	    /*
	     * If either is a MODEL_ALL type select the one with the highest
	     * family type.  Note that only the old group of subtypes (ALL, 386,
	     * 486, 486SX, 586) can ever have MODEL_ALL (a model number of
	     * zero).  Since the cases both subtypes being in the old group are
	     * handled above this makes the the highest family type the one that
	     * is not MODEL_ALL.
	     */
	    if(CPU_SUBTYPE_INTEL_MODEL(cpusubtype1) ==
	       CPU_SUBTYPE_INTEL_MODEL_ALL)
		return(cpusubtype2);
	    if(CPU_SUBTYPE_INTEL_MODEL(cpusubtype2) ==
	       CPU_SUBTYPE_INTEL_MODEL_ALL)
		return(cpusubtype1);
	    /*
	     * For all other families and models they must match exactly to
	     * combine and since that test was done at the start of this routine
	     * we know these do not match and do not combine.
	     */
	    return((cpu_subtype_t)-1);
	    break; /* logically can't get here */

	case CPU_TYPE_MC680x0:
	    if(cpusubtype1 != CPU_SUBTYPE_MC680x0_ALL &&
	       cpusubtype1 != CPU_SUBTYPE_MC68030_ONLY &&
	       cpusubtype1 != CPU_SUBTYPE_MC68040)
		return((cpu_subtype_t)-1);
	    if(cpusubtype2 != CPU_SUBTYPE_MC680x0_ALL &&
	       cpusubtype2 != CPU_SUBTYPE_MC68030_ONLY &&
	       cpusubtype2 != CPU_SUBTYPE_MC68040)
		return((cpu_subtype_t)-1);

	    if(cpusubtype1 == CPU_SUBTYPE_MC68030_ONLY &&
	       cpusubtype2 == CPU_SUBTYPE_MC68040)
		return((cpu_subtype_t)-1);
	    if(cpusubtype1 == CPU_SUBTYPE_MC68040 &&
	       cpusubtype2 == CPU_SUBTYPE_MC68030_ONLY)
		return((cpu_subtype_t)-1);

	    if(cpusubtype1 == CPU_SUBTYPE_MC68030_ONLY ||
	       cpusubtype2 == CPU_SUBTYPE_MC68030_ONLY)
		return(CPU_SUBTYPE_MC68030_ONLY);

	    if(cpusubtype1 == CPU_SUBTYPE_MC68040 ||
	       cpusubtype2 == CPU_SUBTYPE_MC68040)
		return(CPU_SUBTYPE_MC68040);
	    break; /* logically can't get here */

	case CPU_TYPE_POWERPC:
	    /*
	     * Combining with the ALL type becomes the other type. Combining
	     * anything with the 601 becomes 601.  All other non exact matches
	     * combine to the ALL type.
	     */
	    if(cpusubtype1 == CPU_SUBTYPE_POWERPC_ALL)
		return(cpusubtype2);
	    if(cpusubtype2 == CPU_SUBTYPE_POWERPC_ALL)
		return(cpusubtype1);

	    if(cpusubtype1 == CPU_SUBTYPE_POWERPC_601 ||
	       cpusubtype2 == CPU_SUBTYPE_POWERPC_601)
		return(CPU_SUBTYPE_POWERPC_601);

	    return(CPU_SUBTYPE_POWERPC_ALL);
	    break; /* logically can't get here */

	case CPU_TYPE_MC88000:
	    if(cpusubtype1 != CPU_SUBTYPE_MC88000_ALL &&
	       cpusubtype1 != CPU_SUBTYPE_MC88110)
		return((cpu_subtype_t)-1);
	    if(cpusubtype2 != CPU_SUBTYPE_MC88000_ALL &&
	       cpusubtype2 != CPU_SUBTYPE_MC88110)
		return((cpu_subtype_t)-1);

	    if(cpusubtype1 == CPU_SUBTYPE_MC88110 ||
	       cpusubtype2 == CPU_SUBTYPE_MC88110)
		return(CPU_SUBTYPE_MC88110);

	    break; /* logically can't get here */

	case CPU_TYPE_I860:
	    if(cpusubtype1 != CPU_SUBTYPE_I860_ALL &&
	       cpusubtype1 != CPU_SUBTYPE_I860_860)
		return((cpu_subtype_t)-1);
	    if(cpusubtype2 != CPU_SUBTYPE_I860_ALL &&
	       cpusubtype2 != CPU_SUBTYPE_I860_860)
		return((cpu_subtype_t)-1);

	    if(cpusubtype1 == CPU_SUBTYPE_I860_860 ||
	       cpusubtype2 == CPU_SUBTYPE_I860_860)
		return(CPU_SUBTYPE_I860_860);
	    break; /* logically can't get here */

	case CPU_TYPE_HPPA:
	    if(cpusubtype1 != CPU_SUBTYPE_HPPA_ALL &&
	       cpusubtype1 != CPU_SUBTYPE_HPPA_7100LC)
		return((cpu_subtype_t)-1);
	    if(cpusubtype2 != CPU_SUBTYPE_HPPA_ALL &&
	       cpusubtype2 != CPU_SUBTYPE_HPPA_7100LC)
		return((cpu_subtype_t)-1);

	    return(CPU_SUBTYPE_HPPA_7100LC);
	    break; /* logically can't get here */

	case CPU_TYPE_SPARC:
	    if(cpusubtype1 != CPU_SUBTYPE_SPARC_ALL)
			return((cpu_subtype_t)-1);
	    if(cpusubtype2 != CPU_SUBTYPE_SPARC_ALL)
			return((cpu_subtype_t)-1);
	    break; /* logically can't get here */

	default:
	    return((cpu_subtype_t)-1);
	}
	return((cpu_subtype_t)-1); /* logically can't get here */
}
