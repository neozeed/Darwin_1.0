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
 * IOATAPIHDDrive.h - Generic ATAPI CD-ROM driver.
 *
 * HISTORY
 * Sep 2, 1999	jliu - Ported from AppleATAPIDrive.
 */

#ifndef	_IOATAPICDDRIVE_H
#define	_IOATAPICDDRIVE_H

#include <IOKit/IOTypes.h>
#include <IOKit/storage/ata/IOATAPIHDDrive.h>

// ==========================================================================
// IOATAPICDDrive
// ==========================================================================

class IOATAPICDDrive : public IOATAPIHDDrive
{
    OSDeclareDefaultStructors(IOATAPICDDrive)

private:
    UInt32 fMSFStopAddress, fLBAStopAddress, fStatus;
 

protected:
	//-----------------------------------------------------------------------
	// Given the device type from the ATAPI Inquiry data, returns true if
	// the device type is supported by this driver.

	virtual bool           matchATAPIDeviceType(UInt8 type);

	//-----------------------------------------------------------------------
	// Overrides the method in IOATAPIHDDrive and returns an
	// IOATAPICDDriveNub instance.

    virtual IOService *	   instantiateNub();

public:
	//-----------------------------------------------------------------------
	// IOATAHDDrive override. Returns the device type.

    virtual const char *   getDeviceTypeName();

	//-----------------------------------------------------------------------
	// IOATAPIHDDrive override. Reports whether media is write protected.

	virtual IOReturn       reportWriteProtection(bool * isWriteProtected);

	//-----------------------------------------------------------------------
	// Read the Table of Contents.

	virtual IOReturn       readTOC(struct rawToc * buffer,
                                   UInt32          length,
                                   tocFormat       format);

	// Play audio
        virtual IOReturn       setAudioStopAddress (positioningType addressType,
                                     cdAddress address);

	virtual IOReturn       audioPlay(positioningType addressType,
                                   cdAddress address,
                                   audioPlayMode mode);

	virtual IOReturn       playAudioMSF(UInt32 start_msf,
                                   UInt32 end_msf);

	virtual IOReturn       playAudio(UInt32 start_lba,
                                   UInt32 len_lba);

	virtual IOReturn       audioPause(bool pause);

	virtual IOReturn       readAudioVolume(UInt8 * leftVolume,
                                   UInt8 * rightVolume);

	virtual IOReturn       setVolume(UInt8 leftVolume, UInt8 rightVolume);

        virtual IOReturn       readModeSense(UInt8 * buffer,
                                   UInt32 length, UInt32 pageCode);

        virtual IOReturn       writeModeSelect(UInt8 * buffer,
                                   UInt32 length, UInt32 pageCode);

        virtual IOReturn       getAudioStatus(struct audioStatus *status);

        virtual IOReturn       readTheQSubcode(struct qSubcode *buffer);

        virtual IOReturn       readSubChannel(UInt8 * buffer,
                                   UInt32 length,
                                   UInt8 dataFormat,
                                   UInt8 trackNumber,
                                   bool subQ);

        virtual IOReturn       readMCN(UInt8 * buffer, bool * found);

        virtual IOReturn       readISRC(UInt32 track,
                                   UInt8 * buffer, bool * found);

        virtual IOReturn       readHeader(UInt32 blockAddress,
                                   struct headerInfo *buffer);

};

#endif /* !_IOATAPICDDRIVE_H */
