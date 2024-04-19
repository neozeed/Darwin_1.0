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
/* IOCD.h created by rick on Thu 08-Apr-1999 */

/* =============================================================================
 * Copyright (c) 1999 Apple Computer, Inc.  All rights reserved. 
 *
 * IOCDDrive.h
 *
 * This class implements  CD functionality, independent of
 * the physical connection protocol (e.g. SCSI, ATA, USB).
 *
 * A protocol-specific provider implements the functionality using an appropriate
 * protocol and commands.
 */

#ifndef	_IOCDDRIVE_H
#define	_IOCDDRIVE_H

#include <IOKit/IOTypes.h>
#include <IOKit/storage/IOHDDrive.h>
#include "IOCDTypes.h"

class IOCDAudioNub;
class IOCDMedia;
class IOCDDriveNub;

class IOCDDrive : public IOHDDrive {

    OSDeclareDefaultStructors(IOCDDrive)

public:

    /* Overrides of IORegistryEntry */
    
    virtual bool	init(OSDictionary * properties);

    /* Overrides of IOHDDrive: */

    virtual IOReturn	ejectMedia(void);
    virtual const char * getDeviceTypeName(void);
    virtual IOMedia *	instantiateDesiredMediaObject(void);
    virtual bool	setProvider(IOService * provider);
    virtual bool	showStats(void);
    
    /* End of IOHDDrive overrides. */

    /* -------------------------------------------------*/
    /* APIs implemented here, exported by IOCDMediaNub: */
    /* -------------------------------------------------*/

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
    virtual IOReturn	readEntireTOC(struct macEntireToc *buffer);
    virtual IOReturn	readHeader(UInt32 blockAddress,struct headerInfo *buffer);
    virtual IOReturn	readISRC(UInt32 track,UInt8 *buffer,bool *found);
    virtual IOReturn	readLeadOutAddress(cdAddress *buffer);
    virtual IOReturn	readMCN(UInt8 *buffer,bool *found);
    virtual IOReturn	readQSubcodes(struct qSubcodeTocInfo *buffer,UInt32 bufSize);
    virtual IOReturn	readSessionInfo(struct sessionInfo *info);
    virtual IOReturn	readSubcodeBuffer(UInt8 *buffer,bool purge,UInt32 entryCount);
    virtual IOReturn	readTheQSubcode(struct qSubcode *buffer);
    virtual IOReturn	readTrackInfo(UInt32 startingTrack,
                                   	struct trackTypeInfo *buf,UInt32 bufSize);
    virtual IOReturn	readTrackLimits(UInt32 *first,UInt32 *last);

    /* end of IOCDMediaNub APIs */
    
    /* --------------------------------------------------------*/
    /* APIs implemented here, exported by IOCDAudioNub: */
    /* --------------------------------------------------------*/

    virtual IOReturn	audioPlay(positioningType addressType,cdAddress address,audioPlayMode mode);
    virtual IOReturn	audioTrackSearch(positioningType addressType,cdAddress address,
                                      		bool startPlay,audioPlayMode mode);
    virtual IOReturn	getAudioStatus(struct audioStatus *status);
    virtual IOReturn	readAudioVolume(UInt8 *leftVolume,UInt8 *rightVolume);
    virtual IOReturn	setAudioStopAddress(positioningType addressType,cdAddress address);
    virtual IOReturn	setVolume(UInt8 leftVolume,UInt8 rightVolume);

    /* end of IOCDAudioNub APIs */

    /* API used by a CD partitioner: */
    
    virtual UInt32	getTrackBlocks(UInt32 track);
    virtual UInt32	getTrackStartBlock(UInt32 track);
    virtual bool	trackExists(UInt32 track);
    virtual bool	trackIsAudio(UInt32 track);
    virtual bool	trackIsData(UInt32 track);
    
protected:
        
    /* Overrides of IOHDDrive behavior. */

    /* When CD media is inserted, we want to create multiple nubs for the data and
     * audio tracks, for sessions, and the entire media. We override the methods
     * that manage nubs.
     */
    virtual IOReturn	acceptNewMedia(void);
    virtual IOReturn	decommissionMedia(bool forcible);

    /* End of IOHDDrive overrides. */

    /* Internally used methods: */

    struct trackInfo {
        UInt8		track;			/* zero is invalid, thus this entry not in use */
        UInt8		control;
        bool		isData;
        UInt32		lba;
        UInt32		msf;
        UInt32		nblocks;
    };

    virtual IOReturn	cacheTocInfo(void);
    virtual void	dump(char *buf,int count);
    virtual void	setTrackInfoEntry(struct tocTrackDescriptor *d,struct trackInfo *t);

    /* ------- */

    /* We have to keep a pointer to a CD nub, because our base class's
     * provider pointer only expects to point to a Hard Disk nub. Both
     * pointers actually point at the same IOCDDriveNub object.
     */
    IOCDDriveNub *		_CDprovider;
    
    IOCDAudioNub *	_acNub;
    IOCDMedia *			_mediaNub;


    /* We keep the TOC here because we'll always need it, so what the heck. */

    bool		_tocInfoCached;		/* true if toc info is valid */

    struct discInfo {
        struct limits {
            UInt32	first;		/* number of first */
            UInt32	last;		/*  and last */
        } sessions;
        struct limits	tracks;
    } _discInfo;

    /* There are possible "point" track entries for 0xa0..a2, 0xaa, 0xb0..b4, and 0xc0.
     * To keep things simple, we just allow space for all of these possible track entries.
     * Tracks need not start at 1, as long as they're between 1 and 99, and have contiguous
     * numbers. The leadout track is always numbered 0xaa.
     */
static const int kTrackEntries = 0xc0 + 1;	/* 99 data/audio tracks + points, starting at 1 */

    struct trackInfo	_trackInfo[kTrackEntries];
};
#endif
