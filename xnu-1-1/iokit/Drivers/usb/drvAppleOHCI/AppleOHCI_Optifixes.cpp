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

#define super IOUSBController
#define self this

void AppleOHCI::DoOptiFix(OHCIEndpointDescriptorPtr pHead)
{
    OHCIEndpointDescriptorPtr pED;
    OHCIGeneralTransferDescriptorPtr pTD;
    int i = 0;

//  DebugStr ((ConstStr255Param) "DoOptiFix");

    for (i=0; i<27; i++) {
        // allocate ED
        pED = OHCIUIMAllocateED();
        pED->pLogicalNext = nil;
        //make ED and FA = 0
        pED->flags = 0;
        pTD = OHCIUIMAllocateTD();
        pED->tdQueueHeadPtr = OSSwapInt32 ((UInt32) pTD->pPhysical);
        pTD->nextTD = pED->tdQueueTailPtr;
        pTD->pEndpoint = pED;
        pTD->pType = kOHCIOptiLSBug;
        pED->tdQueueTailPtr = OSSwapInt32 ((UInt32) pTD->pPhysical);

        pED->nextED = pHead->nextED;
        pHead->nextED = OSSwapInt32((UInt32) pED->pPhysical);
        pHead = pED;
    }
}

void AppleOHCI::OptiLSHSFix(void)
{

//  Do Opti Errata stuff here!!!!!!
    int i;
    OHCIIntHeadPtr              pInterruptHead;
    OHCIEndpointDescriptorPtr   pControlED;
    OHCIRegistersPtr            pOHCIRegisters;
    UInt32			controlState;

    pOHCIRegisters = pOHCIUIMData->pOHCIRegisters;

    pOHCIUIMData->OptiOn = 1;

    //Turn off list processing
    controlState = OSReadLittleInt32(&pOHCIRegisters->hcControl, 0);
    OSWriteLittleInt32(&pOHCIRegisters->hcControl, 0, 
	kOHCIFunctionalState_Operational << kOHCIHcControl_HCFSPhase);

    // wait a millisecond
    IOSleep(2);             

    pInterruptHead = pOHCIUIMData->pInterruptHead;

    // add dummy EDs to 8ms interrupts
    for ( i = 0; i< 8; i++)
        DoOptiFix(pInterruptHead[48 + i].pHead);

    //Assign Tail of Control to point to head of Bulk

    pControlED = (OHCIEndpointDescriptorPtr) pOHCIUIMData->pControlTail;
    pControlED->nextED = pOHCIRegisters->hcBulkHeadED;

    // add dummy EDs to end of Control
    DoOptiFix( (OHCIEndpointDescriptorPtr) pOHCIUIMData->pControlTail);

    // turn on everything previous but Bulk
    controlState &= ~kOHCIHcControl_BLE;
    OSWriteLittleInt32(&pOHCIRegisters->hcControl, 0, controlState);

    //End of Opti Fix
}
