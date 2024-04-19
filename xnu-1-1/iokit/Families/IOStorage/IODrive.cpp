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

#include <IOKit/assert.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/storage/IODrive.h>

#define super IOStorage
OSDefineMetaClass(IODrive, IOStorage)
OSDefineAbstractStructors(IODrive, IOStorage)

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

const UInt32 kPollerInterval = 1000;                           // (ms, 1 second)

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

bool IODrive::init(OSDictionary * properties = 0)
{
    //
    // Initialize this object's minimal state.
    //

    if (super::init(properties) == false)  return false;

    _deblockerLockForRMWs   = IOLockAlloc();
    _openClients            = OSSet::withCapacity(2);

    for (unsigned index = 0; index < kStatisticsCount; index++)
        _statistics[index] = OSNumber::withNumber(0ULL, 64);

    if (_deblockerLockForRMWs == 0 || _openClients == 0)  return false;

    for (unsigned index = 0; index < kStatisticsCount; index++)
        if (_statistics[index] == 0)  return false;

    IOLockInit(_deblockerLockForRMWs);

    //
    // Create the standard drive registry properties.
    //

    OSDictionary * statistics = OSDictionary::withCapacity(kStatisticsCount);

    if (statistics == 0)  return false;

    statistics->setObject( kIODriveStatisticsBytesRead,
                           _statistics[kStatisticsBytesRead] );
    statistics->setObject( kIODriveStatisticsBytesWritten,
                           _statistics[kStatisticsBytesWritten] );
    statistics->setObject( kIODriveStatisticsReadErrors,
                           _statistics[kStatisticsReadErrors] );
    statistics->setObject( kIODriveStatisticsWriteErrors,
                           _statistics[kStatisticsWriteErrors] );
    statistics->setObject( kIODriveStatisticsLatentReadTime,
                           _statistics[kStatisticsLatentReadTime] );
    statistics->setObject( kIODriveStatisticsLatentWriteTime,
                           _statistics[kStatisticsLatentWriteTime] );
    statistics->setObject( kIODriveStatisticsReads,
                           _statistics[kStatisticsReads] );
    statistics->setObject( kIODriveStatisticsWrites,
                           _statistics[kStatisticsWrites] );
    statistics->setObject( kIODriveStatisticsReadRetries,
                           _statistics[kStatisticsReadRetries] );
    statistics->setObject( kIODriveStatisticsWriteRetries,
                           _statistics[kStatisticsWriteRetries] );
    statistics->setObject( kIODriveStatisticsTotalReadTime,
                           _statistics[kStatisticsTotalReadTime] );
    statistics->setObject( kIODriveStatisticsTotalWriteTime,
                           _statistics[kStatisticsTotalWriteTime] );
    
    setProperty(kIODriveStatistics, statistics);

    return true;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

bool IODrive::start(IOService * provider)
{
    //
    // This method is called once we have been attached to the provider object.
    //

    // Instruct our subclass to prepare the drive for operation.

    if (handleStart(provider) == false)  return false;

    // Initiate the poller mechanism if it is required.

    if (isMediaEjectable() && isMediaPollRequired() && !isMediaPollExpensive())
    {
        lockForArbitration();        // (disable opens/closes; a recursive lock)

        if (!isOpen() && !isInactive())
            schedulePoller();        // (schedule the poller, increments retain)

        unlockForArbitration();       // (enable opens/closes; a recursive lock)
    }

    // Register this object so it can be found via notification requests. It is
    // not being registered to have I/O Kit attempt to have drivers match on it,
    // which is the reason most other services are registered -- that's not the
    // intention of this registerService call.

    registerService();

    return true;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void IODrive::stop(IOService * provider)
{
    //
    // This method is called before we are detached from the provider object.
    //

    // Halt the poller mechanism if it is required.
        
    if (isMediaEjectable() && isMediaPollRequired() && !isMediaPollExpensive())
        unschedulePoller();                           // (unschedule the poller)

    // Instruct our subclass to stop the drive.

    handleStop(provider);

    super::stop(provider);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void IODrive::free()
{
    //
    // Free all of this object's outstanding resources.
    //

    if (_deblockerLockForRMWs)  IOLockFree(_deblockerLockForRMWs);
    if (_openClients)  _openClients->release();

    for (unsigned index = 0; index < kStatisticsCount; index++)
        if (_statistics[index])  _statistics[index]->release();

    super::free();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

bool IODrive::handleOpen(IOService *  client,
                         IOOptionBits options,
                         void *       argument)
{
    //
    // The handleOpen method grants or denies permission to access this object
    // to an interested client.  The argument is an IOStorageAccess value that
    // specifies the level of access desired -- reader or reader-writer.
    //
    // This method can be invoked to upgrade or downgrade the access level for
    // an existing client as well.  The previous access level will prevail for
    // upgrades that fail, of course.   A downgrade should never fail.  If the
    // new access level should be the same as the old for a given client, this
    // method will do nothing and return success.  In all cases, one, singular
    // close-per-client is expected for all opens-per-client received.
    //
    // We are guaranteed that no other opens or closes will be processed until
    // we make our decision, change our state, and return from this method.
    //

    IOStorageAccess access = (IOStorageAccess) (int) argument;

    assert(client);
    assert(access != kAccessNone);

    // Handle the first open on removable media in a special case.

    if (isMediaEjectable() && _openClients->getCount() == 0)
    {
        // Halt the poller if it is active and this is the drive's first open.

        if (isMediaPollRequired() && !isMediaPollExpensive())
            unschedulePoller();                       // (unschedule the poller)

        // Lock down the media while we have opens on this drive object.  The
        // arbitration lock is held during the operation -- however we assume
        // the extra length of time will not be an issue for now.

        if (lockMedia(true) != kIOReturnSuccess)
            IOLog("%s: Unable to lock down removable media.\n", getName());
    }

    // Process the open.

    _openClients->setObject(client);          // (works for up/downgrade case)

    return true;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

bool IODrive::handleIsOpen(const IOService * client) const
{
    //
    // The handleIsOpen method determines whether the specified client, or any
    // client if none is specificed, presently has an open on this object.
    //
    // We are guaranteed that no other opens or closes will be processed until
    // we return from this method.
    //

    if (client)
        return _openClients->containsObject(client);
    else
        return (_openClients->getCount() != 0);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void IODrive::handleClose(IOService * client, IOOptionBits options)
{
    //
    // The handleClose method drops the incoming client's access to this object.
    //
    // We are guaranteed that no other opens or closes will be processed until
    // we change our state and return from this method.
    //

    assert(client);

    // Process the close.

    _openClients->removeObject(client);

    // Handle the last close on removable media in a special case.

    if (isMediaEjectable() && _openClients->getCount() == 0)
    {
        // Unlock the media in the drive.   The arbitration lock is held during
        // the operation -- however we assume the extra length of time will not
        // be an issue for now.

        if (lockMedia(false) != kIOReturnSuccess)
            IOLog("%s: Unable to unlock removable media.\n", getName());

        // Reactivate the poller.

        if (isMediaPollRequired() && !isMediaPollExpensive() && !isInactive())
            schedulePoller();        // (schedule the poller, increments retain)
    }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void IODrive::read(IOService *          /* client */,
                   UInt64               byteStart,
                   IOMemoryDescriptor * buffer,
                   IOStorageCompletion  completion)
{
    //
    // Read data from the storage object at the specified byte offset into the
    // specified buffer, asynchronously.   When the read completes, the caller
    // will be notified via the specified completion action.
    //
    // The buffer will be retained for the duration of the read.
    //
    // Note that the read passes through several other methods before being
    // passed to executeRequest.  The first is deblockRequest, which aligns
    // the request with the media's block boundaries; the second is prepare-
    // Request which prepares the memory involved in the transfer (involves
    // wiring, virtual-to-physical mapping, and breakup of the memory range
    // based on the controller's and/or drive's constraints).
    //

    UInt64 mediaBlockSize = getMediaBlockSize();

    // State our assumptions.

    assert(buffer->getDirection() == kIODirectionIn);

    // If the request is aligned with the media's block boundaries, we can
    // short-circuit the deblocker and call prepareRequest directly.

    if ( (byteStart           % mediaBlockSize) == 0 &&
         (buffer->getLength() % mediaBlockSize) == 0 )
    {
        prepareRequest(byteStart, buffer, completion);
        return;
    }

    deblockRequest(byteStart, buffer, completion);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void IODrive::write(IOService *          /* client */,
                    UInt64               byteStart,
                    IOMemoryDescriptor * buffer,
                    IOStorageCompletion  completion)
{
    //
    // Write data into the storage object at the specified byte offset from the
    // specified buffer, asynchronously.   When the write completes, the caller
    // will be notified via the specified completion action.
    //
    // The buffer will be retained for the duration of the write.
    //
    // Note that the write passes through several other methods before being
    // passed to executeRequest.  The first is deblockRequest, which aligns
    // the request with the media's block boundaries; the second is prepare-
    // Request which prepares the memory involved in the transfer (involves
    // wiring, virtual-to-physical mapping, and breakup of the memory range
    // based on the controller's and/or drive's constraints).
    //

    UInt64 mediaBlockSize = getMediaBlockSize();

    // State our assumptions.

    assert(buffer->getDirection() == kIODirectionOut);

    // If the request is aligned with the media's block boundaries, we can
    // short-circuit the deblocker and call prepareRequest directly.

    if ( (byteStart           % mediaBlockSize) == 0 &&
         (buffer->getLength() % mediaBlockSize) == 0 )
    {
        prepareRequest(byteStart, buffer, completion);
        return;
    }

    deblockRequest(byteStart, buffer, completion);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void IODrive::addToBytesTransferred(UInt64 bytesTransferred,
                                    UInt64 totalTime,                    // (ns)
                                    UInt64 latentTime,                   // (ns)
                                    bool   isWrite)
{
    //
    // Update the total number of bytes transferred, the total transfer time,
    // and the total latency time for this drive -- used for statistics.
    //

    if (isWrite)
    {
        _statistics[kStatisticsWrites]->addValue(1);
        _statistics[kStatisticsBytesWritten]->addValue(bytesTransferred);
        _statistics[kStatisticsTotalWriteTime]->addValue(totalTime);
        _statistics[kStatisticsLatentWriteTime]->addValue(latentTime);
        if (bytesTransferred <= getMediaBlockSize())
            _statistics[kStatisticsSingleBlockWrites]->addValue(1);
    }
    else
    {
        _statistics[kStatisticsReads]->addValue(1);
        _statistics[kStatisticsBytesRead]->addValue(bytesTransferred);
        _statistics[kStatisticsTotalReadTime]->addValue(totalTime);
        _statistics[kStatisticsLatentReadTime]->addValue(latentTime);
        if (bytesTransferred <= getMediaBlockSize())
            _statistics[kStatisticsSingleBlockReads]->addValue(1);
    }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void IODrive::incrementRetries(bool isWrite)
{
    //
    // Update the total retry count -- used for statistics.
    //

    if (isWrite)
        _statistics[kStatisticsWriteRetries]->addValue(1);
    else
        _statistics[kStatisticsReadRetries]->addValue(1);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void IODrive::incrementErrors(bool isWrite)
{
    //
    // Update the total error count -- used for statistics.
    //

    if (isWrite)
        _statistics[kStatisticsWriteErrors]->addValue(1);
    else
        _statistics[kStatisticsReadErrors]->addValue(1);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

UInt32 IODrive::getStatistics(UInt64 * statistics,
                              UInt32   statisticsMaxCount) const
{
    //
    // Ask the drive to report its operating statistics.  The statistics are
    // described by the IODrive::Statistics indices.  This routine fills the
    // caller's buffer, up to the maximum count specified if the real number
    // of statistics would overflow the buffer.   The return value indicates
    // the actual number of statistics copied to the buffer.
    //
    // If the statistics buffer is not supplied or if the maximum count is
    // zero, the routine returns the proposed count of statistics instead.
    //

    if (statistics == 0)
        return kStatisticsCount;

    UInt32 statisticsCount = min(kStatisticsCount, statisticsMaxCount);

    for (unsigned index = 0; index < statisticsCount; index++)
        statistics[index] = _statistics[index]->unsigned64BitValue();

    return statisticsCount;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

UInt64 IODrive::getStatistic(Statistics statistic) const
{
    //
    // Ask the drive to report one of its operating statistics.
    //

    if ((UInt32) statistic >= kStatisticsCount)  return 0;

    return _statistics[statistic]->unsigned64BitValue();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void IODrive::prepareRequest(UInt64               byteStart,
                             IOMemoryDescriptor * buffer,
                             IOStorageCompletion  completion)
{
    //
    // The prepareRequest method prepares the memory involved in the transfer.
    // The memory will be wired down, physically mapped, and broken up based
    // on the controller's and drive's constraints.
    //
    // This method is part of a sequence of methods that are called for each
    // read or write request.  The first is deblockRequest, which aligns the
    // request at the media's block boundaries; the second is prepareRequest,
    // which prepares the buffer involved in the transfer;  and the third is 
    // executeRequest, which implements the actual transfer from the drive.
    //

    executeRequest(byteStart, buffer, completion);
    return;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void IODrive::schedulePoller()
{
    //
    // Schedule the poller mechanism.
    //
    // This method assumes that the arbitration lock is held.
    //

    AbsoluteTime deadline;

    retain();

    clock_interval_to_deadline(kPollerInterval, kMillisecondScale, &deadline);
    thread_call_func_delayed(poller, this, deadline);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void IODrive::unschedulePoller()
{
    //
    // Unschedule the poller mechanism.
    //
    // This method assumes that the arbitration lock is held.
    //

    if (thread_call_func_cancel(poller, this, true))  release();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void IODrive::poller(void * target, void *)
{
    //
    // This method is the timeout handler for the poller mechanism.  It polls
    // for media and reschedules another timeout if the drive is still closed.
    //

    IODrive * drive = (IODrive *) target;

    drive->pollMedia();

    drive->lockForArbitration();     // (disable opens/closes; a recursive lock)

    if (!drive->isOpen() && !drive->isInactive())
        drive->schedulePoller();     // (schedule the poller, increments retain)

    drive->unlockForArbitration();    // (enable opens/closes; a recursive lock)

    drive->release();             // (drop the retain associated with this poll)
}

// -----------------------------------------------------------------------------
// Deblocker Implementation

IODrive::DeblockerContext * IODrive::deblockerContextAllocate()
{
    //
    // Allocate a deblocker context structure.  It comes preinitialized with
    // a block-sized scratch buffer and an associated memory descriptor, but
    // is otherwise uninitialized.
    //
    // A future implementation may recycle deblocker context structures here
    // as an optimization.
    //

    DeblockerContext * deblockerContext = IONew(DeblockerContext, 1);
    UInt64             mediaBlockSize   = getMediaBlockSize();

    if (deblockerContext)
    {
        deblockerContext->blockBufferPtr = IONew(UInt8, mediaBlockSize);

        if (deblockerContext->blockBufferPtr)
        {
            deblockerContext->blockBuffer = IOMemoryDescriptor::withAddress(
                       /* address       */ deblockerContext->blockBufferPtr,
                       /* withLength    */ (vm_size_t) mediaBlockSize,
                       /* withDirection */ kIODirectionNone );

            if (deblockerContext->blockBuffer)  return deblockerContext;

            IODelete(deblockerContext->blockBufferPtr, UInt8, mediaBlockSize);
        }
        IODelete(deblockerContext, DeblockerContext, 1);
    }

    return 0; // (failure)
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void IODrive::deblockerContextFree(IODrive::DeblockerContext * deblockerContext)
{
    //
    // Deallocate a deblocker context structure.  The block-sized scratch
    // buffer and memory descriptor is automatically deallocated as well.
    //

    assert(deblockerContext->blockBuffer && deblockerContext->blockBufferPtr);

    if (deblockerContext->middleBuffer)
        deblockerContext->middleBuffer->release();

    deblockerContext->blockBuffer->release();
    IODelete(deblockerContext->blockBufferPtr, UInt8, getMediaBlockSize());
    IODelete(deblockerContext, DeblockerContext, 1);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void IODrive::deblockRequest(UInt64               byteStart,
                             IOMemoryDescriptor * buffer,
                             IOStorageCompletion  completion)
{
    //
    // The deblockRequest method checks to see if the incoming request rests
    // on the media's block boundaries, and if not, deblocks it.  Deblocking
    // involves breaking up the request into sub-requests that rest on block
    // boundaries, and performing the appropriate unaligned byte copies from
    // the scratch buffer into into the client's request buffer.
    //
    // This method is part of a sequence of methods that are called for each
    // read or write request.  The first is deblockRequest, which aligns the
    // request at the media's block boundaries; the second is prepareRequest,
    // which prepares the memory involved in the transfer;  and the third is 
    // executeRequest, which implements the actual transfer from the drive.
    //
    // The current implementation of deblockRequest is asynchronous.
    //
    // A diagram and description of the key players:
    //    ____ _______________ ______   ______ _______________ ____
    // ...    |               |      ...      |               |    ...
    //    ____|_______________|______   ______|_______________|____
    //              ^                                 ^
    //        |\___/|\_______/|\_____________/|\_____/|
    //        |  |  |    |    |       |       |   |   |
    //        |  |  |    |    |       |       |   |   |_  byteStart + byteCount
    //        |  |  |    |    |       |       |   |_ _ _  bytesFinal
    //        |  |  |    |    |       |       |_ _ _ _ _  BLOCK BOUNDARY
    //        |  |  |    |    |       |
    //        |  |  |    |    |       |_ _ _ _ _ _ _ _ _  bytesMiddle
    //        |  |  |    |    |_ _ _ _ _ _ _ _ _ _ _ _ _  BLOCK BOUNDARY
    //        |  |  |    |
    //        |  |  |    |__ _ _ _ _ _ _ _ _ _ _ _ _ _ _  bytesStart
    //        |  |  |_ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _  byteStart
    //        |  |__ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _  offsetStart
    //        |_ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _  BLOCK BOUNDARY
    // 
    // byteStart   = the start of transfer, absolute position on media
    // offsetStart = unaligned offset into the first block, zero if aligned
    // bytesStart  = unaligned bytecount for the first block, zero if aligned
    // bytesMiddle = aligned byte count for the middle blocks, zero if none
    // bytesFinal  = unaligned byte count for the last block, zero if aligned
    //

    UInt64             byteCount = buffer->getLength();
    DeblockerContext * context   = deblockerContextAllocate();
    bool               isWrite   = (buffer->getDirection() == kIODirectionOut);
    UInt64             mediaBlockSize = getMediaBlockSize();

    if (context == 0)
    {
        complete(completion, kIOReturnNoMemory);
        return;
    }

    //
    // This implementation of the deblocker permits only one read-modify-write
    // at any given time.  Note that other write requests can, and do, proceed
    // simultaneously so long as they do not require the deblocker -- refer to
    // the read() and the write() routines for the decision logic.
    //

    if (isWrite)  IOTakeLock(_deblockerLockForRMWs);

    //
    // We split up our calculation logic into two distinct cases:
    //  (a) all one block transfers with an unaligned end;
    //  (b) all one block transfers with an aligned end and > 2 block transfers
    //

    context->offsetStart = byteStart % mediaBlockSize;

    if (context->offsetStart + byteCount < mediaBlockSize) // (not '<=')
    {
       context->bytesStart = byteCount;
       context->bytesFinal = 0;
       context->bytesMiddle = 0;
    }
    else // (case b)
    {
       context->bytesStart  = (context->offsetStart) ? 
                              (mediaBlockSize - context->offsetStart) : 0;
       context->bytesFinal  = (context->offsetStart + byteCount) %
                              mediaBlockSize;
       context->bytesMiddle = byteCount - context->bytesStart -
                              context->bytesFinal;
    }

    assert(context->bytesMiddle % mediaBlockSize == 0);

    //
    // We set the deblock phase to "begin" and pass control to an internal
    // completion routine, which implements the deblocking state machine.
    //

    context->phase            = (isWrite) ? kPhaseBeginRMW : kPhaseBegin;
    context->bytesTransferred = 0;

    context->originalRequest.byteStart  = byteStart;
    context->originalRequest.buffer     = buffer;
    context->originalRequest.buffer->retain();   // (retain the original buffer)
    context->originalRequest.completion = completion;

    if (context->bytesMiddle)                               // (very bad things)
    {
        context->middleBuffer = IOMemoryDescriptor::withSubRange(
          /* descriptor    */ context->originalRequest.buffer,
          /* withOffset    */ context->bytesStart,
          /* withLength    */ context->bytesMiddle,
          /* withDirection */ context->originalRequest.buffer->getDirection() );
        assert(context->middleBuffer);
    }
    else
    {
        context->middleBuffer = 0;
    }

    context->subrequest.buffer               = 0;
    context->subrequest.byteStart            = 0;
    context->subrequest.completion.target    = this;
    context->subrequest.completion.action    = deblockerCompletion;
    context->subrequest.completion.parameter = context;

    deblockerCompletion(this, context, kIOReturnSuccess, 0);
    return;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void IODrive::deblockerCompletion(void *   target,
                                  void *   parameter,
                                  IOReturn status,
                                  UInt64   actualByteCount)
{
    //
    // This is the completion routine for the aligned deblocker subrequests.
    // It performs work on the just-completed phase, if any, transitions to
    // the next phase, then builds and issues a transfer for the next phase.
    //
    //
    // For write storage requests, the state machine proceeds as follows:
    //
    // kPhaseBeginRMW --> { kPhaseStartRM --> kPhaseStartW } -->
    // { kPhaseMiddleW } --> { kPhaseFinalRM --> kPhaseFinalW } --> kPhaseDone
    //
    // For read storage requests, the state machine proceeds as follows:
    //
    // kPhaseBegin --> { kPhaseStart } --> { kPhaseMiddle } --> { kPhaseFinal }
    // --> kPhaseDone
    //
    // { } denotes skippable states
    //

    DeblockerContext * context        = (DeblockerContext *) parameter;
    IODirection        direction      = kIODirectionIn;
    IODrive *          drive          = (IODrive *) target;
    UInt64             mediaBlockSize = drive->getMediaBlockSize();

    //
    // Complete the work necessary for the just-completed transfer, if any.
    // Note that we go out of our way to squeeze every good byte through to
    // the disk, even if the transfer failed half way through.
    //
    // A note for future developers: be mindful that we haven't checked the
    // returned actual byte count and status yet. 
    //

    switch (context->phase)
    {
        case kPhaseStart: // - - - - - - - - - - - - - - - - - - - - - - - - - -
        {
            UInt64 bytesToCopy;

            // Ensure a sufficient amount of the block was read.

            if (actualByteCount > context->offsetStart)
            {
                // Copy the slice of data to be read from the scratch buffer,
                // or the subset thereof if the actual transfer came up short.

                bytesToCopy = min(context->bytesStart,
                                  actualByteCount - context->offsetStart);

                bytesToCopy = context->originalRequest.buffer->writeBytes(
                                context->bytesTransferred,
                                context->blockBufferPtr + context->offsetStart,
                                (UInt32)bytesToCopy);

                // Bring the total number of bytes transferred up to date.

                context->bytesTransferred += bytesToCopy;

                // If the actual transfer came up short (due to writeBytes),
                // set an error status.

                if (bytesToCopy < context->bytesStart &&
                    status == kIOReturnSuccess)
                {
                    status = kIOReturnNoSpace; // (writeBytes failure)
                }
            }
        } break;

        case kPhaseMiddle:  // - - - - - - - - - - - - - - - - - - - - - - - - -
        {
            // Bring the total number of bytes transferred up to date.

            context->bytesTransferred += actualByteCount;
        } break;

        case kPhaseFinal: // - - - - - - - - - - - - - - - - - - - - - - - - - -
        {
            UInt64 bytesToCopy = min(context->bytesFinal, actualByteCount);

            // Ensure at least some amount of the block was read.

            if (bytesToCopy)
            {
                // Copy the slice of data to be read from the scratch buffer,
                // or the subset thereof if the actual transfer came up short.

                bytesToCopy = context->originalRequest.buffer->writeBytes(
                            context->bytesTransferred,
                            context->blockBufferPtr,
                            (UInt32)bytesToCopy);

                // Bring the total number of bytes transferred up to date.

                context->bytesTransferred += bytesToCopy;

                // If the actual transfer came up short (due to writeBytes),
                // set an error status.

                if (bytesToCopy < context->bytesFinal &&
                    status == kIOReturnSuccess)
                {
                    status = kIOReturnNoSpace; // (writeBytes failure)
                }
            }
        } break;

        case kPhaseStartRM: // - - - - - - - - - - - - - - - - - - - - - - - - -
        {
            // Ensure the entire block was read into the scratch buffer.

            if (actualByteCount == mediaBlockSize)
            {
                // Copy the slice of data to be written into the scratch buffer.

                if (context->originalRequest.buffer->readBytes(
                    context->bytesTransferred,
                    context->blockBufferPtr + context->offsetStart,
                    (UInt32)context->bytesStart) != (UInt32)context->bytesStart)
                {
                    status = kIOReturnInternalError; // (readBytes failure)
                }
            }
        } break;
        
        case kPhaseStartW:  // - - - - - - - - - - - - - - - - - - - - - - - - -
        {
            // Ensure the entire block was written and bring total number of
            // bytes transferred up to date.

            if (actualByteCount == mediaBlockSize)
            {
                // Bring the total number of bytes transferred up to date.
                context->bytesTransferred += context->bytesStart;
            }
        } break;

        case kPhaseMiddleW: // - - - - - - - - - - - - - - - - - - - - - - - - -
        {
            // Bring the total number of bytes transferred up to date.
            context->bytesTransferred += actualByteCount;
        } break;

        case kPhaseFinalRM: // - - - - - - - - - - - - - - - - - - - - - - - - -
        {
            // Ensure the entire block was read into the scratch buffer.

            if (actualByteCount == mediaBlockSize)
            {
                // Copy the slice of data to be written into the scratch buffer.

                if (context->originalRequest.buffer->readBytes(
                    context->bytesTransferred,
                    context->blockBufferPtr,
                    (UInt32)context->bytesFinal) != (UInt32)context->bytesFinal)
                {
                    status = kIOReturnInternalError; // (readBytes failure)
                }
            }
        } break;

        case kPhaseFinalW:  // - - - - - - - - - - - - - - - - - - - - - - - - -
        {
            // Ensure the entire block was written and bring total number of
            // bytes transferred up to date.

            if (actualByteCount == mediaBlockSize)
            {
                // Bring the total number of bytes transferred up to date.
                context->bytesTransferred += context->bytesFinal;
            }
        } break;

        default:
        {
            // Nothing to do.
        } break;
    }

    //
    // Fail the original transfer if an error was detected.
    //

    if (context->phase != kPhaseBegin && context->phase != kPhaseBeginRMW)
    {
        assert( status          != kIOReturnSuccess                        ||
                actualByteCount == context->subrequest.buffer->getLength() );

        if ( status          != kIOReturnSuccess                        || 
             actualByteCount != context->subrequest.buffer->getLength() )
        {
            // Unlock the RMW-lock, to allow the next RMW to proceed.
            if (context->originalRequest.buffer->getDirection() ==
                                                                kIODirectionOut)
                IOUnlock(drive->_deblockerLockForRMWs);

            // Release the retain we placed on the request buffer.
            context->originalRequest.buffer->release();

            // Complete the request.
            drive->complete(context->originalRequest.completion,
                            status,
                            context->bytesTransferred);

            // Release the deblocker context structure.
            drive->deblockerContextFree(context);
            return;
        }
    }

    //
    // Transistion to the next phase of the unaligned transfer.
    //

    switch (context->phase)
    {
        case kPhaseBegin:
            if (context->bytesStart)  { context->phase = kPhaseStart;  break; }
        case kPhaseStart:
            if (context->bytesMiddle) { context->phase = kPhaseMiddle; break; }
        case kPhaseMiddle:
            if (context->bytesFinal)  { context->phase = kPhaseFinal;  break; }
        case kPhaseFinal:
            context->phase = kPhaseDone;
            break;

        case kPhaseBeginRMW:
            if (context->bytesStart)  { context->phase = kPhaseStartRM; break; }
        case kPhaseStartW:
            if (context->bytesMiddle)
            {
                context->phase = kPhaseMiddleW;
                direction = kIODirectionOut;
                break;
            }
        case kPhaseMiddleW:
            if (context->bytesFinal)  { context->phase = kPhaseFinalRM; break; }
        case kPhaseFinalW:
            context->phase = kPhaseDone;
            break;

        case kPhaseStartRM:
            context->phase = kPhaseStartW;
            direction = kIODirectionOut;
            break;
        case kPhaseFinalRM:
            context->phase = kPhaseFinalW;
            direction = kIODirectionOut;
            break;

        default:
            assert(0);
            break;
    }

    //
    // Build and execute the transfer for the new phase.
    //

    switch (context->phase)
    {
        case kPhaseStart: // - - - - - - - - - - - - - - - - - - - - - - - - - -
        case kPhaseStartRM:
        case kPhaseStartW:
            context->subrequest.byteStart = context->originalRequest.byteStart -
                                            context->offsetStart;
            context->subrequest.buffer    = context->blockBuffer;

            context->subrequest.buffer->initWithAddress(
                                    /* address       */ context->blockBufferPtr,
                                    /* withLength    */ mediaBlockSize,
                                    /* withDirection */ direction );
            break;

        case kPhaseMiddle:  // - - - - - - - - - - - - - - - - - - - - - - - - -
        case kPhaseMiddleW:
            context->subrequest.byteStart = context->originalRequest.byteStart +
                                            context->bytesStart;
            context->subrequest.buffer    = context->middleBuffer;

            assert(context->middleBuffer);                           // (to fix)
            break;

        case kPhaseFinal: // - - - - - - - - - - - - - - - - - - - - - - - - - -
        case kPhaseFinalRM:
        case kPhaseFinalW:
            context->subrequest.byteStart = context->originalRequest.byteStart +
                                            context->bytesStart +
                                            context->bytesMiddle;
            context->subrequest.buffer    = context->blockBuffer;

            context->subrequest.buffer->initWithAddress(
                                    /* address       */ context->blockBufferPtr,
                                    /* withLength    */ mediaBlockSize,
                                    /* withDirection */ direction );
            break;

        case kPhaseDone:  // - - - - - - - - - - - - - - - - - - - - - - - - - -

            // Unlock the RMW-lock, to allow the next RMW to proceed.
            if (context->originalRequest.buffer->getDirection() ==
                                                                kIODirectionOut)
                IOUnlock(drive->_deblockerLockForRMWs);

            // Release the retain we placed on the request buffer.
            context->originalRequest.buffer->release();

            // Complete the request.
            drive->complete(context->originalRequest.completion,
                            kIOReturnSuccess,
                            context->bytesTransferred);

            // Release the deblocker context structure.
            drive->deblockerContextFree(context);
            return;

        default:
            assert(0);
            break;
    }

    // (presuming completion.target/action/parameter are unchanged)

    drive->prepareRequest(context->subrequest.byteStart,
                          context->subrequest.buffer,
                          context->subrequest.completion);
    return;
}
