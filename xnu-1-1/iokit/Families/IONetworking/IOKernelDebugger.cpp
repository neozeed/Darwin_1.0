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
 * Kernel debugger nub. This object interfaces with the KDP module and
 * dispatches KDP requests to its provider. The provider, named the
 * debugger device, must implement a pair of handler functions that are
 * called to handle KDP transmit and receive requests. A debugger lock,
 * allocated by the IOKernelDebugger, can be used by the debugger device
 * to block calls to its handler functions during critical sections.
 *
 * The debugger device is usually a subclass of IOEthernetController.
 * However, any IOService can attach an IOKernelDebugger client,
 * implement the two polled mode handlers, and transport the KDP
 * packets through a data channel. Having said this, the KDP assumes
 * that the debugger device is an Ethernet interface and therefore
 * it will always send, and expect to receive, an Ethernet frame.
 *
 * HISTORY
 */

#include <IOKit/assert.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOMessage.h>
#include <IOKit/network/IONetworkController.h>
#include <IOKit/network/IOKernelDebugger.h>

extern "C" {

// Defined in osfmk/kdp/kdp_en_debugger.h,
// but the header file is not exported,
// thus the definition is replicated here.

typedef void (*kdp_send_t)(void * pkt, UInt pkt_len);
typedef void (*kdp_receive_t)(void * pkt, UInt * pkt_len, UInt timeout);
void kdp_register_send_receive(kdp_send_t send, kdp_receive_t receive);
}

#define super IOService
OSDefineMetaClassAndStructorsWithInit(IOKernelDebugger,
                                      IOService,
                                      IOKernelDebugger::initialize())

// Debugger global variables.
//
IOSimpleLock *       gIODebuggerLock       = 0;
UInt32               gIODebuggerFlag       = 0;
IOService *          gIODebuggerDevice     = 0;
IOKernelDebugger *   gIODebuggerNub        = 0;
IODebuggerTxHandler  gIODebuggerTxHandler  = 0;
IODebuggerRxHandler  gIODebuggerRxHandler  = 0;
UInt32               gIODebuggerTxBytes    = 0;
UInt32               gIODebuggerRxBytes    = 0;
UInt32               gIODebuggerSemaphore  = 0;   // deprecated
IOInterruptState     gIODebuggerInterruptState;

// Flags to indicate debugger state.
// 
enum {
    kIODebuggerFlagEnabled     = 0x01,
    kIODebuggerFlagRegistered  = 0x02,
};

#define KDP_GRAB_LOCK        IOSimpleLockLock(gIODebuggerLock)
#define KDP_RELEASE_LOCK     IOSimpleLockUnlock(gIODebuggerLock)

#define IOTakeDebuggerLock(s)    \
        ((s) = IOSimpleLockLockDisableInterrupt(gIODebuggerLock))

#define IOReleaseDebuggerLock(s)  \
        IOSimpleLockUnlockEnableInterrupt(gIODebuggerLock, (s))

//---------------------------------------------------------------------------
// The KDP receive dispatch function. Handles KDP receive requests during
// a debugging session, then dispatches the call to the registered handler.
// This function is registered with KDP via kdp_register_send_receive().
//
// pkt: KDP receive buffer. The buffer allocated has room for 1518 bytes.
//      Never overflow the buffer!
//
// pkt_len: The amount of data placed into the receive buffer. Set to
//          0 if no frame was received during the timeout interval.
//
// timeout: The registered handler must poll for a maximum period of
//          Œtimeout¹ milliseconds while waiting for a frame to arrive.

void IOKernelDebugger::kdpReceiveDispatcher(void *  pkt,
                                            UInt *  pkt_len, 
                                            UInt    timeout)
{
    *pkt_len = 0;    // return length field is zero by default.

#ifdef __USE_DEBUGGER_LOCK
    KDP_GRAB_LOCK;

    // Warning: We are holding a simple_lock, the handler must not block.

    (*gIODebuggerRxHandler)(gIODebuggerDevice, pkt, pkt_len, timeout);
    gIODebuggerRxBytes += *pkt_len;

    KDP_RELEASE_LOCK;
#else
    if (gIODebuggerSemaphore)
        return;

    (*gIODebuggerRxHandler)(gIODebuggerDevice, pkt, pkt_len, timeout);
    gIODebuggerRxBytes += *pkt_len;
#endif
}

//---------------------------------------------------------------------------
// The KDP transmit dispatch function. Handles KDP transmit requests during
// a debugging session, then dispatches the call to the registered handler.
// This function is registered with KDP via kdp_register_send_receive().
//
// pkt: KDP transmit buffer. This buffer contains a KDP frame to be sent
//      on the wire.
//
// pkt_len: The number of bytes in the transmit buffer.

void IOKernelDebugger::kdpTransmitDispatcher(void * pkt, UInt pkt_len)
{
#ifdef __USE_DEBUGGER_LOCK
    KDP_GRAB_LOCK;

    // Warning: We are holding a simple_lock, the handler must not block.

    (*gIODebuggerTxHandler)(gIODebuggerDevice, pkt, pkt_len);
    gIODebuggerTxBytes += pkt_len;

    KDP_RELEASE_LOCK;
#else
    if (gIODebuggerSemaphore)
        return;

    (*gIODebuggerTxHandler)(gIODebuggerDevice, pkt, pkt_len);
    gIODebuggerTxBytes += pkt_len;
#endif
}

//---------------------------------------------------------------------------
// Null request handlers. Those gets registered when an IOKernelDebugger
// instance becomes detached from its debugger device, and before a new 
// debugger device takes over the debugging responsibilities.

void IOKernelDebugger::nullTxHandler(IOService * provider,
                                     void *      buffer,
                                     UInt        length)
{
    IOLog("IOKernelDebugger::%s no debugger device\n", __FUNCTION__);
}

void IOKernelDebugger::nullRxHandler(IOService * provider,
                                     void *      buffer,
                                     UInt *      length,
                                     UInt        timeout)
{
    IOLog("IOKernelDebugger::%s no debugger device\n", __FUNCTION__);
}

//---------------------------------------------------------------------------
// Take the debugger lock conditionally.
// The debugger lock is taken only if the provider given is the current
// registered debugger device. The debugger device is registered via the
// openProvider() method, and unregistered via closeProvider().
//
// Returns kIODebuggerLocked if the lock was taken, or
// 0 if the lock was not taken. 

IODebuggerLockState IOKernelDebugger::lock(IOService * provider)
{
#ifdef __USE_DEBUGGER_LOCK

    if (gIODebuggerDevice == provider)
    {
        IOInterruptState irqState;

        IOTakeDebuggerLock(irqState);

        if (gIODebuggerDevice == provider)
        {
            gIODebuggerInterruptState = irqState;
            return kIODebuggerLockTaken;
        }
        else {
            IOReleaseDebuggerLock(irqState);
        }
    }
    return (IODebuggerLockState) 0;

#else /* !__USE_DEBUGGER_LOCK */

    if (gIODebuggerDevice == provider)
    {
        if (gIODebuggerSemaphore)
            IOLog("IOKernelDebugger::lock: already locked\n");

        gIODebuggerSemaphore++;
        return kIODebuggerLockTaken;
    }
    return (IODebuggerLockState) 0;

#endif
}

//---------------------------------------------------------------------------
// Release the debugger lock if the kIODebuggerLockTaken flag is set.
//
// state: The saved return from a previous IOKernelDebugger::lock() call.

void IOKernelDebugger::unlock(IODebuggerLockState state)
{
#ifdef __USE_DEBUGGER_LOCK

    if (state & kIODebuggerLockTaken)
        IOReleaseDebuggerLock(gIODebuggerInterruptState);

#else /* !__USE_DEBUGGER_LOCK */

    if (state & kIODebuggerLockTaken)
        gIODebuggerSemaphore--;

#endif
}

//---------------------------------------------------------------------------
// IOKernelDebugger class initializer. The debugger lock is allocated and
// initialized.

void IOKernelDebugger::initialize()
{
    // Allocate and initialize the debugger lock (simple_lock).

    gIODebuggerLock = IOSimpleLockAlloc();
    assert(gIODebuggerLock);
    IOSimpleLockInit(gIODebuggerLock);
}

//---------------------------------------------------------------------------
// The IOKernelDebugger initializer.
//
// provider:  The provider of the IOKernelDebugger object.
// txHandler: The transmit handler. A pointer to a C function.
// rxHandler: The receive handler. A pointer to a C function.
//
// Returns true if the instance initialized successfully, false otherwise.

bool IOKernelDebugger::init(IOService *          provider,
                            IODebuggerTxHandler  txHandler,
                            IODebuggerRxHandler  rxHandler)
{
    if (!OSDynamicCast(IOService, provider) || !txHandler || !rxHandler)
        return false;

    if (!super::init())
        return false;

    // Register the provider and handlers provided.

    _provider  = provider;
    _txHandler = txHandler;
    _rxHandler = rxHandler;

    return true;
}

//---------------------------------------------------------------------------
// A factory method that performs allocation and initialization.
//
// provider:  The provider of the IOKernelDebugger object.
// txHandler: The transmit handler. A pointer to a C function.
// rxHandler: The receive handler. A pointer to a C function.
//
// Returns an IOKernelDebugger instance on success, 0 otherwise.

IOKernelDebugger * IOKernelDebugger::debugger(IOService *          provider,
                                              IODebuggerTxHandler  txHandler,
                                              IODebuggerRxHandler  rxHandler)
{
    IOKernelDebugger * debugger = new IOKernelDebugger;

    if (debugger && !debugger->init(provider, txHandler, rxHandler))
    {
        debugger->release();
        debugger = 0;
    }

    return debugger;
}

//---------------------------------------------------------------------------
// Open the attached provider, register the dispatch and handler functions.
//
// provider: The provider object. This object is registered as the debugger
//           device.
//
// Returns true on sucess, or false if the controller open failed, or there
// is a previously registered IOKernelDebugger instance.

bool IOKernelDebugger::openProvider(IOService * provider)
{
    bool              ret = false;
    bool              doOpen;
    bool              wasOpen;
    bool              doRegistration = false;
    IOInterruptState  state;

    IOTakeDebuggerLock(state);
    if (gIODebuggerNub)
    {
        // Debugger nub already registered. Sorry, only
        // a single debugger nub at any given time.

        doOpen = false;
    }
    else
    {
        assert(gIODebuggerDevice == 0);
        assert((gIODebuggerFlag & kIODebuggerFlagEnabled) == 0);
        assert(provider == _provider);

        // Make reservation.

        gIODebuggerNub = this;
        doOpen         = true;
    }
    IOReleaseDebuggerLock(state);

    // If doOpen is true, then proceed and open the provider,
    // otherwise we were unable to make reservation, so
    // we give up and return false.

    do {
        if (!doOpen) break;

        wasOpen = provider->open(this);

        // Take the debugger lock again and commit the reservation.
        // Make sure nothing has happened to cause us to loose the
        // initial reservation.

        IOTakeDebuggerLock(state);

        if (wasOpen && (gIODebuggerNub == this))
        {
            gIODebuggerTxHandler  = _txHandler;
            gIODebuggerRxHandler  = _rxHandler;
            gIODebuggerDevice     = provider;
            gIODebuggerFlag      |= kIODebuggerFlagEnabled;

            if ((gIODebuggerFlag & kIODebuggerFlagRegistered) == 0)
            {
                // Our static dispatch functions have not yet been
                // registered with kdp, set doRegistration to
                // perform registration after releasing the lock.
                // The act of registering may trigger kdp to call
                // our dispatch functions, so we should not be
                // holding the debugger lock.

                gIODebuggerFlag |= kIODebuggerFlagRegistered;
                doRegistration = true;
            }
        }
        else
        {
            // Either we were unable to open provider,
            // or perhaps we lost reservation.

            if (gIODebuggerNub == this)
            {
                // Unable to open provider, remove our
                // reservation.

                gIODebuggerNub = 0;
            }
            IOReleaseDebuggerLock(state);
            break;
        }

        IOReleaseDebuggerLock(state);

        // NOTE: Once the provider has accepted an open from this
        // object, it must be prepared to handle kdp requests
        // immediately after its open method returns.
        //
        // Register the dispatch functions, that will route the
        // kdp request to our provider¹s registered handlers.

        if (doRegistration)
            kdp_register_send_receive(kdpTransmitDispatcher,
                                      kdpReceiveDispatcher);

        ret = true;        // success
    }
    while (0);

    if (doOpen && !ret)
    {
        // Make sure the provider is closed.    
        provider->close(this);
    }

    return ret;
}

//---------------------------------------------------------------------------
// Close our provider, and unregister the handler functions provided to
// init(). Another IOKernelDebugger instance that comes along will be
// able to assume the debugging responsibilities.
//
// provider: The provider object. This object is unregistered as the
//           debugger device.

void IOKernelDebugger::closeProvider(IOService * provider)
{
    bool              doClose = false;
    IOInterruptState  state;

    IOTakeDebuggerLock(state);

    if (gIODebuggerNub == this)
    {
        // There is no kdp unregistration. Thus setup the handlers
        // to point to dummy functions to avoid a system panic
        // when the debugger device gives up its responsibilities
        // and the debugger is activated.
        // Until the next debugger device comes along, nothing will
        // happen when the debugger is activated, which is not good.

        gIODebuggerFlag      &= ~kIODebuggerFlagEnabled;
        gIODebuggerDevice     = 0;
        gIODebuggerNub        = 0;
        gIODebuggerTxHandler  = &IOKernelDebugger::nullTxHandler;
        gIODebuggerRxHandler  = &IOKernelDebugger::nullRxHandler;
        doClose               = true;
    }
    IOReleaseDebuggerLock(state);

    if (doClose)
        provider->close(this);
}

//---------------------------------------------------------------------------
// Attach to our provider, then call openProvider().
//
// provider: The provider object.
//
// Returns true on success, false otherwise.

bool IOKernelDebugger::attach(IOService * provider)
{
    bool ret = false;

    assert(provider == _provider);

    if (!super::attach(provider))
        return false;        // cannot attach to provider.

    ret = openProvider(provider);

    return ret;
}

//---------------------------------------------------------------------------
// Call closeProvider() then detach from our provider.
//
// provider: The provider object.

void IOKernelDebugger::detach(IOService * provider)
{
//  IOLog("IOKernelDebugger::%s provider:%x\n", __FUNCTION__, (UInt) provider);

    assert(provider == _provider);

    closeProvider(provider);

    super::detach(provider);
}

//---------------------------------------------------------------------------
// Final termination notification. Ensure that the closeProvider() is
// called before the object is terminated.
//
// options: termination options, not used.

bool IOKernelDebugger::finalize(IOOptionBits options)
{
//  IOLog("IOKernelDebugger::%s options:%x\n", __FUNCTION__, (UInt) options);

    closeProvider(_provider);
    return super::finalize(options);
}

//---------------------------------------------------------------------------
// Facility provided by IOService for general purpose provider-to-client 
// notification. We catch the kIOMessageServiceIsTerminated message to
// detect when our provider becomes inactive, and call closeProvider().
//
// type: message tupe.
// provider: The provider object.
// argument: message argument. Not used.
//
// Returns kIOReturnSuccess for kIOMessageServiceIsTerminated messages,
// kIOReturnUnsupported otherwise.

IOReturn IOKernelDebugger::message(UInt32       type,
                                   IOService *  provider,
                                   void *       argument = 0)
{
//  IOLog("IOKernelDebugger::%s type:%x provider:%x\n",
//        __FUNCTION__, (UInt) type, (UInt) provider);

    assert(provider == _provider);

    if (type == kIOMessageServiceIsTerminated)
    {
        // Close our provider when it becomes inactive.

        closeProvider(provider);
        return kIOReturnSuccess;
    }

    return kIOReturnUnsupported;
}

//---------------------------------------------------------------------------
// Free the IOKernelDebugger instance.

void IOKernelDebugger::free()
{
//  IOLog("IOKernelDebugger::%s %08lx\n", __FUNCTION__, (UInt32) this);
    super::free();
}
