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

#include <libkern/OSByteOrder.h>
#include <IOKit/IOMemoryCursor.h>

#include "AppleOHCI.h"
#include <IOKit/usb/USB.h>

#define DEBUGGING_LEVEL 0	// 1 = low; 2 = high; 3 = extreme
#define DEBUGLOG kprintf
#define nil (0)

#define super IOUSBController
#define self this

#define NUM_BUFFER_PAGES	9   // 54
#define NUM_TDS			255 // 1500
#define NUM_EDS			256 // 1500
#define NUM_ITDS		192 // 1300

// TDs  per page == 85
// EDs  per page == 128
// ITDs per page == 64

static int GetEDType(OHCIEndpointDescriptorPtr pED);
static void ProcessCompletedITD (OHCIIsochTransferDescriptorPtr pITD, IOReturn status);
extern void print_td(OHCIGeneralTransferDescriptorPtr pTD);
extern void print_itd(OHCIIsochTransferDescriptorPtr x);

static IOReturn TranslateStatusToUSBError(UInt32 status);

OSDefineMetaClassAndStructors(AppleOHCI, IOUSBController)

bool AppleOHCI::init(OSDictionary * propTable)
{
    if (!super::init(propTable))  return false;

    _intLock = IOLockAlloc();
    if (!_intLock)
        return(false);

    pOHCIUIMData = (OHCIUIMDataPtr) IOMalloc(sizeof(OHCIUIMData));
    if (!pOHCIUIMData)
    {
        IOLog("%s: Unable to allocate memory (1)\n", getName());
        return(false);
    }
    bzero( pOHCIUIMData, sizeof(OHCIUIMData));
        
    return (true);
}

void AppleOHCI::SetVendorInfo(void)
{
    OSData		*vendProp, *deviceProp, *revisionProp;

    // get this chips vendID, deviceID, revisionID
    vendProp     = (OSData *) _device->getProperty( "vendor-id" );
    if (vendProp)
        _vendorID = *((UInt32 *) vendProp->getBytesNoCopy());
    deviceProp   = (OSData *) _device->getProperty( "device-id" );
    if (deviceProp)
        _deviceID   = *((UInt32 *) deviceProp->getBytesNoCopy());
    revisionProp = (OSData *) _device->getProperty( "revision-id" );
    if (revisionProp)
        _revisionID = *((UInt32 *) revisionProp->getBytesNoCopy());
}

IOReturn AppleOHCI::UIMInitialize(IOService * provider)
{
    /* UInt16		ivalue, ivalue2; */
    IOReturn		err = 0;
    OHCIRegistersPtr	pOHCIRegisters;
    UInt32		lvalue;

#if (DEBUGGING_LEVEL > 0)
    IOLog("%s: initializing UIM\n", getName());
#endif

    _device = OSDynamicCast(IOPCIDevice, provider);
    if(_device == NULL)
        return kIOReturnBadArgument;

    do {

        if (!(_deviceBase = provider->mapDeviceMemoryWithIndex(0)))
        {
            IOLog("%s: unable to get device memory\n", getName());
            break;
        }

        IOLog("%s: config @ %lx (%lx)\n", getName(),
              _deviceBase->getVirtualAddress(),
              _deviceBase->getPhysicalAddress());

        SetVendorInfo();

        interruptSource = IOInterruptEventSource::
            interruptEventSource(this, &OHCIUIMInterruptHandler, _device);
        if (!interruptSource
        || (_workLoop->addEventSource(interruptSource) != kIOReturnSuccess))
            continue;

        _genCursor =
            IONaturalMemoryCursor::withSpecification(PAGE_SIZE, PAGE_SIZE);
        if(!_genCursor)
            continue;

        _isoCursor =
            IONaturalMemoryCursor::withSpecification(kUSBMaxIsocFrameReqCount, 
							kUSBMaxIsocFrameReqCount);
        if(!_isoCursor)
            continue;

        /*
         * Initialize my data and the hardware
         */
        pOHCIUIMData->errataBits = GetErrataBits(_vendorID, _deviceID, _revisionID);

#if (DEBUGGING_LEVEL > 0)
        IOLog("%s: errata bits=%lx\n", getName(), pOHCIUIMData->errataBits);
#endif
        pOHCIUIMData->pageSize = PAGE_SIZE;
        pOHCIUIMData->pOHCIRegisters = pOHCIRegisters =
            (OHCIRegistersPtr) _deviceBase->getVirtualAddress();

#if (DEBUGGING_LEVEL > 2)
        dumpRegs();
#endif
        
        // enable the card
        lvalue = _device->configRead32(cwCommand);
        _device->configWrite32(cwCommand, (lvalue & 0xffff0000) |
                              (cwCommandEnableBusMaster |
                               cwCommandEnableMemorySpace));

        // Allocate TDs, EDs; FIXME get real numbers to use, CPU specific.
        if ((err = OHCIUIMAllocateMemory(NUM_TDS, NUM_EDS, NUM_ITDS)))
            continue;

        pOHCIRegisters->hcControlCurrentED = 0;
        pOHCIRegisters->hcControlHeadED = 0;
        pOHCIRegisters->hcDoneHead = 0;
        IOSync();

        // Set up HCCA.
        IOPhysicalAddress physAddr;
        pOHCIUIMData->pHCCA = (Ptr) IOMallocContiguous(kHCCAsize, kHCCAalignment, &physAddr);
        if (!pOHCIUIMData->pHCCA)
        {
            IOLog("%s: Unable to allocate memory (2)\n", getName());
            err = kIOReturnNoMemory;
            continue;
        }

        OSWriteLittleInt32(&pOHCIRegisters->hcHCCA, 0, physAddr);
        IOSync();

        // Set the HC to write the donehead to the HCCA, and enable interrupts
        pOHCIRegisters->hcInterruptStatus = OSSwapInt32(kOHCIHcInterrupt_WDH);
        IOSync();

	// Enable the interrupt delivery.
	_workLoop->enableAllInterrupts();

        // Initialize the Root Hub registers
        OHCIRootHubPower(1 /* kOn */);
        IOSync();
        pOHCIUIMData->rootHubFuncAddress = 1;

        // set up Interrupt transfer tree
        if ((err = OHCIUIMIsochronousInitialize()))	continue;
        if ((err = OHCIUIMInterruptInitialize()))	continue;
        if ((err = OHCIUIMBulkInitialize()))		continue;
        if ((err = OHCIUIMControlInitialize()))		continue;

        // Set up hcFmInterval.
        UInt32	hcFSMPS;				// in register hcFmInterval
        UInt32	hcFI;					// in register hcFmInterval
        UInt32	hcPS;					// in register hcPeriodicStart

        hcFI = OSReadLittleInt32(&pOHCIRegisters->hcFmInterval, 0) & kOHCIHcFmInterval_FI;
        // this formula is from the OHCI spec, section 5.4
        hcFSMPS = ((((hcFI-kOHCIMax_OverHead) * 6)/7) << kOHCIHcFmInterval_FSMPSPhase);
        hcPS = (hcFI * 9) / 10;			// per spec- 90%
        OSWriteLittleInt32(&pOHCIRegisters->hcFmInterval, 0, hcFI | hcFSMPS);
        OSWriteLittleInt32(&pOHCIRegisters->hcPeriodicStart, 0, hcPS);

        IOSync();


        // Just so we all start from the same place, reset the OHCI.
        pOHCIRegisters->hcControl =
            OSSwapInt32 ((kOHCIFunctionalState_Reset
                                 << kOHCIHcControl_HCFSPhase));
        IOSync();

        // Set OHCI to operational state and enable processing of control list.
        pOHCIRegisters->hcControl =
            OSSwapInt32 ((kOHCIFunctionalState_Operational
                                 << kOHCIHcControl_HCFSPhase)
                                | kOHCIHcControl_CLE | kOHCIHcControl_BLE
                                | kOHCIHcControl_PLE | kOHCIHcControl_IE);
        IOSync();

        pOHCIRegisters->hcInterruptEnable =
            OSSwapInt32 (kOHCIHcInterrupt_MIE | kOHCIDefaultInterrupts);
        IOSync();
        
        if (pOHCIUIMData->errataBits & kErrataLSHSOpti)
            OptiLSHSFix();
            
//naga  (*pOHCIUIMData->interruptEnabler)(pOHCIUIMData->interruptSetMember, nil);

        return(kIOReturnSuccess);

    } while (false);

    IOLog("%s: Error occurred (%d)\n", getName(), err);
    UIMFinalize();

    if (interruptSource) interruptSource->release();

    return(err);
}

IOReturn AppleOHCI::UIMFinalize(void)
{
    OHCIRegistersPtr            pOHCIRegisters = pOHCIUIMData->pOHCIRegisters;


    IOLog ("UIMFinalize (shutting down HW)\n");

    // Disable All OHCI Interrupts
    pOHCIRegisters->hcInterruptDisable = kOHCIHcInterrupt_MIE;
    IOSync();
    
    // Place the USB bus into the Reset State
    pOHCIRegisters->hcControl = OSSwapInt32((kOHCIFunctionalState_Reset
                                              << kOHCIHcControl_HCFSPhase));
    IOSync();

    //  need to wait at least 1ms here
    IOSleep(2);

    // Take away the controllers ability be a bus master.
    _device->configWrite32(cwCommand, cwCommandEnableMemorySpace);

    // Clear all Processing Registers
    pOHCIRegisters->hcHCCA = 0;
    pOHCIRegisters->hcPeriodCurrentED = 0;
    pOHCIRegisters->hcControlHeadED = 0;
    pOHCIRegisters->hcControlCurrentED = 0;
    pOHCIRegisters->hcBulkHeadED = 0;
    pOHCIRegisters->hcBulkCurrentED = 0;
    pOHCIRegisters->hcDoneHead = 0;
    IOSync();

    // turn off the global power
    // FIXME check for per-port vs. Global power control
    OHCIRootHubPower(0 /* kOff */);
    IOSync();

    //status = OHCIUIMFinalizeOHCIUIMData ((OHCIUIMDataPtr) pOHCIUIMData);

    IOLog("OHCIUIMFinalize Exit\n");

    return(kIOReturnSuccess);
}

/*
 * got an error on a TD with no completion routine.
 * Search for a later TD on the same end point which does have one,
 * so we can tell upper layes of the error.
 */
void AppleOHCI::doCallback(OHCIGeneralTransferDescriptorPtr nextTD,
                           UInt32			    transferStatus,
                           UInt32			    bufferSizeRemaining)
{
    OHCIGeneralTransferDescriptorPtr    pCurrentTD;
    OHCIEndpointDescriptorPtr           pED;
    UInt32                              PhysAddr;

    pED = (OHCIEndpointDescriptorPtr) nextTD->pEndpoint;

    PhysAddr = (UInt32) OSSwapInt32(pED->tdQueueHeadPtr) & kOHCIHeadPMask;
    nextTD = (OHCIGeneralTransferDescriptorPtr) OHCIUIMGetLogicalAddress(PhysAddr);

    pCurrentTD = nextTD;
    if(pCurrentTD == nil) {
        IOLog("No transfer descriptors in AppleOHCI::doCallback!\n");
	return;
    }
    while (pCurrentTD->pLogicalNext != nil)
    {
        bufferSizeRemaining += findBufferRemaining (pCurrentTD);

        // make sure this TD won't be added to any future buffer
	// remaining calculations
        pCurrentTD->currentBufferPtr = 0;

        //ERICif (pCurrentTD->preparationID)
        //ERIC    CheckpointIO(pCurrentTD->preparationID, nil);
        // make sure we don't try to do another CheckpointIO later
        //ERICpCurrentTD->preparationID = nil;

        if (pCurrentTD->completion.action != nil)
        {
            IOUSBCompletion completion;
            // zero out callback first then call it
            completion = pCurrentTD->completion;
            pCurrentTD->completion.action = nil;
            complete(completion,
                     TranslateStatusToUSBError(transferStatus),
                     bufferSizeRemaining);
            bufferSizeRemaining = 0;
            return;
        }

        pCurrentTD = pCurrentTD->pLogicalNext;
    }
}

// FIXME add page size to param list
UInt32 AppleOHCI::findBufferRemaining (OHCIGeneralTransferDescriptorPtr pCurrentTD)
{
    UInt32                      pageSize;
    UInt32                      pageMask;
    UInt32                      bufferSizeRemaining;


    pageSize = pOHCIUIMData->pageSize;
    pageMask = ~(pageSize - 1);

    if (pCurrentTD->currentBufferPtr == 0)
    {
        bufferSizeRemaining = 0;
    }
    else if ((OSSwapInt32(pCurrentTD->bufferEnd) & (pageMask)) ==
             (OSSwapInt32(pCurrentTD->currentBufferPtr)& (pageMask)))
    {
        // we're on the same page
        bufferSizeRemaining =
        (OSSwapInt32 (pCurrentTD->bufferEnd) & ~pageMask) -
        (OSSwapInt32 (pCurrentTD->currentBufferPtr) & ~pageMask) + 1;
    }
    else
    {
        bufferSizeRemaining =
        ((OSSwapInt32(pCurrentTD->bufferEnd) & ~pageMask) + 1)  +
        (pageSize - (OSSwapInt32(pCurrentTD->currentBufferPtr) & ~pageMask));
    }

    return (bufferSizeRemaining);
}


IOReturn AppleOHCI::OHCIUIMControlInitialize(void)
{
    OHCIRegistersPtr            pOHCIRegisters;
    OHCIEndpointDescriptorPtr   pED, pED2;


    pOHCIRegisters = pOHCIUIMData->pOHCIRegisters;

    // Create ED mark it skipped and assign it to ControlTail
    pED = OHCIUIMAllocateED();
    pED->flags = OSSwapInt32 (kOHCIEDControl_K);
    pED->nextED = 0;	// End of list

    pOHCIUIMData->pControlTail = (UInt32) pED;

    // Create ED mark it skipped and assign it to control head
    pED2 = OHCIUIMAllocateED();
    pED2->flags = OSSwapInt32 (kOHCIEDControl_K);
    pOHCIUIMData->pControlHead = (UInt32) pED2;
    pOHCIRegisters->hcControlHeadED = OSSwapInt32 ((UInt32) pED2->pPhysical);

    // have bulk head ED point to Control tail ED
    pED2->nextED = OSSwapInt32 ((UInt32) pED->pPhysical);
    pED2->pLogicalNext = pED;
    return (kIOReturnSuccess);
}

IOReturn AppleOHCI::OHCIUIMBulkInitialize (void)
{
    OHCIRegistersPtr            pOHCIRegisters;
    OHCIEndpointDescriptorPtr   pED, pED2;

    pOHCIRegisters = pOHCIUIMData->pOHCIRegisters;

    // Create ED mark it skipped and assign it to bulkTail
    pED = OHCIUIMAllocateED();
    pED->flags = OSSwapInt32 (kOHCIEDControl_K);
    pED->nextED = 0;	// End of list
    pOHCIUIMData->pBulkTail = (UInt32) pED;

    // Create ED mark it skipped and assign it to bulk head
    pED2 = OHCIUIMAllocateED();
    pED2->flags = OSSwapInt32 (kOHCIEDControl_K);
    pOHCIUIMData->pBulkHead = (UInt32) pED2;
    pOHCIRegisters->hcBulkHeadED = OSSwapInt32 ((UInt32) pED2->pPhysical);

    // have bulk head ED point to Bulk tail ED
    pED2->nextED = OSSwapInt32 ((UInt32) pED->pPhysical);
    pED2->pLogicalNext = pED;
    return (kIOReturnSuccess);

}

IOReturn AppleOHCI::OHCIUIMIsochronousInitialize(void)
{
    OHCIRegistersPtr            pOHCIRegisters;
    OHCIEndpointDescriptorPtr   pED, pED2;


    pOHCIRegisters = pOHCIUIMData->pOHCIRegisters;

    // Create ED mark it skipped and assign it to isoch Tail
    pED = OHCIUIMAllocateED();
    pED->flags = OSSwapInt32 (kOHCIEDControl_K);
    pED->nextED = 0;	// End of list
    pOHCIUIMData->pIsochTail = (UInt32) pED;

    // Create ED mark it skipped and assign it to isoch head
    pED2 = OHCIUIMAllocateED();
    pED2->flags = OSSwapInt32 (kOHCIEDControl_K);
    pOHCIUIMData->pIsochHead = (UInt32) pED2;


    // have iso head ED point to iso tail ED
    pED2->nextED = OSSwapInt32 ((UInt32) pED->pPhysical);
    pED2->pLogicalNext = pED;
    pOHCIUIMData->isochBandwidthAvail = kUSBMaxIsocFrameReqCount;

    return (kIOReturnSuccess);
}

//Initializes the HCCA Interrupt list with statically
//disabled ED's to form the Interrupt polling queues
IOReturn AppleOHCI::OHCIUIMInterruptInitialize (void)
{
    OHCIRegistersPtr            pOHCIRegisters;
    OHCIIntHeadPtr              pInterruptHead;
    IOReturn                    status = 0;
    UInt32                      dummyControl;
    int                         i, p, q, z;
    OHCIEndpointDescriptorPtr   pED, pIsochHead;


    pOHCIRegisters = pOHCIUIMData->pOHCIRegisters;
    pInterruptHead = pOHCIUIMData->pInterruptHead;

    // create UInt32 with same dword0 for use with searching and
    // tracking, skip should be set, and open area should be marked
    dummyControl = kOHCIEDControl_K;
    dummyControl |= 0;   //should be kOHCIFakeED
    dummyControl = OSSwapInt32 (dummyControl);
    pIsochHead = (OHCIEndpointDescriptorPtr) pOHCIUIMData->pIsochHead;

    // do 31 times
    // change to 65 and make isoch head the last one.?????
    for (i = 0; i < 63; i++)
    {
        // allocate Endpoint descriptor
        pED = OHCIUIMAllocateED();
        if (pED == nil)
        {
            return (kIOReturnNoMemory);
        }
        // mark skipped,some how mark as a False endpoint zzzzz
        else
        {
            pED->flags = dummyControl;
            pED->nextED = 0;	// End of list
            pInterruptHead[i].pHead = pED;
            pInterruptHead[i].pHeadPhysical = pED->pPhysical;
            pInterruptHead[i].nodeBandwidth = 0;
        }

        if (i < 32)
            ((UInt32 *)pOHCIUIMData->pHCCA)[i] =
                (UInt32) OSSwapInt32 ((UInt32) pInterruptHead[i].pHeadPhysical);
    }

    p = 0;
    q = 32;
    // FIXME? ERIC
    for (i = 0; i < (32 +16 + 8 + 4 + 2); i++)
    {
        if (i < q/2+p)
            z = i + q;
        else
            z = i + q/2;
        if (i == p+q-1)
        {
            p = p + q;
            q = q/2;
        }
        // point endpoint descriptor to corresponding 8ms descriptor
        pED = pInterruptHead[i].pHead;
        pED->nextED =  OSSwapInt32 (pInterruptHead[z].pHeadPhysical);
        pED->pLogicalNext = pInterruptHead[z].pHead;
        pInterruptHead[i].pTail = (OHCIEndpointDescriptorPtr) pED->pLogicalNext;
    }
    i = 62;
    pED = pInterruptHead[i].pHead;
    pED->nextED = OSSwapInt32 (pIsochHead->pPhysical);
    pED->pLogicalNext = pOHCIUIMData->pIsochHead;
    pInterruptHead[i].pTail = (OHCIEndpointDescriptorPtr) pED->pLogicalNext;

    // point Isochronous head to last endpoint
    return (status);
}

////////////////////////////////////////////////////////////////////////////////
//
//		UInt32 OHCIUIMGetLogicalAddress
//		Given the physical address, return the virtual address
//

UInt32 AppleOHCI::OHCIUIMGetLogicalAddress (UInt32 pPhysicalAddress)
{
    OHCIPhysicalLogicalPtr		pPhysicalLogical;
    UInt32				LogicalAddress = nil;

    if (pPhysicalAddress == 0)
        return(0);

    pPhysicalLogical = pOHCIUIMData->pPhysicalLogical;

    while (pPhysicalLogical != nil) {
        if (pPhysicalAddress <= pPhysicalLogical->PhysicalEnd
            && pPhysicalAddress >= pPhysicalLogical->PhysicalStart)
        {
            LogicalAddress = pPhysicalLogical->LogicalStart +
            (pPhysicalAddress - pPhysicalLogical->PhysicalStart);
            pPhysicalLogical = nil;
        } else {
            pPhysicalLogical = (OHCIPhysicalLogicalPtr) pPhysicalLogical->pNext;
        }
    }

    if ( LogicalAddress == nil)
        IOLog("OHCIUIM: LogicalAddress(0x%lx) == nil !\n", pPhysicalAddress);

    return (LogicalAddress);

}


UInt32 AppleOHCI::OHCIUIMGetPhysicalAddress(UInt32	LogicalAddress,
                                            UInt32	count)
{
    OHCIPhysicalLogicalPtr	pPhysicalLogical;
    UInt32			PhysicalAddress = nil;

    if (LogicalAddress == 0)
        return(0);

    pPhysicalLogical = pOHCIUIMData->pPhysicalLogical;

    while (pPhysicalLogical != nil) {
        if (LogicalAddress <= pPhysicalLogical->LogicalEnd
            && LogicalAddress >= pPhysicalLogical->LogicalStart)
        {
            PhysicalAddress = pPhysicalLogical->PhysicalStart
            + (LogicalAddress - pPhysicalLogical->LogicalStart);
            pPhysicalLogical = nil;
        } else {
            pPhysicalLogical = pPhysicalLogical->pNext;
        }
    }

    if (PhysicalAddress == nil)
        PhysicalAddress = OHCIUIMCreatePhysicalAddress(LogicalAddress, count);

    return (PhysicalAddress);
}

/*
 * OHCIUIMCreatePhysicalAddress:
 * Currently this function only creates a one entry OHCIPhysicalLogical
 * entry.  This is because it is assuming contiguous memory.  
 *
 */
UInt32 AppleOHCI::OHCIUIMCreatePhysicalAddress(UInt32 pLogicalAddress,
                                               UInt32 count)

{
    UInt32				pageSize;
    OHCIPhysicalLogicalPtr		pPhysicalLogical;
    OHCIPhysicalLogicalPtr		p;

    pPhysicalLogical = pOHCIUIMData->pPhysicalLogical;
    pageSize = pOHCIUIMData->pageSize;

    // zzz do we deallocate this?
    p = (OHCIPhysicalLogicalPtr) IOMalloc(sizeof (OHCIPhysicalLogical));

    p->LogicalStart =  pLogicalAddress;
    p->PhysicalStart = kvtophys((vm_offset_t)pLogicalAddress);
    p->LogicalEnd = p->LogicalStart + count * pageSize-1;
    p->PhysicalEnd = p->PhysicalStart + count * pageSize-1;
    p->pNext = pOHCIUIMData->pPhysicalLogical;

    pOHCIUIMData->pPhysicalLogical = p;

    return (p->PhysicalStart);
}

//Allocate
IOReturn AppleOHCI::OHCIUIMAllocateMemory (int	num_of_TDs,
                                           int	num_of_EDs,
                                           int	num_of_ITDs)
{
    Ptr                                 p;
    UInt32                              physical;
    int                                 tdsPerPage, pagesTD,
        				edsPerPage, pagesED,
        				itdsPerPage, pagesITD;
    UInt32                              pageSize;
    OHCIEndpointDescriptorPtr           FreeED, FreeEDCurrent;
    OHCIGeneralTransferDescriptorPtr    FreeTD, FreeTDCurrent;
    OHCIIsochTransferDescriptorPtr      FreeITD, FreeITDCurrent;
    int                                 i,j;


    pageSize = pOHCIUIMData->pageSize;

    tdsPerPage = pageSize / sizeof (OHCIGeneralTransferDescriptor);
    pagesTD = (num_of_TDs + (tdsPerPage - 1)) / tdsPerPage;
    edsPerPage = pageSize / sizeof (OHCIEndpointDescriptor);
    pagesED = (num_of_EDs + (edsPerPage - 1)) / edsPerPage;
    itdsPerPage = pageSize / sizeof (OHCIIsochTransferDescriptor);
    pagesITD = (num_of_ITDs + (itdsPerPage - 1)) / itdsPerPage;

    p = (Ptr)IOMalloc( (pagesED + pagesTD + pagesITD + 1) * pageSize);
    
    if (!p)
        return(kIOReturnNoMemory);

    for(i=0; i<(pagesED + pagesTD + pagesITD + 1) * pageSize/4; i++)
	((UInt32 *)p)[i] = 0x12345678;
//    bzero(p, ((pagesED + pagesTD + pagesITD + 1) * pageSize));

    pOHCIUIMData->pDataAllocation = p;
    // page align and 16 byte align (page align automagically
    // makes it 16 byte aligned)
    p = (Ptr) (((UInt32) p + (pageSize - 1)) & ~(pageSize - 1));
    //bzero(p, ((pagesED) * pageSize));

    
    // create a list of unused ED's, filling in Virtual address,
    // physicaladdress and virtual next physical next.
    FreeED = (OHCIEndpointDescriptorPtr) p;
    FreeEDCurrent = FreeED;
    pOHCIUIMData->pFreeED = FreeED;

    for (i = 0 ; i < pagesED ; i++)
    {
        physical = kvtophys(FreeEDCurrent);
        for (j = 0; j < edsPerPage; j++)
        {
            // create EDs
            FreeEDCurrent[j].pPhysical =
            	physical + (j * sizeof (OHCIEndpointDescriptor));
            FreeEDCurrent[j].pLogicalNext = (&FreeEDCurrent[j+1]);
        }
        if (i != (pagesED - 1))
        {
            FreeEDCurrent[j-1].pLogicalNext = ((UInt32) FreeEDCurrent + pageSize);
        }
        else
        {
            FreeEDCurrent[j-1].pLogicalNext = nil;
            pOHCIUIMData->pLastFreeED = &FreeEDCurrent[j-1];
        }

        // goto next page
        FreeEDCurrent = (OHCIEndpointDescriptorPtr)
        			((UInt32) FreeEDCurrent + pageSize);
        //physical += pageSize;
    }

    FreeTD = (OHCIGeneralTransferDescriptorPtr) FreeEDCurrent;
    FreeTDCurrent = FreeTD;
    pOHCIUIMData->pFreeTD = FreeTD;
    for (i = 0; i < pagesTD; i++)
    {
        physical = OHCIUIMGetPhysicalAddress((UInt32) FreeTDCurrent, 1);
        for (j = 0; j < tdsPerPage; j++)
        {
            //create TDs
            FreeTDCurrent[j].pPhysical =
            	physical + (j * sizeof (OHCIGeneralTransferDescriptor));
            FreeTDCurrent[j].pLogicalNext = (UInt32) (&FreeTDCurrent[j+1]);
        }
        if (i != (pagesTD - 1))
        {
            FreeTDCurrent[j-1].pLogicalNext = ((UInt32) FreeTDCurrent + pageSize);
        }
        else
        {
            FreeTDCurrent[j-1].pLogicalNext = nil;
            pOHCIUIMData->pLastFreeTD = &FreeTDCurrent[j-1];
        }

        // goto next page
        FreeTDCurrent = (OHCIGeneralTransferDescriptorPtr)
        	((UInt32) FreeTDCurrent + pageSize);
        //physical += pageSize;
    }

   // set up freeitd queue
    FreeITD = (OHCIIsochTransferDescriptorPtr) FreeTDCurrent;
    FreeITDCurrent = FreeITD;
    pOHCIUIMData->pFreeITD = FreeITD;
    for (i = 0; i < pagesITD; i++)
    {
        //physical = kvtophys(FreeITDCurrent);
        physical = OHCIUIMGetPhysicalAddress((UInt32) FreeITDCurrent, 1);
        for (j = 0; j < itdsPerPage; j++)
        {
            // create TDs
            FreeITDCurrent[j].pPhysical =
            	physical + (j * sizeof (OHCIIsochTransferDescriptor));
            FreeITDCurrent[j].pLogicalNext = (&FreeITDCurrent[j+1]);
        }
        if (i != (pagesITD - 1))
        {
            FreeITDCurrent[j-1].pLogicalNext =
            	(OHCIIsochTransferDescriptorPtr)((UInt32) FreeITDCurrent + pageSize);
        }
        else
        {
            FreeITDCurrent[j-1].pLogicalNext = nil;
            pOHCIUIMData->pLastFreeITD = &FreeITDCurrent[j-1];
        }

        // goto next page
        FreeITDCurrent = (OHCIIsochTransferDescriptorPtr)
        				((UInt32) FreeITDCurrent + pageSize);
        //physical += pageSize;
    }
    
    // create a list of unused buffers?????

    return (kIOReturnSuccess);
}

OHCIIsochTransferDescriptorPtr AppleOHCI::OHCIUIMAllocateITD(void)
{
    OHCIIsochTransferDescriptorPtr temp;

    // pop a TD off of FreeTD list
    // if FreeTD == NIL return nil
    // should we check if ED is full and if not access that????
    temp = pOHCIUIMData->pFreeITD;

    if (temp != nil)
    {
        pOHCIUIMData->pFreeITD = temp->pLogicalNext;
    }
    else
    {
        IOLog("%s: Out of Isoch Transfer Descriptors!\n", getName());
	return nil;
    }

    temp->pLogicalNext = nil;

    return (temp);
}

OHCIGeneralTransferDescriptorPtr AppleOHCI::OHCIUIMAllocateTD(void)

{
    OHCIGeneralTransferDescriptorPtr freeTD;

    // pop a TD off of FreeTD list
    //if FreeTD == NIL return nil
    // should we check if ED is full and if not access that????
    freeTD = pOHCIUIMData->pFreeTD;

    if (freeTD == nil)
    {
        int j;
        int tdsPerPage;
        UInt32 physical;
        OHCIGeneralTransferDescriptorPtr FreeTDCurrent;
        UInt8 * p = (UInt8 *)IOMallocAligned(pOHCIUIMData->pageSize,
                                        pOHCIUIMData->pageSize);
        if(p == NULL) {
            IOLog("%s: Out of Transfer Descriptors!\n", getName());
            return NULL;
        }

        //bzero(p, pOHCIUIMData->pageSize);

        tdsPerPage = pOHCIUIMData->pageSize / sizeof (OHCIGeneralTransferDescriptor);
        pOHCIUIMData->pFreeTD = FreeTDCurrent = (OHCIGeneralTransferDescriptorPtr)p;
        physical = OHCIUIMGetPhysicalAddress((UInt32) FreeTDCurrent, 1);
        for (j = 0; j < tdsPerPage-1; j++)
        {
            //create TDs
            FreeTDCurrent[j].pPhysical =
                physical + (j * sizeof (OHCIGeneralTransferDescriptor));
            FreeTDCurrent[j].pLogicalNext = &FreeTDCurrent[j+1];
        }
        FreeTDCurrent[j].pPhysical =
            physical + (j * sizeof (OHCIGeneralTransferDescriptor));
        FreeTDCurrent[j].pLogicalNext = nil;
        pOHCIUIMData->pLastFreeTD = &FreeTDCurrent[j];
        freeTD = pOHCIUIMData->pFreeTD;
    }
    pOHCIUIMData->pFreeTD = freeTD->pLogicalNext;
    freeTD->pLogicalNext = nil;
    return (freeTD);
}

OHCIEndpointDescriptorPtr AppleOHCI::OHCIUIMAllocateED(void)
{
    OHCIEndpointDescriptorPtr freeED;

    // Pop a ED off the FreeED list
    // If FreeED == nil return Error
    freeED = pOHCIUIMData->pFreeED;

    if (freeED == nil)
    {
	int j;
	int edsPerPage;
        UInt32 physical;
        OHCIEndpointDescriptorPtr FreeEDCurrent;
        UInt8 * p = (UInt8 *)IOMallocAligned(pOHCIUIMData->pageSize,
					pOHCIUIMData->pageSize);
        if(p == NULL)
        {
            IOLog("%s: Out of Endpoint Descriptors!\n", getName());
            return NULL;
        }

	//bzero(p, pOHCIUIMData->pageSize);

        edsPerPage = pOHCIUIMData->pageSize / sizeof (OHCIEndpointDescriptor);
        physical = kvtophys((vm_offset_t)p);
        pOHCIUIMData->pFreeED = FreeEDCurrent = (OHCIEndpointDescriptorPtr)p;
        for (j = 0; j < edsPerPage-1; j++)
        {
            // create EDs
            FreeEDCurrent[j].pPhysical =
                physical + (j * sizeof (OHCIEndpointDescriptor));
            FreeEDCurrent[j].pLogicalNext = (&FreeEDCurrent[j+1]);
        }
        FreeEDCurrent[j].pPhysical =
            physical + (j * sizeof (OHCIEndpointDescriptor));
        FreeEDCurrent[j].pLogicalNext = nil;
        pOHCIUIMData->pLastFreeED = &FreeEDCurrent[j];
        freeED = pOHCIUIMData->pFreeED;
    }
    pOHCIUIMData->pFreeED = (OHCIEndpointDescriptorPtr) freeED->pLogicalNext;
    freeED->pLogicalNext = nil;
    return (freeED);
}

IOReturn AppleOHCI::OHCIUIMDeallocateITD (OHCIIsochTransferDescriptorPtr pTD)
{
    UInt32		physical;

    // zero out all unnecessary fields
    physical = pTD->pPhysical;
    //bzero(pTD, sizeof(*pTD));
    pTD->pLogicalNext = NULL;
    pTD->pPhysical = physical;
    if (pOHCIUIMData->pFreeITD)
    {
        pOHCIUIMData->pLastFreeITD->pLogicalNext = pTD;
        pOHCIUIMData->pLastFreeITD = pTD;
    }
    else
    {
        // list is currently empty
        pOHCIUIMData->pLastFreeITD = pTD;
        pOHCIUIMData->pFreeITD = pTD;
    }
    return (kIOReturnSuccess);
}


IOReturn AppleOHCI::OHCIUIMDeallocateTD (OHCIGeneralTransferDescriptorPtr pTD)
{
    UInt32		physical;

    //zero out all unnecessary fields
    physical = pTD->pPhysical;
    //bzero(pTD, sizeof(*pTD));
    pTD->pLogicalNext = nil;
    pTD->pPhysical = physical;

    if (pOHCIUIMData->pFreeTD){
        pOHCIUIMData->pLastFreeTD->pLogicalNext = pTD;
        pOHCIUIMData->pLastFreeTD = pTD;
    } else {
        // list is currently empty
        pOHCIUIMData->pLastFreeTD = pTD;
        pOHCIUIMData->pFreeTD = pTD;
    }
    return (kIOReturnSuccess);
}

IOReturn AppleOHCI::OHCIUIMDeallocateED (OHCIEndpointDescriptorPtr pED)
{
    UInt32		physical;

    //zero out all unnecessary fields
    physical = pED->pPhysical;
    //bzero(pED, sizeof(*pED));
    pED->pPhysical = physical;
    pED->pLogicalNext = nil;

    if (pOHCIUIMData->pFreeED){
        pOHCIUIMData->pLastFreeED->pLogicalNext = pED;
        pOHCIUIMData->pLastFreeED = pED;
    } else {
        // list is currently empty
        pOHCIUIMData->pLastFreeED = pED;
        pOHCIUIMData->pFreeED = pED;
    }
    return (kIOReturnSuccess);
}

int GetEDType(OHCIEndpointDescriptorPtr pED)
{
    return ((OSSwapInt32(pED->flags) & kOHCIEDControl_F)
                                                    >> kOHCIEDControl_FPhase);
}


IOReturn AppleOHCI::RemoveAllTDs (OHCIEndpointDescriptorPtr pED)
{
    RemoveTDs(pED);

    if (GetEDType(pED) == kOHCIEDFormatGeneralTD) {
        // remove the last "dummy" TD
        OHCIUIMDeallocateTD(
                            (OHCIGeneralTransferDescriptorPtr) pED->pLogicalTailP);
    }
    else
    {
        OHCIUIMDeallocateITD(
                             (OHCIIsochTransferDescriptorPtr) pED->pLogicalTailP);
    }
    pED->pLogicalTailP = nil;

    return (0);
}


//removes all but the last of the TDs
IOReturn AppleOHCI::RemoveTDs(OHCIEndpointDescriptorPtr pED)
{
    OHCIGeneralTransferDescriptorPtr	pCurrentTD, lastTD;
    UInt32				bufferSizeRemaining = 0;
    OHCIIsochTransferDescriptorPtr	pITD, pITDLast;
	
    if (GetEDType(pED) == kOHCIEDFormatGeneralTD)
    {
        //process and deallocate GTD's
        pCurrentTD = (OHCIGeneralTransferDescriptorPtr)
		(OSSwapInt32(pED->tdQueueHeadPtr) & kOHCIHeadPMask);
        pCurrentTD = (OHCIGeneralTransferDescriptorPtr)
                            OHCIUIMGetLogicalAddress ((UInt32) pCurrentTD);

        lastTD = (OHCIGeneralTransferDescriptorPtr) pED->pLogicalTailP;
        pED->pLogicalHeadP = pED->pLogicalTailP;

        while (pCurrentTD != lastTD)
        {
            if (pCurrentTD == nil)
                return (-1);

            //take out TD from list
            pED->tdQueueHeadPtr = pCurrentTD->nextTD;
            pED->pLogicalHeadP = pCurrentTD->pLogicalNext;	

            bufferSizeRemaining += findBufferRemaining(pCurrentTD);

            if (pCurrentTD->completion.action != nil)
            {
                IOUSBCompletion completion;
                // zero out callback first then call it
                completion = pCurrentTD->completion;
                pCurrentTD->completion.action = nil;
                complete(completion,
                         kIOReturnAborted,
                         bufferSizeRemaining);
                bufferSizeRemaining = 0;
            }

            OHCIUIMDeallocateTD(pCurrentTD);
            pCurrentTD = (OHCIGeneralTransferDescriptorPtr) pED->pLogicalHeadP;		
        }		
    }
    else
    {
        UInt32 phys;
        phys = (OSReadLittleInt32(&pED->tdQueueHeadPtr, 0) & kOHCIHeadPMask);
        pITD = (OHCIIsochTransferDescriptorPtr)
                                OHCIUIMGetLogicalAddress (phys);
        pITDLast = (OHCIIsochTransferDescriptorPtr)pED->pLogicalTailP;

        while (pITD != pITDLast)
        {
            OHCIIsochTransferDescriptorPtr pPrevITD;
            if (pITD == nil)
                return (-1);
            ProcessCompletedITD (pITD, kIOReturnAborted);
            pPrevITD = pITD;
            pITD = pITD->pLogicalNext;
            // deallocate td
            OHCIUIMDeallocateITD(pPrevITD);
        }
    }

    return (0);
}

void ProcessCompletedITD (OHCIIsochTransferDescriptorPtr pITD, IOReturn status)
{

    IOUSBIsocFrame *	pFrames;
    int			i;
    int			frameCount;

    //print_itd(pITD);	
    pFrames = pITD->pIsocFrame;
    frameCount = (OSReadLittleInt32(&pITD->flags, 0) & kOHCIITDControl_FC) >> 
							kOHCIITDControl_FCPhase;
    for (i=0; i <= frameCount; i++)
    {
        UInt16 offset = OSReadLittleInt16(&pITD->offset[i], 0);
        if ( ((offset & kOHCIITDPSW_CCNA) >> kOHCIITDPSW_CCNAPhase) ==
             				kOHCIITDOffsetConditionNotAccessed)
        {
            pFrames[pITD->frameNum + i].frActCount = 0;
            pFrames[pITD->frameNum + i].frStatus =
                kOHCIITDConditionNotAccessedReturn;
        }
        else
        {
            pFrames[pITD->frameNum + i].frStatus =
            (offset & kOHCIITDPSW_CC) >> kOHCIITDOffset_CCPhase;
            // Successful isoch transmit sets the size field to zero,
            // successful receive sets size to actual packet size received.
            if(kIOReturnSuccess == pFrames[pITD->frameNum + i].frStatus &&
					pITD->pType == kOHCIIsochronousOutType)
                pFrames[pITD->frameNum + i].frActCount =
                    pFrames[pITD->frameNum + i].frReqCount;
            else
                pFrames[pITD->frameNum + i].frActCount =
                        offset & kOHCIITDPSW_Size;
        }
    }
    // call callback
    if (pITD->completion.action)
    {
        IOUSBIsocCompletionAction pHandler;
        pHandler = pITD->completion.action;
        pITD->completion.action = nil;
        //zero out handler first than call it
        (*pHandler) (pITD->completion.target,  pITD->completion.parameter,
							status, pFrames);
    }
}


IOReturn
AppleOHCI::DoDoneQueueProcessing(OHCIGeneralTransferDescriptorPtr pHCDoneTD,
                                 IOUSBCompletionAction safeAction)
{
    UInt32				control, transferStatus;
    long				bufferSizeRemaining;
    OHCIGeneralTransferDescriptorPtr	prevTD, nextTD;
    UInt32				PhysAddr;
    UInt32				pageSize;
    UInt32				pageMask;
    OHCIEndpointDescriptorPtr		tempED;
    OHCIRegistersPtr			pOHCIRegisters;
    OHCIIsochTransferDescriptorPtr	pITD;

    pOHCIRegisters = pOHCIUIMData->pOHCIRegisters;

    if(pHCDoneTD == nil)
        /* This should not happen */
        return(kIOReturnSuccess);

    pageSize = pOHCIUIMData->pageSize;
    pageMask = ~(pageSize - 1);

    /* Reverse the done queue use only the virtual Address fields */
    prevTD = 0;
    while (pHCDoneTD != nil)
    {
        PhysAddr = OSSwapInt32(pHCDoneTD->nextTD) & kOHCIHeadPMask;
        nextTD = (OHCIGeneralTransferDescriptorPtr) OHCIUIMGetLogicalAddress(PhysAddr);
        pHCDoneTD->pLogicalNext = prevTD;
        prevTD = pHCDoneTD;
        pHCDoneTD = nextTD;
    }
    pHCDoneTD = prevTD;	/* New qHead */

    while (pHCDoneTD != nil)
    {
#if (DEBUGGING_LEVEL > 2)
        DEBUGLOG("\tdone queue: processing..."); print_td(pHCDoneTD);
#endif
        IOReturn errStatus;
        // find the next one
        nextTD	= pHCDoneTD->pLogicalNext;

        control = OSSwapInt32(pHCDoneTD->flags);
        transferStatus = (control & kOHCIGTDControl_CC)
            >> kOHCIGTDControl_CCPhase;
        errStatus = TranslateStatusToUSBError(transferStatus);
        if (pOHCIUIMData->OptiOn && (pHCDoneTD->pType == kOHCIOptiLSBug))
        {
            // clear any bad errors
            tempED = (OHCIEndpointDescriptorPtr) pHCDoneTD->pEndpoint;
            pHCDoneTD->flags = pHCDoneTD->flags
                & OSSwapInt32(kOHCIGTDClearErrorMask);
            tempED->tdQueueHeadPtr &=  OSSwapInt32(kOHCIHeadPMask);
            pHCDoneTD->nextTD = tempED->tdQueueTailPtr & OSSwapInt32(kOHCIHeadPMask);
            tempED->tdQueueTailPtr = OSSwapInt32(pHCDoneTD->pPhysical);
            pOHCIRegisters->hcCommandStatus =
                OSSwapInt32 (kOHCIHcCommandStatus_CLF);

            // For CMD Buffer Underrun Errata
        }
        else if ((transferStatus == kOHCIGTDConditionBufferUnderrun) &&
                 (pHCDoneTD->pType == kOHCIBulkTransferOutType) &&
                 (pOHCIUIMData->errataBits & kErrataRetryBufferUnderruns))
        {
            tempED = (OHCIEndpointDescriptorPtr) pHCDoneTD->pEndpoint;
            pHCDoneTD->flags = pHCDoneTD->flags
                & OSSwapInt32(kOHCIGTDClearErrorMask);
            pHCDoneTD->nextTD = tempED->tdQueueHeadPtr & OSSwapInt32(kOHCIHeadPMask);
            pHCDoneTD->pLogicalNext = (OHCIGeneralTransferDescriptorPtr)
		OHCIUIMGetLogicalAddress (OSSwapInt32(tempED->tdQueueHeadPtr)
                                                & kOHCIHeadPMask);

            tempED->tdQueueHeadPtr = OSSwapInt32(pHCDoneTD->pPhysical)
                | (tempED->tdQueueHeadPtr & OSSwapInt32( kOHCIEDToggleBitMask));
            pOHCIRegisters->hcCommandStatus =
                OSSwapInt32(kOHCIHcCommandStatus_BLF);

        }
        else if (pHCDoneTD->pType == kOHCIIsochronousInType ||
				pHCDoneTD->pType == kOHCIIsochronousOutType)
        {
            // cast to a isoc type
            pITD = (OHCIIsochTransferDescriptorPtr) pHCDoneTD;
            ProcessCompletedITD(pITD, errStatus);
            // deallocate td
            OHCIUIMDeallocateITD(pITD);
        }
        else
        {
            bufferSizeRemaining = findBufferRemaining (pHCDoneTD);
            if (pHCDoneTD->completion.action != nil)
            {
                IOUSBCompletion completion;
                completion = pHCDoneTD->completion;
                if(!safeAction || (safeAction == completion.action)) {
                    // zero out callback first then call it
                    pHCDoneTD->completion.action = nil;
                    complete(completion, errStatus, bufferSizeRemaining);
                    OHCIUIMDeallocateTD(pHCDoneTD);
                }
                else {
                    if(_pendingHead)
                        _pendingTail->pLogicalNext = pHCDoneTD;
                    else
                        _pendingHead = pHCDoneTD;
                    _pendingTail = pHCDoneTD;
                }
            }
            else if (errStatus != kIOReturnSuccess) {
                doCallback(pHCDoneTD, errStatus, bufferSizeRemaining);
                OHCIUIMDeallocateTD(pHCDoneTD);
            }
        }
        pHCDoneTD = nextTD;	/* New qHead */
    }

    return(kIOReturnSuccess);
}




void AppleOHCI::UIMProcessDoneQueue(IOUSBCompletionAction safeAction)
{
    OHCIRegistersPtr		pOHCIRegisters = pOHCIUIMData->pOHCIRegisters;
    UInt32				interruptStatus;
    UInt32				PhysAddr;
    OHCIGeneralTransferDescriptorPtr 	pHCDoneTD;


    // check if the OHCI has written the DoneHead yet
    interruptStatus = OSSwapInt32(pOHCIRegisters->hcInterruptStatus);
    if( (interruptStatus & kOHCIHcInterrupt_WDH) == 0)
    {	
        return;
    }

    // get the pointer to the list (logical address)
    PhysAddr = (UInt32) OSSwapInt32(*(UInt32 *)(pOHCIUIMData->pHCCA + 0x84));
    PhysAddr &= kOHCIHeadPMask; // mask off interrupt bits
    pHCDoneTD = (OHCIGeneralTransferDescriptorPtr) OHCIUIMGetLogicalAddress(PhysAddr);
    // write to 0 to the HCCA DoneHead ptr so we won't look at it anymore.
    *(UInt32 *)(pOHCIUIMData->pHCCA + 0x84) = 0L;

    // Since we have a copy of the queue to process, we can let the host update it again.
    pOHCIRegisters->hcInterruptStatus = OSSwapInt32(kOHCIHcInterrupt_WDH);

    DoDoneQueueProcessing(pHCDoneTD, safeAction);
    return;
}

void AppleOHCI::finishPending()
{
    while(_pendingHead) {
        OHCIGeneralTransferDescriptorPtr next = _pendingHead->pLogicalNext;
        long bufferSizeRemaining = findBufferRemaining (_pendingHead);
        UInt32 transferStatus = (OSReadLittleInt32(&_pendingHead->flags, 0) &
                kOHCIGTDControl_CC) >> kOHCIGTDControl_CCPhase;

        IOUSBCompletion completion;
        // zero out callback first then call it
        completion = _pendingHead->completion;
        _pendingHead->completion.action = nil;
        complete(completion,
                TranslateStatusToUSBError(transferStatus),
                bufferSizeRemaining);
        OHCIUIMDeallocateTD(_pendingHead);
        _pendingHead = next;
    }
}

UInt32 AppleOHCI::GetBandwidthAvailable()
{
    return pOHCIUIMData->isochBandwidthAvail;
}

UInt64 AppleOHCI::GetFrameNumber()
{

    UInt64	bigFrameNumber;
    UInt16	framenumber16;

    framenumber16 = OSReadLittleInt16(pOHCIUIMData->pHCCA, 0x80);
    bigFrameNumber = pOHCIUIMData->frameNumber + framenumber16;
    return bigFrameNumber;
}

UInt32 AppleOHCI::GetFrameNumber32()
{
    UInt16	framenumber16;
    UInt32	largishFrameNumber;
    framenumber16 = OSReadLittleInt16(pOHCIUIMData->pHCCA, 0x80);
    largishFrameNumber = ((UInt32)pOHCIUIMData->frameNumber) + framenumber16;
    return largishFrameNumber;
}


// #ifdef DEBUG
void AppleOHCI::dumpRegs(void)
{
    UInt32		lvalue;
    
#if 0
    /*device->*/configRead16(cwVendorID, &ivalue);
    /*device->*/configRead16(cwDeviceID, &ivalue);
    IOLog("OHCI: cwVendorID=%x  cwDeviceID=%x\n", ivalue, ivalue2);
#else
    lvalue = _device->configRead32(cwVendorID);
    IOLog("OHCI: cwVendorID=%lx\n", lvalue);
#endif
    lvalue = _device->configRead32(clClassCodeAndRevID);
    IOLog("OHCI: clClassCodeAndRevID=%lx\n", lvalue);
    lvalue = _device->configRead32(clHeaderAndLatency);
    IOLog("OHCI: clHeaderAndLatency=%lx\n", lvalue);
    lvalue = _device->configRead32(clBaseAddressZero);
    IOLog("OHCI: clBaseAddressZero=%lx\n", lvalue);
    lvalue = _device->configRead32(clBaseAddressOne);
    IOLog("OHCI: clBaseAddressOne=%lx\n", lvalue);
    lvalue = _device->configRead32(clExpansionRomAddr);
    IOLog("OHCI: clExpansionRomAddr=%lx\n", lvalue);
    lvalue = _device->configRead32(clLatGntIntPinLine);
    IOLog("OHCI: clLatGntIntPinLine=%lx\n", lvalue);
    lvalue = _device->configRead32(clLatGntIntPinLine+4);
    IOLog("OHCI: clLatGntIntPinLine+4=%lx\n", lvalue);
    lvalue = _device->configRead32(cwCommand);
    IOLog("OHCI: cwCommand=%lx\n", lvalue & 0x0000ffff);
    IOLog("OHCI: cwStatus=%lx\n", lvalue & 0xffff0000);

    lvalue = _device->configRead32(cwCommand);
    _device->configWrite32(cwCommand, lvalue);
    _device->configWrite32(cwCommand, (lvalue & 0xffff0000) |
                          (cwCommandEnableBusMaster |
                           cwCommandEnableMemorySpace));
    lvalue = _device->configRead32(cwCommand);
    IOLog("OHCI: cwCommand=%lx\n", lvalue & 0x0000ffff);
    IOLog("OHCI: cwStatus=%lx\n", lvalue & 0xffff0000);

    IOLog("OHCI: HcRevision=%lx\n",
          OSSwapInt32((pOHCIUIMData->pOHCIRegisters)->hcRevision));
    IOLog("      HcControl=%lx\n",
          OSSwapInt32((pOHCIUIMData->pOHCIRegisters)->hcControl));
    IOLog("      HcFmInterval=%lx\n",
          OSSwapInt32((pOHCIUIMData->pOHCIRegisters)->hcFmInterval));
    IOLog("      hcRhDescriptorA=%lx\n",
          OSSwapInt32((pOHCIUIMData->pOHCIRegisters)->hcRhDescriptorA));
    IOLog("      hcRhDescriptorB=%lx\n",
          OSSwapInt32((pOHCIUIMData->pOHCIRegisters)->hcRhDescriptorB));
}
// #endif /* DEBUG */

IOReturn TranslateStatusToUSBError(UInt32 status)
{
    static const UInt32 statusToErrorMap[] = {
        /* OHCI Error */     /* USB Error */
        /*  0 */		kIOReturnSuccess,
        /*  1 */		kIOUSBPipeStalled, //kUSBCRCErr,
        /*  2 */ 		kIOUSBPipeStalled, //kUSBBitstufErr,
        /*  3 */ 		kIOUSBPipeStalled, //kUSBDataToggleErr,
        /*  4 */ 		kIOUSBPipeStalled, //kUSBEndpointStallErr,
        /*  5 */ 		kIOReturnNotResponding, //kUSBNotRespondingErr,
        /*  6 */ 		kIOUSBPipeStalled, //kUSBPIDCheckErr,
        /*  7 */ 		kIOUSBPipeStalled, //kUSBWrongPIDErr,
        /*  8 */ 		kIOReturnOverrun,
        /*  9 */ 		kIOReturnUnderrun,
        /* 10 */ 		kIOReturnError, //kUSBRes1Err,
        /* 11 */ 		kIOReturnError, //kUSBRes2Err,
        /* 12 */ 		kIOReturnDMAError, //kUSBBufOvrRunErr,
        /* 13 */ 		kIOReturnDMAError, //kUSBBufUnderRunErr
    };

    if (status > 13) return(kIOReturnInternalError);
    return(statusToErrorMap[status]);
}
