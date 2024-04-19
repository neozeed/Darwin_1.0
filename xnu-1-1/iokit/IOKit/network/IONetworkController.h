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
 * IONetworkController.h
 *
 * Network controller driver superclass.
 *
 * HISTORY
 * 9-Dec-1998       Joe Liu (jliu) created.
 *
 */

#ifndef _IONETWORKCONTROLLER_H
#define _IONETWORKCONTROLLER_H

#include <IOKit/IOService.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/network/IONetworkInterface.h>
#include <IOKit/network/IOKernelDebugger.h>

struct mbuf;            // forward declarations
class  IOCommandGate;
class  IOOutputQueue;
class  IONetworkMedium;

// Keys for property table entries.
//
#define kIOVendor                "IOVendor"                // OSString
#define kIOModel                 "IOModel"                 // OSString
#define kIORevision              "IORevision"              // OSString
#define kIOInfo                  "IOInfo"                  // OSString
#define kIOControllerIndex       "IOControllerIndex"       // OSNumber:32
#define kIOFeatureSet            "IOFeatureSet"            // OSNumber:32
#define kIOFamilyFeatureSet      "IOFamilyFeatureSet"      // OSNumber:32
#define kIOMediumDictionary      "IOMediumDictionary"      // OSDictionary
#define kIODefaultMedium         "IODefaultMedium"         // OSString
#define kIOCurrentMedium         "IOCurrentMedium"         // OSSymbol
#define kIOActiveMedium          "IOActiveMedium"          // OSSymbol
#define kIOEnableDebugger        "IOEnableDebugger"        // OSString (Yes/No)
#define kIOLinkSpeed             "IOLinkSpeed"             // OSNumber:64
#define kIOLinkStatus            "IOLinkStatus"            // OSNumber:32
#define kIOLinkData              "IOLinkData"              // OSData
#define kIOPacketFilters         "IOPacketFilters"         // OSNumber:32
#define kIOActivePacketFilters   "IOActivePacketFilters"   // OSNumber:32

/*! @typedef IOPacketBufferConstraints
    @discussion Constraint parameters used by allocatePacket() to
    align the memory buffer associated with a mbuf.
    @field alignStart Alignment for the buffer's starting address.
    @field alignLength Buffer length alignment.
    @field maxLength Maximum buffer size (not implemented).
    */

typedef struct {
    UInt32 alignStart;
    UInt32 alignLength;
    UInt32 maxLength;
} IOPacketBufferConstraints;

// Some frequently used alignment constants.
//
enum {
    kIOPacketBufferAlign1   = 1,
    kIOPacketBufferAlign2   = 2,
    kIOPacketBufferAlign4   = 4,
    kIOPacketBufferAlign8   = 8,
    kIOPacketBufferAlign16  = 16,
    kIOPacketBufferAlign32  = 32,
};

// Packet filters. Upper 16-bits are family specific.
//
enum {
    kIOPacketFilterUnicast          = 0x1,
    kIOPacketFilterBroadcast        = 0x2,
    kIOPacketFilterMulticast        = 0x10,
    kIOPacketFilterMulticastAll     = 0x20,
    kIOPacketFilterPromiscuous      = 0x100,
    kIOPacketFilterPromiscuousAll   = 0x200,
    kIOPacketFilterFamilyMask       = 0xffff0000,
};

// Generic controller features.
//
enum {
    kIONetworkFeatureNoBSDWait = 0x01,  // start() should not wait for BSD.
};

    
/*! @class IONetworkController
    @abstract IONetworkController implements the framework for a generic 
    network controller. A subclass of IONetworkController must provide
    additional functionality specific for a particular network family.
    In addition, the driver must implement (override) a basic set of 
    hardware dependent methods to create a working driver.
    
    IONetworkController attaches itself to the network layer via an
    IONetworkInterface object. A controller object without a companion
    interface is not known to the networking system. The controller
    interacts with the network layer by calling methods defined by
    the interface object. And conversely, the network layer issues
    command and packets to the controller through its interface object.
    
    IONetworkController allocates an IOWorkLoop and an IOCommandGate object.
    Most commands (requests) sent from the interface object are handled
    by instructing the IOCommandGate instance to call one or more methods,
    usually implemented by the driver, that may require hardware access.
    Thus interface requests are always serialized. Outbound packets sent
    from the interface to the controller have no implicit serialization. 
    Drivers must implement an output function that is thread safe, or use
    an IOOutputQueue object which will provide a serialization model.
    */

class IONetworkController : public IOService
{
    OSDeclareAbstractStructors(IONetworkController)

public:

/*! @typedef RequestAction
    @discussion Prototype for an action to be called through syncRequest().
    See syncRequest().
    @param arg0 Action argument.
    @param arg1 Action argument.
    @param arg2 Action argument.
    @param arg3 Action argument.
    */

    typedef UInt32 (OSObject::*RequestAction)(void * arg0,
                                              void * arg1,
                                              void * arg2,
                                              void * arg3);

private:

    IOWorkLoop *             _workLoop;
    IOCommandGate *          _cmdGate;
    IOOutputQueue *          _outputQueue;
    OSSet *                  _clientSet;
    OSObject *               _reqClient;
    UInt32                   _alignStart;
    UInt32                   _alignLength;
    UInt32                   _alignPadding;
    bool                     _handleReady;
    bool                     _requestEnabled;
    IOLock *                 _mediumLock;
    IODebuggerLockState      _debugLockState;
    UInt32                   _linkStatus;
    UInt64                   _linkSpeed;
    OSData *                 _linkData;
    const IONetworkMedium *  _activeMedium;
    const IONetworkMedium *  _currentMedium;

    bool     _controllerIsReady();
    IOReturn _enable(IOService * client);
    IOReturn _disable(IOService * client);
    void     commandHandler(void *, void *, void *, void *);
    IOReturn selectMediumWithName(const OSSymbol * mediumName);

    bool setMediumProperty(const OSSymbol *         key,
                           const IONetworkMedium *  medium,
                           const IONetworkMedium ** cache,
                           bool                     force = false,
                           bool *                   changed = 0);

    bool setLink64Property(const char * key,
                           UInt64       value,
                           UInt64 *     cache,
                           bool         force = false,
                           bool *       changed = 0);

    bool setLink32Property(const char * key,
                           UInt32       value,
                           UInt32 *     cache,
                           bool         force = false,
                           bool *       changed = 0);

    bool verifyMediumDictionary(const OSDictionary * mediumDict);

    static void _logMbuf(struct mbuf * m);

    static void _enableSyncRequestFilter(IONetworkController *, bool);

    static void debugRxHandler(IOService * handler,
                               void *      buffer,
                               UInt *      length,
                               UInt        timeout);

    static void debugTxHandler(IOService * handler,
                               void *      buffer,
                               UInt        length);

public:

/*! @function initialize
    @abstract IONetworkController class initializer.
    @discussion Create often used OSSymbol objects that are used as keys.
    This method is called explicitly by a line in IOStartIOKit.cpp and not
    by the OSDefineMetaClassAndInit() mechanism, since the OSSymbol class
    is not guaranteed to be initialized first, and likewise for the
    OSSymbol pool. */

    static void initialize();

/*! @function init
    @abstract Initialize the IONetworkController instance.
    @discussion Instance variables are initialized to their default values,
    then super::init() is called.
    @param properties A dictionary object containing a property table.
    @result true on success, false otherwise. */ 

    virtual bool init(OSDictionary * properties);

/*! @function start
    @abstract Start the controller driver.
    @discussion Called when the controller was matched to a provider and
    has been selected to start running. IONetworkController will allocate
    resources and gather the controller's properties. No I/O will be
    triggered until the subclass attaches a client object from its own
    start method. Subclasses must override this method and call
    super::start() at the beginning of its implementation. They should
    also check the return value to make sure their superclass was
    started successfully. The resources allocated by IONetworkController
    include: An IOWorkLoop object, an IOCommandGate object to handle 
    synchronous requests, an OSSet to track our clients, and an optional
    IOOutputQueue object for output queueing.

    Tasks that are usually performed by a driver's start method are;
    resource allocation, hardware initialization, allocation of 
    IOEventSources and attaching them to an IOWorkLoop object,
    publishing a medium dictionary, and finally, attaching an
    interface object when the driver is ready to handle client
    requests.
    @param provider The provider that the controller was matched
    (and attached) to.
    @result true on success, false otherwise. */

    virtual bool start(IOService * provider);

/*! @function stop
    @abstract Stop the controller driver.
    @discussion The opposite of start(). The controller has been
    instructed to stop running. This method is called when the
    driver has been terminated. The stop() method should release
    resources and undo actions performed by start(). Subclasses
    must override this method and call super::stop() at the end
    of its implementation.
    @param provider The provider that the controller was matched
    (and attached) to. */

    void stop(IOService * provider);

/*! @function outputPacket
    @abstract Transmit a packet mbuf.
    @discussion If an IOOutputQueue was allocated and returned by 
    createOutputQueue(), then this method will be called by the queue object.
    Otherwise, an interface object will call this method directly upon 
    receiving an output packet from the network layer. When a queue object
    is not present, this method must be safe from multiple client threads,
    each pushing a packet to be sent on the wire.
    
    There is no upper limit on the number of mbufs, hence the number of
    memory fragments, in the mbuf chain provided. Drivers must be able to
    handle cases when the chain might exceed the limit supported by their
    DMA engines, and perform coalescing to copy the various memory fragments
    into a lesser number of fragments. This complexity can be hidden from
    a driver when an IOMBufMemoryCursor is used, which is able to convert
    a mbuf chain into a physical address scatter-gather list that will not
    exceed a specified number of physically contiguous memory segments.
    See IOMBufMemoryCursor.

    Packets may also be chained to form a packet chain. Although the
    network layer, through the interface object, will currently only
    send a single mbuf packet to the controller for each outputPacket()
    call, it is possible for this to change. When a queue object is used,
    the queue will automatically accept a single packet or a packet chain,
    but it will call outputPacket() for each packet removed from the queue.

    The implementation in IONetworkController performs no useful action
    and will drop all packets. A driver must always override this method.

    @param m The packet mbuf.
    @result A return code defined by the caller. */

    virtual UInt32 outputPacket(struct mbuf * m);

/*! @function getFeatureSet
    @discussion Report features supported by the controller.
    @result Returns 0. Drivers may override this method and return a mask
    containing all supported feature bits. */

    virtual UInt32 getFeatureSet() const;

/*! @function getFamilyFeatureSet
    @discussion Report family specific features supported by the controller.
    @result Drivers may override this method and return a mask containing
    all supported family feature bits. */

    virtual UInt32 getFamilyFeatureSet() const = 0;

/*! @function getVendorString
    @result Return a vendor string. i.e. "Apple" */

    virtual const char * getVendorString() const = 0;

/*! @function getModelString
    @result Return a string describing the model of the network controller.
    i.e. "BMac Ethernet" */

    virtual const char * getModelString() const = 0;

/*! @function getRevisionString
    @result Return a string describing the revision level of the 
    controller. */

    virtual const char * getRevisionString() const;

/*! @function getInfoString
    @result Return a string containing any additional information about
    the controller and/or driver. */

    virtual const char * getInfoString() const;

/*! @function getCurrentMedium
    @abstract Get the currently selected medium.
    @discussion Returns the currently selected medium object. If the driver
    has yet to assign an entry in its medium dictionary as the current medium 
    using the setCurrentMedium() method, then the driver's property table is 
    consulted and a default medium property (can be set by the user), is
    looked up and the corresponding entry in the medium dictionary is returned.
    Therefore, drivers can always call getCurrentMedium() to either get
    the current medium selected by the driver, or the default
    medium chosen by the user.
    @result The current medium entry from the medium dictionary, or 0
    if a matching entry was not found. */

    virtual const IONetworkMedium * getCurrentMedium() const;

/*! @function getMediumDictionary
    @abstract Returns the medium dictionary published by the driver.
    @discussion Returns the medium dictionary published by the driver
    through publishMediumDictionary(). Use copyMediumDictionary() to
    get a copy of the medium dictionary.
    @result The published medium dictionary, or 0 if the driver has not
    yet published a medium dictionary using publishMediumDictionary(). */

    virtual const OSDictionary * getMediumDictionary() const;

/*! @function copyMediumDictionary
    @abstract Returns a copy of the medium dictionary published by the driver.
    @discussion The caller is responsible for releasing the dictionary
    object returned. Use getMediumDictionary() to get a reference to the
    published medium dictionary instead of creating a copy.
    @result A copy of the medium dictionary, or 0 if the driver has not
    published a medium dictionary using publishMediumDictionary(). */

    virtual OSDictionary * copyMediumDictionary() const;

/*! @function syncRequest
    @abstract Perform a request action synchronously.
    @discussion Used both internally and also by clients to execute an
    arbitrary request action using the IOCommandGate's runAction()
    method. For client requests, where the client field is not equal to
    'this', the request is filtered by calling syncRequestFilter() to
    qualify the client request. This filter function must return true
    in order for the request to be accepted and the request action called.
    @param client The client (caller) of the synchronous request.
    @param target The target object that implements the request action.
    @param action The action to perform.
    @param ret    The return value from the action is written to the
                  integer with the provided address. The result is
                  not written if the request was rejected by the request
                  filter.
    @param arg0 Optional action argument.
    @param arg1 Optional action argument.
    @param arg2 Optional action argument.
    @param arg3 Optional action argument.
    @result kIOReturnNotReady if the client request was rejected by the filter.
    Otherwise kIOReturnSuccess is returned. */

    virtual IOReturn syncRequest(OSObject *     client,
                                 OSObject *     target,
                                 RequestAction  action,
                                 UInt32 *       ret  = 0,
                                 void *         arg0 = 0,
                                 void *         arg1 = 0,
                                 void *         arg2 = 0,
                                 void *         arg3 = 0);

/*! @function getOutputHandler
    @abstract Get the address of the method designated to handle output 
    packets.
    @result the address of the outputPacket() method. */

    virtual IOOutputAction getOutputHandler() const;

/*! @function doEnablePacketFilters
    @discussion Call enablePacketFilters() through syncRequest().
    See enablePacketFilters(). */

    virtual IOReturn doEnablePacketFilters(IOService * client,
                                           UInt32      filters,
                                           UInt32 *    activeFiltersP = 0);

/*! @function doGetPacketFilters
    @discussion Call getPacketFilters() through syncRequest().
    See getPacketFilters(). */

    virtual IOReturn doGetPacketFilters(IOService * client, UInt32 * filtersP);

/*! @function doEnable
    @discussion Call enable(IOService *) through syncRequest().
    See enable(). */

    virtual IOReturn doEnable(IOService * client);

/*! @function doDisable
    @discussion Call disable(IOService *) through syncRequest().
    See disable(). */

    virtual IOReturn doDisable(IOService * client);

/*! @function doGetControllerIndex
    @discussion Call getControllerIndex(IOService *) through syncRequest().
    See getControllerIndex(). */

    virtual IOReturn doGetControllerIndex(IOService * client,
                                          UInt32 *    index);

/*! @function doSetMaxTransferUnit
    @discussion Call setMaxTransferUnit() through syncRequest().
    See setMaxTransferUnit(). */

    virtual IOReturn doSetMaxTransferUnit(IOService * client, UInt32 mtu);

/*! @function doSelectMedium
    @discussion Call selectMedium() through syncRequest().
    See selectMedium(). */

    virtual IOReturn doSelectMedium(IOService *      client,
                                    const OSSymbol * mediumName);

/*! @function doSetOutputQueueCapacity
    @discussion Call setOutputQueueCapacity() through syncRequest().
    See setOutputQueueCapacity(). */

    virtual IOReturn doSetOutputQueueCapacity(IOService * client,
                                              UInt32      capacity);

/*! @function doGetOutputQueueCapacity
    @discussion Call getOutputQueueCapacity() through syncRequest().
    See getOutputQueueCapacity(). */

    virtual IOReturn doGetOutputQueueCapacity(IOService * client,
                                              UInt32 *    capacityP);

/*! @function doGetOutputQueueSize
    @discussion Call getOutputQueueSize() through syncRequest().
    See getOutputQueueSize(). */

    virtual IOReturn doGetOutputQueueSize(IOService * client,
                                          UInt32 *    sizeP);

/*! @function doFlushOutputQueue
    @discussion Call flushOutputQueue() through syncRequest().
    See flushOutputQueue(). */

    virtual IOReturn doFlushOutputQueue(IOService * client,
                                        UInt32 *    flushCountP);

/*! @function doPerformDiagnostics
    @discussion Call performDiagnostics() through syncRequest().
    See performDiagnostics(). */

    virtual IOReturn doPerformDiagnostics(IOService * client,
                                          UInt32 *    failureCode);

protected:

/*! @function free
    @abstract Free the IONetworkController instance.
    @discussion Free the IONetworkController instance and all allocated
    resources, then call super::free(). */

    virtual void free();

/*! @function setOutputQueueCapacity
    @abstract Adjust the capacity of the output queue.
    @discussion A client request to adjust the capacity of the driver's
    output queue (number of packets the queue can hold). If a driver does
    not override this method, then the default action is to forward the
    request to an IOOutputQueue object if it was created. Otherwise return
    kIOReturnUnsupported.
    @param capacity The new capacity of the output queue.
    @result kIOReturnSuccess on success, kIOReturnBadArgument if the
    specified capacity is invalid, or kIOReturnUnsupported if the
    function is not supported. */

    virtual IOReturn setOutputQueueCapacity(UInt32 capacity);

/*! @function getOutputQueueCapacity
    @abstract Get the capacity of the output queue.
    @discussion A client request to get the capacity of the driver's
    output queue. If a driver does not override this method, then the
    default action is to forward the request to an IOOutputQueue object
    if it was created. Otherwise return kIOReturnUnsupported.
    @param capacityP Address of an integer where the capacity
    shall be written to.
    @result kIOReturnSuccess on success, or kIOReturnUnsupported if an
    IOOutputQueue object was not created. */

    virtual IOReturn getOutputQueueCapacity(UInt32 * capacityP) const;

/*! @function getOutputQueueSize
    @abstract Get the number of packets in the output queue.
    @discussion A client request to get the number of packets currently held
    by the queue. If a driver does not override this method, then the default
    action is to forward the request to an IOOutputQueue object if it was 
    created. Otherwise return kIOReturnUnsupported.
    @param sizeP Address of an integer where the queue size shall be
           written to.
    @result kIOReturnSuccess on success, or kIOReturnUnsupported if an
    IOOutputQueue object was not created. */

    virtual IOReturn getOutputQueueSize(UInt32 * sizeP) const;

/*! @function flushOutputQueue
    @abstract Discard all packets in the output queue.
    @discussion A client request to flush all packets currently held by the
    queue, and return the number of packets discarded. If a driver does not
    override this method, then the default action is to forward the request
    to an IOOutputQueue object if it was created. Otherwise return
    kIOReturnUnsupported.
    @param flushCountP Address of an integer where the number of
    discarded packets shall be written to.
    @result kIOReturnSuccess on success, or kIOReturnUnsupported if an
    IOOutputQueue object was not created. */

    virtual IOReturn flushOutputQueue(UInt32 * flushCountP);

/*! @function ready
    @abstract An indication that the controller is ready to service
    clients.
    @discussion This method is called the first time that a controller
    driver calls attachInterface() or attachDebuggerClient(), which is
    an indication that the driver has been started and is ready to
    service client requests. IONetworkController uses this method to
    complete its initialization before any client objects are attached.
    @param provider The controller's provider.
    @result true on success, false otherwise. */

    virtual bool ready(IOService * provider);

/*! @function enable
    @abstract An enable request from an interface client.
    @discussion Called by an interface client to enable the controller.
    This method must bring up the hardware and enable event sources
    to prepare for packet transmission and reception. A driver should
    delay the allocation of most runtime resources until this method is
    called to conserve shared system resources.
    @param interface The interface client object that requested the enable.
    @result kIOReturnUnsupported. Driver may override this method and
    return kIOReturnSuccess on success, or an error code otherwise. */

    virtual IOReturn enable(IONetworkInterface * interface);

/*! @function disable
    @abstract A disable request from an interface client.
    @discussion Called by an interface object to disable the controller.
    This method should quiesce the hardware and disable event sources.
    Any resources allocated in enable() should also be deallocated.
    @param interface The interface object that requested the disable.
    @result kIOReturnUnsupported. Driver may override this method and
    return kIOReturnSuccess on success, or an error code otherwise. */

    virtual IOReturn disable(IONetworkInterface * interface);

/*! @function enable
    @abstract An enable request from a client.
    @discussion Handle an enable request from a client. The client
    object is typecasted using OSDynamicCast, and if the client is an
    IOKernelDebugger or an IONetworkInterface, then an overloaded enable
    method that takes a more specific argument is called. If the client
    matches neither type, a kIOReturnBadArgument is returned.
    A driver has the option of override this generic enable method,
    or the derived version.
    @param client The client object requesting the enable.
    @result The return value from the derived enable method, or
    kIOReturnBadArgument if the client's type is unknown. */

    virtual IOReturn enable(IOService * client);

/*! @function disable
    @abstract A disable request from a client.
    @discussion Handle an enable request from a client. The client
    object is typecasted using OSDynamicCast, and if the client is an
    IOKernelDebugger or an IONetworkInterface, then an overloaded disable
    method that takes a more specific argument is called. If the client
    matches neither type, a kIOReturnBadArgument is returned.
    A driver has the option of override this generic enable method,
    or the derived version.
    @param client The client object requesting the disable.
    @result The return from the derived disable method, or
    kIOReturnBadArgument if the client's type is unknown. */
    
    virtual IOReturn disable(IOService * client);

/*! @function getControllerIndex
    @abstract Return an ordinal number for multiport adapters.
    @discussion Return an ordinal number for multiport network adapters.
    The implementation in IONetworkController will work for PCI controllers
    behind a PCI-PCI bridge. This method exists solely to support the
    current interface naming scheme, and is likely to
    undergo changes or may disappear in the future.
    @param indexP The oridinal number should be written to the
           integer at this address.
    @result kIOReturnSuccess. */

    virtual IOReturn getControllerIndex(UInt32 * indexP) const;

/*! @function setMaxTransferUnit
    @abstract Change the controller's MTU size.
    @discussion A client request for the controller to change to
    a new MTU size.
    @param mtu The new MTU size requested.
    @result kIOReturnUnsupported. Drivers may override this method
    and return either kIOReturnSuccess to indicate that the new MTU size
    was accepted and is in effect, or an error to indicate failure. */

    virtual IOReturn setMaxTransferUnit(UInt32 mtu);

/*! @function performDiagnostics
    @abstract A request for the driver to perform hardware diagnostics.
    @discussion A client request for the driver to perform hardware 
    diagnostics and return the test result after completion.
    @param resultCodeP In addition to the return code, drivers may
    write a hardware specific result code to the integer at this
    address.
    
    @result kIOReturnSuccess if hardware passed all test, otherwise
    an appropriate error code should be returned. The default return
    is always kIOReturnUnsupported. */

    virtual IOReturn performDiagnostics(UInt32 * resultCodeP);

/*! @function publishCapabilities
    @abstract Publish controller's properties and capabilities.
    @discussion Discover and publish controller capabilities to the
    property table. This method is called by ready().
    @result true if all capabilities were discovered and published
    successfully, false otherwise. Returning false will prevent client
    objects from attaching to the controller since a property that
    a client depends on may be missing. */

    virtual bool publishCapabilities();

/*! @function publishMediumDictionary
    @abstract Publish a medium dictionary.
    @discussion Called by drivers to publish their medium dictionary.
    The dictionary consist of IONetworkMedium entries that represent
    the entire media selection supported by the hardware. This method
    will make a copy of the provided dictionary, then add the copy to
    the driver's property table. The dictionary provided can be
    released after this call returns. It is permissible to call
    this method multiple times, which may be necessary if the hardware's
    media capability changes dynamically. However, if this were not
    so, then drivers will typically call this method from its start() 
    implementation, after the hardware capability is discovered.

    Several methods depends on the presence of a medium dictionary.
    They should be called after the dictionary has been published.
    Those are:
        selectMedium()
        setCurrentMedium()
        getCurrentMedium()
        getMediumDictionary()
        copyMediumDictionary()
    
    Calling publishMediumDictionary() will cause a media change event
    to be delivered to all attached interface clients.
    @param mediumDict A dictionary containing IONetworkMedium objects.
    @result true if the dictionary is valid, and was successfully
    added to the property table, false otherwise. */

    virtual bool publishMediumDictionary(const OSDictionary * mediumDict);

/*! @function setCurrentMedium
    @abstract Designate an entry in the published medium dictionary as
    the currently selected medium.
    @discussion From the set of medium objects in the medium dictionary
    published by the driver, one of them can be designated as the
    currently selected medium. Drivers should call this method whenever
    their media selection changes. An entry in the driver's property
    table is updated to advertise the current medium.
    
    A media change event will be broadcasted to all attached interface
    clients when the current medium property changes.
    
    @param medium A medium object to promote as the current medium.
    @result true if the medium dictionary exists, the medium object
    provided matches an entry in this dictionary, and the property
    table update was successful, false otherwise. */

    virtual bool setCurrentMedium(const IONetworkMedium * medium);

/*! @function selectMedium
    @abstract Change the controller's medium selection.
    @discussion A client request for the controller to change the
    selected medium. Drivers may override this method and provide
    an implementation appropriate for its hardware, then call
    setCurrentMedium() to update the current medium property if
    a change occurred.
    @param medium An entry in the published medium dictionary.
    @result kIOReturnUnsupported. Drivers may override this method and
    return kIOReturnSuccess if the selected medium was activated,
    or an error code otherwise. */

    virtual IOReturn selectMedium(const IONetworkMedium * medium);

/*! @function setLinkStatus
    @abstract Report the link status and the active medium.
    @discussion Update the link status parameters published by the
    controller. Drivers should call this method whenever the link
    status changes. Never call this method from interrupt context
    since this method may block. An event will be sent to all
    attached interface objects when a change is detected.
    @param status Link status bits.
           See IONetworkMedium.h for defined link status bits.
    @param speed Link speed in units of bits per second.
    @param activeMedium A medium entry in the published medium dictionary
           where the link was established. This may not be the same as the
           current medium. See setCurrentMedium().
    @param data An OSData containing any additional link information.
    @result true if all link properties were successfully updated,
    false otherwise. */

    virtual bool setLinkStatus(UInt32                  status,
                               UInt64                  speed,
                               const IONetworkMedium * activeMedium,
                               OSData *                data = 0);

/*! @function syncRequestFilter
    @abstract Filter client requests sent to syncRequest().
    @discussion This method is called to qualify all client requests
    sent to syncRequest(). This implementation will either allow or
    refuse all requests, and this behavior is set by calling the 
    enableSyncRequest() or disableSyncRequest() methods. By default,
    all requests are allowed.
    @param client The client of the synchronous request.
    @param target The target object that implements the request action.
    @param action The action to perform.
    @param arg0 Action argument.
    @param arg1 Action argument.
    @param arg2 Action argument.
    @param arg3 Action argument.
    @result true to accept the request and allow the request action to be
    called, or false to refuse it. */

    virtual bool syncRequestFilter(OSObject *     client,
                                   OSObject *     target,
                                   RequestAction  action,
                                   void *         arg0,
                                   void *         arg1,
                                   void *         arg2,
                                   void *         arg3);

/*! @function enableSyncRequest
    @abstract Set syncRequestFilter() to accept all client requests.
    @discussion Enable all client requests sent to the syncRequest() method.
    Don't use this method if the driver overrides syncRequestFilter(). */

    virtual void enableSyncRequest();

/*! @function disableSyncRequest
    @abstract Set syncRequestFilter() to refuse all client requests.
    @discussion Disable all client requests sent to the syncRequest() method.
    Don't use this method if the driver overrides syncRequestFilter(). */

    virtual void disableSyncRequest();  

/*! @function getSyncRequestClient
    @abstract Get request client.
    @discussion Methods that are called by syncRequest() can call this
    to get the client object which initiated the request.
    @result The request client. If the caller's context does not indicate
    that it is running through syncRequest(), then 0 is returned. */

    virtual OSObject * getSyncRequestClient() const;

/*! @function handleOpen
    @abstract Handle a client open.
    @discussion Handle a client open on the controller object. IOService
    calls this method with the arbitration lock held. Subclasses
    should not override this method.
    @param client The client that is trying to open the controller.
    @param options Not used. See IOService.
    @param argument Not used. See IOService.
    @result true to accept the client open, false to refuse the open. */

    virtual bool handleOpen(IOService *  client,
                            IOOptionBits options,
                            void *       argument);

/*! @function handleClose
    @abstract Handle a client close.
    @discussion Handle a close from one of our client objects. IOService
    calls this method with the arbitration lock held. Subclasses
    should not override this method.
    @param client The client that has closed the controller.
    @param options Not used. See IOService. */

    virtual void handleClose(IOService * client, IOOptionBits options);

/*! @function handleIsOpen
    @abstract Query a client that has an open on the controller.
    @discussion This method is always called by IOService with the
    arbitration lock held. Subclasses should not override this method.
    @result true if the specified client, or any client if none is
    specified, presently has an open on this object. */

    virtual bool handleIsOpen(const IOService * client) const;

/*! @function attachInterface
    @abstract Attach a new interface client object.
    @discussion Create a new interface client object and attach it
    to the controller. The createInterface() method is called to
    perform the allocation and initialization, followed by a call to 
    configureInterface() to configure the interface. Both of these
    methods can be overridden by subclasses to customize the
    interface client attached. Drivers must call attachInterface()
    from its start() method, after it is ready to service client requests.
    @param interfaceP If successful (return value is true), then the
    interface object will be written to the handle provided.
    @param doRegister If true, then registerService() is called to register
    the interface, which will trigger the matching process, and cause the 
    interface to become registered with the network layer. For drivers that
    wish to delay the registration, and hold off servicing requests and data
    packets from the network layer, set doRegister to false and call 
    registerService() on the interface client when the controller becomes
    ready.
    @result true on success, false otherwise. */

    virtual bool attachInterface(IONetworkInterface ** interface,
                                 bool  doRegister = true);

    virtual bool attachNetworkInterface(IONetworkInterface ** interface,
                                        bool  doRegister = true);  // obsolete

/*! @function detachInterface
    @abstract Detach an interface client object.
    @discussion This method will check that the object provided is indeed
    an IONetworkInterface, and if so its terminate() method is called.
    The interface object will close and detach from its controller only
    after the network layer has removed all references to the data
    structures exposed by the interface.
    @param interface An interface object to be detached.
    @param sync If true, the interface is terminated synchronously.
           Note that this may cause detachInterface() to block
           for an indefinite period of time. */

    virtual void detachInterface(IONetworkInterface * interface,
                                 bool                 sync = false);

/*! @function getPacketBufferConstraints
    @abstract Get controller's packet buffer constraints.
    @discussion Called by start() to obtain the constraints on the
    memory buffer associated with each mbuf allocated through
    allocatePacket(). Drivers can override this method to specify
    their buffer constraints imposed by their bus master hardware.
    Note that outbound packets, those that originate from the
    network stack, are not subject to the constraints reported here.
    @param constraintsP A pointer to an IOPacketBufferConstraints
    structure that that this method is expected to initialize.
    See IOPacketBufferConstraints structure definition.
    */

    virtual void getPacketBufferConstraints(
                    IOPacketBufferConstraints * constraintsP) const;

/*! @function allocatePacket
    @discussion Allocate a mbuf packet with the given size. This method
    will always return a single mbuf unless the size requested (plus the
    alignment padding) is greater than MCLBYTES. The mbuf (or a mbuf
    chain) returned is aligned according to the constraints reported
    by getPacketBufferConstraints().
    
    The m_len and pkthdr.len fields in the mbuf is updated by this
    method, thus allowing the packet to be passed directly to an
    IOMbufMemoryCursor object in order to convert the mbuf to a
    scatter-gather list.

    @param size The desired size of the mbuf packet.
    @result The allocated mbuf packet, or 0 if allocation failed. */

    virtual struct mbuf * allocatePacket(UInt size);

/*! @function copyPacket
    @discussion Make a copy of a mbuf, and return the copy.
    The source mbuf is not modified.
    @param m The source mbuf.
    @param size The number of bytes to copy. If set to 0, then the entire
    source mbuf is copied (source length is taken from the m_pkthdr.len).
    @result A new mbuf created from the source packet. */

    virtual struct mbuf * copyPacket(const struct mbuf * m, UInt size = 0);

/*! @function replacePacket
    @discussion Replace the mbuf pointed by the given pointer with
    another mbuf. Drivers can call this method to replace a mbuf before
    passing the original mbuf, which contains a received frame, to the
    network layer.
    @param mp A pointer to the original mbuf that shall be updated by this
    method to point to the new mbuf.
    @param size If size is 0, then the new mbuf shall have the same size
    as the original mbuf that is being replaced. Otherwise, the new
    mbuf shall have the size specified here.
    @result If mbuf allocation was successful, then the replacement will
    take place and the original mbuf will be returned. Otherwise, a NULL
    is returned. */

    virtual struct mbuf * replacePacket(struct mbuf ** mp, UInt size = 0);

/*! @function replaceOrCopyPacket
    @discussion Either replace or copy the source mbuf given depending
    on the amount of data in the source mbuf. This method will either
    perform a copy or replace the source mbuf, whichever is more
    time efficient. If replaced, then the original mbuf is returned, and
    a new mbuf is allocated to take its place. If copied, the source mbuf is
    left intact, while a copy is returned that is just big enough to hold
    all the data from the source mbuf.
    @param mp A pointer to the source mbuf that may be updated by this
    method to point to the new mbuf if replaced.
    @param rcvlen The number of data bytes in the source mbuf.
    @param replacedP Pointer to a bool that is set to true if the
           source mbuf was replaced, or set to false if the
           source mbuf was copied.
    @result A replacement or a copy of the source mbuf, 0 if mbuf
    allocation failed. */

    virtual struct mbuf * replaceOrCopyPacket(struct mbuf ** mp,
                                              UInt           rcvlen,
                                              bool *         replacedP);

/*! @function freePacket
    @discussion Release the mbuf back to the free pool.
    @param m The mbuf to be freed. */

    virtual void freePacket(struct mbuf * m);

/*! @function createInterface
    @abstract Create an interface client object.
    @discussion Create a new IONetworkInterface instance.
    This method is called by attachInterface() to perform the
    allocation and initialization of a new interface client object.
    A family specific subclass of IONetworkController must implement
    this method and return a matching interface instance. For example,
    IOEthernetController implements this method and returns an
    IOEthernetInterface instance.
    @result The allocated interface object. */

    virtual IONetworkInterface * createInterface() = 0;

/*! @function configureInterface
    @abstract Configure an interface client object.
    @discussion Configure an interface object created through
    createInterface(). IONetworkController will register
    its output handler with the interface object provided.
    Subclasses may override this method to customize the interface object.
    Once the interface is registered and opened by a client, it will
    refuse changes to its properties. And since this method is called
    before the interface has become registered, this is a final
    opportunity for the controller to configure the interface.
    @param interface The interface object to be configured.
    @result true if configuration was successful, false otherwise
    (this will cause attachInterface() to fail). */

    virtual bool configureInterface(IONetworkInterface * interface);

    // obsolete
    virtual bool configureNetworkInterface(IONetworkInterface * interface);

/*! @function createOutputQueue
    @abstract Create an IOOutputQueue to handle output queueing.
    @discussion Called by start() to create an IOOutputQueue instance
    to handle output queueing. The default implementation will always
    return 0, hence no output queue will be created. A driver may override
    this method and return a subclass of IOOutputQueue. IONetworkController
    will keep a reference to the queue created, and will release the 
    object when IONetworkController is freed. Also see getOutputQueue().
    @result A newly allocated and initialized IOOutputQueue instance. */

    virtual IOOutputQueue * createOutputQueue();
    virtual IOOutputQueue * allocateOutputQueue();  // obsolete

/*! @function getOutputQueue
    @abstract Get the IOOutputQueue object created by createOutputQueue().
    @result Return the output queue created by createOutputQueue(). */

    virtual IOOutputQueue * getOutputQueue() const;

/*! @function getWorkLoop
    @abstract Get the IOWorkLoop object created by IONetworkController.
    @discussion An IOWorkLoop object is created by the start() method.
    Drivers can call getWorkLoop() to obtain a reference to the
    IOWorkLoop object, and attach their event sources, such
    as IOTimerEventSource or IOInterruptEventSource.
    See IOWorkLoop.
    @result The IOWorkLoop object created by IONetworkController. */

    virtual IOWorkLoop * getWorkLoop() const;

/*! @function getCommandGate
    @abstract Get the IOCommandGate object created by IONetworkController.
    @discussion An IOCommandGate is created and attached to an
    IOWorkLoop by the start() method. This IOCommandGate object is used
    to handle client requests issued through the syncRequest() method.
    Subclasses that need an IOCommandGate should try to use the object
    returned by this method, rather than creating a new instance.
    See IOCommandGate.
    @result The IOCommandGate object created by IONetworkController. */

    virtual IOCommandGate * getCommandGate() const;

/*! @function broadcastEvent
    @abstract Send an event to all interface client objects.
    @discussion Broadcast an event to all attached interface objects.
    This is equivalent to calling the IONetworkInterface::inputEvent()
    method for all interfaces.
    
    IONetworkController uses this method to broadcast link and media
    events. Drivers will usually call the inputEvent() method directly
    since it is more efficient, and most drivers will only attach a
    single interface client.
    
    @param type Event type.
    @param arg Event argument.
    @result true if the event was delivered, false if an error occurred
    (unable to perform object allocation) and the event was not delivered. */

    virtual bool broadcastEvent(UInt32 type, void * arg = 0);

/*! @function getPacketFilters
    @abstract Get the set of packet filters supported by the controller.
    @discussion A subclass must implement this method and report its
    supported filter set. See IOPacketFilter enum for the list of defined
    packet filters.
    @param filtersP A mask of supported filters should be written to the
    integer with this address.
    @result kIOReturnSuccess on success, or an error to indicate
    failure to discover the hardware supported filters. */

    virtual IOReturn getPacketFilters(UInt32 * filtersP) = 0;

/*! @function enablePacketFilters
    @abstract Enable a set of packet filters supported by the controller.
    @discussion After calling getPacketFilters() to gather the set of
    supported packet filters, a client may issue a request to enable a
    (sub)set of filters from the supported set.
    @param filters Each bit that is set indicates a particular filter that
           should be enabled. Filter bits that cleared should be disabled.
    @param activeFiltersP Must be updated by this method to contain the filter
           set that was activated. Ideally, it should be the same as the filter 
           set specified by the first argument.
    @result kIOReturnSuccess on success, otherwise an error code is
    returned. If (*activeFiltersP != filters), then an error is expected. */

    virtual IOReturn enablePacketFilters(UInt32   filters,
                                         UInt32 * activeFiltersP) = 0;

/*! @function attachDebuggerClient
    @abstract Attach a new IOKernelDebugger client object. Attaching
    a debugger client implies that the driver supports the kernel
    debugging interface, and must implement the two polled-mode entry
    points. See sendPacket() and receivePacket().
    @discussion Allocate and attach a new IOKernelDebugger client object.
    @param debuggerP An IOKernelDebugger handle that is updated by this
           method to contain the allocated IOKernelDebugger instance.
    @result true on success, false otherwise. */

    virtual bool attachDebuggerClient(IOKernelDebugger ** debuggerP);

/*! @function detachDebuggerClient
    @abstract Detach an IOKernelDebugger client object.
    @discussion Detach and terminate the IOKernelDebugger client object
    provided. A synchronous termination is issued, and this method returns
    after the client has been terminated.
    @param debugger The IOKernelDebugger instance to be detached and
           terminated. If the argument provided is NULL or is not an
           IOKernelDebugger, this method will return immediately. */

    virtual void detachDebuggerClient(IOKernelDebugger * debugger);

/*! @function enable
    @abstract An enable request from an IOKernelDebugger client.
    @discussion This method is called when an open is received from an
    IOKernelDebugger client. Drivers that wish to provide debugging
    services must override this method and setup the hardware to
    support the polled-mode send and receive methods; receivePacket()
    and sendPacket(). Debug capable drivers may also override the
    more generic enable/disable calls that take an IOService argument.
    @param debugger The IOKernelDebugger client that issued the open.
    @result kIOReturnSuccess. The driver method must return kIOReturnSuccess
    to allow the debugger open, anything else will cause the debugger open
    to fail and the attachDebuggerClient() method will return false. */

    virtual IOReturn enable(IOKernelDebugger * debugger);
    virtual IOReturn handleDebuggerOpen(IOKernelDebugger * debugger);

/*! @function disable
    @abstract A disable request from an IOKernelDebugger client.
    @discussion This method is called when a close is received from an
    IOKernelDebugger client. A driver which implements
    enable(IOKernelDebugger *) should also implement this method to
    disable hardware support for the polled-mode send and receive methods.
    @param debugger The IOKernelDebugger client that issued the close.
    @result kIOReturnSuccess. The driver method should return a status
    from the disable operation. */

    virtual IOReturn disable(IOKernelDebugger * debugger);
    virtual IOReturn handleDebuggerClose(IOKernelDebugger * debugger);

/*! @function reserveDebuggerLock
    @abstract Take the global debugger lock.
    @discussion This method should not be used. Call the
    lock() method provided by IOKernelDebugger instead. */
    
    void reserveDebuggerLock();

/*! @function releaseDebuggerLock
    @abstract Release the global debugger lock.
    @discussion This method should not be used. Call the
    unlock() method provided by IOKernelDebugger instead. */

    void releaseDebuggerLock();

/*! @function receivePacket
    @abstract Debugger polled-mode receive handler.
    @discussion This method must be implemented by a driver that supports
    kernel debugging. After a debugger client is attached through
    attachDebuggerClient(), this method will be called by the debugger
    object to poll for a incoming packet when the debugger is active.
    This method can be called from an interrupt context, and the
    driver must never block or perform any memory allocation. The
    receivePacket() method in IONetworkController is used as a placeholder
    and should never be called. A driver that attaches a debugger client
    must override this method.
    @param pkt Pointer to a receive buffer where the received packet should
           be stored to. The buffer has room for 1518 bytes.
    @param pkt_len The length of the received packet must be written to the
           integer pointed by pkt_len.
    @param timeout The maximum amount of time in milliseconds to poll for
           a packet to arrive before this method must return. */ 

    virtual void receivePacket(void * pkt, UInt * pkt_len, UInt timeout);

/*! @function sendPacket
    @abstract Debugger polled-mode transmit handler.
    @discussion This method must be implemented by a driver that supports
    kernel debugging. After a debugger client is attached through
    attachDebuggerClient(), this method will be called by the debugger
    object to send an outbound packet generated by the debugger.
    This method can be called from an interrupt context, and the
    driver must never block or perform any memory allocation. The
    sendPacket() method in IONetworkController is used as a placeholder
    and should never be called. A driver that attaches a debugger client
    must override this method.
    @param pkt Pointer to a transmit buffer containing the packet to be sent.
    @param pkt_len The amount of data in the transmit buffer. */

    virtual void sendPacket(void * pkt, UInt pkt_len);
};

#define IONetworkAction IONetworkController::RequestAction

inline bool
IONetworkController::attachNetworkInterface(IONetworkInterface ** interface,
                                            bool  doRegister = true)
{
    return attachInterface(interface, doRegister);
}

#endif /* !_IONETWORKCONTROLLER_H */
