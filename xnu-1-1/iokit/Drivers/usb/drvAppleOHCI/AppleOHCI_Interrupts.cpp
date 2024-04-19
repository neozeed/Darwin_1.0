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
 * Copyright (c) 1998 Apple Computer, Inc.  All rights reserved.
 *
 * HISTORY
 *
 */


#include "AppleOHCI.h"
#include <libkern/OSByteOrder.h>

#define nil (0)
#define DEBUGGING_LEVEL 0	// 1 = low; 2 = high; 3 = extreme

#define super IOUSBController
#define self this

void AppleOHCI::pollInterrupts(IOUSBCompletionAction safeAction)
{
    register OHCIRegistersPtr	pOHCIRegisters;
    register UInt32		activeInterrupts;
    register UInt32		interruptEnable;

    pOHCIRegisters = pOHCIUIMData->pOHCIRegisters;

    interruptEnable = OSSwapInt32(pOHCIRegisters->hcInterruptEnable);

    activeInterrupts = interruptEnable &
        OSSwapInt32(pOHCIRegisters->hcInterruptStatus);

    if ((interruptEnable & kOHCIHcInterrupt_MIE) && (activeInterrupts != 0))
    {
        /*
         * SchedulingOverrun Interrupt
         */
        if (activeInterrupts & kOHCIHcInterrupt_SO)
        {
            pOHCIUIMData->errors.scheduleOverrun++;
            pOHCIRegisters->hcInterruptStatus = OSSwapInt32(kOHCIHcInterrupt_SO);
            IOSync();

#if (DEBUGGING_LEVEL > 0)
            IOLog("<SchedulingOverrun Interrupt>\n");
#endif
        }
        /*
         * WritebackDoneHead Interrupt
         */
        if (activeInterrupts & kOHCIHcInterrupt_WDH)
        {
            UIMProcessDoneQueue(safeAction);

#if (DEBUGGING_LEVEL > 0)
            IOLog("<WritebackDoneHead Interrupt>\n");
#endif
        }
        /*
         * StartofFrame Interrupt
         */
        if (activeInterrupts & kOHCIHcInterrupt_SF)
        {
            // Clear the interrrupt
            pOHCIRegisters->hcInterruptStatus = OSSwapInt32(kOHCIHcInterrupt_SF);

            // and mask it off so it doesn't happen again.
            // will have to be turned on manually to happen again.
            pOHCIRegisters->hcInterruptDisable = OSSwapInt32(kOHCIHcInterrupt_SF);

#if (DEBUGGING_LEVEL > 0)
            IOLog("<Frame Interrupt>\n");
#endif
            // FIXME? ERIC performCommand(ROOT_HUB_FRAME, (void *)0);
        }
        /*
         * ResumeDetected Interrupt
         */
        if (activeInterrupts & kOHCIHcInterrupt_RD)
        {
            pOHCIRegisters->hcInterruptStatus = OSSwapInt32(kOHCIHcInterrupt_RD);
#if (DEBUGGING_LEVEL > 0)
            IOLog("<ResumeDetected Interrupt>\n");
#endif
        }
        /*
         * Unrecoverable Error Interrupt
         */
        if (activeInterrupts & kOHCIHcInterrupt_UE)
        {
            pOHCIUIMData->errors.unrecoverableError++;
            // Let's do a SW reset to recover from this condition.
            // We could make sure all OCHI registers and in-memory
            // data structures are valid, too.
            pOHCIRegisters->hcCommandStatus = OSSwapInt32
                                                (kOHCIHcCommandStatus_HCR);
            delay(10 * MICROSECOND);
            pOHCIRegisters->hcInterruptStatus = OSSwapInt32(kOHCIHcInterrupt_UE);
            // zzzz - note I'm leaving the Control/Bulk list processing off
            // for now.  FIXME? ERIC

            pOHCIRegisters->hcControl = OSSwapInt32
                ((kOHCIFunctionalState_Operational << kOHCIHcControl_HCFSPhase)
                 | kOHCIHcControl_PLE);

#if (DEBUGGING_LEVEL > 0)
            IOLog("<Unrecoverable Error Interrupt>\n");
#endif
        }
        /*
         * FrameNumberOverflow Interrupt
         */
        if (activeInterrupts & kOHCIHcInterrupt_FNO)
        {
            // not really an error, but close enough
            pOHCIUIMData->errors.frameNumberOverflow++;
            if ((OSReadLittleInt16 (pOHCIUIMData->pHCCA, 0x80)
                 & kOHCIFmNumberMask) < kOHCIBit15)
            {
                pOHCIUIMData->frameNumber += kOHCIFrameOverflowBit;
            }
            pOHCIRegisters->hcInterruptStatus = OSSwapInt32(kOHCIHcInterrupt_FNO);
#if (DEBUGGING_LEVEL > 0)
            IOLog("<FrameNumberOverflow Interrupt>\n");
#endif
        }
        /*
         * RootHubStatusChange Interrupt
         */
        if (activeInterrupts & kOHCIHcInterrupt_RHSC)
        {
            // Clear status change.
            pOHCIRegisters->hcInterruptStatus =
                                            OSSwapInt32(kOHCIHcInterrupt_RHSC);

#if (DEBUGGING_LEVEL > 0)
            IOLog("<RHSC Interrupt>\n");
#endif

            UIMRootHubStatusChange();
        }
        /*
         * OwnershipChange Interrupt
         */
        if (activeInterrupts & kOHCIHcInterrupt_OC)
        {
            // well, we certainly weren't expecting this!
            pOHCIUIMData->errors.ownershipChange++;
            pOHCIRegisters->hcInterruptStatus = OSSwapInt32(kOHCIHcInterrupt_OC);

#if (DEBUGGING_LEVEL > 0)
            IOLog("<OwnershipChange Interrupt>\n");
#endif
        }
    }
}

void AppleOHCI::OHCIUIMInterruptHandler(OSObject *owner,
                                        IOInterruptEventSource * /*source*/,
                                        int /*count*/)
{
    register AppleOHCI		*controller = (AppleOHCI *) owner;

    if (!controller)
        return;
    // Finish pending transactions first.
    controller->finishPending();
    controller->pollInterrupts();
}

extern void print_td(OHCIGeneralTransferDescriptorPtr x);
