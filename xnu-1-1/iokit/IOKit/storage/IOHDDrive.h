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
 * IOHDDrive.h
 *
 * This class implements generic Hard Disk functionality, independent of
 * the physical connection protocol (e.g. SCSI, ATA, USB).
 *
 * A protocol-specific provider implements the functionality using an appropriate
 * protocol and commands.
 */

#ifndef	_IOHDDRIVE_H
#define	_IOHDDRIVE_H

#include <IOKit/IOTypes.h>
#include <IOKit/storage/IOStorage.h>
#include <IOKit/storage/IODrive.h>
#include <IOKit/IOMemoryDescriptor.h>

class IOHDDriveNub;
class IOMedia;

/*!
 * @class
 * IOHDDrive : public IODrive
 * @abstract
 * Generic Hard Disk Driver.
 * @discussion
 * Storage drivers are split into two parts: the Generic Driver handles
 * all generic device issues, independent of the lower-level transport
 * mechanism (e.g. SCSI, ATA, USB, FireWire). All storage operations
 * at the Generic Driver level are translated into a series of generic
 * device operations. These operations are passed via the Device Nub
 * to a Transport Driver, which implements the appropriate
 * transport-dependent protocol to execute these operations.
 * 
 * To determine the write-protect state of a device (or media), for
 * example, the generic driver would issue a call to the
 * Transport Driver's reportWriteProtection method. If this were a SCSI
 * device, its Transport Driver would issue a Mode Sense command to
 * extract the write-protection status bit. The Transport Driver then
 * reports true or false to the generic driver.
 * 
 * The generic driver therefore has no knowledge of, or involvement
 * with, the actual commands and mechanisms used to communicate with
 * the device. It is expected that the generic driver will rarely, if
 * ever, need to be subclassed to handle device idiosyncrasies; rather,
 * the Transport Driver should be changed via overrides.
 * 
 * A generic driver could be subclassed to create a different type of
 * generic device. The generic driver IOCDDrive class is a subclass
 * of IOHDDrive, adding CD functions. Similarly, the Transport Driver
 * IOSCSICDDrive is a subclass of IOSCSIHDDrive, adding CD functions.
*/

class IOHDDrive : public IODrive {

    OSDeclareDefaultStructors(IOHDDrive)

public:

struct context {
    IOStorageCompletion		completion;
    IOMemoryDescriptor *	buffer;
    UInt64			byteStart;
    UInt64			byteCount;

    IOReturn			result;
    UInt64			actualTransferCount;
};

    /* Overrides from IORegistryEntry */

    virtual void	free(void);
    virtual bool	init(OSDictionary * properties);

    /* Overrides from IOService */

    /*!
     * @function probe
     * @abstract
     * Probe for proper client Transport Driver.
     * @discussion
     * This method first verifies that our provider is of class IOHDDriveNub, then
     * checks that the provider's property "kDeviceTypeProperty" matches what we expect.
     * Failure of either test causes a return of NULL.
     */
    virtual IOService * probe(IOService * provider,SInt32 * score);
    
    /* Mandatory overrides from IODrive: */

    /*!
     * @function constrainByteCount
     * @abstract
     * Constrain the byte count for this IO to device limits.
     * @discussion
     * This function should be called prior to each read or write operation, so that
     * the driver can constrain the requested byte count, as necessary, to meet
     * current device limits. Such limits could be imposed by the device depending
     * on operating modes, media types, or transport prototol (e.g. ATA, SCSI).
     * 
     * At present, this method is not used.
     * @param requestedCount
     * The requested byte count for the next read or write operation.
     * @param isWrite
     * True if the operation will be a write; False if the operation will be a read.
     */
    virtual UInt64	constrainByteCount(UInt64 requestedCount,bool isWrite);

    /*!
     * @function ejectMedia
     * @abstract
     * Eject the media.
     */
    virtual IOReturn	ejectMedia(void);

    /*!
     * @function formatMedia
     * @abstract
     * Format the media to the specified byte capacity.
     * @param byteCapacity
     * The requested final byte capacity for the formatted media.
     */
    virtual IOReturn	formatMedia(UInt64 byteCapacity);

    /*!
     * @function getFormatCapacities
     * @abstract
     * Return the allowable formatting byte capacities.
     * @discussion
     * This function returns the supported byte capacities for the device.
     * @param capacities
     * Pointer for returning the list of capacities.
     * @param capacitiesMaxCount
     * The number of capacity values returned in "capacities."
     */
    virtual UInt32	getFormatCapacities(UInt64 * capacities,
                                            UInt32   capacitiesMaxCount) const;

    /*!
     * @function getMediaBlockSize
     * @abstract
     * Return the block size for the device, in bytes.
     */
    virtual UInt64	getMediaBlockSize(void) const;

    /*!
     * @function getMediaState
     * @abstract
     * Return the state of media in the device.
     */
    virtual IODrive::IOMediaState getMediaState(void) const;

    /*!
     * @function handleStart
     * @abstract
     * Handle startup functionality after superclass is ready.
     * @discussion
     * This function obtains device description strings (e.g. Vendor name), prints
     * a message describing the device, and then obtains all removable-media information
     * (if the device is removable.) Finally, a check for media is performed.
     * @param provider
     * A pointer to our provider.
     */
    virtual bool	handleStart(IOService * provider);

    /*!
     * @function handleStop
     * @abstract
     * Handle clean up functionality before driver is unloaded.
     * @discussion
     * If the media is writable, this method issues a Synchronize Cache operation.
     * Then the media is ejected.
     * @param provider
     * A pointer to our provider.
     */
    virtual void	handleStop(IOService * provider);

    /*!
     * @function isMediaEjectable
     * @abstract
     * Report whether the media is removable or not.
     * @result
     * True indicates the media can be ejected by software; False indicates it cannot.
     */
    virtual bool	isMediaEjectable(void) const;

    /*!
     * @function isMediaPollExpensive
     * @abstract
     * Report whether polling for media is expensive.
     * @result
     * True indicates the media poll is expensive; False indicates polling is cheap.
     */
    virtual bool	isMediaPollExpensive(void) const;

    /*!
     * @function isMediaPollRequired
     * @abstract
     * Report whether device must be polled to determine media state.
     * @discussion
     * @result
     * True indicates that polling is required; False indicates that no polling is needed.
     */
    virtual bool	isMediaPollRequired(void) const;

    /*!
     * @function lockMedia
     * @abstract
     * Lock or unlock the media in the device.
     * @discussion
     * This method may be used to lock the media in the device, thus preventing the user
     * from removing the media manually.
     * 
     * This method only makes sense if it is known that the device supports locking.
     * @param lock
     * True indicates the media should be locked in the device;
     * False indicates the media should be unlocked.
     */
    virtual IOReturn	lockMedia(bool lock);

    /*!
     * @function pollMedia
     * @abstract
     * Poll for media insertion/removal.
     * @discussion
     * This method is called periodically by the superclass, if isMediaPollRequired has
     * previously reported True. We are responsible for reacting to new media insertion,
     * or to existing media being removed.
     */
    virtual IOReturn	pollMedia(void);

protected:

    /*!
     * @function executeRequest
     * @abstract
     * Start an asynchronous read or write operation.
     * @discussion
     * This method is the main entry to start an asynchronous read or
     * write operation. All IO operations must be block aligned and a
     * multiple of the device block size. After some validations, the
     * request is passed to the Transport Driver's doAsyncReadWrite method.
     * The completion from the operation is set to call RWCompletion, by
     * way of the gc_glue C function.
     * @param byteStart
     * The starting byte offset of the data transfer.
     * @param buffer
     * The data buffer for the operation. A pointer to an IOMemoryDescriptor.
     * @param completion
     * The completion information for an asynchronous read or write operation.
     */
    virtual void 	executeRequest(UInt64               byteStart,
                	               IOMemoryDescriptor * buffer,
                	               IOStorageCompletion  completion);

    /*!
     * @function acceptNewMedia
     * @abstract
     * React to new media insertion.
     * @discussion
     * This method logs the media block size and block count, then calls
     * instantiateMediaObject to get a media object instantiated. The
     * media object is then attached above us and registered.
     * 
     * This method can be overridden to control what happens when new media
     * is inserted. The default implementation deals with one IOMedia object.
     */
    virtual IOReturn	acceptNewMedia(void);
    
    /*!
     * @function decommissionMedia
     * @abstract
     * Decommission an existing piece of media that has gone away.
     * @discussion
     * This method wraps a call to tearDown, to tear down the stack and
     * the IOMedia object for the media. If "forcible" is true, the media
     * object will be forgotten, and initMediaStates will be called. A
     * forcible decommission would occur when an unrecoverable error
     * happens during teardown (e.g. perhaps a client is still open), but
     * we must still forget about the media.
     * @param forcible
     * True to force forgetting of the media object even if tearDown reports
     * that there was an active client.
     */
    virtual IOReturn	decommissionMedia(bool forcible);

    /*!
     * @function instantiateDesiredMediaObject
     * @abstract
     * Create an IOMedia object for media.
     * @discussion
     * This method creates the exact type of IOMedia object desired. It is called by
     * instantiateMediaObject. A subclass may override this one-line method to change
     * the type of media object actually instantiated.
     */
    virtual IOMedia *	instantiateDesiredMediaObject(void);

    /*!
     * @function instantiateMediaObject
     * @abstract
     * Create an IOMedia object for media.
     * @discussion
     * This method creates an IOMedia object from the supplied parameters. It is a
     * convenience method to wrap the handful of steps to do the job.
     * @param media
     * A pointer to the created IOMedia object.
     * @param base
     * Byte number of beginning of active data area of the media. Usually zero.
     * @param byteSize
     * Size of the data area of the media, in bytes.
     * @param blockSize
     * Block size of the media, in bytes.
     * @param isWholeMedia
     * True indicates the IOMedia object represents the entire media; False indicates
     * the IOMedia object represents a portion of the entire media.
     * @param mediaName
     * Name of the IOMedia object.
     */
    virtual IOReturn	instantiateMediaObject(IOMedia **media,UInt64 base,UInt64 byteSize,UInt32 blockSize,
                                            bool isWholeMedia,char *mediaName);

    /*!
     * @function recordAdditionalMediaParameters
     * @abstract
     * Record any additional media parameters as necessary.
     * @discussion
     * This method is called by recordMediaParameters() when media is detected.
     * The default implementation does nothing and returns kIOReturnSuccess. A
     * subclass may override this method to record additional parameters.
     */
    virtual IOReturn	recordAdditionalMediaParameters(void);
    
    /* --- Internally used methods. --- */

    /*
     * @group
     * Internally Used Methods
     * @discussion
     * These methods are used internally, and will not generally be modified.
     */
    
    /*!
     * @function allocateContext
     * @abstract
     * Allocate a context structure for use with the current async operation.
     * @discussion
     * A context structure is only needed for asynchronous read or write operations,
     * to contain information used to complete the request with the client.
     */
    virtual struct IOHDDrive::context *allocateContext(void);
    
    /*!
     * @function checkForMedia
     * @abstract
     * Check if media has newly arrived or disappeared.
     * @discussion
     * This method does most of the work in polling for media, first
     * calling the Transport Driver's reportMediaState method. If
     * reportMediaState reports no change in the media state, kIOReturnSuccess
     * is returned. If media has just become available, calls are made to
     * recordMediaParameters and acceptNewMedia. If media has just gone
     * away, a call is made to decommissionMedia, with the forcible
     * parameter set to true. The forcible teardown is needed to enforce
     * the disappearance of media, regardless of interested clients.
     */
    virtual IOReturn	checkForMedia(void);

    /*!
     * @function constructMediaProperties
     * @abstract
     * Create properties related to the media.
     * @discussion
     * This function creates a set of properties to express media properties.
     * 
     * This function is presently called by recordMediaParameters, but it does nothing.
     */
    virtual OSDictionary *constructMediaProperties(void);
    
    /*!
     * @function deleteContext
     * @abstract
     * Delete a context structure.
     * @discussion
     * This method also issues a "release" for the IO buffer, if any.
     * @param cx
     * A pointer to the context structure to be deleted.
     */
    virtual void	deleteContext(struct IOHDDrive::context *cx);
    
    /*!
     * @function getDeviceTypeName
     * @abstract
     * Return the desired device name.
     * @discussion
     * This method returns a string, used to compare the kDeviceTypeProperty of
     * our provider. This method is called from probe.
     *  
     * The default implementation of this method returns kDeviceTypeHardDisk.
     */
    virtual const char * getDeviceTypeName(void);
    
    /*!
     * @function initMediaStates
     * @abstract
     * Initialize media-related instance variables.
     * @discussion
     * Called when media is not present, this method marks the drive state
     * as not having media present, not spun up, and write-enabled.
     */
    virtual void	initMediaStates(void);
    
    /*!
     * @function recordMediaParameters
     * @abstract
     * Obtain media-related parameters on media insertion.
     * @discussion
     * This method obtains media-related parameters via calls to the
     * Transport Driver's reportBlockSize, reportMaxValidBlock,
     * reportMaxReadTransfer, reportMaxWriteTransfer, and reportWriteProtection
     * methods.
     */
    virtual IOReturn	recordMediaParameters(void);
    
    /*!
     * @function setProvider
     * @abstract
     * Store the provider pointer.
     * @discussion
     * This method uses IODynamicCast to verify that the provider is of
     * the proper class type. The default implementation checks for a nub
     * of type IOHDDriveNub.
     * 
     * This method would be overridden for a new generic driver subclass. For
     * example, a generic CD driver would need to accept a different provider
     * class (e.g. IOCDDriveNub instead of IOHDDriveNub).
     * @param provider
     * A pointer to our provider.
     */
    virtual bool	setProvider(IOService * provider);
    
    /*!
     * @function showStats
     * @abstract
     * Print debugging statistics.
     * @discussion
     * This method prints debugging statistics maintained by the class.
     * 
     * Present statistics include counts of up and down calls.
     * @result
     * True indicates statistics should be printed; False indicates no printing desired.
     */
    virtual bool	showStats(void);
    
    /*!
     * @function tearDown
     * @abstract
     * Tear down the stack above this object when media goes away.
     * @discussion
     * This method calls media->terminate, and if that succeeds, issues
     * a release on the media object, followed by a call to initMediaStates.
     * @param media
     * A pointer to the IOMedia object from which to initiate teardown.
     */
    virtual IOReturn	tearDown(IOService *media);

    /*
     * @endgroup
     */
    
    /*!
     * @function validateNewMedia
     * @abstract
     * Verify that new media is acceptable.
     * @discussion
     * This method will be called whenever new media is detected. Return true to accept
     * the media, or false to reject it (andcall rejectMedia). Vendors might override
     * this method to handle password-protection for new media.
     * 
     * The default implementation always returns True, indicating media is accepted.
     */
    virtual bool	validateNewMedia(void);
    
    /*!
     * @function rejectMedia
     * @abstract
     * Reject new media.
     * @discussion
     * This method will be called if validateNewMedia returns False (thus rejecting
     * the new media. A vendor may choose to override this method to control behavior
     * when media is rejected.
     * 
     * The default implementation simply calls ejectMedia.
     */
    virtual void	rejectMedia(void);	/* default ejects */
    
public:
        
    /*!
     * @function RWCompletion
     * @abstract
     * Handle a C callback for an IO completion.
     * @discussion
     * This method must be public so we can reach it from the C-language callback
     * "glue" routine. It should not be called from outside this class.
     */
    virtual void	RWCompletion(struct IOHDDrive::context *cx);

protected:

    /* -------------*/

    /*!
     * @var _provider
     * A pointer to our provider.
     */
    IOHDDriveNub *	_provider;

    /* Device info: */

    /*!
     * @var _removable
     * True if the media is removable; False if it is fixed (not removable).
     */
    bool		_removable;

    /*!
     * @var _ejectable
     * True if the media is ejectable under software control.
     */
    bool		_ejectable;		/* software-ejectable */

    /*!
     * @var _lockable
     * True if the media can be locked in the device under software control.
     */
    bool		_lockable;		/* software lockable in drive */
    /*!
     * @var _pollIsRequired
     * True if we must poll to detect media insertion or removal.
     */
    bool		_pollIsRequired;
    /*!
     * @var _pollIsExpensive
     * True if polling is expensive; False if not.
     */
    bool		_pollIsExpensive;

    /* Async IO statistics, usually only useful for debugging. These counters show
     * outstanding IO operations and completions.
     */

    /*!
     * @struct stats
     * A data structure to contain debugging statistics. Each field is a count.
     * @field clientReceived
     * Requests received from our client.
     * @field providerSent
     * Requests sent to our provider.
     * @field providerReject
     * Client requests we rejected without sending to our provider.
     * @field providerCompletionsRcvd
     * Completion calls that have arrived from our provider. 
     * @field clientCompletionsSent
     * Completion calls that have been made to our client.
     * @field clientCompletionsDone
     * Completion calls to the client that have come back to us.
     * This count MUST match clientCompletionsSent!
     */
    /*!
     * @var stats
     * The stats structure.
     */
    struct stats {				/* executeRequest & completion counts */
        int		clientReceived;		/* requests received from client */
        int		providerSent;		/* requests sent to provider */
        int		providerReject;		/* provider requests rejected */
        int		providerCompletionsRcvd; /* completions received from provider */
        int		clientCompletionsSent;	/* completions sent to client */
        int		clientCompletionsDone;	/* client completions made it back to us */
    }			_stats;
                                            
    /* Media info and states: */

    /*!
     * @var _mediaObject
     * A pointer to the media object we have instantiated (if any).
     */
    IOMedia *		_mediaObject;
    /*!
     * @var _mediaPresent
     * True if media is present in the device; False if not.
     */
    bool		_mediaPresent;		/* media is present and ready */
    /*!
     * @var _writeProtected
     * True if the media is write-protected; False if not.
     */
    bool		_writeProtected;
    
    /*!
     * @var _mediaStateLock
     * A lock used to protect during media checks.
     */
    IOLock *		_mediaStateLock;

    /*!
     * @var _mediaBlockSize
     * The block size of the media, in bytes.
     */
    UInt64		_mediaBlockSize;
    /*!
     * @var _maxBlockNumber
     * The maximum allowable block number for the media, zero-based.
     */
    UInt64		_maxBlockNumber;

    /*!
     * @var _maxReadByteTransfer
     * The maximum byte transfer allowed for read operations.
     */
    UInt64		_maxReadByteTransfer;

    /*!
     * @var _maxWriteByteTransfer
     * The maximum byte transfer allowed for write operations.
     */
    UInt64		_maxWriteByteTransfer;
};
#endif
