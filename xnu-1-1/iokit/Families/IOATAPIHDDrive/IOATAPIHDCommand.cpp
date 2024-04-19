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
 * IOATAPIHDCommand.cpp - Performs ATAPI command processing.
 *
 * HISTORY
 * Sep 2, 1999	jliu - Ported from AppleATAPIDrive.
 */

#include <IOKit/assert.h>
#include <IOKit/storage/ata/IOATAPIHDDrive.h>

#define	super IOATAHDDrive

// Enable this define to generate debugging messages.
// #define DEBUG_LOG 1

// --------------------------------------------------------------------------
// Returns the Command protocol to use (e.g. ataProtocolPIO, ataProtocolDMA).

void
IOATAPIHDDrive::selectCommandProtocol(bool useDMA)
{
	super::selectCommandProtocol(useDMA);

	if (useDMA)
		_atapiProtocol = atapiProtocolDMA;
	else
		_atapiProtocol = atapiProtocolPIO;
}

// --------------------------------------------------------------------------
// Setup a ATATaskFile for an ATAPI packet command from the parameters given.

void
IOATAPIHDDrive::setupPacketTaskFile(ATATaskfile * taskfile,
                                    ATAProtocol   protocol,
                                    UInt16        byteCount)
{
	taskfile->protocol = protocol;

	taskfile->regmask  = ATARegtoMask(atapiRegDeviceSelect) 
                       | ATARegtoMask(atapiRegCommand)
                       | ATARegtoMask(atapiRegByteCountLow)
                       | ATARegtoMask(atapiRegByteCountHigh)
                       | ATARegtoMask(atapiRegFeatures);
					
	taskfile->resultmask = ATARegtoMask(atapiRegError);

	taskfile->ataRegs[atapiRegDeviceSelect]  = ataModeLBA | (_unit << 4);
	taskfile->ataRegs[atapiRegCommand]       = atapiCommandPacket;
    taskfile->ataRegs[atapiRegByteCountLow]  = byteCount & 0xff;
    taskfile->ataRegs[atapiRegByteCountHigh] = (byteCount >> 8) & 0xff;
	taskfile->ataRegs[atapiRegFeatures]      = (protocol == atapiProtocolPIO) ?
                                               0 : kIOATAPIFeaturesDMA;
}

// --------------------------------------------------------------------------
// Create a generic ATAPI command object.

IOATACommand *
IOATAPIHDDrive::atapiCommand(ATAPICmd *           packetCommand,
                             IOMemoryDescriptor * transferBuffer = 0,
                             IOMemoryDescriptor * senseData = 0)
{
	ATATaskfile taskfile;
	bool        isWrite;
	UInt32      transferLength;

	IOATACommand * cmd = allocateCommand();
	if (!cmd) return 0;		// error, command allocation failed.
	
	// Create ATA packet command.
	//
	setupPacketTaskFile(&taskfile, _atapiProtocol, kIOATAPIMaxTransfer);

	// Get a pointer to the client data buffer, and record parameters
	// which shall be later used by the completion routine.
	//
	IOATAClientData * clientData = ATA_CLIENT_DATA(cmd);
	assert(clientData);

	clientData->buffer  = transferBuffer;

	cmd->setTaskfile(&taskfile);
	cmd->setATAPICmd(packetCommand, senseData,
                     senseData ? senseData->getLength() : 0);

	if (transferBuffer) {
		isWrite = (transferBuffer->getDirection() == kIODirectionOut);
		transferLength = transferBuffer->getLength();
	}
	else {
		isWrite = false;
		transferLength = 0;
	}
	cmd->setPointers(transferBuffer, transferLength, isWrite);

	return cmd;
}

// --------------------------------------------------------------------------
// Allocates and return an IOATACommand to perform a read/write operation.

IOATACommand *
IOATAPIHDDrive::atapiCommandReadWrite(IOMemoryDescriptor * buffer,
                                      UInt32               block,
                                      UInt32               nblks)
{
    ATAPICmd	atapiCmd;

	assert(buffer);

#ifdef DEBUG_LOG
	IOLog("%s: atapiCommandReadWrite %08x (%d) %s %d %d\n",
		getName(),
		buffer,
		buffer->getLength(),
		(buffer->getDirection() == kIODirectionOut) ? "WR" :
		"RD",
		block,
		nblks);
#endif

	// Create the ATAPI packet (bytes 1, 10, 11 are reserved).
	//
    bzero(&atapiCmd, sizeof(atapiCmd));

    atapiCmd.cdbLength = 12;
    atapiCmd.cdb[0]    = (buffer->getDirection() == kIODirectionOut) ? 
                          kIOATAPICommandWrite : kIOATAPICommandRead;
    atapiCmd.cdb[2]    = (block >> 24) & 0xff;
	atapiCmd.cdb[3]    = (block >> 16) & 0xff;
	atapiCmd.cdb[4]    = (block >>  8) & 0xff;
	atapiCmd.cdb[5]    = (block & 0xff);
    atapiCmd.cdb[6]    = (nblks >> 24) & 0xff;
	atapiCmd.cdb[7]    = (nblks >> 16) & 0xff;
	atapiCmd.cdb[8]    = (nblks >>  8) & 0xff;
	atapiCmd.cdb[9]    = (nblks & 0xff);

	return atapiCommand(&atapiCmd, buffer);
}

// --------------------------------------------------------------------------
// ATAPI Start/Stop Unit command (1B).

IOATACommand *
IOATAPIHDDrive::atapiCommandStartStopUnit(IOMemoryDescriptor * senseData,
                                          bool                 doStart,
                                          bool                 doLoadEject,
                                          bool                 immediate)
{
    ATAPICmd	atapiCmd;

#ifdef DEBUG_LOG
	IOLog("%s: atapiCommandStartStopUnit: %s\n", getName(),
		doStart ? "start" : "stop");
#endif

	// Create the ATAPI packet.
	//
    bzero(&atapiCmd, sizeof(atapiCmd));

    atapiCmd.cdbLength = 12;
    atapiCmd.cdb[0]    = kIOATAPICommandStartStopUnit;
	atapiCmd.cdb[1]    = immediate ?    0x01 : 0x00;
	atapiCmd.cdb[4]    = (doStart     ? 0x01 : 0) |
                         (doLoadEject ? 0x02 : 0);

	return atapiCommand(&atapiCmd, 0, senseData);
}

// --------------------------------------------------------------------------
// ATAPI Prevent/Allow medium removal command (1E).

IOATACommand *
IOATAPIHDDrive::atapiCommandPreventAllowRemoval(bool doLock)
{
    ATAPICmd	atapiCmd;

	// Create the ATAPI packet.
	//
    bzero(&atapiCmd, sizeof(atapiCmd));

    atapiCmd.cdbLength = 12;
    atapiCmd.cdb[0]    = kIOATAPICommandPreventAllow;
	atapiCmd.cdb[4]    = doLock ? 0x01 : 0;

	return atapiCommand(&atapiCmd);
}

// --------------------------------------------------------------------------
// ATAPI Test Unit Ready command (00).

IOATACommand *
IOATAPIHDDrive::atapiCommandTestUnitReady(IOMemoryDescriptor * senseData)
{
    ATAPICmd	atapiCmd;

#ifdef DEBUG_LOG
	IOLog("%s: atapiCommandTestUnitReady\n", getName());
#endif

	// Create the ATAPI packet.
	//
    bzero(&atapiCmd, sizeof(atapiCmd));

    atapiCmd.cdbLength = 12;
    atapiCmd.cdb[0]    = kIOATAPICommandTestUnitReady;

	return atapiCommand(&atapiCmd, 0, senseData);
}

// --------------------------------------------------------------------------
// ATAPI Read TOC command (43).

IOATACommand *
IOATAPIHDDrive::atapiCommandReadTOC(IOMemoryDescriptor * buffer,
                                    tocFormat            format,
                                    UInt8                startTrackSession)
{
    ATAPICmd	atapiCmd;

	assert(buffer);

	// Create the ATAPI packet.
	//
    bzero(&atapiCmd, sizeof(atapiCmd));

    atapiCmd.cdbLength = 12;
    atapiCmd.cdb[0]    = kIOATAPICommandReadTOC;
	atapiCmd.cdb[1]    = (format == ktocSCSI2MSF) ? 0x02 : 0x00;
	atapiCmd.cdb[6]    = startTrackSession;
	atapiCmd.cdb[7]    = (buffer->getLength() >> 8) & 0xff;
	atapiCmd.cdb[8]    =  buffer->getLength() & 0xff;
	
	switch (format) {
		case ktocSCSI2MSF:
		case ktocSCSI2LBA:
			atapiCmd.cdb[9] = 0x00;
			break;
		
		case ktocSessionInfo:
			atapiCmd.cdb[9] = 0x40;
			break;
		
		case ktocQLeadin:
			atapiCmd.cdb[9] = 0x80;
			break;
	}

	return atapiCommand(&atapiCmd, buffer);
}

IOATACommand *
IOATAPIHDDrive::atapiCommandPlayAudioMSF(UInt32 starting_msf,
                                      UInt32 ending_msf)
{
    ATAPICmd	atapiCmd;

    // IOLog("IOATAPIHDDrive::atapiCommandPlayAudioMSF %x %x\n",starting_msf,ending_msf);

    bzero(&atapiCmd, sizeof(atapiCmd));
    atapiCmd.cdbLength = 12;

    atapiCmd.cdb[0]    = kIOATAPICommandPlayAudioMSF;

    // starting MSF address
    atapiCmd.cdb[3]    = (starting_msf >> 16) & 0xff;
    atapiCmd.cdb[4]    = (starting_msf >>  8) & 0xff;
    atapiCmd.cdb[5]    = (starting_msf & 0xff);

    // ending MSF address
    atapiCmd.cdb[6]    = (ending_msf >> 16) & 0xff;
    atapiCmd.cdb[7]    = (ending_msf >>  8) & 0xff;
    atapiCmd.cdb[8]    = (ending_msf & 0xff);

    return atapiCommand(&atapiCmd);
}

IOATACommand *
IOATAPIHDDrive::atapiCommandPlayAudio(UInt32 starting_lba,
                                         UInt32 length_lba)
{
    ATAPICmd	atapiCmd;

    // IOLog("IOATAPIHDDrive::atapiCommandPlayAudio\n");

    bzero(&atapiCmd, sizeof(atapiCmd));
    atapiCmd.cdbLength = 12;

    atapiCmd.cdb[0]    = kIOATAPICommandPlayAudio;

    // starting LBA address
    atapiCmd.cdb[2]    = (starting_lba >> 24) & 0xff;
    atapiCmd.cdb[3]    = (starting_lba >> 16) & 0xff;
    atapiCmd.cdb[4]    = (starting_lba >>  8) & 0xff;
    atapiCmd.cdb[5]    = (starting_lba & 0xff);

    // length in blocks
    atapiCmd.cdb[7]    = (length_lba >>  8) & 0xff;
    atapiCmd.cdb[8]    = (length_lba & 0xff);

    return atapiCommand(&atapiCmd);
}

IOATACommand *
IOATAPIHDDrive::atapiCommandPauseResume(bool resume)
{
    ATAPICmd	atapiCmd;

    // IOLog("IOATAPIHDDrive::atapiCommandPauseResume\n");

    bzero(&atapiCmd, sizeof(atapiCmd));
    atapiCmd.cdbLength = 12;

    atapiCmd.cdb[0]    = kIOATAPICommandPauseResume;

    // set resume bit
    if (resume) atapiCmd.cdb[8]    = 0x01;

    return atapiCommand(&atapiCmd);
}

IOATACommand *
IOATAPIHDDrive::atapiCommandModeSense(IOMemoryDescriptor * buffer,
                                      UInt8 pageCode)
{
    ATAPICmd atapiCmd;

    assert(buffer);

    bzero(&atapiCmd, sizeof(atapiCmd));

    atapiCmd.cdbLength = 12;
    atapiCmd.cdb[0]    = kIOATAPICommandModeSense;
    atapiCmd.cdb[2]    = pageCode;
    atapiCmd.cdb[7]    = (buffer->getLength() >> 8) & 0xff;
    atapiCmd.cdb[8]    =  buffer->getLength() & 0xff;

    return atapiCommand(&atapiCmd, buffer);
}

IOATACommand *
IOATAPIHDDrive::atapiCommandModeSelect(IOMemoryDescriptor * buffer,
                                      UInt8 pageCode)
{
    ATAPICmd atapiCmd;

    assert(buffer);

    bzero(&atapiCmd, sizeof(atapiCmd));

    atapiCmd.cdbLength = 12;
    atapiCmd.cdb[0]    = kIOATAPICommandModeSelect;
    atapiCmd.cdb[1]    = 0x10;
    atapiCmd.cdb[2]    = pageCode;
    atapiCmd.cdb[7]    = (buffer->getLength() >> 8) & 0xff;
    atapiCmd.cdb[8]    =  buffer->getLength() & 0xff;

    return atapiCommand(&atapiCmd, buffer);
}

IOATACommand *
IOATAPIHDDrive::atapiCommandReadSubChannel(IOMemoryDescriptor * buffer,
                                      UInt8 dataFormat,
                                      UInt8 trackNumber,
                                      bool subQ)
{
    ATAPICmd atapiCmd;

    assert(buffer);

    bzero(&atapiCmd, sizeof(atapiCmd));

    atapiCmd.cdbLength = 12;
    atapiCmd.cdb[0]    = kIOATAPICommandReadSubChannel;
    atapiCmd.cdb[1]    = 0x02;
    if (subQ) atapiCmd.cdb[2]    = 0x40;
    atapiCmd.cdb[3]    = dataFormat;
    atapiCmd.cdb[6]    = trackNumber;
    atapiCmd.cdb[7]    = (buffer->getLength() >> 8) & 0xff;
    atapiCmd.cdb[8]    =  buffer->getLength() & 0xff;

    return atapiCommand(&atapiCmd, buffer);
}

IOATACommand *
IOATAPIHDDrive::atapiCommandReadHeader(IOMemoryDescriptor * buffer,
                                      UInt32 address)
{
    ATAPICmd atapiCmd;

    assert(buffer);

    bzero(&atapiCmd, sizeof(atapiCmd));

    atapiCmd.cdbLength = 12;
    atapiCmd.cdb[0]    = kIOATAPICommandReadHeader;
    atapiCmd.cdb[1]    = 0x02;  // MSF

    // starting LBA address
    atapiCmd.cdb[2]    = (address >> 24) & 0xff;
    atapiCmd.cdb[3]    = (address >> 16) & 0xff;
    atapiCmd.cdb[4]    = (address >>  8) & 0xff;
    atapiCmd.cdb[5]    = (address & 0xff);

    atapiCmd.cdb[7]    = (buffer->getLength() >> 8) & 0xff;
    atapiCmd.cdb[8]    =  buffer->getLength() & 0xff;

    return atapiCommand(&atapiCmd, buffer);
}

IOATACommand *
IOATAPIHDDrive::atapiCommandReadCD(IOMemoryDescriptor * buffer,
                  UInt32 address,
                  UInt32 length,
                  UInt8 sector,
                  UInt8 header,
                  UInt8 error,
                  UInt8  data)
{

    ATAPICmd atapiCmd;

    assert(buffer);

    bzero(&atapiCmd, sizeof(atapiCmd));

    atapiCmd.cdbLength = 12;
    atapiCmd.cdb[0]    = kIOATAPICommandReadCD;

    // expected sector type
    atapiCmd.cdb[1]    = (sector << 2) & 0xff;

    // starting LBA address
    atapiCmd.cdb[2]    = (address >> 24) & 0xff;
    atapiCmd.cdb[3]    = (address >> 16) & 0xff;
    atapiCmd.cdb[4]    = (address >>  8) & 0xff;
    atapiCmd.cdb[5]    = (address & 0xff);

    // transfer length
    atapiCmd.cdb[6]    = (buffer->getLength() >> 16) & 0xff;
    atapiCmd.cdb[7]    = (buffer->getLength() >>  8) & 0xff;
    atapiCmd.cdb[8]    =  buffer->getLength() & 0xff;

    // flag bits
    atapiCmd.cdb[9]    = ((header << 5) | (error << 1)) & 0xff;

    // sub-channel data selection
    atapiCmd.cdb[10]    = (data) & 0xff;

    return atapiCommand(&atapiCmd, buffer);
}
