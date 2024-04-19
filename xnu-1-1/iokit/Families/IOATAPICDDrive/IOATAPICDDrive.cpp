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
 * IOATAPICDDrive.h - Generic ATAPI CD-ROM driver.
 *
 * HISTORY
 * Sep 2, 1999	jliu - Ported from AppleATAPIDrive.
 */

#include <IOKit/assert.h>
#include <IOKit/storage/ata/IOATAPICDDrive.h>
#include <IOKit/storage/ata/IOATAPICDDriveNub.h>

#define	super IOATAPIHDDrive
OSDefineMetaClassAndStructors( IOATAPICDDrive, IOATAPIHDDrive )

// --------------------------------------------------------------------------
// Looks for an ATAPI device which is a CD-ROM device.

bool
IOATAPICDDrive::matchATAPIDeviceType(UInt8 type)
{
	if (type == kIOATAPIDeviceTypeCDROM)
		return true;
	return false;
}

// --------------------------------------------------------------------------
// Instantiate an ATAPI specific subclass of IOCDDriveNub.

IOService *
IOATAPICDDrive::instantiateNub()
{
    IOService * nub = new IOATAPICDDriveNub;
    return nub;
}

// --------------------------------------------------------------------------
// Report whether media is write-protected.

IOReturn
IOATAPICDDrive::reportWriteProtection(bool * isWriteProtected)
{
	*isWriteProtected = true;
	return kIOReturnSuccess;
}

// --------------------------------------------------------------------------
// Returns the device type.

const char *
IOATAPICDDrive::getDeviceTypeName()
{
	return kDeviceTypeCDROM;
}

// --------------------------------------------------------------------------
// Read the Table of Contents.

IOReturn
IOATAPICDDrive::readTOC(struct rawToc * buffer,
                        UInt32          length,
                        tocFormat       format)
{
	IOReturn             ret;
	IOATACommand *       cmd;
	IOMemoryDescriptor * tocDesc;
	
	assert(buffer);
	
    tocDesc = IOMemoryDescriptor::withAddress(buffer,
	                                    length,
	                                    kIODirectionIn);
	if (!tocDesc)
		return kIOReturnNoMemory;
	
	cmd = atapiCommandReadTOC(tocDesc, format, 0);
	if (!cmd)
		return kIOReturnNoMemory;

	// Execute the Read TOC command.
	//
	ret = getIOReturn(syncExecute(cmd));

	// Release the memory descriptor.
	//
	tocDesc->release();
	
	cmd->release();

	return ret;
}

// --------------------------------------------------------------------------
// Start analog audio play

IOReturn
IOATAPICDDrive::setAudioStopAddress (positioningType addressType,
                                     cdAddress address)
{
    if (addressType == kAbsoluteTime)
        fMSFStopAddress = (UInt32)address;
    if (addressType == kBlockAddress)
        fLBAStopAddress = (UInt32)address;
    return 0;
}

IOReturn
IOATAPICDDrive::audioPlay(positioningType addressType,
                          cdAddress address,
                          audioPlayMode mode)
{
    if (addressType == kBlockAddress) {
        return playAudio((UInt32)address,0xfff);
    }
    if (addressType == kAbsoluteTime) {
        return playAudioMSF((UInt32)address,(UInt32)fMSFStopAddress);
    }
    return 0;
}

IOReturn
IOATAPICDDrive::playAudioMSF (UInt32 start_msf,
                              UInt32 end_msf)
{
	IOATACommand *       cmd;
	IOReturn             ret;

        // IOLog("IOATAPICDDrive::playAudioMSF %x %x\n",start_msf,end_msf);
	cmd = atapiCommandPlayAudioMSF(start_msf, end_msf);
	if (!cmd)
		return kIOReturnNoMemory;

	// Execute the audio play command.
	//
	ret = getIOReturn(syncExecute(cmd));

	cmd->release();

	return ret;
}

IOReturn
IOATAPICDDrive::playAudio (UInt32 start_lba,
                           UInt32 len_lba)
{
	IOATACommand *       cmd;
	IOReturn             ret;

        // IOLog("IOATAPICDDrive::playAudio\n");
	cmd = atapiCommandPlayAudio(start_lba, len_lba);
	if (!cmd)
		return kIOReturnNoMemory;

	// Execute the audio play command.
	//
	ret = getIOReturn(syncExecute(cmd));

	cmd->release();

	return ret;
}

IOReturn
IOATAPICDDrive::audioPause(bool pause)
{
	IOATACommand *       cmd;
	IOReturn             ret;

        // IOLog("IOATAPICDDrive::audioPause\n");
	cmd = atapiCommandPauseResume(!pause);
	if (!cmd)
		return kIOReturnNoMemory;

	// Execute the audio pause/resume command.
	//
	ret = getIOReturn(syncExecute(cmd));

	cmd->release();

	return ret;
}

IOReturn
IOATAPICDDrive::readAudioVolume(UInt8 * leftVolume, UInt8 * rightVolume)
{
    UInt8 *audio_control;
    IOReturn status = -1;

    audio_control = (UInt8 *)IOMalloc(144);
    if (!audio_control) return kIOReturnNoMemory;

    status = readModeSense(audio_control,(UInt32)144,(UInt32)0xe);
    *leftVolume = audio_control[21] & 0xff;
    *rightVolume = audio_control[17] & 0xff;

    // IOLog("IOATAPICDDrive::readAudioVolume left0=%d right0=%d left1=%d right1=%d\n", *leftVolume,*rightVolume, audio_control[23],audio_control[19]);

    // IOLog("IOATAPICDDrive::readAudioVolume p0=%d p1=%d p2=%d p3=%d\n", audio_control[8],audio_control[10], audio_control[12],audio_control[14]);

    IOFree(audio_control,144);
    return status;
}

IOReturn
IOATAPICDDrive::setVolume(UInt8 leftVolume, UInt8 rightVolume)
{
    UInt8 *audio_control;
    IOReturn status = -1;
    UInt32 len;

    // IOLog("IOATAPICDDrive::setVolume %d %d\n",leftVolume,rightVolume);

    // init
    audio_control = (UInt8 *)IOMalloc(144);
    if (!audio_control) return kIOReturnNoMemory;

    // get current values
    status = readModeSense(audio_control,(UInt32)144,(UInt32)0xe);
    len = (UInt32)audio_control[1] + 2;

    // set new values
    // audio_control[ 9] = audio_control[11] = 0xff;
    // audio_control[23] = audio_control[21] = leftVolume & 0xff;
    audio_control[19] = audio_control[17] = rightVolume & 0xff;

    // get current values
    status = writeModeSelect(audio_control,(UInt32)len,(UInt32)0xe);

    // cleanup and exit
    IOFree(audio_control,144);
    return status;
}

IOReturn
IOATAPICDDrive::readModeSense(UInt8 * buffer, UInt32 length, UInt32 pageCode)
{
    IOReturn             ret;
    IOATACommand *       cmd;
    IOMemoryDescriptor * senseDesc;
	
    assert(buffer);

    // IOLog("IOATAPICDDrive::readModeSense len=%d page=%d\n",length,pageCode);

    senseDesc = IOMemoryDescriptor::withAddress(buffer,
                                     length,
                                     kIODirectionIn);
    if (!senseDesc)
        return kIOReturnNoMemory;

    cmd = atapiCommandModeSense(senseDesc, pageCode);
    if (!cmd)
        return kIOReturnNoMemory;

    // Execute the Mode Sense command.
    //
    ret = getIOReturn(syncExecute(cmd));

    // Release the memory descriptor.
    //
    senseDesc->release();

    cmd->release();

    return ret;
}

IOReturn
IOATAPICDDrive::writeModeSelect(UInt8 * buffer, UInt32 length, UInt32 pageCode)
{
    IOReturn             ret;
    IOATACommand *       cmd;
    IOMemoryDescriptor * selectDesc;
	
    // IOLog("IOATAPICDDrive::writeModeSelect %d %d\n",length,pageCode);
    assert(buffer);

    selectDesc = IOMemoryDescriptor::withAddress(buffer,
                                     length,
                                     kIODirectionOut);
    if (!selectDesc)
        return kIOReturnNoMemory;

    cmd = atapiCommandModeSelect(selectDesc, pageCode);
    if (!cmd)
        return kIOReturnNoMemory;

    // Execute the Mode Select command.
    //
    ret = getIOReturn(syncExecute(cmd));

    // Release the memory descriptor.
    //
    selectDesc->release();

    cmd->release();

    return ret;
}

IOReturn
IOATAPICDDrive::readTheQSubcode(struct qSubcode *buffer)
{
    UInt8 * channel_data;
    UInt32 address;
    IOReturn ret;

    // init
    channel_data = (UInt8 *)IOMalloc(16);
    if (!channel_data) return kIOReturnNoMemory;

    // get audio status
    ret = readSubChannel(channel_data,16,0x01,0x00,true);

    // get current absolute address
    address = ((channel_data[ 8] & 0xff) << 24) |
              ((channel_data[ 9] & 0xff) << 16) |
              ((channel_data[10] & 0xff) <<  8) |
              ((channel_data[11] & 0xff));
    buffer->absAddress = (cdAddress)address;

    // get current track relative address
    address = ((channel_data[12] & 0xff) << 24) |
              ((channel_data[13] & 0xff) << 16) |
              ((channel_data[14] & 0xff) <<  8) |
              ((channel_data[15] & 0xff));
    buffer->relAddress = (cdAddress)address;

    // get type, track, index
    buffer->type  = channel_data[5] & 0x0f;
    buffer->track = channel_data[6] & 0x0f;
    buffer->index = channel_data[7] & 0x0f;

    // IOLog("IOATAPICDDrive::readTheQSubcode absAddr=0x%x relAddr=0x%xx\n", buffer->absAddress, buffer->relAddress);

    // cleanup
    IOFree(channel_data,16);
    return ret;
}

IOReturn
IOATAPICDDrive::getAudioStatus(struct audioStatus *status)
{
    UInt8 * channel_data;
    UInt32 address;
    IOReturn ret;

    // init
    channel_data = (UInt8 *)IOMalloc(16);
    if (!channel_data) return kIOReturnNoMemory;

    // get audio status
    ret = readSubChannel(channel_data,16,0x01,0x00,true);

    // get current absolute address
    address = ((channel_data[ 8] & 0xff) << 24) |
              ((channel_data[ 9] & 0xff) << 16) |
              ((channel_data[10] & 0xff) <<  8) |
              ((channel_data[11] & 0xff));
    status->address = (cdAddress)address;

    // get current status
    if ((channel_data[1] & 0xff) == 0x00) status->status = kUnknown;
    if ((channel_data[1] & 0xff) == 0x11) status->status = kAudioPlayInProgress;
    if ((channel_data[1] & 0xff) == 0x12) status->status = kHoldTrackMode;
    if ((channel_data[1] & 0xff) == 0x13) status->status = kAudioPlayCompleted;
    if ((channel_data[1] & 0xff) == 0x14) status->status = kError;
    if ((channel_data[1] & 0xff) == 0x15) status->status = fStatus;
    fStatus = status->status;

    // get current track type
    status->type = channel_data[5] & 0x0f;

    // IOLog("IOATAPICDDrive::getAudioStatus addr=0x%x status=0x%x type=0x%x\n", status->address, status->status, status->type);

    // cleanup
    IOFree(channel_data,16);
    return ret;
}

IOReturn
IOATAPICDDrive::readMCN(UInt8 * buffer, bool * found)
{
    UInt8 * channel_data;
    UInt32 address;
    IOReturn ret;

    // init
    channel_data = (UInt8 *)IOMalloc(24);
    if (!channel_data) return kIOReturnNoMemory;

    // get audio status
    ret = readSubChannel(channel_data,24,0x02,0x00,true);

    // check if found
    *found = (channel_data[8] & 0x80 == 0x80);

    // copy the data
    if (*found) {
        bcopy(&channel_data[9],buffer,15);
    }

    // IOLog("IOATAPICDDrive::readMCN found=%d\n",*found);

    // cleanup
    IOFree(channel_data,24);
    return ret;
}

IOReturn
IOATAPICDDrive::readISRC(UInt32 track, UInt8 * buffer, bool * found)
{
    UInt8 * channel_data;
    UInt32 address;
    IOReturn ret;

    // init
    channel_data = (UInt8 *)IOMalloc(24);
    if (!channel_data) return kIOReturnNoMemory;

    // get audio status
    ret = readSubChannel(channel_data,24,0x03,track,true);

    // check if found
    *found = (channel_data[8] & 0x80 == 0x80);

    // copy the data
    if (*found) {
        bcopy(&channel_data[9],buffer,15);
    }

    // IOLog("IOATAPICDDrive::readISRC found=%d\n",*found);

    // cleanup
    IOFree(channel_data,24);
    return ret;
}

IOReturn
IOATAPICDDrive::readHeader(UInt32 blockAddress,struct headerInfo *buffer)
{
    UInt8 * header_data;
    IOReturn             ret;
    IOATACommand *       cmd;
    IOMemoryDescriptor * readDesc;
	
    // init
    header_data = (UInt8 *)IOMalloc(8);
    if (!header_data) return kIOReturnNoMemory;

    // IOLog("IOATAPICDDrive::readHeader addr=0x%x\n",blockAddress);

    readDesc = IOMemoryDescriptor::withAddress(header_data,
                                     8,
                                     kIODirectionIn);
    if (!readDesc)
        return kIOReturnNoMemory;

    cmd = atapiCommandReadHeader(readDesc, blockAddress);
    if (!cmd)
        return kIOReturnNoMemory;

    // Execute the Read Header command.
    //
    ret = getIOReturn(syncExecute(cmd));

    // Release the memory descriptor.
    //
    readDesc->release();
    cmd->release();

    // save the data
    buffer->address = ((header_data[4] & 0xff) << 24) |
                      ((header_data[5] & 0xff) << 16) |
                      ((header_data[6] & 0xff) <<  8) |
                      ((header_data[7] & 0xff));
    buffer->mode = header_data[0] & 0xff;

    IOFree(header_data,8);

    return ret;
}

IOReturn
IOATAPICDDrive::readSubChannel(UInt8 * buffer,
                          UInt32 length,
                          UInt8 dataFormat,
                          UInt8 trackNumber,
                          bool subQ)
{
    IOReturn             ret;
    IOATACommand *       cmd;
    IOMemoryDescriptor * readDesc;
	
    assert(buffer);

    // IOLog("IOATAPICDDrive::readSubChannel len=%d\n",length);

    readDesc = IOMemoryDescriptor::withAddress(buffer,
                                     length,
                                     kIODirectionIn);
    if (!readDesc)
        return kIOReturnNoMemory;

    cmd = atapiCommandReadSubChannel(readDesc, dataFormat, trackNumber, subQ);
    if (!cmd)
        return kIOReturnNoMemory;

    // Execute the Mode Sense command.
    //
    ret = getIOReturn(syncExecute(cmd));

    // Release the memory descriptor.
    //
    readDesc->release();

    cmd->release();

    return ret;
}
