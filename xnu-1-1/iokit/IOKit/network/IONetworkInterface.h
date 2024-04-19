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
 * IONetworkInterface.h
 *
 * HISTORY
 * 8-Jan-1999       Joe Liu (jliu) created.
 */

#ifndef _IONETWORKINTERFACE_H
#define _IONETWORKINTERFACE_H

#include <IOKit/IOService.h>
#include <libkern/c++/OSString.h>
#include <libkern/c++/OSData.h>
#include <libkern/c++/OSArray.h>
#include <libkern/c++/OSSet.h>
#include <IOKit/IOLocks.h>
#include <IOKit/network/IONetworkData.h>
#include <IOKit/network/IONetworkStats.h>
#include <IOKit/network/IONetworkMedium.h>

struct mbuf;                // forward declarations.
struct ifnet;
class  IONetworkController;
class  IONetworkStack;

// Property table keys.
//
#define kIONetworkData            "IONetworkData"          // OSDictionary
#define kIOInterfaceType          "IOInterfaceType"        // OSNumber:8
#define kIOMaxTransferUnit        "IOMaxTransferUnit"      // OSNumber:32
#define kIOMediaAddressLength     "IOMediaAddressLength"   // OSNumber:8
#define kIOMediaHeaderLength      "IOMediaHeaderLength"    // OSNumber:8
#define kIOInterfaceFlags         "IOInterfaceFlags"       // OSNumber:16
#define kIOInterfaceExtraFlags    "IOInterfaceExtraFlags"  // OSNumber:32

/*! @typedef IOOutputAction
    @discussion Prototype for an output packet handler that will receive
    all outbound packets sent to the interface from the network layer.
    This handler is registered by calling registerOutputHandler().
    @param m A packet mbuf. */

typedef UInt32 (OSObject::*IOOutputAction)(struct mbuf * m);

/*! @typedef BPF_FUNC
    @discussion Prototype for the BPF tap handler. This will disappear
    when the appropriate DLIL header file is included. */

typedef int (*BPF_FUNC)(struct ifnet *, struct mbuf *);

/*! @enum IOFilterTapMode
    @constant kIOFilterTapModeInternal The filter tap will automatically
    tap into the packet stream inside the interface object for a given
    direction (that matches the filter's direction). This is the default mode.
    @constant kIOFilterTapModeExternal The filter tap will only receive
    packets when the feedInputFilterTap() or feedOutputFilterTap() methods
    are called (probably by a controller driver). This allows the filter
    to tap at a place that is logically closer to the network media. */

typedef enum {
    kIOFilterTapModeInternal,
    kIOFilterTapModeExternal,
} IOFilterTapMode;

// Network events.
//
enum {
    /* A new network medium was selected by the controller */
    kIONetworkEventMediumChange  = 0xff000001,

    /* A link change was detected */
    kIONetworkEventLinkChange    = 0xff000002,
};

/*! @class IONetworkInterface
    @abstract An IONetworkInterface object manages the connection
    between an IONetworkController and the network layer (DLIL).
    All interaction between the controller and DLIL must flow
    through an interface object. Any data structures required by
    the DLIL for a given "network interface" type is allocated and
    mantained by the interface object. Just as IONetworkController
    may be subclassed to satisfy the needs of a particular network
    family (i.e. Ethernet), a corresponding IONetworkInterface
    subclass should be created.
    
    Although most drivers will allocate a single interface object.
    It is possible for multiple interfaces to be attached to a single
    controller. The controller driver is responsible for arbitrating
    requests among its interface clients.
    
    IONetworkInterface also maintains a dictionary of IONetworkData
    objects containing statistics structures. Controller drivers can
    ask for a particular data object by name and update the
    statistics counters within directly. This dictionary is added to
    the interface's property table and is visible outside of the kernel.
    
    The inbound and outbound packet flow through the interface may be
    tapped by an external entity. By default, the interface will
    manage the filter taps automatically. But a controller driver may
    disable the default behavior to have a better control over when
    and which packet are fed to the filter taps.
    */

class IONetworkInterface : public IOService
{
    OSDeclareAbstractStructors(IONetworkInterface)

    friend class IONetworkStack;
    friend class IONetworkUserClient;

private:
    IONetworkController *    _provider;
    struct ifnet *           _ifp;
    IORecursiveLock *        _ifLock;
    OSSet *                  _clientSet;
    bool                     _registered;
    IOFilterTapMode          _inputFilterTapMode;
    IOFilterTapMode          _outputFilterTapMode;
    BPF_FUNC                 _inputFilterFunc;
    BPF_FUNC                 _outputFilterFunc;
    OSObject *               _target;
    IOOutputAction           _outAction;
    IOService *              _client;
    OSDictionary *           _dataDict;
    IOMediumType             _bsdMediaType;
    struct mbuf *            inputQHead;
    struct mbuf *            inputQTail;

    bool _copyNetworkDataDictToPropertyTable();
    void _handleMediumAndLinkChangeEvent();
    bool _setFilterTapMode(IOFilterTapMode * modePtr, IOFilterTapMode mode);
    bool _setInterfaceProperty(UInt32   value,
                               UInt32   mask,
                               UInt32   bytes,
                               void *   addr,
                               char *   name,
                               bool     restrict);

    SInt syncSIOCSIFMEDIA(IONetworkController * ctlr, struct ifreq * ifr);
    SInt syncSIOCGIFMEDIA(IONetworkController * ctlr, struct ifreq * ifr);
    SInt syncSIOCSIFMTU(IONetworkController * ctlr, struct ifreq * ifr);
    SInt syncPerformCommand(IONetworkController * ctlr,
                            UInt32                cmd,
                            void *                arg0,
                            void *                arg1);

    static int  ioctl_shim(struct ifnet * ifp, u_long cmd, caddr_t data);
    static int  set_bpf_tap_shim(struct ifnet * ifp, int mode, BPF_FUNC func);
    static int  free_shim(struct ifnet * ifp);
    static int  output_shim(struct ifnet * ifp, struct mbuf *m);
    static void null_shim(struct ifnet * ifp);

public:

/*! @function init
    @abstract Initialize an IONetworkInterface instance.
    @discussion Initialize instance variables, call getIfnet() to
    get the ifnet structure allocated by a concrete subclass,
    then call initIfnet() to initialize the ifnet structure.
    @param properties A property dictionary.
    @result true if initialized successfully, false otherwise. */

    virtual bool init(OSDictionary * properties = 0);

/*! @function isRegistered
    @abstract Returns true if the interface has been registered with
    the network layer.
    @result true if registered, false if the network layer has no
    knowledge or reference to this network interface. */

    virtual bool isRegistered() const;

/*! @function inputPacket
    @abstract Handle a packet received by the controller.
    @discussion Called by a controller to pass a received packet
    to the network layer. Packets received by this method can
    also be placed on a queue local to the interface, that the controller
    can use to delay the packet handoff to the network layer, until all
    received packets have been transferred to the queue. A subsequent call
    of flushInputQueue(), or inputPacket() with the queue argument set to 
    false, will cause all queued packets (may be a single packet) to be 
    delivered to the network layer, by making a single dlil_input() call.
    Additional methods that manipulate the input queue are flushInputQueue() 
    and clearInputQueue(). This queue, which is nothing more than a chain of
    mbufs, is not protected by a lock since the controller is expected to
    manipulate the input queue from a single thread. If the input filter
    tap mode is set to kIOFilterTapModeInternal, then packets sent to
    this method are also fed to the input filter tap.
    @param m The packet mbuf containing the received frame.
    @param length If non zero, the mbuf will be truncated to the
           given length. If zero, then no truncation will take place.
    @param queue If true, the only action performed is to queue the
           input packet. Otherwise, the dlil_input() function is
           called to handoff all queued packets (including the packet
           passed in).
    @result The number of packets submitted to the network layer.
            Returns 0 if the packet was queued. */

    virtual UInt32 inputPacket(struct mbuf * m,
                               UInt32        length = 0,
                               bool          queue  = false);

/*! @function flushInputQueue
    @abstract Deliver all packets in the input queue to the network layer.
    @discussion Remove all packets from the input queue and
    submit them to the network layer by calling dlil_input(). This
    method should be used in connection with the inputPacket() method,
    to flush the input queue after inputPacket() has queued up
    all received packets. See inputPacket() and clearInputQueue().
    @result The number of packets submitted to the network layer.
            May be zero if the queue is empty. */

    virtual UInt32 flushInputQueue();

/*! @function clearInputQueue
    @abstract Discard all packets in the input queue.
    @discussion Remove all packets from the input queue and
    release them back to the free buffer pool. The packets are not
    delivered to the network layer. Also see flushInputQueue().
    @result The number of packets released. */

    virtual UInt32 clearInputQueue();

/*! @function inputEvent
    @abstract Deliver an event to the network layer.
    @discussion Called by the controller driver to send a network event
    to the network layer. Possible applications include: media changed
    events, power management events, controller state change events.
    @param eventType The event type.
    @param arg An argument associated with the event. */

    virtual void inputEvent(UInt32 eventType, void * arg);

/*! @function registerOutputHandler
    @abstract Register the output handler.
    @discussion The interface will forward all output packets, sent
    from the network layer, to the output handler registered through
    this method. Until a handler is registered, handleClientOpen()
    will refuse all client opens. The output handler cannot be changed
    when the interface is registered. 
    @param target Target object that implements the output action.
    @param action The action which handles output packets.
    @result true if the handler provided was accepted, false otherwise. */

    virtual bool registerOutputHandler(OSObject *      target,
                                       IOOutputAction  action);

/*! @function setInputFilterTapMode
    @abstract Set the mode of the input packet tap
    @discussion To specify how the filter tap will connect to the input packet 
    stream when the tap becomes enabled. See IOFilterTapMode enumeration for
    a description of the modes. This method will refuse to change the mode
    when the interface is registered.
    @param The new mode.
    @result true if the new mode was accepted, false otherwise. */

    virtual bool setInputFilterTapMode(IOFilterTapMode mode);

/*! @function setOutputFilterTapMode
    @abstract Set the mode of the output packet tap
    @discussion To specify how the filter tap will connect to the output packet
    stream once the tap becomes enabled. See IOFilterTapMode enumeration for
    a description of the modes. This method will refuse to change the mode
    when the interface is registered.
    @param The new mode.
    @result true if the new mode was accepted, false otherwise. */

    virtual bool setOutputFilterTapMode(IOFilterTapMode mode);

/*! @function getInputFilterTapMode
    @abstract Get the input filter tap mode.
    @result Returns the current mode of the input packet tap. */

    virtual IOFilterTapMode getInputFilterTapMode() const;

/*! @function getOutputFilterTapMode
    @abstract Get the output filter tap mode.
    @result Returns the current mode of the input packet tap. */

    virtual IOFilterTapMode getOutputFilterTapMode() const;

/*! @function feedInputFilterTap
    @abstract Feed a packet to the input filter tap.
    @discussion
    This method should not be called if the input filter tap mode is
    set to kIOFilterTapModeInternal, since the tap would already
    receive every input packet that flows through the interface.
    A further call to feedInputFilterTap() would cause
    the tap to receive multiple copies of the same input packet.
    @param pkt The packet mbuf to pass to the input tap. The rcvif
    field in the mbuf is modified by this method. */

    virtual void feedInputFilterTap(struct mbuf * pkt);
    
/*! @function feedOutputFilterTap
    @abstract Feed a packet to the output filter tap.
    @discussion
    This method should not be called if the output filter tap mode is
    set to kIOFilterTapModeInternal, since the tap would already
    receive every output packet that flows through the interface.
    A further call to feedOutputFilterTap() would cause
    the tap to receive multiple copies of the same output packet.
    @param pkt The packet mbuf to pass to the output tap. */

    virtual void feedOutputFilterTap(struct mbuf * pkt);

/*! @function getNamePrefix
    @abstract Get a name prefix for the interface.
    @discussion The name of the interface advertised to the network layer
    is generated by concatenating the string returned by this method,
    and an integer unit number assigned by an external entity.
    A family specific subclass of IONetworkInterface must implement
    this method to assign a consistent name for all interfaces that
    are members of the same family.

    @result A pointer to a constant string. */

    virtual const char * getNamePrefix() const = 0;

/*! @function getInterfaceName
    @discussion The if_name field in the ifnet structure is returned.
    @result Pointer to a string containing the interface name. NULL
    if a name has not yet been assigned. */

    virtual const char * getInterfaceName() const;

/*! @function getInterfaceType
    @discussion The if_data.ifi_type field in the ifnet structure is returned.
    @result The interface type that matches one of the types
    defined in bsd/net/if_types.h. */

    virtual UInt8  getInterfaceType() const;

/*! @function getMaxTransferUnit
    @discussion The if_data.ifi_mtu field in the ifnet structure is returned.
    @result The interface MTU size. For Ethernet (not jumbo frames), this
    should be 1500. */

    virtual UInt32 getMaxTransferUnit() const;

/*! @function getFlags
    @discussion The if_flags field in the ifnet structure is returned.
    @result The interface flags. */

    virtual UInt16 getFlags() const;

/*! @function getExtraFlags
    @discussion The if_eflags field in the ifnet structure is returned.
    @result The interface extra flags. */

    virtual UInt32 getExtraFlags() const;

/*! @function getMediaAddressLength
    @discussion The if_data.ifi_addrlen field in the ifnet structure
    is returned.
    @result The size of the media or link address. For Ethernet,
    this should be 6. */

    virtual UInt8  getMediaAddressLength() const;

/*! @function getMediaHeaderLength
    @discussion The if_data.ifi_hdrlen field in the ifnet structure
    is returned.
    @result The media header length. For Ethernet, this should be 14. */

    virtual UInt8  getMediaHeaderLength() const;

/*! @function getUnitNumber
    @discussion The if_unit field in the ifnet structure is returned.
    @result The assigned interface unit number. */

    virtual UInt16 getUnitNumber() const;

/*! @function setInterfaceName
    @discussion Update the if_name field in the ifnet structure.
    @param name A new name for the interface.
    @result true if the interface is unregistered
    and the update was successful, false otherwise. */

    virtual bool setInterfaceName(const char * name);
   
/*! @function setInterfaceType
    @discussion Update the if_data.ifi_type field in the ifnet structure.
    @param type The type of the interface. See bsd/net/if_types.h for the
    defined types.
    @result true if the interface is unregistered
    and the update was successful, false otherwise. */

    virtual bool setInterfaceType(UInt8 type);

/*! @function setMaxTransferUnit
    @discussion Update the if_data.ifi_mtu field in the ifnet structure.
    @param mtu The interface MTU size.
    @result true if the interface is unregistered
    and the update was successful, false otherwise. */

    virtual bool setMaxTransferUnit(UInt32 mtu);

/*! @function setFlags
    @discussion Perform a read-modify-write operation on the if_flags 
    field in the ifnet structure.
    @param flags The flag bits that should be set.
    @param clear The flag bits that should be cleared. If 0, then non
    of the flags are cleared and the result is formed by OR'ing the
    original flags with the new flags.
    @result true if the interface is unregistered
    and the update was successful, false otherwise. */

    virtual bool setFlags(UInt16 flags, UInt16 clear = 0);

/*! @function setExtraFlags
    @discussion Perform a read-modify-write operation on the if_eflags 
    field in the ifnet structure.
    @param flags The flags that should be set.
    @param clear The flags that should be cleared. If 0, then non
    of the flags are cleared and the result is formed by OR'ing the
    original flags with the new flags.
    @result true if the interface is unregistered
    and the update was successful, false otherwise. */

    virtual bool setExtraFlags(UInt32 flags, UInt32 clear = 0);

/*! @function setMediaAddressLength
    @discussion Update the if_data.ifi_addrlen field in the ifnet structure.
    @param length The new media address length.
    @result true if the interface is unregistered
    and the update was successful, false otherwise. */

    virtual bool setMediaAddressLength(UInt8 length);

/*! @function setMediaHeaderLength
    @discussion Update the if_data.ifi_hdrlen field in the ifnet structure.
    @param length The new media header length.
    @result true if the interface is unregistered
    and the update was successful, false otherwise. */

    virtual bool setMediaHeaderLength(UInt8 length);

/*! @function setUnitNumber
    @discussion Update the if_unit field in the ifnet structure.
    @param unit The unit number to assign to this interface.
    @result true if the interface is unregistered
    and the unit number was accepted, false otherwise. */

    virtual bool setUnitNumber(UInt16 unit);

/*! @function addNetworkData
    @abstract Add an IONetworkData object to a dictionary kept by
    the interface.
    @param aData An IONetworkData object.
    @result true if the data object was added successfully, false otherwise. */

    virtual bool addNetworkData(IONetworkData * aData);

/*! @function removeNetworkData
    @abstract Remove an entry from the dictionary of IONetworkData
    objects.
    @param aKey An OSSymbol key for an IONetworkData entry in the
           dictionary.
    @result True if completed without errors,
    false if the operation was aborted. */

    virtual bool removeNetworkData(const OSSymbol * aKey);

/*! @function removeNetworkData
    @abstract Remove an entry from the dictionary of IONetworkData
    objects.
    @param aKey A C string key for an IONetworkData entry in the
           dictionary.
    @result True if completed without errors,
    false if the operation was aborted. */

    virtual bool removeNetworkData(const char * aKey);

/*! @function getNetworkData
    @abstract Get an entry from the dictionary of IONetworkData objects.
    @discussion Perform a lookup of the dictionary kept by the interface,
    and return an entry that matches the specified string key.
    @param aKey Search for an entry with this string key.
    @result The matching entry, or 0 if no match was found. */

    virtual IONetworkData * getNetworkData(const char * aKey) const;

/*! @function getNetworkData
    @abstract Get an entry from the dictionary of IONetworkData objects.
    @discussion Perform a lookup of the dictionary kept by the interface,
    and return an entry that matches the specified OSSymbol key.
    @param aKey Search for an entry with this OSSymbol key.
    @result The matching entry, or 0 if no match was found. */

    virtual IONetworkData * getNetworkData(const OSSymbol * aKey) const;

    // Compatibility method (to be removed)
    inline IONetworkData * getParameter(const char * aKey) const
    { return getNetworkData(aKey); }

protected:

/*! @function free
    @abstract Free the IONetworkInterface instance.
    @discussion Resource allocated during init are released, and
    clearInputQueue() is called to make sure the input queue is clean. */

    virtual void free();

/*! @function handleOpen
    @abstract Handle a client open on the interface.
    @discussion The open() method in 
    IOService calls this method with the arbitration lock held, and this 
    method must return true to accept the client open. This method will
    in turn call handleClientOpen() and controllerDidOpen(). Subclasses
    must not override this method.
    @param client The client object that requested the open.
    @param options Not used. See IOService.
    @param argument Not used. See IOService.
    @result true to accept the client open, false otherwise. */

    virtual bool handleOpen(IOService *  client,
                            IOOptionBits options,
                            void *       argument);

/*! @function handleClose
    @abstract Handle a close by a client.
    @discussion The close() method in
    IOService calls this method with the arbitration lock held. This method 
    will in turn call handleClientClose() and controllerWillClose().
    Subclasses must not override this method.
    @param client The client object that requested the close.
    @param options Not used. See IOService. */

    virtual void handleClose(IOService * client, IOOptionBits options);

/*! @function handleIsOpen
    @abstract Query a client that has an open on the controller.
    @discussion This method is always called by IOService with the
    arbitration lock held. Subclasses must not override this method.
    @result true if the specified client, or any client if none is
    specified, presently has an open on this object. */

    virtual bool handleIsOpen(const IOService * client) const;

/*! @function lock
    @abstract Take the interface lock.
    @discussion Take the recursive lock that protects the interface
    data and state. All updates to the interface state and to the
    ifnet structure fields are protected by this lock. */
    
    virtual void lock();

/*! @function unlock
    @abstract Release the interface lock.
    @discussion Release the recursive lock that protects the interface
    data and state. */

    virtual void unlock();

/*! @function controllerDidOpen
    @abstract Prepare the controller after it has been opened.
    @discussion Called by handleOpen() to configure the controller after it
    has just been opened. The open on the controller occurs after the
    interface receives the initial open request from a client. Subclasses
    can override this method and setup the controller before allowing the
    client open. The implementation in the subclass must call the method in
    super and check the return value.
    @param controller The controller that was opened.
    @result Must return true in order for handleOpen() to accept 
    the client open. If the return is false, then the controller will be
    closed and the client open will be rejected. */

    virtual bool controllerDidOpen(IONetworkController * controller);

/*! @function controllerWillClose
    @abstract Quiesce the controller before it is closed.
    @discussion Called by handleClose() after receiving a close from the
    last client, and just before the controller is closed. Subclasses
    can override this method to perform any cleanup action before the 
    controller is closed.
    @param controller The controller that will be closed. */

    virtual void controllerWillClose(IONetworkController * controller);

/*! @function performCommand
    @abstract Handle commands from the network layer.
    @discussion Handles generic socket ioctl commands sent to the interface.
    IONetworkInterface handles commands that are common to all network
    families. A subclass of IONetworkInterface may override this method
    in order to handle the same command that is handled here, but in a
    different manner, or (the more like case) to augment the command
    handling to include additional commands, and call super for any
    commands not handled in the subclass.
    The ioctl commands handled by IONetworkInterface are
        SIOCGIFMTU (Get interface MTU size),
        SIOCSIFMTU (Set interface MTU size),
        SIOCSIFMEDIA (Set media), and
        SIOCGIFMEDIA (Get media and link status).
    @param controller The controller object.
    @param cmd The command code.
    @param arg0 Command argument.
    @param arg1 Command argument.
    @result A BSD return code defined in bsd/sys/errno.h. */

    virtual SInt performCommand(IONetworkController * controller,
                                UInt32                cmd,
                                void *                arg0,
                                void *                arg1);

/*! @function getIfnet
    @abstract Get the ifnet structure allocated by the interface object.
    @discussion Request an interface to reveal its ifnet structure.
    A concrete subclass must allocate an ifnet structure when the
    object is initialized, and return a pointer to the ifnet structure
    when this method is called.
    @result Pointer to an ifnet structure allocated by a concrete
    interface subclass. */

    virtual struct ifnet * getIfnet() const = 0;

/*! @function initIfnet
    @abstract Initialize the ifnet structure.
    @discussion Subclasses must override
    this method and initialize the ifnet structure given in a family specific
    manner. Subclasses may use the "setter" methods such as setFlags() to
    initialize the ifnet fields. The implementation in the subclass must call
    the version in super before it returns, to allow IONetworkInterface to
    complete the initialization.
    @param ifp Pointer to an ifnet structure obtained earlier through the
               getIfnet() call.
    @result true on success, false otherwise. */

    virtual bool initIfnet(struct ifnet * ifp);

/*! @function handleClientOpen
    @abstract Handle a client open.
    @discussion Called by handleOpen() to qualify a client object that
    is attempting to open the interface.
    @param controller The controller object (provider).
    @param client The client object.
    @result true to accept the client open, false to refuse the open. */

    virtual bool handleClientOpen(IONetworkController * controller,
                                  IOService *           client);

/*! @function handleClientClose
    @abstract Handle a client close.
    @discussion Called by handleOpen() to notify that a client object has
    closed the interface.
    @param controller The controller object (provider).
    @param client The client object. */

    virtual void handleClientClose(IONetworkController * controller,
                                   IOService *           client);

/*! @function newUserClient
    @abstract Create a new user client.
    @discussion Create a new IOUserClient to handle userspace client requests.
    The default implementation will create an IONetworkUserClient instance
    if the type given is kIONUCType.
    @param owningTask See IOService.
    @param security_id See IOService.
    @param type See IOService.
    @param handler See IOService.
    @result kIOReturnSuccess if an IONetworkUserClient was created. */

    virtual IOReturn newUserClient(task_t           owningTask,
                                   void *           security_id,
                                   UInt32           type,
                                   IOUserClient **  handler);

/*! @function setInterfaceNameInt
    @discussion Update the if_name field in the ifnet structure.
    @param name A new name for the interface.
    @result true if the update was successful, false otherwise. */

    virtual bool setInterfaceNameInt(const char * name);
    
/*! @function setInterfaceTypeInt
    @discussion Update the if_data.ifi_type field in the ifnet structure.
    @param type The type of the interface. See bsd/net/if_types.h for the
    defined types.
    @result true if the update was successful, false otherwise. */

    virtual bool setInterfaceTypeInt(UInt8 type);

/*! @function setMaxTransferUnitInt
    @discussion Update the if_data.ifi_mtu field in the ifnet structure.
    @param mtu The interface MTU size.
    @result true if the update was successful, false otherwise. */

    virtual bool setMaxTransferUnitInt(UInt32 mtu);

/*! @function setFlagsInt
    @discussion Perform a read-modify-write operation on the if_flags 
    field in the ifnet structure.
    @param flags The flag bits that should be set.
    @param clear The flag bits that should be cleared. If 0, then non
    of the flags are cleared and the result is formed by OR'ing the
    original flags with the new flags.
    @result true if the update was successful, false otherwise. */

    virtual bool setFlagsInt(UInt16 flags, UInt16 clear = 0);

/*! @function setExtraFlagsInt
    @discussion Perform a read-modify-write operation on the if_eflags 
    field in the ifnet structure.
    @param flags The flags that should be set.
    @param clear The flags that should be cleared. If 0, then non
    of the flags are cleared and the result is formed by OR'ing the
    original flags with the new flags.
    @result true if the update was successful, false otherwise. */

    virtual bool setExtraFlagsInt(UInt32 flags, UInt32 clear = 0);

/*! @function setMediaAddressLengthInt
    @discussion Update the if_data.ifi_addrlen field in the ifnet structure.
    @param length The new media address length.
    @result true if the field was updated, false otherwise. */

    virtual bool setMediaAddressLengthInt(UInt8 length);

/*! @function setMediaHeaderLengthInt
    @discussion Update the if_data.ifi_hdrlen field in the ifnet structure.
    @param length The new media header length.
    @result true if the update was successful, false otherwise. */

    virtual bool setMediaHeaderLengthInt(UInt8 length);

/*! @function setUnitNumberInt
    @discussion Update the if_unit field in the ifnet structure.
    @param unit The unit number to assign to this interface.
    @result true if the unit number was accepted, false otherwise. */

    virtual bool setUnitNumberInt(UInt16 unit);
};

#endif /* !_IONETWORKINTERFACE_H */
