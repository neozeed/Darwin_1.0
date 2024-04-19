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
 * IOATAPICDDriveNub.cpp
 *
 * This subclass implements a relay to a protocol and device-specific
 * provider.
 *
 * HISTORY
 * 2-Sep-1999		Joe Liu (jliu) created.
 */

#include <IOKit/IOLib.h>
#include <IOKit/storage/ata/IOATAPICDDriveNub.h>
#include <IOKit/storage/ata/IOATAPICDDrive.h>

#define	super	IOCDDriveNub
OSDefineMetaClassAndStructors( IOATAPICDDriveNub, IOCDDriveNub )

// --------------------------------------------------------------------------
// attach to provider.

bool IOATAPICDDriveNub::attach(IOService * provider)
{
    if (!super::attach(provider))
        return false;

    _provider = OSDynamicCast(IOATAPICDDrive, provider);
    if (_provider == 0) {
        IOLog("IOATAPICDDriveNub: attach; wrong provider type!\n");
        return false;
    }
	
	return true;
}

// --------------------------------------------------------------------------
// detach from provider.

void IOATAPICDDriveNub::detach(IOService * provider)
{
    if (_provider == provider)
		_provider = 0;

    super::detach(provider);
}

// --------------------------------------------------------------------------
// 

IOReturn IOATAPICDDriveNub::doAsyncReadWrite(IOMemoryDescriptor * buffer,
                                             UInt32               block,
                                             UInt32               nblks,
                                             gdCompletionFunction action,
                                             IOService *          target,
                                             void *               param)
{
    if (buffer->getDirection() == kIODirectionOut)
        return(kIOReturnNotWritable);

	return (_provider->doAsyncReadWrite(buffer,
                                        block,
                                        nblks,
                                        action,
                                        target,
                                        param));
}

IOReturn IOATAPICDDriveNub::doSyncReadWrite(IOMemoryDescriptor *buffer,
                                     UInt32 block,UInt32 nblks)
{
        return(kIOReturnUnsupported);	//xxx must be implemented!
}

// --------------------------------------------------------------------------
// 

IOReturn IOATAPICDDriveNub::doEjectMedia()
{
    return (_provider->doEjectMedia());
}

// --------------------------------------------------------------------------
// 

IOReturn IOATAPICDDriveNub::doLockUnlockMedia(bool doLock)
{
    return (_provider->doLockUnlockMedia(doLock));
}

// --------------------------------------------------------------------------
// 

IOReturn IOATAPICDDriveNub::executeCdb(struct cdbParams *params)
{
    return(kIOReturnUnsupported);		// xxx must be implemented!
}

// --------------------------------------------------------------------------
// 

char * IOATAPICDDriveNub::getVendorString()
{
    return (_provider->getVendorString());
}

// --------------------------------------------------------------------------
// 

char * IOATAPICDDriveNub::getProductString()
{
    return (_provider->getProductString());
}

// --------------------------------------------------------------------------
// 

char * IOATAPICDDriveNub::getRevisionString()
{
    return (_provider->getRevisionString());
}

// --------------------------------------------------------------------------
// 

char * IOATAPICDDriveNub::getAdditionalDeviceInfoString()
{
    return (_provider->getAdditionalDeviceInfoString());
}

// --------------------------------------------------------------------------
// 

IOReturn IOATAPICDDriveNub::reportBlockSize(UInt64 * blockSize)
{
    return (_provider->reportBlockSize(blockSize));
}

// --------------------------------------------------------------------------
// 

IOReturn IOATAPICDDriveNub::reportEjectability(bool * isEjectable)
{
    return (_provider->reportEjectability(isEjectable));
}

// --------------------------------------------------------------------------
// 

IOReturn IOATAPICDDriveNub::reportLockability(bool * isLockable)
{
    return (_provider->reportLockability(isLockable));
}

// --------------------------------------------------------------------------
// 

IOReturn IOATAPICDDriveNub::reportMediaState(bool * mediaPresent,
                                             bool * changed)    
{
    return (_provider->reportMediaState(mediaPresent, changed));
}

// --------------------------------------------------------------------------
// 

IOReturn IOATAPICDDriveNub::reportPollRequirements(bool * pollIsRequired,
                                                   bool * pollIsExpensive)
{
    return (_provider->reportPollRequirements(pollIsRequired, 
                                              pollIsExpensive));
}

// --------------------------------------------------------------------------
// 

IOReturn IOATAPICDDriveNub::reportMaxReadTransfer(UInt64   blockSize,
                                                  UInt64 * max)
{
    return (_provider->reportMaxReadTransfer(blockSize, max));
}

// --------------------------------------------------------------------------
// 

IOReturn IOATAPICDDriveNub::reportMaxValidBlock(UInt64 * maxBlock)
{
    return (_provider->reportMaxValidBlock(maxBlock));
}

// --------------------------------------------------------------------------
// 

IOReturn IOATAPICDDriveNub::reportRemovability(bool * isRemovable)
{
    return (_provider->reportRemovability(isRemovable));
}

// --------------------------------------------------------------------------
// Audio commands are rejected until our provider implements the
// functionality.

IOReturn IOATAPICDDriveNub::audioPause(bool pause)
{
    return (_provider->audioPause(pause));
}

IOReturn IOATAPICDDriveNub::audioScan(positioningType addressType,
                                      cdAddress       address,
                                      bool            reverse)
{
    return kIOReturnUnsupported;
}

IOReturn IOATAPICDDriveNub::readAudioData(positioningType addressType,
                                          cdAddress       address,
                                          UInt8           blockCount,
                                          UInt8 *         buffer)
{
    return kIOReturnUnsupported;
}

IOReturn IOATAPICDDriveNub::readAudioSubcodes(positioningType addressType,
                                              cdAddress       address,
                                              UInt8           blockCount,
                                              UInt8 *         buffer)
{
    return kIOReturnUnsupported;
}

IOReturn IOATAPICDDriveNub::readAudioWithQSubcode(positioningType addressType,
                                                  cdAddress       address,
                                                  UInt8           blockCount,
                                                  UInt8 *         buffer)
{
    return kIOReturnUnsupported;
}

IOReturn IOATAPICDDriveNub::readAudioWithAllSubcodes(
                                            positioningType addressType,
                                            cdAddress       address,
                                            UInt8           blockCount,
                                            UInt8 *         buffer)
{
    return kIOReturnUnsupported;
}

IOReturn IOATAPICDDriveNub::readHeader(UInt32              blockAddress,
                                       struct headerInfo * buffer)
{
    return (_provider->readHeader(blockAddress,buffer));
}

IOReturn IOATAPICDDriveNub::readISRC(UInt32  track,
                                     UInt8 * buffer,
                                     bool * found)
{
    return (_provider->readISRC(track,buffer,found));
}

IOReturn IOATAPICDDriveNub::readMCN(UInt8 * buffer, bool * found)
{
    return (_provider->readMCN(buffer,found));
}

IOReturn IOATAPICDDriveNub::readQSubcodes(struct qSubcodeTocInfo * buffer,
                                          UInt32                   bufSize)
{
    return kIOReturnUnsupported;
}

IOReturn IOATAPICDDriveNub::readSubcodeBuffer(UInt8 * buffer,
                                              bool    purge,
                                              UInt32  entryCount)
{
    return kIOReturnUnsupported;
}

IOReturn IOATAPICDDriveNub::readTheQSubcode(struct qSubcode * buffer)
{
    return (_provider->readTheQSubcode(buffer));
}

IOReturn IOATAPICDDriveNub::readTOC(struct rawToc * buffer,
                                    UInt32          length,
                                    tocFormat       format)
{
    return (_provider->readTOC(buffer, length, format));
}

IOReturn IOATAPICDDriveNub::setAudioStopAddress(positioningType addressType,
                                                cdAddress       address)
{
    return (_provider->setAudioStopAddress(addressType, address));
}

IOReturn IOATAPICDDriveNub::audioPlay(positioningType addressType,
                                      cdAddress       address,
                                      audioPlayMode   mode)
{
    return (_provider->audioPlay(addressType, address, mode));
}

IOReturn IOATAPICDDriveNub::audioTrackSearch(positioningType addressType,
                                             cdAddress       address,
                                             bool            startPlay,
                                             audioPlayMode   mode)
{
    return kIOReturnUnsupported;
}

IOReturn IOATAPICDDriveNub::getAudioStatus(struct audioStatus * status)
{
    return (_provider->getAudioStatus(status));
}

IOReturn IOATAPICDDriveNub::readAudioVolume(UInt8 * leftVolume,
                                            UInt8 * rightVolume)
{
    return (_provider->readAudioVolume(leftVolume, rightVolume));
}

IOReturn IOATAPICDDriveNub::setVolume(UInt8 leftVolume, UInt8 rightVolume)
{
    return (_provider->setVolume(leftVolume, rightVolume));
}
