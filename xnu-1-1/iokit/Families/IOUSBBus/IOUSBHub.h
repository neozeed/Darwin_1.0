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


#ifndef _IOKIT_IOUSBHUB_H
#define _IOKIT_IOUSBHUB_H

#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>
#include <IOKit/IOBufferMemoryDescriptor.h>

#include <IOKit/usb/USB.h>
#include <IOKit/usb/USBHub.h>

enum{
      kErrataCaptiveOK = 1
};


class IOUSBController;
class IOUSBDevice;
class IOUSBInterface;
class IOUSBPipe;

class IOUSBHub : public IOService
{
    OSDeclareDefaultStructors(IOUSBHub)

    friend class 		IOUSBHubPort;

    IOUSBController *		_bus;
    IOUSBDevice *		_device;
    IOUSBInterface *		_hubInterface;
    IOUSBConfigurationDescriptor *_configDescriptor;
    IOUSBHubDescriptor		_hubDescriptor;
    USBDeviceAddress   		_address;
    IOUSBHubStatus       	_hubStatus;
    IOUSBPipe * 		_interruptPipe;
    IOBufferMemoryDescriptor *	_buffer;

    // Power stuff
    bool			_busPowered;
    bool			_selfPowered;
    bool			_busPowerGood;
    bool       			_selfPowerGood;
    UInt32			_powerForCaptive;

    // Port stuff
    IOUSBHubPort **	   	_ports;		// Allocated at runtime
    UInt8			_numCaptive;

    UInt32			_errataBits;

public:

    bool		init(OSDictionary * propTable );
    virtual bool	start(IOService * provider);
    virtual void 	stop(IOService *  provider);

    // Hub functions
    void	UnpackPortFlags(void);
    void 	CountCaptivePorts(void);
    bool	CheckPortPowerRequirements(void);
    bool	AllocatePortMemory(void);
    bool	StartPorts(void);
    bool 	StopPorts(void);
    bool	StartHandler(void);
    bool	statusChanged(void);
    const UInt8 * getStatusChanged(void);

    IOReturn	GetHubDescriptor(IOUSBHubDescriptor *desc);
    IOReturn	GetHubStatus(IOUSBHubStatus *status);
    IOReturn	ClearHubFeature(UInt16 feature);

    IOReturn	GetPortStatus(IOUSBHubPortStatus *status, UInt16 port);
    IOReturn	GetPortState(UInt8 *state, UInt16 port);
    IOReturn	SetPortFeature(UInt16 feature, UInt16 port);
    IOReturn	ClearPortFeature(UInt16 feature, UInt16 port);

    void PrintHubDescriptor(IOUSBHubDescriptor *desc);

    void 	fatalError(IOReturn err, char *str);
};

/*****************************************************
 * Port Stuff
 *****************************************************/


typedef IOReturn (IOUSBHubPort::*ChangeHandlerFuncPtr)(UInt16 changeFlags);


typedef struct {
    ChangeHandlerFuncPtr handler;
    UInt32 bit;
    UInt32 clearFeature;
} portStatusChangeVector;

enum{
    kNumChangeHandlers = 5
};


class IOUSBHubPort
{
    IOUSBController		*_bus;
    IOUSBHub 			*_hub;
    IOUSBHubDescriptor		*_hubDesc;
    IOUSBDevice			*_portDevice;
    bool			_devZero;

    portStatusChangeVector	_changeHandler[kNumChangeHandlers];

public:

    IOUSBDeviceDescriptor	_desc;
    UInt8			_speed;	// 1 = kUSBLowSpeed, 0 = kUSBHighSpeed
    UInt32			_portPowerAvailable;

    int 			_portNum;

    void        init(IOUSBHub *	parent,
                     int	portNum,
                     UInt32	powerAvailable);
    IOReturn	start(void);
    void 	stop(void);

    IOReturn addDevice(void);
    void removeDevice(void);

    // Reset the device, then restore the old address
    IOReturn resetDevice();

    bool statusChanged(void);
    IOReturn statusChangeHandler(UInt16 changeFlags);

    void initPortVectors(void);
    void setPortVector(ChangeHandlerFuncPtr	routine,
                       UInt32 			condition);
    IOReturn defaultOverCrntChangeHandler(UInt16 changeFlags);
    IOReturn defaultResetChangeHandler(UInt16 changeFlags);
    IOReturn defaultSuspendChangeHandler(UInt16 changeFlags);
    IOReturn defaultEnableChangeHandler(UInt16 changeFlags);
    IOReturn defaultConnectionChangeHandler(UInt16 changeFlags);
    IOReturn addDeviceResetChangeHandler(UInt16 changeFlags);
    IOReturn handleResetDevice(UInt16 changeFlags);

    void fatalError(IOReturn err, char *str);
};

#endif _IOKIT_IOUSBHUB_H
