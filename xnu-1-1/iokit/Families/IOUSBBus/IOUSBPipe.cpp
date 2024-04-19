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
 * 08 Dec 98 ehewitt created.
 *
 */
#include <libkern/OSByteOrder.h>

#include <IOKit/IOSyncer.h>
#include <IOKit/usb/IOUSBController.h>

#include <IOKit/usb/IOUSBPipe.h>
#include <IOKit/usb/IOUSBNub.h>

#define super OSObject
#define self this
#define DEBUGGING_LEVEL 0
#define DEBUGLOG kprintf

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

OSDefineMetaClassAndStructors(IOUSBPipe, OSObject)

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool IOUSBPipe::initToEndpoint(const IOUSBEndpointDescriptor *ed, UInt8 speed,
                               USBDeviceAddress address, IOUSBController * controller)
{
    if( !super::init() || ed == 0)
        return (false);

    _controller = controller;
    _descriptor = ed;
    _endpoint.number = ed->endpointAddress & kUSBPipeIDMask;
    _endpoint.transferType = ed->attributes & 0x03;
    if(_endpoint.transferType == kUSBControl)
        _endpoint.direction = kUSBAnyDirn;
    else
	_endpoint.direction = ed->endpointAddress & 0x80 ? kUSBIn : kUSBOut;
    _endpoint.maxPacketSize = OSReadLittleInt16(&ed->maxPacketSizeLo, 0);
    _endpoint.interval = ed->interval;
    _status = 0;
    _address = address;
    if(_controller->openPipe(_address, speed, &_endpoint) != kIOReturnSuccess)
        return false;

    return (true);
}

IOUSBPipe *IOUSBPipe::toEndpoint(const IOUSBEndpointDescriptor *ed, UInt8 speed,
                                 USBDeviceAddress address, IOUSBController * controller)
{
    IOUSBPipe *me = new IOUSBPipe;

    if (me && !me->initToEndpoint(ed, speed, address, controller)) {
        me->free();
        return 0;
    }

    return me;
}

void IOUSBPipe::free()
{
    _controller->closePipe(_address, &_endpoint);
    super::free();
}

// Controlling pipe state

UInt8 IOUSBPipe::status(void)
{
    return(_status);
}

IOReturn IOUSBPipe::abort(void)
{
    return _controller->abortPipe(_address, &_endpoint);
}

IOReturn IOUSBPipe::reset(void)
{
    return _controller->resetPipe(_address, &_endpoint);
}

IOReturn IOUSBPipe::clearStall(void)
{
    return _controller->clearPipeStall(_address, &_endpoint);
}

// Transferring Data
IOReturn IOUSBPipe::read(IOMemoryDescriptor * 	buffer,
                           IOUSBCompletion *	completion = 0,
                           IOByteCount *	bytesRead = 0)
{
    IOReturn	err = kIOReturnSuccess;


    if (completion == 0)
    {
        // put in our own completion routine if none was specified to
        // fake synchronous operation
        IOUSBCompletion	tap;
        IOSyncer *	syncer;

        syncer  = IOSyncer::create();
	if (bytesRead)
	    *bytesRead = buffer->getLength();

        tap.target = syncer;
        tap.action = &IOUSBSyncCompletion;
        tap.parameter = bytesRead;

        err = _controller->read(buffer, _address, &_endpoint, &tap);
        if (err == kIOReturnSuccess){
            err = syncer->wait();
        }
        else {
            syncer->release(); syncer->release();
        }
    }
    else
    {
        err = _controller->read(buffer, _address, &_endpoint, completion);
    }

    return(err);
}

IOReturn IOUSBPipe::write(IOMemoryDescriptor * buffer,
                            IOUSBCompletion *	 completion = 0)
{
    IOReturn	err = kIOReturnSuccess;

    if (completion == 0)
    {
        // put in our own completion routine if none was specified to
        // fake synchronous operation
        IOUSBCompletion	tap;
        IOSyncer *	syncer;

        syncer  = IOSyncer::create();

        tap.target = syncer;
        tap.action = &IOUSBSyncCompletion;
        tap.parameter = NULL;

        err = _controller->write(buffer, _address, &_endpoint, &tap);
        if (err == kIOReturnSuccess) {
            err = syncer->wait();
        }
        else {
            syncer->release(); syncer->release();
        }
    }
    else
    {
        err = _controller->write(buffer, _address, &_endpoint, completion);
    }

    return(err);
}

// Isochronous read and write
IOReturn IOUSBPipe::read(IOMemoryDescriptor * buffer,
	UInt64 frameStart, UInt32 numFrames, IOUSBIsocFrame *pFrames,
                         IOUSBIsocCompletion *	completion)
{
    IOReturn	err = kIOReturnSuccess;

    if (completion == 0)
    {
        // put in our own completion routine if none was specified to
        // fake synchronous operation
        IOUSBIsocCompletion	tap;
        IOSyncer *		syncer;

        syncer  = IOSyncer::create();

        tap.target = (void *)syncer;
        tap.action = IOUSBSyncIsoCompletion;
        tap.parameter = NULL;

        err = _controller->isocIO(buffer, frameStart, numFrames, pFrames,
                _address, &_endpoint, &tap);

        if (err == kIOReturnSuccess) {
            err = syncer->wait();
        }
        else {
            syncer->release(); syncer->release();
	}
    }
    else {
        err = _controller->isocIO(buffer, frameStart, numFrames, pFrames,
                _address, &_endpoint, completion);
    }

    return(err);
}

IOReturn IOUSBPipe::write(IOMemoryDescriptor * buffer,
	UInt64 frameStart, UInt32 numFrames, IOUSBIsocFrame *pFrames,
					IOUSBIsocCompletion *	completion)
{
    IOReturn	err = kIOReturnSuccess;

    if (completion == 0)
    {
        // put in our own completion routine if none was specified to
        // fake synchronous operation
        IOUSBIsocCompletion	tap;
        IOSyncer *		syncer;

        syncer  = IOSyncer::create();

        tap.target = syncer;
        tap.action = &IOUSBSyncIsoCompletion;
        tap.parameter = NULL;

        err = _controller->isocIO(buffer, frameStart, numFrames, pFrames,
                _address, &_endpoint, &tap);

        if (err == kIOReturnSuccess)
        {
            err = syncer->wait();
        }
        else {
            syncer->release(); syncer->release();
        }
    }
    else
    {
        err = _controller->isocIO(buffer, frameStart, numFrames, pFrames, 
		_address, &_endpoint, completion);

    }

    return(err);
}

IOReturn IOUSBPipe::controlRequest(IOUSBDevRequest	*request,
                                    IOUSBCompletion	*completion = 0)
{
    IOReturn	err = kIOReturnSuccess;


#if (DEBUGGING_LEVEL > 0)
    DEBUGLOG("%s: deviceRequest([%x,%s,%x,%x],[%x,%x],[%x,%lx])\n", "IOUSBPipe",
             request->bRequest,
             request->rqDirection?"in":"out",
             request->rqType, request->rqRecipient,
             OSReadLittleInt16(&request->wValue, 0),
             OSReadLittleInt16(&request->wIndex, 0),
             OSReadLittleInt16(&request->wLength, 0),
             (UInt32)request->pData);
#endif

    if (completion == 0)
    {
        // put in our own completion routine if none was specified to
        // fake synchronous operation
        IOUSBCompletion	tap;
        IOSyncer *	syncer;

        syncer  = IOSyncer::create();
        request->wLenDone = OSReadLittleInt16(&request->wLength, 0);

        tap.target = syncer;
        tap.action = &IOUSBSyncCompletion;
        tap.parameter = &request->wLenDone;

        err = _controller->deviceRequest(request, &tap, _address, _endpoint.number);
        if (err == kIOReturnSuccess) {
            err = syncer->wait();
        }
	else {
            syncer->release(); syncer->release();
        }
    }
    else
    {
        err = _controller->deviceRequest(request, completion, _address, _endpoint.number);
    }

    return(err);
}

IOReturn IOUSBPipe::controlRequest(IOUSBDevRequestDesc	*request,
                                    IOUSBCompletion	*completion = 0)
{
    IOReturn	err = kIOReturnSuccess;


#if (DEBUGGING_LEVEL > 0)
    DEBUGLOG("%s: deviceRequest([%x,%s,%x,%x],[%x,%x],[%x,%lx])\n", "IOUSBPipe",
             request->bRequest,
             request->rqDirection?"in":"out",
             request->rqType, request->rqRecipient,
             OSReadLittleInt16(&request->wValue, 0),
             OSReadLittleInt16(&request->wIndex, 0),
             OSReadLittleInt16(&request->wLength, 0),
             (UInt32)request->pData);
#endif

    if (completion == 0)
    {
        // put in our own completion routine if none was specified to
        // fake synchronous operation
        IOUSBCompletion	tap;
        IOSyncer *	syncer;

        syncer  = IOSyncer::create();
        request->wLenDone = OSReadLittleInt16(&request->wLength, 0);

        tap.target = syncer;
        tap.action = &IOUSBSyncCompletion;
        tap.parameter = &request->wLenDone;

        err = _controller->deviceRequest(request, &tap, _address, _endpoint.number);
        if (err == kIOReturnSuccess) {
            err = syncer->wait();
        }
	else {
            syncer->release(); syncer->release();
	}
    }
    else
    {
        err = _controller->deviceRequest(request, completion, _address, _endpoint.number);
    }

    return(err);
}

const IOUSBDescriptorHeader *
IOUSBPipe::findNextAssociatedDescriptor(const void *current, UInt8 type)
{
    const IOUSBDescriptorHeader *next;
    if(current == NULL)
        current = _descriptor;
    next = (const IOUSBDescriptorHeader *)current;

    while (true) {
        next = IOUSBNub::findNextDescriptor(next, kUSBAnyDesc);

        if(!next || next->descriptorType == kUSBEndpointDesc)
            return NULL;	// Reached end of our list.
        if(next->descriptorType == type || type == kUSBAnyDesc)
            break;
    }
    return next;
}

UInt32 IOUSBPipe::GetBandwidthAvailable()
{
    return _controller->GetBandwidthAvailable();
}

UInt64 IOUSBPipe::GetFrameNumber()
{
    return _controller->GetFrameNumber();
}


/*
 *  Accessors
 */
const IOUSBController::Endpoint *IOUSBPipe::endpoint(void)
{ return(&_endpoint); }

const IOUSBEndpointDescriptor *IOUSBPipe::endpointDescriptor(void)
{ return(_descriptor); }

UInt8 IOUSBPipe::direction(void)
{ return(_endpoint.direction); }

UInt8 IOUSBPipe::type(void)
{ return(_endpoint.transferType); }

UInt8 IOUSBPipe::endpointNumber(void)
{ return(_endpoint.number); }

USBDeviceAddress IOUSBPipe::address(void)
{ return(_address); }
