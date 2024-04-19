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
/* IOCDAudioNub.h created by rick on Mon 22-Mar-1999
 *
 * This class is the protocol for  CD Audio control functionality, independent of
 * the physical connection protocol (e.g. SCSI, ATA, USB).
 *
 * Any methods that deal with audio play mode and/or volume are here. By having
 * clients call through this nub, we can implement a local communication with
 * the audio subsystem as well as the CD device.
 *
 * A subclass implements relay methods that translate our requests into
 * calls to a protocol- and device-specific provider.
 */

#ifndef	_IOCDAUDIONUB_H
#define	_IOCDAUDIONUB_H

#include <IOKit/IOService.h>
#include <IOKit/storage/IOCDTypes.h>

class IOCDDrive;

class IOCDAudioNub : public IOService {

    OSDeclareDefaultStructors(IOCDAudioNub)

protected:
 
    virtual IOReturn newUserClient( task_t              owningTask,
                         void *              security_id,
                         UInt32              type,
                         IOUserClient **     handler ); 

public:

    /* Overrides from IOService: */

    virtual bool	attach(IOService *provider);

    /* end of IOService overrides */

    /* Position the head at the specified address and begin audio playback. */

    virtual IOReturn	audioPlay(positioningType addressType,cdAddress address,audioPlayMode mode);

    /* Pause or release the drive. */

    virtual IOReturn	audioPause(bool pause);

    /* Perform a fast-forward (reverse) operation starting at the specified address. */

    virtual IOReturn	audioScan(positioningType addressType,cdAddress address,
                               		bool reverse);

    /* Get the current audio play status and the Q-subcode for the current track. */

    virtual IOReturn	getAudioStatus(struct audioStatus *status);

    /* Position the head at the specified address and optionally begin audio play. */

    virtual IOReturn	audioTrackSearch(positioningType addressType,cdAddress address,
                                      		bool startPlay,audioPlayMode mode);
    
    /* Read audio data only.
     * Equivalent to MacOS ReadAudio type 0.
     */
    
    virtual IOReturn	readAudioData(positioningType addressType,cdAddress address,
                                       UInt8 blockCount,UInt8 *buffer);

    /* Read audio subcodes (P-W) only.
     * Equivalent to MacOS ReadAudio type 3.
     */
    
    virtual IOReturn	readAudioSubcodes(positioningType addressType,cdAddress address,
                                       UInt8 blockCount,UInt8 *buffer);
    
    /* Read audio data with Q-subcode.
     * Equivalent to MacOS ReadAudio type 1.
     */
    
    virtual IOReturn	readAudioWithQSubcode(positioningType addressType,cdAddress address,
                                       UInt8 blockCount,UInt8 *buffer);

    /* Read audio data with all subcodes (P-W).
     * Equivalent to MacOS ReadAudio type 2.
     */
    
    virtual IOReturn	readAudioWithAllSubcodes(positioningType addressType,cdAddress address,
                                       UInt8 blockCount,UInt8 *buffer);
    
    /* Read the raw Table of Contents.
     * Equivalent to MacOS ReadToc type 4.
     */
    
    virtual IOReturn	readEntireTOC(struct macEntireToc *buffer);

    /* Read the header information for the specified logical block. */
    
    virtual IOReturn	readHeader(UInt32 blockAddress,struct headerInfo *buffer);

    /* Read the International Standard Recording Code (ISRC) for a track. THe
     * "found" boolean is set true if an ISRC was found, meaning the returned
     * bytes are valid.
     */

    virtual IOReturn	readISRC(UInt32 track,UInt8 *buffer,bool *found);
    
    /* Read the mm:ss:ff address of the lead-out for the last session.
     * Equivalent to MacOS ReadToc type 2.
     */

    virtual IOReturn	readLeadOutAddress(cdAddress *buffer);

    /* Read the Media Catalog Number (MCN). The "found" boolean is set true
     * if an MCN was found, meaning the returned bytes are valid.
     */

    virtual IOReturn	readMCN(UInt8 *buffer,bool *found);

    /* Read all Q-subcode entries in the lead-in areas (including TOC info),
     * starting from the specified session. We stop at the last session or the
     * buffer limit, whichever occurs fist.
     * Equivalent to MacOS ReadToc type 6.
     */

    virtual IOReturn	readQSubcodes(struct qSubcodeTocInfo *buffer,UInt32 bufSize);
    
    /* Read session information: number of sessions & location of last session.
     * Equivalent to MacOS ReadToc type 5.
     */

    virtual IOReturn	readSessionInfo(struct sessionInfo *info);

    /* Read the subcode buffer from the drive while it's playing audio. If purge
     * is true, we instruct the drive to purge the buffer first.
     * Equivalent to MacOS ReadAllSubcodes.
     */

    virtual IOReturn	readSubcodeBuffer(UInt8 *buffer,bool purge,UInt32 entryCount);
    
    /* Read the Q subcode information for the current track being accessed or played. */
    
    virtual IOReturn	readTheQSubcode(struct qSubcode *buffer);

    /* Read the track type and address, starting at the specified track. We
     * stop at the last track or the buffer limit, whichever occurs first.
     * Equivalent to MacOS ReadToc type 3.
     */

    virtual IOReturn	readTrackInfo(UInt32 startingTrack,
                                   	struct trackTypeInfo *buf,UInt32 bufSize);
    
    /* Read the track numbers of the first and last tracks.
     * Equivalent to MacOS ReadToc type 1.
     */

    virtual IOReturn	readTrackLimits(UInt32 *first,UInt32 *last);

    /* Set the audio-play stop address. An address of blockAddress==zero will cause
     * any current audio play operation to be immediately stopped. The address can
     * also be set via an audioPlay operation that specifies a stop address.
     */
    
    virtual IOReturn	setAudioStopAddress(positioningType addressType,cdAddress address);

    /* Get the current volume settings. */

    virtual IOReturn	readAudioVolume(UInt8 *leftVolume,UInt8 *rightVolume);

    /* Set the audio play volume for the drive's audio channels. */

    virtual IOReturn	setVolume(UInt8 leftVolume,UInt8 rightVolume);

protected:

    IOCDDrive *	_provider;
};
#endif
