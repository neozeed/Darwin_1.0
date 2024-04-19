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

/*!
 * @header IODrive
 * @abstract
 * This header contains the IODrive class definition.
 */

#ifndef _IODRIVE_H
#define _IODRIVE_H

/*!
 * @defined kIODriveStatistics
 * @abstract
 * kIODriveStatistics is a property of IODrive objects.  It is an OSDictionary.
 * @discussion
 * The kIODirectionStatistics property contains a table of statistics describing
 * drive operations.  All the different statistics are grouped under this table.
 */
 
#define kIODriveStatistics "Statistics"

/*!
 * @defined kIODriveStatisticsBytesRead
 * @abstract
 * kIODriveStatisticsBytesRead is one of the statistic entries listed
 * under the kIODriveStatistics property of IODrive objects.  It is an OSNumber.
 * @discussion
 * The kIODriveStatisticsBytesRead property describes the number of
 * bytes read since the drive object was instantiated.
 */

#define kIODriveStatisticsBytesRead "Bytes (Read)"

/*!
 * @defined kIODriveStatisticsBytesWritten
 * @abstract
 * kIODriveStatisticsBytesWritten is one of the statistic entries listed
 * under the kIODriveStatistics property of IODrive objects.  It is an OSNumber.
 * @discussion
 * The kIODriveStatisticsBytesWritten property describes the number of
 * bytes written since the drive object was instantiated.
 */

#define kIODriveStatisticsBytesWritten "Bytes (Write)"

/*!
 * @defined kIODriveStatisticsReadErrors
 * @abstract
 * kIODriveStatisticsReadErrors is one of the statistic entries listed
 * under the kIODriveStatistics property of IODrive objects.  It is an OSNumber.
 * @discussion
 * The kIODriveStatisticsReadErrors property describes the number of
 * read errors encountered since the drive object was instantiated.
 */

#define kIODriveStatisticsReadErrors "Errors (Read)"

/*!
 * @defined kIODriveStatisticsWriteErrors
 * @abstract
 * kIODriveStatisticsWriteErrors is one of the statistic entries listed
 * under the kIODriveStatistics property of IODrive objects.  It is an OSNumber.
 * @discussion
 * The kIODriveStatisticsWriteErrors property describes the number of
 * write errors encountered since the drive object was instantiated.
 */

#define kIODriveStatisticsWriteErrors "Errors (Write)"

/*!
 * @defined kIODriveStatisticsLatentReadTime
 * @abstract
 * kIODriveStatisticsLatentReadTime is one of the statistic entries listed
 * under the kIODriveStatistics property of IODrive objects.  It is an OSNumber.
 * @discussion
 * The kIODriveStatisticsLatentReadTime property describes the number of
 * nanoseconds of latency during reads since the drive object was instantiated.
 */

#define kIODriveStatisticsLatentReadTime "Latency Time (Read)"

/*!
 * @defined kIODriveStatisticsLatentWriteTime
 * @abstract
 * kIODriveStatisticsLatentWriteTime is one of the statistic entries listed
 * under the kIODriveStatistics property of IODrive objects.  It is an OSNumber.
 * @discussion
 * The kIODriveStatisticsLatentWriteTime property describes the number of
 * nanoseconds of latency during writes since the drive object was instantiated.
 */

#define kIODriveStatisticsLatentWriteTime "Latency Time (Write)"

/*!
 * @defined kIODriveStatisticsReads
 * @abstract
 * kIODriveStatisticsReads is one of the statistic entries listed
 * under the kIODriveStatistics property of IODrive objects.  It is an OSNumber.
 * @discussion
 * The kIODriveStatisticsReads property describes the number of
 * read operations processed since the drive object was instantiated.
 */

#define kIODriveStatisticsReads "Operations (Read)"

/*!
 * @defined kIODriveStatisticsWrites
 * @abstract
 * kIODriveStatisticsWrites is one of the statistic entries listed
 * under the kIODriveStatistics property of IODrive objects.  It is an OSNumber.
 * @discussion
 * The kIODriveStatisticsWrites property describes the number of
 * write operations processed since the drive object was instantiated.
 */

#define kIODriveStatisticsWrites "Operations (Write)"

/*!
 * @defined kIODriveStatisticsReadRetries
 * @abstract
 * kIODriveStatisticsReadRetries is one of the statistic entries listed
 * under the kIODriveStatistics property of IODrive objects.  It is an OSNumber.
 * @discussion
 * The kIODriveStatisticsReadRetries property describes the number of
 * read retries required since the drive object was instantiated.
 */

#define kIODriveStatisticsReadRetries "Retries (Read)"

/*!
 * @defined kIODriveStatisticsWriteRetries
 * @abstract
 * kIODriveStatisticsWriteRetries is one of the statistic entries listed
 * under the kIODriveStatistics property of IODrive objects.  It is an OSNumber.
 * @discussion
 * The kIODriveStatisticsWriteRetries property describes the number of
 * write retries required since the drive object was instantiated.
 */

#define kIODriveStatisticsWriteRetries "Retries (Write)"

/*!
 * @defined kIODriveStatisticsTotalReadTime
 * @abstract
 * kIODriveStatisticsTotalReadTime is one of the statistic entries listed
 * under the kIODriveStatistics property of IODrive objects.  It is an OSNumber.
 * @discussion
 * The kIODriveStatisticsTotalReadTime property describes the number of
 * nanoseconds spent performing reads since the drive object was instantiated.
 */

#define kIODriveStatisticsTotalReadTime "Total Time (Read)"

/*!
 * @defined kIODriveStatisticsTotalWriteTime
 * @abstract
 * kIODriveStatisticsTotalWriteTime is one of the statistic entries listed
 * under the kIODriveStatistics property of IODrive objects.  It is an OSNumber.
 * @discussion
 * The kIODriveStatisticsTotalWriteTime property describes the number of
 * nanoseconds spent performing writes since the drive object was instantiated.
 */

#define kIODriveStatisticsTotalWriteTime "Total Time (Write)"

/*
 * Kernel
 */

#if defined(KERNEL) && defined(__cplusplus)

#include <IOKit/storage/IOStorage.h>

/*!
 * @class IODrive
 * @abstract
 * The IODrive class is the common base class for all disk drive objects.
 * @discussion
 * The IODrive class is the common base class for all disk drive objects.  It
 * extends the IOStorage class by implementing the appropriate open and close
 * semantics for drive objects, deblocking for unaligned transfers, automatic
 * polling for removable media, statistics gathering and reporting functions,
 * and declaring the abstract APIs for media formatting, ejection and locking
 * (removal prevention).
 *
 * There are more specific subclasses of IODrive which further implement the
 * general semantics of hard disk drives (IOHDDrive), CD drives (IOCDDrive),
 * and so on.
 */

class IODrive : public IOStorage
{
    OSDeclareAbstractStructors(IODrive);

public:

    /*!
     * @enum Statistics
     * @discussion
     * Indices for the different statistics that getStatistics() can report.
     * @constant kStatisticsReads
     * Number of read operations thus far.
     * @constant kStatisticsSingleBlockReads
     * Number of read operations for a single block thus far.
     * @constant kStatisticsBytesRead
     * Number of bytes read thus far.
     * @constant kStatisticsTotalReadTime
     * Nanoseconds spent performing reads thus far.
     * @constant kStatisticsLatentReadTime
     * Nanoseconds of latency during reads thus far.
     * @constant kStatisticsReadRetries
     * Number of read retries thus far.
     * @constant kStatisticsReadErrors
     * Number of read errors thus far.
     * @constant kStatisticsWrites
     * Number of write operations thus far.
     * @constant kStatisticsSingleBlockWrites
     * Number of write operations for a single block thus far.
     * @constant kStatisticsBytesWritten
     * Number of bytes written thus far.
     * @constant kStatisticsTotalWriteTime
     * Nanoseconds spent performing writes thus far.
     * @constant kStatisticsLatentWriteTime
     * Nanoseconds of latency during writes thus far.
     * @constant kStatisticsWriteRetries
     * Number of write retries thus far.
     * @constant kStatisticsWriteErrors
     * Number of write errors thus far.
     */

    enum Statistics
    {
        kStatisticsReads,
        kStatisticsSingleBlockReads,
        kStatisticsBytesRead,
        kStatisticsTotalReadTime,
        kStatisticsLatentReadTime,
        kStatisticsReadRetries,
        kStatisticsReadErrors,

        kStatisticsWrites,
        kStatisticsSingleBlockWrites,
        kStatisticsBytesWritten,
        kStatisticsTotalWriteTime,
        kStatisticsLatentWriteTime,
        kStatisticsWriteRetries,
        kStatisticsWriteErrors
    };

    static const UInt32 kStatisticsCount = kStatisticsWriteErrors + 1;

    /*!
     * @enum IOMediaState
     * @discussion
     * The different states that getMediaState() can report.
     * @constant kMediaOffline
     * Media is not available.
     * @constant kMediaOnline
     * Media is available and ready for operations.
     * @constant kMediaBusy
     * Media is available, but not ready for operations.
     */

    enum IOMediaState
    {
        kMediaOffline,
        kMediaOnline,
        kMediaBusy
    };

protected:

    OSSet *         _openClients;
    OSNumber *      _statistics[kStatisticsCount];

    /*
     * Free all of this object's outstanding resources.
     *
     * This method's implementation is not typically overidden.
     */

    void free();

    /*!
     * @function handleOpen
     * @discussion
     * The handleOpen method grants or denies permission to access this object
     * to an interested client.  The argument is an IOStorageAccess value that
     * specifies the level of access desired -- reader or reader-writer.
     *
     * This method can be invoked to upgrade or downgrade the access level for
     * an existing client as well.  The previous access level will prevail for
     * upgrades that fail, of course.   A downgrade should never fail.  If the
     * new access level should be the same as the old for a given client, this
     * method will do nothing and return success.  In all cases, one, singular
     * close-per-client is expected for all opens-per-client received.
     *
     * This implementation replaces the IOService definition of handleIsOpen().
     * @param client
     * Client requesting the open.
     * @param options
     * Options for the open.  Set to zero.
     * @param access
     * Access level for the open.  Set to kAccessReader or kAccessReaderWriter.
     * @result
     * Returns true if the open was successful, false otherwise.
     */

    virtual bool handleOpen(IOService *  client,
                            IOOptionBits options,
                            void *       access);

    /*!
     * @function handleIsOpen
     * @discussion
     * The handleIsOpen method determines whether the specified client, or any
     * client if none is specificed, presently has an open on this object.
     *
     * This implementation replaces the IOService definition of handleIsOpen().
     * @param client
     * Client to check the open state of.  Set to zero to check the open state
     * of all clients.
     * @result
     * Returns true if the client was (or clients were) open, false otherwise.
     */

    virtual bool handleIsOpen(const IOService * client) const;

    /*!
     * @function handleClose
     * @discussion
     * The handleClose method closes the client's access to this object.
     *
     * This implementation replaces the IOService definition of handleIsOpen().
     * @param client
     * Client requesting the close.
     * @param options
     * Options for the close.  Set to zero.
     */

    virtual void handleClose(IOService * client, IOOptionBits options);

    /*!
     * @function addToBytesTransferred
     * @discussion
     * Update the total number of bytes transferred, the total transfer time,
     * and the total latency time for this drive -- used for statistics.
     *
     * This method's implementation is not typically overidden.
     * @param bytesTransferred
     * Number of bytes transferred in this operation.
     * @param totalTime
     * Nanoseconds spent performing this operation.
     * @param latentTime
     * Nanoseconds of latency during this operation.
     * @param isWrite
     * Indicates whether this operation was a write, otherwise is was a read.
     */

    virtual void addToBytesTransferred(UInt64 bytesTransferred,
                                       UInt64 totalTime,
                                       UInt64 latentTime,
                                       bool   isWrite);

    /*!
     * @function incrementErrors
     * @discussion
     * Update the total error count -- used for statistics.
     *
     * This method's implementation is not typically overidden.
     * @param isWrite
     * Indicates whether this operation was a write, otherwise is was a read.
     */

    virtual void incrementErrors(bool isWrite);

    /*!
     * @function incrementRetries
     * @discussion
     * Update the total retry count -- used for statistics.
     *
     * This method's implementation is not typically overidden.
     * @param isWrite
     * Indicates whether this operation was a write, otherwise is was a read.
     */

    virtual void incrementRetries(bool isWrite);

    /*!
     * @function deblockRequest
     * @discussion
     * The deblockRequest method checks to see if the incoming request rests
     * on the media's block boundaries, and if not, deblocks it.  Deblocking
     * involves breaking up the request into sub-requests that rest on block
     * boundaries, and performing the appropriate unaligned byte copies from
     * the scratch buffer into into the client's request buffer.
     *
     * This method is part of a sequence of methods that are called for each
     * read or write request.  The first is deblockRequest, which aligns the
     * request at the media's block boundaries; the second is prepareRequest,
     * which prepares the memory involved in the transfer;  and the third is 
     * executeRequest, which implements the actual transfer from the drive.
     *
     * This method's implementation is not typically overidden.
     * @param byteStart
     * Starting byte offset for the data transfer.
     * @param buffer
     * Buffer for the data transfer.  The size of the buffer implies the size of
     * the data transfer.
     * @param completion
     * Completion routine to call once the data transfer is complete.
     */

    virtual void deblockRequest(UInt64               byteStart,
                                IOMemoryDescriptor * buffer,
                                IOStorageCompletion  completion);

    /*!
     * @function prepareRequest
     * @discussion
     * The prepareRequest method prepares the memory involved in the transfer.
     * The memory will be wired down, physically mapped, and broken up based
     * on the controller's and drive's constraints.
     *
     * This method is part of a sequence of methods that are called for each
     * read or write request.  The first is deblockRequest, which aligns the
     * request at the media's block boundaries; the second is prepareRequest,
     * which prepares the buffer involved in the transfer;  and the third is 
     * executeRequest, which implements the actual transfer from the drive.
     *
     * This method's implementation is not typically overidden.
     * @param byteStart
     * Starting byte offset for the data transfer.
     * @param buffer
     * Buffer for the data transfer.  The size of the buffer implies the size of
     * the data transfer.
     * @param completion
     * Completion routine to call once the data transfer is complete.
     */

    virtual void prepareRequest(UInt64               byteStart,
                                IOMemoryDescriptor * buffer,
                                IOStorageCompletion  completion);

    /*!
     * @function executeRequest
     * @discussion
     * Execute the specified storage request.  The request is guaranteed to
     * be block-aligned, and to have a memory buffer that is wired, mapped,
     * and is constrained according to the controller's and drive's limits.
     *
     * This method is part of a sequence of methods that are called for each
     * read or write request.  The first is deblockRequest, which aligns the
     * request at the media's block boundaries; the second is prepareRequest,
     * which prepares the buffer involved in the transfer;  and the third is 
     * executeRequest, which implements the actual transfer from the drive.
     *
     * When implementing this abstract method, it is important to remember
     * that the buffer must be retained while the asynchronous operation is
     * being processed, as per regular I/O Kit object-passing semantics.
     * @param byteStart
     * Starting byte offset for the data transfer.
     * @param buffer
     * Buffer for the data transfer.  The size of the buffer implies the size of
     * the data transfer.
     * @param completion
     * Completion routine to call once the data transfer is complete.
     */

    virtual void executeRequest(UInt64               byteStart,
                                IOMemoryDescriptor * buffer,
                                IOStorageCompletion  completion) = 0;

    /*!
     * @function handleStart
     * @discussion
     * Prepare the drive for operation.
     *
     * This is where a media object needs to be created for fixed media, and
     * optionally for removable media.  For removable media, the poller will
     * issue handlePoll() once a second, so the creation of the media object
     * could be delayed until then if you wish.
     *
     * Note that this method is called from the superclass' start() routine;
     * if this method returns successfully,  it should be prepared to accept
     * any of IODrive's abstract methods.
     * @param provider
     * This object's provider.
     * @result
     * Returns true on success, false otherwise.
     */

    virtual bool handleStart(IOService * provider) = 0;

    /*!
     * @function handleStop
     * @discussion
     * Stop the drive.
     *
     * This is where the driver should clean up its state in preparation for
     * removal from the system.
     *
     * Note that this method is called from the superclass' stop() routine.
     * @param provider
     * This object's provider.
     */

    virtual void handleStop(IOService * provider) = 0;

    /*!
     * @function getMediaBlockSize
     * @discussion
     * Ask the drive about the media's actual block size.
     * @result
     * Natural block size, in bytes.
     */

    virtual inline UInt64 getMediaBlockSize() const = 0;

public:

///m:2333367:workaround:commented:start
//  IOStorage::open;
//  IOStorage::close;
//  IOStorage::read;
//  IOStorage::write;
///m:2333367:workaround:commented:stop

    /*
     * Initialize this object's minimal state.
     *
     * This method's implementation is not typically overidden.
     */

    virtual bool init(OSDictionary * properties = 0);

    /*
     * This method is called once we have been attached to the provider object.
     *
     * This method's implementation is not typically overidden.
     */

    virtual bool start(IOService * provider);

    /*
     * This method is called before we are detached from the provider object.
     *
     * This method's implementation is not typically overidden.
     */

    virtual void stop(IOService * provider);

    /*!
     * @function read
     * @discussion
     * The read method is the receiving end for all read requests from the
     * storage framework, that is, the child storage objects of this drive.
     *
     * This method kicks off a sequence of methods which are called for each
     * read or write request.  The first is deblockRequest, which aligns the
     * request at the media's block boundaries; the second is prepareRequest,
     * which prepares the buffer involved in the transfer;  and the third is 
     * executeRequest, which implements the actual transfer from the drive.
     *
     * This method's implementation is not typically overidden.
     * @param client
     * Client requesting the read.
     * @param byteStart
     * Starting byte offset for the data transfer.
     * @param buffer
     * Buffer for the data transfer.  The size of the buffer implies the size of
     * the data transfer.
     * @param completion
     * Completion routine to call once the data transfer is complete.
     */

    virtual void read(IOService *          client,
                      UInt64               byteStart,
                      IOMemoryDescriptor * buffer,
                      IOStorageCompletion  completion);

    /*!
     * @function write
     * @discussion
     * The write method is the receiving end for all write requests from the
     * storage framework, that is, the child storage objects of this drive.
     *
     * This method kicks off a sequence of methods which are called for each
     * read or write request.  The first is deblockRequest, which aligns the
     * request at the media's block boundaries; the second is prepareRequest,
     * which prepares the buffer involved in the transfer;  and the third is 
     * executeRequest, which implements the actual transfer from the drive.
     *
     * This method's implementation is not typically overidden.
     * @param client
     * Client requesting the write.
     * @param byteStart
     * Starting byte offset for the data transfer.
     * @param buffer
     * Buffer for the data transfer.  The size of the buffer implies the size of
     * the data transfer.
     * @param completion
     * Completion routine to call once the data transfer is complete.
     */

    virtual void write(IOService *          client,
                       UInt64               byteStart,
                       IOMemoryDescriptor * buffer,
                       IOStorageCompletion  completion);

    /*!
     * @function ejectMedia
     * @discussion
     * Eject the media from the drive. The drive object is responsible for
     * tearing down the child storage objects it created before proceeding
     * with the eject.  If the teardown fails, an error should be returned.
     * @result
     * An IOReturn code.
     */

    virtual IOReturn ejectMedia() = 0;

    /*!
     * @function formatMedia
     * @discussion
     * Format the media in the drive with the specified byte capacity.    The
     * drive object is responsible for tearing down the child storage objects
     * and recreating them, if applicable.
     * @param byteCapacity
     * Number of bytes to format media to.
     * @result
     * An IOReturn code.
     */

    virtual IOReturn formatMedia(UInt64 byteCapacity) = 0;

    /*!
     * @function lockMedia
     * @discussion
     * Lock or unlock the ejectable media in the drive, that is, prevent
     * it from manual ejection or allow its manual ejection.
     * @param lock
     * Pass true to lock the media, otherwise pass false to unlock the media.
     * @result
     * An IOReturn code.
     */

    virtual IOReturn lockMedia(bool lock) = 0;

    /*!
     * @function pollMedia
     * @discussion
     * Poll for the presence of media in the drive.  The drive object is
     * responsible for tearing down the child storage objects it created
     * should the media have been removed since the last poll, and vice-
     * versa,   creating the child storage objects should new media have
     * arrived since the last poll.
     * @result
     * An IOReturn code.
     */

    virtual IOReturn pollMedia() = 0;

    /*!
     * @function isMediaEjectable
     * @discussion
     * Ask the drive whether it contains ejectable media.
     * @result
     * Returns true if the media is ejectable, false otherwise.
     */

    virtual bool isMediaEjectable() const = 0;

    /*!
     * @function isMediaPollExpensive
     * @discussion
     * Ask the drive whether it a pollMedia would be an expensive operation,
     * that is, one that requires the drive to spin up or delay for a while.
     * @result
     * Returns true if polling the media is expensive, false otherwise.
     */

    virtual bool isMediaPollExpensive() const = 0;

    /*!
     * @function isMediaPollRequired
     * @discussion
     * Ask the drive whether it requires polling, which is typically required
     * for drives with ejectable media without the ability to asynchronously
     * detect the arrival or departure of the media.
     * @result
     * Returns true if polling the media is required, false otherwise.
     */

    virtual bool isMediaPollRequired() const = 0;

    /*!
     * @function getMediaState
     * @discussion
     * Ask the drive about the media's current state.
     * @result
     * An IOMediaState value.
     */

    virtual IOMediaState getMediaState() const = 0;

    /*!
     * @function getFormatCapacities
     * @discussion
     * Ask the drive to report the feasible formatting capacities for the
     * inserted media (in bytes).  This routine fills the caller's buffer,
     * up to the maximum count specified if the real number of capacities
     * would overflow the buffer.   The return value indicates the actual
     * number of capacities copied to the buffer.
     *
     * If the capacities buffer is not supplied or if the maximum count is
     * zero, the routine returns the proposed count of capacities instead.
     * @param capacities
     * Buffer that will receive the UInt64 capacity values.
     * @param capacitiesMaxCount
     * Maximum number of capacity values that can be held in the buffer.
     * @result
     * Actual number of capacity values copied to the buffer, or if no buffer
     * is given, the total number of capacity values available.
     */

    virtual UInt32 getFormatCapacities(UInt64 * capacities,
                                       UInt32   capacitiesMaxCount) const = 0;

    /*!
     * @function getStatistics
     * @discussion
     * Ask the drive to report its operating statistics.  The statistics are
     * described by the IODrive::Statistics indices.  This routine fills the
     * caller's buffer, up to the maximum count specified if the real number
     * of statistics would overflow the buffer.   The return value indicates
     * the actual number of statistics copied to the buffer.
     *
     * If the statistics buffer is not supplied or if the maximum count is
     * zero, the routine returns the proposed count of statistics instead.
     * @param statistics
     * Buffer that will receive the UInt64 statistic values.
     * @param statisticsMaxCount
     * Maximum number of statistic values that can be held in the buffer.
     * @result
     * Actual number of statistic values copied to the buffer, or if no buffer
     * is given, the total number of statistic values available.
     */

    virtual UInt32 getStatistics(UInt64 * statistics,
                                 UInt32   statisticsMaxCount) const;

    /*!
     * @function getStatistic
     * @discussion
     * Ask the drive to report one of its operating statistics.
     * @param statistic
     * Statistic index (an IODrive::Statistics index).
     * @result
     * Statistic value.
     */

    virtual UInt64 getStatistic(Statistics statistic) const;

protected:

    /*
     * Definitions for the default deblocker implementation.
     */

    enum DeblockerPhase
    {
        kPhaseBegin,
        kPhaseStart,
        kPhaseMiddle,
        kPhaseFinal,
        kPhaseDone,

        kPhaseBeginRMW,
        kPhaseStartRM,
        kPhaseStartW,
        kPhaseMiddleW,
        kPhaseFinalRM,
        kPhaseFinalW
    };

    struct DeblockerContext
    {
        struct
        {
            UInt64               byteStart;
            IOMemoryDescriptor * buffer;
            IOStorageCompletion  completion;
        } subrequest;

        UInt64               offsetStart;
        UInt64               bytesStart;
        UInt64               bytesMiddle;
        UInt64               bytesFinal;

        IOMemoryDescriptor * blockBuffer;
        UInt8 *              blockBufferPtr;

        IOMemoryDescriptor * middleBuffer;

        UInt64               bytesTransferred;

        struct
        {
            UInt64               byteStart;
            IOMemoryDescriptor * buffer;
            IOStorageCompletion  completion;
        } originalRequest;

        DeblockerPhase       phase;
    };

    IOLock * _deblockerLockForRMWs;

    /*
     * Allocate a deblocker context structure.  It comes preinitialized with
     * a block-sized scratch buffer and an associated memory descriptor, but
     * is otherwise uninitialized.
     */

    DeblockerContext * deblockerContextAllocate();

    /*
     * Deallocate a deblocker context structure.  The block-sized scratch
     * buffer and memory descriptor is automatically deallocated as well.
     */

    void deblockerContextFree(DeblockerContext * deblockContext);

    /*
     * This is the completion routine for the aligned deblocker subrequests.
     * It performs work on the just-completed phase, if any, transitions to
     * the next phase, then builds and issues a transfer for the next phase.
     */

    static void deblockerCompletion(void *   target,
                                    void *   parameter,
                                    IOReturn status,
                                    UInt64   actualByteCount);

    /*
     * Schedule the poller mechanism.
     */

    void schedulePoller();

    /*
     * Unschedule the poller mechanism.
     */

    void unschedulePoller();

    /*
     * This method is the timeout handler for the poller mechanism.  It polls
     * for media and reschedules another timeout if the drive is still closed.
     */

    static void poller(void *, void *);
};

#endif /* defined(KERNEL) && defined(__cplusplus) */

#endif /* !_IODRIVE_H */
