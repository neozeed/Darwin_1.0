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


#ifndef _IOKIT_IOUSBPIPE_H
#define _IOKIT_IOUSBPIPE_H

#include <IOKit/IOService.h>
#include <IOKit/IOMemoryDescriptor.h>

#include <IOKit/usb/USB.h>

#include <IOKit/usb/IOUSBController.h>

/*!
    @class IOUSBPipe
    @abstract The object representing an open pipe for a device.
*/
class IOUSBPipe : public OSObject
{
    OSDeclareDefaultStructors(IOUSBPipe)

protected:

    const IOUSBEndpointDescriptor *	_descriptor;
    IOUSBController::Endpoint		_endpoint;	// tidied up version of descriptor
    IOUSBController *			_controller;
    USBDeviceAddress			_address;
    UInt8 				_status;	// stalled, active

    virtual void free();

public:

    virtual bool initToEndpoint(const IOUSBEndpointDescriptor *endpoint, UInt8 speed,
                                USBDeviceAddress address, IOUSBController * controller);
    static IOUSBPipe *toEndpoint(const IOUSBEndpointDescriptor *endpoint, UInt8 speed,
                                 USBDeviceAddress address, IOUSBController * controller);

    // Controlling pipe state
    /*!
        @function status
	Get the current state of the pipe (kIOUSBPipeStalled if stalled or 0 if active).
	We clear a pipe stall as soon as it is detected, so in practice this routine
	always returns zero
    */
    virtual UInt8 status(void);
    /*!
        @function abort
	abort a pipe, causing all outstanding I/O to complete
	with return code kIOReturnAborted
    */
    virtual IOReturn abort(void);
    /*!
        @function reset
        reset a pipe, causing all outstanding I/O to complete
        with return code kIOReturnAborted, and clear any stall.
    */
    virtual IOReturn reset(void);
    /*!
        @function clearStall
        clear any stall.
    */
    virtual IOReturn clearStall(void);

    //
    // Transferring Data
    //
    
    // Transfer data over Bulk or Interrupt pipes.
    /*!
        @function read
        Read from an interrupt or bulk endpoint
        @param buffer place to put the transferred data
        @param completion describes action to take when buffer has been filled
        @param bytesRead returns total bytes read for synchronous reads
    */
    virtual IOReturn read(IOMemoryDescriptor *	buffer,
                          IOUSBCompletion *	completion = 0,
			  IOByteCount *		bytesRead = 0);

    /*!
        @function write
        Write to an interrupt or bulk endpoint
        @param buffer place to get the transferred data
        @param completion describes action to take when buffer has been emptied
    */
    virtual IOReturn write(IOMemoryDescriptor *	buffer,
                           IOUSBCompletion *	completion = 0);

    // Transfer data over Isochronous pipes
    /*!
        @function read
        Read from an isochronous endpoint
        @param buffer place to put the transferred data
        @param frameStart USB frame number of the frame to start transfer
	@param numFrames Number of frames to transfer
	@param frameList Bytes to transfer and result for each frame
        @param completion describes action to take when buffer has been filled
    */
    virtual IOReturn read(IOMemoryDescriptor *	buffer,
                          UInt64 frameStart, UInt32 numFrames, IOUSBIsocFrame *frameList,
                          IOUSBIsocCompletion *	completion = 0);
    /*!
        @function write
        Write to an interrupt or bulk endpoint
        @param buffer place to get the transferred data
        @param frameStart USB frame number of the frame to start transfer
        @param numFrames Number of frames to transfer
        @param frameList Bytes to transfer and result for each frame
        @param completion describes action to take when buffer has been emptied
    */
    virtual IOReturn write(IOMemoryDescriptor *	buffer,
                           UInt64 frameStart, UInt32 numFrames, IOUSBIsocFrame *frameList,
                           IOUSBIsocCompletion * completion = 0);

    /*!
        @function controlRequest
        Make a control request
        There are two versions of this method, one uses a simple void *
        to point to the data portion of the transfer, the other uses an
        IOMemoryDescriptor to point to the data.
        @param request parameter block for the control request
        @param completion describes action to take when the request has been executed
    */
    // Do a control request over a Control pipe, using a memory descriptor
    virtual IOReturn controlRequest(IOUSBDevRequestDesc	*request,
                                   IOUSBCompletion	*completion = 0);

    // Do a control request over a Control pipe, using a simple buffer
    virtual IOReturn controlRequest(IOUSBDevRequest	*request,
                                IOUSBCompletion	*completion = 0);

    /*!
        @function GetBandwidthAvailable
        Returns the available bandwidth (in bytes) per frame for
        isochronous transfers.
        @result maximum number of bytes that a new iso pipe could transfer
        per frame given current allocations.
    */
    virtual UInt32 GetBandwidthAvailable();

    /*!
        @function GetFrameNumber
        Returns the full current frame number of the bus the device is
        attached to
        @result The frame number.
    */
    virtual UInt64 GetFrameNumber();

    // Return cached pointer to an associated descriptor.
    // Don't free returned pointer.
    /*!
        @function findNextAssociatedDescriptor
        Find the next descriptor of the requested type associated with the endpoint.
        @param current Descriptor to start searching from, NULL to start from beginning of list.
        @param type Descriptor type to search for, or kUSBAnyDesc to return any descriptor type.
    */
    virtual const IOUSBDescriptorHeader *
    findNextAssociatedDescriptor(const void *current, UInt8 type);

    /*!
        @function endpoint
        returns a pointer to the Endpoint structure for the pipe.
    */
    const IOUSBController::Endpoint *	endpoint();
    /*!
        @function endpointDescriptor
        returns the endpoint descriptor for the pipe.
    */
    const IOUSBEndpointDescriptor *	endpointDescriptor();
    /*!
        @function direction
        returns the direction of the pipe:kUSBOut/kUSBIn for a bulk or interrupt pipe,
	kUSBAnyDirn for a control pipe.
    */
    UInt8				direction();
    /*!
        @function type
        returns the pipe type: kUSBControl, kUSBBulk or kUSBInterrupt.
    */
    UInt8				type();
    /*!
        @function endpointNumber
        returns the endpoint number in the device that the pipe is connected to.
    */
    UInt8				endpointNumber();
    /*!
        @function address
        returns the bus address of the pipe's device
    */
    virtual USBDeviceAddress		address(void);
};

#endif /* _IOKIT_IOUSBPIPE_H */
