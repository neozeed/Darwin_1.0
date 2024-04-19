/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
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
 *	PCI Control registers for Cmd646X chipset 
 *
 */
#define	Ultra646CFR				0x50		/* Configuration */
#define Ultra646CFR_DSA1			0x40
#define Ultra646CFR_IDEIntPRI			0x04

#define Ultra646CNTRL				0x51		/* Drive 0/1 Control Register */
#define Ultra646CNTRL_Drive1ReadAhead		0x80
#define Ultra646CNTRL_Drive0ReadAhead		0x40
#define Ultra646CNTRL_EnableSDY			0x08
#define Ultra646CNTRL_EnablePRI			0x04

#define Ultra646CMDTIM				0x52		/* Task file timing (all drives) */
#define Ultra646CMDTIM_Drive01CmdActive		0xF0
#define Ultra646CMDTIM_Drive01CmdRecovery	0x0F

#define Ultra646ARTTIM0				0x53		/* Drive 0 Address Setup */
#define Ultra646ARTTIM0_Drive0AddrSetup		0xC0

#define Ultra646DRWTIM0				0x54		/* Drive 0 Data Read/Write - DACK Time	*/
#define Ultra646DRWTIM0_Drive0DataActive	0xF0
#define Ultra646DRWTIM0_Drive0DataRecovery	0x0F

#define Ultra646ARTTIM1				0x55		/* Drive 1 Address Setup */
#define Ultra646ARTTIM1_Drive1AddrSetup		0xC0

#define Ultra646DRWTIM1				0x56		/* Drive 1 Data Read/Write - DACK Time */
#define Ultra646DRWTIM1_Drive1DataActive	0xF0
#define Ultra646DRWTIM1_Drive1DataRecover	0x0F

#define Ultra646ARTTIM23			0x57		/* Drive 2/3 Control/Status */
#define Ultra646ARTTIM23_AddrSetup		0xC0
#define Ultra646ARTTIM23_IDEIntSDY		0x10
#define Ultra646ARTTIM23_Drive3ReadAhead	0x08
#define Ultra646ARTTIM23_Drive2ReadAhead	0x04

#define Ultra646DRWTIM2				0x58		/* Drive 2 Read/Write - DACK Time */
#define Ultra646DRWTIM2_Drive2DataActive	0xF0	
#define Ultra646DRWTIM2_Drive2DataRecovery	0x0F

#define Ultra646BRST				0x59		/* Read Ahead Count */

#define Ultra646DRWTIM3				0x5B		/* Drive 3 Read/Write - DACK Time */
#define Ultra646DRWTIM3_Drive3DataActive	0xF0
#define Ultra646DRWTIM3_Drive3DataRecover	0x0F

#define Ultra646BMIDECR0			0x70		/* BusMaster Command Register - Primary */
#define Ultra646BMIDECR0_PCIWritePRI		0x08
#define Ultra646BMIDECR0_StartDMAPRI		0x01

#define Ultra646MRDMODE				0x71		/* DMA Master Read Mode Select */
#define Ultra646MRDMODE_PCIReadMask		0x03
#define Ultra646MRDMODE_PCIRead			0x00
#define Ultra646MRDMODE_PCIReadMultiple		0x01
#define Ultra646MRDMODE_IDEIntPRI		0x04
#define Ultra646MRDMODE_IDEIntSDY		0x08
#define Ultra646MRDMODE_IntEnablePRI		0x10
#define Ultra646MRDMODE_IntEnableSDY		0x20
#define Ultra646MRDMODE_ResetAll		0x40

#define Ultra646BMIDESR0			0x72		/* BusMaster Status Register - Primary */
#define Ultra646BMIDESR0_Simplex		0x80
#define Ultra646BMIDESR0_Drive1DMACap		0x40
#define Ultra646BMIDESR0_Drive0DMACap		0x20
#define Ultra646BMIDESR0_DMAIntPRI		0x04
#define Ultra646BMIDESR0_DMAErrorPRI		0x02
#define Ultra646BMIDESR0_DMAActivePRI		0x01

#define Ultra646UDIDETCR0			0x73		/* Ultra DMA Timing Control Register - Primary */
#define Ultra646UDIDETCR0_Drive1UDMACycleTime	0xC0
#define Ultra646UDIDETCR0_Drive0UDMACycleTime	0x30
#define Ultra646UDIDETCR0_Drive1UDMAEnable	0x02
#define Ultra646UDIDETCR0_Drive0UDMAEnable	0x01

#define Ultra646DTPR0				0x74		/* Descriptor Table Pointer - Primary */

#define Ultra646BMIDECR1			0x78		/* BusMaster Command Register - Secondary */
#define Ultra646BMIDECR1_PCIWriteSDY		0x08
#define Ultra646BMIDECR1_StartDMASDY		0x01

#define Ultra646BMIDESR1			0x7A		/* BusMaster Status Register - Secondary */
#define Ultra646BMIDESR1_Simplex		0x80
#define Ultra646BMIDESR1_Drive3DMACap		0x40
#define Ultra646BMIDESR1_Drive2DMACap		0x20
#define Ultra646BMIDESR1_DMAIntSDY		0x04
#define Ultra646BMIDESR1_DMAErrorSDY		0x02
#define Ultra646BMIDESR1_DMAActiveSDY		0x01

#define Ultra646UDIDETCR1			0x7B		/* Ultra DMA Timing Control Register - Secondary */
#define Ultra646UDIDETCR1_Drive3UDMACycleTime	0xC0
#define Ultra646UDIDETCR1_Drive2UDMACycleTime	0x30
#define Ultra646UDIDETCR1_Drive3UDMAEnable	0x02
#define Ultra646UDIDETCR1_Drive2UDMAEnable	0x01

#define Ultra646DTPR1				0x7C		/* Descriptor Table Pointer - Primary */

typedef struct
{
    UInt32    		cntrlReg;
    UInt32		arttimReg;
    UInt32		cmdtimReg;
    UInt32		drwtimRegPIO;
    UInt32		drwtimRegDMA;
    UInt32		udidetcrReg;
} Ultra646Regs;       


typedef struct
{
    UInt32		start;
    UInt32		length;
} Ultra646Descriptor;   


#define IDE_SYSCLK_NS		30
