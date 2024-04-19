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
 * IOATAPIHDDrive.h - Generic ATAPI Direct-Access driver.
 *
 * HISTORY
 * Sep 2, 1999	jliu - Ported from AppleATAPIDrive.
 */

#include <IOKit/assert.h>
#include <IOKit/storage/ata/IOATAPIHDDrive.h>
#include <IOKit/storage/ata/IOATAPIHDDriveNub.h>

#define	super IOATAHDDrive
OSDefineMetaClassAndStructors( IOATAPIHDDrive, IOATAHDDrive )

// --------------------------------------------------------------------------
// Override the init() method in IOATAHDDrive.

bool
IOATAPIHDDrive::init(OSDictionary * properties)
{
	_mediaPresent = false;
	_isRemovable  = false;

    return (super::init(properties));
}

// --------------------------------------------------------------------------
// Override probe() method inherited from IOATAHDDrive. We need to
// perform additional matching on ATAPI device type from the Inquiry data.

IOService * 
IOATAPIHDDrive::probe(IOService * provider, SInt32 * score)
{
	UInt8	deviceType;

	// Our provider must be a IOATADevice nub, most likely created
	// by an IOATAController instance.
	//
	IOATADevice * device = OSDynamicCast(IOATADevice, provider);
	if (!device)
		return 0;	// IOATADevice nub not found.

	// Let superclass have a go at probe first.
	//
    if (!super::probe(provider, score))
        return 0;

	// Perform ATAPI type matching, CD-ROM, Direct-Access, Tape, etc.
	//
	if (!device->getInquiryData(1, (ATAPIInquiry *) &deviceType) ||
		!matchATAPIDeviceType(deviceType & 0x1f))
		return 0;

	return this;	// probe successful.
}

// --------------------------------------------------------------------------
// Report as an ATAPI device.

ATADeviceType
IOATAPIHDDrive::reportATADeviceType() const
{
	return ataDeviceATAPI;
}

// --------------------------------------------------------------------------
// Looks for an ATAPI device which is a direct-access device.

bool
IOATAPIHDDrive::matchATAPIDeviceType(UInt8 type)
{
	if (type == kIOATAPIDeviceTypeDirectAccess)
		return true;
	return false;
}

// --------------------------------------------------------------------------
// Gather information about the ATAPI device nub.

void
IOATAPIHDDrive::inspectDevice(IOService * provider)
{
	OSString *    string;
	IOATADevice * ataDevice = OSDynamicCast(IOATADevice, provider);
	if (!ataDevice)
		return;

	// Fetch ATAPI device information from the nub.
	//
	string = OSDynamicCast(OSString,
                           ataDevice->getProperty(ATAPropertyVendorName));
	if (string) {
		strncpy(_vendor, string->getCStringNoCopy(), 8);
		_vendor[8] = '\0';
	}

	string = OSDynamicCast(OSString,
                           ataDevice->getProperty(ATAPropertyProductName));
	if (string) {
		strncpy(_product, string->getCStringNoCopy(), 16);
		_product[16] = '\0';
	}

	string = OSDynamicCast(OSString,
                           ataDevice->getProperty(ATAPropertyProductRevision));
	if (string) {
		strncpy(_revision, string->getCStringNoCopy(), 4);
		_revision[4] = '\0';
	}
}

// --------------------------------------------------------------------------
// Handles read/write requests.

IOReturn IOATAPIHDDrive::doAsyncReadWrite(IOMemoryDescriptor * buffer,
                                          UInt32               block,
                                          UInt32               nblks,
                                          gdCompletionFunction action,
                                          IOService *          target,
                                          void *               param)
{
	IOReturn       ret;
	IOATACommand * cmd = atapiCommandReadWrite(buffer, block, nblks);
	if (!cmd)
		return kIOReturnNoMemory;

	ret = getIOReturn(asyncExecute(cmd, target, action, param));

	cmd->release();

	return ret;
}

// --------------------------------------------------------------------------
// Eject the media in the removable drive.

IOReturn
IOATAPIHDDrive::doEjectMedia()
{
	IOReturn       ret;
	IOATACommand * cmd = atapiCommandStartStopUnit(0,        /* senseData */
                                                   false,    /* start unit */
                                                   true,     /* Load/Eject */
                                                   false);   /* Immediate */	

	if (!cmd)
		return kIOReturnNoMemory;

	ret = getIOReturn(syncExecute(cmd));

	cmd->release();

	return ret;
}

// --------------------------------------------------------------------------
// Format the media in the drive.

IOReturn
IOATAPIHDDrive::doFormatMedia(UInt64 /* byteCapacity */)
{
	return kIOReturnUnsupported;
}

// --------------------------------------------------------------------------
// Lock/unlock the media in the removable drive.

IOReturn
IOATAPIHDDrive::doLockUnlockMedia(bool doLock)
{
	IOReturn       ret;
	IOATACommand * cmd = atapiCommandPreventAllowRemoval(doLock);
	if (!cmd)
		return kIOReturnNoMemory;
	
	ret = getIOReturn(syncExecute(cmd));

	cmd->release();
	
	return ret;
}

// --------------------------------------------------------------------------
// Sync the write cache.

IOReturn
IOATAPIHDDrive::doSynchronizeCache()
{
	return kIOReturnSuccess;
}

// --------------------------------------------------------------------------
// Start up the drive.

IOReturn
IOATAPIHDDrive::doStart()
{
	return doStartStop(true);
}

// --------------------------------------------------------------------------
// Stop the drive.

IOReturn
IOATAPIHDDrive::doStop()
{
	return doStartStop(false);
}

// --------------------------------------------------------------------------
// Issue a START/STOP Unit command.

IOReturn
IOATAPIHDDrive::doStartStop(bool doStart)
{
	IOReturn       ret;
	IOATACommand * cmd;

	cmd = atapiCommandStartStopUnit(0,          /* senseData */
                                    doStart,    /* start unit */
                                    false,      /* Load/Eject */
                                    false);     /* Immediate operation */

	if (!cmd) return kIOReturnNoMemory;

	ret = getIOReturn(syncExecute(cmd));

	cmd->release();

	return ret;
}

// --------------------------------------------------------------------------
// Return device identification strings

char * IOATAPIHDDrive::getVendorString()
{
    return _vendor;
}

char * IOATAPIHDDrive::getProductString()
{
    return _product;
}

char * IOATAPIHDDrive::getRevisionString()
{
    return _revision;
}

char * IOATAPIHDDrive::getAdditionalDeviceInfoString()
{
    return ("[ATAPI]");
}

// --------------------------------------------------------------------------
// Report whether the media in the drive is ejectable.

IOReturn
IOATAPIHDDrive::reportEjectability(bool * isEjectable)
{
	*isEjectable = true;	/* default: if it's removable, it's ejectable */
	return kIOReturnSuccess;
}

// --------------------------------------------------------------------------
// Report whether the drive can prevent user-initiated ejects by locking
// the media in the drive.

IOReturn
IOATAPIHDDrive::reportLockability(bool * isLockable)
{
    *isLockable = true;		/* default: if it's removable, it's lockable */
	return kIOReturnSuccess;
}

// --------------------------------------------------------------------------
// Report our polling requirments.

IOReturn
IOATAPIHDDrive::reportPollRequirements(bool * pollRequired,
                                       bool * pollIsExpensive)
{
    *pollIsExpensive = false;
    *pollRequired    = _isRemovable;
    return kIOReturnSuccess;
}

// --------------------------------------------------------------------------
// Report the current state of the media.

IOReturn
IOATAPIHDDrive::reportMediaState(bool * mediaPresent, 
                                 bool * changed)
{
	IOATACommand *       cmd;
	ATAResults  		 results;
	IOReturn			 ret = kIOReturnSuccess;
	UInt8				 senseData[18];
	IOMemoryDescriptor * senseDesc;

	assert(mediaPresent && changed);

    senseDesc = IOMemoryDescriptor::withAddress(senseData,
	                                    sizeof(senseData),
	                                    kIODirectionIn);
    if (!senseDesc)
        return kIOReturnNoMemory;

	cmd = atapiCommandTestUnitReady(senseDesc);
	if (!cmd)
		return kIOReturnNoMemory;

	// Execute the Test Unit Ready command with no retries.
	//
	syncExecute(cmd, ATADefaultTimeout, 0);

	// Release the memory descriptor.
	//
	senseDesc->release();

	if (cmd->getResults(&results) == ataReturnNoError)
	{
		*mediaPresent = true;
		*changed = (*mediaPresent != _mediaPresent);
		_mediaPresent = true;
	}
	else
	{
		UInt8 errorCode      = senseData[0];
		UInt8 senseKey       = senseData[2];

#ifdef DEBUG_LOG
		UInt8 senseCode      = senseData[12];
		UInt8 senseQualifier = senseData[13];

		IOLog("-- reportMediaState --\n");
		IOLog("Error code: %02x\n", errorCode);
		IOLog("Sense Key : %02x\n", senseKey);
		IOLog("ASC       : %02x\n", senseCode);
		IOLog("ASCQ      : %02x\n", senseQualifier);
#endif

		*mediaPresent = false;
		*changed = (*mediaPresent != _mediaPresent);
		_mediaPresent = false;

		// The error code field for ATAPI request sense should always
		// be 0x70 or 0x71. Otherwise ignore the sense data.
		//
		if ((errorCode == 0x70) || (errorCode == 0x71))
		{
			switch (senseKey) {	
				case 5:		/* Invalid ATAPI command */
					ret = kIOReturnIOError;
					break;

				case 2:		/* Not ready */
					break;

				default:
					break;
			}
		}
	}

	cmd->release();

#if 0
	IOLog("%s: media present %s, changed %s\n", getName(),
		*mediaPresent ? "Y" : "N",
		*changed ? "Y" : "N"
	);
#endif

#ifndef DISKPM
	if (*changed && *mediaPresent)
		doStart();
#endif

	return ret;
}

// --------------------------------------------------------------------------
// Report media removability.

IOReturn
IOATAPIHDDrive::reportRemovability(bool * isRemovable)
{
	UInt8	inqBuf[2];
	
	*isRemovable = false;
	
	if (_ataDevice->getInquiryData(sizeof(inqBuf), (ATAPIInquiry *) inqBuf))
	{
		if (inqBuf[1] & 0x80)
			*isRemovable = _isRemovable = true;
		else
			*isRemovable = _isRemovable = false;
	}

	return kIOReturnSuccess;
}

// --------------------------------------------------------------------------
// Report whether media is write-protected.

IOReturn
IOATAPIHDDrive::reportWriteProtection(bool * isWriteProtected)
{
	*isWriteProtected = false;		// defaults to read-write
	return kIOReturnSuccess;
}

// --------------------------------------------------------------------------
// Instantiate an ATAPI specific subclass of IOHDDriveNub.

IOService *
IOATAPIHDDrive::instantiateNub()
{
    IOService * nub = new IOATAPIHDDriveNub;
    return nub;
}
