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
/* IOSCSICDDriveNub.h created by rick on Wed 07-Apr-1999 */

/* This subclass implements a relay to a protocol- and device-specific provider. */

#ifndef	_IOSCSICDDRIVENUB_H
#define	_IOSCSICDDRIVENUB_H

#include <IOKit/IOTypes.h>
#include <IOKit/storage/IOCDDriveNub.h>

class IOSCSICDDrive;

class IOSCSICDDriveNub : public IOCDDriveNub {

    OSDeclareDefaultStructors(IOSCSICDDriveNub)

public:

    /* Overrides from IOService */

    virtual bool	attach(IOService * provider);

    /* Overrides from IOCDDriveNub: */
    
    virtual IOReturn	doAsyncReadWrite(IOMemoryDescriptor *buffer,
                                            UInt32 block,UInt32 nblks,
                                            gdCompletionFunction action,
                                            IOService *target,void *param);
    virtual IOReturn	doSyncReadWrite(IOMemoryDescriptor *buffer,UInt32 block,UInt32 nblks);
    
    /* --------------------------------------------------------------------------*/
    /* Serrvice APIs used by the IOHDDrive portion of IOCDDriveNub: */
    /* --------------------------------------------------------------------------*/

    virtual IOReturn	doEjectMedia(void);
    virtual IOReturn	doLockUnlockMedia(bool doLock);
    virtual IOReturn	executeCdb(struct cdbParams *params);
    virtual char *	getVendorString(void);
    virtual char *	getProductString(void);
    virtual char *	getRevisionString(void);
    virtual char *	getAdditionalDeviceInfoString(void);
    virtual IOReturn	reportBlockSize(UInt64 *blockSize);
    virtual IOReturn	reportEjectability(bool *isEjectable);
    virtual IOReturn	reportLockability(bool *isLockable);
    virtual IOReturn	reportPollRequirements(bool *pollIsRequired,bool *pollIsExpensive);
    virtual IOReturn	reportMaxReadTransfer(UInt64 blockSize,UInt64 *max);
    virtual IOReturn	reportMaxValidBlock(UInt64 *maxBlock);
    virtual IOReturn	reportMediaState(bool *mediaPresent,bool *changed);    
    virtual IOReturn	reportRemovability(bool *isRemovable);

    /* ------------------------------------------------*/
    /* APIs that affect the entire media in the drive, */
    /* exported eventually by IOCDMedia:               */
    /* ------------------------------------------------*/

    virtual IOReturn	audioPause(bool pause);
    virtual IOReturn	audioScan(positioningType addressType,cdAddress address,
                               		bool reverse);
    virtual IOReturn	readAudioData(positioningType addressType,cdAddress address,
                                       UInt8 blockCount,UInt8 *buffer);
    virtual IOReturn	readAudioSubcodes(positioningType addressType,cdAddress address,
                                       UInt8 blockCount,UInt8 *buffer);
    virtual IOReturn	readAudioWithQSubcode(positioningType addressType,cdAddress address,
                                       UInt8 blockCount,UInt8 *buffer);
    virtual IOReturn	readAudioWithAllSubcodes(positioningType addressType,cdAddress address,
                                       UInt8 blockCount,UInt8 *buffer);
    virtual IOReturn	readHeader(UInt32 blockAddress,struct headerInfo *buffer);
    virtual IOReturn	readISRC(UInt32 track,UInt8 *buffer,bool *found);
    virtual IOReturn	readMCN(UInt8 *buffer,bool *found);
    virtual IOReturn	readQSubcodes(struct qSubcodeTocInfo *buffer,UInt32 bufSize);
    virtual IOReturn	readSubcodeBuffer(UInt8 *buffer,bool purge,UInt32 entryCount);
    virtual IOReturn	readTheQSubcode(struct qSubcode *buffer);
    virtual IOReturn	readTOC(struct rawToc *buffer,UInt32 length,tocFormat format);
    virtual IOReturn	setAudioStopAddress(positioningType addressType,cdAddress address);

    /* ---------------------------------------------------*/
    /*  APIs exported eventually by IOCDAudioNub:         */
    /* ---------------------------------------------------*/

    virtual IOReturn	audioPlay(positioningType addressType,cdAddress address,
                                         	audioPlayMode mode);
    virtual IOReturn	audioTrackSearch(positioningType addressType,cdAddress address,
                                      		bool startPlay,audioPlayMode mode);
    virtual IOReturn	getAudioStatus(struct audioStatus *status);
    virtual IOReturn	readAudioVolume(UInt8 *leftVolume,UInt8 *rightVolume);
    virtual IOReturn	setVolume(UInt8 leftVolume,UInt8 rightVolume);

protected:

    virtual void	dump(char *buf,int count);
    
    IOSCSICDDrive *	_provider;
};
#endif
