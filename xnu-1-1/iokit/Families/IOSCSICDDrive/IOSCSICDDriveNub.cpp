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
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IOLib.h>
#include <IOKit/storage/scsi/IOSCSICDDriveNub.h>
#include <IOKit/storage/scsi/IOSCSICDDrive.h>

#define	super	IOCDDriveNub
OSDefineMetaClassAndStructors(IOSCSICDDriveNub,IOCDDriveNub)

bool
IOSCSICDDriveNub::attach(IOService * provider)
{
    if (!super::attach(provider)) {
        return(false);
    }

    _provider = OSDynamicCast(IOSCSICDDrive,provider);
    if (_provider == NULL) {
        return(false);
    } else {
        return(true);
    }
}

IOReturn
IOSCSICDDriveNub::audioPlay(positioningType addressType,cdAddress address,
                                         	audioPlayMode mode)
{
    return(_provider->audioPlay(addressType,address,mode));
}

IOReturn
IOSCSICDDriveNub::audioPause(bool pause)
{
    return(_provider->audioPause(pause));
}

IOReturn
IOSCSICDDriveNub::audioScan(positioningType addressType,cdAddress address,
                               		bool reverse)
{
    return(_provider->audioScan(addressType,address,reverse));
}

IOReturn
IOSCSICDDriveNub::audioTrackSearch(positioningType addressType,cdAddress address,
                                      		bool startPlay,audioPlayMode mode)
{
    return(_provider->audioTrackSearch(addressType,address,startPlay,mode));
}

IOReturn
IOSCSICDDriveNub::doAsyncReadWrite(IOMemoryDescriptor *buffer,
                                            UInt32 block,UInt32 nblks,
                                            gdCompletionFunction action,
                                            IOService *target,void *param)
{
    IOReturn result;
///    char *bp;
//    int bytes;
    
    if (buffer->getDirection() == kIODirectionOut) {
        return(kIOReturnNotWritable);
    }

//    bp = (char *)buffer->getVirtualAddress();
    
    result = _provider->doAsyncReadWrite(buffer,block,nblks,action,target,param);

//    bytes = nblks * 2048;
//    IOLog("--blk %04d, %04d blks (%04d bytes):\n",(int)block,(int)nblks,bytes);
///    dump(bp,32 /*bytes*/);
///    IOLog("\n");
    
    return(result);
}

IOReturn
IOSCSICDDriveNub::doSyncReadWrite(IOMemoryDescriptor *buffer,UInt32 block,UInt32 nblks)
{
    if (buffer->getDirection() == kIODirectionOut) {
        return(kIOReturnNotWritable);
    }

    return(_provider->doSyncReadWrite(buffer,block,nblks));
}

#define isprint(c) (c > 0x20)

void
IOSCSICDDriveNub::dump(char *buf,int count)
{
    static char string[17] = "xxxxxxxxxxxxxxxx";
    
    short i;
    char  c;
    char *cp;	

    if (!count) return;

    i = 0;
    cp = buf;
    do {
	c = *cp;
	IOLog("%02x ",(c & 0xff));
	c &= 0x7f;
	string[i % 16] = (isprint(c) || (c == ' ')) ? c : '.';
	if ((i % 16)==15)
	    IOLog("%s\n",string);
	cp++; i++;
    } while (--count);

    /* pad out for last line */
    if (i % 16) {	    /* then a shortie left */
	string[i % 16] = '\0';
	while (i % 16) {
	    IOLog("   ");
	    i++;    
	}
	IOLog("%s\n",string);	
    }
}

IOReturn
IOSCSICDDriveNub::doEjectMedia(void)
{
    return(_provider->doEjectMedia());
}

IOReturn
IOSCSICDDriveNub::doLockUnlockMedia(bool doLock)
{
    return(_provider->doLockUnlockMedia(doLock));
}

IOReturn
IOSCSICDDriveNub::executeCdb(struct cdbParams *params)
{
    return(_provider->executeCdb(params));
}

IOReturn
IOSCSICDDriveNub::getAudioStatus(struct audioStatus *status)
{
    return(_provider->getAudioStatus(status));
}

char *
IOSCSICDDriveNub::getVendorString(void)
{
    return(_provider->getVendorString());
}

char *
IOSCSICDDriveNub::getProductString(void)
{
    return(_provider->getProductString());
}

char *
IOSCSICDDriveNub::getRevisionString(void)
{
    return(_provider->getRevisionString());
}

char *
IOSCSICDDriveNub::getAdditionalDeviceInfoString(void)
{
    return(_provider->getAdditionalDeviceInfoString());
}

IOReturn
IOSCSICDDriveNub::readAudioData(positioningType addressType,cdAddress address,
                                    UInt8 blockCount,UInt8 *buffer)
{
    return(_provider->readAudioData(addressType,address,blockCount,buffer));
}

IOReturn
IOSCSICDDriveNub::readAudioSubcodes(positioningType addressType,cdAddress address,
                                    UInt8 blockCount,UInt8 *buffer)
{
    return(_provider->readAudioSubcodes(addressType,address,blockCount,buffer));
}

IOReturn
IOSCSICDDriveNub::readAudioVolume(UInt8 *leftVolume,UInt8 *rightVolume)
{
    return(_provider->readAudioVolume(leftVolume,rightVolume));
}

IOReturn
IOSCSICDDriveNub::readAudioWithQSubcode(positioningType addressType,cdAddress address,
                                    UInt8 blockCount,UInt8 *buffer)
{
    return(_provider->readAudioWithQSubcode(addressType,address,blockCount,buffer));
}

IOReturn
IOSCSICDDriveNub::readAudioWithAllSubcodes(positioningType addressType,cdAddress address,
                                    UInt8 blockCount,UInt8 *buffer)
{
    return(_provider->readAudioWithAllSubcodes(addressType,address,blockCount,buffer));
}

IOReturn
IOSCSICDDriveNub::readHeader(UInt32 blockAddress,struct headerInfo *buffer)
{
    return(_provider->readHeader(blockAddress,buffer));
}

IOReturn
IOSCSICDDriveNub::readISRC(UInt32 track,UInt8 *buffer,bool *found)
{
    return(_provider->readISRC(track,buffer,found));
}

IOReturn
IOSCSICDDriveNub::readMCN(UInt8 *buffer,bool *found)
{
    return(_provider->readMCN(buffer,found));
}

IOReturn
IOSCSICDDriveNub::readQSubcodes(struct qSubcodeTocInfo *buffer,UInt32 bufSize)
{
    return(_provider->readQSubcodes(buffer,bufSize));
}

IOReturn
IOSCSICDDriveNub::readSubcodeBuffer(UInt8 *buffer,bool purge,UInt32 entryCount)
{
    return(_provider->readSubcodeBuffer(buffer,purge,entryCount));
}

IOReturn
IOSCSICDDriveNub::readTheQSubcode(struct qSubcode *buffer)
{
    return(_provider->readTheQSubcode(buffer));
}

IOReturn
IOSCSICDDriveNub::readTOC(struct rawToc *buffer,UInt32 length,tocFormat format)
{
    return(_provider->readTOC(buffer,length,format));
}

IOReturn
IOSCSICDDriveNub::reportBlockSize(UInt64 *blockSize)
{
    return(_provider->reportBlockSize(blockSize));
}

IOReturn
IOSCSICDDriveNub::reportEjectability(bool *isEjectable)
{
    return(_provider->reportEjectability(isEjectable));
}

IOReturn
IOSCSICDDriveNub::reportLockability(bool *isLockable)
{
    return(_provider->reportLockability(isLockable));
}

IOReturn
IOSCSICDDriveNub::reportPollRequirements(bool *pollIsRequired,bool *pollIsExpensive)
{
    return(_provider-> reportPollRequirements(pollIsRequired,pollIsExpensive));
}

IOReturn
IOSCSICDDriveNub::reportMaxReadTransfer (UInt64 blockSize,UInt64 *max)
{
    return(_provider->reportMaxReadTransfer(blockSize,max));
}

IOReturn
IOSCSICDDriveNub::reportMediaState(bool *mediaPresent,bool *changed)    
{
    return(_provider-> reportMediaState(mediaPresent,changed));
}

IOReturn
IOSCSICDDriveNub::reportRemovability(bool *isRemovable)
{
    return(_provider->reportRemovability(isRemovable));
}

IOReturn
IOSCSICDDriveNub::reportMaxValidBlock(UInt64 *maxBlock)
{
    return(_provider->reportMaxValidBlock(maxBlock));
}

IOReturn
IOSCSICDDriveNub::setAudioStopAddress(positioningType addressType,cdAddress address)
{
    return(_provider->setAudioStopAddress(addressType,address));
}

IOReturn
IOSCSICDDriveNub::setVolume(UInt8 leftVolume,UInt8 rightVolume)
{
    return(_provider->setVolume(leftVolume,rightVolume));
}

