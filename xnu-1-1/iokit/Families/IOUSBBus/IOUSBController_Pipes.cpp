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

#include <IOKit/system.h>

#define DEBUGGING_LEVEL 0	// 1 = low; 2 = high; 3 = extreme
#define DEBUGLOG kprintf

#include <IOKit/usb/IOUSBController.h>
#include <IOKit/IOMemoryDescriptor.h>

#define super IOUSBBus
#define self this

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
IOReturn IOUSBController::openPipe(USBDeviceAddress address, UInt8 speed,
						Endpoint *endpoint)
{
    return _commandGate->runAction(doCreateEP, (void *)address,
			(void *)speed, endpoint);
}

IOReturn IOUSBController::closePipe(USBDeviceAddress address,
                                    		Endpoint * endpoint)
{
    return _commandGate->runAction(doDeleteEP, (void *)address,
			(void *)endpoint->number, (void *)endpoint->direction);
}

IOReturn IOUSBController::abortPipe(USBDeviceAddress address,
                                    Endpoint * endpoint)
{
    return UIMAbortEndpoint(address,
                endpoint->number, endpoint->direction);
}

IOReturn IOUSBController::resetPipe(USBDeviceAddress address,
                                    Endpoint * endpoint)
{
    return UIMClearEndpointStall(address,
                endpoint->number, endpoint->direction);
}

IOReturn IOUSBController::clearPipeStall(USBDeviceAddress address,
                                         Endpoint * endpoint)
{
    return UIMClearEndpointStall(address,
                endpoint->number, endpoint->direction);
}


// Transferring Data
IOReturn IOUSBController::read(IOMemoryDescriptor *   	buffer,
                               USBDeviceAddress 	address,
                               Endpoint *		endpoint,
                               IOUSBCompletion *       	completion)
{
    IOReturn	 err = kIOReturnSuccess;
    IOUSBCommand *command = (IOUSBCommand *)IOMalloc(sizeof(IOUSBCommand));

    do
    {
        /* Validate its a inny pipe */
        if (endpoint->direction != kUSBIn)
        {
            err = kIOReturnBadArgument;
            break;
        }

        /* Validate the completion */
        if (completion == 0)
        {
            err = kIOReturnNoCompletion;
            break;
        }
        
        command->selector	= READ;
        command->request	= 0;            // Not a device request
        command->address	= address;
        command->endpoint	= endpoint->number;
	command->direction	= kUSBIn;
        command->type		= endpoint->transferType;
        command->buffer		= buffer;
        command->completion	= *completion;

        if (_commandGate == 0)
        {
            err = kIOReturnInternalError;
            break;
        }

        if ((err = _commandGate->runAction(doIOTransfer, command)))
            break;
            
        return(err);

    } while (0);

    /* Free/give back the command */
    IOFree(command, sizeof(IOUSBCommand));

    return (err);
}

IOReturn IOUSBController::write(IOMemoryDescriptor *   	buffer,
                                USBDeviceAddress 	address,
                                Endpoint *		endpoint,
                                IOUSBCompletion *	completion)
{
    IOReturn	 err = kIOReturnSuccess;
    IOUSBCommand *command = (IOUSBCommand *)IOMalloc(sizeof(IOUSBCommand));

    do
    {
        /* Validate its a outty pipe */
        if(endpoint->direction != kUSBOut)
        {
            err = kIOReturnBadArgument;
            break;
        }

        command->selector	= WRITE;
        command->request	= 0;            // Not a device request
        command->address	= address;
        command->endpoint	= endpoint->number;
        command->direction	= kUSBOut;
        command->type		= endpoint->transferType;
        command->buffer		= buffer;
        command->completion	= *completion;

        if (_commandGate == 0)
        {
            err = kIOReturnInternalError;
            break;
        }

        if ((err = _commandGate->runAction(doIOTransfer, command)))
            break;
            
        return(err);

    } while (0);

    /* Free/give back the command */
    IOFree(command, sizeof(IOUSBCommand));
    
    return (err);
}

IOReturn IOUSBController::isocIO(IOMemoryDescriptor * buffer,
                               UInt64 frameStart,
                               UInt32 numFrames,
                               IOUSBIsocFrame *frameList,
                               USBDeviceAddress address,
                               Endpoint * endpoint,
                               IOUSBIsocCompletion * completion)
{
    IOReturn	 err = kIOReturnSuccess;
    IOUSBIsocCommand *command = (IOUSBIsocCommand *)IOMalloc(sizeof(IOUSBIsocCommand));

    do
    {
        /* Validate the completion */
        if (completion == 0)
        {
            err = kIOReturnNoCompletion;
            break;
        }

        /* Set up direction */
        if (endpoint->direction == kUSBOut) {
            command->selector	= WRITE;
            command->direction	= kUSBOut;
	}
        else if (endpoint->direction == kUSBIn) {
            command->selector	= READ;
            command->direction	= kUSBIn;
	}
	else {
            err = kIOReturnBadArgument;
            break;
        }

        command->address	= address;
        command->endpoint	= endpoint->number;
        command->buffer		= buffer;
        command->completion	= *completion;
        command->startFrame	= frameStart;
        command->numFrames	= numFrames;
        command->frameList	= frameList;
        command->status		= kIOReturnInvalid;

        if (_commandGate == 0)
        {
            err = kIOReturnInternalError;
            break;
        }

        if ((err = _commandGate->runAction(doIsocTransfer, command)))
            break;

        return(err);

    } while (0);

    /* Free/give back the command */
    IOFree(command, sizeof(IOUSBIsocCommand));

    return (err);
}
