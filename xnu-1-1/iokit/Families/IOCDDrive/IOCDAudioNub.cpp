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
#include "IOCDAudioNub.h"
#include "IOCDAudioNubClient.h"
#include <IOKit/storage/IOCDDrive.h>

#define	super	IOService
OSDefineMetaClassAndStructors(IOCDAudioNub,IOService)

IOReturn
IOCDAudioNub::newUserClient(task_t owningTask,
                  void *            /* security_id */,
                  UInt32            type,
                  IOUserClient **   handler )
    
{   
    IOReturn err = kIOReturnSuccess;
    IOCDAudioNubClient *      client;
 
    kprintf("IOCDAudioNub::newUserClient\n");
    if( type != 11) {
        kprintf("IOCDAudioNub::newUserClient bad argument\n");
        return( kIOReturnBadArgument);
    }
    
    client = IOCDAudioNubClient::withTask(owningTask);
    
    if( !client || (false == client->attach( this )) ||
        (false == client->start( this )) ) {
        if(client) {  
            client->detach( this );
            client->release();
        }
        err = kIOReturnNoMemory;
    }
    
    *handler = client;
    return( err );
}       

bool
IOCDAudioNub::attach(IOService *provider)
{
    _provider = (IOCDDrive *)provider;

    return(super::attach(provider));
}

IOReturn
IOCDAudioNub::audioPlay(positioningType addressType,cdAddress address,
                                          audioPlayMode mode)
{
    return(_provider->audioPlay(addressType,address,mode));
}

IOReturn
IOCDAudioNub::audioPause(bool pause)
{
    return(_provider->audioPause(pause));
}

IOReturn
IOCDAudioNub::audioScan(positioningType addressType,cdAddress address,
                        bool reverse)
{
    return(_provider->audioScan(addressType,address,reverse));
}

IOReturn
IOCDAudioNub::audioTrackSearch(positioningType addressType,cdAddress address,
                                                 bool startPlay,audioPlayMode mode)
{
    return(_provider->audioTrackSearch(addressType,address,startPlay,mode));
}

IOReturn
IOCDAudioNub::getAudioStatus(struct audioStatus *status)
{
    return(_provider->getAudioStatus(status));
}

IOReturn
IOCDAudioNub::readAudioData(positioningType addressType,cdAddress address,
                                    UInt8 blockCount,UInt8 *buffer)
{
    return(_provider->readAudioData(addressType,address,blockCount,buffer));
}

IOReturn
IOCDAudioNub::readAudioSubcodes(positioningType addressType,cdAddress address,
                                    UInt8 blockCount,UInt8 *buffer)
{
    return(_provider->readAudioSubcodes(addressType,address,blockCount,buffer));
}

IOReturn
IOCDAudioNub::readAudioVolume(UInt8 *leftVolume,UInt8 *rightVolume)
{
    return(_provider->readAudioVolume(leftVolume,rightVolume));
}

IOReturn
IOCDAudioNub::readAudioWithQSubcode(positioningType addressType,cdAddress address,
                                    UInt8 blockCount,UInt8 *buffer)
{
    return(_provider->readAudioWithQSubcode(addressType,address,blockCount,buffer));
}

IOReturn
IOCDAudioNub::readAudioWithAllSubcodes(positioningType addressType,cdAddress address,
                                    UInt8 blockCount,UInt8 *buffer)
{
    return(_provider->readAudioWithAllSubcodes(addressType,address,blockCount,buffer));
}

IOReturn
IOCDAudioNub::readEntireTOC(struct macEntireToc *buffer)
{
    return(_provider->readEntireTOC(buffer));
}

IOReturn
IOCDAudioNub::readHeader(UInt32 blockAddress,struct headerInfo *buffer)
{
    return(_provider->readHeader(blockAddress,buffer));
}

IOReturn
IOCDAudioNub::readISRC(UInt32 track,UInt8 *buffer,bool *found)
{
    return(_provider->readISRC(track,buffer,found));
}

IOReturn
IOCDAudioNub::readLeadOutAddress(cdAddress *buffer)
{
    return(_provider->readLeadOutAddress(buffer));
}

IOReturn
IOCDAudioNub::readMCN(UInt8 *buffer,bool *found)
{
    return(_provider->readMCN(buffer,found));
}

IOReturn
IOCDAudioNub::readQSubcodes(struct qSubcodeTocInfo *buffer,UInt32 bufSize)
{
    return(_provider->readQSubcodes(buffer,bufSize));
}

IOReturn
IOCDAudioNub::readSessionInfo(struct sessionInfo *info)
{
    return(_provider->readSessionInfo(info));
}

IOReturn
IOCDAudioNub::readSubcodeBuffer(UInt8 *buffer,bool purge,UInt32 entryCount)
{
    return(_provider->readSubcodeBuffer(buffer,purge,entryCount));
}

IOReturn
IOCDAudioNub::readTheQSubcode(struct qSubcode *buffer)
{
    return(_provider->readTheQSubcode(buffer));
}

IOReturn
IOCDAudioNub::readTrackInfo(UInt32 startingTrack,
                                   	struct trackTypeInfo *buf,UInt32 bufSize)
{
    return(_provider->readTrackInfo(startingTrack,buf,bufSize));
}

IOReturn
IOCDAudioNub::readTrackLimits(UInt32 *first,UInt32 *last)
{
    return(_provider->readTrackLimits(first,last));
}

IOReturn
IOCDAudioNub::setAudioStopAddress(positioningType addressType,cdAddress address)
{
    return(_provider->setAudioStopAddress(addressType,address));
}

IOReturn
IOCDAudioNub::setVolume(UInt8 leftVolume,UInt8 rightVolume)
{
    return(_provider->setVolume(leftVolume,rightVolume));
}
