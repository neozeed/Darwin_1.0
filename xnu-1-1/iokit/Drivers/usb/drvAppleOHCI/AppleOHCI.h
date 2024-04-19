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


#ifndef _IOKIT_APPLEOHCI_H
#define _IOKIT_APPLEOHCI_H

#include <libkern/c++/OSData.h>
#include <IOKit/IOService.h>
#include <IOKit/IOInterruptEventSource.h>
#include <IOKit/pci/IOPCIBridge.h>
#include <IOKit/pci/IOPCIDevice.h>

#include <IOKit/usb/IOUSBController.h>
#include <IOKit/usb/USB.h>
#include <IOKit/usb/USBHub.h>

#include "OHCI.h"
#include "OHCIRootHub.h"

#define USB_CONSTANT16(x)	((((x) >> 8) & 0x0ff) | ((x & 0xff) << 8))
#define MICROSECOND		(1)
#define MILLISECOND		(1000)

#ifdef __ppc__
#define IOSync eieio
#endif

extern "C" {
    extern int	kvtophys(vm_offset_t);
    extern void delay(int);
};

struct InterruptTransaction {
    IOMemoryDescriptor *	buf;
    UInt32 			bufLen;
    IOUSBCompletion		completion;
};
#define kMaxOutstandingTrans 4

class IONaturalMemoryCursor;

class AppleOHCI : public IOUSBController
{
    OSDeclareDefaultStructors(AppleOHCI)

protected:

    char *usbBuffer;
    IOPCIDevice *      	_device;
    IOMemoryMap *	_deviceBase;
    IONaturalMemoryCursor * _genCursor;
    IONaturalMemoryCursor * _isoCursor;
    OHCIGeneralTransferDescriptorPtr _pendingHead, _pendingTail;
    UInt16		_vendorID;
    UInt16		_deviceID;
    UInt16		_revisionID;

    static void OHCIUIMInterruptHandler(OSObject *owner,
                                        IOInterruptEventSource * source,
                                        int count);
    void	SetVendorInfo(void);
    void	finishPending();

    OHCIUIMDataPtr		pOHCIUIMData;
    IOInterruptEventSource 	*interruptSource;
    IOLock *			_intLock;
    struct InterruptTransaction	_outstandingTrans[kMaxOutstandingTrans];

    IOReturn OHCIUIMControlInitialize(void);
    IOReturn OHCIUIMBulkInitialize (void);
    IOReturn OHCIUIMIsochronousInitialize(void);
    IOReturn OHCIUIMInterruptInitialize (void);

    // Memory routines
    UInt32 OHCIUIMGetLogicalAddress(UInt32 pPhysicalAddress);
    UInt32 OHCIUIMGetPhysicalAddress(UInt32 LogicalAddress,
                                     UInt32 count);
    UInt32 OHCIUIMCreatePhysicalAddress(UInt32 pLogicalAddress,
                                        UInt32 count);
    IOReturn OHCIUIMAllocateMemory(int num_of_TDs,
                                   int num_of_EDs,
                                   int num_of_ITDs);
    void doCallback(OHCIGeneralTransferDescriptorPtr	nextTD,
                    UInt32				transferStatus,
                    UInt32  				bufferSizeRemaining);
    UInt32 findBufferRemaining (OHCIGeneralTransferDescriptorPtr pCurrentTD);
    OHCIIsochTransferDescriptorPtr OHCIUIMAllocateITD(void);
    OHCIGeneralTransferDescriptorPtr OHCIUIMAllocateTD(void);
    OHCIEndpointDescriptorPtr OHCIUIMAllocateED(void);
    IOReturn OHCIUIMDeallocateITD (OHCIIsochTransferDescriptorPtr pTD);
    IOReturn OHCIUIMDeallocateTD (OHCIGeneralTransferDescriptorPtr pTD);
    IOReturn OHCIUIMDeallocateED (OHCIEndpointDescriptorPtr pED);
    IOReturn RemoveAllTDs(OHCIEndpointDescriptorPtr pED);
    IOReturn RemoveTDs(OHCIEndpointDescriptorPtr pED);
    IOReturn DoDoneQueueProcessing(OHCIGeneralTransferDescriptorPtr pHCDoneTD,
                                   IOUSBCompletionAction safeAction);
    void UIMProcessDoneQueue(IOUSBCompletionAction safeAction=0);
    void UIMRootHubStatusChange(void);
    void SimulateRootHubInt(
            UInt8					endpoint,
            IOMemoryDescriptor * 			buf,
            UInt32 					bufLen,
            IOUSBCompletion				completion);
    
    OHCIEndpointDescriptorPtr AddEmptyEndPoint(
            UInt8 					functionAddress,
            UInt8					endpointNumber,
            UInt16					maxPacketSize,
            UInt8					speed,
            UInt8					direction,
            OHCIEndpointDescriptorPtr			pED,
            OHCIEDFormat				format);

    OHCIEndpointDescriptorPtr FindEndpoint (
            short 					functionNumber,
            short 					endpointNumber,
            short 					direction,
            OHCIEndpointDescriptorPtr 			*pEDQueueBack,
            UInt32 					*controlMask);

    OHCIEndpointDescriptorPtr FindControlEndpoint (
            short 					functionNumber,
            short					endpointNumber,
            OHCIEndpointDescriptorPtr   		*pEDBack);

    OHCIEndpointDescriptorPtr FindBulkEndpoint (
            short 					functionNumber,
            short					endpointNumber,
            short					direction,
            OHCIEndpointDescriptorPtr			*pEDBack);

    OHCIEndpointDescriptorPtr FindIsochronousEndpoint(
            short 					functionNumber,
            short					endpointNumber,
            short 					direction,
            OHCIEndpointDescriptorPtr			*pEDBack);
            
    OHCIEndpointDescriptorPtr FindInterruptEndpoint(
            short 					functionNumber,
            short					endpointNumber,
            short					direction,
            OHCIEndpointDescriptorPtr			*pEDBack);

    
    void DoOptiFix(OHCIEndpointDescriptorPtr pIsochHead);
    void OptiLSHSFix(void);
    void dumpRegs(void);
    bool DetermineInterruptOffset(UInt32          pollingRate,
                            UInt32          reserveBandwidth,
                            int             *offset);
    void ReturnTransactions(
                OHCIGeneralTransferDescriptor 	*transaction,
                UInt32				tail);

public:
    bool init(OSDictionary * propTable);

    /*
     * UIM methods
     */
    IOReturn UIMInitialize(IOService * provider);
    IOReturn UIMFinalize();
    // Control
    virtual IOReturn UIMCreateControlEndpoint(
            UInt8				functionNumber,
            UInt8				endpointNumber,
            UInt16				maxPacketSize,
            UInt8				speed);

   virtual IOReturn UIMCreateControlTransfer(
            short				functionNumber,
            short				endpointNumber,
            IOUSBCompletion			completion,
            void *				CBP,
            bool				bufferRounding,
            UInt32				bufferSize,
            short				direction);

    virtual IOReturn UIMCreateControlTransfer(
            short				functionNumber,
            short				endpointNumber,
            IOUSBCompletion			completion,
            IOMemoryDescriptor *		CBP,
            bool				bufferRounding,
            UInt32				bufferSize,
            short				direction);

    // Bulk
    virtual IOReturn UIMCreateBulkEndpoint(
            UInt8				functionNumber,
            UInt8				endpointNumber,
            UInt8				direction,
            UInt8				speed,
            UInt8				maxPacketSize);

    virtual IOReturn UIMCreateBulkTransfer(
            short				functionNumber,
            short				endpointNumber,
            IOUSBCompletion			completion,
            IOMemoryDescriptor *		CBP,
            bool				bufferRounding,
            UInt32				bufferSize,
            short				direction);

    virtual IOReturn UIMCreateGeneralTransfer(
            OHCIEndpointDescriptorPtr		queue,
            IOUSBCompletion			completion,
            IOMemoryDescriptor *		CBP,
            UInt32				bufferSize,
            UInt32				flags,
            UInt32				type,
            UInt32				kickBits);

    // Interrupt
    virtual IOReturn UIMCreateInterruptEndpoint(
            short				functionAddress,
            short				endpointNumber,
            UInt8				direction,
            short				speed,
            UInt16				maxPacketSize,
            short				pollingRate);

    virtual IOReturn UIMCreateInterruptTransfer(
            short				functionNumber,
            short				endpointNumber,
            IOUSBCompletion			completion,
            IOMemoryDescriptor *		CBP,
            bool				bufferRounding,
            UInt32				bufferSize,
            short				direction);

    // Isoch
    virtual IOReturn UIMCreateIsochEndpoint(
            short				functionAddress,
            short				endpointNumber,
            UInt32				maxPacketSize,
            UInt8				direction);

    virtual IOReturn UIMCreateIsochTransfer(
	short				functionAddress,
	short				endpointNumber,
	IOUSBIsocCompletion		completion,
	UInt8				direction,
	UInt64				frameStart,
	IOMemoryDescriptor *		pBuffer,
	UInt32				frameCount,
	IOUSBIsocFrame			*pFrames);

    virtual IOReturn UIMAbortEndpoint(
            short				functionNumber,
            short				endpointNumber,
            short				direction);
    virtual IOReturn UIMDeleteEndpoint(
            short				functionNumber,
            short				endpointNumber,
            short				direction);
    virtual IOReturn UIMClearEndpointStall(
            short				functionNumber,
            short				endpointNumber,
            short				direction);
    /*
     * Root hub methods
     */
    IOReturn getRootHubDeviceDescriptor(IOUSBDeviceDescriptor *desc);
    IOReturn getRootHubDescriptor(IOUSBHubDescriptor *desc);
    IOReturn setRootHubDescriptor(OSData *buffer);
    IOReturn getRootHubConfDescriptor(OSData *desc);
    IOReturn getRootHubStatus(IOUSBHubStatus *status);
    IOReturn setRootHubFeature(UInt16 wValue);
    IOReturn clearRootHubFeature(UInt16 wValue);
    IOReturn getRootHubPortStatus(IOUSBHubPortStatus *status, UInt16 port);
    IOReturn setRootHubPortFeature(UInt16 wValue, UInt16 port);
    IOReturn clearRootHubPortFeature(UInt16 wValue, UInt16 port);
    IOReturn getRootHubPortState(UInt8 *state, UInt16 port);
    IOReturn setHubAddress(UInt16 wValue);

    void OHCIRootHubPower(bool on);
    void OHCIRootHubResetChangeConnection(UInt16 port);
    void OHCIRootHubResetResetChange(UInt16 port);
    void OHCIRootHubResetSuspendChange(UInt16 port);
    void OHCIRootHubResetEnableChange(UInt16 port);
    void OHCIRootHubResetOverCurrentChange(UInt16 port);
    void OHCIRootHubResetPort (UInt16 port);
    void OHCIRootHubPortEnable(UInt16 port, bool on);
    void OHCIRootHubPortSuspend(UInt16 port, bool on);
    void OHCIRootHubPortPower(UInt16 port, bool on);

    virtual UInt32 GetBandwidthAvailable();
    virtual UInt64 GetFrameNumber();
    virtual UInt32 GetFrameNumber32();

    virtual void pollInterrupts(IOUSBCompletionAction safeAction=0);

};

#endif /* _IOKIT_APPLEOHCI_H */
