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
 * Copyright (c) 1999 Apple Computer, Inc.  All rights reserved.
 *
 * HISTORY
 * 27 May 99 wgulland created.
 *
 */
#include <IOKit/assert.h>
#include <IOKit/IOSyncer.h>
#include <IOKit/firewire/IOFWCommand.h>
#include <IOKit/firewire/IOFireWireController.h>
#include <IOKit/firewire/IOFireWireNub.h>

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define super OSObject

OSDefineMetaClass( IOFWCommand, OSObject )
OSDefineAbstractStructors(IOFWCommand, OSObject)

bool IOFWCommand::initWithGate(IOCommandGate *gate)
{
    if(!super::init())
        return false;

    fGate = gate;
    return true;
}

IOReturn IOFWCommand::submit()
{
    IOReturn res;
    if(fSync) {
        fSyncWakeup = IOSyncer::create();
        if(!fSyncWakeup)
            return kIOReturnNoMemory;
    }
    fStatus = kIOReturnBusy;
    res = fGate->runCommand(this, 0, 0);
    if(res == kIOReturnBusy)
        res = kIOReturnSuccess;
    if(fSync) {
	if(res == kIOReturnSuccess)
            fSyncWakeup->wait();
	else {
            fSyncWakeup->release();
            fSyncWakeup->release();
            fSyncWakeup = NULL;
	}
    }
    return res;
}

IOReturn IOFWCommand::complete(IOReturn status)
{
    return fStatus = status;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#undef super
#define super IOFWCommand

OSDefineMetaClass( IOFWBusCommand, IOFWCommand )
OSDefineAbstractStructors(IOFWBusCommand, IOFWCommand)

bool IOFWBusCommand::initWithController(IOFireWireController *control, FWBusCallback completion, void *refcon)
{
    if(!super::initWithGate(control->getGate()))
	return false;

    fControl = control;
    fSync = completion == NULL;
    fComplete = completion;
    fRefCon = refcon;
    return true;
}

IOReturn IOFWBusCommand::reinit(FWBusCallback completion, void *refcon)
{
    if(fStatus == kIOReturnBusy)
	return fStatus;

    fSync = completion == NULL;
    fComplete = completion;
    fRefCon = refcon;
    return kIOReturnSuccess;
}


IOReturn IOFWBusCommand::complete(IOReturn state)
{
    state = super::complete(state);
    if(fSync)
        fSyncWakeup->signal();
    else if(fComplete)
	(*fComplete)(fRefCon, state, fControl, this);
    return state;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#undef super
#define super IOFWBusCommand

OSDefineMetaClassAndStructors(IOFWAllocAddressCommand, IOFWBusCommand)

bool IOFWAllocAddressCommand::initWithSpace(IOFireWireNub *device,
	IOFWAddressSpace *space, FWBusCallback completion, void *refcon)
{
    IOFireWireController *control = device->fControl;
    if(!super::initWithController(control, completion, refcon))
	return false;
    fSpace = space;
    return true;
}

IOReturn IOFWAllocAddressCommand::reinit(IOFWAddressSpace *space, 
				FWBusCallback completion, void *refcon)
{
    IOReturn res;
    res = super::reinit(completion, refcon);
    if(res != kIOReturnSuccess)
	return res;
    fSpace = space;
    return kIOReturnSuccess;
}

IOReturn IOFWAllocAddressCommand::execute()
{
    return complete(fControl->allocAddress(fSpace));
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#undef super
#define super IOFWBusCommand

OSDefineMetaClassAndStructors(IOFWDeallocAddressCommand, IOFWBusCommand)

bool IOFWDeallocAddressCommand::initWithSpace(IOFireWireNub *device,
	IOFWAddressSpace *space, FWBusCallback completion, void *refcon)
{
    IOFireWireController *control = device->fControl;
    if(!super::initWithController(control, completion, refcon))
	return false;
    fSpace = space;
    return true;
}

IOReturn IOFWDeallocAddressCommand::reinit(IOFWAddressSpace *space, 
				FWBusCallback completion, void *refcon)
{
    IOReturn res;
    res = super::reinit(completion, refcon);
    if(res != kIOReturnSuccess)
	return res;
    fSpace = space;
    return kIOReturnSuccess;
}

IOReturn IOFWDeallocAddressCommand::execute()
{
    fControl->freeAddress(fSpace);
    return complete(kIOReturnSuccess);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#undef super
#define super IOFWBusCommand

OSDefineMetaClassAndStructors(IOFWUpdateROM, IOFWBusCommand)

IOReturn IOFWUpdateROM::execute()
{
    IOReturn res;
    res = fControl->UpdateROM();
    if(res == kIOReturnSuccess)
        res = fControl->resetBus();
    return complete(res);
}

bool IOFWUpdateROM::initWithController(IOFireWireController *control, 
	FWBusCallback completion, void *refcon)
{
    return super::initWithController(control, completion, refcon);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
#undef super
#define super IOFWCommand

OSDefineMetaClass( IOFWAsyncCommand, IOFWCommand )
OSDefineAbstractStructors(IOFWAsyncCommand, IOFWCommand)

bool IOFWAsyncCommand::initAll(IOFireWireController *control,
	IOFireWireNub *device, FWAddress devAddress,
	IOMemoryDescriptor *hostMem, FWDeviceCallback completion,
	void *refcon, bool failOnReset)
{
    if(!super::initWithGate(control->getGate()))
	return false;
    fControl = control;
    fDevice = device;
    fMemDesc = hostMem;
    fComplete = completion;
    fRefCon = refcon;

    if(device) {
        fGeneration = device->fGeneration;
        fNodeID = device->fNodeID;
        fAddressHi = devAddress.addressHi;
        fAddressLo = devAddress.addressLo;
    }
    else {
        fGeneration = control->getGeneration();
        fNodeID = devAddress.nodeID;
        fAddressHi = devAddress.addressHi;
        fAddressLo = devAddress.addressLo;
    }
    if(hostMem)
        fSize = hostMem->getLength();
    fBytesTransferred = 0;
    fSpeed = fControl->FWSpeed(fNodeID);
    fMaxPack = 1 << fControl->maxPackLog(true, fNodeID);
    fFailOnReset = failOnReset;
    fSync = completion == NULL;
    fTrans = fControl->allocTrans();
    return fTrans != NULL;
}

IOReturn IOFWAsyncCommand::reinit(FWAddress devAddress, IOMemoryDescriptor *hostMem,
			FWDeviceCallback completion, void *refcon, bool failOnReset)
{
    if(fStatus == kIOReturnBusy)
	return fStatus;

    fComplete = completion;
    fRefCon = refcon;

    if(fDevice) {
        fGeneration = fDevice->fGeneration;
        fNodeID = fDevice->fNodeID;
        fAddressHi = devAddress.addressHi;
        fAddressLo = devAddress.addressLo;
    }
    else {
        fGeneration = fControl->getGeneration();
        fNodeID = devAddress.nodeID;
        fAddressHi = devAddress.addressHi;
        fAddressLo = devAddress.addressLo;
    }
    if(hostMem)
        fSize = hostMem->getLength();
    fBytesTransferred = 0;
    fSpeed = fControl->FWSpeed(fNodeID);
    fMaxPack = 1 << fControl->maxPackLog(true, fNodeID);
    fFailOnReset = failOnReset;
    fSync = completion == NULL;
    if(!fTrans) {
        fTrans = fControl->allocTrans();
        if(!fTrans)
            return fStatus = kIOReturnNoResources;
    }
    else
	fTrans->fHandler = NULL;	// Not in mid execution.
    return fStatus = kIOReturnSuccess;
}

IOReturn IOFWAsyncCommand::complete(IOReturn status)
{
    status = super::complete(status);
    if(fSync)
        fSyncWakeup->signal();
    else if(fComplete)
	(*fComplete)(fRefCon, status, fDevice, this);

    // The completion routine might have reinited and executed the command,
    // in which case don't free the transaction record.
    if(fTrans && fStatus != kIOReturnBusy) {
        fControl->freeTrans(fTrans);
        fTrans = NULL;
    }
    release();	// We are finished, release retain() from execute().
    return status;
}

void IOFWAsyncCommand::gotAck(int ackCode)
{
    int rcode;
    if(ackCode == kFWAckPending) {
	// This shouldn't happen.
	kprintf("Command 0x%x received Ack code %d\n", this, ackCode);
	return;
    }
    switch (ackCode) {
        case kFWAckComplete:
	rcode = kFWResponseComplete;
        break;

    // Device is still busy after several hardware retries - give up!
    case kFWAckBusyX:
    case kFWAckBusyA:
    case kFWAckBusyB:
    // Device isn't acking at all - give up!
    case kFWAckTimeout:
	rcode = kFWResponseAddressError;// Don't retry
        break;

    default:
        rcode = kFWResponseTypeError;	// Block transfers will try quad now
    }
    gotPacket(fControl, rcode, NULL, 0);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
#undef super
#define super IOFWAsyncCommand

OSDefineMetaClassAndStructors(IOFWReadCommand, IOFWAsyncCommand)

void IOFWReadCommand::gotPacket(IOFireWireController* control,
				int rcode, UInt8* data, int size)
{
    if(rcode != kFWResponseComplete) {
        //kprintf("Received rcode %d for read command 0x%x, nodeID %x\n", rcode, this, fNodeID);
        if(rcode == kFWResponseTypeError && fMaxPack > 4) {
            // try reading a quad at a time
            fMaxPack = 4;
            size = 0;
        }
        else {
            complete(kIOFireWireResponseBase+rcode);
            return;
        }
    }
    else {
        fMemDesc->writeBytes(fBytesTransferred, data, size);
        fSize -= size;
	fBytesTransferred += size;
    }

    if(fSize > 0) {
        int transfer;
        fAddressLo += size;
        if(!fControl->checkGeneration(fGeneration)) {
            complete(kIOFireWireBusReset);
        }

        transfer = fSize;
        if(transfer > fMaxPack)
            transfer = fMaxPack;

        fControl->asyncRead(fNodeID, fAddressHi,
                        fAddressLo, fSpeed, fTrans->fTCode, transfer, this);
    }
    else {
        complete(kIOReturnSuccess);
    }
}

bool IOFWReadCommand::initAll(IOFireWireNub *device, FWAddress devAddress,
	IOMemoryDescriptor *hostMem, FWDeviceCallback completion,
	void *refcon, bool failOnReset)
{
    if(!super::initAll(device->fControl, device, devAddress,
	hostMem, completion, refcon, failOnReset))
	return false;

    return true;

}

IOReturn IOFWReadCommand::reinit(FWAddress devAddress,
	IOMemoryDescriptor *hostMem,
	FWDeviceCallback completion, void *refcon, bool failOnReset)
{
    return super::reinit(devAddress,
	hostMem, completion, refcon, failOnReset);
}

IOReturn IOFWReadCommand::execute()
{
    int transfer;

    // Make sure object survives until complete() is called!
    retain();
    fStatus = kIOReturnBusy;

    // Do this when we're in execute, not before,
    // so that Reset handling knows which commands are waiting a response.
    fTrans->fHandler = this;

    if(!fControl->checkGeneration(fGeneration)) {
        return complete(kIOFireWireBusReset);
    }

    transfer = fSize;
    if(transfer > fMaxPack)
	transfer = fMaxPack;

    fControl->asyncRead(fNodeID, fAddressHi,
                    fAddressLo, fSpeed, fTrans->fTCode, transfer, this);

    return (kIOReturnBusy);
}
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
#undef super
#define super IOFWAsyncCommand

OSDefineMetaClassAndStructors(IOFWReadQuadCommand, IOFWAsyncCommand)
void IOFWReadQuadCommand::gotPacket(IOFireWireController* control,
				int rcode, UInt8* data, int size)
{
    if(rcode != kFWResponseComplete) {
        //kprintf("Received rcode %d for read command 0x%x, nodeID %x\n", rcode, this, fNodeID);
        if(rcode == kFWResponseTypeError && fMaxPack > 4) {
            // try reading a quad at a time
            fMaxPack = 4;
            size = 0;
        }
        else {
            complete(kIOFireWireResponseBase+rcode);
            return;
        }
    }
    else {
        int i;
        UInt32 *src = (UInt32 *)data;
        for(i=0; i<size/4; i++)
            *fQuads++ = *src++;
        fSize -= size;
    }

    if(fSize > 0) {
        int transfer;

        fAddressLo += size;
        if(!fControl->checkGeneration(fGeneration)) {
            complete(kIOFireWireBusReset);
        }
        transfer = fSize;
        if(transfer > fMaxPack)
            transfer = fMaxPack;

        fControl->asyncRead(fNodeID, fAddressHi,
                        fAddressLo, fSpeed, fTrans->fTCode, transfer, this);
    }
    else {
        complete(kIOReturnSuccess);
    }
}

bool IOFWReadQuadCommand::initAll(IOFireWireController *control, 
	IOFireWireNub *device, FWAddress devAddress,
	UInt32 *quads, int numQuads, FWDeviceCallback completion,
	void *refcon, bool failOnReset)
{
    if(!super::initAll(control, device, devAddress,
	NULL, completion, refcon, failOnReset))
	return false;
    fSize = 4*numQuads;
    fQuads = quads;
    return true;
}

IOReturn IOFWReadQuadCommand::reinit(FWAddress devAddress,
	UInt32 *quads, int numQuads, FWDeviceCallback completion,
					void *refcon, bool failOnReset)
{
    IOReturn res;
    res = super::reinit(devAddress,
	NULL, completion, refcon, failOnReset);
    if(res != kIOReturnSuccess)
	return res;

    fSize = 4*numQuads;
    fQuads = quads;
    return res;
}

IOReturn IOFWReadQuadCommand::execute()
{
    int transfer;

    // Make sure object survives until complete() is called!
    retain();
    fStatus = kIOReturnBusy;

    // Do this when we're in execute, not before,
    // so that Reset handling knows which commands are waiting a response.
    fTrans->fHandler = this;

    if(!fControl->checkGeneration(fGeneration)) {
        return complete(kIOFireWireBusReset);
    }
    transfer = fSize;
    if(transfer > fMaxPack)
	transfer = fMaxPack;

    fControl->asyncRead(fNodeID, fAddressHi,
                    fAddressLo, fSpeed, fTrans->fTCode, transfer, this);

    return (kIOReturnBusy);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
#undef super
#define super IOFWAsyncCommand

OSDefineMetaClassAndStructors(IOFWWriteCommand, IOFWAsyncCommand)

void IOFWWriteCommand::gotPacket(IOFireWireController* control,
				int rcode, UInt8* data, int size)
{
    if(rcode != kFWResponseComplete) {
        //kprintf("Received rcode %d for command 0x%x\n", rcode, this);
        if(rcode == kFWResponseTypeError && fMaxPack > 4) {
            // try writing a quad at a time
            fMaxPack = 4;
            fPackSize = 0;
        }
        else {
            complete(kIOFireWireResponseBase+rcode);
            return;
        }
    }
    else {
	fBytesTransferred += fPackSize;
        fSize -= fPackSize;
    }

    if(fSize > 0) {
        fAddressLo += fPackSize;

        if(!fControl->checkGeneration(fGeneration)) {
            complete(kIOFireWireBusReset);
        }

        fPackSize = fSize;
        if(fPackSize > fMaxPack)
            fPackSize = fMaxPack;

        fControl->asyncWrite(fNodeID, fAddressHi, fAddressLo, fSpeed,
            fTrans->fTCode, fMemDesc, fPackSize, this);

    }
    else {
        complete(kIOReturnSuccess);
    }
}

bool IOFWWriteCommand::initAll(IOFireWireNub *device, FWAddress devAddress,
	IOMemoryDescriptor *hostMem, FWDeviceCallback completion,
	void *refcon, bool failOnReset)
{
    if(!super::initAll(device->fControl, device, devAddress,
	hostMem, completion, refcon, failOnReset))
	return false;
    return true;
}

IOReturn IOFWWriteCommand::reinit(FWAddress devAddress,
	IOMemoryDescriptor *hostMem,
	FWDeviceCallback completion, void *refcon, bool failOnReset)
{
    return super::reinit(devAddress,
	hostMem, completion, refcon, failOnReset);
}

IOReturn IOFWWriteCommand::execute()
{
    // Make sure object survives until complete() is called!
    retain();
    fStatus = kIOReturnBusy;

    // Do this when we're in execute, not before,
    // so that Reset handling knows which commands are waiting a response.
    fTrans->fHandler = this;

    if(!fControl->checkGeneration(fGeneration)) {
        return complete(kIOFireWireBusReset);
    }

    fPackSize = fSize;
    if(fPackSize > fMaxPack)
	fPackSize = fMaxPack;

    fControl->asyncWrite(fNodeID, fAddressHi, fAddressLo, fSpeed, 
	fTrans->fTCode, fMemDesc, fPackSize, this);

    return (kIOReturnBusy);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
#undef super
#define super IOFWAsyncCommand

OSDefineMetaClassAndStructors(IOFWWriteQuadCommand, IOFWAsyncCommand)
void IOFWWriteQuadCommand::gotPacket(IOFireWireController* control,
				int rcode, UInt8* data, int size)
{
    if(rcode != kFWResponseComplete) {
        //kprintf("Received rcode %d for command 0x%x\n", rcode, this);
        if(rcode == kFWResponseTypeError && fMaxPack > 4) {
            // try writing a quad at a time
            fMaxPack = 4;
            fPackSize = 0;
        }
        else {
            complete(kIOFireWireResponseBase+rcode);
            return;
        }
    }
    else {
        fQPtr += fPackSize/4;
        fSize -= fPackSize;
    }

    if(fSize > 0) {
        fAddressLo += fPackSize;

        if(!fControl->checkGeneration(fGeneration)) {
            complete(kIOFireWireBusReset);
        }

        fPackSize = fSize;
        if(fPackSize > fMaxPack)
            fPackSize = fMaxPack;

        fControl->asyncWrite(fNodeID, fAddressHi, fAddressLo,
            fSpeed, fTrans->fTCode, fQPtr, fPackSize, this);
    }
    else {
        complete(kIOReturnSuccess);
    }
}

bool IOFWWriteQuadCommand::initAll(IOFireWireController *control, 
	IOFireWireNub *device, FWAddress devAddress,
	UInt32 *quads, int numQuads, FWDeviceCallback completion,
	void *refcon, bool failOnReset)
{
    int i;
    if(numQuads > 2)
	return false;

    if(!super::initAll(control, device, devAddress,
	NULL, completion, refcon, failOnReset))
	return false;
    fSize = 4*numQuads;
    for(i=0; i<numQuads; i++)
        fQuads[i] = *quads++;
    fQPtr = fQuads;
    return true;
}

IOReturn IOFWWriteQuadCommand::reinit(FWAddress devAddress,
	UInt32 *quads, int numQuads, FWDeviceCallback completion,
					void *refcon, bool failOnReset)
{
    IOReturn res;
    int i;
    if(numQuads > 2)
	return kIOReturnUnsupported;
    res = super::reinit(devAddress,
	NULL, completion, refcon, failOnReset);
    if(res != kIOReturnSuccess)
	return res;

    fSize = 4*numQuads;
    for(i=0; i<numQuads; i++)
        fQuads[i] = *quads++;
    fQPtr = fQuads;
    return res;
}

IOReturn IOFWWriteQuadCommand::execute()
{
    // Make sure object survives until complete() is called!
    retain();
    fStatus = kIOReturnBusy;

    // Do this when we're in execute, not before,
    // so that Reset handling knows which commands are waiting a response.
    fTrans->fHandler = this;

    if(!fControl->checkGeneration(fGeneration)) {
        return complete(kIOFireWireBusReset);
    }

    fPackSize = fSize;
    if(fPackSize > fMaxPack)
	fPackSize = fMaxPack;

    fControl->asyncWrite(fNodeID, fAddressHi, fAddressLo,
	fSpeed, fTrans->fTCode, fQPtr, fPackSize, this);

    return (kIOReturnBusy);
}


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
#undef super
#define super IOFWAsyncCommand

OSDefineMetaClassAndStructors(IOFWCompareAndSwapCommand, IOFWAsyncCommand)

void IOFWCompareAndSwapCommand::gotPacket(IOFireWireController* control,
				int rcode, UInt8* data, int size)
{
    int i;
    if(rcode != kFWResponseComplete) {
        kprintf("Received rcode %d for lock command 0x%x, nodeID %x\n", rcode, this, fNodeID);
        complete(kIOFireWireResponseBase+rcode);
        return;
    }
    for(i=0; i<size/4; i++) {
	fOldVal[i] = ((UInt32 *)data)[i];
    }
    complete(kIOReturnSuccess);
}

bool IOFWCompareAndSwapCommand::initAll(IOFireWireNub *device, FWAddress devAddress,
	const UInt32 *cmpVal, const UInt32 *newVal, int size, FWDeviceCallback completion,
	void *refcon, bool failOnReset)
{
    int i;
    if(!super::initAll(device->fControl, device, devAddress,
	NULL, completion, refcon, failOnReset))
	return false;
    for(i=0; i<size; i++) {
        fInputVals[i] = cmpVal[i];
        fInputVals[size+i] = newVal[i];
    }
    fSize = 8*size;
    return true;
}

IOReturn IOFWCompareAndSwapCommand::execute()
{
    // Make sure object survives until complete() is called!
    retain();
    if(!fControl->checkGeneration(fGeneration)) {
        return complete(kIOFireWireBusReset);
    }

    fStatus = kIOReturnBusy;

    // Do this when we're in execute, not before,
    // so that Reset handling knows which commands are waiting a response.
    fTrans->fHandler = this;

    fControl->asyncLock(fNodeID, fAddressHi, fAddressLo, fSpeed,
		fTrans->fTCode, kFWExtendedTCodeCompareSwap,
		fInputVals, fSize, this);

    return (kIOReturnBusy);
}

bool IOFWCompareAndSwapCommand::locked(UInt32 *oldVal)
{
    int i;
    bool ret = true;
    for(i = 0; i<fSize/8; i++) {
        ret = ret && (fInputVals[i] == fOldVal[i]);
	oldVal[i] = fOldVal[i];
    }
    return ret;
}
