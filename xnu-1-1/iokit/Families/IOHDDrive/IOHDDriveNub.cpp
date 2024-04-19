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
#include <IOKit/IOLib.h>
#include <IOKit/storage/IOHDDriveNub.h>

#define	super	IOService
OSDefineMetaClass(IOHDDriveNub,IOService)
OSDefineAbstractStructors(IOHDDriveNub,IOService)

bool
IOHDDriveNub::init(OSDictionary * properties)
{
    bool result;

    // IOLog("%s[IOHDDriveNub]::init.\n",""/*getName()*/);
    
    result = super::init(properties);
    if (result) {
        result = setProperty(kDeviceTypeProperty,kDeviceTypeHardDisk);
        if (result == false) {
            IOLog("%s[IOHDDriveNub]::init; can't set property\n",""/*getName()*/);
        }
    } else {
        IOLog("%s[IOHDDriveNub]::init; superclass failed to init\n",""/*getName()*/);
    }
    
    return(result);
}
