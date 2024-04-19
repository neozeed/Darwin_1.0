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
 * 16 Mar 1999 wgulland created.
 *
 */
#include <libkern/OSByteOrder.h>

#include <IOKit/assert.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/usb/IOUSBDevice.h>
#include <IOKit/usb/IOUSBInterface.h>
#include <IOKit/usb/IOUSBPipe.h>

#include "IOUSBUserClient.h"

#define super IOUserClient
#define DEBUGGING_LEVEL 0

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

OSDefineMetaClass( IOUSBUserClient, IOUserClient )
OSDefineAbstractStructors(IOUSBUserClient, IOUserClient)

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool IOUSBUserClient::start( IOService * provider )
{
    IOUSBNub *owner = OSDynamicCast(IOUSBNub, provider);

    assert(owner);
    if(!super::start(provider))
        return false;

    fPipes[0] = owner->pipeZero();
    if(!fPipes[0])
	return false;

    // Initialize the call structures valid for IOUSBNub
    fMethods[kUSBSetConfig].object = NULL;
    fMethods[kUSBSetConfig].func = NULL;
    fMethods[kUSBSetConfig].count0 = 1;
    fMethods[kUSBSetConfig].count1 = 0;
    fMethods[kUSBSetConfig].flags = kIOUCScalarIScalarO;

    fMethods[kUSBGetConfig].object = provider;
    fMethods[kUSBGetConfig].func = (IOMethod)&IOUSBNub::GetConfiguration;
    fMethods[kUSBGetConfig].count0 = 0;
    fMethods[kUSBGetConfig].count1 = 1;
    fMethods[kUSBGetConfig].flags = kIOUCScalarIScalarO;

    fMethods[kUSBGetConfigDescriptor].object = NULL;
    fMethods[kUSBGetConfigDescriptor].func = NULL;
    fMethods[kUSBGetConfigDescriptor].count0 = 1;
    fMethods[kUSBGetConfigDescriptor].count1 = 0xffffffff; // variable
    fMethods[kUSBGetConfigDescriptor].flags = kIOUCScalarIStructO;

    fMethods[kUSBControlReqOut].object = this;
    fMethods[kUSBControlReqOut].func =
        (IOMethod)&IOUSBUserClient::ControlReqOut;
    fMethods[kUSBControlReqOut].count0 = 4;
    fMethods[kUSBControlReqOut].count1 = 0xffffffff; // variable
    fMethods[kUSBControlReqOut].flags = kIOUCScalarIStructI;

    fMethods[kUSBControlReqIn].object = this;
    fMethods[kUSBControlReqIn].func =
        (IOMethod)&IOUSBUserClient::ControlReqIn;
    fMethods[kUSBControlReqIn].count0 = 4;
    fMethods[kUSBControlReqIn].count1 = 0xffffffff; // variable
    fMethods[kUSBControlReqIn].flags = kIOUCScalarIStructO;

    fMethods[kUSBOpenPipe].object = NULL;
    fMethods[kUSBOpenPipe].func = NULL;
    fMethods[kUSBOpenPipe].count0 = 4;
    fMethods[kUSBOpenPipe].count1 = 1;
    fMethods[kUSBOpenPipe].flags = kIOUCScalarIScalarO;

    fMethods[kUSBClosePipe].object = this;
    fMethods[kUSBClosePipe].func =
        (IOMethod)&IOUSBUserClient::ClosePipe;
    fMethods[kUSBClosePipe].count0 = 1;
    fMethods[kUSBClosePipe].count1 = 0;
    fMethods[kUSBClosePipe].flags = kIOUCScalarIScalarO;

    fMethods[kUSBReadPipe].object = this;
    fMethods[kUSBReadPipe].func =
        (IOMethod)&IOUSBUserClient::ReadPipe;
    fMethods[kUSBReadPipe].count0 = 1;
    fMethods[kUSBReadPipe].count1 = 0xffffffff; // variable
    fMethods[kUSBReadPipe].flags = kIOUCScalarIStructO;

    fMethods[kUSBWritePipe].object = this;
    fMethods[kUSBWritePipe].func =
        (IOMethod)&IOUSBUserClient::WritePipe;
    fMethods[kUSBWritePipe].count0 = 1;
    fMethods[kUSBWritePipe].count1 = 0xffffffff; // variable
    fMethods[kUSBWritePipe].flags = kIOUCScalarIStructI;

    fMethods[kUSBGetPipeStatus].object = this;
    fMethods[kUSBGetPipeStatus].func =
        (IOMethod)&IOUSBUserClient::getPipeStatus;
    fMethods[kUSBGetPipeStatus].count0 = 1;
    fMethods[kUSBGetPipeStatus].count1 = 0;
    fMethods[kUSBGetPipeStatus].flags = kIOUCScalarIScalarO;

    fMethods[kUSBAbortPipe].object = this;
    fMethods[kUSBAbortPipe].func =
        (IOMethod)&IOUSBUserClient::abortPipe;
    fMethods[kUSBAbortPipe].count0 = 1;
    fMethods[kUSBAbortPipe].count1 = 0;
    fMethods[kUSBAbortPipe].flags = kIOUCScalarIScalarO;

    fMethods[kUSBResetPipe].object = this;
    fMethods[kUSBResetPipe].func =
        (IOMethod)&IOUSBUserClient::resetPipe;
    fMethods[kUSBResetPipe].count0 = 1;
    fMethods[kUSBResetPipe].count1 = 0;
    fMethods[kUSBResetPipe].flags = kIOUCScalarIScalarO;

    fMethods[kUSBSetPipeIdle].object = this;
    fMethods[kUSBSetPipeIdle].func =
        (IOMethod)&IOUSBUserClient::setPipeIdle;
    fMethods[kUSBSetPipeIdle].count0 = 1;
    fMethods[kUSBSetPipeIdle].count1 = 0;
    fMethods[kUSBSetPipeIdle].flags = kIOUCScalarIScalarO;

    fMethods[kUSBSetPipeActive].object = this;
    fMethods[kUSBSetPipeActive].func =
        (IOMethod)&IOUSBUserClient::setPipeActive;
    fMethods[kUSBSetPipeActive].count0 = 1;
    fMethods[kUSBSetPipeActive].count1 = 0;
    fMethods[kUSBSetPipeActive].flags = kIOUCScalarIScalarO;

    fMethods[kUSBClearPipeStall].object = this;
    fMethods[kUSBClearPipeStall].func =
        (IOMethod)&IOUSBUserClient::clearPipeStall;
    fMethods[kUSBClearPipeStall].count0 = 1;
    fMethods[kUSBClearPipeStall].count1 = 0;
    fMethods[kUSBClearPipeStall].flags = kIOUCScalarIScalarO;

    fMethods[kUSBReadPipeOOL].object = this;
    fMethods[kUSBReadPipeOOL].func =
        (IOMethod)&IOUSBUserClient::ReadPipeOOL;
    fMethods[kUSBReadPipeOOL].count0 = 3;
    fMethods[kUSBReadPipeOOL].count1 = 1;
    fMethods[kUSBReadPipeOOL].flags = kIOUCScalarIScalarO;

    fAsyncMethods[kUSBReadPipeOOL].object = this;
    fAsyncMethods[kUSBReadPipeOOL].func =
        (IOAsyncMethod)&IOUSBUserClient::ReadPipeAsync;
    fAsyncMethods[kUSBReadPipeOOL].count0 = 3;
    fAsyncMethods[kUSBReadPipeOOL].count1 = 0;
    fAsyncMethods[kUSBReadPipeOOL].flags = kIOUCScalarIScalarO;

    fMethods[kUSBWritePipeOOL].object = this;
    fMethods[kUSBWritePipeOOL].func =
        (IOMethod)&IOUSBUserClient::WritePipeOOL;
    fMethods[kUSBWritePipeOOL].count0 = 3;
    fMethods[kUSBWritePipeOOL].count1 = 0;
    fMethods[kUSBWritePipeOOL].flags = kIOUCScalarIScalarO;

    fAsyncMethods[kUSBWritePipeOOL].object = this;
    fAsyncMethods[kUSBWritePipeOOL].func =
        (IOAsyncMethod)&IOUSBUserClient::WritePipeAsync;
    fAsyncMethods[kUSBWritePipeOOL].count0 = 3;
    fAsyncMethods[kUSBWritePipeOOL].count1 = 0;
    fAsyncMethods[kUSBWritePipeOOL].flags = kIOUCScalarIScalarO;

    fMethods[kUSBControlReqInOOL].object = this;
    fMethods[kUSBControlReqInOOL].func =
        (IOMethod)&IOUSBUserClient::ControlReqInOOL;
    fMethods[kUSBControlReqInOOL].count0 = sizeof(DevReqOOL);
    fMethods[kUSBControlReqInOOL].count1 = sizeof(UInt32);
    fMethods[kUSBControlReqInOOL].flags = kIOUCStructIStructO;

    fAsyncMethods[kUSBControlReqInOOL].object = this;
    fAsyncMethods[kUSBControlReqInOOL].func =
        (IOAsyncMethod)&IOUSBUserClient::ControlReqInAsync;
    fAsyncMethods[kUSBControlReqInOOL].count0 = sizeof(DevReqOOL);
    fAsyncMethods[kUSBControlReqInOOL].count1 = 0;
    fAsyncMethods[kUSBControlReqInOOL].flags = kIOUCStructIStructO;

    fMethods[kUSBControlReqOutOOL].object = this;
    fMethods[kUSBControlReqOutOOL].func =
        (IOMethod)&IOUSBUserClient::ControlReqOutOOL;
    fMethods[kUSBControlReqOutOOL].count0 = sizeof(DevReqOOL);
    fMethods[kUSBControlReqOutOOL].count1 = 0;
    fMethods[kUSBControlReqOutOOL].flags = kIOUCStructIStructO;

    fAsyncMethods[kUSBControlReqOutOOL].object = this;
    fAsyncMethods[kUSBControlReqOutOOL].func =
        (IOAsyncMethod)&IOUSBUserClient::ControlReqOutAsync;
    fAsyncMethods[kUSBControlReqOutOOL].count0 = sizeof(DevReqOOL);
    fAsyncMethods[kUSBControlReqOutOOL].count1 = 0;
    fAsyncMethods[kUSBControlReqOutOOL].flags = kIOUCStructIStructO;


    fMethods[kUSBResetDevice].object = NULL;
    fMethods[kUSBResetDevice].func = NULL;
    fMethods[kUSBResetDevice].count0 = 0;
    fMethods[kUSBResetDevice].count1 = 0;
    fMethods[kUSBResetDevice].flags = kIOUCScalarIScalarO;

    return true;
}

IOExternalMethod *
IOUSBUserClient::getExternalMethodForIndex( UInt32 index )
{
    if(index >= kNumUSBMethods || fMethods[index].object == NULL)
    	return NULL;
    else
        return &fMethods[index];
}

IOExternalAsyncMethod *
IOUSBUserClient::getExternalAsyncMethodForIndex( UInt32 index )
{
    if(index >= kNumUSBMethods || fAsyncMethods[index].object == NULL)
        return NULL;
    else
        return &fAsyncMethods[index];
}

/*
 * There's a limit of max 6 arguments to user client methods, so the type, recipient and request
 * are packed into one 16 bit integer.
 */
IOReturn
IOUSBUserClient::ControlReqIn(UInt32 pipe, UInt16 bmreqtypeRequest, UInt16 wValue,
		UInt16 wIndex, void *buf, UInt32 *size)
{
    IOReturn res;
    IOUSBDevRequest	req;
    IOUSBPipe *pipeObj;

    pipeObj = fPipes[pipe];
    if(!pipeObj)
        return kIOUSBUnknownPipeErr;

    req.rqDirection = kUSBIn;
    req.rqType = (bmreqtypeRequest >> kUSBRqTypeShift) & kUSBRqTypeMask;
    req.rqRecipient = bmreqtypeRequest & kUSBRqRecipientMask;
    req.bRequest = bmreqtypeRequest >> 8;
    OSWriteLittleInt16(&req.wValue, 0, wValue);
    OSWriteLittleInt16(&req.wIndex, 0, wIndex);
    OSWriteLittleInt16(&req.wLength, 0, *size);
    req.pData = buf;
    res = pipeObj->controlRequest(&req);

    if(res == kIOReturnSuccess) {
        *size = req.wLenDone;
    }
    else {
        IOLog("IOUSBUserClient::ControlReqIn err:0x%x\n", res);
        *size = 0;
    }
    return res;
}

/*
 * There's a limit of max 6 arguments to user client methods, so the type, recipient and request
 * are packed into one 16 bit integer.
 */
IOReturn
IOUSBUserClient::ControlReqOut(UInt32 pipe, UInt16 bmreqtypeRequest, UInt16 wValue,
                UInt16 wIndex, void *buf, UInt32 size)
{
    IOReturn res;
    IOUSBDevRequest	req;
    IOUSBPipe *pipeObj;

    pipeObj = fPipes[pipe];
    if(!pipeObj)
        return kIOUSBUnknownPipeErr;

    req.rqDirection = kUSBOut;
    req.rqType = (bmreqtypeRequest >> kUSBRqTypeShift) & kUSBRqTypeMask;
    req.rqRecipient = bmreqtypeRequest & kUSBRqRecipientMask;
    req.bRequest = bmreqtypeRequest >> 8;
    OSWriteLittleInt16(&req.wValue, 0, wValue);
    OSWriteLittleInt16(&req.wIndex, 0, wIndex);
    OSWriteLittleInt16(&req.wLength, 0, size);
    req.pData = buf;
    res = pipeObj->controlRequest(&req);

    if(kIOReturnSuccess != res) {
        IOLog("IOUSBUserClient::ControlReqOut err:0x%x\n", res);
    }
    return res;
}

IOReturn
IOUSBUserClient::ControlReqInOOL(DevReqOOL *reqIn, UInt32 *sizeOut, IOByteCount inCount,
                                IOByteCount *outCount)
{
    IOReturn 			res;
    IOUSBDevRequestDesc		req;
    IOMemoryDescriptor *	mem;
    IOUSBPipe *			pipeObj;

    if(inCount != sizeof(DevReqOOL) ||
       *outCount != sizeof(UInt32))
        return kIOReturnBadArgument;

    pipeObj = fPipes[reqIn->pipe];
    if(!pipeObj)
        return kIOUSBUnknownPipeErr;

    mem = IOMemoryDescriptor::withAddress((vm_address_t)reqIn->buf, reqIn->sizeIn,
                                          kIODirectionIn,
                                          fTask);
    if(!mem) {
        return kIOReturnNoMemory;
    }
    req.rqDirection = kUSBIn;
    req.rqType = (reqIn->bmreqtype >> kUSBRqTypeShift) & kUSBRqTypeMask;
    req.rqRecipient = reqIn->bmreqtype & kUSBRqRecipientMask;
    req.bRequest = reqIn->request;
    OSWriteLittleInt16(&req.wValue, 0, reqIn->wValue);
    OSWriteLittleInt16(&req.wIndex, 0, reqIn->wIndex);
    OSWriteLittleInt16(&req.wLength, 0, reqIn->sizeIn);
    req.pData = mem;

    res = mem->prepare();
    if(res == kIOReturnSuccess)
        res = pipeObj->controlRequest(&req);
    mem->complete();
    mem->release();
    if(res == kIOReturnSuccess) {
        *sizeOut = req.wLenDone;
    }
    else {
        IOLog("IOUSBUserClient::ControlReqInOOL err:0x%x\n", res);
        *sizeOut = 0;
    }
    *outCount = sizeof(UInt32);

    return res;
}

typedef struct AsyncPB {
    OSAsyncReference fAsyncRef;
    UInt32 fMax;
    IOMemoryDescriptor *fMem;
};

IOReturn
IOUSBUserClient::ControlReqInAsync(OSAsyncReference asyncRef,
                                   DevReqOOL *reqIn, IOByteCount inCount)
{
    IOReturn 			res;
    IOUSBDevRequestDesc		req;
    IOUSBPipe *			pipeObj;

    IOUSBCompletion		tap;
    AsyncPB * 			pb = NULL;
    IOMemoryDescriptor *	mem = NULL;

    if(inCount != sizeof(DevReqOOL))
        return kIOReturnBadArgument;

    pipeObj = fPipes[reqIn->pipe];
    if(!pipeObj)
        return kIOUSBUnknownPipeErr;

    do {
        mem = IOMemoryDescriptor::withAddress((vm_address_t)reqIn->buf,
                                            reqIn->sizeIn,
                                            kIODirectionIn,
                                            fTask);
        if(!mem) {
            res = kIOReturnNoMemory;
            break;
        }
        res = mem->prepare();
        if(res != kIOReturnSuccess)
            break;
        pb = (AsyncPB *)IOMalloc(sizeof(AsyncPB));
        if(!pb) {
            res = kIOReturnNoMemory;
            break;
        }

        req.rqDirection = kUSBIn;
        req.rqType = (reqIn->bmreqtype >> kUSBRqTypeShift) & kUSBRqTypeMask;
        req.rqRecipient = reqIn->bmreqtype & kUSBRqRecipientMask;
        req.bRequest = reqIn->request;
        OSWriteLittleInt16(&req.wValue, 0, reqIn->wValue);
        OSWriteLittleInt16(&req.wIndex, 0, reqIn->wIndex);
        OSWriteLittleInt16(&req.wLength, 0, reqIn->sizeIn);
        req.pData = mem;

        bcopy(asyncRef, pb->fAsyncRef, sizeof(OSAsyncReference));
        pb->fMax = reqIn->sizeIn;
        pb->fMem = mem;
        tap.target = this;
        tap.action = &ReqComplete;
        tap.parameter = pb;
        res = pipeObj->controlRequest(&req, &tap);

    } while (false);
    
    if(res != kIOReturnSuccess) {
        IOLog("ControlReqInAsync err 0x%x\n", res);
        if(mem) {
            mem->complete();
            mem->release();
        }
        if(pb)
            IOFree(pb, sizeof(*pb));
    }

    return res;
}

void
IOUSBUserClient::ReqComplete(void *obj, void *param, IOReturn res, UInt32 remaining)
{
    void *	args[1];
    AsyncPB * pb = (AsyncPB *)param;

    if(res == kIOReturnSuccess) {
        args[0] = (void *)(pb->fMax - remaining);
    }
    else {
        IOLog("IOUSBUserClient::ReqComplete err:0x%x\n", res);
        args[0] = 0;
    }
    pb->fMem->complete();
    pb->fMem->release();
    sendAsyncResult(pb->fAsyncRef, res, args, 1);

    IOFree(pb, sizeof(*pb));
}

IOReturn
IOUSBUserClient::ControlReqOutOOL(DevReqOOL *reqIn, IOByteCount inCount)
{
    IOReturn 			res;
    IOUSBDevRequestDesc		req;
    IOMemoryDescriptor *	mem;
    IOUSBPipe *			pipeObj;

    if(inCount != sizeof(DevReqOOL))
        return kIOReturnBadArgument;

    pipeObj = fPipes[reqIn->pipe];
    if(!pipeObj)
        return kIOUSBUnknownPipeErr;


    mem = IOMemoryDescriptor::withAddress((vm_address_t)reqIn->buf,
		reqIn->sizeIn, kIODirectionOut, fTask);
    if(!mem) {
        return kIOReturnNoMemory;
    }
    req.rqDirection = kUSBOut;
    req.rqType = (reqIn->bmreqtype >> kUSBRqTypeShift) & kUSBRqTypeMask;
    req.rqRecipient = reqIn->bmreqtype & kUSBRqRecipientMask;
    req.bRequest = reqIn->request;
    OSWriteLittleInt16(&req.wValue, 0, reqIn->wValue);
    OSWriteLittleInt16(&req.wIndex, 0, reqIn->wIndex);
    OSWriteLittleInt16(&req.wLength, 0, reqIn->sizeIn);
    req.pData = mem;

    res = mem->prepare();
    if(res == kIOReturnSuccess)
        res = pipeObj->controlRequest(&req);
    mem->complete();
    mem->release();
    return res;
}

IOReturn
IOUSBUserClient::ControlReqOutAsync(OSAsyncReference asyncRef,
                    DevReqOOL *reqIn, IOByteCount inCount)
{
    IOReturn 			res;
    IOUSBDevRequestDesc		req;
    IOUSBPipe *			pipeObj;

    IOUSBCompletion		tap;
    AsyncPB * 			pb = NULL;
    IOMemoryDescriptor *	mem = NULL;

    if(inCount != sizeof(DevReqOOL))
        return kIOReturnBadArgument;

    pipeObj = fPipes[reqIn->pipe];
    if(!pipeObj)
        return kIOUSBUnknownPipeErr;

    do {
        mem = IOMemoryDescriptor::withAddress((vm_address_t)reqIn->buf,
                                            reqIn->sizeIn,
                                            kIODirectionOut,
                                            fTask);
        if(!mem) {
            res = kIOReturnNoMemory;
            break;
        }
        res = mem->prepare();
        if(res != kIOReturnSuccess)
            break;
        pb = (AsyncPB *)IOMalloc(sizeof(AsyncPB));
        if(!pb) {
            res = kIOReturnNoMemory;
            break;
        }

        req.rqDirection = kUSBOut;
        req.rqType = (reqIn->bmreqtype >> kUSBRqTypeShift) & kUSBRqTypeMask;
        req.rqRecipient = reqIn->bmreqtype & kUSBRqRecipientMask;
        req.bRequest = reqIn->request;
        OSWriteLittleInt16(&req.wValue, 0, reqIn->wValue);
        OSWriteLittleInt16(&req.wIndex, 0, reqIn->wIndex);
        OSWriteLittleInt16(&req.wLength, 0, reqIn->sizeIn);
        req.pData = mem;

        bcopy(asyncRef, pb->fAsyncRef, sizeof(OSAsyncReference));
        pb->fMax = reqIn->sizeIn;
        pb->fMem = mem;
        tap.target = this;
        tap.action = &ReqComplete;	// Want same number of reply args as for ControlReqIn
        tap.parameter = pb;
        res = pipeObj->controlRequest(&req, &tap);

    } while (false);

    if(res != kIOReturnSuccess) {
        IOLog("controlRequest err 0x%x\n", res);
        if(mem) {
            mem->complete();
            mem->release();
        }
        if(pb)
            IOFree(pb, sizeof(*pb));
    }

    return res;
}


// Controlling pipe state

IOReturn IOUSBUserClient::getPipeStatus(UInt32 pipe)
{
    IOUSBPipe *pipeObj;
    pipeObj = fPipes[pipe];
    if(!pipeObj)
        return kIOUSBUnknownPipeErr;
    return pipeObj->status();
}

IOReturn IOUSBUserClient::abortPipe(UInt32 pipe)
{
    IOUSBPipe *pipeObj;
    pipeObj = fPipes[pipe];
    if(!pipeObj)
        return kIOUSBUnknownPipeErr;
    return pipeObj->abort();
}

IOReturn IOUSBUserClient::resetPipe(UInt32 pipe)
{
    IOUSBPipe *pipeObj;
    pipeObj = fPipes[pipe];
    if(!pipeObj)
        return kIOUSBUnknownPipeErr;
    return pipeObj->reset();
}

IOReturn IOUSBUserClient::setPipeIdle(UInt32 pipe)
{
    return(0);
}

IOReturn IOUSBUserClient::setPipeActive(UInt32 pipe)
{
    return(0);
}

IOReturn IOUSBUserClient::clearPipeStall(UInt32 pipe)
{
    IOUSBPipe *pipeObj;
    pipeObj = fPipes[pipe];
    if(!pipeObj)
        return kIOUSBUnknownPipeErr;
    return pipeObj->clearStall();
}

IOReturn
IOUSBUserClient::ReadPipe(UInt32 pipe, void *buf, UInt32 *size)
{
    IOReturn 			res;
    IOMemoryDescriptor *	mem;
    IOUSBPipe *			pipeObj;

    pipeObj = fPipes[pipe];
    if(!pipeObj)
        return kIOUSBUnknownPipeErr;

    mem = IOMemoryDescriptor::withAddress(buf, *size, kIODirectionIn);
    if(!mem) {
        *size = 0;
        return kIOReturnNoMemory;
    }
    res = pipeObj->read(mem, 0, size);
    if(kIOReturnSuccess != res)
        *size = 0;
    mem->release();

    return res;
}

IOReturn
IOUSBUserClient::WritePipe(UInt32 pipe, void *buf, UInt32 size)
{
    IOReturn 			res;
    IOMemoryDescriptor *	mem;
    IOUSBPipe *			pipeObj;

    pipeObj = fPipes[pipe];
    if(!pipeObj)
        return kIOUSBUnknownPipeErr;

    mem = IOMemoryDescriptor::withAddress(buf, size, kIODirectionOut);
    if(!mem) {
        res = kIOReturnNoMemory;
    }
    else {
        res = pipeObj->write(mem);
        mem->release();
    }
    return res;
}

/*
 * Out of line version of read pipe - the buffer isn't mapped (yet) into the kernel task
 */
IOReturn
IOUSBUserClient::ReadPipeOOL(UInt32 pipe, void *buf, UInt32 sizeIn, UInt32 *sizeOut)
{
    IOReturn 			res;
    IOMemoryDescriptor *	mem;
    IOUSBPipe *			pipeObj;

    pipeObj = fPipes[pipe];
    if(!pipeObj)
        return kIOUSBUnknownPipeErr;

    mem = IOMemoryDescriptor::withAddress((vm_address_t)buf, sizeIn,
                                          kIODirectionIn,
                                          fTask);
    if(!mem) {
        *sizeOut = 0;
        return kIOReturnNoMemory;
    }
    res = mem->prepare();
    if(res == kIOReturnSuccess)
        res = pipeObj->read(mem, 0, sizeOut);
    mem->complete();
    if(kIOReturnSuccess != res)
        *sizeOut = 0;
    mem->release();

    return res;
}

/*
 * Out of line version of write pipe - the buffer isn't mapped (yet) into the kernel task
 */
IOReturn
IOUSBUserClient::WritePipeOOL(UInt32 pipe, void *buf, UInt32 size)
{
    IOReturn 			res;
    IOMemoryDescriptor *	mem;
    IOUSBPipe *			pipeObj;

    pipeObj = fPipes[pipe];
    if(!pipeObj)
        return kIOUSBUnknownPipeErr;

    mem = IOMemoryDescriptor::withAddress((vm_address_t)buf, size,
                                          kIODirectionOut,
                                          fTask);
    if(!mem) {
        res = kIOReturnNoMemory;
    }
    else {
        res = mem->prepare();
        if(res == kIOReturnSuccess)
            res = pipeObj->write(mem);
        mem->complete();
        mem->release();
    }
    return res;
}

/*
 * Async version of read pipe - the buffer isn't mapped (yet) into the kernel task
 */
IOReturn
IOUSBUserClient::ReadPipeAsync(OSAsyncReference asyncRef,
                               UInt32 pipe, void *buf, UInt32 size)
{
    IOReturn 			res;
    IOUSBPipe *			pipeObj;
    IOUSBCompletion		tap;
    IOMemoryDescriptor *	mem = NULL;
    AsyncPB * 			pb = NULL;

    pipeObj = fPipes[pipe];
    if(!pipeObj)
        return kIOUSBUnknownPipeErr;

    do {
        mem = IOMemoryDescriptor::withAddress((vm_address_t)buf, size,
                                            kIODirectionIn,
                                            fTask);
        if(!mem) {
            res = kIOReturnNoMemory;
            break;
        }
        res = mem->prepare();
        if(res != kIOReturnSuccess)
            break;

        pb = (AsyncPB *)IOMalloc(sizeof(AsyncPB));
        if(!pb) {
            res = kIOReturnNoMemory;
            break;
        }

        bcopy(asyncRef, pb->fAsyncRef, sizeof(OSAsyncReference));
        pb->fMax = size;
        pb->fMem = mem;
        tap.target = this;
        tap.action = &ReqComplete;
        tap.parameter = pb;
        res = pipeObj->read(mem, &tap, NULL);
    } while (false);
    if(res != kIOReturnSuccess) {
        if(mem) {
            mem->complete();
            mem->release();
        }
        if(pb)
            IOFree(pb, sizeof(*pb));
    }

    return res;
}

/*
 * Async version of write pipe - the buffer isn't mapped (yet) into the kernel task
 */
IOReturn
IOUSBUserClient::WritePipeAsync(OSAsyncReference asyncRef,
                                UInt32 pipe, void *buf, UInt32 size)
{
    IOReturn 			res;
    IOUSBPipe *			pipeObj;
    IOUSBCompletion		tap;
    IOMemoryDescriptor *	mem = NULL;
    AsyncPB * 			pb = NULL;

    pipeObj = fPipes[pipe];
    if(!pipeObj)
        return kIOUSBUnknownPipeErr;

    do {
        mem = IOMemoryDescriptor::withAddress((vm_address_t)buf, size,
                                            kIODirectionOut,
                                            fTask);
        if(!mem) {
            res = kIOReturnNoMemory;
            break;
        }
        res = mem->prepare();
        if(res != kIOReturnSuccess)
            break;

        pb = (AsyncPB *)IOMalloc(sizeof(AsyncPB));
        if(!pb) {
            res = kIOReturnNoMemory;
            break;
        }

        bcopy(asyncRef, pb->fAsyncRef, sizeof(OSAsyncReference));
        pb->fMax = size;
        pb->fMem = mem;
        tap.target = this;
        tap.action = &ReqComplete;
        tap.parameter = pb;
        res = pipeObj->write(mem, &tap);
    } while (false);
    if(res != kIOReturnSuccess) {
        if(mem) {
            mem->complete();
            mem->release();
        }
        if(pb)
            IOFree(pb, sizeof(*pb));
    }
    return res;
}

IOReturn IOUSBUserClient::ClosePipe(UInt32 pipe)
{
    fPipes[pipe] = 0;
    return kIOReturnSuccess;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

OSDefineMetaClassAndStructors(IOUSBDeviceUserClient, IOUSBUserClient)

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOUSBDeviceUserClient *IOUSBDeviceUserClient::withTask(task_t owningTask)
{
    IOUSBDeviceUserClient *me;

    me = new IOUSBDeviceUserClient;
    if(me) {
        if(!me->init()) {
            me->release();
            return NULL;
        }
        me->fTask = owningTask;
    }
    return me;
}

bool IOUSBDeviceUserClient::start( IOService * provider )
{
    assert(OSDynamicCast(IOUSBDevice, provider));
    if(!IOUSBUserClient::start(provider))
        return false;
    fOwner = (IOUSBDevice *)provider;
    fOwner->retain();
    fInterfaces = OSSet::withCapacity(1);
    if(!fInterfaces)
        return false;

    // initialize the call structures valid for IOUSBDevice
    fMethods[kUSBSetConfig].object = provider;
    fMethods[kUSBSetConfig].func = (IOMethod)&IOUSBDevice::SetConfiguration;
    fMethods[kUSBSetConfig].count0 = 1;
    fMethods[kUSBSetConfig].count1 = 0;
    fMethods[kUSBSetConfig].flags = kIOUCScalarIScalarO;

    fMethods[kUSBGetConfigDescriptor].object = this;
    fMethods[kUSBGetConfigDescriptor].func =
        (IOMethod)&IOUSBDeviceUserClient::GetConfigDescriptor;
    fMethods[kUSBGetConfigDescriptor].count0 = 1;
    fMethods[kUSBGetConfigDescriptor].count1 = 0xffffffff; // variable
    fMethods[kUSBGetConfigDescriptor].flags = kIOUCScalarIStructO;

    fMethods[kUSBOpenPipe].object = this;
    fMethods[kUSBOpenPipe].func =
        (IOMethod)&IOUSBDeviceUserClient::OpenPipe;
    fMethods[kUSBOpenPipe].count0 = 4;
    fMethods[kUSBOpenPipe].count1 = 1;
    fMethods[kUSBOpenPipe].flags = kIOUCScalarIScalarO;

    fMethods[kUSBResetDevice].object = provider;
    fMethods[kUSBResetDevice].func =
        (IOMethod)&IOUSBDevice::resetDevice;
    fMethods[kUSBResetDevice].count0 = 0;
    fMethods[kUSBResetDevice].count1 = 0;
    fMethods[kUSBResetDevice].flags = kIOUCScalarIScalarO;

    return true;
}

IOReturn IOUSBDeviceUserClient::clientClose( void )
{
    if(fInterfaces) {
        OSCollectionIterator *iter;

        iter = OSCollectionIterator::withCollection(fInterfaces);
        if(iter) {
            OSObject *obj;
            while( (obj=iter->getNextObject()) ) {
                ((IOService *)obj)->close(this);
            }
            iter->release();
        }
        fInterfaces->release();
        fInterfaces = NULL;
    }
    if(fOwner) {
        detach( fOwner);
        fOwner->release();
        fOwner = NULL;
    }
    return IOUSBUserClient::clientClose();
}

IOReturn
IOUSBDeviceUserClient::OpenPipe(UInt32 configIndex,
                          UInt32 intfNo, UInt32 altSet, UInt32 endPtOff, UInt32 *pipe)
{
    IOReturn res;
    IOUSBInterface *intf;
    const IOUSBInterfaceDescriptor *id = NULL;
    UInt8 configVal;
    IOUSBDevice::FindInterfaceRequest request;
    UInt32 i;
    const IOUSBConfigurationDescriptor *config;

    config = fOwner->getFullConfigurationDescriptor(configIndex);
    configVal = config->configValue;

    intf = NULL;
    request.theClass = 0;
    request.subClass = 0;
    request.protocol = 0;
    request.maxPower = 0;

    while( (intf = fOwner->findNextInterface(intf, &request))) {
        if(intf->getConfigValue() == configVal) {
            id = intf->interfaceDescriptor();
            if(id->interfaceNumber == intfNo &&
                    id->alternateSetting == altSet)
                break;
        }
        intf->release();
        // Reset request fields to wildcard values.
        request.theClass = 0;
        request.subClass = 0;
        request.protocol = 0;
        request.maxPower = 0;
    }

    if(!intf)
        return kIOUSBInterfaceNotFound;

    if(id->numEndpoints <= endPtOff)
        return kIOUSBUnknownPipeErr;

    do {
        IOUSBFindEndpointRequest findPipe;
        IOUSBPipe *pipeObj;
        if(!fInterfaces->member(intf)) {
            if(!intf->open(this)) {
                res = kIOReturnExclusiveAccess;
                break;
            }
            fInterfaces->setObject(intf);
        }

        // Loop through pipes the requested number of times
        pipeObj = NULL;
        endPtOff++; // Easier to be 1-based.
        for(i=0; i<endPtOff; i++) {
            findPipe.type = kUSBAnyType;
            findPipe.direction = kUSBAnyDirn;
            pipeObj = intf->findNextPipe(pipeObj, &findPipe);
            if(pipeObj == NULL) {
                res = kIOUSBEndpointNotFound;
                break;
            }
        }
        if(i != endPtOff) {
           res = kIOUSBEndpointNotFound;
            break;
        }
        for(i=1; i<kUSBMaxPipes; i++) {
            if(!fPipes[i]) {
                fPipes[i] = pipeObj;
                break;
            }
        }
        if(i >= kUSBMaxPipes) {
            res = kIOUSBTooManyPipesErr;
            break;
        }
        *pipe = i;
        res = kIOReturnSuccess;
    } while(false);
    if(intf)
        intf->release();
    return res;
}

IOReturn
IOUSBDeviceUserClient::GetConfigDescriptor(UInt8 configIndex,
                        IOUSBConfigurationDescriptorPtr desc, UInt32 *size)
{
    UInt16 length;
    const IOUSBConfigurationDescriptor *cached;

    cached = fOwner->getFullConfigurationDescriptor(configIndex);
    length = OSReadLittleInt16(&cached->totalLength, 0);
    if(length < *size)
        *size = length;
    bcopy(cached, desc, *size);
    return kIOReturnSuccess;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

OSDefineMetaClassAndStructors(IOUSBInterfaceUserClient, IOUSBUserClient)

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOUSBInterfaceUserClient *IOUSBInterfaceUserClient::withTask(task_t owningTask)
{
    IOUSBInterfaceUserClient *me;

    me = new IOUSBInterfaceUserClient;
    if(me) {
        if(!me->init()) {
            me->release();
            return NULL;
        }
        me->fTask = owningTask;
    }
    return me;
}

bool IOUSBInterfaceUserClient::start( IOService * provider )
{
    assert(OSDynamicCast(IOUSBInterface, provider));
    if(!IOUSBUserClient::start(provider))
        return false;
    fOwner = (IOUSBInterface *)provider;
    fOwner->retain();

// initialize the call structures valid for IOUSBInterface
    fMethods[kUSBGetConfigDescriptor].object = this;
    fMethods[kUSBGetConfigDescriptor].func =
        (IOMethod)&IOUSBInterfaceUserClient::GetConfigDescriptor;
    fMethods[kUSBGetConfigDescriptor].count0 = 1;
    fMethods[kUSBGetConfigDescriptor].count1 = 0xffffffff; // variable
    fMethods[kUSBGetConfigDescriptor].flags = kIOUCScalarIStructO;

    fMethods[kUSBOpenPipe].object = this;
    fMethods[kUSBOpenPipe].func =
        (IOMethod)&IOUSBInterfaceUserClient::OpenPipe;
    fMethods[kUSBOpenPipe].count0 = 4;
    fMethods[kUSBOpenPipe].count1 = 1;
    fMethods[kUSBOpenPipe].flags = kIOUCScalarIScalarO;

    return true;
}

IOReturn IOUSBInterfaceUserClient::clientClose( void )
{
    if(fOwner) {
        if(fOpen) {
            fOwner->close(this);
            fOpen = false;
	}
        detach( fOwner);
        fOwner->release();
        fOwner = NULL;
    }
    return IOUSBUserClient::clientClose();
}

IOReturn
IOUSBInterfaceUserClient::OpenPipe(UInt32 configIndex,
                          UInt32 intfNo, UInt32 altSet, UInt32 endPtOff, UInt32 *pipe)
{
    IOReturn res;
    const IOUSBInterfaceDescriptor *id = fOwner->interfaceDescriptor();
    UInt32 i;
    const IOUSBConfigurationDescriptor *config;

    config = fOwner->device()->getFullConfigurationDescriptor(configIndex);

    if(config->configValue != fOwner->getConfigValue())
        return kIOUSBConfigNotFound;

    if(intfNo != id->interfaceNumber ||
       altSet != id->alternateSetting)
        return kIOUSBInterfaceNotFound;

    if(id->numEndpoints <= endPtOff)
        return kIOUSBUnknownPipeErr;

    do {
        IOUSBFindEndpointRequest findPipe;
        IOUSBPipe *pipeObj;
        if(!fOpen) {
            if(!fOwner->open(this)) {
                res = kIOReturnExclusiveAccess;
                break;
            }
            fOpen = true;
        }

        // Loop through pipes the requested number of times
        pipeObj = NULL;
        endPtOff++; // Easier to be 1-based.
        for(i=0; i<endPtOff; i++) {
            findPipe.type = kUSBAnyType;
            findPipe.direction = kUSBAnyDirn;
            pipeObj = fOwner->findNextPipe(pipeObj, &findPipe);
            if(pipeObj == NULL) {
                res = kIOUSBEndpointNotFound;
                break;
            }
        }
        if(i != endPtOff) {
           res = kIOUSBEndpointNotFound;
            break;
        }
        for(i=1; i<kUSBMaxPipes; i++) {
            if(!fPipes[i]) {
                fPipes[i] = pipeObj;
                break;
            }
        }
        if(i >= kUSBMaxPipes) {
            res = kIOUSBTooManyPipesErr;
            break;
        }
        *pipe = i;
        res = kIOReturnSuccess;
    } while(false);
    return res;
}

IOReturn
IOUSBInterfaceUserClient::GetConfigDescriptor(UInt8 configIndex,
                        IOUSBConfigurationDescriptorPtr desc, UInt32 *size)
{
    UInt16 length;
    const IOUSBConfigurationDescriptor *cached;

    cached = fOwner->device()->getFullConfigurationDescriptor(configIndex);
    length = OSReadLittleInt16(&cached->totalLength, 0);
    if(length < *size)
        *size = length;
    bcopy(cached, desc, *size);
    return kIOReturnSuccess;
}
