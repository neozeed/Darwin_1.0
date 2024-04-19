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
 *
 */

#include <IOKit/assert.h>
#include <IOKit/IOLib.h>
#include <IOKit/network/IONetworkController.h>
#include <IOKit/network/IONetworkInterface.h>
#include <IOKit/network/IONetworkUserClient.h>
#include <IOKit/network/IONetworkData.h>

//------------------------------------------------------------------------

#define super IOUserClient
OSDefineMetaClassAndStructors( IONetworkUserClient, IOUserClient )

#ifdef  DEBUG
#define DLOG(fmt, args...)  IOLog(fmt, ## args)
#else
#define DLOG
#endif

//---------------------------------------------------------------------------
// Factory method that performs allocation and initialization
// of an IONetworkUserClient instance.
//
// owningTask See IOUserClient.
//
// Returns an IONetworkUserClient on success, 0 otherwise.

IONetworkUserClient * IONetworkUserClient::withTask(task_t owningTask)
{
    IONetworkUserClient * me;

    me = new IONetworkUserClient;
    if (me) {
        if(!me->init()) {
            me->release();
            return 0;
        }
        me->_task = owningTask;
        me->_notifyPort = MACH_PORT_NULL;
    }
    return me;
}

//---------------------------------------------------------------------------
// Start the IONetworkUserClient.
// Open the provider, must be an IONetworkInterface object, and initialize
// the IOExternalMethod array.
//
// Returns true on success, false otherwise.

bool IONetworkUserClient::start(IOService * provider)
{
    UInt i;

    _owner = OSDynamicCast(IONetworkInterface, provider);
    assert(_owner);

    _ctlr = OSDynamicCast(IONetworkController, _owner->getProvider());
    if (!_ctlr)
        return false;

    if (!super::start(_owner))
        return false;

    if (!_owner->open(this))
        return false;

    // Initialize the call structures.
    //
    i = kIONUCResetNetworkDataIndex;
    _methods[i].object = this;
    _methods[i].func   = (IOMethod) &IONetworkUserClient::resetNetworkData;
    _methods[i].count0 = kIONUCResetNetworkDataInputs;
    _methods[i].count1 = kIONUCResetNetworkDataOutputs;
    _methods[i].flags  = kIONUCResetNetworkDataFlags;

    i = kIONUCWriteNetworkDataIndex;
    _methods[i].object = this;
    _methods[i].func   = (IOMethod) &IONetworkUserClient::writeNetworkData;
    _methods[i].count0 = kIONUCWriteNetworkDataInput0;
    _methods[i].count1 = kIONUCWriteNetworkDataInput1;
    _methods[i].flags  = kIONUCWriteNetworkDataFlags;

    i = kIONUCReadNetworkDataIndex;
    _methods[i].object = this;
    _methods[i].func   = (IOMethod) &IONetworkUserClient::readNetworkData;
    _methods[i].count0 = kIONUCReadNetworkDataInputs;
    _methods[i].count1 = kIONUCReadNetworkDataOutputs;
    _methods[i].flags  = kIONUCReadNetworkDataFlags;

    i = kIONUCGetNetworkDataCapacityIndex;
    _methods[i].object = this;
    _methods[i].func   = (IOMethod) 
                         &IONetworkUserClient::getNetworkDataCapacity;
    _methods[i].count0 = kIONUCGetNetworkDataCapacityInputs;
    _methods[i].count1 = kIONUCGetNetworkDataCapacityOutputs;
    _methods[i].flags  = kIONUCGetNetworkDataCapacityFlags;

    i = kIONUCGetNetworkDataHandleIndex;
    _methods[i].object = this;
    _methods[i].func   = (IOMethod) &IONetworkUserClient::getNetworkDataHandle;
    _methods[i].count0 = kIONUCGetNetworkDataHandleInputs;
    _methods[i].count1 = kIONUCGetNetworkDataHandleOutputs;
    _methods[i].flags  = kIONUCGetNetworkDataHandleFlags;

    return true;
}

//---------------------------------------------------------------------------
// Free the IONetworkUserClient instance.

void IONetworkUserClient::free()
{
    super::free();
}

//---------------------------------------------------------------------------
// Handle a client close. Close and detach from our owner (provider).
//
// Return kIOReturnSuccess.

IOReturn IONetworkUserClient::clientClose()
{
    if (_owner) {
        _owner->close(this);
        detach(_owner);
    }

    return kIOReturnSuccess;
}

//---------------------------------------------------------------------------
// Handle client death. Close and detach from our owner (provider).
//
// Return kIOReturnSuccess.

IOReturn IONetworkUserClient::clientDied()
{
    return clientClose();
}

//---------------------------------------------------------------------------
// Look up an entry from the method array. Called by IOUserClient to fetch
// the method entry, encoded by an IOExternalMethod structure, that correspond
// to the index given.
//
// index: The method index.
//
// Returns a pointer to a IOExternalMethod structure containing the 
// method definition.

IOExternalMethod *
IONetworkUserClient::getExternalMethodForIndex(UInt32 index)
{
    if (index >= kIONUCLastIndex)
        return 0;
    else
        return &_methods[index];
}

//---------------------------------------------------------------------------
// Called by a client to register its notification port.
//
// port:   A mach port where the notification message should be sent.
//
// type:   The type of notification that the client is interested in.
//
// refCon: An argument to deliver with the notification.
//
// Returns kIOReturnUnsupported if the notification type is unknown,
// kIOReturnSuccess otherwise. 

IOReturn IONetworkUserClient::registerNotificationPort(mach_port_t port,
                                                       UInt32      type,
                                                       UInt32      refCon)
{
    if (type != kIONUCNotificationTypeLinkChange)
        return kIOReturnUnsupported;
    
    _notifyPort = port;
    
    return kIOReturnSuccess;
}

//---------------------------------------------------------------------------
// Deliver a notification to the registered notification port.
//
// type: The type of notification being delivered.
//
// Returns kIOReturnSuccess on success,
//         kIOReturnUnsupported if the notification type is unknown, or 
//         kIOReturnError for a mach messaging error.

IOReturn IONetworkUserClient::deliverNotification(UInt32 type)
{
    IOReturn ret = kIOReturnSuccess;

    IONetworkNotifyMsg msg = {
    {
    MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND,0), /* mach_msg_bits_t msgh_bits */
    sizeof(IONetworkNotifyMsg),                /* mach_msg_size_t msgh_size */
    MACH_PORT_NULL,                         /* mach_port_t msgh_remote_port */
    MACH_PORT_NULL,                         /* mach_port_t  msgh_local_port */
    0,                                  /* mach_msg_size_t    msgh_reserved */
    type                                  /* mach_msg_id_t          msgh_id */
    }};

    if (type != kIONUCNotificationTypeLinkChange)
        return kIOReturnUnsupported;

    if (_notifyPort != MACH_PORT_NULL) {
        msg.h.msgh_remote_port = _notifyPort;

        kern_return_t r = mach_msg_send_from_kernel(
                                  (mach_msg_header_t *) &msg,
                                  msg.h.msgh_size);

        if (r != MACH_MSG_SUCCESS) {
            IOLog("%s: msg_send error: %x\n", getName(), (UInt) r);
            ret = kIOReturnError;
        }
    }

    return ret;
}

//---------------------------------------------------------------------------
// Fill the data buffer in an IONetworkData object with zeroes.
//
// key: The OSSymbol key of an IONetworkData object.
//
// Returns kIOReturnSuccess on success,
//         kIOReturnBadArgument if an argument is invalid, or an
//         error from IONetworkData::reset().

IOReturn IONetworkUserClient::resetNetworkData(OSSymbol * key)
{
    IONetworkData * data;
    IOReturn        ret;

    data = _owner->getNetworkData(key);
    ret = data ? data->reset() : kIOReturnBadArgument;

    return ret;
}

//---------------------------------------------------------------------------
// Write to the data buffer in an IONetworkData object from a buffer
// provided by the caller.
//
// key:    The OSSymbol key of an IONetworkData object.
//
// srcBuf: The data to write is taken from this buffer.
//
// inSize: The size of the source buffer.
//
// Returns kIOReturnSuccess on success,
//         kIOReturnBadArgument if an argument is invalid, or an 
//         error from IONetworkData::write().

IOReturn
IONetworkUserClient::writeNetworkData(OSSymbol *   key,
                                      void *       srcBuffer,
                                      IOByteCount  srcBufferSize)
{
    IONetworkData * data;
    IOReturn        ret;

    if (!srcBuffer || (srcBufferSize == 0))
        return kIOReturnBadArgument;

    data = _owner->getNetworkData(key);
    ret = data ? data->write(srcBuffer, srcBufferSize) : kIOReturnBadArgument;

    return ret;
}

//---------------------------------------------------------------------------
// Read (copy) the data buffer in an IONetworkData object to a buffer
// provided by the caller.
//
// key:        The OSSymbol key of an IONetworkData object.
//
// destBuf:    The buffer where the data shall be written to.
//
// inOutSizeP: Pointer to an integer that the caller must initialize
//             to contain the size of the buffer. This method will overwrite
//             it with the actual number of bytes written to the buffer.
//
// Returns kIOReturnSuccess on success,
//         kIOReturnBadArgument if an argument is invalid, or an
//         error from IONetworkData::read().

IOReturn
IONetworkUserClient::readNetworkData(OSSymbol *    key,
                                     void *        destBuf,
                                     IOByteCount * inOutSizeP)
{
    IONetworkData * data;
    IOReturn        ret ;

    if (!destBuf || !inOutSizeP)
        return kIOReturnBadArgument;

    data = _owner->getNetworkData(key);
    ret = data ? data->read(destBuf, (UInt *) inOutSizeP) : 
                 kIOReturnBadArgument;

    return ret;
}

//---------------------------------------------------------------------------
// Get the capacity of an IONetworkData object.
//
// key:       The OSSymbol key associated with the IONetworkData object.
//
// capacityP: On success, the capacity is written to the integer
//            at this address.
//
// Returns kIOReturnSuccess on success, or
//         kIOReturnBadArgument if an argument is invalid.

IOReturn
IONetworkUserClient::getNetworkDataCapacity(OSSymbol * key,
                                            UInt32 *   capacityP)
{
    IOReturn        ret = kIOReturnBadArgument;
    IONetworkData * data;

    data = _owner->getNetworkData(key);

    if (data) {
        *capacityP = data->getCapacity();
        ret = kIOReturnSuccess;
    }

    return ret;
}

//---------------------------------------------------------------------------
// Called to obtain a handle that maps to an IONetworkData object.
// This handle can be later passed to other methods in this class
// to refer to the same object.
//
// name:        A C string with the name of the IONetworkData object.
//
// handle:      If an IONetworkData object with the given name is found,
//              then its associated OSSymbol object is written to this address.
//
// nameSize:    The size of the name string, including the final
//              null character.
//
// handleSizeP: The size of the buffer allocated by the caller
//              to store the handle. This should be 4 bytes.
//
// Returns kIOReturnSuccess on success,
//         kIOReturnBadArgument if an argument is invalid, or
//         kIOReturnNoMemory if unable to allocate memory.

IOReturn
IONetworkUserClient::getNetworkDataHandle(char *         name,
                                          OSObject **    handle,
                                          IOByteCount    nameSize,
                                          IOByteCount *  handleSizeP)
{
    IOReturn         ret = kIOReturnBadArgument;
    const OSSymbol * key;

    if (!name || !nameSize || (name[nameSize - 1] != '\0') ||
        (*handleSizeP != sizeof(*handle)))
        return kIOReturnBadArgument;

    key = OSSymbol::withCStringNoCopy(name);
    if (!key)
        return kIOReturnNoMemory;

    if (_owner->getNetworkData(key))
    {
        *handle = (OSObject *) key;
        ret = kIOReturnSuccess;
    }

    if (key)
        key->release();

    return ret;
}
