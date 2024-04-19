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
 * IONetworkController.cpp
 *
 * HISTORY
 * 9-Dec-1998       Joe Liu (jliu) created.
 *
 */

#include <IOKit/assert.h>
#include <IOKit/IOCommandGate.h>
#include <IOKit/network/IONetworkController.h>
#include <IOKit/network/IOOutputQueue.h>
#include <IOKit/network/IONetworkMedium.h>

// IONetworkController needs to know about mbufs, but it shall have no
// other dependencies on BSD networking.
//
extern "C" {
#include <sys/param.h>  // mbuf limits defined here.
#include <sys/mbuf.h>
//
// osfmk/kern/spl.h - Need splimp for mbuf macros.
//
typedef unsigned spl_t;
extern spl_t    (splimp)(void);
}

//-------------------------------------------------------------------------
// Macros.

#define super IOService

OSDefineMetaClass(         IONetworkController, IOService )
OSDefineAbstractStructors( IONetworkController, IOService )

// Define SYNC_REQ macro to simplify syncRequest() calls.
//
#define SYNC_REQ(sender, action, ret, args...)  \
        syncRequest(sender, this, (IONetworkAction) action, (UInt32 *) ret, \
                    ## args)

#define DO_SYNC_REQ(action, args...)                                 \
{                                                                    \
    IOReturn  ret = kIOReturnNotReady;                               \
    syncRequest(client, this,                                        \
                (RequestAction) &IONetworkController:: ## action,    \
                (UInt32 *) &ret, ## args);                           \
    return ret;                                                      \
} 

static bool isPowerOfTwo(UInt num)
{
    return (num == (num & ~(num - 1)));
}

#ifndef MAX
#define MAX(a, b)   (((a)>(b))?(a):(b))
#endif

#define MEDIUM_LOCK     IOTakeLock(_mediumLock);
#define MEDIUM_UNLOCK   IOUnlock(_mediumLock);

#ifdef  DEBUG
#define DLOG(fmt, args...)  IOLog(fmt, ## args)
#else
#define DLOG
#endif

// OSSymbols for frequently used keys.
//
static const OSSymbol * gIOActiveMediumKey;
static const OSSymbol * gIOCurrentMediumKey;

//---------------------------------------------------------------------------
// Class initializer for IONetworkController. Create OSSymbol objects
// ahead of time that are used as keys. This method is called explicitly
// by a line in IOStartIOKit.cpp and not by the OSDefineMetaClassAndInit()
// mechanism, since the OSSymbol class is not guaranteed to be initialized
// first, thus its OSSymbol pool may not be setup.

void IONetworkController::initialize()
{
    gIOActiveMediumKey  = OSSymbol::withCStringNoCopy(kIOActiveMedium); 
    gIOCurrentMediumKey = OSSymbol::withCStringNoCopy(kIOCurrentMedium);
    
    assert(gIOActiveMediumKey && gIOCurrentMediumKey);

    IONetworkData::initialize();
}

//---------------------------------------------------------------------------
// Initialize the IONetworkController instance. Instance variables are 
// initialized to their default values, and the init() method in superclass
// is called.
//
// properties: A property table.
//
// Returns true on success, false otherwise.

bool IONetworkController::init(OSDictionary * properties)
{
    // Initialize instance variables.
    //
    _workLoop           = 0;
    _cmdGate            = 0;
    _outputQueue        = 0;
    _clientSet          = 0;
    _reqClient          = 0;
    _requestEnabled     = true;
    _handleReady        = true;
    _mediumLock         = 0;
    _linkData           = 0;

    if (!super::init(properties))
    {
        DLOG("IONetworkController: super::init() failed\n");
        return false;
    }
    return true;
}

//-------------------------------------------------------------------------
// Called when the controller was matched to a provider and
// has been selected to start running. IONetworkController will allocate
// resources and gather the controller's properties. No I/O will be
// triggered until the subclass attaches a client object from its own
// start method. Subclasses must override this method and call
// super::start() at the beginning of its implementation. They should
// also check the return value to make sure their superclass was
// started successfully. The resources allocated
// by IONetworkController are:
//
// - An IOWorkLoop object.
// - An IOCommandGate object to handle synchronous requests.
// - An OSSet to track our clients.
// - An optional IOOutputQueue object for output queueing.
//
// Tasks that are usually performed by a typical network driver in start
// include:
//
// - Resource allocation
// - Hardware initialization
// - Allocation of IOEventSources and attaching them to an IOWorkLoop object.
// - Publishing a medium dictionary.
// - And finally, attaching an interface object when the driver is ready
//   to handle client requests.
//
// provider: The provider that the controller was matched
//           (and attached) to.
//
// Returns true on success, false otherwise.

bool IONetworkController::start(IOService * provider)
{
    // Most drivers will probably want to wait for BSD due to their
    // dependency on mbufs, which is not available until BSD is
    // initialized.
    //
    if ((getFeatureSet() & kIONetworkFeatureNoBSDWait) == 0)
        waitForService(resourceMatching( "IOBSD" ));

    // Start our superclass.
    //
    if (!super::start(provider))
        return false;

    // Create an OSSet to store our clients.
    //
    _clientSet = OSSet::withCapacity(2);
    if (!_clientSet)
        return false;

    // Initialize link status properties.
    //
    setMediumProperty(gIOActiveMediumKey,  0, &_activeMedium,  true);
    setMediumProperty(gIOCurrentMediumKey, 0, &_currentMedium, true);
    setLink32Property(kIOLinkStatus,       0, &_linkStatus,    true);
    setLink64Property(kIOLinkSpeed,        0, &_linkSpeed,     true);
            
    // Allocate a mutex lock to serialize access to the medium dictionary.
    //
    _mediumLock = IOLockAlloc();
    if (!_mediumLock)
        return false;
    IOLockInitWithState(_mediumLock, kIOLockStateUnlocked);

    // Allocate and initialize the default IOWorkLoop object.
    //
    _workLoop = IOWorkLoop::workLoop();
    if (!_workLoop)
    {
        DLOG("%s: IOWorkLoop allocation failed\n", getName());
        return false;
    }

    // Create a 'private' IOCommandGate object and attach it to
    // our workloop created above. This is used by the syncRequest()
    // function.
    //
    _cmdGate = IOCommandGate::commandGate(this);
    if (!_cmdGate ||
        (_workLoop->addEventSource(_cmdGate) != kIOReturnSuccess))
    {
        DLOG("%s: IOCommandGate initialization failed\n", getName());
        return false;
    }

    // Try to allocate an IOOutputQueue instance. This is optional and
    // _outputQueue may be 0.
    //
    _outputQueue = createOutputQueue();

    // Remove this when allocateOutputQueue is deleted.
    if (!_outputQueue) 
        _outputQueue = allocateOutputQueue();

    // Query the controller's mbuf buffer restrictions.
    //
    IOPacketBufferConstraints constraints;
    getPacketBufferConstraints(&constraints);
    if ((constraints.alignStart  > kIOPacketBufferAlign32) ||
        (constraints.alignLength > kIOPacketBufferAlign32) ||
        !isPowerOfTwo(constraints.alignStart) ||
        !isPowerOfTwo(constraints.alignLength))
    {
        IOLog("%s: Invalid alignment: start:%ld, length:%ld\n",
            getName(),
            constraints.alignStart,
            constraints.alignLength);
        return false;
    }

    // Make it easier to satisfy both constraints.
    //
    if (constraints.alignStart < constraints.alignLength)
        constraints.alignStart = constraints.alignLength;
    
    // Convert to alignment masks.
    //
    _alignStart  = (constraints.alignStart) ? constraints.alignStart - 1 : 0;
    _alignLength = (constraints.alignLength) ? constraints.alignLength - 1 : 0;
    _alignPadding = _alignStart + _alignLength;

    // Starts the power manager
    //
    PMinit();           // initialize superclass power management variables
    provider->joinPMtree(this); // attach into the power management hierarchy

    return true;
}

//---------------------------------------------------------------------------
// The opposite of start(). The controller has been instructed to stop running.
// This method should release resources and undo actions performed by start().
// Subclasses must override this method and call super::stop() at the end of 
// its implementation.
//
// provider: The provider that the controller was matched
//           (and attached) to.

void IONetworkController::stop(IOService * provider)
{
    // Free the memory allocated by PMInit:
    PMstop();

    super::stop(provider);
}   

//---------------------------------------------------------------------------
// Get the IOWorkLoop object created by IONetworkController.
// An IOWorkLoop object is created by the start() method. Drivers can call 
// getWorkLoop() to obtain a reference to the IOWorkLoop object, and attach
// their event sources, such as IOTimerEventSource or IOInterruptEventSource.
// See IOWorkLoop.
//
// Returns the IOWorkLoop object created by IONetworkController.

IOWorkLoop * IONetworkController::getWorkLoop() const
{
    return _workLoop;
}

//---------------------------------------------------------------------------
// Get the IOCommandGate object created by IONetworkController.
// An IOCommandGate is created and attached to an IOWorkLoop by the start().
// method This IOCommandGate object is used to handle client requests issued
// through the syncRequest() method. Subclasses that need an IOCommandGate 
// should try to use the object returned by this method, rather than creating
// a new instance.
// See IOCommandGate.
//
// Returns the IOCommandGate object created by IONetworkController.
    
IOCommandGate * IONetworkController::getCommandGate() const
{
    return _cmdGate;
}

//---------------------------------------------------------------------------
// Get the address of the method designated to handle output packets.
//
// Returns the address of the outputPacket() method.

IOOutputAction IONetworkController::getOutputHandler() const
{
    return (IOOutputAction) &IONetworkController::outputPacket;
}

//---------------------------------------------------------------------------
// Create a new interface client object and attach it
// to the controller. The createInterface() method is called to
// perform the allocation and initialization, followed by a call to 
// configureInterface() to configure the interface. Both of these
// methods can be overridden by subclasses to customize the
// interface client attached. Drivers must call attachInterface()
// from its start() method, after it is ready to service client requests.
//
// interfaceP: If successful (return value is true), then the interface
//             object will be written to the handle provided.
//
// doRegister: If true, then registerService() is called to register
//             the interface, which will trigger the matching process,
//             and cause the interface to become registered with the network
//             layer. For drivers that wish to delay the registration, and
//             hold off servicing requests and data packets from the network
//             layer, set doRegister to false and call registerService() on
//             the interface client when the controller becomes ready.
//
// Returns true on success, false otherwise.

bool
IONetworkController::attachInterface(IONetworkInterface ** interfaceP,
                                     bool  doRegister = true)
{
    IONetworkInterface * netif;
    bool                 initOk = false;

    *interfaceP = 0;

    // We delay some initialization until the first time that
    // attachInterface() is called by the subclass.
    //
    SYNC_REQ(this, &IONetworkController::_controllerIsReady, &initOk);
    if (!initOk)
        return false;

    do {
        // Allocate a concrete subclass of IONetworkInterface
        // by calling createInterface().
        //
        netif = createInterface();
        if (!netif)
            break;

        // Configure the interface instance by calling 
        // configureInterface(), then attach it as our client.
        //
        if (!configureInterface(netif) ||
            !configureNetworkInterface(netif) ||
            !netif->attach(this))
        {
            netif->release();
            break;
        }

        *interfaceP = netif;

        // Register the interface nub. Spawns a matching thread.
        //
        if (doRegister)
            netif->registerService();

        return true;
    }
    while (0);

    return false;   // failed
}

//---------------------------------------------------------------------------
// Detach the interface object. This method will check that
// the object provided is indeed an IONetworkInterface, and if so its 
// terminate() method is called. The interface object will close and detach 
// from its controller only after the network layer has removed all references
// to the data structures exposed by the interface.
//
// interface: An interface object to be detached.
// sync:      If true, the interface is terminated synchronously.
//            Note that this may cause detachInterface() to block
//            for an indefinite period of time.

void
IONetworkController::detachInterface(IONetworkInterface * interface,
                                     bool                 sync = false)
{
    IOOptionBits options = kIOServiceRequired;

    if (OSDynamicCast(IONetworkInterface, interface) == 0)
        return;

    if (sync)
        options |= kIOServiceSynchronous;

    interface->terminate(options);
}

//---------------------------------------------------------------------------
// This method is called the first time that a controller driver calls 
// attachInterface() or attachDebuggerClient(), which is an indication that
// the driver has been started and is ready to service client requests.
// IONetworkController  uses this method to complete its initialization
// before any client objects are attached.
//
// provider: The controller's provider.
//
// Returns true on success, false otherwise.

bool IONetworkController::ready(IOService * provider)
{
    return publishCapabilities();
}

//---------------------------------------------------------------------------
// Called when the controller is ready to handle client requests.
// Returns true to indicate success, false otherwise.

bool IONetworkController::_controllerIsReady()
{
    if (!_handleReady)
        return true;

    if (!ready(getProvider()))    // Call ready().
        return false;

    _handleReady = false;
    return true;
}

//---------------------------------------------------------------------------
// Handle a client open on the controller object. IOService calls this method 
// with the arbitration lock held. Subclasses are not expected to override
// this method.
//
//   client: The client that is trying to open the controller.
//  options: See IOService.
// argument: See IOService.
//
// Returns true to accept the client open, false to refuse the open.

bool IONetworkController::handleOpen(IOService *  client,
                                     IOOptionBits options,
                                     void *       argument)
{
    bool ok;

    assert(client);

    ok = _clientSet->setObject(client);
    if (ok && OSDynamicCast(IOKernelDebugger, client))
    {
        ok = (doEnable(client) == kIOReturnSuccess);
        if (!ok)
            _clientSet->removeObject(client);
    }
    return ok;
}

//---------------------------------------------------------------------------
// Handle a close from one of our client objects. IOService calls this method
// with the arbitration lock held. Subclasses are not expected to override this 
// method.
//
//  client: The client that has closed the controller.
// options: See IOService.

void IONetworkController::handleClose(IOService * client, IOOptionBits options)
{
    _clientSet->removeObject(client);

    if (OSDynamicCast(IOKernelDebugger, client))
        doDisable(client);
}

//---------------------------------------------------------------------------
// This method is always called by IOService with the arbitration lock held. 
// Subclasses should not override this method.
//
// Returns true if the specified client, or any client if none is
// specified, presently has an open on this object.

bool IONetworkController::handleIsOpen(const IOService * client) const
{
    if (client)
        return _clientSet->containsObject(client);
    else
        return (_clientSet->getCount() > 0);
}

//---------------------------------------------------------------------------
// Free the IONetworkController instance and all allocated resources,
// then call super::free().

void IONetworkController::free()
{
    if (_outputQueue)
        _outputQueue->release();

    if (_cmdGate)
        _cmdGate->release();

    if (_workLoop)
        _workLoop->release();

    if (_clientSet)
    {
        // We should have no clients at this point. If we do,
        // then something is very wrong! It means that a client
        // has an open on us, and yet we are going away.
        //
        assert(_clientSet->getCount() == 0);
        _clientSet->release();
    }

    if (_mediumLock)
        IOLockFree(_mediumLock);

    super::free();
}

//---------------------------------------------------------------------------
// Handle an enable request from a client. The client's class
// is typecasted using OSDynamicCast, and if the client is an
// IOKernelDebugger or an IONetworkInterface, an overloaded enable
// method that takes a more specific argument is called. If the client
// matches neither type, a kIOReturnBadArgument is returned.
// A driver has the option of override this generic enable method,
// or the derived version.
//
// client: The client object requesting the enable.
//
// The return value from the derived enable method, or
// kIOReturnBadArgument if the client's type is unknown.

IOReturn IONetworkController::enable(IOService * client)
{
    if (OSDynamicCast(IONetworkInterface, client))
        return enable((IONetworkInterface *) client);
    
    if (OSDynamicCast(IOKernelDebugger, client))
        return enable((IOKernelDebugger *) client);

    IOLog("%s::%s Unknown client type\n", getName(), __FUNCTION__);
    return kIOReturnBadArgument;
}

//---------------------------------------------------------------------------
// Handle an enable request from a client. The client
// object is typecasted using OSDynamicCast, and if the client is an
// IOKernelDebugger or an IONetworkInterface, then an overloaded disable
// method that takes a more specific argument is called. If the client
// matches neither type, a kIOReturnBadArgument is returned.
// A driver has the option of override this generic enable method,
// or the derived version.
//
// client: The client object requesting the disable.
//
// The return from the derived disable method, or
// kIOReturnBadArgument if the client's type is unknown

IOReturn IONetworkController::disable(IOService * client)
{
    if (OSDynamicCast(IONetworkInterface, client))
        return disable((IONetworkInterface *) client);
    
    if (OSDynamicCast(IOKernelDebugger, client))
        return disable((IOKernelDebugger *) client);

    IOLog("%s::%s Unknown client type\n", getName(), __FUNCTION__);
    return kIOReturnBadArgument;
}

//---------------------------------------------------------------------------
// Called by an interface client to enable the controller.
// This method must bring up the hardware and enable event sources
// to prepare for packet transmission and reception. A driver should
// delay the allocation of most runtime resources until this method is
// called to conserve shared system resources.
//
// interface: The interface object that requested the enable.
//
// Returns kIOReturnUnsupported. Driver may override this method and
// return kIOReturnSuccess on success, or an error code otherwise.

IOReturn IONetworkController::enable(IONetworkInterface * interface)
{
    IOLog("IONetworkController::%s called\n", __FUNCTION__);
    return kIOReturnUnsupported;
}

//---------------------------------------------------------------------------
// Called by an interface object to disable the controller.
// This method should quiesce the hardware and disable event sources.
// Any resources allocated in enable() should also be deallocated.
//
// interface: The interface object that requested the disable.
//
// Returns kIOReturnUnsupported. Driver may override this method and
// return kIOReturnSuccess on success, or an error code otherwise.

IOReturn IONetworkController::disable(IONetworkInterface * interface)
{
    IOLog("IONetworkController::%s called\n", __FUNCTION__);
    return kIOReturnUnsupported;
}

//---------------------------------------------------------------------------
// With the various overloaded forms of enable() and disable(), what does it
// mean when one does &IONetworkController::enable, to get the address of the 
// member function?
// Instead, these private functions are registered which then calls the
// correct functions.

IOReturn IONetworkController::_enable(IOService * client)
{
    return enable(client);
};

IOReturn IONetworkController::_disable(IOService * client)
{
    return disable(client);
};

//---------------------------------------------------------------------------
// Discover and publish controller capabilities to the property table.
// This method is called by ready().
//
// Returns true if all capabilities were discovered and published
// successfully, false otherwise. Returning false will prevent client
// objects from attaching to the controller since a property that
// a client depends on may be missing

bool IONetworkController::publishCapabilities()
{
    bool        ret = true;
    OSString *  string;
    UInt32      index;

    string = OSString::withCString(getVendorString());
    if (string) {
        ret = setProperty(kIOVendor, string) && ret;
        string->release();
    }

    string = OSString::withCString(getModelString());
    if (string) {
        ret = setProperty(kIOModel, string) && ret;
        string->release();
    }

    string = OSString::withCString(getRevisionString());
    if (string) {
        ret = setProperty(kIORevision, string) && ret;
        string->release();
    }

    string = OSString::withCString(getInfoString());
    if (string) {
        ret = setProperty(kIOInfo, string) && ret;
        string->release();
    }

    // Set OSNumber properties.
    //  
    ret = (getControllerIndex(&index) == kIOReturnSuccess) && ret;
    ret = setProperty(kIOControllerIndex,  index, 32) && ret;
    ret = setProperty(kIOFeatureSet,       getFeatureSet(), 32) && ret;
    ret = setProperty(kIOFamilyFeatureSet, getFamilyFeatureSet(), 32) && ret;

    if (getPacketFilters(&index) == kIOReturnSuccess) {
        ret = setProperty(kIOPacketFilters, index, 32) && ret; 
    }
    else {
        ret = false;
    }

    if (!ret)
        DLOG("IONetworkController::%s error\n", __FUNCTION__);

    return ret;
}

//---------------------------------------------------------------------------
// Broadcast a network event to all attached interface objects.
// This is equivalent to calling the IONetworkInterface::inputEvent()
// function for each interface client.
//
// IONetworkController uses this method to broadcast link and media
// events. Drivers will usually call the inputEvent() method directly
// since it is more efficient, and most drivers will only attach a
// single interface client.
//
// type: Event type.
//  arg: Event argument.
//
// Returns true if the event was delivered, false if an error occurred
// (unable to perform object allocation) and the event was not delivered

bool IONetworkController::broadcastEvent(UInt32 type, void * arg = 0)
{
    IONetworkInterface *   netif;
    OSCollectionIterator * iter = 0;
    OSSet *                clientSet;

    lockForArbitration();   // locks open/close/state changes.
    
    if (_clientSet->getCount()) {
        clientSet = OSSet::withSet(_clientSet, _clientSet->getCount());
        if (clientSet) {
            iter = OSCollectionIterator::withCollection(clientSet);
            clientSet->release();
        }
    }
    
    unlockForArbitration();

    if (!iter)
        return false;

    // Send the event to all attached interface objects.
    //
    while ((netif = (IONetworkInterface *) iter->getNextObject())) {
        if (OSDynamicCast(IONetworkInterface, netif) == 0)
            continue;   // only send events to IONetworkInterface subclasses.
        netif->inputEvent(type, arg);
    }

    iter->release();

    return true;
}

//---------------------------------------------------------------------------
// A client request for the controller to switch to a new MTU size.
//
// mtu: The new MTU size requested.
//
// Returns kIOReturnUnsupported. Drivers may override this method
// and return either kIOReturnSuccess to indicate that the new MTU size
// was accepted and is in effect, or an error to indicate failure.

IOReturn IONetworkController::setMaxTransferUnit(UInt32 mtu)
{
    return kIOReturnUnsupported;
}

//---------------------------------------------------------------------------
// A client request for the driver to perform hardware 
// diagnostics and return the test result after completion.
//
// resultCodeP: In addition to the return code, drivers may return
//               a hardware specific failure code.
//
// Return kIOReturnSuccess if the hardware passed all test, or an
// appropriate error code otherwise. The default return is always
// kIOReturnUnsupported.

IOReturn IONetworkController::performDiagnostics(UInt32 * resultCodeP)
{
    return kIOReturnUnsupported;
}

//---------------------------------------------------------------------------
// Transmit a packet mbuf. If an IOOutputQueue was allocated and returned by 
// createOutputQueue(), then this method will be called by the queue object.
// Otherwise, an interface object will call this method directly upon 
// receiving an output packet from the network layer. When a queue object
// is not present, this method must be safe from multiple client threads,
// each pushing a packet to be sent on the wire.
//
// There is no upper limit on the number of mbufs, hence the number of
// memory fragments, in the mbuf chain provided. Drivers must be able to
// handle cases when the chain might exceed the limit supported by their
// DMA engines, and perform coalescing to copy the various memory fragments
// into a lesser number of fragments. This complexity can be hidden from
// a driver when an IOMBufMemoryCursor is used, which is able to convert
// a mbuf chain into a physical address scatter-gather list that will not
// exceed a specified number of physically contiguous memory segments.
// See IOMBufMemoryCursor.
//
// Packets may also be chained to form a packet chain. Although the
// network layer, through the interface object, will currently only
// send a single mbuf packet to the controller for each outputPacket()
// call, it is possible for this to change. When a queue object is used,
// the queue will automatically accept a single packet or a packet chain,
// but it will call outputPacket() for each packet removed from the queue.
//
// The implementation in IONetworkController performs no useful action
// and will drop all packets. A driver must always override this method.
//
// m: The packet mbuf.
//
// Returns a return code defined by the caller.

UInt32 IONetworkController::outputPacket(struct mbuf * m)
{
    // The implementation here is simply a sink-hole, all packets are
    // dropped.
    //
    if (m) freePacket(m);
    return 0;
}

//---------------------------------------------------------------------------
// A client request to adjust the capacity of the driver's output queue
// (number of packets the queue can hold). If a driver does not override
// this method, then the default action is to forward the request to
// an IOOutputQueue object if it was created. Otherwise return
// kIOReturnUnsupported.
//
// capacity: The new capacity of the output queue.
//
// Returns kIOReturnSuccess on success, kIOReturnBadArgument if the
// specified capacity is invalid, or kIOReturnUnsupported if the
// function is not supported.

IOReturn IONetworkController::setOutputQueueCapacity(UInt32 capacity)
{
    if (_outputQueue)
        return _outputQueue->setCapacity(capacity) ?
               kIOReturnSuccess : kIOReturnBadArgument;
    else
        return kIOReturnUnsupported;
}

//---------------------------------------------------------------------------
// A client request to get the capacity of the output queue. If a driver
// does not override this method, then the default action is to forward the
// request to an IOOutputQueue object if it was created. Otherwise return
// kIOReturnUnsupported.
//
// capacityP: Address of an integer where the capacity
//            shall be written to.
//
// Returns kIOReturnSuccess on success, or kIOReturnUnsupported if an
// IOOutputQueue object was not created.

IOReturn IONetworkController::getOutputQueueCapacity(UInt32 * capacityP) const
{
    if (_outputQueue) {
        *capacityP = _outputQueue->getCapacity();
        return kIOReturnSuccess;
    }
    return kIOReturnUnsupported;
}

//---------------------------------------------------------------------------
// A client request to fetch the number of packets currently held by the
// queue. If a driver does not override this method, then the default action
// is to forward the request to an IOOutputQueue object if it was created. 
// Otherwise return kIOReturnUnsupported.
//
// sizeP: Address of an integer where the size shall be written to.
//
// Returns kIOReturnSuccess on success, or kIOReturnUnsupported if an
// IOOutputQueue object was not created.

IOReturn IONetworkController::getOutputQueueSize(UInt32 * sizeP) const
{
    if (_outputQueue) {
        *sizeP = _outputQueue->getSize();
        return kIOReturnSuccess;
    }
    return kIOReturnUnsupported;
}

//---------------------------------------------------------------------------
// A client request to flush all packets currently held by the queue,
// and return the number of packets discarded. If a driver does not
// override this method, then the default action is to forward the
// request to an IOOutputQueue object if it was created. 
// Otherwise return kIOReturnUnsupported.
//
// flushCountP: Address of an integer where the number of packets
//              discarded shall be written to.
//
// Returns kIOReturnSuccess on success, or kIOReturnUnsupported if an
// IOOutputQueue object was not created.

IOReturn IONetworkController::flushOutputQueue(UInt32 * flushCountP)
{
    if (_outputQueue) {
        *flushCountP = _outputQueue->flush();
        return kIOReturnSuccess;
    }
    return kIOReturnUnsupported;
}

//---------------------------------------------------------------------------
// Report features supported by the controller.
//
// Returns 0. Drivers may override this method and return a mask
// containing all supported feature bits.

UInt32 IONetworkController::getFeatureSet() const
{
    return 0;
}

//---------------------------------------------------------------------------
// Return a string describing the revision level of the controller.

const char * IONetworkController::getRevisionString() const
{
    return 0;
}

//---------------------------------------------------------------------------
// Return a string containing any additional information about the controller
// and/or driver.

const char * IONetworkController::getInfoString() const
{
    return 0;
}

//---------------------------------------------------------------------------
// Return an ordinal number for multiport network adapters.
// The implementation in IONetworkController will work for PCI controllers
// behind a PCI-PCI bridge. This method exists solely to support the
// current interface naming scheme, and is likely to
// undergo changes or may disappear in the future.
//
// indexP: The oridinal number should be written to the
//         integer at this address.
//
// Returns kIOReturnSuccess.

IOReturn IONetworkController::getControllerIndex(UInt32 * index) const
{
    IOService * provider = getProvider();

    *index = 0;

    if (provider) {
        OSNumber * offset = OSDynamicCast(OSNumber,
                            provider->getProperty("IOChildIndex"));
        if (offset)
            *index = offset->unsigned32BitValue();
    }
    return kIOReturnSuccess;
}

//---------------------------------------------------------------------------
// Encodes a request received by syncRequest. This command structure is then
// submitted to commandHandler() through the IOCommandGate::runAction().

struct cmdStruct {
    OSObject *                          client;
    OSObject *                          target;
    IONetworkController::RequestAction  action;
    void *                              arg0;
    void *                              arg1;
    void *                              arg2;
    void *                              arg3;
};

//---------------------------------------------------------------------------
// Execute the request received by syncRequest() and encoded onto
// a cmdStruct structure. This method is called by IOCommandGate's
// runAction() method.

void IONetworkController::commandHandler(void * arg0,
                                         void * arg1,
                                         void * arg2,
                                         void * /*arg3*/)
{
    cmdStruct * cmd       = (cmdStruct *) arg0;  // cmdStruct
    UInt32 *    actRetP   = (UInt32 *)    arg1;  // action return
    IOReturn *  cmdRetP   = (IOReturn *)  arg2;  // commandHandler return
    OSObject *  oldClient = _reqClient;
    bool        accept    = true;

    assert(cmd && actRetP && cmdRetP);

    if (cmd->client != this)
    {
        // Filter the client request.

        accept = syncRequestFilter(cmd->client,
                                   cmd->target,
                                   cmd->action,
                                   cmd->arg0,
                                   cmd->arg1,
                                   cmd->arg2,
                                   cmd->arg3);
    }

    if (accept) {
        _reqClient = cmd->client;

        *actRetP = ((cmd->target)->*(cmd->action))(cmd->arg0,
                                                   cmd->arg1,
                                                   cmd->arg2,
                                                   cmd->arg3);

        _reqClient = oldClient;

        *cmdRetP = kIOReturnSuccess;
    }
    else {
        // actRetP is not set if the request was rejected by the filter.
        *cmdRetP = kIOReturnNotReady;
    }
}

//---------------------------------------------------------------------------
// Perform a request action synchronously. Used both internally and also by
// clients to execute an arbitrary request action using the IOCommandGate's 
// runAction() method. For client requests, where the client field is not
// equal to 'this', the request is filtered by calling syncRequestFilter()
// to qualify the client request. This filter function must return true
// in order for the request to be accepted and the request action called.
//
// client: The client (caller) of the synchronous request.
// target: The target object that implements the request action.
// action: The action to perform.
// ret:    The return value from the action is written to the
//         integer with the provided address. The result is
//         not written if the request was rejected by the request filter.
// arg0:   Optional action argument.
// arg1:   Optional action argument.
// arg2:   Optional action argument.
// arg3:   Optional action argument.
//
// kIOReturnNotReady if the client request was rejected by the filter.
// Otherwise kIOReturnSuccess is returned.

IOReturn IONetworkController::syncRequest(OSObject *     sender,
                                          OSObject *     target,
                                          RequestAction  action,
                                          UInt32 *       ret  = 0,
                                          void *         arg0 = 0,
                                          void *         arg1 = 0,
                                          void *         arg2 = 0,
                                          void *         arg3 = 0)
{
    cmdStruct cmd;
    IOReturn  rc;
    UInt32    ra;

    // bzero(&cmd, sizeof(cmd));

    cmd.client  = sender;   // request client.
    cmd.target  = target;   // target object.
    cmd.action  = action;   // target action.
    cmd.arg0    = arg0;     // action arguments.
    cmd.arg1    = arg1;
    cmd.arg2    = arg2;
    cmd.arg3    = arg3;

    _cmdGate->runAction( (IOCommandGate::Action)
                         &IONetworkController::commandHandler,     /* Action */
                         (void *) &cmd,                 /* arg0 -  cmdStruct */
                         (void *) &ra,               /* arg1 - action return */
                         (void *) &rc );    /* arg2 -  commandHandler return */

    if ((rc == kIOReturnSuccess) && ret)
        *ret = ra;

    return rc;
}

//---------------------------------------------------------------------------
// This method is called to qualify all client requests
// sent to syncRequest(). This implementation will either allow or
// refuse all requests, and this behavior is set by calling the 
// enableSyncRequest() or disableSyncRequest() methods. By default,
// all requests are allowed.
//
//  client: The client of the synchronous request.
//  target: The target object that implements the request action.
//  action: The action to perform.
//  arg0:   Action argument.
//  arg1:   Action argument.
//  arg2:   Action argument.
//  arg3:   Action argument.
//
// Returns true to accept the request and allow the request action to be
// called, or false to refuse it.

bool IONetworkController::syncRequestFilter(OSObject *     client,
                                            OSObject *     target,
                                            RequestAction  action,
                                            void *         arg0,
                                            void *         arg1,
                                            void *         arg2,
                                            void *         arg3)
{
    // If _requestEnabled ifs true, allow the request, otherwise refuse it.

    return _requestEnabled;
}

//---------------------------------------------------------------------------
// A static member function to control the default syncRequestFilter()
// filter function.

void  IONetworkController::_enableSyncRequestFilter(IONetworkController * ctlr,
                                                    bool enabled)
{
    ctlr->_requestEnabled = enabled;
}

//---------------------------------------------------------------------------
// Enable all client requests sent to the syncRequest() method.
// Don't use this method if the driver overrides syncRequestFilter().

void IONetworkController::enableSyncRequest()
{
    _cmdGate->runAction( (IOCommandGate::Action)
                         &IONetworkController::_enableSyncRequestFilter,
                         (void *) true );
}

//---------------------------------------------------------------------------
// Disable all client requests sent to the syncRequest() method.
// Don't use this method if the driver overrides syncRequestFilter().

void IONetworkController::disableSyncRequest()
{
    _cmdGate->runAction( (IOCommandGate::Action)
                         &IONetworkController::_enableSyncRequestFilter,
                         (void *) false );
}

//---------------------------------------------------------------------------
// Get request client. Methods that are called by syncRequest() can call this
// to get the client object which initiated the request.
//
// Returns the request client. If the caller's context does not indicate
// that it is running through syncRequest(), then 0 is returned.

OSObject * IONetworkController::getSyncRequestClient() const
{
    return ( _workLoop->inGate() ? _reqClient : 0 );
}

//---------------------------------------------------------------------------
// Configure an interface object created through
// createInterface(). IONetworkController will register
// its output handler with the interface object provided.
// Subclasses may override this method to customize the interface object.
// Once the interface is registered and opened by a client, it will
// refuse changes to its properties. And since this method is called
// before the interface has become registered, this is a final
// opportunity for the controller to configure the interface.
//
// interface: The interface object to be configured.
//
// Returns true if configuration was successful, false otherwise (this
// will cause attachInterface() to fail).

bool IONetworkController::configureInterface(IONetworkInterface * interface)
{
    IOOutputAction  handler;
    OSObject *      target;
    bool            ret;
    IONetworkData * stats;

    if (!OSDynamicCast(IONetworkInterface, interface))
        return false;

    IOOutputQueue * outQueue = getOutputQueue();

    // Must register an output handler with the interface object.
    // The interface will send output packets, and requests (commands)
    // to its registered output handler. If we allocated an output
    // queue, then we register the queue as the output handler,
    // otherwise, we become the output handler.

    if (outQueue)
    {
        target  = outQueue;
        handler = outQueue->getOutputHandler();
    
        stats   = outQueue->getStatisticsData();
        interface->addNetworkData(stats);
    }
    else
    {
        target  = this;
        handler = getOutputHandler();
    }
    ret = interface->registerOutputHandler(target, handler);

    return ret;
}

bool
IONetworkController::configureNetworkInterface(IONetworkInterface * interface)
{
    return true;
}

//---------------------------------------------------------------------------
// Called by start() to create an optional IOOutputQueue instance to handle
// output queueing. The default implementation will always return 0, hence
// no output queue will be created. A driver may override this method and 
// return a subclass of IOOutputQueue. IONetworkController will keep a 
// reference to the queue created, and will release the object when 
// IONetworkController is freed. Also see getOutputQueue().
//
// Returns a newly allocated and initialized IOOutputQueue instance.

IOOutputQueue * IONetworkController::createOutputQueue()
{
    return 0;
}

IOOutputQueue * IONetworkController::allocateOutputQueue()
{
    return 0;
}

//---------------------------------------------------------------------------
// Return the output queue allocated though createOutputQueue().

IOOutputQueue * IONetworkController::getOutputQueue() const
{
    return _outputQueue;
}

//---------------------------------------------------------------------------
// Called by start() to obtain the constraints on the memory buffer
// associated with each mbuf allocated through allocatePacket().
// Drivers can override this method to specify their buffer constraints
// imposed by their bus master hardware. Note that outbound packets,
// those that originate from the network stack, are not subject
// to the constraints reported here.
//
// constraintsP: A pointer to an IOPacketBufferConstraints structure
//               that that this method is expected to initialize.
//               See IOPacketBufferConstraints structure definition.

void IONetworkController::getPacketBufferConstraints(
                          IOPacketBufferConstraints * constraintsP) const
{
    assert(constraintsP);
    constraintsP->alignStart  = kIOPacketBufferAlign1;
    constraintsP->alignLength = kIOPacketBufferAlign1;
}

//---------------------------------------------------------------------------
// Allocates a mbuf chain. Each mbuf in the chain is aligned according to
// the constraints from IONetworkController::getPacketBufferConstraints().
// The last mbuf in the chain will be guaranteed to be length aligned if 
// the 'size' argument is a multiple of the length alignment.
//
// The m->m_len and m->pkthdr.len fields are updated by this function.
// This allows the driver to pass the mbuf chain obtained through this
// function to the IOMbufMemoryCursor object directly.
//
// If (size + alignments) is smaller than MCLBYTES, then this function
// will always return a single mbuf header or cluster.
//
// The allocation is guaranteed not to block. If a packet cannot be
// allocated, this function will return NULL.

static inline UInt IO_ALIGN_MBUF(
    struct mbuf * m,
    UInt          size,
    UInt          alignStart,
    UInt          alignLength)
{
    assert(m);
    UInt not_aligned = mtod(m, UInt) & alignStart;
    
    // Align starting address.
    //
    if (not_aligned) {
        not_aligned = alignStart - not_aligned + 1;
        m->m_data += not_aligned;
        size      -= not_aligned;
    }
    
    // Align buffer length.
    //
    if ((not_aligned = size & alignLength))
        size -= not_aligned;

    return size;
}

#define IO_APPEND_MBUF(head, tail, m) {     \
    if (tail) {                             \
        (tail)->m_next = (m);               \
        (tail) = (m);                       \
    }                                       \
    else {                                  \
        (head) = (tail) = (m);              \
        (head)->m_pkthdr.len = 0;           \
    }                                       \
}

struct mbuf * IONetworkController::allocatePacket(UInt size)
{
    struct mbuf * head = 0;
    struct mbuf * tail = 0;

    while (size) {
        UInt mbufSize;
        struct mbuf * m;
    
        // Allocate a mbuf, for the initial mbuf segment, allocate a
        // mbuf header, otherwise a 'normal' mbuf will do.
        
        if (head) {
            MGET(m, M_DONTWAIT, MT_DATA);
            mbufSize = MLEN;
        }
        else {
            MGETHDR(m, M_DONTWAIT, MT_DATA);
            mbufSize = MHLEN;
        }

        if (!m) goto error;

        // Append the new mbuf to our chain.
        
        IO_APPEND_MBUF(head, tail, m);

        // If the remaining size exceed the buffer size of a normal mbuf,
        // then attach a cluster to our mbuf. Currently, the cluster size
        // is fixed, and the allocated memory is always MCLBYTES in size.

        if ((size + _alignPadding) > mbufSize) {
            MCLGET(m, M_DONTWAIT);
            if ((m->m_flags & M_EXT) == 0) goto error;
            mbufSize = MCLBYTES;
        }
        
        // mbufSize shall contain the (reduced) capacity of the mbuf after 
        // alignment.
        //
        mbufSize = IO_ALIGN_MBUF(m, mbufSize, _alignStart, _alignLength);

        if (mbufSize > size) {
            // If we wanted to force all mbufs in the chain to be aligned,
            // including the last one, then do the following.
            //
            // mbufSize -= (mbufSize - size) & ~_alignLength;

            mbufSize = size;
            size = 0;
        }
        else
            size -= mbufSize;           // decrement the remaining size         

        m->m_len = mbufSize;            // update the length for this mbuf
        head->m_pkthdr.len += mbufSize; // increment the total length
    }

    return head;

error:
    if (head) m_freem(head);
    return 0;
}

//---------------------------------------------------------------------------
// Release the mbuf back to the free pool.

void IONetworkController::freePacket(struct mbuf * m)
{
    assert(m);
    while (m) m = m_free(m);
}

static inline bool IO_COPY_MBUF(
    const struct mbuf * src,
    struct mbuf *       dst,
    int                 length)
{
    caddr_t src_dat, dst_dat;
    int dst_len, src_len;

    assert(src && dst);

    dst_len = dst->m_len;
    dst_dat = dst->m_data;

    while (src) {

        src_len = src->m_len;
        src_dat = src->m_data;

        if (src_len > length)
            src_len = length;

        while (src_len) {
        
            if (dst_len >= src_len) {
                // copy entire src mbuf to dst mbuf.

                bcopy(src_dat, dst_dat, src_len);               
                length -= src_len;
                dst_len -= src_len;
                dst_dat += src_len;
                src_len = 0;
            }
            else {
                // fill up dst mbuf with some portion of the data in
                // the src mbuf.

                bcopy(src_dat, dst_dat, dst_len);       // dst_len = 0?             
                length -= dst_len;
                dst_len = 0;
                src_len -= dst_len;         
            }
            
            // Go to the next destination mbuf segment.
            
            if (dst_len == 0) {
                if (!(dst = dst->m_next))
                    return (length == 0);
                dst_len = dst->m_len;
                dst_dat = dst->m_data;
            }

        } /* while (src_len) */

        src = src->m_next;

    } /* while (src) */
    
    return (length == 0);   // returns true on success.
}

//---------------------------------------------------------------------------
// Replace the mbuf pointed by the given pointer with another mbuf.
// Drivers can call this method to replace a mbuf before passing the
// original mbuf, which contains a received frame, to the network layer.
//
// mp:   A pointer to the original mbuf that shall be updated by this
//       method to point to the new mbuf.
// size: If size is 0, then the new mbuf shall have the same size
//       as the original mbuf that is being replaced. Otherwise, the new
//       mbuf shall have the size specified here.
//
// If mbuf allocation was successful, then the replacement will
// take place and the original mbuf will be returned. Otherwise,
// a NULL is returned.

struct mbuf * IONetworkController::replacePacket(struct mbuf ** mp,
                                                 UInt size = 0)
{   
    assert((mp != NULL) && (*mp != NULL));

    struct mbuf * m = *mp;

    // If size is zero, then size is taken from the source mbuf.
    //  
    if (size == 0)
        size = m->m_pkthdr.len;
    
    // Allocate a new packet to replace the current packet.
    //
    if (!(*mp = allocatePacket(size)))
    {
        *mp = m; m = 0;
    }

    return m;
}

//---------------------------------------------------------------------------
// Make a copy of a mbuf, and return the copy. The source mbuf is not modified.
//
// m:    The source mbuf.
// size: The number of bytes to copy. If set to 0, then the entire
//       source mbuf is copied.
//
// Returns a new mbuf created from the source packet.

struct mbuf * IONetworkController::copyPacket(const struct mbuf * m,
                                              UInt size = 0)
{
    struct mbuf * mn;

    assert(m != NULL);

    // If size is zero, then size is taken from the source mbuf.
    //  
    if (size == 0)
        size = m->m_pkthdr.len;

    // Copy the current mbuf to the new mbuf, and return the new mbuf.
    // The input mbuf is left intact.
    //
    if (!(mn = allocatePacket(size)))
        return 0;

    if (!IO_COPY_MBUF(m, mn, size))
    {
        IOLog("IONetworkController: copyPacket failure\n");
        freePacket(mn);
        mn = 0;
    }

    return mn;
}

//---------------------------------------------------------------------------
// Either replace or copy the source mbuf given depending on the amount of
// data in the source mbuf. This method will either perform a copy or replace 
// the source mbuf, whichever is more time efficient. If replaced, then the
// original mbuf is returned, and a new mbuf is allocated to take its place.
// If copied, the source mbuf is left intact, while a copy is returned that
// is just big enough to hold all the data from the source mbuf.
//
// mp:        A pointer to the source mbuf that may be updated by this
//            method to point to the new mbuf if replaced.
// rcvlen:    The number of data bytes in the source mbuf.
// replacedP: Pointer to a bool that is set to true if the
//            source mbuf was replaced, or set to false if the
//            source mbuf was copied.
//
// Returns a replacement or a copy of the source mbuf, 0 if mbuf
// allocation failed.

struct mbuf * IONetworkController::replaceOrCopyPacket(struct mbuf ** mp,
                                                       UInt   rcvlen,
                                                       bool * replacedP)
{
    struct mbuf * m;

    if ((rcvlen + _alignPadding) > MHLEN)
    {
        // Large packet, it is more efficient to allocate a new mbuf
        // to replace the original mbuf than to make a copy. The new
        // packet shall have the same size as the original mbuf being
        // replaced.
        // 
        m = replacePacket(mp);
        *replacedP = true;
    }
    else {
        // The copy will fit within a header mbuf. Fine, make a copy
        // of the original mbuf instead of replacing it. We only copy
        // the rcvlen bytes, not the entire source mbuf.
        //
        assert((mp != NULL) && (*mp != NULL));
        m = copyPacket(*mp, rcvlen);        
        *replacedP = false;
    }

    return m;
}

//---------------------------------------------------------------------------
// Used for debugging only. Log the mbuf header.

void IONetworkController::_logMbuf(struct mbuf * m)
{
    if (!m) {
        IOLog("logMbuf: NULL mbuf\n");
        return;
    }
    
    while (m) {
        IOLog("m_next   : %08x\n", (UInt) m->m_next);
        IOLog("m_nextpkt: %08x\n", (UInt) m->m_nextpkt);
        IOLog("m_len    : %d\n",   (UInt) m->m_len);
        IOLog("m_data   : %08x\n", (UInt) m->m_data);
        IOLog("m_type   : %08x\n", (UInt) m->m_type);
        IOLog("m_flags  : %08x\n", (UInt) m->m_flags);
        
        if (m->m_flags & M_PKTHDR)
            IOLog("m_pkthdr.len  : %d\n", (UInt) m->m_pkthdr.len);

        if (m->m_flags & M_EXT) {
            IOLog("m_ext.ext_buf : %08x\n", (UInt) m->m_ext.ext_buf);
            IOLog("m_ext.ext_size: %d\n", (UInt) m->m_ext.ext_size);
        }
        
        m = m->m_next;
    }
    IOLog("\n");
}

//---------------------------------------------------------------------------
// Allocate and attache a new IOKernelDebugger client object.
//
// debuggerP: An IOKernelDebugger handle that is updated by this method
//            to contain the allocated IOKernelDebugger instance.
//
// Returns true on success, false otherwise.

bool IONetworkController::attachDebuggerClient(IOKernelDebugger ** debugger)
{
    IOKernelDebugger *  client;
    OSString *          debuggerString;
    bool                ret = false;
    bool                initOk = false;

    // We delay some initialization until the first time that
    // attachInterface() is called by the subclass.
    //
    SYNC_REQ(this, &IONetworkController::_controllerIsReady, &initOk);
    if (!initOk)
        return false;

    // Refuse to attach a debugger client if the controller's kIOEnableDebugger
    // property set to No (first letter starts with 'n' or 'N').

    debuggerString = OSDynamicCast(OSString, getProperty(kIOEnableDebugger));
    if (!debuggerString ||
        (debuggerString->getChar(0) == 'N') ||
        (debuggerString->getChar(0) == 'n'))
        return false;

    // Create a debugger client nub and register the static
    // member functions as the polled-mode handlers.
    //
    client = IOKernelDebugger::debugger(this,
                                        &debugTxHandler,
                                        &debugRxHandler);

    *debugger = client;

    if (client && !client->attach(this))
    {
        // Unable to attach the client object.
        *debugger = 0;
        client->detach(this);
        client->release();
    }

    if (*debugger)
    {
        IOLog("%s: Debugger client attached\n", getName());
        publishResource("kdp");
        ret = true;
    }

    return ret;
}

//---------------------------------------------------------------------------
// Detach and terminate the IOKernelDebugger client object provided.
// A synchronous termination is issued, and this method returns after
// the client has been terminated.
//
// debugger: The IOKernelDebugger instance to be detached and terminated.
//           If the argument provided is NULL or is not an IOKernelDebugger,
//           this method will return immediately.

void IONetworkController::detachDebuggerClient(IOKernelDebugger * debugger)
{
    if (OSDynamicCast(IOKernelDebugger, debugger) == 0)
        return;

    // Terminate the debugger client and return after the client has
    // been terminated. Since the debugger has no IOKit clients of
    // its own, this should be fast.

    debugger->terminate(kIOServiceRequired | kIOServiceSynchronous);
}

//---------------------------------------------------------------------------
// An enable request from an IOKernelDebugger client. This method is called 
// when an open is received from an IOKernelDebugger client. Drivers that
// wish to provide debugging services must override this method and setup
// the hardware to support the polled-mode send and receive methods;
// receivePacket() and sendPacket(). Debug capable drivers may also override
// the more generic enable/disable calls that take an IOService argument.
//
// debugger: The IOKernelDebugger client that issued the open.
//
// Returns kIOReturnSuccess. The driver method must return kIOReturnSuccess
// to allow the debugger open, anything else will cause the debugger open
// to fail and the attachDebuggerClient() method will return false.

IOReturn IONetworkController::enable(IOKernelDebugger * debugger)
{
    return handleDebuggerOpen(debugger);
}

IOReturn IONetworkController::handleDebuggerOpen(IOKernelDebugger * debugger)
{
    return kIOReturnSuccess;
}

//---------------------------------------------------------------------------
// A disable request from an IOKernelDebugger client. This method is called
// when a close is received from an IOKernelDebugger client. A driver which
// implements enable(IOKernelDebugger *) should also implement this method
// to disable hardware support for the polled-mode send and receive methods.
//
// debugger: The IOKernelDebugger client that issued the close.
//
// Returns kIOReturnSuccess. The driver method should return a status
// from the disable operation.

IOReturn IONetworkController::disable(IOKernelDebugger * debugger)
{
    return handleDebuggerClose(debugger);
}

IOReturn IONetworkController::handleDebuggerClose(IOKernelDebugger * debugger)
{
    return kIOReturnSuccess;
}

//---------------------------------------------------------------------------
// Take and release the debugger lock.

void IONetworkController::reserveDebuggerLock()
{
    _debugLockState = IODebuggerLock(this);
}

void IONetworkController::releaseDebuggerLock()
{
    IODebuggerUnlock(_debugLockState);
}

//---------------------------------------------------------------------------
// This static C++ member function is registered by attachDebuggerClient()
// as the debugger receive handler. IOKernelDebugger will call this
// function when KDP is polling for a received packet. This function will
// in turn will call the receivePacket() member function implemented by
// a driver with debugger support.

void IONetworkController::debugRxHandler(IOService * handler,
                                         void *      buffer,
                                         UInt *      length,
                                         UInt        timeout)
{
    ((IONetworkController *) handler)->receivePacket(buffer,
                                                     length,
                                                     timeout);   
}

//---------------------------------------------------------------------------
// This static C++ member function is registered by attachDebuggerClient()
// as the debugger transmit handler. IOKernelDebugger will call this
// function when KDP sends an outgoing packet. This function will in turn
// call the sendPacket() member function implemented by a driver with
// debugger support.

void IONetworkController::debugTxHandler(IOService * handler,
                                         void *      buffer,
                                         UInt        length)
{
    ((IONetworkController *) handler)->sendPacket(buffer, length);
}

//---------------------------------------------------------------------------
// Debugger polled-mode receive handler. This method must be implemented
// by a driver that supports kernel debugging. After a debugger client is 
// attached through attachDebuggerClient(), this method will be called by
// the debugger object to poll for a incoming packet when the debugger is
// active. This method can be called from an interrupt context, and the
// driver must never block or perform any memory allocation. The
// receivePacket() method in IONetworkController is used as a placeholder
// and should never be called. A driver that attaches a debugger client
// must override this method.
//
// pkt:     Pointer to a receive buffer where the received packet should
//          be stored to. The buffer has room for 1518 bytes.
// pkt_len: The length of the received packet must be written to the
//          integer pointed by pkt_len.
// timeout: The maximum amount of time in milliseconds to poll for
//          a packet to arrive before this method must return.

void IONetworkController::receivePacket(void * /*pkt*/,
                                        UInt * /*pkt_len*/,
                                        UInt /*timeout*/)
{
    IOLog("IONetworkController::%s()\n", __FUNCTION__);
}

//---------------------------------------------------------------------------
// Debugger polled-mode transmit handler. This method must be implemented
// by a driver that supports kernel debugging. After a debugger client is 
// attached through attachDebuggerClient(), this method will be called by
// the debugger object to send an outbound packet generated by the debugger.
// This method can be called from an interrupt context, and the
// driver must never block or perform any memory allocation. The
// sendPacket() method in IONetworkController is used as a placeholder
// and should never be called. A driver that attaches a debugger client
// must override this method.
//
// pkt:     Pointer to a transmit buffer containing the packet to be sent.
// pkt_len: The amount of data in the transmit buffer.

void IONetworkController::sendPacket(void * /*pkt*/, UInt /*pkt_len*/)
{
    IOLog("IONetworkController::%s()\n", __FUNCTION__);
}

//---------------------------------------------------------------------------
// Report the link status and the active medium. Update the link status 
// parameters published by the controller. Drivers should call this method 
// whenever the link status changes. Never call this method from interrupt 
// context since this method may block. An event will be sent to all attached
// interface objects when a change is detected.
//
// status: Link status bits. See IONetworkMedium.h for defined link
//         status bits.
// speed:  Link speed in units of bits per second.
// activeMedium: A medium entry in the published medium dictionary
//               where the link was established. This may not be the
//               same as the current medium.
// data:   An OSData containing any additional link information.
//
// Returns true if all link properties were successfully updated,
// false otherwise.

bool IONetworkController::setLinkStatus(UInt32                  status,
                                        UInt64                  speed,
                                        const IONetworkMedium * activeMedium,
                                        OSData *                data = 0)
{
    bool   ret;
    bool   changed = false;

    MEDIUM_LOCK;

    ret = setMediumProperty(gIOActiveMediumKey,
                            activeMedium,
                            &_activeMedium,
                            false,
                            &changed);

    ret = setLink32Property(kIOLinkStatus,
                            status,
                            &_linkStatus,
                            false,
                            &changed) && ret;

    ret = setLink64Property(kIOLinkSpeed,
                            speed,
                            &_linkSpeed,
                            false,
                            &changed) && ret;

    if (data) {
        if (_linkData != data) {
            ret = setProperty(kIOLinkData, data) && ret;
            _linkData = data;
            changed = true;
        }
    }
    else if (_linkData) {
        _linkData = 0;
        removeProperty(kIOLinkData);
        changed = true;
    }

    MEDIUM_UNLOCK;

    if (changed)
        broadcastEvent(kIONetworkEventLinkChange);

    return ret;
}

//---------------------------------------------------------------------------
// Returns the medium dictionary published by the driver through
// publishMediumDictionary(). Use copyMediumDictionary() to get a copy
// of the medium dictionary.
//
// Returns the published medium dictionary, or 0 if the driver has not
// yet published a medium dictionary using publishMediumDictionary().

const OSDictionary * IONetworkController::getMediumDictionary() const
{
    return OSDynamicCast(OSDictionary, getProperty(kIOMediumDictionary));
}

//---------------------------------------------------------------------------
// Returns a.copy of the medium dictionary published by the driver.
// The caller is responsible for releasing the dictionary object returned.
// Use getMediumDictionary() to get a reference to the published medium
// dictionary instead of creating a copy.
//
// Returns a copy of the medium dictionary, or 0 if the driver has not
// published a medium dictionary using publishMediumDictionary().

OSDictionary * IONetworkController::copyMediumDictionary() const
{
    OSDictionary * newMediumDict = 0;

    MEDIUM_LOCK;

    if (getMediumDictionary()) {
        newMediumDict = OSDictionary::withDictionary(getMediumDictionary(),
                                      getMediumDictionary()->getCount());
    }

    MEDIUM_UNLOCK;

    return newMediumDict;
}

//---------------------------------------------------------------------------
// A client request for the controller to change the selected medium.
// Drivers may override this method and provide an implementation
// appropriate for its hardware, then call setCurrentMedium() to update
// the current medium property if a change occurred.
//
// medium: An entry in the published medium dictionary.
//
// Return kIOReturnUnsupported. Drivers may override this method and
// return kIOReturnSuccess if the selected medium was activated,
// or an error code otherwise.

IOReturn IONetworkController::selectMedium(const IONetworkMedium * medium)
{
    return kIOReturnUnsupported;
}

//---------------------------------------------------------------------------
// Private function to lookup a key in the medium dictionary and call 
// setMedium() if a match is found. This function is called by our
// clients to change the medium by passing a name for the desired medium.

IOReturn IONetworkController::selectMediumWithName(const OSSymbol * mediumName)
{
    OSSymbol *        currentMediumName;
    IONetworkMedium * newMedium = 0;
    bool              doChange  = true;
    IOReturn          ret       = kIOReturnSuccess;

    if (OSDynamicCast(OSSymbol, mediumName) == 0)
        return kIOReturnBadArgument;

    MEDIUM_LOCK;

    do {
        const OSDictionary * mediumDict = getMediumDictionary();
        if (!mediumDict) {  // no medium dictionary, bail out.
            ret = kIOReturnUnsupported;
            break;
        }

        // Lookup the new medium in the dictionary.
        //
        newMedium = OSDynamicCast(IONetworkMedium, 
                                  mediumDict->getObject(mediumName));
        if (!newMedium) {
            ret = kIOReturnBadArgument;
            break;          // not found, invalid mediumName.
        }

        newMedium->retain();

        // Lookup the current medium key to avoid unnecessary
        // medium changes.
        //
        currentMediumName = (OSSymbol *) getProperty(gIOCurrentMediumKey);

        // Is change necessary?
        //
        if (currentMediumName && mediumName->isEqualTo(currentMediumName))
            doChange = false;
    }
    while (0);

    MEDIUM_UNLOCK;

    if (newMedium)
    {
        // Call the driver's selectMedium() without holding the medium lock.
    
        if (doChange)
            ret = selectMedium(newMedium);

        // offset the earlier retain count increment.
        //
        newMedium->release();
    }

    return ret;
}

//---------------------------------------------------------------------------
// Private function to add/replace/remove a medium property in the 
// property table. Returns true if the medium property was successfully
// added to the property table. The medium lock should be held before
// calling this function.

bool IONetworkController::setMediumProperty(const OSSymbol *         key,
                                            const IONetworkMedium *  medium,
                                            const IONetworkMedium ** cache,
                                            bool                     force,
                                            bool *                   changed)
{
    bool  ret = false;

    do {
        if (!force && (*cache == medium))
        {
            ret = true;
            break;
        }

        if (medium == 0)
        {
            removeProperty(key);
            if (changed) *changed = true;
            *cache = 0;
            ret = true;
            break;
        }

        const OSDictionary * mediumDict = getMediumDictionary();
        if (!mediumDict)
            break;    // no medium dictionary, bail out.

        if (OSDynamicCast(IONetworkMedium, medium) == 0)
            break;    // not a valid IONetworkMedium.

        // Make sure the medium given is an entry in the medium dictionary.
        //
        if (mediumDict->getObject(medium->getName()) != (OSObject *) medium)
            break;    // not a member of dictionary.

        // Update property table.
        //
        if ((ret = setProperty(key, (OSSymbol *) medium->getName())))
        {
            if (changed) *changed = true;
            *cache = medium;
        }
    }
    while (0);

    return ret;
}

bool IONetworkController::setLink64Property(const char *   key,
                                            UInt64         value,
                                            UInt64 *       cache,
                                            bool           force = false,
                                            bool *         changed = 0)
{
    if (!force && (*cache == value))
        return true;

    if (setProperty(key, value, 64))
    {
        if (changed) *changed = true;
        *cache = value;
        return true;
    }

    return false;
}

bool IONetworkController::setLink32Property(const char *   key,
                                            UInt32         value,
                                            UInt32 *       cache,
                                            bool           force = false,
                                            bool *         changed = 0)
{
    if (!force && (*cache == value))
        return true;

    if (setProperty(key, value, 32))
    {
        if (changed) *changed = true;
        *cache = value;
        return true;
    }

    return false;
}

//---------------------------------------------------------------------------
// From the set of medium objects in the medium dictionary published by the 
// driver, one of them can be designated as the currently selected medium. 
// Drivers should call this method whenever their media selection changes.
// An entry in the driver's property table is updated to advertise the 
// current medium.
//
// A media change event will be broadcasted to all attached interface
// clients when the current medium property changes.
//
// medium: A medium object to promote as the current medium.
//
// Returns true if the medium dictionary exists, the medium object
// provided matches an entry in this dictionary, and the property
// table update was successful, false otherwise.

bool IONetworkController::setCurrentMedium(const IONetworkMedium * medium)
{
    bool ret, changed;

    MEDIUM_LOCK;
    ret = setMediumProperty(gIOCurrentMediumKey,
                            medium, 
                            &_currentMedium,
                            false,
                            &changed);
    MEDIUM_UNLOCK;

    if (changed)
        broadcastEvent(kIONetworkEventMediumChange);
    
    return ret;
}

//---------------------------------------------------------------------------
// Returns the currently selected medium object. If the driver has yet to
// assign an entry in its medium dictionary as the current medium using the
// setCurrentMedium() method, then the driver's property table is consulted
// and a default medium property (can be set by the user), is looked
// up and the corresponding entry in the medium dictionary is returned.
// Therefore, drivers can always call getCurrentMedium() to either get
// the current medium selected by the driver, or the default
// medium chosen by the user.
//
// Returns the current medium entry from the medium dictionary, or 0
// if a matching entry was not found.

const IONetworkMedium * IONetworkController::getCurrentMedium() const
{
    IONetworkMedium * currentMedium     = 0;
    OSString *        currentMediumName = 0;

    MEDIUM_LOCK;

    do {
        const OSDictionary * mediumDict = getMediumDictionary();
        if (!mediumDict)    // no medium dictionary, bail out.
            break;

        // Fetch the current medium name from the property table.
        //
        currentMediumName = OSDynamicCast(OSString,
                            getProperty(gIOCurrentMediumKey));

        // Make sure the current medium name points to an entry in
        // the medium dictionary.
        //
        if (currentMediumName && !mediumDict->getObject(currentMediumName))
            currentMediumName = 0;

        if (currentMediumName == 0) {
            OSString * defaultMediumName;

            // No (valid) current medium name, try the default medium name.
            //
            defaultMediumName = OSDynamicCast(OSString, 
                                getProperty(kIODefaultMedium));

            // If there is a default medium name, and it points to a
            // valid entry in the medium dictionary, then make it
            // current.
            //
            if (defaultMediumName &&
                mediumDict->getObject(defaultMediumName))
            {
                // setProperty(kIOCurrentMedium, defaultMediumKey);
                currentMediumName = defaultMediumName;
            }
        }

        if (currentMediumName)
            currentMedium = OSDynamicCast(IONetworkMedium, 
                            mediumDict->getObject(currentMediumName));
    }
    while (0);

    MEDIUM_UNLOCK;

    return currentMedium;
}

//---------------------------------------------------------------------------
// A private function to verify a medium dictionary. Returns true if the
// dictionary is OK.

bool IONetworkController::verifyMediumDictionary(
                                const OSDictionary * mediumDict)
{
    OSCollectionIterator * iter;
    bool                   verifyOk = true;
    OSSymbol *             key;

    if (!OSDynamicCast(OSDictionary, mediumDict))
        return false;   // invalid argument

    if (mediumDict->getCount() == 0)
        return false;   // empty dictionary

    iter = OSCollectionIterator::withCollection((OSDictionary *) mediumDict);
    if (!iter)
        return false;   // cannot allocate iterator

    while ((key = (OSSymbol *) iter->getNextObject()))
    {
        if ( !OSDynamicCast(IONetworkMedium, mediumDict->getObject(key)) )
        {
            verifyOk = false;   // non-medium object in dictionary
            break;
        }
    }

    iter->release();
    
    return verifyOk;
}

//---------------------------------------------------------------------------
// Called by drivers to publish their medium dictionary.
// The dictionary consist of IONetworkMedium entries that represent
// the entire media selection supported by the hardware. This method
// will make a copy of the provided dictionary, then add the copy to
// the driver's property table. The dictionary provided can be
// released after this call returns. It is permissible to call
// this method multiple times, which may be necessary if the hardware's
// media capability changes dynamically. However, if this were not
// so, then drivers will typically call this method from its start() 
// implementation, after the hardware capability is discovered.
//
// Several methods depend on the presence of a medium dictionary.
// They should be called after the dictionary has been published.
// Those are:
//   selectMedium()
//   setCurrentMedium()
//   getCurrentMedium()
//   getMediumDictionary()
//   copyMediumDictionary()
//
// Calling publishMediumDictionary() will cause a media change event
// to be delivered to all attached interface clients.
//
// Returns true if the dictionary is valid, and was successfully
// added to the property table, false otherwise.

bool
IONetworkController::publishMediumDictionary(const OSDictionary * mediumDict)
{
    OSDictionary *   cloneDict;
    bool             ret = false;

    if (!verifyMediumDictionary(mediumDict))
        return false;   // invalid dictionary

    // Create a clone of the source dictionary. This prevents the driver
    // from adding/removing entries after the medium dictionary is added
    // to the property table.
    //
    cloneDict = OSDictionary::withDictionary(mediumDict,
                                             mediumDict->getCount());
    if (!cloneDict)
        return false;  // unable to create a copy

    MEDIUM_LOCK;                              

    // Add the dictionary to the property table.
    //
    if (setProperty(kIOMediumDictionary, cloneDict))
    {
        const OSSymbol *   mediumName;
        IONetworkMedium *  medium;

        mediumName = (OSSymbol *) getProperty(gIOCurrentMediumKey);
        medium     = mediumName ?
                     (IONetworkMedium *) cloneDict->getObject(mediumName) : 0;
        setMediumProperty(gIOCurrentMediumKey, medium, &_currentMedium, true);
        
        mediumName = (OSSymbol *) getProperty(gIOActiveMediumKey);
        medium     = mediumName ?
                     (IONetworkMedium *) cloneDict->getObject(mediumName) : 0;
        setMediumProperty(gIOActiveMediumKey, medium, &_activeMedium, true);

        ret = true;
    }

    MEDIUM_UNLOCK;

    // Retained by the property table. drop our retain count.
    //
    cloneDict->release();

    // Broadcast a medium change event.
    //
    broadcastEvent(kIONetworkEventMediumChange);

    return ret;
}

//---------------------------------------------------------------------------
// Call enablePacketFilters() through syncRequest(). See enablePacketFilters().

IOReturn IONetworkController::doEnablePacketFilters(IOService * client,
                                                    UInt32      newFilters,
                                                    UInt32 *    activeFiltersP)
{
    IOReturn  ret = kIOReturnNotReady;
    UInt32    dummy;
    UInt32 *  activeP = activeFiltersP ? activeFiltersP : &dummy;

    *activeP = 0;

    if (SYNC_REQ(client, &IONetworkController::enablePacketFilters,
                 &ret,
                 (void *) newFilters, (void *) activeP) == kIOReturnSuccess)
    {
        // Update the registry.
        //
        setProperty(kIOActivePacketFilters, *activeP, 32);
    }

    return ret;
}

//---------------------------------------------------------------------------
// Calls getPacketFilters() through syncRequest(). See getPacketFilters().

IOReturn IONetworkController::doGetPacketFilters(IOService * client,
                                                 UInt32 *    filtersP)
{
    DO_SYNC_REQ(getPacketFilters, (void *) filtersP)
}

//---------------------------------------------------------------------------
// Call enable(IOService *) through syncRequest(). See enable().

IOReturn IONetworkController:: doEnable(IOService * client)
{
    DO_SYNC_REQ(_enable, (void *) client)
}

//---------------------------------------------------------------------------
// Call disable(IOService *) through syncRequest(). See disable().

IOReturn IONetworkController:: doDisable(IOService * client)
{
    DO_SYNC_REQ(_disable, (void *) client)
}

//---------------------------------------------------------------------------
// Call getControllerIndex() through syncRequest(). See getControllerIndex().

IOReturn IONetworkController::doGetControllerIndex(IOService * client,
                                                   UInt32 *    index)
{
    DO_SYNC_REQ(getControllerIndex, (void *) index)
}

//---------------------------------------------------------------------------
// Call setMaxTransferUnit() through syncRequest(). See setMaxTransferUnit().

IOReturn IONetworkController::doSetMaxTransferUnit(IOService * client,
                                                   UInt32      mtu)
{
    DO_SYNC_REQ(setMaxTransferUnit, (void *) mtu)
}

//---------------------------------------------------------------------------
// Call selectMedium() through syncRequest(). See selectMedium().

IOReturn IONetworkController::doSelectMedium(IOService *      client,
                                             const OSSymbol * mediumName)
{
    DO_SYNC_REQ(selectMediumWithName, (void *) mediumName)
}

//---------------------------------------------------------------------------
// Call setOutputQueueCapacity() through syncRequest().
// See setOutputQueueCapacity().

IOReturn IONetworkController::doSetOutputQueueCapacity(IOService * client,
                                                       UInt32      capacity)
{
    DO_SYNC_REQ(setOutputQueueCapacity, (void *) capacity)
}

//---------------------------------------------------------------------------
// Call getOutputQueueCapacity() through syncRequest().
// See getOutputQueueCapacity().

IOReturn
IONetworkController::doGetOutputQueueCapacity(IOService * client,
                                              UInt32 *    capacityP)
{
    DO_SYNC_REQ(getOutputQueueCapacity, (void *) capacityP)
}

//---------------------------------------------------------------------------
// Call getOutputQueueSize() through syncRequest().
// See getOutputQueueSize().

IOReturn
IONetworkController::doGetOutputQueueSize(IOService * client,
                                          UInt32 *    sizeP)
{
    DO_SYNC_REQ(getOutputQueueSize, (void *) sizeP)
}

//---------------------------------------------------------------------------
// Call flushOutputQueue() through syncRequest(). See flushOutputQueue().

IOReturn
IONetworkController::doFlushOutputQueue(IOService * client,
                                        UInt32 *    flushCountP)
{
    DO_SYNC_REQ(flushOutputQueue, (void *) flushCountP)
}

//---------------------------------------------------------------------------
// Call performDiagnostics() through syncRequest().
// See performDiagnostics().

IOReturn
IONetworkController::doPerformDiagnostics(IOService * client,
                                          UInt32 *    failureCode)
{
    DO_SYNC_REQ(performDiagnostics, (void *) failureCode)
}
