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
 * IOKernelDebugger.cpp
 *
 * HISTORY
 *
 */

#ifndef _IOKERNELDEBUGGER_H
#define _IOKERNELDEBUGGER_H

// #define __USE_DEBUGGER_LOCK  1    // XXX - not yet!!!

#include <IOKit/IOService.h>


/*! @typedef IODebuggerRxHandler
    @discussion Defines the receive handler that must be implemented
    by the provider to service KDP requests. This handler is called
    by the dispatch function in IOKernelDebugger.
    @param provider The provider object.
    @param buffer KDP receive buffer. The buffer allocated has room for
           1518 bytes. Never overflow the buffer!
    @param length The amount of data placed into the receive buffer
           by teh receive handler. Set to 0 if no frame was received
           during the poll interval.
    @param timeout Poll for a maximum period of 'timeout' milliseconds
           while waiting for a frame to arrive. */

typedef void (*IODebuggerRxHandler)(IOService * provider,
                                    void *      buffer,
                                    UInt *      length,
                                    UInt        timeout);

/*! @typedef IODebuggerTxHandler
    @discussion Defines the transmit handler that must be implemented
    by the provider to service KDP requests. This handler is called
    by the dispatch function in IOKernelDebugger.
    @param provider The provider object.
    @param buffer KDP transmit buffer. This buffer contains a KDP frame
           to be sent on the wire.
    @param length The number of bytes in the transmit buffer. */

typedef void (*IODebuggerTxHandler)(IOService * provider,
                                    void *      buffer,
                                    UInt        length);

/*! @typedef IODebuggerLockState
    @discussion Defines the return from IOKernelDebugger::lock.
    @constant kIODebuggerLocked Set if the debugger lock was taken.
	          Otherwise, the debugger lock was not taken. */

typedef enum {
    kIODebuggerLockTaken = 0x1,
} IODebuggerLockState;

/*! class IOKernelDebugger : public IOService
    @abstract Kernel debugger nub.
    @discussion This object interfaces with the KDP module and
    dispatches KDP requests to its provider. The provider, designated the
    debugger device, must implement a pair of handler functions that are
    called to handle KDP transmit and receive requests. A debugger lock,
    allocated by the IOKernelDebugger, can be used by the debugger device
    to block calls to its handler functions during critical sections.
    Only a single IOKernelDebugger can be registered at any given time.

    The debugger device is usually a subclass of IOEthernetController.
    However, any IOService can attach an IOKernelDebugger client,
    implement the two polled mode handlers, and transport the KDP
    packets through a data channel. Having said this, the KDP assumes
    that the debugger device is an Ethernet interface and therefore
    it will always send, and expect to receive, an Ethernet frame. */

class IOKernelDebugger : public IOService
{
    OSDeclareDefaultStructors(IOKernelDebugger)

protected:
    IOService *            _provider;      // debugger device.
    IODebuggerTxHandler    _txHandler;     // provider's transmit handler.
    IODebuggerRxHandler    _rxHandler;     // provider's receive handler.

/*! @function initialize
    @discussion IOKernelDebugger class initializer.
    The debugger lock is allocated and initialized. */

    static void initialize();

/*! @function kdpReceiveDispatcher
    @abstract The KDP receive dispatch function.
    @discussion Handles KDP receive requests during a debugging session,
    then dispatches the call to the registered receiver handler. This
    function is registered with KDP via kdp_register_send_receive().
    @param pkt KDP receive buffer. The buffer allocated has room for
           1518 bytes. Never overflow the buffer!
    @param pkt_len The amount of data placed into the receive buffer.
           Set to 0 if a frame was not received during the poll interval.
    @param timeout The registered handler must poll for a maximum period of
           'timeout' milliseconds while waiting for a frame to arrive. */

    static void kdpReceiveDispatcher(void * pkt, UInt * pkt_len, UInt timeout);

/*! @function kdpTransmitDispatcher
    @abstract The KDP transmit dispatch function.
    @discussion Handles KDP transmit requests during a debugging session,
    then dispatches the call to the registered transmit handler. This
    function is registered with KDP via kdp_register_send_receive().
    @param pkt KDP transmit buffer. This buffer contains a KDP frame to be
           sent on the wire.
    @param pkt_len The number of bytes in the transmit buffer. */

    static void kdpTransmitDispatcher(void * pkt, UInt pkt_len);

/*! @function free
    @discussion Free the IOKernelDebugger instance. Used for debugging.
    Log a message then call super::free(). */

    virtual void free();

/*! @function openProvider
    @discussion Open the attached provider, register it as the debugger
    device, and register the dispatch and handler functions.
    @param provider The provider object. This object is registered as the
           debugger device.
    @result true on sucess, or false if the controller open failed, or an
    IOKernelDebugger instance has already been registered. */

    bool openProvider(IOService * provider);

/*! @function closeProvider
    @discussion Close our provider, and undo the registration performed in
    openProvider(). Another IOKernelDebugger instance that comes along will
    be able to take over the debugging responsibilities.
    provider: The provider object. */

    void closeProvider(IOService * provider);

/*! @function nullTxHandler
    @abstract Null transmit handler.
    @discussion This function is registered as the transmit handler by
    closeProvider() after the provider's handler is unregistered. This
    function does nothing except to log a warning message. */

    static void nullTxHandler(IOService * provider,
                              void *      buffer,
                              UInt        length);

/*! @function nullRxHandler
    @abstract Null receive handler.
    @discussion This function is registered as the receive handler by
    closeProvider() after the provider's handler is unregistered. This
    function does nothing except to log a warning message. */

    static void nullRxHandler(IOService * provider,
                              void *      buffer,
                              UInt *      length,
                              UInt        timeout);

public:

/*! @function lock
    @discussion Take the debugger lock conditionally. The debugger lock
    is taken only if the provider given is the current registered
    debugger device. The debugger device is registered via the
    openProvider() method, and unregistered via closeProvider().
    @param provider The caller of the lock function.
    @result kIODebuggerLocked if the lock was taken, or kIODebuggerUnlocked
    if the lock was not taken. */

    static IODebuggerLockState   lock(IOService * provider);

/*! @function unlock
    @discussion Release the debugger lock if the argument is
    kIODebuggerLocked.
    @param state The return from the IOKernelDebugger::lock(). */

    static void             unlock(IODebuggerLockState state);

/*! @function init
    @discussion The IOKernelDebugger initializer.
    @param provider  The provider of the IOKernelDebugger object.
    @param txHandler The provider's transmit handler. A pointer to a C 
           function.
    @param rxHandler The provider's receive handler. A pointer to a C function.
    @result true if the instance initialized successfully, false otherwise. */

    virtual bool init(IOService *         provider,
                      IODebuggerTxHandler txHandler,
                      IODebuggerRxHandler rxHandler);

/*! @function debugger
    @discussion A factory method that performs allocation and initialization.
    @param provider  The provider of the IOKernelDebugger object.
    @param txHandler The provider's transmit handler. A pointer to a C 
           function.
    @param rxHandler The provider's receive handler. A pointer to a C function.
    @result An IOKernelDebugger instance on success, 0 otherwise. */

    static IOKernelDebugger * debugger(IOService *         provider,
                                       IODebuggerTxHandler txHandler,
                                       IODebuggerRxHandler rxHandler);

/*! @function attach
    @discussion Attach to our provider, then call openProvider().
    @param provider The provider object.
    @result true on success, false otherwise. */

    virtual bool attach(IOService * provider);

/*! @function detach
    @discussion Call closeProvider(), then detach from our provider.
    @param provider The provider object. */

    virtual void detach(IOService * provider);

/*! @function finalize
    @discussion Final termination notification. To ensure that closeProvider()
    is called before the object is terminated.
    @param options termination options, not used. */

    virtual bool finalize(IOOptionBits options = 0);

/*! @function message
    @discussion Facility provided by IOService for general purpose
    provider-to-client notification. We catch the kIOMessageServiceIsTerminated
    message to detect when our provider becomes inactive, and call
    closeProvider().
    @param type message tupe.
    @param provider The provider object.
    @param argument message argument. Not used.
    @result kIOReturnSuccess for kIOMessageServiceIsTerminated messages,
    kIOReturnUnsupported otherwise. */

    virtual IOReturn message(UInt32      type,
                             IOService * provider,
                             void *      argument = 0);
};

// Shortened versions of the lock/unlock static functions.
//
#define IODebuggerLock    IOKernelDebugger::lock
#define IODebuggerUnlock  IOKernelDebugger::unlock

#endif /* !_IOKERNELDEBUGGER_H */
