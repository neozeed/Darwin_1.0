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

#include "AppleOHCI.h"
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IOMemoryCursor.h>

#define DEBUGGING_LEVEL 0	// 1 = low; 2 = high; 3 = extreme
#define DEBUGLOG IOLog
#define nil (0)
#define kP_UIMName ""

#if DEBUGGING_LEVEL > 0
#define USBExpertStatusLevel(a, b, c, d) DEBUGLOG("UIM: %s 0x%lx\n", c, (UInt32)(d))
#else
#define USBExpertStatusLevel(a, b, c, d)
#endif

#define super IOUSBController

void print_td(OHCIGeneralTransferDescriptorPtr x);
void print_itd(OHCIIsochTransferDescriptorPtr x);
void print_ed(OHCIEndpointDescriptorPtr x);
void print_isoc_ed(OHCIEndpointDescriptorPtr x);
void print_list(OHCIEndpointDescriptorPtr pListHead,
                OHCIEndpointDescriptorPtr pListTail);
void print_control_list(OHCIUIMDataPtr pData);
void print_bulk_list(OHCIUIMDataPtr pData);
void print_int_list(OHCIUIMDataPtr pData);

static inline OHCIEDFormat
GetEDType(OHCIEndpointDescriptorPtr pED)
{
    return ((OSReadLittleInt32(&pED->flags, 0) & kOHCIEDControl_F) >>
						kOHCIEDControl_FPhase);
}

IOReturn AppleOHCI::UIMCreateGeneralTransfer(
            OHCIEndpointDescriptorPtr		queue,
            IOUSBCompletion			completion,
            IOMemoryDescriptor *		CBP,
            UInt32				bufferSize,
            UInt32				flags,
            UInt32				type,
            UInt32				kickBits)
{
    OHCIRegistersPtr			pOHCIRegisters;
    OHCIGeneralTransferDescriptorPtr	pOHCIGeneralTransferDescriptor,
                                        newOHCIGeneralTransferDescriptor;
    IOReturn				status = kIOReturnSuccess;
    IOPhysicalSegment      			physicalAddresses[2];	
    IOByteCount				transferOffset;
    UInt32				pageSize;
    UInt32				pageCount;

    pOHCIRegisters = pOHCIUIMData->pOHCIRegisters;
    pageSize = pOHCIUIMData->pageSize;

    // Handy for debugging transfer lists
    flags |= (kOHCIGTDConditionNotAccessed << kOHCIGTDControl_CCPhase);

// FERG DEBUG
// uncomment the next line to force the data to be put in TD list, but not be processed
// this is handy for using USBProber/Macsbug to look at TD's to see if they're OK.
// pEDQueue->dWord0 |= OSSwapInt32 (kOHCIEDControl_K);
// FERG DEBUG
    if (bufferSize != 0)
    {
        transferOffset = 0;
        while (transferOffset < bufferSize)
        {
            if(pOHCIUIMData->errataBits & kErrataOnlySinglePageTransfers)
                pageCount = _genCursor->getPhysicalSegments(CBP, transferOffset, physicalAddresses, 1);
            else
                pageCount = _genCursor->getPhysicalSegments(CBP, transferOffset, physicalAddresses, 2);
            newOHCIGeneralTransferDescriptor = OHCIUIMAllocateTD();
            if (newOHCIGeneralTransferDescriptor == nil) {
                status = kIOReturnNoMemory;
                break;
            }
            pOHCIGeneralTransferDescriptor = 
		(OHCIGeneralTransferDescriptorPtr)queue->pLogicalTailP;
            OSWriteLittleInt32(&pOHCIGeneralTransferDescriptor->flags, 0, flags);
            OSWriteLittleInt32(&pOHCIGeneralTransferDescriptor->currentBufferPtr, 0,
                physicalAddresses[0].location);
            OSWriteLittleInt32(&pOHCIGeneralTransferDescriptor->nextTD, 0,
		newOHCIGeneralTransferDescriptor->pPhysical);
            if (pageCount == 2) {
                OSWriteLittleInt32(&pOHCIGeneralTransferDescriptor->bufferEnd, 0,
                        physicalAddresses[1].location + physicalAddresses[1].length - 1);
                transferOffset += physicalAddresses[1].length;
            }
            else
                OSWriteLittleInt32(&pOHCIGeneralTransferDescriptor->bufferEnd, 0,
                        physicalAddresses[0].location + physicalAddresses[0].length - 1);

            pOHCIGeneralTransferDescriptor->pLogicalNext =
                newOHCIGeneralTransferDescriptor;
            pOHCIGeneralTransferDescriptor->pEndpoint = queue;
            pOHCIGeneralTransferDescriptor->pType = type;
            transferOffset += physicalAddresses[0].length;

            // only supply a callback when the entire buffer has been
            // transfered.
            if (transferOffset >= bufferSize)
                pOHCIGeneralTransferDescriptor->completion = completion;
            else
                pOHCIGeneralTransferDescriptor->completion.action = nil;
            queue->tdQueueTailPtr = pOHCIGeneralTransferDescriptor->nextTD;
            queue->pLogicalTailP =
                newOHCIGeneralTransferDescriptor;
            OSWriteLittleInt32(&pOHCIRegisters->hcCommandStatus, 0, kickBits);
        }
    }
    else
    {
        newOHCIGeneralTransferDescriptor = OHCIUIMAllocateTD();
        // last in queue is dummy descriptor. Fill it in then add new dummy
        pOHCIGeneralTransferDescriptor =
            (OHCIGeneralTransferDescriptorPtr) queue->pLogicalTailP;

        OSWriteLittleInt32(&pOHCIGeneralTransferDescriptor->flags, 0, flags);
        OSWriteLittleInt32(&pOHCIGeneralTransferDescriptor->nextTD, 0,
            newOHCIGeneralTransferDescriptor->pPhysical);
        pOHCIGeneralTransferDescriptor->pLogicalNext =
            newOHCIGeneralTransferDescriptor;
        pOHCIGeneralTransferDescriptor->pEndpoint = queue;
        pOHCIGeneralTransferDescriptor->pType = type;

        /* for zero sized buffers */
        pOHCIGeneralTransferDescriptor->currentBufferPtr = 0;
        pOHCIGeneralTransferDescriptor->bufferEnd = 0;
        pOHCIGeneralTransferDescriptor->completion = completion;

        /* Make new descriptor the tail */
        queue->tdQueueTailPtr = pOHCIGeneralTransferDescriptor->nextTD;
        queue->pLogicalTailP = newOHCIGeneralTransferDescriptor;
        OSWriteLittleInt32(&pOHCIRegisters->hcCommandStatus, 0, kickBits);
    }

#if (DEBUGGING_LEVEL > 0)
    print_td(pOHCIGeneralTransferDescriptor);
    DEBUGLOG("UIMCreateGeneralTransfer: returning status=%d\n", status);
#endif
    return (status);
}

IOReturn AppleOHCI::UIMCreateControlEndpoint(
            UInt8				functionAddress,
            UInt8				endpointNumber,
            UInt16				maxPacketSize,
            UInt8				speed)
{
    OHCIEndpointDescriptorPtr	pOHCIEndpointDescriptor, pED;

#if (DEBUGGING_LEVEL > 0)
    DEBUGLOG("%s: UIMCreateControlEndpoint(%d, %d, %d, %d)\n", getName(),
          functionAddress, endpointNumber, maxPacketSize, speed);
#endif
    if (pOHCIUIMData->rootHubFuncAddress == functionAddress)
    {
        return(kIOReturnSuccess);
    }
        
    pED = (OHCIEndpointDescriptorPtr) pOHCIUIMData->pControlHead;
    if ((speed == kOHCIEDSpeedFull) && pOHCIUIMData->OptiOn)
        pED = (OHCIEndpointDescriptorPtr) pOHCIUIMData->pBulkHead;

    pOHCIEndpointDescriptor = AddEmptyEndPoint(functionAddress,
                                               endpointNumber,
                                               maxPacketSize,
                                               speed,
                                               kOHCIEDDirectionTD,
                                               pED,
                                               kOHCIEDFormatGeneralTD);

#if (DEBUGGING_LEVEL > 2)
    if ((speed == kOHCIEDSpeedFull) && pOHCIUIMData->OptiOn)
        print_bulk_list(pOHCIUIMData);
    else
	print_control_list(pOHCIUIMData);
#endif

    if (pOHCIEndpointDescriptor == nil)
        return(-1);  //FIXME
    return (kIOReturnSuccess);
}


IOReturn AppleOHCI::UIMCreateControlTransfer(
            short				functionAddress,
            short				endpointNumber,
            IOUSBCompletion			completion,
            IOMemoryDescriptor *		CBP,
            bool				bufferRounding,
            UInt32				bufferSize,
            short				direction)
{
    UInt32				myBufferRounding;
    UInt32				myDirection;
    UInt32				myToggle;
    OHCIEndpointDescriptorPtr		pEDQueue, pEDDummy;
    IOReturn				status;

#if (DEBUGGING_LEVEL > 0)
    DEBUGLOG("\tCrntlTx: adr=%d:%d cbp=%lx:%lx br=%s cback=[%lx:%lx] dir=%d)\n",
          functionAddress, endpointNumber, CBP, bufferSize,
          bufferRounding?"YES":"NO",
             (UInt32)completion.target, (UInt32)completion.parameter, direction);
#endif

    if (direction == kUSBOut)
    {
        direction = kOHCIGTDPIDOut;
    }
    else if (direction == kUSBIn)
    {
        direction = kOHCIGTDPIDIn;
    }
    else
    {
        direction = kOHCIGTDPIDSetup;
    }
    // search for endpoint descriptor

    pEDQueue = FindControlEndpoint(functionAddress, endpointNumber, &pEDDummy);
    if (pEDQueue == nil)
    {
        IOLog("UIMCreateControlTransfer: Could not find endpoint\n");
        return(kIOUSBEndpointNotFound);
    }
    myBufferRounding = (UInt32) bufferRounding << kOHCIBufferRoundingOffset;
    myDirection = (UInt32) direction << kOHCIDirectionOffset;
    myToggle = kOHCIBit25;	/* Take data toggle from TD */
    if (direction != 0)
    {
        /* Setup uses Data 0, data status use Data1 */
        myToggle |= kOHCIBit24;	/* use Data1 */
    }

    status = UIMCreateGeneralTransfer(pEDQueue, completion, CBP, bufferSize,
	myBufferRounding | myDirection | myToggle, kOHCIControlSetupType, kOHCIHcCommandStatus_CLF);

#if (DEBUGGING_LEVEL > 2)
    print_ed(pEDQueue);
#endif

    return (status);
}

IOReturn AppleOHCI::UIMCreateControlTransfer(
            short				functionAddress,
            short				endpointNumber,
            IOUSBCompletion			completion,
            void *				CBP,
            bool				bufferRounding,
            UInt32				bufferSize,
            short				direction)
{
    IOMemoryDescriptor *		desc = NULL;
    IODirection				descDirection;
    IOReturn				status;

#if (DEBUGGING_LEVEL > 0)
    DEBUGLOG("\tCrntlTx: adr=%d:%d cbp=%lx:%lx br=%s cback=[%lx:%lx] dir=%d)\n",
          functionAddress, endpointNumber, CBP, bufferSize,
          bufferRounding?"YES":"NO",
             (UInt32)completion.target, (UInt32)completion.parameter, direction);
#endif
    if (direction == kUSBOut)
    {
        descDirection = kIODirectionOut;
    }
    else if (direction == kUSBIn)
    {
        descDirection = kIODirectionIn;
    }
    else
    {
        descDirection = kIODirectionOut;
    }
    if(bufferSize != 0) {
        desc = IOMemoryDescriptor::withAddress(CBP, bufferSize, descDirection);
        if(!desc)
            return(kIOReturnNoMemory);
    }

    status = UIMCreateControlTransfer(functionAddress, endpointNumber, completion,
		desc, bufferRounding, bufferSize, direction);

    if(desc)
        desc->release();

    return (status);
}

/* Not implemented - use UIMAbortEndpoint
IOReturn AppleOHCI::UIMAbortControlEndpoint(void);
IOReturn AppleOHCI::UIMEnableControlEndpoint(void);
IOReturn AppleOHCI::UIMDisableControlEndpoint(void);
*/

// Bulk
IOReturn AppleOHCI::UIMCreateBulkEndpoint(
            UInt8				functionAddress,
            UInt8				endpointNumber,
            UInt8				direction,
            UInt8				speed,
            UInt8				maxPacketSize)
{
    OHCIEndpointDescriptorPtr	pOHCIEndpointDescriptor, pED;


#if (DEBUGGING_LEVEL > 0)
    DEBUGLOG("%s: UIMCreateBulkEndpoint(adr=%d:%d, max=%d, dir=%d)\n", getName(),
          functionAddress, endpointNumber, maxPacketSize, direction);
#endif
    
    if (direction == kUSBOut)
            direction = kOHCIEDDirectionOut;
    else if (direction == kUSBIn)
            direction = kOHCIEDDirectionIn;
    else
            direction = kOHCIEDDirectionTD;

    pED = (OHCIEndpointDescriptorPtr) pOHCIUIMData->pBulkHead;
    pOHCIEndpointDescriptor = AddEmptyEndPoint (functionAddress,
                                                endpointNumber,
                                                maxPacketSize,
                                                speed,
                                                direction,
                                                pED,
                                                kOHCIEDFormatGeneralTD);
    if (pOHCIEndpointDescriptor == nil)
        return(kIOReturnNoMemory);

    return (kIOReturnSuccess);
}

IOReturn AppleOHCI::UIMCreateBulkTransfer(
            short				functionAddress,
            short				endpointNumber,
            IOUSBCompletion			completion,
            IOMemoryDescriptor *		CBP,
            bool				bufferRounding,
            UInt32				bufferSize,
            short				direction)
{
    IOReturn				status = kIOReturnSuccess;
    UInt32				myBufferRounding;
    UInt32				TDDirection;
    UInt32				kickBits;
    OHCIEndpointDescriptorPtr		pEDQueue, pEDDummy;

#if (DEBUGGING_LEVEL > 0)
    DEBUGLOG("\tBulkTx: adr=%d:%d cbp=%lx:%x br=%s cback=[%lx:%lx:%lx] dir=%d)\n",
	functionAddress, endpointNumber, CBP, bufferSize, bufferRounding?"YES":"NO", 
	(UInt32)completion.action, (UInt32)completion.target, (UInt32)completion.parameter, direction);
#endif

    if (direction == kUSBOut)
        direction = kOHCIEDDirectionOut;
    else if (direction == kUSBIn)
        direction = kOHCIEDDirectionIn;
    else
        direction = kOHCIEDDirectionTD;

    // search for endpoint descriptor
    pEDQueue =
        FindBulkEndpoint(functionAddress, endpointNumber, direction, &pEDDummy);

    if (!pEDQueue)
    {
        IOLog("UIMCreateBulkTransfer: Could not find endpoint\n");
        return (kIOUSBEndpointNotFound);
    }

    myBufferRounding = (UInt32) bufferRounding << kOHCIBufferRoundingOffset;
    TDDirection = (UInt32) direction << kOHCIDirectionOffset;
    kickBits = kOHCIHcCommandStatus_BLF;
    if ( pOHCIUIMData->OptiOn)
        kickBits |= kOHCIHcCommandStatus_CLF;		

    status = UIMCreateGeneralTransfer(pEDQueue, completion, CBP, bufferSize,
	myBufferRounding | TDDirection, kOHCIBulkTransferOutType, kickBits);

#if (DEBUGGING_LEVEL > 2)
    print_bulk_list(pOHCIUIMData);
#endif

    return (status);
}

// Interrupt
IOReturn AppleOHCI::UIMCreateInterruptEndpoint(
            short				functionAddress,
            short				endpointNumber,
            UInt8				direction,
            short				speed,
            UInt16				maxPacketSize,
            short				pollingRate)
{
    OHCIEndpointDescriptorPtr		pOHCIEndpointDescriptor;
    OHCIEndpointDescriptorPtr		pED;
    OHCIIntHeadPtr			pInterruptHead;
    int					offset;


#if (DEBUGGING_LEVEL > 0)
    DEBUGLOG("%s: UIMCreateInterruptEndpoint (%d, %d, %s, %d, %d)\n", getName(),
          functionAddress, endpointNumber, speed ? "lo" : "hi", maxPacketSize,
          pollingRate);
#endif

    if (pOHCIUIMData->rootHubFuncAddress == functionAddress)
    {
        return(kIOReturnSuccess);
    }

    if (direction == kUSBOut)
            direction = kOHCIEDDirectionOut;
    else if (direction == kUSBIn)
            direction = kOHCIEDDirectionIn;
    else
            direction = kOHCIEDDirectionTD;

    pInterruptHead = pOHCIUIMData->pInterruptHead;

///ZZZZz  opti bug fix!!!!
    if (pOHCIUIMData->OptiOn)
            if (speed == kOHCIEDSpeedFull)
                    if (pollingRate >= 8)
                            pollingRate = 7;

    // Do we have room?? if so return with offset equal to location
    if (DetermineInterruptOffset(pollingRate, maxPacketSize, &offset) == false)
        return(kIOReturnNoBandwidth);

#if (DEBUGGING_LEVEL > 0)
    DEBUGLOG("%s: UIMCreateInterruptEndpoint: offset = %d\n", getName(), offset);
#endif
    pED = (OHCIEndpointDescriptorPtr) pInterruptHead[offset].pHead;
    pOHCIEndpointDescriptor = AddEmptyEndPoint (functionAddress, endpointNumber, 
			maxPacketSize, speed, direction, pED, kOHCIEDFormatGeneralTD);
    if (nil == pOHCIEndpointDescriptor)
            return(-1);

    pInterruptHead[offset].nodeBandwidth += maxPacketSize;

#if (DEBUGGING_LEVEL > 2)
    print_int_list(pOHCIUIMData);
#endif

    return (kIOReturnSuccess);
}

IOReturn AppleOHCI::UIMCreateInterruptTransfer(
            short				functionAddress,
            short				endpointNumber,
            IOUSBCompletion			completion,
            IOMemoryDescriptor *		CBP,
            bool				bufferRounding,
            UInt32				bufferSize,
            short				direction)
{
    IOReturn				status = kIOReturnSuccess;
    UInt32				myBufferRounding;
    UInt32				myDirection;
    UInt32				myToggle;
    OHCIEndpointDescriptorPtr		pEDQueue, temp;
    OHCIIntHeadPtr			pInterruptHead;

#if (DEBUGGING_LEVEL > 0)
    DEBUGLOG("\tIntTx: adr=%d:%d cbp=%lx:%lx br=%s cback=[%lx:%lx:%lx])\n", functionAddress, endpointNumber, CBP, bufferSize, bufferRounding?"YES":"NO", (UInt32)completion.action, (UInt32)completion.target, (UInt32)completion.parameter);
#endif

    if (pOHCIUIMData->rootHubFuncAddress == functionAddress)
    {
        SimulateRootHubInt(endpointNumber, CBP, bufferSize, completion);
        return(kIOReturnSuccess);
    }

    if (direction == kUSBOut)
        direction = kOHCIEDDirectionOut;
    else if (direction == kUSBIn)
        direction = kOHCIEDDirectionIn;
    else
        direction = kOHCIEDDirectionTD;

    pInterruptHead = pOHCIUIMData->pInterruptHead;

    pEDQueue = FindInterruptEndpoint(functionAddress, endpointNumber,
							direction,&temp);
    if (pEDQueue != nil)
    {
        myBufferRounding = (UInt32) bufferRounding << kOHCIBufferRoundingOffset;
        myToggle = 0;	/* Take data toggle from Endpoint Descriptor */

        myDirection = (UInt32) direction << kOHCIDirectionOffset;

        status = UIMCreateGeneralTransfer(pEDQueue, completion, CBP, bufferSize,
            myBufferRounding | myDirection | myToggle, kOHCIInterruptInType, 0);
    }
    else
    {
        IOLog("UIMCreateInterruptTransfer: Could not find endpoint\n");
        status = kIOUSBEndpointNotFound;
    }
#if (DEBUGGING_LEVEL > 0)
    DEBUGLOG("UIMCreateInterruptTransfer: returning status=%d\n", status);
#endif
    return (status);
}

// Isoch
IOReturn AppleOHCI::UIMCreateIsochEndpoint(
            short				functionAddress,
            short				endpointNumber,
            UInt32				maxPacketSize,
            UInt8				direction)
{
    OHCIEndpointDescriptorPtr	pOHCIEndpointDescriptor, pED;
    UInt32			curMaxPacketSize;
    UInt32			xtraRequest;
    UInt32			edFlags;


    if (direction == kUSBOut)
        direction = kOHCIEDDirectionOut;
    else if (direction == kUSBIn)
        direction = kOHCIEDDirectionIn;
    else
        direction = kOHCIEDDirectionTD;

    pED = FindIsochronousEndpoint(functionAddress, endpointNumber, direction, nil);
    if (pED) {
        // this is the case where we have already created this endpoint, and now we are adjusting the maxPacketSize
        USBExpertStatusLevel (3, 123456789,
		kP_UIMName"IsochEDCreate- Endpoint already exists, checking maxPacketSize ",
		maxPacketSize);
        edFlags = OSReadLittleInt32(&pED->flags, 0);
        curMaxPacketSize = ( edFlags & kOHCIEDControl_MPS) >> kOHCIEDControl_MPSPhase;
        if (maxPacketSize == curMaxPacketSize) {
            USBExpertStatusLevel (3, 123456789,
		kP_UIMName"IsochEDCreate- maxPacketSize the same, no change ",
		maxPacketSize);
            return kIOReturnSuccess;
        }
        if (maxPacketSize > curMaxPacketSize) {
            // client is trying to get more bandwidth
            xtraRequest = maxPacketSize - curMaxPacketSize;
            if (xtraRequest > pOHCIUIMData->isochBandwidthAvail)
            {
                USBExpertStatusLevel (3, 123456789,
			kP_UIMName"IsochEDCreate- out of bandwidth, request (extra) = ",
			xtraRequest);
                USBExpertStatusLevel (3, 123456789,
			kP_UIMName"IsochEDCreate- available = ",
			pOHCIUIMData->isochBandwidthAvail);
                return kIOReturnNoBandwidth;
            }
            pOHCIUIMData->isochBandwidthAvail -= xtraRequest;
        }
        else {
            // client is trying to return some bandwidth
            xtraRequest = curMaxPacketSize - maxPacketSize;
            USBExpertStatusLevel (3, 123456789,
		kP_UIMName"IsochEDCreate- returning some bandwidth: ",
		xtraRequest);
            pOHCIUIMData->isochBandwidthAvail += xtraRequest;
            USBExpertStatusLevel (3, 123456789,
		kP_UIMName"IsochEDCreate- new available = ",
		pOHCIUIMData->isochBandwidthAvail);
        }
        // update the maxPacketSize field in the endpoint
        edFlags &= ~kOHCIEDControl_MPS;					// strip out old MPS
        edFlags |= (maxPacketSize << kOHCIEDControl_MPSPhase);
        OSWriteLittleInt32(&pED->flags, 0, edFlags);
        return kIOReturnSuccess;
    }

    if (maxPacketSize > pOHCIUIMData->isochBandwidthAvail) {
        USBExpertStatusLevel (3, 123456789,
		kP_UIMName"Isoch Endpoint create- no bandwidth, request = ",
		maxPacketSize);
        USBExpertStatusLevel (3, 123456789,
		kP_UIMName"Isoch Endpoint create- available = ",
		pOHCIUIMData->isochBandwidthAvail);
        return kIOReturnNoBandwidth;
    }

    pOHCIUIMData->isochBandwidthAvail -= maxPacketSize;
    pED = pOHCIUIMData->pIsochHead;
    pOHCIEndpointDescriptor = AddEmptyEndPoint(functionAddress, endpointNumber,
	maxPacketSize, kOHCIEDSpeedFull, direction, pED, kOHCIEDFormatIsochronousTD);
    if (pOHCIEndpointDescriptor == nil) {
        pOHCIUIMData->isochBandwidthAvail += maxPacketSize;
        return(kIOReturnNoMemory);
    }

    USBExpertStatusLevel (5, 123456789,
	kP_UIMName"Isoch Endpoint create- success, bandwidth used = ",
	maxPacketSize);
    USBExpertStatusLevel (5, 123456789, 
	kP_UIMName"Isoch Endpoint create- success, new available = ",
	pOHCIUIMData->isochBandwidthAvail);
    return (kIOReturnSuccess);
}


IOReturn AppleOHCI::UIMCreateIsochTransfer(
            short				functionAddress,
            short				endpointNumber,
            IOUSBIsocCompletion			completion,
            UInt8				direction,
            UInt64				frameNumberStart,
            IOMemoryDescriptor *		pBuffer,
            UInt32				frameCount,
            IOUSBIsocFrame			*pFrames)
{
    IOReturn 				status = kIOReturnSuccess;
    OHCIIsochTransferDescriptorPtr	pTailITD = nil;
    OHCIIsochTransferDescriptorPtr	pNewITD = nil;
    OHCIIsochTransferDescriptorPtr	pTempITD = nil;
    UInt32				i;
    UInt32				curFrameInRequest = 0;
    UInt32				bufferSize = 0;
    UInt32				pageOffset = 0;
    UInt32				curFrameLength;
    UInt32				lastPhysical = 0;
    OHCIEndpointDescriptorPtr		pED;
    UInt32				curFrameInTD = 0;
    UInt16				frameNumber = (UInt16) frameNumberStart;
    UInt64				curFrameNumber = GetFrameNumber();
    UInt64				frameDiff;
    UInt64				maxOffset = (UInt64)(0x00007FF0);
    UInt32				diff32;

    UInt32				itdFlags = 0;
    UInt32				numSegs = 0;
    UInt32				physPageStart = 0;
    UInt32				physPageEnd = 0;
    UInt32				pageSelectMask = 0;
    bool				needNewITD;
    IOPhysicalSegment			segs[2];
    UInt32				tdType;
    IOByteCount				transferOffset;
#if 0
    AbsoluteTime						startTime = UpTime();
    Duration							elapsedTime;
#endif

    if ((frameCount == 0) || (frameCount > 1000))
    {
        USBExpertStatusLevel (3, 123456789, kP_UIMName"Isoch transfer- bad frameCount", kIOReturnBadArgument);
        return kIOReturnBadArgument;
    }

    if (direction == kUSBOut) {
        direction = kOHCIEDDirectionOut;
        tdType = kOHCIIsochronousOutType;
    }
    else if (direction == kUSBIn) {
        direction = kOHCIEDDirectionIn;
        tdType = kOHCIIsochronousInType;
    }
    else
        return kIOReturnInternalError;

    pED = FindIsochronousEndpoint(functionAddress, endpointNumber, direction, nil);

    if (!pED)
    {
        USBExpertStatusLevel (3, 123456789, kP_UIMName"Isoch transfer- endpoint not found, err:", kIOUSBEndpointNotFound);
        return kIOUSBEndpointNotFound;
    }

    if (frameNumberStart <= curFrameNumber)
    {
        if (frameNumberStart < (curFrameNumber - maxOffset))
        {
            USBExpertStatusLevel (3, 123456789, kP_UIMName"Isoch transfer- frameNumberStart:", (UInt32)frameNumberStart);
            USBExpertStatusLevel (3, 123456789, kP_UIMName"Isoch transfer- curFrameNumber:", (UInt32)curFrameNumber);
            USBExpertStatusLevel (3, 123456789, kP_UIMName"Isoch transfer- request frame WAY too old, err:", kIOReturnIsoTooOld);
            return kIOReturnIsoTooOld;
        }
        USBExpertStatusLevel (3, 123456789, kP_UIMName"Isoch transfer- WARNING! curframe later than requested, expect some notSent errors!", 0);
        USBExpertStatusLevel (3, 123456789, kP_UIMName"Isoch transfer- frameNumberStart:", (UInt32)frameNumberStart);
        USBExpertStatusLevel (3, 123456789, kP_UIMName"Isoch transfer- curFrameNumber:", (UInt32)curFrameNumber);
        USBExpertStatusLevel (3, 123456789, kP_UIMName"USBIsocFrame Ptr:", (UInt32)pFrames);
        USBExpertStatusLevel (3, 123456789, kP_UIMName"Isoch transfer- First ITD:", (UInt32)pED->pLogicalTailP);
    } else {					// frameNumberStart > curFrameNumber
        if (frameNumberStart > (curFrameNumber + maxOffset))
        {
            USBExpertStatusLevel (3, 123456789, kP_UIMName"Isoch transfer- frameNumberStart:", (UInt32)frameNumberStart);
            USBExpertStatusLevel (3, 123456789, kP_UIMName"Isoch transfer- curFrameNumber:", (UInt32)curFrameNumber);
            USBExpertStatusLevel (3, 123456789, kP_UIMName"Isoch transfer- request frame too far ahead, err:", kIOReturnIsoTooNew);
            return kIOReturnIsoTooNew;
        }
        frameDiff = frameNumberStart - curFrameNumber;
        diff32 = (UInt32)frameDiff;
        if (diff32 < 2)
        {
            USBExpertStatusLevel (5, 123456789, kP_UIMName"Isoch transfer- WARNING! - frameNumberStart less than 2 ms.", diff32);
            USBExpertStatusLevel (5, 123456789, kP_UIMName"Isoch transfer- frameNumberStart:", (UInt32)frameNumberStart);
            USBExpertStatusLevel (5, 123456789, kP_UIMName"Isoch transfer- curFrameNumber:", (UInt32)curFrameNumber);
        }
    }

    //
    //  Get the total size of buffer
    //
    for ( i = 0; i< frameCount; i++)
    {
        if (pFrames[i].frReqCount > kUSBMaxIsocFrameReqCount)
        {
            USBExpertStatusLevel (3, 123456789, kP_UIMName"Isoch frame too big:", pFrames[i].frReqCount);
            return kIOReturnBadArgument;
        }
        bufferSize += pFrames[i].frReqCount;
    }

    if (direction == kOHCIEDDirectionIn)
        USBExpertStatusLevel (5, 123456789, kP_UIMName"Isoch transfer in, buffer:",
                              (UInt32)pBuffer);
    else
        USBExpertStatusLevel (5, 123456789, kP_UIMName"Isoch transfer out, buffer:",
                              (UInt32)pBuffer);
    USBExpertStatusLevel (5, 123456789, kP_UIMName"Isoch transfer total length:",
                          bufferSize);

    //
    // go ahead and make sure we can grab at least ONE TD, before we lock the buffer	
    //
    pNewITD = OHCIUIMAllocateITD();
    if (pNewITD == nil)
    {
        return kIOReturnNoMemory;
    }

    if (!bufferSize) {
	// Set up suitable dummy info
        numSegs = 1;
        segs[0].location = segs[0].length = 0;
	pageOffset = 0;
    }
    pTailITD = (OHCIIsochTransferDescriptorPtr)pED->pLogicalTailP;	// start with the unused TD on the tail of the list
    OSWriteLittleInt32(&pTailITD->nextTD, 0, pNewITD->pPhysical);	// link in the new ITD
    pTailITD->pLogicalNext = pNewITD;

    needNewITD = false;
    transferOffset = 0;
    while (curFrameInRequest < frameCount) {
        // Get physical segments for next frame
        if(!needNewITD && bufferSize) {
            numSegs = _isoCursor->getPhysicalSegments(pBuffer, transferOffset,
					segs, 2, pFrames[curFrameInRequest].frReqCount);
#if 0
            IOLog("F%ld (%d): numSegs = %ld, (0x%lx, 0x%lx), (0x%lx, 0x%lx)\n", curFrameInRequest,
                pFrames[curFrameInRequest].frReqCount, numSegs, segs[0].location, segs[0].length,
                    segs[1].location, segs[1].length);
#endif
            pageOffset = segs[0].location & kOHCIPageOffsetMask;
            transferOffset += segs[0].length;
            if(numSegs == 2)
                transferOffset += segs[1].length;
        }

        if (curFrameInTD == 0) {
            // set up counters which get reinitialized with each TD
            physPageStart = segs[0].location & kOHCIPageMask;	// for calculating real 13 bit offsets
            pageSelectMask = 0;					// First frame always starts on first page
            needNewITD = false;

            // set up the header of the TD - itdFlags will be stored into flags later
            itdFlags = (UInt16)(curFrameInRequest + frameNumber);
            pTailITD->pIsocFrame = pFrames;		// so we can get back to our info later
            pTailITD->frameNum = curFrameInRequest;	// our own index into the above array
            pTailITD->pType = tdType;			// So interrupt handler knows TD type.
            OSWriteLittleInt32(&pTailITD->bufferPage0, 0,  physPageStart);
        }
        else if ((segs[0].location & kOHCIPageMask) != physPageStart) {
            // pageSelectMask is set if we've already used our one allowed page cross.
            if(pageSelectMask && 
		(((segs[0].location & kOHCIPageMask) != physPageEnd) || numSegs == 2)) {
                // Need new ITD for this
                needNewITD = true;
                continue;
            }
            pageSelectMask = kOHCIPageSize;	// ie. set bit 13
            physPageEnd = segs[numSegs-1].location & kOHCIPageMask;
        }
        curFrameLength = pFrames[curFrameInRequest].frReqCount;
        if ((curFrameInTD > 7) || needNewITD) {
            // we need to start a new TD
            needNewITD = true;	// To simplify test at top of loop.
            itdFlags |= (curFrameInTD-1) << kOHCIITDControl_FCPhase;
            OSWriteLittleInt32(&pTailITD->bufferEnd, 0, lastPhysical);
            curFrameInTD = 0;
            pNewITD = OHCIUIMAllocateITD();
            if (pNewITD == nil) {
                status = kIOReturnNoMemory;
		break;
            }
            // Handy for debugging transfer lists
            itdFlags |= (kOHCIGTDConditionNotAccessed << kOHCIGTDControl_CCPhase);
            OSWriteLittleInt32(&pTailITD->flags, 0, itdFlags);
            pTailITD->completion.action = NULL;
            pTailITD = pTailITD->pLogicalNext;		// this is the "old" pNewTD
            OSWriteLittleInt32(&pTailITD->nextTD, 0, pNewITD->pPhysical);	// link to the "new" pNewTD
            pTailITD->pLogicalNext = pNewITD;
            continue;		// start over
        }
        //
        // at this point we know we have a frame which will fit into the current TD
        //
        // calculate the buffer offset for the beginning of this frame
        OSWriteLittleInt16(&pTailITD->offset[curFrameInTD], 0,
            pageOffset |		// offset
            pageSelectMask |		// offset from BP0 or BufferEnd
            (kOHCIITDOffsetConditionNotAccessed << kOHCIITDOffset_CCPhase));	// mark as unused

        // adjust counters and calculate the physical offset of the end of the frame for the next time around the loop
        curFrameInRequest++;
        curFrameInTD++;
        lastPhysical = segs[numSegs-1].location + segs[numSegs-1].length - 1;
    }			

    if (status != kIOReturnSuccess)
    {
        // unlink the TDs, unlock the buffer, and return the status
        pNewITD = pTailITD->pLogicalNext;	// point to the "old" pNewTD, which will also get deallocated
        pTempITD = (OHCIIsochTransferDescriptorPtr)pED->pLogicalTailP;
        pTailITD = pTempITD->pLogicalNext;	// don't deallocate the real tail!
        pTempITD->pLogicalNext = nil;		// just to make sure
        pTempITD->nextTD = nil;			// just to make sure
        while (pTailITD != pNewITD)
        {
            pTempITD = pTailITD;
            pTailITD = pTailITD->pLogicalNext;
            OHCIUIMDeallocateITD(pTempITD);
        }
    }
    else
    {
        // we have good status, so let's kick off the machine
        // we need to tidy up the last TD, which is not yet complete
        itdFlags |= (curFrameInTD-1) << kOHCIITDControl_FCPhase;
        OSWriteLittleInt32(&pTailITD->flags, 0, itdFlags);
        OSWriteLittleInt32(&pTailITD->bufferEnd, 0, lastPhysical);
        pTailITD->completion = completion;
        //print_itd(pTailITD);
        // Make new descriptor the tail
        pED->pLogicalTailP = pNewITD;
        OSWriteLittleInt32(&pED->tdQueueTailPtr, 0, pNewITD->pPhysical);
    }

#if 0
    elapsedTime = AbsoluteDeltaToDuration(UpTime(), startTime);
    if ((elapsedTime > 0) || (elapsedTime < -700))	// measured in milliseconds or more than 700 microseconds to queue everything
    {
        USBExpertStatusLevel (4, 123456789, kP_UIMName"Isoch WARNING! > 1 ms (or getting close): ", elapsedTime);
        USBExpertStatusLevel (4, 123456789, kP_UIMName"Isoch transfer frame count: ", frameCount);
        USBExpertStatusLevel (4, 123456789, kP_UIMName"Isoch transfer buffer size: ", bufferSize);
    }
#endif
    //print_isoc_ed(pED);

    return status;

}

IOReturn AppleOHCI::UIMAbortEndpoint(
            short				functionAddress,
            short				endpointNumber,
            short				direction)
{
    OHCIRegistersPtr		pOHCIRegisters;
    OHCIEndpointDescriptorPtr	pED;
    OHCIEndpointDescriptorPtr	pEDQueueBack;
    UInt32			something, controlMask;

    if (functionAddress == 0)
    {
        IOLog("UIMAbortEndpoint: Error: operation on root hub\n");
        return(kIOReturnSuccess);
    }

    pOHCIRegisters = pOHCIUIMData->pOHCIRegisters;

    if (direction == kUSBOut)
        direction = kOHCIEDDirectionOut;
    else if (direction == kUSBIn)
        direction = kOHCIEDDirectionIn;
    else
        direction = kOHCIEDDirectionTD;
    
    //search for endpoint descriptor
    pED = FindEndpoint (functionAddress,
                        endpointNumber,
                        direction,
                        &pEDQueueBack,
                        &controlMask);
    if (!pED)
    {
        IOLog("UIMAbortEndpoint: Error: Could not find endpoint\n");
        return (kIOUSBEndpointNotFound);
    }

    pED->flags |= OSSwapInt32(kOHCIEDControl_K);	// mark the ED as skipped

    // poll for interrupt  zzzzz turn into real interrupt
    pOHCIRegisters->hcInterruptStatus = OSSwapInt32(kOHCIHcInterrupt_SF);
    IOSleep(1);
    something = OSSwapInt32(pOHCIRegisters->hcInterruptStatus)
        					& kOHCIInterruptSOFMask;

    if (!something)
    {
        /* This should have been set, just in case wait another ms */
        IOSleep(1);
    }

    RemoveTDs(pED);

    pED->flags &= ~OSSwapInt32(kOHCIEDControl_K);	// activate ED again


    return (kIOReturnSuccess);
}

IOReturn AppleOHCI::UIMDeleteEndpoint(
            short				functionAddress,
            short				endpointNumber,
            short				direction)
{
    OHCIRegistersPtr		pOHCIRegisters;
    OHCIEndpointDescriptorPtr	pED;
    OHCIEndpointDescriptorPtr	pEDQueueBack;
    UInt32			hcControl;
    UInt32			something, controlMask;
    //	UInt32			edDirection;


#if 0
    if (functionAddress == 0)
    {
        IOLog("UIMDeleteEndpoint: Error: operation on root hub\n");
        return(kIOReturnSuccess);
    }
#endif

    pOHCIRegisters = pOHCIUIMData->pOHCIRegisters;

    if (direction == kUSBOut)
        direction = kOHCIEDDirectionOut;
    else if (direction == kUSBIn)
        direction = kOHCIEDDirectionIn;
    else
        direction = kOHCIEDDirectionTD;

    //search for endpoint descriptor
    pED = FindEndpoint (functionAddress,
                        endpointNumber,
                        direction,
                        &pEDQueueBack,
                        &controlMask);
    if (!pED)
    {
        DEBUGLOG("UIMDeleteEndpoint: Error: Could not find endpoint\n");
        return (kIOUSBEndpointNotFound);
    }
    
    // Remove Endpoint
    //mark sKipped
    pED->flags |= OSSwapInt32(kOHCIEDControl_K);
    //	edDirection = OSSwapInt32(pED->dWord0) & kOHCIEndpointDirectionMask;
    // remove pointer wraps
    pEDQueueBack->nextED = pED->nextED;
    pEDQueueBack->pLogicalNext = pED->pLogicalNext;

    // clear some bit in hcControl
    hcControl = OSSwapInt32(pOHCIRegisters->hcControl);	
    hcControl &= ~controlMask;
    hcControl &= OHCIBitRange(0, 10);

    pOHCIRegisters->hcControl = OSSwapInt32(hcControl);

    // poll for interrupt  zzzzz turn into real interrupt
    pOHCIRegisters->hcInterruptStatus = OSSwapInt32(kOHCIHcInterrupt_SF);
    IOSleep(1);
    something = OSSwapInt32(pOHCIRegisters->hcInterruptStatus)
        					& kOHCIInterruptSOFMask;
    if (!something)
    {
        /* This should have been set, just in case wait another ms */
        IOSleep(1);
    }
    // restart hcControl
    hcControl |= controlMask;
    pOHCIRegisters->hcControl = OSSwapInt32(hcControl);

#if (DEBUGGING_LEVEL > 0)
    DEBUGLOG("UIMDeleteEndpoint: pED=%lx hcControl=%lx SOF int?=%s\n",
          (UInt32)pED, hcControl, something?"yes":"no");
#endif
    if (GetEDType(pED) == kOHCIEDFormatIsochronousTD)
    {
        UInt32 maxPacketSize = (OSReadLittleInt32(&pED->flags, 0) & kOHCIEDControl_MPS) >>
                                                                            kOHCIEDControl_MPSPhase;
        pOHCIUIMData->isochBandwidthAvail += maxPacketSize;
        USBExpertStatusLevel (5, 123456789,
            kP_UIMName"Isoch Endpoint delete- bandwidth returned = ", maxPacketSize);
        USBExpertStatusLevel (5, 123456789,
            kP_UIMName"Isoch Endpoint delete- new available = ", pOHCIUIMData->isochBandwidthAvail);
    }
    RemoveAllTDs(pED);

    pED->nextED = nil;

    //deallocate ED
    OHCIUIMDeallocateED(pED);
#if (DEBUGGING_LEVEL > 2)
        print_bulk_list(pOHCIUIMData);
        print_control_list(pOHCIUIMData);
#endif
    return (kIOReturnSuccess);
}

void AppleOHCI::ReturnTransactions(
            OHCIGeneralTransferDescriptor 	*transaction,
            UInt32				tail)
{
    UInt32                          physicalAddress;
    OHCIGeneralTransferDescriptor   *nextTransaction;

    while(transaction->pPhysical != tail)
    {
        if (transaction->completion.action != nil) {
            IOUSBCompletion completion;
            // zero out callback first then call it
            completion = transaction->completion;
            transaction->completion.action = nil;
            complete(completion, kIOUSBPipeStalled, 0);
        }
        /* walk the physically-addressed list */
        physicalAddress =
        	OSSwapInt32(transaction->nextTD) & kOHCIHeadPMask;
        nextTransaction = (OHCIGeneralTransferDescriptorPtr)
                            OHCIUIMGetLogicalAddress (physicalAddress);
        OHCIUIMDeallocateTD(transaction);
        transaction = nextTransaction;
        if(transaction == nil)
        {
            IOLog("returnTransactions: Return queue broken");
            break;
        }
    }
}

IOReturn AppleOHCI::UIMClearEndpointStall(
            short				functionAddress,
            short				endpointNumber,
            short				direction)
{
    OHCIRegistersPtr			pOHCIRegisters;
    OHCIEndpointDescriptorPtr		pEDQueueBack, pED;
    OHCIGeneralTransferDescriptor	*transaction;
    UInt32				tail, controlMask;


#if DEBUGGING_LEVEL > 0
    DEBUGLOG("%s: clearing endpoint %d:%d stall\n", getName(), functionAddress, endpointNumber);
#endif
    
    if (pOHCIUIMData->rootHubFuncAddress == functionAddress)
    {
        IOLog("UIMDeleteEndpoint: Error: operation on root hub\n");
        return(kIOReturnSuccess);
    }

    if (direction == kUSBOut)
        direction = kOHCIEDDirectionOut;
    else if (direction == kUSBIn)
        direction = kOHCIEDDirectionIn;
    else
        direction = kOHCIEDDirectionTD;

    pOHCIRegisters = pOHCIUIMData->pOHCIRegisters;

    transaction = nil;
    tail = nil;
    //search for endpoint descriptor
    pED = FindEndpoint (functionAddress,
                        endpointNumber,
                        direction,
                        &pEDQueueBack,
                        &controlMask);
    if (!pED)
    {
        IOLog("UIMDeleteEndpoint: Error: Could not find endpoint\n");
        return (kIOUSBEndpointNotFound);
    }

    if (pED != nil)
    {
        tail = OSSwapInt32(pED->tdQueueTailPtr);
        transaction = (OHCIGeneralTransferDescriptor *)
            OHCIUIMGetLogicalAddress(OSSwapInt32(pED->tdQueueHeadPtr) & kOHCIHeadPMask);
        /* unlink all transactions at once (this also clears the halted bit) */
        pED->tdQueueHeadPtr = pED->tdQueueTailPtr;
        pED->pLogicalHeadP = pED->pLogicalTailP;
    }	

    if (transaction != nil)
    {
        ReturnTransactions(transaction, tail);
    }
    return (kIOReturnSuccess);
}

OHCIEndpointDescriptorPtr AppleOHCI::AddEmptyEndPoint(
        UInt8 						functionAddress,
        UInt8						endpointNumber,
        UInt16						maxPacketSize,
        UInt8						speed,
        UInt8						direction,
        OHCIEndpointDescriptorPtr			pED,
        OHCIEDFormat					format)
{
    UInt32				myFunctionAddress,
    					myEndpointNumber,
    					myEndpointDirection,
    					myMaxPacketSize,
    					mySpeed,
    					myFormat;
    OHCIEndpointDescriptorPtr		pOHCIEndpointDescriptor;
    OHCIGeneralTransferDescriptorPtr	pOHCIGeneralTransferDescriptor;
    OHCIIsochTransferDescriptorPtr	pITD;

    
    pOHCIEndpointDescriptor = (OHCIEndpointDescriptorPtr) OHCIUIMAllocateED();
    myFunctionAddress = ((UInt32) functionAddress) << kOHCIEDControl_FAPhase;
    myEndpointNumber = ((UInt32) endpointNumber) << kOHCIEDControl_ENPhase;
    myEndpointDirection = ((UInt32) direction) << kOHCIEDControl_DPhase;
    mySpeed = ((UInt32) speed) << kOHCIEDControl_SPhase;
    myMaxPacketSize = ((UInt32) maxPacketSize) << kOHCIEDControl_MPSPhase;
    myFormat = ((UInt32) format) << kOHCIEDControl_FPhase;
    pOHCIEndpointDescriptor->flags =
        OSSwapInt32(myFunctionAddress
                     | myEndpointNumber
                     | myEndpointDirection
                     | myMaxPacketSize
                     | mySpeed
                     | myFormat);

    if (format == kOHCIEDFormatGeneralTD)
    {
        pOHCIGeneralTransferDescriptor = OHCIUIMAllocateTD();

        /* These were previously nil */
        pOHCIEndpointDescriptor->tdQueueTailPtr =
            OSSwapInt32( pOHCIGeneralTransferDescriptor->pPhysical);
        pOHCIEndpointDescriptor->tdQueueHeadPtr =
            OSSwapInt32( pOHCIGeneralTransferDescriptor->pPhysical);
        pOHCIEndpointDescriptor->pLogicalTailP =
            pOHCIGeneralTransferDescriptor;
        pOHCIEndpointDescriptor->pLogicalHeadP =
            pOHCIGeneralTransferDescriptor;
    }
    else
    {
        pITD = OHCIUIMAllocateITD();

        /* These were previously nil */
        pOHCIEndpointDescriptor->tdQueueTailPtr =
            OSSwapInt32( pITD->pPhysical);
        pOHCIEndpointDescriptor->tdQueueHeadPtr =
            OSSwapInt32( pITD->pPhysical);
        pOHCIEndpointDescriptor->pLogicalTailP = pITD;
        pOHCIEndpointDescriptor->pLogicalHeadP = pITD;		

    }

    pOHCIEndpointDescriptor->nextED = pED->nextED;
    pOHCIEndpointDescriptor->pLogicalNext = pED->pLogicalNext;
    pED->pLogicalNext = pOHCIEndpointDescriptor;
    pED->nextED = OSSwapInt32(pOHCIEndpointDescriptor->pPhysical);

    return (pOHCIEndpointDescriptor);
}

OHCIEndpointDescriptorPtr AppleOHCI::FindControlEndpoint (
	short 						functionNumber, 
	short						endpointNumber, 
	OHCIEndpointDescriptorPtr   			*pEDBack)
{
    UInt32			unique;
    OHCIEndpointDescriptorPtr	pEDQueue;
    OHCIEndpointDescriptorPtr	pEDQueueBack;

	
    // search for endpoint descriptor
    unique = (UInt32) ((((UInt32) endpointNumber) << kOHCIEndpointNumberOffset)
                       		| ((UInt32) functionNumber));
    pEDQueueBack = (OHCIEndpointDescriptorPtr) pOHCIUIMData->pControlHead;
    pEDQueue = (OHCIEndpointDescriptorPtr) pEDQueueBack->pLogicalNext;

    while ((UInt32) pEDQueue != pOHCIUIMData->pControlTail)
    {
        if ((OSSwapInt32(pEDQueue->flags) & kUniqueNumNoDirMask) == unique)
        {
            *pEDBack = pEDQueueBack;
            return (pEDQueue);
        }
        else
        {
            pEDQueueBack = pEDQueue;
            pEDQueue = (OHCIEndpointDescriptorPtr) pEDQueue->pLogicalNext;
        }
    }
    if (pOHCIUIMData->OptiOn)
    {
        pEDQueue = FindBulkEndpoint (functionNumber, endpointNumber,
                                     kOHCIEDDirectionTD, &pEDQueueBack);
        *pEDBack = pEDQueueBack;
        return (pEDQueue);
    }
    return (nil);
}


OHCIEndpointDescriptorPtr AppleOHCI::FindBulkEndpoint (
	short 						functionNumber, 
	short						endpointNumber,
	short						direction,
	OHCIEndpointDescriptorPtr			*pEDBack)
{

    UInt32			unique;
    UInt32			myEndpointDirection;
    OHCIEndpointDescriptorPtr	pEDQueue;
    OHCIEndpointDescriptorPtr	pEDQueueBack;


    // search for endpoint descriptor
    myEndpointDirection = ((UInt32) direction) << kOHCIEndpointDirectionOffset;
    unique = (UInt32) ((((UInt32) endpointNumber) << kOHCIEndpointNumberOffset)
                       | ((UInt32) functionNumber) | myEndpointDirection);
    pEDQueueBack = (OHCIEndpointDescriptorPtr) pOHCIUIMData->pBulkHead;
    pEDQueue = (OHCIEndpointDescriptorPtr) pEDQueueBack->pLogicalNext;

    while (((UInt32) pEDQueue) != pOHCIUIMData->pBulkTail )
    {
        if ((OSSwapInt32(pEDQueue->flags) & kUniqueNumMask) == unique)
        {
            *pEDBack = pEDQueueBack;
            return (pEDQueue);
        }
        else
        {
            pEDQueueBack = pEDQueue;
            pEDQueue = (OHCIEndpointDescriptorPtr) pEDQueue->pLogicalNext;
        }
    }
    return (nil);
}


OHCIEndpointDescriptorPtr AppleOHCI::FindEndpoint (
	short 						functionNumber, 
	short 						endpointNumber,
	short 						direction, 
	OHCIEndpointDescriptorPtr 			*pEDQueueBack, 
	UInt32 						*controlMask)
{
    OHCIEndpointDescriptorPtr pED, pEDBack;

    pED = FindControlEndpoint (functionNumber, endpointNumber, &pEDBack);
    if (pED != nil)
    {
        *pEDQueueBack = pEDBack;
        *controlMask = kOHCIHcControl_CLE;
        return (pED);
    }

    pED = FindBulkEndpoint(functionNumber, endpointNumber, direction, &pEDBack);
    if (pED != nil)
    {
        *pEDQueueBack = pEDBack;

        *controlMask = kOHCIHcControl_BLE;
        //zzzz Opti Bug
        if(pOHCIUIMData->OptiOn)
            *controlMask = kOHCIHcControl_CLE;
        return (pED);
    }

    pED = FindInterruptEndpoint(functionNumber, endpointNumber, direction,
	&pEDBack);
    if (pED != nil)
    {
        *pEDQueueBack = pEDBack;
        *controlMask = 0;
        return (pED);	
    }

    pED = FindIsochronousEndpoint(functionNumber, endpointNumber,
                                  direction, &pEDBack);
    *pEDQueueBack = pEDBack;
    *controlMask = 0;
    return (pED);	
}


OHCIEndpointDescriptorPtr AppleOHCI::FindIsochronousEndpoint(
	short 						functionNumber,
	short						endpointNumber,
	short 						direction, 
	OHCIEndpointDescriptorPtr			*pEDBack)
{
    UInt32			myEndpointDirection;
    UInt32			unique;
    OHCIEndpointDescriptorPtr	pEDQueue, pEDQueueBack;

    // search for endpoint descriptor
    myEndpointDirection = ((UInt32) direction) << kOHCIEndpointDirectionOffset;
    unique = (UInt32) ((((UInt32) endpointNumber) << kOHCIEndpointNumberOffset)
                       | ((UInt32) functionNumber) | myEndpointDirection);

    pEDQueueBack = (OHCIEndpointDescriptorPtr) pOHCIUIMData->pIsochHead;
    pEDQueue = (OHCIEndpointDescriptorPtr) pEDQueueBack->pLogicalNext;
    while (((UInt32) pEDQueue) != pOHCIUIMData->pIsochTail )
    {
        if ((OSSwapInt32(pEDQueue->flags) & kUniqueNumMask) == unique)
        {
            if(pEDBack)
                *pEDBack = pEDQueueBack;
            return (pEDQueue);
        }
        else
        {
            pEDQueueBack = pEDQueue;
            pEDQueue = (OHCIEndpointDescriptorPtr) pEDQueue->pLogicalNext;
        }
    }
    return (nil);
}


OHCIEndpointDescriptorPtr AppleOHCI::FindInterruptEndpoint(
	short 					functionNumber,
	short					endpointNumber,
        short					direction,
	OHCIEndpointDescriptorPtr		*pEDBack)
{
    OHCIRegistersPtr			pOHCIRegisters;
    UInt32				myEndpointDirection;
    UInt32				unique;
    OHCIEndpointDescriptorPtr		pEDQueue;
    OHCIIntHeadPtr			pInterruptHead;
    int					i;
    UInt32				temp;
    
    pOHCIRegisters = pOHCIUIMData->pOHCIRegisters;
    pInterruptHead = pOHCIUIMData->pInterruptHead;

    //search for endpoint descriptor
    myEndpointDirection = ((UInt32) direction) << kOHCIEndpointDirectionOffset;
    unique = (UInt32) ((((UInt32) endpointNumber) << kOHCIEDControl_ENPhase)
                       | (((UInt32) functionNumber) << kOHCIEDControl_FAPhase)
                       | myEndpointDirection);

    for (i = 0; i < 63; i++)
    {
        pEDQueue = pInterruptHead[i].pHead;
        *pEDBack = pEDQueue;
        // BT do this first, or you find the dummy endpoint
        // all this is hanging off. It matches 0,0
        pEDQueue = (OHCIEndpointDescriptorPtr) pEDQueue->pLogicalNext;
        while (pEDQueue != pInterruptHead[i].pTail)
        {
            temp = (OSSwapInt32(pEDQueue->flags)) & kUniqueNumMask;

            if ( temp == unique)
            {
                return(pEDQueue);
            }
            *pEDBack = pEDQueue;
            pEDQueue = (OHCIEndpointDescriptorPtr) pEDQueue->pLogicalNext;
        }
    }
    return(nil);
}

bool AppleOHCI::DetermineInterruptOffset(
    UInt32          pollingRate,
    UInt32          /* reserveBandwidth */,
    int             *offset)
{
    int num;
    OHCIRegistersPtr            pOHCIRegisters;

    pOHCIRegisters = pOHCIUIMData->pOHCIRegisters;

    num = OSSwapInt32(pOHCIRegisters->hcFmNumber)&kOHCIFmNumberMask;
    if (pollingRate <  1)
        //error condition
        return(false);
    else if(pollingRate < 2)
        *offset = 62;
    else if(pollingRate < 4)
        *offset = (num%2) + 60;
    else if(pollingRate < 8)
        *offset = (num%4) + 56;
    else if(pollingRate < 16)
        *offset = (num%8) + 48;
    else if(pollingRate < 32)
        *offset = (num%16) + 32;
    else
        *offset = (num%32) + 0;
    return (true);
}

static char *cc_errors[] = {
    "NO ERROR",			/* 0 */
    "CRC",			/* 0 */
    "BIT STUFFING",		/* 0 */
    "DATA TOGGLE MISMATCH",	/* 0 */
    "STALL",			/* 0 */
    "DEVICE NOT RESPONDING",	/* 0 */
    "PID CHECK FAILURE",	/* 0 */
    "UNEXPECTED PID",		/* 0 */
    "DATA OVERRUN",		/* 0 */
    "DATA UNDERRUN",		/* 0 */
    "??",			/* reserved */
    "??",			/* reserved */
    "BUFFER OVERRUN",		/* 0 */
    "BUFFER UNDERRUN",		/* 0 */
    "NOT ACCESSED A",		/* not processed yet */
    "NOT ACCESSED B"		/* not processed yet */
};

void print_td(OHCIGeneralTransferDescriptorPtr pTD)
{
    UInt32 w0, dir, err;

    if (pTD == 0) return;

    w0 = OSSwapInt32(pTD->flags);
    dir = (w0 & kOHCIGTDControl_DP) >> kOHCIGTDControl_DPPhase;
    err = (w0 & kOHCIGTDControl_CC) >> kOHCIGTDControl_CCPhase;
    DEBUGLOG("\tTD(0x%08lx->0x%08lx) dir=%s cc=%s errc=%ld t=%ld rd=%s: c=0x%08lx cbp=0x%08lx, next=0x%08lx, bend=0x%08lx\n",
	(UInt32)pTD, pTD->pPhysical,
	dir == 0 ? "SETUP" : (dir==2?"IN":"OUT"),
	cc_errors[err],
	(w0 & kOHCIGTDControl_EC) >> kOHCIGTDControl_ECPhase,
	(w0 & kOHCIGTDControl_T)  >> kOHCIGTDControl_TPhase,
	(w0 & kOHCIGTDControl_R)?"yes":"no",
	OSSwapInt32(pTD->flags),
	OSSwapInt32(pTD->currentBufferPtr),
	OSSwapInt32(pTD->nextTD),
	OSSwapInt32(pTD->bufferEnd));
}

void print_itd(OHCIIsochTransferDescriptorPtr pTD)
{
    UInt32 w0, err;
    int i;
    if (pTD == 0) return;

    w0 = OSReadLittleInt32(&pTD->flags, 0);
    err = (w0 & kOHCIITDControl_CC) >> kOHCIITDControl_CCPhase;
    DEBUGLOG("\tTD(0x%08lx->0x%08lx) cc=%s fc=%ld sf=0x%lx c=0x%08lx bp0=0x%08lx, next=0x%08lx, bend=0x%08lx\n",
        (UInt32)pTD, pTD->pPhysical,
        cc_errors[err],
        (w0 & kOHCIITDControl_FC) >> kOHCIITDControl_FCPhase,
        (w0 & kOHCIITDControl_SF)  >> kOHCIITDControl_SFPhase,
        w0,
        OSSwapInt32(pTD->bufferPage0),
        OSSwapInt32(pTD->nextTD),
        OSSwapInt32(pTD->bufferEnd));
    for(i=0; i<8; i++) {
	DEBUGLOG("Offset/PSW %d = 0x%x\n", i, OSReadLittleInt16(&pTD->offset[i], 0));
    }
    DEBUGLOG("frames = 0x%lx, FrameNumber %ld\n", (UInt32)pTD->pIsocFrame, pTD->frameNum);
}

void print_ed(OHCIEndpointDescriptorPtr pED)
{
    OHCIGeneralTransferDescriptorPtr	pTD;
    UInt32 w0;


    if (pED == 0) {
	kprintf("Null ED\n");
	return;
    }
    w0 = OSSwapInt32(pED->flags);

    if ((w0 & kOHCIEDControl_K) == 0 /*noskip*/)
    {
        DEBUGLOG("ED(0x%08lx->0x%08lx) %ld:%ld d=%ld s=%s sk=%s i=%s max=%ld : c=0x%08lx tail=0x%08lx, head=0x%08lx, next=0x%08lx\n",
              (UInt32)pED, (UInt32)pED->pPhysical,
              (w0 & kOHCIEDControl_FA) >> kOHCIEDControl_FAPhase,
              (w0 & kOHCIEDControl_EN) >> kOHCIEDControl_ENPhase,
              (w0 & kOHCIEDControl_D)  >> kOHCIEDControl_DPhase,
              w0 & kOHCIEDControl_S?"low":"hi",
              w0 & kOHCIEDControl_K?"yes":"no",
              w0 & kOHCIEDControl_F?"yes":"no",
              (w0 & kOHCIEDControl_MPS) >> kOHCIEDControl_MPSPhase,
              OSSwapInt32(pED->flags),
              OSSwapInt32(pED->tdQueueTailPtr),
              OSSwapInt32(pED->tdQueueHeadPtr),
              OSSwapInt32(pED->nextED));

        //pTD = (OHCIGeneralTransferDescriptorPtr) pED->pVirtualHeadP;
       pTD = (OHCIGeneralTransferDescriptorPtr)
           (OSSwapInt32(pED->tdQueueHeadPtr) & kOHCINextEndpointDescriptor_nextED);
        
        while (pTD != 0)
        {
            DEBUGLOG("\t");
            print_td(pTD);
            pTD = pTD->pLogicalNext;
        }
    }
}

void print_isoc_ed(OHCIEndpointDescriptorPtr pED)
{
    OHCIIsochTransferDescriptorPtr	pTD;
    UInt32 w0;


    if (pED == 0) {
        kprintf("Null ED\n");
        return;
    }
    w0 = OSSwapInt32(pED->flags);

    if ((w0 & kOHCIEDControl_K) == 0 /*noskip*/)
    {
        DEBUGLOG("ED(0x%08lx->0x%08lx) %ld:%ld d=%ld s=%s sk=%s i=%s max=%ld : c=0x%08lx tail=0x%08lx, head=0x%08lx, next=0x%08lx\n",
              (UInt32)pED, (UInt32)pED->pPhysical,
              (w0 & kOHCIEDControl_FA) >> kOHCIEDControl_FAPhase,
              (w0 & kOHCIEDControl_EN) >> kOHCIEDControl_ENPhase,
              (w0 & kOHCIEDControl_D)  >> kOHCIEDControl_DPhase,
              w0 & kOHCIEDControl_S?"low":"hi",
              w0 & kOHCIEDControl_K?"yes":"no",
              w0 & kOHCIEDControl_F?"yes":"no",
              (w0 & kOHCIEDControl_MPS) >> kOHCIEDControl_MPSPhase,
              OSSwapInt32(pED->flags),
              OSSwapInt32(pED->tdQueueTailPtr),
              OSSwapInt32(pED->tdQueueHeadPtr),
              OSSwapInt32(pED->nextED));

        pTD = (OHCIIsochTransferDescriptorPtr) pED->pLogicalHeadP;
        while (pTD != 0)
        {
            DEBUGLOG("\t");
            print_itd(pTD);
            pTD = pTD->pLogicalNext;
        }
    }
}

void print_list(OHCIEndpointDescriptorPtr pListHead,
                OHCIEndpointDescriptorPtr pListTail)
{
    OHCIEndpointDescriptorPtr		pED, pEDTail;


    pED = (OHCIEndpointDescriptorPtr) pListHead;
    pEDTail = (OHCIEndpointDescriptorPtr) pListTail;

    while (pED != pEDTail)
    {
        print_ed(pED);
        pED = (OHCIEndpointDescriptorPtr) pED->pLogicalNext;
    }
    print_ed(pEDTail);
}

void print_control_list(OHCIUIMDataPtr pData)
{
    DEBUGLOG("Control List: h/w head = 0x%lx\n",
	OSReadLittleInt32(&pData->pOHCIRegisters->hcControlHeadED, 0));
    print_list((OHCIEndpointDescriptorPtr) pData->pControlHead,
               (OHCIEndpointDescriptorPtr) pData->pControlTail);
}

void print_bulk_list(OHCIUIMDataPtr pData)
{
    DEBUGLOG("Bulk List:\n");
    DEBUGLOG("Bulk List: h/w head = 0x%lx\n",
	OSReadLittleInt32(&pData->pOHCIRegisters->hcBulkHeadED, 0));
    print_list((OHCIEndpointDescriptorPtr) pData->pBulkHead,
               (OHCIEndpointDescriptorPtr) pData->pBulkTail);
}

void print_int_list(OHCIUIMDataPtr pData)
{
    OHCIIntHeadPtr	pInterruptHead = pData->pInterruptHead;
    int			i;
    UInt32		w0;
    OHCIEndpointDescriptorPtr pED;


    DEBUGLOG("Interrupt List:\n");
    for (i = 0; i < 63; i++)
    {
        pED = (OHCIEndpointDescriptorPtr) pInterruptHead[i].pHead->pLogicalNext;
        w0 = OSSwapInt32(pED->flags);

        if ((w0 & kOHCIEDControl_K) == 0 /*noskip*/)
        {
            DEBUGLOG("%d:", i);
            print_ed(pED);
        }
    }
}
