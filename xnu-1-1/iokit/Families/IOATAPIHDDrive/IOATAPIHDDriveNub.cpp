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
 * Copyright (c) 1999 Apple Computer, Inc.  All rights reserved. 
 *
 * IOATAPIHDDriveNub.cpp
 *
 * This subclass implements a relay to a protocol and device-specific
 * provider.
 *
 * HISTORY
 * 2-Sep-1999		Joe Liu (jliu) created.
 */

#include <IOKit/IOLib.h>
#include <IOKit/storage/ata/IOATAPIHDDriveNub.h>
#include <IOKit/storage/ata/IOATAPIHDDrive.h>

#define	super	IOHDDriveNub
OSDefineMetaClassAndStructors( IOATAPIHDDriveNub, IOHDDriveNub )

// --------------------------------------------------------------------------
// attach to provider.

bool IOATAPIHDDriveNub::attach(IOService * provider)
{    
    if (!super::attach(provider))
        return false;

    _provider = OSDynamicCast(IOATAPIHDDrive, provider);
    if (_provider == 0) {
        IOLog("IOATAPIHDDriveNub: attach; wrong provider type!\n");
        return false;
	}

	return true;
}

// --------------------------------------------------------------------------
// detach from provider.

void IOATAPIHDDriveNub::detach(IOService * provider)
{
    if (_provider == provider)
		_provider = 0;

    super::detach(provider);
}

// --------------------------------------------------------------------------
// 

IOReturn IOATAPIHDDriveNub::doAsyncReadWrite(IOMemoryDescriptor * buffer,
                                             UInt32               block,
                                             UInt32               nblks,
                                             gdCompletionFunction action,
                                             IOService *          target,
                                             void *               param)
{
	return (_provider->doAsyncReadWrite(buffer,
                                        block,
                                        nblks,
                                        action,
                                        target,
                                        param));
}

IOReturn IOATAPIHDDriveNub::doSyncReadWrite(IOMemoryDescriptor *buffer,
                                     UInt32 block,UInt32 nblks)
{
        return(kIOReturnUnsupported);	//xxx must be implemented!
}


// --------------------------------------------------------------------------
// 

IOReturn IOATAPIHDDriveNub::doEjectMedia()
{
    return (_provider->doEjectMedia());
}

// --------------------------------------------------------------------------
// 

IOReturn IOATAPIHDDriveNub::doFormatMedia(UInt64 byteCapacity)  
{
    return (_provider->doFormatMedia(byteCapacity));
}

// --------------------------------------------------------------------------
// 

UInt32
IOATAPIHDDriveNub::doGetFormatCapacities(UInt64 * capacities,
                                         UInt32   capacitiesMaxCount) const
{
    return (_provider->doGetFormatCapacities(capacities, capacitiesMaxCount));
}

// --------------------------------------------------------------------------
// 

IOReturn IOATAPIHDDriveNub::doLockUnlockMedia(bool doLock)
{
    return (_provider->doLockUnlockMedia(doLock));
}

// --------------------------------------------------------------------------
// 

IOReturn IOATAPIHDDriveNub::doSynchronizeCache()
{
    if (_provider)
        return (_provider->doSynchronizeCache());
	
	return kIOReturnSuccess;
}

IOReturn IOATAPIHDDriveNub::executeCdb(struct cdbParams *params)
{
    return(kIOReturnUnsupported);		// xxx must be implemented!
}

// --------------------------------------------------------------------------
// 

char * IOATAPIHDDriveNub::getVendorString()
{
    return (_provider->getVendorString());
}

// --------------------------------------------------------------------------
// 

char * IOATAPIHDDriveNub::getProductString()
{
    return (_provider->getProductString());
}

// --------------------------------------------------------------------------
// 

char * IOATAPIHDDriveNub::getRevisionString()
{
    return (_provider->getRevisionString());
}

// --------------------------------------------------------------------------
// 

char * IOATAPIHDDriveNub::getAdditionalDeviceInfoString()
{
    return (_provider->getAdditionalDeviceInfoString());
}

// --------------------------------------------------------------------------
// 

IOReturn IOATAPIHDDriveNub::reportBlockSize(UInt64 * blockSize)
{
    return (_provider->reportBlockSize(blockSize));
}

// --------------------------------------------------------------------------
// 

IOReturn IOATAPIHDDriveNub::reportEjectability(bool * isEjectable)
{
    return (_provider->reportEjectability(isEjectable));
}

// --------------------------------------------------------------------------
// 

IOReturn IOATAPIHDDriveNub::reportLockability(bool * isLockable)
{
    return (_provider->reportLockability(isLockable));
}

// --------------------------------------------------------------------------
// 

IOReturn IOATAPIHDDriveNub::reportPollRequirements(bool * pollIsRequired,
                                                   bool * pollIsExpensive)
{
    return (_provider->reportPollRequirements(pollIsRequired, 
                                              pollIsExpensive));
}

// --------------------------------------------------------------------------
// 

IOReturn IOATAPIHDDriveNub::reportMaxReadTransfer(UInt64   blockSize,
                                                  UInt64 * max)
{
    return (_provider->reportMaxReadTransfer(blockSize, max));
}

// --------------------------------------------------------------------------
// 

IOReturn IOATAPIHDDriveNub::reportMaxValidBlock(UInt64 * maxBlock)
{
    return (_provider->reportMaxValidBlock(maxBlock));
}

// --------------------------------------------------------------------------
// 

IOReturn IOATAPIHDDriveNub::reportMaxWriteTransfer(UInt64   blockSize, 
                                                   UInt64 * max)
{
    return (_provider->reportMaxWriteTransfer(blockSize, max));
}

// --------------------------------------------------------------------------
// 

IOReturn IOATAPIHDDriveNub::reportMediaState(bool * mediaPresent,
                                             bool * changed)    
{
    return (_provider->reportMediaState(mediaPresent,changed));
}

// --------------------------------------------------------------------------
// 

IOReturn IOATAPIHDDriveNub::reportRemovability(bool * isRemovable)
{
    return (_provider->reportRemovability(isRemovable));
}

// --------------------------------------------------------------------------
// 

IOReturn IOATAPIHDDriveNub::reportWriteProtection(bool * isWriteProtected)
{
    return (_provider->reportWriteProtection(isWriteProtected));
}
