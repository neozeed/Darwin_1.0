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
 * 05 Mar 99 ehewitt created.
 *
 * IOUSBInterface
 * In USB there is the concept of a composite device, which means that
 * each interface in a selected configuration can act as a separate device.
 * Take for example a combo keyboard/trackball device.  In this example,
 * we might want a generic keyboard driver to handle the keyboard part and
 * a generic pointer driver to handle the trackball part.
 * So what we do is publish a IOUSBInterface for each interface of a
 * composite device and intercept the few device requests that are not allowed
 * by an interface, like set address and set configuration.
 * If another driver handled both functions, or if there was a specific driver
 * for that device, the matching logic would do the right thing.
 * 
 */

#include <libkern/OSByteOrder.h>
#include <IOKit/usb/IOUSBDevice.h>
#include <IOKit/usb/IOUSBController.h>
#include <IOKit/usb/IOUSBInterface.h>
#include <libkern/c++/OSDictionary.h>
#include <libkern/c++/OSData.h>
#include <IOKit/assert.h>

#include <IOKit/usb/IOUSBPipe.h>
#include "IOUSBUserClient.h"
#define super IOUSBNub
#define self this

int devintdebug = 0;
#define DEBUGLOG if (devintdebug) kprintf

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

OSDefineMetaClassAndStructors(IOUSBInterface, IOUSBNub)

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool IOUSBInterface::init(OSDictionary * propTable,
                          const IOUSBConfigurationDescriptor *cfdesc,
				const IOUSBInterfaceDescriptor *ifdesc)
{
    if(!ifdesc || !cfdesc)
        return (false);

    if( !IOUSBNub::init(propTable))
        return (false);
    _configDesc = cfdesc;
    _interfaceDesc = ifdesc;

    return (true);
}

bool IOUSBInterface::attach(IOService *provider)
{
    IOUSBDevice * device = OSDynamicCast(IOUSBDevice, provider);
    if(!device)
	return false;
    if( !IOUSBNub::attach(provider))
        return false;
    _device = device;
    _controller = device->bus();
    _pipeZero = device->pipeZero();
    _pipeZero->retain();

    if(_interfaceDesc->interfaceStrIndex) {
        IOReturn err;
        char name[128];

        err = GetStringDescriptor(_interfaceDesc->interfaceStrIndex, name, sizeof(name));
        if(err == kIOReturnSuccess)
            setName(name);
    }

    return true;
}

bool IOUSBInterface::finalize(IOOptionBits options)
{
    cleanPipes();
    return IOUSBNub::finalize(options);
}

/*
 * deviceRequest
 *   The idea here is to intercept only the calls that an interface,
 * thinking it's a driver, might make.  These include setting it's
 * configuration, getting it's device descriptor, and setting it's address.
 */
IOReturn IOUSBInterface::deviceRequest(IOUSBDevRequest	*request,
                                             IOUSBCompletion	*completion = 0)
{
    IOReturn		err;
    UInt16		theRequest;


    if (!request)
        return(kIOReturnBadArgument);
    
    theRequest = EncodeRequest(request->bRequest, request->rqDirection,
                               request->rqType,   request->rqRecipient);

    if (theRequest == kSetConfiguration)
    {
        DEBUGLOG("\tignoring kSetConfiguration\n");
        err = kIOReturnNotPermitted;
    }
    else
    {
        err = super::deviceRequest(request, completion);
    }

    return(err);
}

void IOUSBInterface::cleanPipes(void)
{
    IOUSBPipe * pipe;

    for( unsigned int i=0; i < kUSBMaxPipes; i++) {
        if( (pipe = _pipeList[i])) {
            pipe->release();
            _pipeList[i] = 0;
        }
    }
}

// Open all the pipes.
bool IOUSBInterface::open(IOService *	forClient,
                        IOOptionBits	options = 0,
                        void *		arg = 0 )
{
    unsigned int i = 0;
    const IOUSBDescriptorHeader *pos = NULL;
    if(!IOUSBNub::open(forClient, options, arg))
	return false;

    if(_currentAlt != _interfaceDesc->alternateSetting) {
        IOUSBDevRequest	request;
        IOReturn res;

        request.rqDirection = kUSBOut;
        request.rqType = kUSBStandard;
        request.rqRecipient = kUSBInterface;
        request.bRequest = kUSBRqSetInterface;
        OSWriteLittleInt16(&request.wValue, 0, _interfaceDesc->alternateSetting);
        OSWriteLittleInt16(&request.wIndex, 0, _interfaceDesc->interfaceNumber);
        OSWriteLittleInt16(&request.wLength, 0, 0);
        request.pData = 0;
        res = deviceRequest(&request);
        IOLog("SetInterface (%d, %d) returned 0x%x\n",
            _interfaceDesc->interfaceNumber, _interfaceDesc->alternateSetting, res);
        if(res != kIOReturnSuccess) {
            close(forClient);
            return false;
        }
        _currentAlt = _interfaceDesc->alternateSetting;
    }
    while ((pos = findNextAssociatedDescriptor(pos, kUSBEndpointDesc))) {
        // Don't open twice!
        if(_pipeList[i] == NULL) {
            _pipeList[i] = IOUSBPipe::toEndpoint((const IOUSBEndpointDescriptor *)pos,
                                                            _speed, _address, _controller);
        }
        if(_pipeList[i] == NULL) {
            close(forClient);
            return false;
        }
        i++;
    }
    assert(i == _interfaceDesc->numEndpoints);

    return true;
}

void IOUSBInterface::close(IOService *	forClient,
                        IOOptionBits	options = 0 )
{
    cleanPipes();
    IOUSBNub::close(forClient, options);
}

const IOUSBInterfaceDescriptor *
IOUSBInterface::findNextAltInterface(const IOUSBInterfaceDescriptor *current,
                                FindInterfaceRequest *request)
{
    const IOUSBInterfaceDescriptor *	id;
    const IOUSBDescriptorHeader *	pos = NULL;
    bool				found;
    if(current == NULL)
        pos = (IOUSBDescriptorHeader *)_configDesc;
    else
        pos = (IOUSBDescriptorHeader *)current;
    while (pos)
    {
        pos = findNextDescriptor(pos, kUSBInterfaceDesc);
        id = (const IOUSBInterfaceDescriptor *)pos;
        if(id == NULL || (id->interfaceNumber != _interfaceDesc->interfaceNumber)) {
            continue;
        }

        // check the request parameters
        found = true;
        if (request->theClass != 0 && request->theClass != id->interfaceClass)
            found = false;		// this is not it
        if (request->subClass != 0 && request->subClass != id->interfaceSubClass)
            found = false;		// this is not it
        if (request->protocol != 0 && request->protocol != id->interfaceProtocol)
            found = false;		// this is not it
        if (found)
        {
            request->theClass = id->interfaceClass;
            request->subClass = id->interfaceSubClass;
            request->protocol = id->interfaceProtocol;
            request->maxPower = _configDesc->maxPower;
            request->busPowered = _configDesc->attributes & kUSBAtrBusPowered ? 2 : 1;
            request->selfPowered = _configDesc->attributes & kUSBAtrSelfPowered ? 2 : 1;
            request->remoteWakeup = _configDesc->attributes & kUSBAtrRemoteWakeup ? 2 : 1;
             return(id);
        }
    }
    return(0);
}


IOUSBPipe *IOUSBInterface::findNextPipe(IOUSBPipe *current,
                                IOUSBFindEndpointRequest *request)
{
    const IOUSBController::Endpoint *	endpoint;
    IOUSBPipe *				pipe;
    int					numEndpoints;
    int					i;

    numEndpoints = _interfaceDesc->numEndpoints;

    if (request == 0)
        return NULL;
    if(current != 0) {
        for(i=0;i < numEndpoints; i++) {
            if(_pipeList[i] == current) {
		i++; // Skip the one we just did
                break;
            }
	}
    }
    else
        i = 0;	// Start at beginning.

    for (;i < numEndpoints; i++) {
        pipe = _pipeList[i];
        if(!pipe)
            continue;
        endpoint = pipe->endpoint();

        // check the request parameters
        if (request->type != kUSBAnyType &&
            request->type != endpoint->transferType)
            pipe = 0;		// this is not it
        else if (request->direction != kUSBAnyDirn &&
            request->direction != endpoint->direction)
            pipe = 0;		// this is not it

        if (pipe == 0)
            continue;

        request->type = endpoint->transferType;
        request->direction = endpoint->direction;
        request->maxPacketSize = endpoint->maxPacketSize;
        request->interval = endpoint->interval;
        return(pipe);
    }

    return(0);
}

const IOUSBDescriptorHeader *
IOUSBInterface::findNextAssociatedDescriptor(const void *current, UInt8 type)
{
    const IOUSBDescriptorHeader *next;
    if(current == NULL)
        current = _interfaceDesc;
    next = (const IOUSBDescriptorHeader *)current;

    while (true) {
        next = findNextDescriptor(next, kUSBAnyDesc);

        if(!next || next->descriptorType == kUSBInterfaceDesc)
            return NULL;	// Reached end of our list.
        if(next->descriptorType == type || type == kUSBAnyDesc)
            break;
    }
    return next;
}

IOReturn
IOUSBInterface::setAlternateInterface(UInt8 alternateSetting)
{
    const IOUSBDescriptorHeader *next;
    const IOUSBInterfaceDescriptor *ifdesc;

    next = (const IOUSBDescriptorHeader *)_configDesc;

    while( (next = findNextDescriptor(next, kUSBInterfaceDesc))) {
        ifdesc = (const IOUSBInterfaceDescriptor *)next;
        if(ifdesc->interfaceNumber == _interfaceDesc->interfaceNumber &&
            ifdesc->alternateSetting == alternateSetting)
            break;
    }

    if(ifdesc) {
        _interfaceDesc = ifdesc;
        OSData * data;
        data = OSData::withBytes(ifdesc, sizeof(*ifdesc));
        if (data) {
            setProperty("interface descriptor", data);
            data->release();
        }

        return kIOReturnSuccess;
    }
    else
        return kIOUSBConfigNotFound;
}

UInt8 IOUSBInterface::getConfigValue()
{
    return _configDesc->configValue;
}

const IOUSBInterfaceDescriptor * IOUSBInterface::interfaceDescriptor()
{
    return _interfaceDesc;
}

IOUSBDevice * IOUSBInterface::device()
{
    return _device;
}

IOReturn IOUSBInterface::newUserClient(  task_t		owningTask,
                                      void * 		/* security_id */,
                                      UInt32  		type,
                                      IOUserClient **	handler )

{
    IOReturn			err = kIOReturnSuccess;
    IOUSBInterfaceUserClient *	client;

    if( type != kIOUSBDeviceConnectType)
        return( kIOReturnBadArgument);

    client = IOUSBInterfaceUserClient::withTask(owningTask);

    if( !client || (false == client->attach( this )) ||
        (false == client->start( this )) ) {
        if(client) {
            client->detach( this );
            client->release();
        }
        err = kIOReturnNoMemory;
    }

    *handler = client;
    return( err );
}



