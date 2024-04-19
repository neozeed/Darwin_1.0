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
/* IOCDDriveNub.h created by rick on Wed 07-Apr-1999

 * This class is the protocol for generic CDROM functionality, independent of
 * the physical connection protocol (e.g. SCSI, ATA, USB).
 *
 * The APIs are the union of CDROM (hard disk) data APIs and all
 * necessary low-level CD APIs.
 *
 * A subclass implements relay methods that translate our requests into
 * calls to a protocol- and device-specific provider.
 */

#ifndef	_IOCDDRIVENUB_H
#define	_IOCDDRIVENUB_H

#include <IOKit/IOTypes.h>
#include <IOKit/storage/IOHDDriveNub.h>
#include "IOCDTypes.h"

/* Property used for matching, so the generic driver gets the nub it wants. */
#define	kDeviceTypeCDROM	"CDROM"

class IOMemoryDescriptor;

class IOCDDriveNub : public IOHDDriveNub {

    OSDeclareAbstractStructors(IOCDDriveNub)

public:

    /* Overrides from IORegistryEntry */
    
    virtual bool	init(OSDictionary * properties);

    /* Overrides from IOHDDriveNub: */
    
    virtual IOReturn	doFormatMedia(UInt64 byteCapacity);
    virtual UInt32	doGetFormatCapacities(UInt64 * capacities,
                                            UInt32   capacitiesMaxCount) const;
    virtual IOReturn	doSynchronizeCache(void);
    virtual IOReturn	reportMaxWriteTransfer(UInt64 blockSize,UInt64 *max);
    virtual IOReturn	reportWriteProtection(bool *isWriteProtected);
    
    /* ------------------------------------------------*/
    /* APIs that affect the entire media in the drive, */
    /* exported eventually by IOCDMediaNub:            */
    /* ------------------------------------------------*/

    virtual IOReturn	audioPause(bool pause)							= 0;
    virtual IOReturn	audioScan(positioningType addressType,cdAddress address,bool reverse)	= 0;
    virtual IOReturn	readAudioData(positioningType addressType,cdAddress address,
                                       UInt8 blockCount,UInt8 *buffer)				= 0;
    virtual IOReturn	readAudioSubcodes(positioningType addressType,cdAddress address,
                                       UInt8 blockCount,UInt8 *buffer)				= 0;
    virtual IOReturn	readAudioWithQSubcode(positioningType addressType,cdAddress address,
                                       UInt8 blockCount,UInt8 *buffer)				= 0;
    virtual IOReturn	readAudioWithAllSubcodes(positioningType addressType,cdAddress address,
                                       UInt8 blockCount,UInt8 *buffer)				= 0;
    virtual IOReturn	readHeader(UInt32 blockAddress,struct headerInfo *buffer)		= 0;
    virtual IOReturn	readISRC(UInt32 track,UInt8 *buffer,bool *found)			= 0;
    virtual IOReturn	readMCN(UInt8 *buffer,bool *found)					= 0;
    virtual IOReturn	readQSubcodes(struct qSubcodeTocInfo *buffer,UInt32 bufSize)		= 0;
    virtual IOReturn	readSubcodeBuffer(UInt8 *buffer,bool purge,UInt32 entryCount)		= 0;
    virtual IOReturn	readTheQSubcode(struct qSubcode *buffer)    				= 0;
    virtual IOReturn	readTOC(struct rawToc *buffer,UInt32 length,tocFormat format)		= 0;

    /* ----------------------------------------*/
    /*  APIs exported by IOCDAudioNub:         */
    /* ----------------------------------------*/

    virtual IOReturn	audioPlay(positioningType addressType,cdAddress address,
                               					audioPlayMode mode)		= 0;
    virtual IOReturn	audioTrackSearch(positioningType addressType,cdAddress address,
                                      		bool startPlay,audioPlayMode mode)		= 0;
    virtual IOReturn	getAudioStatus(struct audioStatus *status)				= 0;
    virtual IOReturn	readAudioVolume(UInt8 *leftVolume,UInt8 *rightVolume)			= 0;
    virtual IOReturn	setAudioStopAddress(positioningType addressType,cdAddress address)	= 0;
    virtual IOReturn	setVolume(UInt8 leftVolume,UInt8 rightVolume)				= 0;

};
#endif
