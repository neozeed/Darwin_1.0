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
 * IOATAHDCommand.cpp - Performs ATA command processing.
 *
 * HISTORY
 * Aug 27, 1999	jliu - Ported from AppleATADrive.
 */

#include <IOKit/assert.h>
#include <IOKit/IOSyncer.h>
#include <IOKit/storage/ata/IOATAHDDrive.h>

// Enable this define to generate debugging messages.
// #define DEBUG_LOG 1

// --------------------------------------------------------------------------
// Set the device timings.

bool
IOATAHDDrive::selectTimingProtocol(ATATimingProtocol protocol)
{
	bool				ret;
	UInt8				ataReadCmd;
	UInt8				ataWriteCmd;
	
	if (protocol >= ataMaxTimings)
	{
		// If protocol override is invalid, get the list of supported timing
		// protocols from the device, and use the 'best' one. If the device
		// cannot report the available protocols, default to PIO mode.
	
		ret = _ataDevice->getTimingsSupported(&protocol);
		if (!ret)
		{
			IOLog("%s: getTimingsSupported() error\n", getName());
			protocol = ataTimingPIO;
		}
	}

	// IOLog("%s: timing protocols: %08x\n", getName(), protocol);
	
	if (protocol & (ataTimingUltraDMA66 | ataTimingUltraDMA33 | ataTimingDMA)) 
	{
		if (protocol & ataTimingUltraDMA66)
		{
			IOLog("%s: Using U-DMA/66 transfers\n", getName());
			protocol = ataTimingUltraDMA66;
		}
		else if (protocol & ataTimingUltraDMA33)
		{
			IOLog("%s: Using U-DMA/33 transfers\n", getName());
			protocol = ataTimingUltraDMA33;
		}
		else
		{
			IOLog("%s: Using DMA transfers\n", getName());
			protocol = ataTimingDMA;
		}

		ataReadCmd  = kIOATACommandReadDMA;
		ataWriteCmd = kIOATACommandWriteDMA;		
		selectCommandProtocol(true);
	}
	else
	{
		IOLog("%s: Using PIO transfers\n", getName());
		protocol    = ataTimingPIO;
		ataReadCmd  = kIOATACommandReadPIO;
		ataWriteCmd = kIOATACommandWritePIO;
		selectCommandProtocol(false);
	}

	ret = _ataDevice->selectTiming(protocol);
	if (!ret) {
		IOLog("%s: selectTiming() failed\n", getName());
	}
	else {
		_timingProtocol = protocol;
		_ataReadCmd     = ataReadCmd;
		_ataWriteCmd    = ataWriteCmd;
	}

	return ret;
}

// --------------------------------------------------------------------------
// Returns the Command protocol to use (e.g. ataProtocolPIO, ataProtocolDMA).

void
IOATAHDDrive::selectCommandProtocol(bool useDMA)
{
	if (useDMA)
		_ataProtocol = ataProtocolDMA;
	else
		_ataProtocol = ataProtocolPIO;
}

// --------------------------------------------------------------------------
// Setup an ATATaskFile from the parameters given, and write the taskfile
// to the ATATaskfile structure pointer provided.
//
// taskfile - pointer to a taskfile structure.
// protocol - An ATA transfer protocol (ataProtocolPIO, ataProtocolDMA, etc)
// command  - ATA command byte.
// block    - Initial transfer block.
// nblks    - Number of blocks to transfer.

void
IOATAHDDrive::setupReadWriteTaskFile(ATATaskfile * taskfile,
                                     ATAProtocol   protocol,
                                     UInt8         command,
                                     UInt32        block,
                                     UInt32        nblks)
{
	taskfile->protocol = protocol;

	// Mask of all taskfile registers that shall contain valid
	// data and should be written to the hardware registers.
	//
	taskfile->regmask  = ATARegtoMask(ataRegSectorNumber)	|
                         ATARegtoMask(ataRegCylinderLow)	|
                         ATARegtoMask(ataRegCylinderHigh)	|
						 ATARegtoMask(ataRegDriveHead)		|
						 ATARegtoMask(ataRegSectorCount)	|
						 ATARegtoMask(ataRegCommand);

	taskfile->resultmask = 0;

	taskfile->ataRegs[ataRegSectorNumber] = block & 0x0ff;
	taskfile->ataRegs[ataRegCylinderLow]  = (block >> 8) & 0xff;
	taskfile->ataRegs[ataRegCylinderHigh] = (block >> 16) & 0xff;
	taskfile->ataRegs[ataRegDriveHead]    = ((block >> 24) & 0x0f) |
                                            ataModeLBA | (_unit << 4);  
	taskfile->ataRegs[ataRegSectorCount]  = (nblks == kIOATAMaxBlocksPerXfer) ? 
                                            0 : nblks;
    taskfile->ataRegs[ataRegCommand]      = command;
}

// --------------------------------------------------------------------------
// Allocate and return an IOATACommand that is initialized to perform
// a read/write operation.
//
// buffer   - IOMemoryDescriptor object describing this transfer.
// block    - Initial transfer block.
// nblks    - Number of blocks to transfer.

IOATACommand *
IOATAHDDrive::ataCommandReadWrite(IOMemoryDescriptor * buffer,
                                  UInt32               block,
                                  UInt32               nblks)
{
	ATATaskfile taskfile;
	bool        isWrite;

	assert(buffer);

	IOATACommand * cmd = allocateCommand();
	if (!cmd) return 0;		// error, command allocation failed.

	isWrite = (buffer->getDirection() == kIODirectionOut) ?
	          true : false;

#ifdef DEBUG_LOG
	IOLog("%s: ataCommandReadWrite %08x (%d) %s %d %d\n",
		getName(),
		buffer,
		buffer->getLength(),
		isWrite ? "WR" : "RD",
		block,
		nblks);
#endif

#if 0	// used for testing - force PIO mode
	setupReadWriteTaskFile(&taskfile,
	                       ataProtocolPIO,
	                       isWrite ? kIOATACommandWritePIO : 
						   kIOATACommandReadPIO,
	                       block,
	                       nblks);
#else

	// Setup the taskfile structure with the size and direction of the
	// transfer. This structure will be written to the actual taskfile
	// registers when this command is processed.
	//
	setupReadWriteTaskFile(&taskfile,
	                       _ataProtocol,
	                       isWrite ? _ataWriteCmd : _ataReadCmd,
	                       block,
	                       nblks);
#endif

	// Get a pointer to the client data buffer, and record parameters
	// which shall be later used by the completion routine.
	//
	IOATAClientData * clientData = ATA_CLIENT_DATA(cmd);
	assert(clientData);

	clientData->buffer  = buffer;

	cmd->setTaskfile(&taskfile);

	cmd->setPointers(buffer,                /* (IOMemoryDescriptor *) */
                     buffer->getLength(),   /* transferCount (bytes) */
                     isWrite);              /* isWrite */

	return cmd;
}

// --------------------------------------------------------------------------
// Allocate and return a ATA SetFeatures command.

IOATACommand *
IOATAHDDrive::ataCommandSetFeatures(UInt8 features,
                                    UInt8 SectorCount,
                                    UInt8 SectorNumber,
                                    UInt8 CylinderLow,
                                    UInt8 CyclinderHigh)
{
	ATATaskfile  taskfile;

	IOATACommand * cmd = allocateCommand();
	if (!cmd) return 0;		// error, command allocation failed.

	taskfile.protocol   = ataProtocolSetRegs;

	taskfile.regmask    = ATARegtoMask(ataRegSectorNumber)  |
                          ATARegtoMask(ataRegCylinderLow)   |
                          ATARegtoMask(ataRegCylinderHigh)  |
                          ATARegtoMask(ataRegDriveHead)     |
                          ATARegtoMask(ataRegSectorCount)   |
                          ATARegtoMask(ataRegCommand);

	taskfile.resultmask = ATARegtoMask(ataRegError) |
                          ATARegtoMask(ataRegStatus);

	taskfile.ataRegs[ataRegFeatures]     = features;
	taskfile.ataRegs[ataRegSectorNumber] = SectorNumber;
	taskfile.ataRegs[ataRegCylinderLow]  = CylinderLow;
	taskfile.ataRegs[ataRegCylinderHigh] = CyclinderHigh;
    taskfile.ataRegs[ataRegDriveHead]    = ataModeLBA | (_unit << 4);
	taskfile.ataRegs[ataRegSectorCount]  = SectorCount;
    taskfile.ataRegs[ataRegCommand]      = kIOATACommandSetFeatures;

	cmd->setTaskfile(&taskfile);

	return cmd;
}

// --------------------------------------------------------------------------
// This method is responsible for calling the client's completion routine
// for an async command.

void
IOATAHDDrive::completionCallback(IOService *          target,
                                 gdCompletionFunction action,
                                 void *               param,
                                 UInt32               bytesTransferred,
                                 IOReturn             status)
{
	(*action)(target, param, bytesTransferred, status);
}

// --------------------------------------------------------------------------
// This routine is called by our provider when a command processing has
// completed. We currently do not use the 'refcon' argument.

void
IOATAHDDrive::completionHandler(IOService *     device,
                                IOATACommand *  cmd,
                                void *          refcon)
{
	assert(cmd && device);

	ATAResults  	  results;
	IOATAClientData * clientData = ATA_CLIENT_DATA(cmd);

	assert(clientData);

	if ((cmd->getResults(&results) != ataReturnNoError) &&
		(clientData->maxRetries-- > 0) &&
		enqueueCommand(cmd))
		return;

#if 0
	// Force command retry to test retry logic.
	// Controller will reset the IOMemoryDescriptor's position, right?
	//
	cmd->getResults(&results);
	if (clientData->maxRetries-- > 2) {		
		enqueueCommand(cmd);
		return;
	}
#endif

#ifdef DEBUG_LOG
	IOLog("%s: completionHandler %08x %08x %08x %08x %d\n",
		getName(), device, cmd, refcon, getIOReturn(&results),
		results.bytesTransferred);
#endif

	// Return ATAReturnCode for sync commands.
	//
	clientData->returnCode = results.returnCode;

	if (clientData->isSync) {
		// For sync commands, unblock the client thread.
		//
		assert(clientData->completion.syncLock);
		clientData->completion.syncLock->signal();	// unblock the client.
	}
	else {
		// The TAP fields must be set for an async command.
		// Signal the completion routine that the request has been completed.
		//
		assert(clientData->completion.async.action &&
			   clientData->completion.async.target);

		completionCallback(clientData->completion.async.target,
                           clientData->completion.async.action,
                           clientData->completion.async.param,
                           results.bytesTransferred,
                           getIOReturn(&results));
	}

	// Release the IOMemoryDescriptor.
	//
	if (clientData->buffer)
		clientData->buffer->release();

	// Command processing is complete, release the command object.
	//
	cmd->release();
}

// --------------------------------------------------------------------------
// Issue a synchronous ATA command.

ATAReturnCode
IOATAHDDrive::syncExecute(IOATACommand *  cmd,       /* command object */
                          UInt32          timeout,   /* timeout in ms */
                          UInt            retries)   /* max retries */
{
	IOATAClientData * clientData = ATA_CLIENT_DATA(cmd);

	// Bump the retain count on the command. The completion handler
	// will decrement the retain count.
	//
	cmd->retain();

	// Set timeout and register the completion handler.
	//
	cmd->setTimeout(timeout);
        cmd->setCallback(this,
                         (ATACallback) &IOATAHDDrive::completionHandler,
                         (void *) 0);

	// Increment the retain count on the IOMemoryDescriptor.
	// Release when the completion routine gets called.
	//
	if (clientData->buffer)
		clientData->buffer->retain();

	// Set the max retry count. If retry count is 0, then the command shall
	// not be retried if an error occurs.
	//
	clientData->maxRetries = retries;
	clientData->completion.syncLock = IOSyncer::create();
	clientData->isSync = true;

	cmd->execute();
//	enqueueCommand(cmd);	// queue command and kick off workloop.

	// Block client thread on lock until the completion handler
	// receives an indication that the processing is complete.
	//
        clientData->completion.syncLock->wait();

	return clientData->returnCode;
}

// --------------------------------------------------------------------------
// Issue an asynchronous ATA command.

ATAReturnCode
IOATAHDDrive::asyncExecute(IOATACommand *       cmd,      /* command object */
                           IOService *          target,
                           gdCompletionFunction action,
                           void *               param,
                           UInt32               timeout,  /* timeout in ms */
                           UInt                 retries)  /* max retries */
{
	IOATAClientData * clientData = ATA_CLIENT_DATA(cmd);

	// For async commands, the completion target/action must have
	// not be 0.
	//
	if ((target && action) == 0)
		return ataReturnNotSupported;

	// Bump the retain count on the command. The completion handler
	// will decrement the retain count.
	//
	cmd->retain();

	// Set timeout and register the completion handler.
	//
	cmd->setTimeout(timeout);
        cmd->setCallback(this,
                         (ATACallback) &IOATAHDDrive::completionHandler,
                         (void *) 0);

	// Increment the retain count on the IOMemoryDescriptor.
	// Release when the completion routine gets called.
	//
	if (clientData->buffer)
		clientData->buffer->retain();

	// Set the max retry count. If retry count is 0, then the command shall
	// not be retried if an error occurs.
	//
	clientData->maxRetries = retries;
	clientData->isSync     = false;
	
	clientData->completion.async.target = target;
	clientData->completion.async.action = action;
	clientData->completion.async.param  = param;

	return (cmd->execute() ? ataReturnNoError : ataReturnNoResource);
}

// --------------------------------------------------------------------------
// Add a command to the retry/sync queue.

bool
IOATAHDDrive::enqueueCommand(IOATACommand * cmd)
{
	// Add command to the queue.
	//
	_retryQueue->enqueueCommand(cmd);

	// wake up the workloop.
	//
	_evSource->interruptOccurred(0, 0, 0);

	return true;
}

// --------------------------------------------------------------------------
// Dequeues commands from the queue and executes them.

void
IOATAHDDrive::dequeueCommands(IOInterruptEventSource * /*source*/,
                              int /*count*/)
{
	_retryQueue->executeCommands();
}

// --------------------------------------------------------------------------
// Allocate an IOATACommand object.

IOATACommand *
IOATAHDDrive::allocateCommand()
{
	IOATACommand * cmd;

	cmd = _ataDevice->allocCommand(sizeof(IOATAClientData));
	return cmd;
}

// ==========================================================================
// IOATACommandQueue
// ==========================================================================

#define	super OSObject
OSDefineMetaClassAndStructors(IOATACommandQueue, OSObject)

// --------------------------------------------------------------------------
// Static member function which allocates and returns a queue object.

IOATACommandQueue * IOATACommandQueue::commmandQueue()
{
	IOATACommandQueue * queue = new IOATACommandQueue;
	
	if (queue && !queue->init()) {
		queue->release();
		queue = 0;
	}
	
	return queue;
}

// --------------------------------------------------------------------------
// Initializes the queue object.

bool IOATACommandQueue::init()
{
	_enabled = false;
	_cmdLock = 0;
	_qLock   = 0;

	if (!super::init())
		return false;

	// Initialize the queue.
	//
	queue_init(&_queue);

	_cmdLock = IORecursiveLockAlloc();
	if (!_cmdLock)
		return false;

	// Queue access is protected by a spinlock.
	//
	_qLock = IOSimpleLockAlloc();
	if (!_qLock)
		return false;
	
	return true;
}

// --------------------------------------------------------------------------
// Frees the queue object.

void IOATACommandQueue::free()
{
	if (_qLock) {
		IOSimpleLockFree(_qLock);
		_qLock = 0;
	}

	if (_cmdLock) {
		IORecursiveLockFree(_cmdLock);
		_cmdLock = 0;
	}
	
	//
	// What about IOATACommand objects in the queue?
	//
	
	super::free();
}

// --------------------------------------------------------------------------
// Dequeues and executes all IOATACommand objects in the queue.

void IOATACommandQueue::executeCommands()
{
	IOATAClientData * clientData;
	IOATACommand * 	  cmd;

        IORecursiveLockLock(_cmdLock);

	while (_enabled && !queue_empty(&_queue)) {
		IOSimpleLockLock(_qLock);
		queue_remove_first(&_queue, clientData, IOATAClientData *, link);
		IOSimpleLockUnlock(_qLock);
		
		assert(clientData);

		cmd = clientData->command;

#ifdef DEBUG_LOG
		IOLog("cmd:%08x clientData:%08x ATA_CLIENT_DATA(cmd):%08x\n",
			cmd, clientData, ATA_CLIENT_DATA(cmd));
#endif

		assert(cmd && (ATA_CLIENT_DATA(cmd) == clientData));

		cmd->execute();
	}

        IORecursiveLockUnlock(_cmdLock);
}

// --------------------------------------------------------------------------
// Enqueues an IOATACommand object to the queue.
// Returns true if the command specifed was enqueued.

bool IOATACommandQueue::enqueueCommand(IOATACommand * cmd)
{
	assert(cmd);

	IOATAClientData * clientData = ATA_CLIENT_DATA(cmd);

	// Make sure the clientData to IOATACommand linkage is correct.
	//
	clientData->command = cmd;

#ifdef DEBUG_LOG
	IOLog("enqueueCommand: %08x\n", (UInt) cmd);
#endif

	IOSimpleLockLock(_qLock);
	queue_enter(&_queue, clientData, IOATAClientData *, link);
	IOSimpleLockUnlock(_qLock);

	return true;
}

// --------------------------------------------------------------------------
// Enable/disable the queue.

void IOATACommandQueue::setEnabled(bool enable)
{
        IORecursiveLockLock(_cmdLock);
	_enabled = enable;
        IORecursiveLockUnlock(_cmdLock);
}
