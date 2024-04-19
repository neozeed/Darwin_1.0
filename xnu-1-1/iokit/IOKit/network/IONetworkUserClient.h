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
 * HISTORY
 *
 */

#ifndef _IONETWORKUSERCLIENT_H
#define _IONETWORKUSERCLIENT_H

// IONetworkUserClient type ID.
//
#define kIONUCType  0xff000001

// IONetworkUserClient call structure definitions.
//
enum {
        kIONUCResetNetworkDataIndex          = 0,
#define kIONUCResetNetworkDataInputs           1
#define kIONUCResetNetworkDataOutputs          0
#define kIONUCResetNetworkDataFlags            kIOUCScalarIScalarO

        kIONUCWriteNetworkDataIndex          = 1,
#define kIONUCWriteNetworkDataInput0           0xffffffff
#define kIONUCWriteNetworkDataInput1           0xffffffff
#define kIONUCWriteNetworkDataFlags            kIOUCScalarIStructI

        kIONUCReadNetworkDataIndex           = 2,
#define kIONUCReadNetworkDataInputs            1
#define kIONUCReadNetworkDataOutputs           0xffffffff
#define kIONUCReadNetworkDataFlags             kIOUCScalarIStructO

        kIONUCGetNetworkDataCapacityIndex    = 3,
#define kIONUCGetNetworkDataCapacityInputs     1
#define kIONUCGetNetworkDataCapacityOutputs    1
#define kIONUCGetNetworkDataCapacityFlags      kIOUCScalarIScalarO

        kIONUCGetNetworkDataHandleIndex      = 4,
#define kIONUCGetNetworkDataHandleInputs       0xffffffff
#define kIONUCGetNetworkDataHandleOutputs      0xffffffff
#define kIONUCGetNetworkDataHandleFlags        kIOUCStructIStructO

        kIONUCLastIndex
};

// Notification types.
//
enum {
    kIONUCNotificationTypeLinkChange  = 0xff000001,
};

typedef struct _IONetworkNotifyMsg {
    mach_msg_header_t h;
//  msg_type_t        ref;
} IONetworkNotifyMsg;

#ifdef KERNEL

#include <IOKit/IOUserClient.h>

class IONetworkInterface;
class IONetworkController;

/*! @class IONetworkUserClient
    @abstract An IOUserClient created by an IONetworkInterface to
    manage user space requests. */

class IONetworkUserClient : public IOUserClient
{
    OSDeclareDefaultStructors(IONetworkUserClient)

protected:
    IONetworkInterface *        _owner;
    IONetworkController *       _ctlr;
    mach_port_t                 _notifyPort;
    task_t                      _task;
    IOExternalMethod            _methods[kIONUCLastIndex];

/*! @function
    @abstract Free the IONetworkUserClient instance. */

    virtual void free();

public:

/*! @function withTask
    @abstract Factory method that performs allocation and initialization
    of an IONetworkUserClient instance.
    @param owningTask See IOUserClient.
    @result An IONetworkUserClient on success, 0 otherwise. */

    static IONetworkUserClient * withTask(task_t owningTask);

/*! @function start
    @abstract Start the IONetworkUserClient.
    @discussion Open the provider, must be an IONetworkInterface object,
    and initialize the IOExternalMethod array.
    @result true on success, false otherwise. */

    virtual bool start(IOService * provider);

/*! @function clientClose
    @abstract Handle a client close.
    @discussion Close and detach from our owner (provider).
    @result kIOReturnSuccess */

    virtual IOReturn clientClose();

/*! @function clientDied
    @abstract Handle client death.
    @discussion Close and detach from our owner (provider).
    @result kIOReturnSuccess */

    virtual IOReturn clientDied();

/*! @function registerNotificationPort
    @abstract Called by a client to register its notification port.
    @param port A mach port where the notification message should be sent.
    @param type The type of notification that the client is interested in.
    @param refCon An argument to deliver with the notification.
    @result kIOReturnUnsupported if the notification type is unknown,
    kIOReturnSuccess otherwise. */

    virtual IOReturn registerNotificationPort(mach_port_t port,
                                              UInt32      type,
                                              UInt32      refCon);

/*! @function deliverNotification
    @abstract Deliver a notification to the registered notification port.
    @param type The type of notification being delivered.
    @result kIOReturnSuccess on success, kIOReturnUnsupported if the
    notification type is unknown, or kIOReturnError for a mach messaging
    error. */

    virtual IOReturn deliverNotification(UInt32 type);

/*! @function getExternalMethodForIndex
    @abstract Look up an entry from the method array.
    @discussion Called by IOUserClient to fetch the method entry,
    encoded by an IOExternalMethod structure, that correspond to
    the index given.
    @param index The method index.
    @result A pointer to a IOExternalMethod structure containing the
    method definition. */

    virtual IOExternalMethod * getExternalMethodForIndex(UInt32 index);

protected:

/*! @function resetNetworkData
    @abstract Fill the data buffer in an IONetworkData object with zeroes.
    @param key The OSSymbol key of an IONetworkData object.
    @result kIOReturnSuccess on success, kIOReturnBadArgument if an
    argument is invalid, or an error from IONetworkData::reset(). */

    virtual IOReturn resetNetworkData(OSSymbol * key);

/*! @function writeNetworkData
    @abstract Write to the data buffer in an IONetworkData object from
    a buffer provided by the caller.
    @param key The OSSymbol key of an IONetworkData object.
    @param srcBuf The data to write is taken from this buffer.
    @param inSize The size of the source buffer.
    @result kIOReturnSuccess on success, kIOReturnBadArgument if an
    argument is invalid, or an error from IONetworkData::write(). */

    virtual IOReturn writeNetworkData(OSSymbol *   key,
                                      void *       srcBuf,
                                      IOByteCount  inSize);

/*! @function readNetworkData
    @abstract Read (copy) the data buffer in an IONetworkData object to
    a buffer provided by the caller.
    @param key The OSSymbol key of an IONetworkData object.
    @param destBuf The buffer where the data shall be written to.
    @param inOutSize Pointer to an integer that the caller must initialize
    to contain the size of the buffer. This method will overwrite it with
    the actual number of bytes written to the buffer.
    @result kIOReturnSuccess on success, kIOReturnBadArgument if an
    argument is invalid, or an error from IONetworkData::read(). */

    virtual IOReturn readNetworkData(OSSymbol *    key,
                                     void *        destBuf,
                                     IOByteCount * inOutSizeP);

/*! @function getNetworkDataCapacity
    @abstract Get the capacity of an IONetworkData object.
    @param key The OSSymbol key of an IONetworkData object.
    @param capacityP On success, the capacity is written to the integer
    at this address.
    @result kIOReturnSuccess on success, kIOReturnBadArgument if an
    argument is invalid. */

    virtual IOReturn getNetworkDataCapacity(OSSymbol * key,
                                            UInt32 *   capacityP);

/*! @function getNetworkDataHandle
    @abstract Return an opaque handle to a provider's IONetworkData object.
    @discussion Called to obtain a handle that maps to an IONetworkData
    object. This handle can be later passed to other methods in this class
    to refer to the same object.
    @param name A C string with the name of the IONetworkData object.
    @param handle If an IONetworkData object with the given name is found,
    then its associated OSSymbol object is written to this address.
    @param nameSize The size of the name string, including the final
    null character.
    @param handleSizeP The size of the buffer allocated by the caller
    to store the handle. This should be 4 bytes.
    @result kIOReturnSuccess on success, kIOReturnBadArgument if an
    argument is invalid, or kIOReturnNoMemory if unable to allocate memory. */

    virtual IOReturn getNetworkDataHandle(char *         name,
                                          OSObject **    handle,
                                          IOByteCount    nameSize,
                                          IOByteCount *  handleSizeP);
};

#endif /* KERNEL */

#endif /* !_IONETWORKUSERCLIENT_H */
