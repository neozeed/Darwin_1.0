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
/* =============================================================================
 * Copyright (c) 1999 Apple Computer, Inc.  All rights reserved. 
 *
 * IOSCSICDDrive.h
 *
 * This class implements SCSI CDROM functionality.
 *
 * Subclasses may modify the operations to handle device-specific variations.
 */

#ifndef _IOSCSICDDRIVE_H
#define	_IOSCSICDDRIVE_H

#include <IOKit/IOTypes.h>
#include <IOKit/scsi/IOSCSIDeviceInterface.h>
#include <IOKit/storage/IOCDTypes.h>
#include <IOKit/storage/scsi/IOSCSIHDDrive.h>

const UInt8	drive_left_signal	= 0x01;
const UInt8	drive_right_signal	= 0x02;
const UInt8	drive_both_signals	= drive_left_signal | drive_right_signal;
enum channel {
    kLeft,
    kRight
};

const UInt8	SOP_SEEK		= 0x2b;		/* seek */
const UInt8	SOP_READSUBCHANNEL	= 0x42;		/* read subchannel data */
const UInt8	SOP_READTOC		= 0x43;		/* read TOC */
const UInt8	SOP_READHEADER		= 0x44;		/* read header */
const UInt8	SOP_PLAYAUDIO		= 0x45;		/* play audio by lba */
const UInt8	SOP_PLAYAUDIOMSF	= 0x47;		/* play audio by M:S:F */
const UInt8	SOP_AUDIOSCAN		= 0xcd;		/* audio scan */

struct IOAudioScancdb {
    UInt8	opcode;
    UInt8	lunbits;
static const UInt8	kForward = 0x10;
    UInt8	start3;			/* msb */
    UInt8	start2;
    UInt8	start1;
    UInt8	start0;			/* lsb */
    UInt8	ctlbyte;
static const UInt8	kType_lba	= 0x00;
static const UInt8	kType_msf	= 0x40;
static const UInt8	kType_track	= 0x80;
};

struct IOAudioPlaycdb {
    UInt8	opcode;
    UInt8	lunbits;
    UInt8	lba_3;		/* msb */
    UInt8	lba_2;
    UInt8	lba_1;
    UInt8	lba_0;		/* lsb */
    UInt8	reserved1;
    UInt8	len_hi;
    UInt8	len_lo;
    UInt8	ctlbyte;
};

struct IOAudioPlayMSFcdb {
    UInt8	opcode;
    UInt8	lunbits;
    UInt8	reserved1;
    UInt8	start_m;
    UInt8	start_s;
    UInt8	start_f;
    UInt8	end_m;
    UInt8	end_s;
    UInt8	end_f;
    UInt8	ctlbyte;
};

struct IOReadHeadercdb {
    UInt8	opcode;
    UInt8	lunbits;
static const UInt8	kMSF = 0x02;
    UInt8	lba_3;		/* msb */
    UInt8	lba_2;
    UInt8	lba_1;
    UInt8	lba_0;		/* lsb */
    UInt8	reserved1;
    UInt8	len_hi;
    UInt8	len_lo;
    UInt8	ctlbyte;
};

struct IOReadToccdb {
    UInt8	opcode;
    UInt8	lunbits;
static const UInt8	kMSF = 0x02;		/* set to get mm:ss:ff format, else logical addr */
    UInt8	reserved1;
    UInt8	reserved2;
    UInt8	reserved3;
    UInt8	reserved4;
    UInt8	start_trk_session;	/* starting track/session number */
    UInt8	len_hi;
    UInt8	len_lo;
    UInt8	ctlbyte;		/* and format code */
static const UInt8	kSCSI2		= 0x00;		/* ANSI SCSI2 style: all track descriptors */
static const UInt8	kSessionInfo	= 0x40;		/* first & last session #s, last sess start addr. */
static const UInt8	kQLeadIn 	= 0x80;		/* all Q subcodes for lead-in of specified sess */
};

struct IORSCcdb {
    UInt8	opcode;
    UInt8	lunbits;
static const UInt8	kMSF = 0x02;		/* set to get mm:ss:ff format, else logical addr */
    UInt8	subq;
static const UInt8	kSubq = 0x40;		/* set to get subq data */
    UInt8	dataformat;
static const UInt8	kCurrentPosition	= 1;
static const UInt8	kMCN			= 2;
static const UInt8	kISRC			= 3;
    UInt8	reserved1;
    UInt8	reserved2;
    UInt8	track;
    UInt8	len_hi;
    UInt8	len_lo;
    UInt8	ctlbyte;
};

struct IOSeekcdb {
    UInt8	opcode;
    UInt8	lunbits;
    UInt8	lba_3;		/* msb */
    UInt8	lba_2;
    UInt8	lba_1;
    UInt8	lba_0;		/* lsb */
    UInt8	reserved1;
    UInt8	reserved2;
    UInt8	reserved3;
    UInt8	ctlbyte;
};

/*!
 * @class IOSCSICDDrive : public IOSCSIHDDrive
 * @abstract
 * Driver for SCSI CD-ROM drives.
 * @discussion
 * IOSCSICDDrive is a subclass of IOSCSIHDDrive. It adds appropriate CD-ROM
 * APIs (e.g. audio), and overrides some methods of IOSCSIHDDrive in order
 * to alter their behavior for CD-ROM devices.
 */
/*------------------------------------------------*/
class IOSCSICDDrive : public IOSCSIHDDrive {

    OSDeclareDefaultStructors(IOSCSICDDrive)

public:

    /* Overrides from IOService: */
    
    virtual bool	init(OSDictionary * properties);
    
    /* Overrides from IOBasicSCSI: */

    /*!
     * @function deviceTypeMatches
     * @abstract
     * Determine if the device type matches that which we expect.
     * @discussion
     * This override allows us to check for the SCSI CD-ROM
     * device type instead of hard disk.
     * See IOBasicSCSI for details.
     */
    virtual bool	deviceTypeMatches(UInt8 inqBuf[],UInt32 inqLen);

    /* End of IOBasicSCSI overrides */

    /* IOSCSIHDDrive overrides: */

    /*!
     * @function getDeviceTypeName
     * @abstract
     * Return a character string for the device type.
     * @discussion
     * This override returns kDeviceTypeCDROM.   
     */
    virtual const char * getDeviceTypeName(void);
    /*!
     * @function instantiateNub
     * @abstract
     * Create the device nub.
     * @discussion
     * This override instantiates an IOSCSICDDriveNub instead of an IOSCSIHDDriveNub.
     */
    virtual IOService *	instantiateNub(void);

    /* We want to track media changes to do cleanup.     */
    /*!
     * @function reportMediaState
     * @abstract
     * Report the device's media state.
     * @discussion
     * This override allows us to reset device settings when media changes.
     */
    virtual IOReturn	reportMediaState(bool *mediaPresent,bool *changed);

    /* end of IOSCSIHDDrive overrides */
    
    /* ------------------------------------------------*/
    /* APIs that affect the entire media in the drive, */
    /* exported eventually by IOCDMedia:               */
    /* ------------------------------------------------*/

    /*!
     * @function audioPause
     * @abstract
     * Pause or resume the audio playback.
     * @param pause
     * True to pause playback; False to resume.
     */
    virtual IOReturn	audioPause(bool pause);
    
    /*!
     * @function audioScan
     * @abstract
     * Perform a fast-forward or fast-backward operation.
     * @param addressType
     * The type of positioning address. See positioningType for details.
     * @param address
     * The position from which to begin.
     * @param reverse
     * True to go backward; False to go forward.
     */
    virtual IOReturn	audioScan(positioningType addressType,cdAddress address,bool reverse);
    
    /*!
     * @function readAudioData
     * @abstract
     * Read audio data blocks.
     * @param addressType
     * The type of positioning address. See positioningType for details.
     * @param address
     * The position from which to begin.
     * @param blockCount
     * The number of blocks to read.
     * @param buffer
     * The buffer for the data.
     */
    virtual IOReturn	readAudioData(positioningType addressType,cdAddress address,
                                       UInt8 blockCount,UInt8 *buffer);
    
    /*!
     * @function readAudioSubcodes
     * @abstract
     * Read audio subcodes only.
     * @param addressType
     * The type of positioning address. See positioningType for details.
     * @param address
     * The position from which to begin.
     * @param blockCount
     * The number of blocks to read.
     * @param buffer
     * The buffer for the data.
     */
    virtual IOReturn	readAudioSubcodes(positioningType addressType,cdAddress address,
                                       UInt8 blockCount,UInt8 *buffer);
    
    /*!
     * @function readAudioWithQSubcode
     * @abstract
     * Read audio blocks along with the Q subcode.
     * @param addressType
     * The type of positioning address. See positioningType for details.
     * @param address
     * The position from which to begin.
     * @param blockCount
     * The number of blocks to read.
     * @param buffer
     * The buffer for the data.
     */
    virtual IOReturn	readAudioWithQSubcode(positioningType addressType,cdAddress address,
                                       UInt8 blockCount,UInt8 *buffer);
    
    /*!
     * @function readAudioWithAllSubcodes
     * @abstract
     * Read audio data along with all subcodes.
     * @param addressType
     * The type of positioning address. See positioningType for details.
     * @param address
     * The position from which to begin.
     * @param blockCount
     * The number of blocks to read.
     * @param buffer
     * The buffer for the data.
     */
    virtual IOReturn	readAudioWithAllSubcodes(positioningType addressType,cdAddress address,
                                       UInt8 blockCount,UInt8 *buffer);
    
    /*!
     * @function readHeader
     * @abstract
     * Read the header for the specified logical block.
     * @param address
     * The logical block from which to read the header data.
     * @param buffer
     * The buffer for the header data.
     */
    virtual IOReturn	readHeader(UInt32 blockAddress,struct headerInfo *buffer);

    /*!
     * @function readISRC
     * @abstract
     * Read the ISRC data for the disc.
     * @param track
     * The track number from which to read the ISRC.
     * @param buffer
     * The buffer for the ISRC data.
     * @param found
     * A pointer to the result: True if the ISRC was found, else False.
     */
    virtual IOReturn	readISRC(UInt32 track,UInt8 *buffer,bool *found);
    
    /*!
     * @function readMCN
     * @abstract
     * Read the MCN (Media Catalog Number) for the disc.
     * @param buffer
     * The buffer for the MCN data.
     * @param found
     * A pointer to the result: True if the ISRC was found, else False.
     */
    virtual IOReturn	readMCN(UInt8 *buffer,bool *found);
    
    /*!
     * @function readQSubcodes
     * @abstract
     * Read the Q subcode entries in the lead-in areas of the disc.
     * @param buffer
     * The buffer for the returned data
     * @param bufSize
     * The size of the buffer in bytes.
     */
    virtual IOReturn	readQSubcodes(struct qSubcodeTocInfo *buffer,UInt32 bufSize);
    
    /*!
     * @function readSubcodeBuffer
     * @abstract
     * Read the device subcode buffer while audio is playing.
     * @param buffer
     * The buffer for the returned data
     * @param purge
     * True if the buffer should be purged before doing the read.
     * @param entryCount
     * The number of consecutive subcode blocks (96 bytes each) to return.
     */
    virtual IOReturn	readSubcodeBuffer(UInt8 *buffer,bool purge,UInt32 entryCount);
    
    /*!
     * @function readTheQSubcode
     * @abstract
     * Read the Q-subcode information for the current track.
     * @param buffer
     * The buffer for the returned data
     */
    virtual IOReturn	readTheQSubcode(struct qSubcode *buffer);
    
    /*!
     * @function readTOC
     * @abstract
     * Read the entire Table Of Contents (TOC) for the disc.
     * @param buffer
     * The buffer for the returned data
     * @param length
     * The maximum length of the buffer.
     * @param format
     * The desired TOC format desired. See tocFormat for details.
     */
    virtual IOReturn	readTOC(struct rawToc *buffer,UInt32 length,tocFormat format);
    
    /*!
     * @function setAudioStopAddress
     * @abstract
     * Set the address at which the device is to stop playing audio.
     * @param addressType
     * The type of positioning address. See positioningType for details.
     * @param address
     * The position from which to begin.
     */
    virtual IOReturn	setAudioStopAddress(positioningType addressType,cdAddress address);

    /* ---------------------------------------------------*/
    /*  APIs exported eventually by IOCDAudioNub:         */
    /* ---------------------------------------------------*/

    /*!
     * @function audioPlay
     * @abstract
     * Play audio.
     * @param addressType
     * The type of positioning address. See positioningType for details.
     * @param address
     * The position from which to begin.
     * @param mode
     * The mode for playing the audio. See audioPlayMode for details.
     */
    virtual IOReturn	audioPlay(positioningType addressType,cdAddress address,
                                         	audioPlayMode mode);
    /*!
     * @function audioTrackSearch
     * @abstract
     * Position the optical pick-up at the specified audio address.
     * @param addressType
     * The type of positioning address. See positioningType for details.
     * @param address
     * The position from which to begin.
     * @param startPlay
     * True to begin playing audio after positioning; False to enter the pause state.
     * @param mode
     * The mode for playing the audio. See audioPlayMode for details.
     */
    virtual IOReturn	audioTrackSearch(positioningType addressType,cdAddress address,
                                      		bool startPlay,audioPlayMode mode);
    /*!
     * @function getAudioStatus
     * @abstract
     * Return the current audio play status information.
     * @param status
     * The buffer for the returned information.
     */
    virtual IOReturn	getAudioStatus(struct audioStatus *status);
    /*!
     * @function readAudioVolume
     * @abstract
     * Read the current audio volume.
     * @param leftVolume
     * A pointer to the returned left-channel volume.
     * @param rightVolume
     * A pointer to the returned right-channel volume.
     */
    virtual IOReturn	readAudioVolume(UInt8 *leftVolume,UInt8 *rightVolume);
    /*!
     * @function setVolume
     * @abstract
     * Set the audio volume.
     * @param leftVolume
     * The desired left-channel volume.
     * @param rightVolume
     * The desired right-channel volume.
     */
    virtual IOReturn	setVolume(UInt8 leftVolume,UInt8 rightVolume);

protected:

    /* Internally used methods: */

    /*!
     * @function convertMSFToLba
     * @abstract
     * Convert an MM:SS:FF value to a Logical Block Address
     * @param lba
     * A pointer to the returned LBA.
     * @param address
     * The address to convert from MM:SS:FF format.
     * @return
     * This routine always returns kIOReturnSuccess
     */
    virtual IOReturn	convertMSFToLba(UInt32 *lba,cdAddress address);
    
    /*!
     * @function doAudioPlayCommand
     * @abstract
     * Issue an audio play command to the device.
     * @param startType
     * The type of positioning address. See positioningType for details.
     * @param startAddress
     * The position from which to begin.
     * @param endType
     * The type of positioning address. See positioningType for details.
     * @param endAddress
     * The position at which to end.
     */
    virtual IOReturn	doAudioPlayCommand(positioningType startType,cdAddress startAddress,
                                            positioningType endType,cdAddress endAddress);
    /*!
     * @function initAudioModes
     * @abstract
     * Initialize audio modes for the device when media is changed.
     */
    virtual void	initAudioModes(void);
    
    /*!
     * @function mediaGone
     * @abstract
     * React to media going away.
     */
    virtual void	mediaGone(void);
    
    /*!
     * @function playModeToDriveBits
     * @abstract
     * Convert an audioPlayMode value to the format used by the device.
     * @param mode
     * The audioPlayMode value to be converted.
     * @param chan
     * The desired audio channel.
     * @return
     * The converted bits in a format directly usable by the device.
     */
    virtual UInt8	playModeToDriveBits(audioPlayMode mode,channel chan);
    
    /*!
     * @function readISRCMCN
     * @abstract
     * Perform the command necessary to read ISRC or MCN data.
     * @param dataformat
     * The desired data format: ISRC or MCN.
     * @param track
     * The desired track from which to read the data
     * @param buffer
     * The buffer for the data.
     * @param found
     * A pointer to the result: True if the item was found; false if not.
     */
    virtual IOReturn	readISRCMCN(UInt8 dataformat,UInt32 track,UInt8 *buffer,bool *found);
    
    /*!
     * @function readSubchannel
     * @abstract
     * Issue the command necessary to read subchannel data.
     * @param buffer
     * The buffer for the data.
     * @param length
     * The maximum data length desired.
     * @param track
     * The desired track from which to read the data
     * @param dataFormat
     * The subchannel data desired.
     */
    virtual IOReturn	readSubchannel(UInt8 *buffer,UInt32 length,UInt8 track,UInt8 dataFormat);
    
    /*!
     * @function seek
     * @abstract
     * Issue the command necessary to position the device at the specified LBA.
     * @param lba
     * The desired Logical Block Address at which to position the device.
     */
    virtual IOReturn	seek(UInt32 lba);
    
    /*!
     * @function setDefaultAudioModes
     * @abstract
     * Set the device audio modes to the default.
     * @discussion
     * This method calls initAudioModes and then sets volume to the max.
     */
    virtual IOReturn	setDefaultAudioModes(void);

    /*!
     * @var _audioPlayMode
     * The current audio play mode for the device.
     */
    audioPlayMode      	_audioPlayMode;

    /*!
     * @var _leftVolume
     * The current volume of the left channel.
     */
    UInt8		_leftVolume;

    /*!
     * @var _rightVolume
     * The current volume of the right channel.
     */
    UInt8		_rightVolume;

    /*!
     * @var _leftPortChannel
     * The current signal-routing assignment of the left-channel audio signal.
     */
    UInt8		_leftPortChannel;	/* matches IOCDTypes consts */

    /*!
     * @var _rightPortChannel
     * The current signal-routing assignment of the right-channel audio signal.
     */
    UInt8		_rightPortChannel;	/* matches IOCDTypes consts */

    struct qualifiedCDAddress		_audioStopAddress;
};
#endif
