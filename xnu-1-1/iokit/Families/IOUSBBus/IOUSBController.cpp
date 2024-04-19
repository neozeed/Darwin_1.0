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

#define DEBUGGING_LEVEL 0	// 1 = low; 2 = high; 3 = extreme
#define DEBUGLOG kprintf

#include <libkern/c++/OSDictionary.h>
#include <libkern/c++/OSData.h>
#include <IOKit/usb/IOUSBController.h>
#include <IOKit/usb/IOUSBDevice.h>
#include "IOUSBRootHubDevice.h"
#include <IOKit/assert.h>
#include <IOKit/IOSyncer.h>

enum {
    kSetupSent  = 0x01,
    kDataSent   = 0x02,
    kStatusSent = 0x04,
    kSetupBack  = 0x10,
    kDataBack   = 0x20,
    kStatusBack = 0x40
};

#define kUSBSetup kUSBNone

void IOUSBSyncCompletion(void *	target,
                    void * 	parameter,
                    IOReturn	status,
                    UInt32	bufferSizeRemaining)
{
    IOSyncer *syncer = (IOSyncer *)target;

    if(parameter != NULL) {
        *(UInt32 *)parameter -= bufferSizeRemaining;
    }

    syncer->signal(status);
}

void IOUSBSyncIsoCompletion(void *target, void * 	parameter,
                                 IOReturn	status,
                                 IOUSBIsocFrame *pFrames)
{
    IOSyncer *syncer = (IOSyncer *)target;
    syncer->signal(status);
}


#define super IOUSBBus

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

OSDefineMetaClass( IOUSBController, IOUSBBus )
OSDefineAbstractStructors(IOUSBController, IOUSBBus)

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool IOUSBController::init(OSDictionary * propTable)
{
    if (!super::init(propTable))  return false;

    return (true);
}

bool IOUSBController::start( IOService * provider )
{
    IOUSBDeviceDescriptor desc;

    
    if( !super::start(provider))
        return (false);

    do {

        /*
         * Initialize the Workloop and Command gate
         */
        _workLoop = IOWorkLoop::workLoop();
        if (!_workLoop)
        {
            IOLog("%s: unable to create workloop\n", getName());
            break;
        }

        _commandGate = IOCommandGate:: commandGate(this, NULL);
        if (!_commandGate)
        {
            IOLog("%s: unable to create command gate\n", getName());
            break;
        }

        if (_workLoop->addEventSource(_commandGate) != kIOReturnSuccess)
        {
            IOLog("%s: unable to add command gate\n", getName());
            break;
        }
        
        /*
         * Initialize the UIM
         */
        if (UIMInitialize(provider) != kIOReturnSuccess)
        {
            IOLog("%s: unable to initialize UIM\n", getName());
            break;
        }

        /*
         * Initialize device zero
         */
	_devZeroLock = IOLockAlloc();
            
        /*
         * Create the root hub device
         */
        if (getRootHubDeviceDescriptor( &desc ) != kIOReturnSuccess)
        {
            IOLog("%s: unable to get root hub descriptor\n", getName());
            break;
        }

        _rootHubDevice = new IOUSBRootHubDevice;
        
        CreateDevice(_rootHubDevice, getNewAddress(), &desc,
                         kUSBHighSpeed, kUSB500mAAvailable);
        if (_rootHubDevice == 0)
        {
            IOLog("%s: unable to initialize root hub\n", getName());
            break;
        }
        _rootHubDevice->registerService();
        
        return(true);

    } while (false);

    if (_workLoop)	_workLoop->release();
    if (_commandGate)	_commandGate->release();

    return( false );
}

IOWorkLoop *IOUSBController::getWorkLoop() const
{
    return _workLoop;
}

IOUSBDevice *IOUSBController::newDevice()
{
    return(new IOUSBDevice);
}

/*
 * CreateDevice:
 *  This method just creates the device so we can minimally talk to it, e.g.
 *  get it's descriptor.  After this method, the device is not ready for
 *  prime time.
 */
IOReturn IOUSBController::CreateDevice(IOUSBDevice 		*newDevice,
                                           USBDeviceAddress	deviceAddress,
                                           IOUSBDeviceDescriptor *desc,
                                           UInt8		speed,
                                           UInt32		powerAvailable)
{
    OSDictionary 	*propTable = 0;
    OSData		*data;
    OSNumber		*offset;
    char		location[8];

#if (DEBUGGING_LEVEL > 0)
    DEBUGLOG("%s: CreateDevice: addr=%d, speed=%s, power=%d\n", getName(), 
             deviceAddress, speed ? "low" : "high", powerAvailable*2);
#endif
    
    do {
        propTable = OSDictionary::withCapacity(4);
        if (!propTable)
            break;

        if (desc) {
	    data = OSData::withBytes( (void *)desc, sizeof( *desc ));
	    if(data) {
                propTable->setObject("device descriptor", data);
		data->release();
	    }
            offset = OSNumber::withNumber(desc->deviceClass, 8);
            if(offset) {
                propTable->setObject("class", offset);
                offset->release();
            }
            offset = OSNumber::withNumber(desc->deviceSubClass, 8);
            if(offset) {
                propTable->setObject("subClass", offset);
                offset->release();
            }
            offset = OSNumber::withNumber(desc->protocol, 8);
            if(offset) {
                propTable->setObject("protocol", offset);
                offset->release();
            }
            offset = OSNumber::withNumber(OSReadLittleInt16(&desc->vendor, 0), 16);
            if(offset) {
                propTable->setObject("vendor", offset);
                offset->release();
            }
            offset = OSNumber::withNumber(OSReadLittleInt16(&desc->product, 0), 16);
            if(offset) {
                propTable->setObject("product", offset);
                offset->release();
            }
            offset = OSNumber::withNumber(OSReadLittleInt16(&desc->devRel, 0), 16);
            if(offset) {
                propTable->setObject("version", offset);
                offset->release();
            }
	}

	data = OSData::withBytes( (void *)&deviceAddress,
                                    sizeof( deviceAddress ));
	if (data) {
            propTable->setObject("device address", data);
	    data->release();
	}

	data = OSData::withBytes( (void *)&powerAvailable,
                                    sizeof( powerAvailable ));
	if (data) {
            propTable->setObject("bus power available", data);
	    data->release();
	}

	data = OSData::withBytes( (void *)&speed, sizeof( speed ));
	if (data) {
            propTable->setObject("low speed device", data);
	    data->release();
	}

        if (!newDevice->init(propTable))
            break;
        
        propTable->release();	// done with it after init
        propTable = 0;

        sprintf( location, "%x", deviceAddress );
        newDevice->setLocation(location);

        if (!newDevice->attach(this))
            break;

        return(kIOReturnSuccess);

    } while (false);

    if (propTable)
        propTable->release();

    return(kIOReturnNoMemory);
}

/*
 * deviceRequest:
 * Queue up a low level device request.  It's very simple because the
 * device has done all the error checking.  Commands get allocated here and get
 * deallocated in the handlers.
 *
 */ 
IOReturn IOUSBController::deviceRequest(IOUSBDevRequest *	request,
                                        IOUSBCompletion *	completion,
                                        USBDeviceAddress address, UInt8 ep)
{
    // FIXME -- should preallocate a bunch of 'command's
    IOUSBCommand *command = (IOUSBCommand *)IOMalloc(sizeof(IOUSBCommand));
    IOReturn	err = kIOReturnSuccess;

    do
    {
        command->selector	= DEVICE_REQUEST;
        command->request	= request;
        command->address	= address;
	command->endpoint	= ep;
        command->type		= kUSBControl;
        command->buffer		= 0; // no buffer for device requests
        command->completion	= *completion;

        if (_commandGate == 0)
        {
            err = kIOReturnInternalError;
            break;
        }

        err = _commandGate->runAction(doControlTransfer, command);
        if ( err)
            break;
            
        return (err);
    } while (0);

    /* Free/give back the command */
    IOFree(command, sizeof(IOUSBCommand));

    return(err);
}

IOReturn IOUSBController::deviceRequest(IOUSBDevRequestDesc *	request,
                                        IOUSBCompletion *	completion,
                                        USBDeviceAddress address, UInt8 ep)
{
    // FIXME -- should preallocate a bunch of 'command's
    IOUSBCommand *command = (IOUSBCommand *)IOMalloc(sizeof(IOUSBCommand));
    IOReturn	err = kIOReturnSuccess;

    do
    {
        command->selector	= DEVICE_REQUEST_DESC;
	// IOUSBDevRequest and IOUSBDevRequestDesc are same except for
        // pData (void * or descriptor).
        command->request	= (IOUSBDevRequest *)request;
        command->address	= address;
        command->endpoint	= ep;
        command->type		= kUSBControl;
        command->buffer		= request->pData;
        command->completion	= *completion;

        if (_commandGate == 0)
        {
            err = kIOReturnInternalError;
            break;
        }

        err = _commandGate->runAction(doControlTransfer, command);
        if ( err)
            break;

        return (err);
    } while (0);

    /* Free/give back the command */
    IOFree(command, sizeof(IOUSBCommand));

    return(err);
}

USBDeviceAddress IOUSBController::getNewAddress(void)
{
    int i;
    bool assigned[kUSBMaxDevices];
    OSIterator * clients;

    bzero(assigned, sizeof(assigned));

    clients = getClientIterator();
    if(clients) {
        OSObject *next;
        while((next = clients->getNextObject())) {
            IOUSBDevice *testIt = OSDynamicCast(IOUSBDevice, next);
            if(testIt) {
                assigned[testIt->address()] = true;
            }
        }
        clients->release();
    }

    for(i=1; i<kUSBMaxDevices; i++) {
	if(!assigned[i])
            return i;
    }

    return(0);	// No free device addresses!
}

/*
 * ControlPacket:
 *   Send a USB control packet which consists of at least two stages: setup
 * and status.   Optionally there can be multiple data stages.
 *
 */
IOReturn IOUSBController::ControlTransaction(IOUSBCommand *command)
{
    IOUSBDevRequest	*request = command->request;
    UInt8		direction = request->rqDirection;
    UInt8		endpoint = command->endpoint;
    UInt16		wLength = OSReadLittleInt16(&request->wLength, 0);
    IOReturn		err = kIOReturnSuccess;
    IOUSBCompletion	completion;
    
    do
    {
        // Setup Stage
#if (DEBUGGING_LEVEL > 1)
        DEBUGLOG("\tQueueing Setup TD (dir=%d) packet=0x%lx 0x%lx\n",
              direction, *(UInt32*)request, *((UInt32*)request+1));
#endif

        completion.target    = (void *)this;
        completion.action    = &IOUSBController::ControlPacketHandler;
        completion.parameter = (void *)command;

        command->dataRemaining = wLength;
        command->stage = kSetupSent;
        err = UIMCreateControlTransfer(
                    command->address	/*functionAddress*/,
                    endpoint	 	/*endpointNumber*/,
                    completion   	/*completion*/,
                    request		/*packet*/,
                    true		/*bufferRounding*/,
                    8			/*packet size*/,
                    kUSBSetup		/*direction*/);
        if (err)
        {
            IOLog("ControlTransaction: control packet 1 error %d\n", err);
            break;
        }

        // Data Stage
        if (wLength != 0 && request->pData != 0)
        {
#if (DEBUGGING_LEVEL > 1)
            DEBUGLOG("\tQueueing Data TD (dir=%d, wLength=0x%x, pData=%lx)\n", direction,
                  wLength, request->pData);
#endif
            command->stage |= kDataSent;
            if(command->selector == DEVICE_REQUEST_DESC)
                err = UIMCreateControlTransfer(
                        command->address	/*functionAddress*/,
                        endpoint		/*endpointNumber*/,
                        completion   		/*completion*/,
                        command->buffer		/*buffer*/,
                        true			/*bufferRounding*/,
                        wLength			/*bufferSize*/,
                        direction		/*direction*/);
            else
                err = UIMCreateControlTransfer(
                        command->address	/*functionAddress*/,
                        endpoint		/*endpointNumber*/,
                        completion   		/*completion*/,
                        request->pData		/*buffer*/,
                        true			/*bufferRounding*/,
                        wLength			/*bufferSize*/,
                        direction		/*direction*/);
            if (err)
            {
                IOLog("ControlTransaction: control packet 2 error %d\n", err);
                break;
            }
            
            direction = kUSBOut + kUSBIn - direction; // swap direction
        }
        else
            direction = kUSBIn;

        // Status Stage
#if (DEBUGGING_LEVEL > 1)
        DEBUGLOG("\tQueueing Status TD (dir=%d)\n", direction);
#endif
        command->stage |= kStatusSent;
        err = UIMCreateControlTransfer(
                    command->address	/*functionAddress*/,
                    endpoint		/*endpointNumber*/,
                    completion   	/*completion*/,
                    (void *)0		/*buffer*/,
                    true		/*bufferRounding*/,
                    0			/*bufferSize*/,
                    direction		/*direction*/);
        if (err)
        {
            IOLog("ControlTransaction: control packet 3 error %d\n", err);
            break;
        }
    } while(false);

    if (err)
        complete(command->completion, err, 0);
    return(err);
}

/*
 * ControlPacketHandler:
 * Handle all three types of control packets and maintain what stage
 * we're at.  When we receive the last one, then call the clients
 * completion routine.
 */
void IOUSBController::ControlPacketHandler(void *	parameter,
                                           IOReturn	status,
                                           UInt32	bufferSizeRemaining)
{
    IOUSBCommand 	*command = (IOUSBCommand *)parameter;
    IOUSBDevRequest	*request;
    UInt8		sent, back, todo;

#if (DEBUGGING_LEVEL > 1)
    DEBUGLOG("ControlPacketHandler lParam=%lx  status=0x%x bufferSizeRemaining=0x%x\n", parameter, status, bufferSizeRemaining);
#endif

    if (command == 0)
        return;

    request = command->request;

    sent = (command->stage & 0x0f) << 4;
    back = command->stage & 0xf0;
    todo = sent ^ back; /* thats xor */

    if((todo & kSetupBack) != 0)
        command->stage |= kSetupBack;
    else if((todo & kDataBack) != 0)
    {
        /* This is the data transport phase, so this is the interesting one */
        command->stage |= kDataBack;
        command->dataRemaining = bufferSizeRemaining;
    }
    else if((todo & kStatusBack) != 0)
        command->stage |= kStatusBack;
    else
        IOLog("ControlPacketHandler: Spare transactions, This seems to be harmless");

    back = command->stage & 0xf0;
    todo = sent ^ back; /* thats xor */

    if (status != kIOReturnSuccess)
    {
        USBDeviceAddress	addr = command->address;
        UInt8			endpt = command->endpoint;

        command->status = status;

#if (DEBUGGING_LEVEL > 0)
        DEBUGLOG("ControlPacketHandler: error %d occured.  todo = %d\n", status, todo);
        
#endif
        UIMClearEndpointStall(addr, endpt, kUSBAnyDirn);
    }
    if (todo == 0)
    {
#if (DEBUGGING_LEVEL > 0)
        DEBUGLOG("ControlPacketHandler: transaction complete status=%d\n", status);
#endif

        /* Call the clients handler */
        complete(command->completion, status, command->dataRemaining);

        /* Free/give back the command */
        IOFree(command, sizeof(IOUSBCommand));
    }
}

/*
 * InterruptTransaction:
 *   Send a USB interrupt packet.
 *
 */
IOReturn IOUSBController::InterruptTransaction(IOUSBCommand *command)
{
    IOReturn		err = kIOReturnSuccess;
    IOUSBCompletion	completion;

    completion.target 	 = (void *)this;
    completion.action 	 = &IOUSBController::InterruptPacketHandler;
    completion.parameter = (void *)command;

    err = UIMCreateInterruptTransfer(
                command->address			/*functionAddress*/,
                command->endpoint			/*endpointNumber*/,
                completion   				/*completion*/,
                command->buffer				/*buffer*/,
                true					/*bufferRounding*/,
                command->buffer->getLength()       	/*bufferSize*/,
                command->direction);

    return(err);
}

void IOUSBController::InterruptPacketHandler(void * parameter, IOReturn status, UInt32 bufferSizeRemaining)
{
    IOUSBCommand 	*command = (IOUSBCommand *)parameter;

    if (command == 0)
        return;

#if (DEBUGGING_LEVEL > 1)
    DEBUGLOG("InterruptPacketHandler: transaction complete status=%d bufferSizeRemaining = %d\n", status, bufferSizeRemaining);
#endif
    if (status != kIOReturnSuccess)
        UIMClearEndpointStall(command->address,
                              command->endpoint,
                              command->direction);

    /* Call the clients handler */
    complete(command->completion, status, bufferSizeRemaining);

    /* Free/give back the command */
    IOFree(command, sizeof(IOUSBCommand));
}


/*
 * BulkTransaction:
 *   Send a USB bulk packet.
 *
 */
IOReturn IOUSBController::BulkTransaction(IOUSBCommand *command)
{
    IOUSBCompletion	completion;
    IOReturn		err = kIOReturnSuccess;

    
    completion.target 	 = (void *)this;
    completion.action 	 = &IOUSBController::BulkPacketHandler;
    completion.parameter = (void *)command;

    err = UIMCreateBulkTransfer(
                command->address			/*functionAddress*/,
                command->endpoint			/*endpointNumber*/,
                completion  				/*completion*/,
                command->buffer				/*buffer*/,
                true					/*bufferRounding*/,
                command->buffer->getLength()     	/*bufferSize*/,
                command->direction			/*direction*/);

    if (err)
        IOLog("BulkTransaction: error queueing bulk packet (%d)\n", err);

    return(err);
}

void IOUSBController::BulkPacketHandler(void * 		parameter,
                                        IOReturn	status,
                                        UInt32		bufferSizeRemaining)
{
    IOUSBCommand 	*command = (IOUSBCommand *)parameter;

    
    if (command == 0)
        return;

#if (DEBUGGING_LEVEL > 0)
    DEBUGLOG("BulkPacketHandler: transaction complete status=%d bufferSizeRemaining = %d\n", status, bufferSizeRemaining);
#endif
    if (status != kIOReturnSuccess)
        UIMClearEndpointStall(command->address,
                              command->endpoint,
                              command->direction);
    
    /* Call the clients handler */
    complete(command->completion, status, bufferSizeRemaining);

    /* Free/give back the command */
    IOFree(command, sizeof(IOUSBCommand));
}

/*
 * BulkTransaction:
 *   Send a USB bulk packet.
 *
 */
IOReturn IOUSBController::doIsocTransfer(OSObject *owner, void *cmd,
                        void */*field2*/, void */*field3*/, void */*field4*/)
{
    IOUSBController	*controller = (IOUSBController *)owner;
    IOUSBIsocCommand	*command  = (IOUSBIsocCommand *) cmd;
    return(controller->IsocTransaction(command));
}

IOReturn IOUSBController::IsocTransaction(IOUSBIsocCommand *command)
{
    IOUSBIsocCompletion	completion;
    IOReturn		err = kIOReturnSuccess;


    completion.target 	 = (void *)this;
    completion.action 	 = &IOUSBController::IsocCompletionHandler;
    completion.parameter = (void *)command;

    err = UIMCreateIsochTransfer(
                command->address	/*functionAddress*/,
                command->endpoint	/*endpointNumber*/,
                completion  		/*completion*/,
		command->direction	/*direction*/,
                command->startFrame	/*Start frame */,
                command->buffer		/*buffer*/,
                command->numFrames	/*number of frames*/,
                command->frameList     	/*transfer for each frame*/);

    if (err) {
        IOLog("IsocTransaction: error queueing isoc transfer (0x%x)\n", err);
        if (command->completion.action)
            (command->completion.action)(command->completion.target,
                                          command->completion.parameter,
                                          err,
                                          command->frameList);
    }
    return(err);
}

void IOUSBController::IsocCompletionHandler(void * 	parameter,
                                        IOReturn	status,
                                        IOUSBIsocFrame	*pFrames)
{
    IOUSBIsocCommand 	*command = (IOUSBIsocCommand *)parameter;

    if (command == 0)
        return;

    if (status != kIOReturnSuccess)
        UIMClearEndpointStall(command->address,
                              command->endpoint,
                              command->direction);

    /* Call the clients handler */
    IOUSBIsocCompletion *completion = &command->completion;
    if (completion->action)  (*completion->action)(completion->target,
                                                   completion->parameter,
                                                 status,
                                                 pFrames);

    /* Free/give back the command */
    IOFree(command, sizeof(IOUSBIsocCommand));
}


IOReturn IOUSBController::doIOTransfer(OSObject *owner,
                           void *cmd,
                           void */*field2*/, void */*field3*/, void */*field4*/)
{
    IOUSBController	*controller = (IOUSBController *)owner;
    IOUSBCommand	*command  = (IOUSBCommand *) cmd;
    IOReturn		err       = kIOReturnSuccess;

    switch (command->type)
    {
        case kUSBInterrupt:
            err = controller->InterruptTransaction(command);
            break;
        case kUSBIsoc:
            IOLog("Isoc transactions not supported on non-isoc pipes!!\n");
            err = kIOReturnBadArgument;
            break;
        case kUSBBulk:
            err = controller->BulkTransaction(command);
            break;
        default:
            IOLog("Unknown transaction type\n");
            err = kIOReturnBadArgument;
            break;
    }

    if (err)
        controller->complete(command->completion, err, 0);

    return err;
}

IOReturn IOUSBController::doControlTransfer(OSObject *owner,
                           void *arg0, void *arg1,
                           void *arg2, void *arg3)
{
    IOUSBController *me = (IOUSBController *)owner;

    return me->ControlTransaction((IOUSBCommand *)arg0);
}

IOReturn IOUSBController::doDeleteEP(OSObject *owner,
                           void *arg0, void *arg1,
                           void *arg2, void *arg3)
{
    IOUSBController *me = (IOUSBController *)owner;

    return me->UIMDeleteEndpoint((short)arg0, (short)arg1, (short)arg2);
}

IOReturn IOUSBController::doCreateEP(OSObject *owner,
                           void *arg0, void *arg1,
                           void *arg2, void *arg3)
{
    IOUSBController *me = (IOUSBController *)owner;
    UInt8 address = (UInt8)arg0;
    UInt8 speed = (UInt8)arg1;
    Endpoint *endpoint = (Endpoint *)arg2;
    IOReturn err;
    switch (endpoint->transferType)
    {
        case kUSBInterrupt:
            err = me->UIMCreateInterruptEndpoint(address,
                                             endpoint->number,
                                             endpoint->direction,
                                             speed,
                                             endpoint->maxPacketSize,
                                             endpoint->interval);
            break;

        case kUSBBulk:
            err = me->UIMCreateBulkEndpoint(address,
                                        endpoint->number,
                                        endpoint->direction,
                                        speed,
                                        endpoint->maxPacketSize);
            break;

        case kUSBControl:
            err = me->UIMCreateControlEndpoint(address,
                                           endpoint->number,
                                           endpoint->maxPacketSize,
                                           speed);
            break;

        case kUSBIsoc:
            err = me->UIMCreateIsochEndpoint(address,
                                        endpoint->number,
                                        endpoint->maxPacketSize,
                                        endpoint->direction);
            break;

        default:
            err = kIOReturnBadArgument;
            break;
    }
    return (err);
}

IOReturn IOUSBController::AcquireDeviceZero()
{
    IOReturn err = 0;
    Endpoint ep;

    ep.number = 0;
    ep.transferType = kUSBControl;
    ep.maxPacketSize = 8;

#if (DEBUGGING_LEVEL > 0)
    DEBUGLOG("%s: TRYING TO ACQUIRE DEVICE ZERO\n", getName());
#endif

    IOTakeLock(_devZeroLock);

#if (DEBUGGING_LEVEL > 0)
    DEBUGLOG("%s: ACQUIRED DEVICE ZERO\n", getName());
#endif

    err = openPipe(0, kUSBHighSpeed, &ep);
    return(err);
}

void IOUSBController::ReleaseDeviceZero(void)
{
    IOReturn err = 0;

    err = _commandGate->runAction(doDeleteEP, (void *)0, (void *)0, (void *)kUSBAnyDirn);
    IOUnlock(_devZeroLock);

#if (DEBUGGING_LEVEL > 0)
    DEBUGLOG("%s: RELEASED DEVICE ZERO\n", getName());
#endif

    return;
}

void IOUSBController::WaitForReleaseDeviceZero()
{
    IOTakeLock(_devZeroLock);
    IOUnlock(_devZeroLock);
}

IOReturn IOUSBController::ConfigureDeviceZero(UInt8 maxPacketSize, UInt8 speed)
{
    IOReturn	err = kIOReturnSuccess;
    Endpoint ep;

    ep.number = 0;
    ep.transferType = kUSBControl;
    ep.maxPacketSize = maxPacketSize;

#if (DEBUGGING_LEVEL > 0)
    DEBUGLOG("********** ConfigureDeviceZero %d %d **********\n", maxPacketSize, speed);
#endif
    err = _commandGate->runAction(doDeleteEP, (void *)0, (void *)0, (void *)kUSBAnyDirn);
    err = openPipe(0, speed, &ep);

    return(err);
}


IOReturn IOUSBController::GetDeviceZeroDescriptor(IOUSBDeviceDescriptor *desc)
{
    IOReturn		err = kIOReturnSuccess;
    IOUSBDevRequest	request;

#if (DEBUGGING_LEVEL > 0)
    DEBUGLOG("********** GET DEVICE ZERO DEVICE DESCRIPTOR **********\n");
#endif

    do
    {
        IOUSBCompletion	tap;
        IOSyncer *	syncer;

        syncer  = IOSyncer::create();

        tap.target = syncer;
        tap.action = &IOUSBSyncCompletion;
        tap.parameter = NULL;

        if (!desc)
        {
            err = kIOReturnBadArgument;
            break;
        }

        request.rqDirection = kUSBIn;
        request.rqType = kUSBStandard;
        request.rqRecipient = kUSBDevice;
        request.bRequest = kUSBRqGetDescriptor;
        OSWriteLittleInt16(&request.wValue, 0, (kUSBDeviceDesc << 8));
        OSWriteLittleInt16(&request.wIndex, 0, 0);
        OSWriteLittleInt16(&request.wLength, 0, sizeof(IOUSBDeviceDescriptor));
        request.pData = desc;

        err = deviceRequest(&request, &tap, 0, 0);

        if (err)
        {
#if (DEBUGGING_LEVEL > 0)
            DEBUGLOG("%s: error getting device descriptor. err=%d\n", getName(), err);
#endif
            syncer->release();
            syncer->release();
            break;
        }
        err = syncer->wait();

    } while(false);

    return(err);
}

IOReturn IOUSBController::SetDeviceZeroAddress(USBDeviceAddress address,
                                               UInt8 maxPacketSize, UInt8 speed)
{
    IOReturn		err = kIOReturnSuccess;
    IOUSBDevRequest	request;

#if (DEBUGGING_LEVEL > 0)
    DEBUGLOG("********** SET DEVICE ZERO ADDRESS: %d %d %d **********\n",
             address, maxPacketSize, speed);
#endif

    do
    {
        IOUSBCompletion	tap;
        IOSyncer *	syncer;

        syncer  = IOSyncer::create();

        tap.target = syncer;
        tap.action = &IOUSBSyncCompletion;
        tap.parameter = NULL;

        request.rqDirection = kUSBOut;
        request.rqType = kUSBStandard;
        request.rqRecipient = kUSBDevice;
        request.bRequest = kUSBRqSetAddress;
        OSWriteLittleInt16(&request.wValue, 0, address);
        OSWriteLittleInt16(&request.wIndex, 0, 0);
        OSWriteLittleInt16(&request.wLength, 0, 0);
        request.pData = 0;

        err = deviceRequest(&request, &tap, 0, 0);

        if (err)
        {
            syncer->release(); syncer->release();
            break;
        }
        err = syncer->wait();

    } while(false);

#if (DEBUGGING_LEVEL > 0)
    DEBUGLOG("%s: error setting device address. err=%d\n", getName(), err);
#endif
    return(err);
}

IOUSBDevice *IOUSBController::MakeDevice(IOUSBDeviceDescriptor *desc,
                                        UInt8 speed, UInt32 power)
{
    IOReturn		err = kIOReturnSuccess;
    IOUSBDevice		*newDev;
    USBDeviceAddress	address;

#if (DEBUGGING_LEVEL > 0)
    DEBUGLOG("*************** MakeDevice ***************\n");
#endif

    newDev = newDevice();
    if (newDev == 0)
        return(0);

    address = getNewAddress();
    if(address == 0) {
	newDev->release();
	return 0;
    }

    err = SetDeviceZeroAddress(address, desc->maxPacketSize, speed);

    if (err)
    {
#if (DEBUGGING_LEVEL > 0)
        DEBUGLOG("%s: error setting address. err=%d device=%lx\n",
              getName(), err, (UInt32)this);
#endif
        //return(0); Some devices produce a spurious error here, eg. Altec Lansing speakers
    }

    err = CreateDevice(newDev, address, desc, speed, power);
    if(err) {
        newDev->release();
        return(0);
    }
    return(newDev);
}

IOReturn IOUSBController::PolledRead(
        short				functionNumber,
        short				endpointNumber,
        IOUSBCompletion			completion,
        IOMemoryDescriptor *		CBP,
        bool				bufferRounding,
        UInt32				bufferSize)
{
    return UIMCreateInterruptTransfer(functionNumber, endpointNumber,
			completion, CBP, bufferRounding, bufferSize, kUSBIn);
}
