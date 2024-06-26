/******************************************************************************
 * Copyright 1991 Advanced Micro Devices, Inc.
 *
 * This software is the property of Advanced Micro Devices, Inc  (AMD)  which
 * specifically  grants the user the right to modify, use and distribute this
 * software provided this notice is not removed or altered.  All other rights
 * are reserved by AMD.
 *
 * AMD MAKES NO WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, WITH REGARD TO THIS
 * SOFTWARE.  IN NO EVENT SHALL AMD BE LIABLE FOR INCIDENTAL OR CONSEQUENTIAL
 * DAMAGES IN CONNECTION WITH OR ARISING FROM THE FURNISHING, PERFORMANCE, OR
 * USE OF THIS SOFTWARE.
 *
 * Comments about this software should be directed to udi@amd.com. If access
 * to electronic mail isn't available, send mail to:
 *
 * Advanced Micro Devices, Inc.
 * 29K Support Products
 * Mail Stop 573
 * 5900 E. Ben White Blvd.
 * Austin, TX 78741
 *****************************************************************************
 *       $Id: udipt29k.h,v 1.1.1.1 2000/03/16 19:57:02 kdienes Exp $
 *	 $Id: @(#)udipt29k.h	2.5, AMD
 */

/* This file is to be used to reconfigure the UDI Procedural interface
   for a given target. This file should be placed so that it will be
   included from udiproc.h. Everything in here will probably need to
   be changed when you change the target processor. Nothing in here
   should need to change when you change hosts or compilers.
*/

/* Select a target CPU Family */
#define TargetCPUFamily 	Am29K

/* Enumerate the processor specific values for Space in a resource */
#define UDI29KDRAMSpace		0
#define UDI29KIOSpace		1
#define UDI29KCPSpace0		2
#define UDI29KCPSpace1		3
#define UDI29KIROMSpace		4
#define UDI29KIRAMSpace		5
#define UDI29KLocalRegs		8
#define UDI29KGlobalRegs	9
#define UDI29KRealRegs		10
#define UDI29KSpecialRegs	11
#define UDI29KTLBRegs		12	/* Not Am29005 */
#define UDI29KACCRegs		13	/* Am29050 only */
#define UDI29KICacheSpace	14	/* Am2903x only */
#define UDI29KAm29027Regs	15	/* When available */
#define UDI29KPC		16
#define UDI29KDCacheSpace	17	/* When available */

/* Enumerate the Co-processor registers */
#define UDI29KCP_F		0
#define UDI29KCP_Flag		8
#define UDI29KCP_I		12
#define UDI29KCP_ITmp		16
#define UDI29KCP_R		20
#define UDI29KCP_S		28
#define UDI29KCP_RTmp		36
#define UDI29KCP_STmp		44
#define UDI29KCP_Stat		52
#define UDI29KCP_Prec		56
#define UDI29KCP_Reg0		60
#define UDI29KCP_Reg1		68
#define UDI29KCP_Reg2		76
#define UDI29KCP_Reg3		84
#define UDI29KCP_Reg4		92
#define UDI29KCP_Reg5		100
#define UDI29KCP_Reg6		108
#define UDI29KCP_Reg7		116
#define UDI29KCP_Mode		124

/* Enumerate the stacks in StackSizes array */
#define UDI29KMemoryStack	0
#define UDI29KRegisterStack	1

/* Enumerate the chips for ChipVersions array */
#define UDI29K29KVersion	0
#define UDI29K29027Version	1

/* Define special value for elements of ChipVersions array for
 * chips not present */
#define UDI29KChipNotPresent	-1

typedef	UDIInt32		UDICount;
typedef	UDIUInt32		UDISize;

typedef UDIInt			CPUSpace;
typedef UDIUInt32		CPUOffset;
typedef	UDIUInt32		CPUSizeT;
