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
 * IOATAHDDrive.cpp - Generic ATA disk driver.
 *
 * HISTORY
 * Aug 27, 1999	jliu - Ported from AppleATADrive.
 */

#include <IOKit/assert.h>
#include <IOKit/storage/ata/IOATAHDDrive.h>
#include <IOKit/storage/ata/IOATAHDDriveNub.h>

#define	super IOService
OSDefineMetaClassAndStructors( IOATAHDDrive, IOService )

// --------------------------------------------------------------------------
// init() method.

bool
IOATAHDDrive::init(OSDictionary * properties)
{
	_ataDevice  = 0;	// provider (nub).
	_workloop   = 0;	// workloop to service command retries.
	_evSource   = 0;	// signals workloop that work is available.
	_retryQueue = 0;	// queue object to store retry commands.

    return (super::init(properties));
}

// --------------------------------------------------------------------------
// Override probe() method inherited from IOService.

IOService * 
IOATAHDDrive::probe(IOService * provider, SInt32 * score)
{
    if (!super::probe(provider, score))
        return 0;

	// Our provider must be a IOATADevice nub, most likely created
	// by an IOATAController instance.
	//
	IOATADevice * device = OSDynamicCast(IOATADevice, provider);
	if (!device)
		return 0;	// IOATADevice nub not found.

	// Do ATA device type matching.
	//
	if (device->getDeviceType() != reportATADeviceType())
		return 0;	// error, type mismatch.

	return this;	// probe successful.
}

// --------------------------------------------------------------------------
// Starts up the driver and spawn a nub.

bool
IOATAHDDrive::start(IOService * provider)
{
	// First call start() in our superclass.
	//
	if (!super::start(provider))
		return false;

	// Cache our provider.
	//
	_ataDevice = OSDynamicCast(IOATADevice, provider);
	if (!_ataDevice)
		return false;

	// Open our provider.
	//
	_ataDevice->retain();
	_ataDevice->open(this);

	// Inspect the provider.
	//
	inspectDevice(_ataDevice);

	// Cache the drive unit number (master/slave assignment).
	//
	_unit = _ataDevice->getUnit();

	// Allocate a workloop which is solely responsible for performing
	// command retries. It is otherwise not involved in the data path.
	//
	_workloop = IOWorkLoop::workLoop();
	if (!_workloop)
		return false;

	// Create an event source to trigger the workloop when a command
	// needs to be retried.
	//
	_evSource = IOInterruptEventSource::interruptEventSource(
                    this,
                    (IOInterruptEventAction) &IOATAHDDrive::dequeueCommands,
                    0);

	if (!_evSource ||
		(_workloop->addEventSource(_evSource) != kIOReturnSuccess))
		return false;

	// Initialize _retryQueue to store sync and retry command objects.
	//
	_retryQueue = IOATACommandQueue::commmandQueue();
	if (!_retryQueue)
		return false;

	// Select ATA timing.
	//
	if (!selectTimingProtocol())
		return false;

	// Enable retry queue.
	//
	_retryQueue->setEnabled(true);

	return (createNub(provider));
}

// --------------------------------------------------------------------------
// Release allocated resources.

void
IOATAHDDrive::free()
{
	if (_evSource)
		_evSource->release();
	if (_workloop)
		_workloop->release();
	if (_ataDevice)
		_ataDevice->release();
	if (_retryQueue)
		_retryQueue->release();
	
	super::free();
}

// --------------------------------------------------------------------------
// Fetch information about the ATA device nub.

void
IOATAHDDrive::inspectDevice(IOService * provider)
{
	OSString *    string;
	IOATADevice * ataDevice = OSDynamicCast(IOATADevice, provider);
	if (!ataDevice)
		return;

	// Fetch ATA device information from the nub.
	//
	string = OSDynamicCast(OSString,
                           ataDevice->getProperty(ATAPropertyModelNumber));
	if (string) {		
		strncpy(_model, string->getCStringNoCopy(), 40);
		_model[40] = '\0';
	}

	string = OSDynamicCast(OSString,
                           ataDevice->getProperty(ATAPropertyFirmwareRev));
	if (string) {
		strncpy(_revision, string->getCStringNoCopy(), 8);
		_revision[8] = '\0';
	}
}

// --------------------------------------------------------------------------
// Report the type of ATA device (ATA vs. ATAPI).

ATADeviceType
IOATAHDDrive::reportATADeviceType() const
{
	return ataDeviceATA;
}

// --------------------------------------------------------------------------
// Returns the device type.

const char *
IOATAHDDrive::getDeviceTypeName()
{
	return kDeviceTypeHardDisk;
}

// --------------------------------------------------------------------------
// Instantiate an ATA specific subclass of IOHDDriveNub.

IOService * IOATAHDDrive::instantiateNub()
{
    IOService * nub = new IOATAHDDriveNub;
    return nub;
}

// --------------------------------------------------------------------------
// Returns an IOATAHDDriveNub.

bool IOATAHDDrive::createNub(IOService * provider)
{
    IOService * nub;

	// Instantiate a generic hard disk nub so a generic driver
	// can match above us.
	//
    nub = instantiateNub();

    if (nub == 0) {
        IOLog("%s: instantiateNub() failed\n", getName());
        return false;
    }

    nub->init();
    
    if (!nub->attach(this))
        IOPanic("IOATAHDDrive::createNub; couldn't attach IOATAHDDriveNub");

    nub->registerService();

    return true;
}

// --------------------------------------------------------------------------
// Handles read/write requests.

IOReturn IOATAHDDrive::doAsyncReadWrite(IOMemoryDescriptor * buffer,
                                        UInt32               block,
                                        UInt32               nblks,
                                        gdCompletionFunction action,
                                        IOService *          target,
                                        void *               param)
{
	IOReturn       ret;
	IOATACommand * cmd = ataCommandReadWrite(buffer, block, nblks);
	if (!cmd)
		return kIOReturnNoMemory;

	ret = getIOReturn(asyncExecute(cmd, target, action, param));

	cmd->release();

	return ret;
}

// --------------------------------------------------------------------------
// Eject the media in the drive.

IOReturn IOATAHDDrive::doEjectMedia()
{
	return kIOReturnUnsupported;	// No support for removable ATA devices.
}

// --------------------------------------------------------------------------
// Format the media in the drive.
// ATA devices does not support low level formatting.

IOReturn IOATAHDDrive::doFormatMedia(UInt64 byteCapacity)
{
	return kIOReturnUnsupported;
}

// --------------------------------------------------------------------------
// Returns disk capacity.

UInt32 IOATAHDDrive::doGetFormatCapacities(UInt64 * capacities,
                                           UInt32   capacitiesMaxCount) const
{
	UInt32		blockCount = 0;
	UInt32		blockSize  = 0;

	assert(_ataDevice);

	if (_ataDevice->getDeviceCapacity(&blockCount, &blockSize) &&
		(capacities != NULL) && (capacitiesMaxCount > 0)) {
		UInt64 count = blockCount;
		UInt64 size  = blockSize;

		*capacities = size * (count + 1);
		
		return 1;
	}
	
	return 0;
}

// --------------------------------------------------------------------------
// Lock the media and prevent a user-initiated eject.

IOReturn IOATAHDDrive::doLockUnlockMedia(bool doLock)
{
	return kIOReturnUnsupported;	// No removable ATA device support.
}

// --------------------------------------------------------------------------
// Flush the write-cache to the physical media.

IOReturn IOATAHDDrive::doSynchronizeCache()
{
	// FIXME - A lie, need implementation.
	//
	return kIOReturnSuccess;
}

// --------------------------------------------------------------------------
// Handle a Start Unit command.

IOReturn
IOATAHDDrive::doStart()
{
	return kIOReturnSuccess;
}

// --------------------------------------------------------------------------
// Handle a Stop Unit command.

IOReturn
IOATAHDDrive::doStop()
{
	return kIOReturnSuccess;
}

// --------------------------------------------------------------------------
// Return device identification strings.

char * IOATAHDDrive::getAdditionalDeviceInfoString()
{
    return ("[ATA]");
}

char * IOATAHDDrive::getProductString()
{
    return _model;
}

char * IOATAHDDrive::getRevisionString()
{
    return _revision;
}

char * IOATAHDDrive::getVendorString()
{
    return NULL;
}

// --------------------------------------------------------------------------
// Report the device block size in bytes. We ask the device nub for the
// block size. We expect this to be 512-bytes.

IOReturn IOATAHDDrive::reportBlockSize(UInt64 * blockSize)
{
	UInt32  blkCount = 0;
	UInt32  blkSize  = 0;

	assert(_ataDevice);

	if (!_ataDevice->getDeviceCapacity(&blkCount, &blkSize))
		return kIOReturnNoDevice;

	*blockSize = blkSize;
    return kIOReturnSuccess;
}

// --------------------------------------------------------------------------
// Report the media in the ATA device as non-ejectable.

IOReturn IOATAHDDrive::reportEjectability(bool * isEjectable)
{
	*isEjectable = false;
	return kIOReturnSuccess;
}

// --------------------------------------------------------------------------
// Fixed media, locking is invalid.

IOReturn IOATAHDDrive::reportLockability(bool * isLockable)
{
    *isLockable = false;
	return kIOReturnSuccess;
}

// --------------------------------------------------------------------------
// Report the polling requirements for a removable media.

IOReturn IOATAHDDrive::reportPollRequirements(bool * pollRequired,
                                              bool * pollIsExpensive)
{
    *pollIsExpensive = false;
    *pollRequired    = false;

    return kIOReturnSuccess;
}

// --------------------------------------------------------------------------
// Report the max number of bytes transferred for an ATA read command.

IOReturn IOATAHDDrive::reportMaxReadTransfer(UInt64 blocksize, UInt64 * max)
{
    *max = blocksize * kIOATAMaxBlocksPerXfer;
    return kIOReturnSuccess;
}

// --------------------------------------------------------------------------
// Report the max number of bytes transferred for an ATA write command.

IOReturn IOATAHDDrive::reportMaxWriteTransfer(UInt64 blocksize, UInt64 * max)
{
	// Same as read transfer limits.
	//
    return reportMaxReadTransfer(blocksize, max);
}

// --------------------------------------------------------------------------
// Returns the maximum addressable sector number.

IOReturn IOATAHDDrive::reportMaxValidBlock(UInt64 * maxBlock)
{
	UInt32		blockCount = 0;
	UInt32		blockSize  = 0;

	assert(_ataDevice && maxBlock);

	if (!_ataDevice->getDeviceCapacity(&blockCount, &blockSize))
		return kIOReturnNoDevice;

    *maxBlock = blockCount;

    return kIOReturnSuccess;
}

// --------------------------------------------------------------------------
// Report whether the media is currently present, and whether a media
// change has been registered since the last reporting.

IOReturn IOATAHDDrive::reportMediaState(bool * mediaPresent, bool * changed)
{
	*mediaPresent = true;
	*changed      = true;
	
	return kIOReturnSuccess;
}

// --------------------------------------------------------------------------
// Report whether the media is removable.

IOReturn IOATAHDDrive::reportRemovability(bool * isRemovable)
{
	*isRemovable = false;
	return kIOReturnSuccess;
}

// --------------------------------------------------------------------------
// Report if the media is write-protected.

IOReturn IOATAHDDrive::reportWriteProtection(bool * isWriteProtected)
{
	*isWriteProtected = false;
	return kIOReturnSuccess;
}

// --------------------------------------------------------------------------
// Handles messages from our provider.

IOReturn
IOATAHDDrive::message(UInt32 type, IOService * provider, void * argument)
{
	IOReturn ret = kIOReturnSuccess;

	switch (type)
	{
		case ataMessageInitDevice:
			if (!selectTimingProtocol(_timingProtocol))
				ret = kIOReturnIOError;
			break;

		case ataMessageResetStarted:
			//
			// Freeze the sync/retry queue.
			// What about async commands?
			//
			_retryQueue->setEnabled(false);
			break;

		case ataMessageResetComplete:
			//
			// Re-enable the sync/retry queue.
			//
			_retryQueue->setEnabled(true);
			_evSource->interruptOccurred(0, 0, 0);
			break;

		default:
			ret = super::message(type, provider, argument);
			break;
	}

	return ret;
}

// --------------------------------------------------------------------------
// Perform translation from ATAReturnCode to IOReturn codes.

static inline IOReturn
getIOReturnFunc(ATAReturnCode code)
{
	switch (code) {
		case ataReturnNoError:
			return kIOReturnSuccess;

		case ataReturnNotSupported:
			return kIOReturnUnsupported;

		case ataReturnNoResource:
			return kIOReturnNoResources;

		case ataReturnErrorBusy:
			return kIOReturnBusy;

		case ataReturnErrorInterruptTimeout:
			return kIOReturnTimeout;

		case ataReturnErrorRetryPIO:
		case ataReturnErrorStatus:
		case ataReturnErrorProtocol:
		default:
			return kIOReturnIOError;
	}
}

IOReturn
IOATAHDDrive::getIOReturn(ATAResults * result)
{
	// Translate the return code in an ATAResult structure.
	//
	return getIOReturnFunc(result->returnCode);
}

IOReturn
IOATAHDDrive::getIOReturn(ATAReturnCode code)
{
	return getIOReturnFunc(code);
}
