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

#include <libkern/OSByteOrder.h>
#include "AppleComposite.h"
#include <IOKit/usb/IOUSBInterface.h>

#include <IOKit/assert.h>

#define super IOService
#define DEBUGGING_LEVEL 0

OSDefineMetaClassAndStructors(AppleComposite, IOService)

bool AppleComposite::start(IOService * provider)
{
    IOReturn			err = 0;
    UInt8			configNum;
    IOUSBDevice::FindInterfaceRequest	req;
    IOUSBInterface *		interface;
    IOUSBDevice *		device;

    if( !super::start(provider))
        return (false);

    // get my device
    device = OSDynamicCast(IOUSBDevice, provider);
    if(!device)
	return false;

    do {
#if (DEBUGGING_LEVEL > 0)
        IOLog("%s: USB Generic Composite @ %d\n", getName(), _device->address());
#endif
        // Find the first config/interface
        if (device->deviceDescriptor()->numConf < 1)
        {
            IOLog("%s: no composite configurations\n", getName());
            continue;
        }

        req.theClass = req.subClass = req.protocol = 0;
        req.maxPower = 0;
        if ((interface = device->findNextInterface(NULL, &req)) == 0)
        {
            IOLog("%s: no interface found\n", getName());
            continue;
        }

        configNum = interface->getConfigValue();

        // set my configuration
        if ((err = device->SetConfiguration(configNum)))
            continue;

#if (DEBUGGING_LEVEL > 0)
        printInterfaceDescriptor(interface->descriptor);
#endif
        // Set up interface for matching, and release it.
	// it's also held by the device (which it is attached to)
        interface->registerService();
        interface->release();

	// create a new IOUSBInterface nub for each interface
        // with the same configuration.
	do {
            req.theClass = req.subClass = req.protocol = 0;
            req.maxPower = 0;
            interface = device->findNextInterface(interface, &req);
            if (interface == 0)
            {
                break;
            }
            if(interface->getConfigValue() == configNum)
                interface->registerService();
            interface->release();
        } while (true);

        return(true);

    } while (false);

    IOLog("%s: aborting startup.  err = %d\n", getName(), err);
    
    return(false);
}


