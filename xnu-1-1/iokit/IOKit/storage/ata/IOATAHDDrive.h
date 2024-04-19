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
 * IOATAHDDrive.h
 *
 * HISTORY
 * Aug 27, 1999	jliu - Ported from AppleATADrive.
 */

#ifndef	_IOATAHDDRIVE_H
#define	_IOATAHDDRIVE_H

#include <IOKit/IOTypes.h>
#include <IOKit/ata/IOATA.h>
#include <IOKit/IOInterruptEventSource.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOLocks.h>
#include <IOKit/storage/IOHDTypes.h>

class IOSyncer;

// ATA parameters.
//
#define kIOATASectorSize		512
#define kIOATAMaxBlocksPerXfer	256

// ATA commands.
//
enum {
	kIOATACommandReadPIO      = 0x20,
	kIOATACommandWritePIO     = 0x30,
	kIOATACommandReadDMA      = 0xc8,
	kIOATACommandWriteDMA     = 0xca,
	kIOATACommandSetFeatures  = 0xef,
};

// ==========================================================================
// IOATAClientData - This structure is stored on the IOATACommand's
// driver private area.
// ==========================================================================

struct IOATAClientData {
	IOATACommand *          command;    // back pointer to command object.
	IOMemoryDescriptor *    buffer;     // memory descriptor.
	union {
		struct {
			IOService *             target; // completion target.
			gdCompletionFunction    action; // completion action.
			void *                  param;  // completion param.
		} async;
		IOSyncer *              syncLock;   // lock for synchronous operation.
	} completion;
	bool                    isSync;     // synchronous command.
	SInt32                  maxRetries; // max retry attempts (0 is no retry).
	ATAReturnCode           returnCode; // operation return code.
	queue_chain_t           link;       // queue linkage.
};

// Get driver private data (IOATAClientData) from an IOATACommand object.
//
#define ATA_CLIENT_DATA(x)	((IOATAClientData *)(x->getClientData()))

// ==========================================================================
// IOATACommandQueue
// ==========================================================================

class IOATACommandQueue : public OSObject
{
    OSDeclareDefaultStructors(IOATACommandQueue)

protected:
	queue_head_t	_queue;
	IORecursiveLock * _cmdLock;
	IOSimpleLock *  _qLock;
	bool            _enabled;

	virtual void free();
	
public:
	static IOATACommandQueue * commmandQueue();
	virtual bool init();
	
	void executeCommands();
	bool enqueueCommand(IOATACommand * cmd);
	void setEnabled(bool enable);
};

// ==========================================================================
// IOATAHDDrive
// ==========================================================================

class IOATAHDDrive : public IOService
{
    OSDeclareDefaultStructors(IOATAHDDrive)

protected:
	IOATADevice *				_ataDevice;
	IOWorkLoop *				_workloop;
	IOInterruptEventSource * 	_evSource;
	UInt						_unit;

	ATATimingProtocol			_timingProtocol;
	ATAProtocol					_ataProtocol;
	UInt8						_ataReadCmd;
	UInt8						_ataWriteCmd;

	IOATACommandQueue *         _retryQueue;

	char						_revision[9];
	char						_model[41];

	//-----------------------------------------------------------------------
	// Default timeout interval for async and sync commands.
	
    static const UInt ATADefaultTimeout = 30000;		// 30 seconds

	//-----------------------------------------------------------------------
	// Default retry count for async and sync commands.

	static const UInt ATADefaultRetries = 4;

	//-----------------------------------------------------------------------
	// Release all allocated resource before calling super::free().

	virtual void           free();

	//-----------------------------------------------------------------------
	// Set the ATA device timings.

	virtual bool           selectTimingProtocol(ATATimingProtocol protocol = 
                                                ataMaxTimings);

	//-----------------------------------------------------------------------
	// Selects the Command protocol to use (e.g. ataProtocolPIO, 
	// ataProtocolDMA).

	virtual void           selectCommandProtocol(bool useDMA);

	//-----------------------------------------------------------------------
	// Setup an ATATaskFile from the parameters given, and write the taskfile
	// to the ATATaskfile structure pointer provided.

	virtual void           setupReadWriteTaskFile(ATATaskfile * taskfile,
                                                  ATAProtocol   protocol,
                                                  UInt8         command,
                                                  UInt32        block,
                                                  UInt32        nblks);

	//-----------------------------------------------------------------------
	// Allocate and return an IOATACommand that is initialized to perform
	// a read/write operation.

	virtual IOATACommand * ataCommandReadWrite(IOMemoryDescriptor * buffer,
                                               UInt32               block,
                                               UInt32               nblks);

	//-----------------------------------------------------------------------
	// Allocate and return a ATA Set Features command.

	virtual IOATACommand * ataCommandSetFeatures(UInt8 features,
                                                 UInt8 SectorCount,
                                                 UInt8 SectorNumber,
                                                 UInt8 CylinderLow,
                                                 UInt8 CyclinderHigh);

	//-----------------------------------------------------------------------
	// This method is responsible for calling the client's completion routine
	// for an async command.

	virtual void           completionCallback(IOService *          target,
                                              gdCompletionFunction action,
                                              void *               param,
                                              UInt32               bytes,
                                              IOReturn             status);

	//-----------------------------------------------------------------------
	// This routine is called by our provider when a command processing has
	// completed.

	virtual void           completionHandler(IOService *     device,
                                             IOATACommand *  cmd,
                                             void *          refcon);

	//-----------------------------------------------------------------------
	// Issue a synchronous ATA command.

	virtual ATAReturnCode  syncExecute(
                               IOATACommand * cmd,
                               UInt32         timeout = ATADefaultTimeout,
                               UInt           retries = ATADefaultRetries);

	//-----------------------------------------------------------------------
	// Issue an asynchronous ATA command.

	virtual ATAReturnCode  asyncExecute(
                             IOATACommand *       cmd,
                             IOService *          target,
                             gdCompletionFunction action,
                             void *               param,
                             UInt32               timeout = ATADefaultTimeout,                                                                             
                             UInt                 retries = ATADefaultRetries);

	//-----------------------------------------------------------------------
	// Add a command to the retry/sync queue.

	virtual bool           enqueueCommand(IOATACommand * cmd);

	//-----------------------------------------------------------------------
	// Dequeues commands from the queue and executes them.

	virtual void           dequeueCommands(IOInterruptEventSource * source,
                                           int                      count);

	//-----------------------------------------------------------------------
	// Allocate an IOATACommand object.

	virtual IOATACommand * allocateCommand();

	//-----------------------------------------------------------------------
	// Inspect the ATA device.

	virtual void           inspectDevice(IOService * provider);

	//-----------------------------------------------------------------------
	// Returns an IOATAHDDriveNub instance.

	virtual IOService *    instantiateNub();

	//-----------------------------------------------------------------------
	// Calls instantiateNub() then initialize, attach, and register the
	// drive nub.

	virtual bool           createNub(IOService * provider);

public:
	/*
	 * Overrides from IOService.
	 */
    virtual bool           init(OSDictionary * properties);
	virtual IOService *    probe(IOService * provider, SInt32 * score);
    virtual bool           start(IOService * provider);

	//-----------------------------------------------------------------------
	// Report the type of ATA device (ATA vs. ATAPI).

	virtual ATADeviceType  reportATADeviceType() const;

	//-----------------------------------------------------------------------
	// Handles read/write requests.

	virtual IOReturn       doAsyncReadWrite(IOMemoryDescriptor * buffer,
                                            UInt32               block,
                                            UInt32               nblks,
                                            gdCompletionFunction action,
                                            IOService *          target,
                                            void *               param);

	//-----------------------------------------------------------------------
	// Eject the media in the drive.

	virtual IOReturn       doEjectMedia();

	//-----------------------------------------------------------------------
	// Format the media in the drive.

	virtual IOReturn       doFormatMedia(UInt64 byteCapacity);

	//-----------------------------------------------------------------------
	// Returns disk capacity in bytes.

	virtual UInt32         doGetFormatCapacities(UInt64 * capacities,
                                      UInt32   capacitiesMaxCount) const;

	//-----------------------------------------------------------------------
	// Lock the media and prevent a user-initiated eject.
	
	virtual IOReturn       doLockUnlockMedia(bool doLock);

	//-----------------------------------------------------------------------
	// Flush the write-cache to the physical media.

	virtual IOReturn       doSynchronizeCache();

	//-----------------------------------------------------------------------
	// Start/stop the drive.

	virtual IOReturn       doStart();
	virtual IOReturn       doStop();

	//-----------------------------------------------------------------------
	// Return device identification strings

	virtual char *         getAdditionalDeviceInfoString();
	virtual char *         getProductString();
	virtual char *         getRevisionString();
	virtual char *         getVendorString();

	//-----------------------------------------------------------------------
	// Report the device block size in bytes.

	virtual IOReturn       reportBlockSize(UInt64 * blockSize);

	//-----------------------------------------------------------------------
	// Report whether the media in the ATA device is ejectable.

	virtual IOReturn       reportEjectability(bool * isEjectable);

	//-----------------------------------------------------------------------
	// Report whether the media can be locked.

	virtual IOReturn       reportLockability(bool * isLockable);

	//-----------------------------------------------------------------------
	// Report the polling requirements for a removable media.

	virtual IOReturn       reportPollRequirements(bool * pollRequired,
                                                  bool * pollIsExpensive);

	//-----------------------------------------------------------------------
	// Report the max number of bytes transferred for an ATA read command.

	virtual IOReturn       reportMaxReadTransfer(UInt64   blocksize,
                                                 UInt64 * max);

	//-----------------------------------------------------------------------
	// Report the max number of bytes transferred for an ATA write command.

	virtual IOReturn       reportMaxWriteTransfer(UInt64   blocksize,
                                                  UInt64 * max);

	//-----------------------------------------------------------------------
	// Returns the maximum addressable sector number.

	virtual IOReturn       reportMaxValidBlock(UInt64 * maxBlock);

	//-----------------------------------------------------------------------
	// Report whether the media is currently present, and whether a media
	// change has been registered since the last reporting.

	virtual IOReturn       reportMediaState(bool * mediaPresent, 
                                            bool * changed);
	
	//-----------------------------------------------------------------------
	// Report whether the media is removable.
	
	virtual IOReturn       reportRemovability(bool * isRemovable);
	
	//-----------------------------------------------------------------------
	// Report if the media is write-protected.

	virtual IOReturn       reportWriteProtection(bool * isWriteProtected);

	//-----------------------------------------------------------------------
	// Handles messages (notifications) from our provider.

	virtual IOReturn       message(UInt32 type, IOService * provider,
                                   void * argument);

	//-----------------------------------------------------------------------
	// Perform translation from ATAReturnCode to IOReturn codes.

	virtual IOReturn       getIOReturn(ATAResults * result);
	virtual IOReturn       getIOReturn(ATAReturnCode code);

	//-----------------------------------------------------------------------
	// Returns the device type.

    virtual const char *   getDeviceTypeName();
};

#endif /* !_IOATAHDDRIVE_H */
