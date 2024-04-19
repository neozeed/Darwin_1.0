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


#ifndef _IOKIT_IOUSBNUB_H
#define _IOKIT_IOUSBNUB_H

#include <IOKit/IOService.h>
#include <libkern/c++/OSData.h>
#include <IOKit/IOMemoryDescriptor.h>

#include <IOKit/usb/USB.h>

class IOUSBController;
class IOUSBPipe;

/*!
    @struct IOUSBDevRequest
    parameter block for control requests, using a simple pointer
    for the data to be transferred.
    @field rdDirection Direction of data part of request: kUSBIn or kUSBOut
    @field rqType Request type: kUSBStandard, kUSBClass or kUSBVendor
    @field rqRecipient Target of the request: kUSBDevice, kUSBInterface,
	kUSBEndpoint or kUSBOther
    @field bRequest Request code
    @field wValue 16 bit parameter for request, low byte first
    @field wIndex 16 bit parameter for request, low byte first
    @field wLength Length of data part of request, 16 bits, low byte first
    @field pData Pointer to data for request
    @field wLenDone Set by standard completion routine to number of data bytes
	actually transferred
*/
typedef struct {
    UInt8   	rqDirection:1;	// bmRequestType:7
    UInt8   	rqType:2;	// bmRequestType:6..5
    UInt8   	rqRecipient:5;	// bmRequestType:4..0
    UInt8 	bRequest;
    USBWord 	wValue;
    USBWord 	wIndex;
    USBWord	wLength;
    void * 	pData;		// data pointer
    UInt32	wLenDone;	// # bytes transferred
} IOUSBDevRequest;
typedef IOUSBDevRequest * IOUSBDeviceRequestPtr;

/*!
    @struct IOUSBDevRequestDesc
    parameter block for control requests, using a memory descriptor
    for the data to be transferred
    @field rdDirection Direction of data part of request: kUSBIn or kUSBOut
    @field rqType Request type: kUSBStandard, kUSBClass or kUSBVendor
    @field rqRecipient Target of the request: kUSBDevice, kUSBInterface,
        kUSBEndpoint or kUSBOther
    @field bRequest Request code
    @field wValue 16 bit parameter for request, low byte first
    @field wIndex 16 bit parameter for request, low byte first
    @field wLength Length of data part of request, 16 bits, low byte first
    @field pData Pointer to memory descriptor for data for request
    @field wLenDone Set by standard completion routine to number of data bytes
     actually transferred
*/
typedef struct {
    UInt8   	rqDirection:1;	// bmRequestType:7
    UInt8   	rqType:2;	// bmRequestType:6..5
    UInt8   	rqRecipient:5;	// bmRequestType:4..0
    UInt8 	bRequest;
    USBWord 	wValue;
    USBWord 	wIndex;
    USBWord	wLength;
    IOMemoryDescriptor *pData;	// data pointer
    UInt32	wLenDone;	// # bytes transferred
} IOUSBDevRequestDesc;

/*!
    @class IOUSBNub
    @abstract abstract base class for IOUSBDevice and IOUSBInterface
    @discussion This class provides functionality that is useful for both
    a device driver (which would attach to an IOUSBDevice) and an interface driver
    (which would attach to an IOUSBInterface).
*/

class IOUSBNub : public IOService
{
    OSDeclareDefaultStructors(IOUSBNub)

protected:
    USBDeviceAddress		_address;
    IOUSBController *	     	_controller;
    IOUSBPipe *			_pipeZero;
    IOUSBDeviceDescriptor 	_descriptor;
    UInt32			_busPowerAvailable;
    UInt8			_speed;

public:
    virtual bool finalize(IOOptionBits options);

    /*!
        @struct FindInterfaceRequest
        Parameter block for finding interfaces in a device.
	Initialize each field to the desired value
        before calling findNextInterface, set a field to 0 if any value is OK.
        @field theClass Requested class
        @field subClass Requested subclass
        @field protocol Requested protocol
        @field maxPower max power consumption in 2mA units
        @field busPowered 1 = not bus powered, 2 = bus powered
        @field selfPowered 1 = not self powered, 2 = self powered
        @field remoteWakeup 1 = doesn't support remote wakeup, 2 = does
    */
    struct FindInterfaceRequest {
        UInt8 theClass;		// requested class,    0 = don't care
        UInt8 subClass;		// requested subclass; 0 = don't care
        UInt8 protocol;		// requested protocol; 0 = don't care
        UInt8 maxPower;		// max power in 2ma increments; 0 = don't care
        UInt8 busPowered:2;	// 1 = not bus powered, 2 = bus powered,
        UInt8 selfPowered:2;	// 1 = not self powered, 2 = self powered,
        UInt8 remoteWakeup:2;	// 1 = doesn't support remote wakeup; 2 = does
        UInt8 reserved:2;
    };

    /*!
    	@function findNextDescriptor
        @abstract find next descriptor in configuration list of given type
	(kUSBAnyDesc matches any type).
        @discussion call this function with a pointer to a descriptor in a descriptor list,
	for example the descriptor list returned by IOUSBDevice::getFullConfigurationDescriptor().
        Returns NULL if no more descriptors match descType.
        @param cur current descriptor in list
        @param descType descriptor type to return (kUSBAnyDesc to match any type)
	@result Pointer to the next matching descriptor, or NULL if no more match.
    */
    static const IOUSBDescriptorHeader *findNextDescriptor(const void *cur, UInt8 descType);

    /*! 
	@function GetStringDescriptor
	Get a string descriptor as ASCII, in the specified language (default is US English)
	@param index Index of the string descriptor to get.
	@param buf Pointer to place to store ASCII string
	@param maxLen Size of buffer pointed to by buf
        @param lang Language to get string in (default is US English)
    */
    virtual IOReturn GetStringDescriptor(UInt8 index,
                                        char *buf, int maxLen, UInt16 lang=0x409);

    // Lowlevel requests for non-standard device requests
    /*!
	@function deviceRequest
        @abstract execute a device request
        @param request The parameter block to send to the device
	@completion Function to call when request completes. If omitted then deviceRequest()
	executes synchronously, blocking until the request is complete.
    */
    virtual IOReturn deviceRequest(IOUSBDevRequest	*request,
                            IOUSBCompletion	*completion = 0);

    // Same but with a memory descriptor
    virtual IOReturn deviceRequest(IOUSBDevRequestDesc	*request,
                            IOUSBCompletion	*completion = 0);

    /*!
	@function GetConfiguration
	Gets the current configuration from the device
	@param configNum Pointer to place to store configuration value
    */
    virtual IOReturn GetConfiguration(UInt8 *configNumber);

    /*!
        @function GetConfiguration
        Gets the current configuration from the device
        @param configNum Pointer to place to store configuration value
    */
    virtual IOReturn GetDeviceStatus(USBStatus *status);

    // Access to addressing and cached info
    /*!
        @function address
        returns the bus address of the device
    */
    virtual USBDeviceAddress		address(void);
    /*!
        @function deviceDescriptor
        returns a pointer to the device descriptor
    */
    virtual const IOUSBDeviceDescriptor *deviceDescriptor(void);
    /*!
        @function bus
        returns a pointer to the bus object for the device
    */
    virtual IOUSBController *	     	bus(void);
    /*!
        @function busPowerAvailable
        returns the power available to the device, in units of 2mA
    */
    virtual UInt32 			busPowerAvailable( void );

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

    /*!
        @function pipeZero
        returns a pointer to the device's default pipe
    */
    virtual IOUSBPipe *			pipeZero();

    virtual bool init(OSDictionary * propTable);
    virtual bool matchPropertyTable(OSDictionary * table);
};

#ifdef __cplusplus
extern "C" {
#endif

void printDescriptor(IOUSBDescriptorHeader *desc);
void printDeviceDescriptor(IOUSBDeviceDescriptor *desc);
void printConfigDescriptor(IOUSBConfigurationDescriptor *cd);
void printEndpointDescriptor(IOUSBEndpointDescriptor *ed);
void printInterfaceDescriptor(IOUSBInterfaceDescriptor *id);

#ifdef __cplusplus
}
#endif

#endif /* _IOKIT_IOUSBNUB_H */
