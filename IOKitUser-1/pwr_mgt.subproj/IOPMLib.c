/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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
#include <sys/cdefs.h>

#include <mach/mach.h>
#include <IOKit/iokitmig.h> 	// mig generated

#include <IOKit/pwr_mgt/IOPM.h>
#include "IOPMLib.h"

io_connect_t IOPMFindPowerManagement( mach_port_t master_device_port )
{
io_connect_t	fb;
kern_return_t	kr;
io_iterator_t	enumer;
io_connect_t	obj = NULL;

kr = IORegistryCreateIterator( master_device_port, kIOServicePlane, TRUE, &enumer);

if ( kIOReturnSuccess == kr ) {
    while( (obj = IOIteratorNext( enumer ))) {
        if( IOObjectConformsTo( obj, "IOPMrootDomain" )) {
            break;
        }
        IOObjectRelease( obj );
    }
}
kr = IOObjectRelease(enumer);

if( obj ) {
    kr = IOServiceOpen( obj,mach_task_self(), 0, &fb);
    if ( kr == kIOReturnSuccess ) {
    return fb;
    }
}
return 0;
}


IOReturn IOPMGetAggressiveness ( io_connect_t fb, unsigned long type, unsigned long * aggressiveness )
{
    mach_msg_type_number_t	len = 1;
    IOReturn	err;
    int		param = type;

    err = io_connect_method_scalarI_scalarO( (io_connect_t) fb, kPMGetAggressiveness, &param, 1, (int *)aggressiveness, &len);
    return err;
}


IOReturn IOPMSetAggressiveness ( io_connect_t fb, unsigned long type, unsigned long aggressiveness )
{
    mach_msg_type_number_t	len = 0;
    IOReturn	err;
   int		params[2];

    params[0] = (int)type;
    params[1] = (int)aggressiveness;

    err = io_connect_method_scalarI_scalarO( (io_connect_t) fb, kPMSetAggressiveness, params, 2, NULL, &len);
    return err;
}


IOReturn IOPMSleepSystem ( io_connect_t fb )
{
    mach_msg_type_number_t	len = 0;

    return io_connect_method_scalarI_scalarO( (io_connect_t) fb, kPMSleepSystem, NULL, 0, NULL, &len);
}


IOReturn IOPMWakeSystem ( io_connect_t fb )
{
    mach_msg_type_number_t	len = 0;

    return io_connect_method_scalarI_scalarO( (io_connect_t) fb, kPMWakeSystem, NULL, 0, NULL, &len);
}

IOReturn IOPMCopyBatteryInfo( mach_port_t masterPort, CFArrayRef * oInfo )
{
    CFArrayRef		info;
    io_registry_entry_t	entry;
    CFDictionaryRef	props;
    IOReturn		kr = kIOReturnUnsupported;
    
    entry = IORegistryEntryFromPath( masterPort,
                                     kIODeviceTreePlane ":mac-io/battery");
    if (entry) {
        
        kr = IORegistryEntryCreateCFProperties( entry, &props,
                                                kCFAllocatorDefault, kNilOptions);
        if (kIOReturnSuccess == kr) {
            info = (CFArrayRef) CFDictionaryGetValue(props, CFSTR(kIOBatteryInfoKey));
            if(info) {
                CFRetain(info);
                *oInfo = info;
            }   
            CFRelease(props);
        }
        IOObjectRelease(entry);
    }
    return kr;
}

