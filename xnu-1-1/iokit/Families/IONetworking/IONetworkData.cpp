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
 * IONetworkData.cpp
 */

#include <libkern/c++/OSDictionary.h>
#include <libkern/c++/OSNumber.h>
#include <IOKit/assert.h>
#include <IOKit/IOLib.h>
#include <IOKit/network/IONetworkData.h>

#define super OSData
OSDefineMetaClassAndStructors( IONetworkData, OSData )

#define TAP_IS_VALID    (_tapTarget && _tapAction)

// All access method are serialized by a single global lock,
// shared among all IONetworkData instances.
//
static  IOLock * gIONDLock = 0;
#define LOCK     IOTakeLock(gIONDLock)
#define UNLOCK   IOUnlock(gIONDLock)

static const OSSymbol * gIONDDataKey;
static const OSSymbol * gIONDAccessKey;
static const OSSymbol * gIONDCapacityKey;

//---------------------------------------------------------------------------
// IONetworkData class initializer.

void IONetworkData::initialize()
{
    // Allocates the global data lock.
    //
    gIONDLock = IOLockAlloc();
    assert(gIONDLock);
    IOLockInitWithState(gIONDLock, kIOLockStateUnlocked);

    gIONDDataKey     = OSSymbol::withCStringNoCopy("Data"); 
    gIONDAccessKey   = OSSymbol::withCStringNoCopy("Access Flags");
    gIONDCapacityKey = OSSymbol::withCStringNoCopy("Capacity");

    assert(gIONDDataKey && gIONDAccessKey && gIONDCapacityKey);
}

//---------------------------------------------------------------------------
// Initialize an IONetworkData instance. A target/action may be
// registered to receive a notification when the read(), write(),
// reset(), or serialize() methods are called.
//
// name:        A name assigned to this object.
//
// buffer:      Pointer to an external data buffer. If 0,
//              then an internal buffer shall be allocated.
//
// capacity:    Capacity of the data buffer.
//
// accessFlags: Access flags.
//              Can be later modified by calling setAccessFlags().
//
// target:      Notification target. The target will be notified
//              when IONetworkData receives a call to one of its
//              access methods. If 0, then notification is disabled.
//
// action:      Notification action. If 0, then notification is
//              disabled.
//
// arg:         An argument to passed to the notification action.
//
// Returns true if initialized successfully, false otherwise.

bool
IONetworkData::initWithName(const char * name,
                            UInt32       inCapacity,
                            void *       buffer = 0,
                            UInt32       accessFlags = kIONDBasicAccessTypes,
                            OSObject *   target = 0,
                            Action       action = 0,
                            void *       arg    = 0)
{
    if ( ((buffer == 0) ?
           super::initWithCapacity(inCapacity) :
           super::initWithBytesNoCopy(buffer, inCapacity)) == false )
    {
        return false;
    }

    _access    = accessFlags;
    _tapTarget = target;
    _tapAction = action;
    _tapArg    = arg;
    _capacity  = inCapacity;
    length     = inCapacity;  // (ivar in OSData) force length to maximum

    // Generate a key for this object based on its assigned name.
    //
    if ((_key = OSSymbol::withCString(name)) == 0)
        return false;

    // Clear the data buffer.
    //
    clearBytes();

    return true;
}

//---------------------------------------------------------------------------
// Factory method that will construct and initialize an IONetworkData 
// instance.
//
// Returns an IONetworkData instance on success, or 0 otherwise.

IONetworkData *
IONetworkData::withName(const char * name,
                        UInt32       inCapacity,
                        void *       buffer = 0,
                        UInt32       accessFlags = kIONDBasicAccessTypes,
                        OSObject *   target = 0,
                        Action       action = 0,
                        void *       arg    = 0)
{
    IONetworkData * aData = new IONetworkData;
    
    if (aData && !aData->initWithName(name, inCapacity, buffer, accessFlags,
                                      target, action, arg))
    {
        aData->release();
        aData = 0;
    }
    return aData;
}

//---------------------------------------------------------------------------
// Free the IONetworkData instance.

void IONetworkData::free()
{
    if (_key)
        _key->release();

    // Superclass will free the internal data buffer.

    super::free();
}

//---------------------------------------------------------------------------
// Change the access flags.
//
// accessFlags: A bitfield of access flag bits.
//              See IONDAccessFlags enum.

void IONetworkData::setAccessFlags(UInt32 accessFlags)
{
    LOCK;
    _access = (_access & kIONDImmutableAccessFlags) |
              (accessFlags & ~kIONDImmutableAccessFlags);
    UNLOCK;
}

//---------------------------------------------------------------------------
// Register a target/action to handle access notification.
// A notification is sent by an IONetworkData to the registered
// notification handler when an access method is called.
//
// target: The target object that implements the notification action.
//         If 0, then notification will be disabled.
//
// action: The notification action. If 0, then notification will be disabled.
//
// arg:    An optional argument passed to the notification handler.

void IONetworkData::setNotificationTarget(OSObject *  target,
                                          Action      action,
                                          void *      arg = 0)
{
    LOCK;
    _tapTarget = target;
    _tapAction = action;
    _tapArg    = arg;
    UNLOCK;
}

//---------------------------------------------------------------------------
// Return a pointer to the data buffer.

const void * IONetworkData::getBuffer() const
{
    return getBytesNoCopy();
}

//---------------------------------------------------------------------------
// Return the access flags.

UInt32 IONetworkData::getAccessFlags() const
{
    return _access;
}

//---------------------------------------------------------------------------
// Return the notification target.

OSObject * IONetworkData::getTarget() const
{
    return _tapTarget;
}

//---------------------------------------------------------------------------
// Return the notification action.

IONetworkData::Action IONetworkData::getAction() const
{
    return _tapAction;
}

//---------------------------------------------------------------------------
// Return the notification argument.

void * IONetworkData::getArgument() const
{
    return _tapArg;
}

//---------------------------------------------------------------------------
// Get an OSSymbol key associated with this IONetworkData instance.
// During initialization, IONetworkData will create an OSSymbol
// key based on its assigned name.
//
// Return an OSSymbol key generated from the assigned name.

const OSSymbol * IONetworkData::getKey() const
{
    return _key;
}

//---------------------------------------------------------------------------
// Override the getCapacity() method inherited from OSData.
// This method overrides the implementation in OSData in
// order to report the capacity for both internal or external
// data buffers.
//
// Return the capacity of the data buffer in bytes.

UInt IONetworkData::getCapacity() const
{
    return _capacity;
}

//---------------------------------------------------------------------------
// Update the data buffer from a source buffer provided by the
// caller.
//
// bytes:    Pointer to a source buffer provided by the caller.
//
// inLength: Length of the source buffer, or the amount of data
//           to copy to the data buffer. The length provided must
//           be smaller than or equal to the capacity of the object.
//
// Returns true if the length of the source buffer is within limits, and
// the source buffer was copied, false otherwise.

bool IONetworkData::setBytes(const void * bytes, UInt inLength)
{
    if (inLength > _capacity)
        return false;       // specified buffer is too large.

    if (inLength == 0)
        return true;

    bcopy(bytes, data, inLength);

    length = inLength;      // set the new data length.

    return true;
}

//---------------------------------------------------------------------------
// Set a new data buffer length to indicate the amount of
// valid data in the data buffer.
//
// inLength: Length of the data buffer in bytes. The length provided must
//           be smaller than or equal to the capacity of the data object.
//
// Returns true if the length provided was accepted.

bool IONetworkData::setLength(UInt inLength)
{
    if (inLength > _capacity)
        return false;       // Length cannot exceed capacity.

    length = inLength;

    return true;
}

//---------------------------------------------------------------------------
// Copy the data buffer (with length bytes) to a destination buffer
// provided by the caller.
//
// bytes:       Pointer to a destination buffer.
//
// inOutLength: The caller must initialize the integer value pointed
//              by inOutLength with the size of the destination buffer
//              before the call. This method will overwrite it with the
//              number of bytes copied.
//
// Returns true if the destination buffer is larger than or equal to the
// length of the data buffer, false otherwise.

bool IONetworkData::copyBytes(void * bytes, UInt * inOutLength) const
{
    if (length > *inOutLength)
        return false;       // specified buffer is too small.
    
    *inOutLength = length;  // return the number of bytes copied.

    if (length)
        bcopy(data, bytes, length);

    return true;
}

//---------------------------------------------------------------------------
// Clear the entire data buffer and fill it with zeroes.
//
// Return true if the buffer was cleared, false otherwise.

bool IONetworkData::clearBytes()
{
    if (_capacity)
        bzero(data, _capacity);

    return true;
}

//---------------------------------------------------------------------------
// Handle a user space request to reset the data buffer. If notication
// is enabled, then the notification handler is called after the data
// buffer has been cleared
//
// Returns kIOReturnNotWritable if reset access is not allowed,
// kIOReturnSuccess otherwise. If a notification handler is called,
// and it returns an error code, then that error is returned.

IOReturn IONetworkData::reset()
{
    IOReturn ret = kIOReturnUnsupported;

    LOCK;

    do {
        // Check access.
        //
        if ((_access & kIONDAccessTypeReset) == 0)
        {
            ret = kIOReturnNotWritable;
            break;
        }

        // Default action is to bzero the entire buffer.
        //
        if ((_access & kIONDAccessNoBufferAccess) == 0)
        {
            clearBytes();
            ret = kIOReturnSuccess;
        }

        // Notify our target.
        //
        if (TAP_IS_VALID)
        {
            ret = (_tapTarget->*_tapAction)(this,
                                            (UInt32) kIONDAccessTypeReset,
                                            _tapArg, 0, 0);
        }
    }
    while (0);

    UNLOCK;

    return ret;
}

//---------------------------------------------------------------------------
// Handle a user space request to read from the data buffer and copy it
// to the destination buffer provided. If notification is enabled,
// then the notification handler is called before the data buffer
// is copied to the destination buffer. The notification handler may
// use this opportunity to update the contents of the data buffer.
//
// inBuffer:  Pointer to the destination buffer.
//
// inOutSize: Pointer to an integer containing the size of the destination
//            buffer before the call. Overwritten by this method to the
//            actual number of bytes copied to the destination buffer.
//
// Returns kIOReturnSuccess if success,
// kIOReturnBadArgument if an argument provided is invalid,
// kIOReturnNoSpace if the destination buffer is too small,
// kIOReturnNotReadable if read access is not permitted,
// or an error from the notification handler.

IOReturn IONetworkData::read(void * inBuffer, UInt * inOutSize)
{
    IOReturn ret = kIOReturnUnsupported;

    LOCK;

    do {
        // Check the arguments.
        //
        if (!inBuffer || !inOutSize)
        {
            ret = kIOReturnBadArgument;
            break;
        }

        // Check access.
        //
        if ((_access & kIONDAccessTypeRead) == 0)
        {
            ret = kIOReturnNotReadable;
            break;
        }

        // Notify the target before the read operation.
        // The target can take this opportunity to update the
        // data buffer. If the target returns an error,
        // abort and return the error.
        //
        if (TAP_IS_VALID)
        {
            ret = (_tapTarget->*_tapAction)(this,
                                            (UInt32) kIONDAccessTypeRead,
                                            _tapArg,
                                            inBuffer,
                                            (UInt32 *) inOutSize);
            if (ret != kIOReturnSuccess)
                break;
        }

        if ((_access & kIONDAccessNoBufferAccess) == 0)
        {
            ret = (copyBytes(inBuffer, inOutSize) ?
                  kIOReturnSuccess : kIOReturnNoSpace);
        }
    }
    while (0);

    UNLOCK;

    return ret;
}

//---------------------------------------------------------------------------
// Handle a request to write to the data buffer from a source buffer provided.
// If notification is enabled, then the notification handler is called after 
// the source buffer has been copied to the data buffer.
//
// inBuffer: Pointer to the source buffer.
//
// size:     Size of the source buffer in bytes.
//
// Returns kIOReturnSuccess on success,
// kIOReturnBadArgument if an argument provided is invalid,
// kIOReturnNoSpace if the source buffer is too big,
// kIOReturnNotWritable if write access is not permitted,
// or an error from the notification handler.

IOReturn IONetworkData::write(void * inBuffer, UInt size)
{
    IOReturn ret = kIOReturnUnsupported;

    LOCK;

    do {
        // Check the arguments.
        //
        if (!inBuffer)
        {
            ret = kIOReturnBadArgument;
            break;
        }

        // Check access.
        //
        if ((_access & kIONDAccessTypeWrite) == 0)
        {
            ret = kIOReturnNotWritable;
            break;
        }

        // Update the data buffer.
        //
        if ((_access & kIONDAccessNoBufferAccess) == 0)
        {
            ret = (setBytes(inBuffer, size)) ?
                  kIOReturnSuccess : kIOReturnNoSpace;

            if (ret != kIOReturnSuccess)
                break;
        }

        // Notify the target after a successful write operation.
        //
        if (TAP_IS_VALID)
        {
            ret = (_tapTarget->*_tapAction)(this,
                                            (UInt32) kIONDAccessTypeWrite,
                                            _tapArg,
                                            inBuffer,
                                            (UInt32 *) &size);
        }
    }
    while (0);

    UNLOCK;

    return ret;
}

//---------------------------------------------------------------------------
// Serialize the IONetworkData object. If notification is enabled,
// then the notification handler is called before the data buffer is 
// serialized.
//
// s: An OSSerialize object.

bool IONetworkData::serialize(OSSerialize * s) const
{
    bool           ok;
    OSDictionary * dictToSerialize;
    OSData *       dataEntry;
    OSNumber *     numberEntry;

    dictToSerialize = OSDictionary::withCapacity(3);
    if (!dictToSerialize)
        return false;

    dataEntry = OSData::withBytesNoCopy((void *) &_access, sizeof(_access));
    if (dataEntry) {
        dictToSerialize->setObject(gIONDAccessKey, dataEntry);
        dataEntry->release();
    }

    numberEntry = OSNumber::withNumber(_capacity, sizeof(_capacity) * 8);
    if (numberEntry) {
        dictToSerialize->setObject(gIONDCapacityKey, numberEntry);
        numberEntry->release();
    }

    LOCK;

    do {
        // Check access.
        //
        if ((_access & kIONDAccessTypeSerialize) == 0)
            break;

        if (_access & kIONDAccessNoBufferAccess)
            break;

        // Notify the target before the read operation.
        // The target can take this opportunity to update the
        // data buffer. If the target returns an error,
        // then the data buffer is not serialized.
        //
        if (TAP_IS_VALID &&
            ((_tapTarget->*_tapAction)((IONetworkData *) this,
                                       kIONDAccessTypeSerialize,
                                       _tapArg, 0, 0) != kIOReturnSuccess))
        {
            break;
        }

        dataEntry = OSData::withBytesNoCopy(data, length);
        if (dataEntry) {
            dictToSerialize->setObject(gIONDDataKey, dataEntry);
            dataEntry->release();
        }
    }
    while (0);

    ok = dictToSerialize->serialize(s);
    dictToSerialize->release();

    UNLOCK;

    return ok;
}
