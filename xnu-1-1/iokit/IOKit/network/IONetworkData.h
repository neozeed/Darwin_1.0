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
 * Copyright (c) 1998 Apple Computer, Inc.  All rights reserved. 
 *
 * IONetworkData.h
 *
 * HISTORY
 * 21-Apr-1999      Joe Liu (jliu) created.
 *
 */

#ifndef _IONETWORKDATA_H
#define _IONETWORKDATA_H

#define IONetworkParameter IONetworkData

/*! @enum IONDAccessFlags
    @discussion Flags to encode access types, and other flags
    used by the access methods.
    @constant kIONDAccessTypeRead  read access type.
    @constant kIONDAccessTypeWrite write access type.
    @constant kIONDAccessTypeReset reset access type.
    @constant kIONDAccessTypeSerialize serialize access type.
    @constant kIONDAccessNoBufferAccess The data buffer
    cannot be accessed through the access methods. Buffer serialization
    is also disabled. However, the notification handler will continue
    to be called for supported access types. */

enum IONDAccessFlags {
    kIONDAccessTypeRead        = 0x01,
    kIONDAccessTypeWrite       = 0x02,
    kIONDAccessTypeReset       = 0x04,
    kIONDAccessTypeSerialize   = 0x08,
    kIONDAccessTypeMask        = 0xff,

    kIONDAccessNoBufferAccess  = 0x100,
};

/*! @define kIONDBasicAccessTypes
    @discussion The default access types supported by an IONetworkData
    object. Allow read() and serialize(). */

#define kIONDBasicAccessTypes \
       (kIONDAccessTypeRead | kIONDAccessTypeSerialize)

/*! @define kIONDImmutableAccessFlags
    @discussion Access flags that cannot be changed by setAccessFlags(),
    after the object has been initialized. */

#define kIONDImmutableAccessFlags   0

#ifdef KERNEL

#include <libkern/c++/OSData.h>
#include <libkern/c++/OSSymbol.h>
#include <libkern/c++/OSSerialize.h>

/*! @class IONetworkData : public OSData
    An IONetworkData manages a fixed-capacity named buffer.
    This class provide external access methods that can be called by
    an IOUserClient to allow user space access to the data buffer.
    In addition, serialization is supported, and therefore the object
    can be added to a property table to publish the data object.
    An unique name must be assigned to the object during initialization.
    An OSSymbol key will be created based on the assigned name, and this
    key should be used when the object is added to a dictionary.

    The level of access granted to the access methods can be controlled,
    by specifying a default set of access flags when the object is
    initialized, or modified later by calling setAccessFlags(). By
    default, each IONetworkData object created will support serialization,
    and will also allow its data buffer to be read through the read()
    access method.

    A notification handler, in the form of a C++ function pointer, can
    be registered to receive a call each time the data buffer is accessed
    through an access method. Arguments provided will identify the data
    object and the type of access that triggered the notification.
    The handler can therefore perform lazy update of the data buffer
    until an interested party tries to read the data.
    The notification handler can also take over the default action performed
    by the access methods when the kIONDAccessNoBufferAccess flag is set.
    This will prevent the access methods from accessing the data buffer,
    and allow the handler to override the access protocol.

    This object is used by IONetworkInterface to export statistics
    counters to user space. */


class IONetworkData : public OSData
{
    OSDeclareDefaultStructors(IONetworkData)

public:

/*! @typedef Action
    Defines the prototype of the notification handler that is called
    when the data buffer is accessed through an access method.
    @param data The IONetworkData object being accessed
           (the sender of the notification).
    @param accessType A bit will be set indicating a particular
           access type.
    @param arg An action argument.
    @param buffer Pointer to the client buffer for read() and write()
           accesses.
    @param bufSize Pointer to the size of the client buffer. */

    typedef IOReturn (OSObject::*Action)(IONetworkData * data,
                                         UInt32          accessType,
                                         void *          arg,
                                         void *          buffer,
                                         UInt32 *        bufSize);

protected:
    const OSSymbol *    _key;       // key associated with this object.
    UInt32              _access;    // access flags.
    UInt                _capacity;  // data buffer capacity.
    OSObject *          _tapTarget; // a target for access notification.
    Action              _tapAction; // the target method to call.
    void *              _tapArg;    // arbitrary target argument.

/*! @function free
    @abstract Free the IONetworkData instance. */

    virtual void free();

public:

/*! @function initialize
    @abstract IONetworkData class initializer. */

    static void initialize();

/*! @function withName
    @abstract Factory method that will construct and initialize an
    IONetworkData instance.
    @param name A name assigned to this object.
    @param capacity Capacity of the data buffer.
    @param buffer Pointer to an external data buffer. If 0,
           then an internal buffer shall be allocated.
    @param accessFlags Access flags.
           Can be later modified by calling setAccessFlags().
    @param target Notification target. The target will be notified
           when IONetworkData receives a call to one of its
           access methods. If 0, then notification is disabled.
    @param action Notification action. If 0, then notification is
           disabled.
    @param arg An argument to passed to the notification
           action when it is called.
    @result An IONetworkData instance on success, or 0 otherwise. */

    static IONetworkData * withName(
                           const char * name,
                           UInt32       capacity,
                           void *       buffer = 0,
                           UInt32       accessFlags  = kIONDBasicAccessTypes,
                           OSObject *   target = 0,
                           Action       action = 0,
                           void *       arg    = 0);

/*! @function init
    @abstract Initialize an IONetworkData instance.
    @param name A name assigned to this object.
    @param capacity Capacity of the data buffer.
    @param buffer Pointer to an external data buffer. If 0,
           then an internal buffer shall be allocated.
    @param accessFlags Access flags.
           Can be later modified by calling setAccessFlags().
    @param target Notification target. The target will be notified
           when IONetworkData receives a call to one of its
           access methods. If 0, then notification is disabled.
    @param action Notification action. If 0, then notification is
           disabled.
    @param arg An argument to passed to the notification
           action when it is called.
    @result true if initialized successfully, false otherwise. */

    virtual bool initWithName(const char * name,
                              UInt32       capacity,
                              void *       buffer = 0,
                              UInt32       accessFlags = kIONDBasicAccessTypes,
                              OSObject *   target = 0,
                              Action       action = 0,
                              void *       arg    = 0);

/*! @function setAccessFlags
    @abstract Change the access flags.
    @param accessFlags A bitfield of access flag bits.
           See IONDAccessFlags enum. */

    virtual void setAccessFlags(UInt32 accessFlags);

/*! @function setNotificationTarget
    @abstract Register a target/action to handle access notification.
    @discussion A notification is sent by an IONetworkData to the registered
    notification handler when an access method is called.
    @param target The target object that implements the notification action.
           If 0, then notification will be disabled.
    @param action The notification action.
           If 0, then notification will be disabled.
    @param arg An optional argument passed to the notification handler. */

    virtual void setNotificationTarget(OSObject *  target,
                                       Action      action,
                                       void *      arg = 0);

/*! @function getBuffer
    @result A pointer to the data buffer. */

    virtual const void *     getBuffer() const;

/*! @function getAccessFlags
    @result The access flags. */

    virtual UInt32           getAccessFlags() const;

/*! @function getTarget
    @result The notification target. */

    virtual OSObject *       getTarget() const;

/*! @function getAction
    @result The notification action. */

    virtual Action           getAction() const;

/*! @function getArgument
    @result The optional notification argument. */

    virtual void *           getArgument() const;

/*! @function getKey
    @abstract Get an OSSymbol key associated with this IONetworkData
    instance.
    @discussion During initialization, IONetworkData will create an
    OSSymbol key based on its assigned name.
    @result An OSSymbol key generated from the assigned name. */

    virtual const OSSymbol * getKey() const;

/*! @function getCapacity
    @abstract Override the getCapacity() method inherited from OSData.
    @discussion This method overrides the implementation in OSData
    in order to report the capacity for both internal or external
    data buffers.
    @result The capacity of the data buffer in bytes. */

    virtual UInt getCapacity() const;

/*! @function setBytes
    @abstract Update the data buffer from a source buffer provided by the
    caller.
    @param bytes Pointer to a source buffer provided by the caller.
    @param inLength Length of the source buffer, or the amount of data
           to copy to the data buffer. The length provided must
           be smaller than or equal to the capacity of the object.
    @result true if the length of the source buffer is within limits,
    and the source buffer was copied, false otherwise. */

    virtual bool setBytes(const void * bytes, UInt inLength);

/*! @function setLength
    @abstract Set a new data buffer length to indicate the amount of
    valid data in the data buffer.
    @param inLength Length of the data buffer in bytes. This must be
           smaller than or equal to the capacity of the data object.
    @result true if the length provided was accepted. */

    virtual bool setLength(UInt inLength);

/*! @function copyBytes
    @abstract Copy the data buffer (with length bytes) to a destination
    buffer provided by the caller.
    @param bytes Pointer to a destination buffer.
    @param inOutLength The caller must initialize the integer value pointed
    by inOutLength with the size of the destination buffer before the call.
    This method will overwrite it with the number of bytes copied.
    @result true if the destination buffer is larger than or equal to the 
    length of the data buffer, false otherwise. */

    virtual bool copyBytes(void * bytes, UInt * inOutLength) const;

/*! @function clearBytes
    @abstract Clear the entire data buffer and fill it with zeroes.
    @result true if the buffer was cleared, false otherwise. */

    virtual bool clearBytes();

/*! @function reset
    @abstract An access method to reset the data buffer.
    @discussion Handle an external request to reset the data buffer.
    If notication is enabled, then the notification handler is called
    after the data buffer has been cleared.
    @result kIOReturnNotWritable if reset access is not allowed,
    kIOReturnSuccess otherwise. If a notification handler is called,
    and it returns an error code, then that error is returned. */

    virtual IOReturn reset();

/*! @function read
    @abstract An access method to read from the data buffer.
    @discussion Handle an external request to read from the data buffer
    and copy it to the destination buffer provided. If notification is
    enabled, then the notification handler is called before the data buffer
    is copied to the destination buffer. The notification handler may use
    this opportunity to update the contents of the data buffer.
    @param inBuffer Pointer to the destination buffer.
    @param inOutSize Pointer to an integer containing the size of the
    destination buffer before the call. Overwritten by this method to
    the actual number of bytes copied to the destination buffer.
    @result kIOReturnSuccess on success,
    kIOReturnBadArgument if an argument provided is invalid,
    kIOReturnNoSpace if the destination buffer is too small,
    kIOReturnNotReadable if read access is not permitted,
    or an error from the notification handler. */

    virtual IOReturn read(void * inBuffer, UInt * inOutSize);

/*! @function write
    @abstract An access method to write to the data buffer.
    @discussion Handle an external request to write to the data buffer
    from a source buffer provided. If notification is enabled, then
    the notification handler is called after the source buffer has
    been copied to the data buffer.
    @param inBuffer Pointer to the source buffer.
    @param size Size of the source buffer in bytes.
    @result kIOReturnSuccess on success,
    kIOReturnBadArgument if an argument provided is invalid,
    kIOReturnNoSpace if the source buffer is too big,
    kIOReturnNotWritable if write access is not permitted,
    or an error from the notification handler. */

    virtual IOReturn write(void * inBuffer, UInt size);

/*! @function serialize
    @abstract Serialize the object, including the data buffer.
    @discussion If notification is enabled, then the notification
    handler is called just before the data buffer is serialized.
    @param s An OSSerialize object.
    @result true on success, false otherwise. */

    virtual bool serialize(OSSerialize * s) const;
};

#endif /* KERNEL */

#endif /* !_IONETWORKDATA_H */
