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
 * HISTORY
 *
 */


#ifndef _IOKIT_IOUSBCONTROLLER_H
#define _IOKIT_IOUSBCONTROLLER_H

#include <IOKit/IOService.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <libkern/c++/OSArray.h>
#include <IOKit/IODeviceMemory.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOCommandGate.h>

#include <IOKit/usb/USB.h>
#include <IOKit/usb/IOUSBBus.h>
#include <IOKit/usb/IOUSBDevice.h>
#include <IOKit/usb/USBHub.h>

/*
 * USB Command
 * This is the command block for a USB command to be queued on the
 * Command Gate of the WorkLoop.  Although the actual work of creating
 * the command and enqueueing it is done for them via the deviceRequest,
 * read, and write methods, this is the primary way that devices will get
 * work done.
 * Note: This is private to IOUSBController and should be moved to a
 * private header.
 */

typedef enum {
    DEVICE_REQUEST,	// Device request using pointer
    READ,
    WRITE,
    CREATE_EP,
    DELETE_EP,
    DEVICE_REQUEST_DESC	// Device request using descriptor
} usbCommand;

typedef struct {
    usbCommand		selector;
    IOUSBDevRequest	*request;
    USBDeviceAddress	address;
    UInt8		endpoint;
    UInt8		direction;
    UInt8		type;
    IOMemoryDescriptor 	*buffer;
    IOUSBCompletion	completion;
    UInt32		dataRemaining;	// For Control transfers
    UInt8		stage;		// For Control transfers
    IOReturn		status;
} IOUSBCommand;

typedef struct {
    usbCommand		selector;
    USBDeviceAddress	address;
    UInt8		endpoint;
    UInt8		direction;
    IOMemoryDescriptor *buffer;
    IOUSBIsocCompletion	completion;
    UInt64		startFrame;
    UInt32		numFrames;
    IOUSBIsocFrame *	frameList;
    IOReturn		status;
} IOUSBIsocCommand;

/*
 * Errata
 * These indicate the anomolies of the various chips sets.  Test
 * for these in errataBits.
 */
enum {
    // turn off UHCI test mode
    kErrataCMDDisableTestMode		= (1 << 0),
    // Don't cross page boundaries in a single transfer
    kErrataOnlySinglePageTransfers	= (1 << 1),
    // UIM will retry out transfers with buffer underrun errors
    kErrataRetryBufferUnderruns		= (1 << 2),
    // UIM will insert delay buffer between HS and LS transfers
    kErrataLSHSOpti			= (1 << 3),
    // Always set the NOCP bit in rhDescriptorA register
    kErrataDisableOvercurrent		= (1 << 4),
    // Don't allow port suspend at the root hub
    kErrataLucentSuspendResume		= (1 << 5)
};
/* errataBits */

/*
    This table contains the list of errata that are necessary for known problems with particular devices.
    The format is vendorID, product ID, lowest revisionID needing errata, highest rev needing errata, errataBits.
    The result of all matches is ORed together, so more than one entry may match.
    Typically for a given errata a list of revisions that this applies to is supplied.
*/

struct ErrataListEntryStruct {
    UInt16 				vendID;
    UInt16 				deviceID;
    UInt16 				revisionLo;
    UInt16 				revisionHi;
    UInt32 				errata;
};

typedef struct ErrataListEntryStruct
                    ErrataListEntry,
                    *ErrataListEntryPtr;

class IOUSBController;
class IOUSBDeviceZero;
class IOUSBRootHubDevice;
class IOMemoryDescriptor;

void IOUSBSyncCompletion(void *target, void * parameter,
                                  IOReturn	status,
                                  UInt32	bufferSizeRemaining);

void  IOUSBSyncIsoCompletion(void *target, void * parameter,
                                  IOReturn	status,
                                  IOUSBIsocFrame *pFrames);



/*!
    @class IOUSBController
    @abstract Base class for USB hardware driver
    @discussion Not many directly useful methods for USB device driver writers,
    IOUSBDevice, IOUSBInterface and IOUSBPipe provide more useful abstractions.
    The bulk of this class interfaces between IOKit and the low-level UIM, which is
    based on the MacOS9 UIM.
*/
class IOUSBController : public IOUSBBus
{
    OSDeclareAbstractStructors(IOUSBController)

protected:

    IOWorkLoop 		*_workLoop;
    IOCommandGate 	*_commandGate;

    IOUSBRootHubDevice *_rootHubDevice;

    IOLock *		_devZeroLock;

    static IOReturn doDeleteEP(OSObject *owner,
                               void *arg0, void *arg1,
                               void *arg2, void *arg3);
    static IOReturn doCreateEP(OSObject *owner,
                               void *arg0, void *arg1,
                               void *arg2, void *arg3);

    static IOReturn doControlTransfer(OSObject *owner,
                               void *arg0, void *arg1,
                               void *arg2, void *arg3);
    static IOReturn doIOTransfer(OSObject *owner,
                               void *arg0, void *arg1,
                               void *arg2, void *arg3);
    static IOReturn doIsocTransfer(OSObject *owner,
                               void *arg0, void *arg1,
                               void *arg2, void *arg3);
    virtual bool init(OSDictionary * propTable);
    virtual bool start( IOService * provider );

    virtual IOUSBDevice *newDevice();
    IOReturn	CreateDevice(IOUSBDevice		*device,
			     USBDeviceAddress		deviceAddress,
                             IOUSBDeviceDescriptor	*desc,
                             UInt8			speed,
                             UInt32			powerAvailable);
    IOReturn	getNubResources( IOService * regEntry );
    USBDeviceAddress	getNewAddress(void);
    IOReturn    ControlTransaction(IOUSBCommand *command);
    IOReturn    InterruptTransaction(IOUSBCommand *command);
    IOReturn    BulkTransaction(IOUSBCommand *command);
    IOReturn    IsocTransaction(IOUSBIsocCommand *command);
    void	ControlPacketHandler(
                                void * 		parameter,
                                IOReturn	status,
                                UInt32		bufferSizeRemaining);
    void	InterruptPacketHandler(
                                void * 		parameter,
                                IOReturn	status,
                                UInt32		bufferSizeRemaining);
    void	BulkPacketHandler(
                                void * 		parameter,
                                IOReturn	status,
                                UInt32		bufferSizeRemaining);

    void	IsocCompletionHandler(
				void * 	parameter,
                                IOReturn	status,
                                IOUSBIsocFrame	*pFrames);

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    // Invokes the specified completion action of the request.  If
    // the completion action is unspecified, no action is taken.
    inline void complete(IOUSBCompletion	completion,
                         IOReturn		status,
                         UInt32			actualByteCount = 0);

    void freeCommand(IOUSBCommand * command);

    virtual UInt32 GetErrataBits(UInt16 vendorID, UInt16 deviceID, UInt16 revisionID);

    /*
     * UIM methods
     */
    virtual IOReturn UIMInitialize(IOService * provider) = 0;
    virtual IOReturn UIMFinalize() = 0;
    // Control
    virtual IOReturn UIMCreateControlEndpoint(
            UInt8				functionNumber,
            UInt8				endpointNumber,
            UInt16				maxPacketSize,
            UInt8				speed) = 0;

    virtual IOReturn UIMCreateControlTransfer(
            short				functionNumber,
            short				endpointNumber,
            IOUSBCompletion			completion,
            void *				CBP,
            bool				bufferRounding,
            UInt32				bufferSize,
            short				direction) = 0;

    virtual IOReturn UIMCreateControlTransfer(
            short				functionNumber,
            short				endpointNumber,
            IOUSBCompletion			completion,
            IOMemoryDescriptor *		CBP,
            bool				bufferRounding,
            UInt32				bufferSize,
            short				direction) = 0;
    
    // Bulk
    virtual IOReturn UIMCreateBulkEndpoint(
            UInt8				functionNumber,
            UInt8				endpointNumber,
            UInt8				direction,
            UInt8				speed,
            UInt8				maxPacketSize) = 0;

    virtual IOReturn UIMCreateBulkTransfer(
            short				functionNumber,
            short				endpointNumber,
            IOUSBCompletion			completion,
            IOMemoryDescriptor *		CBP,
            bool				bufferRounding,
            UInt32				bufferSize,
            short				direction) = 0;

    // Interrupt
    virtual IOReturn UIMCreateInterruptEndpoint(
            short				functionAddress,
            short				endpointNumber,
            UInt8				direction,
            short				speed,
            UInt16				maxPacketSize,
            short				pollingRate) = 0;

    virtual IOReturn UIMCreateInterruptTransfer(
            short				functionNumber,
            short				endpointNumber,
            IOUSBCompletion			completion,
            IOMemoryDescriptor *		CBP,
            bool				bufferRounding,
            UInt32				bufferSize,
            short				direction) = 0;

    // Isoch
    virtual IOReturn UIMCreateIsochEndpoint(
            short				functionAddress,
            short				endpointNumber,
            UInt32				maxPacketSize,
            UInt8				direction) = 0;

    virtual IOReturn UIMCreateIsochTransfer(
            short			functionAddress,
            short			endpointNumber,
            IOUSBIsocCompletion		completion,
            UInt8			direction,
            UInt64			frameStart,
            IOMemoryDescriptor *	pBuffer,
            UInt32			frameCount,
            IOUSBIsocFrame		*pFrames) = 0;

    virtual IOReturn UIMAbortEndpoint(
            short				functionNumber,
            short				endpointNumber,
            short				direction) = 0;

    virtual IOReturn UIMDeleteEndpoint(
            short				functionNumber,
            short				endpointNumber,
            short				direction) = 0;
    
    virtual IOReturn UIMClearEndpointStall(
            short				functionNumber,
            short				endpointNumber,
            short				direction) = 0;

    virtual void UIMRootHubStatusChange(void) = 0;

public:

    /*!
	@struct Endpoint
        Describes an endpoint of a device.
	Simply an easier to use version of the endpoint descriptor.
        @field descriptor The raw endpoint descriptor.
        @field number Endpoint number
	@field direction Endpoint direction: kUSBOut, kUSBIn, kUSBAnyDirn
	@field transferType Type of endpoint: kUSBControl, kUSBIsoc, kUSBBulk, kUSBInterrupt
	@field maxPacketSize Maximum packet size for endpoint
	@field interval Polling interval in milliseconds (only relevent for Interrupt endpoints)
    */
    struct Endpoint {
        IOUSBEndpointDescriptor	*descriptor;
        UInt8 			number;
        UInt8			direction;	// in, out
        UInt8			transferType;	// cntrl, bulk, isoc, int
        UInt16			maxPacketSize;
        UInt8			interval;
    };

    // Implements IOService::getWorkLoop const member function
    virtual IOWorkLoop *getWorkLoop() const;
    
    /*
     * Root hub methods
     * Only of interest to the IOUSBRootHubDevice object
     */
    virtual IOReturn getRootHubDeviceDescriptor(IOUSBDeviceDescriptor *desc) = 0;
    virtual IOReturn getRootHubDescriptor(IOUSBHubDescriptor *desc) = 0;
    virtual IOReturn setRootHubDescriptor(OSData *buffer) = 0;
    virtual IOReturn getRootHubConfDescriptor(OSData *desc) = 0;
    virtual IOReturn getRootHubStatus(IOUSBHubStatus *status) = 0;
    virtual IOReturn setRootHubFeature(UInt16 wValue) = 0;
    virtual IOReturn clearRootHubFeature(UInt16 wValue) = 0;
    virtual IOReturn getRootHubPortStatus(IOUSBHubPortStatus *status,
                                                UInt16 port) = 0;
    virtual IOReturn setRootHubPortFeature(UInt16 wValue, UInt16 port) = 0;
    virtual IOReturn clearRootHubPortFeature(UInt16 wValue, UInt16 port) = 0;
    virtual IOReturn getRootHubPortState(UInt8 *state, UInt16 port) = 0;
    virtual IOReturn setHubAddress(UInt16 wValue) = 0;

    /*!
	@function openPipe
        Open a pipe to the specified device endpoint
        @param address Address of the device on the USB bus
	@param speed of the device: kUSBHighSpeed or kUSBLowSpeed
        @param endpoint description of endpoint to connect to
    */
    virtual IOReturn openPipe(USBDeviceAddress address, UInt8 speed,
                              Endpoint *endpoint);
    /*!
        @function closePipe
        Close a pipe to the specified device endpoint
        @param address Address of the device on the USB bus
	@param endpoint description of endpoint
    */
    virtual IOReturn closePipe(USBDeviceAddress address,
                               Endpoint * endpoint);

    // Controlling pipe state
    /*!
        @function abortPipe
        Abort pending I/O to/from the specified endpoint, causing them to complete
	with return code kIOReturnAborted
        @param address Address of the device on the USB bus
        @param endpoint description of endpoint
    */
    virtual IOReturn abortPipe(USBDeviceAddress address,
                               Endpoint * endpoint);
    /*!
        @function resetPipe
        Abort pending I/O and clear stalled state - this method is a combination of
	abortPipe and clearPipeStall
        @param address Address of the device on the USB bus
        @param endpoint description of endpoint
    */
    virtual IOReturn resetPipe(USBDeviceAddress address,
                               Endpoint * endpoint);
    /*!
        @function clearPipeStall
        Clear a pipe stall.
        @param address Address of the device on the USB bus
        @param endpoint description of endpoint
    */
    virtual IOReturn clearPipeStall(USBDeviceAddress address,
                                    Endpoint * endpoint);

    // Transferring Data
    /*!
        @function read
        Read from an interrupt or bulk endpoint
	@param buffer place to put the transferred data
        @param address Address of the device on the USB bus
        @param endpoint description of endpoint
	@param completion describes action to take when buffer has been filled 
    */
    virtual IOReturn read(IOMemoryDescriptor * 	buffer,
                          USBDeviceAddress 	address,
                          Endpoint *	endpoint,
                          IOUSBCompletion *	completion);
    /*!
        @function write
        Write to an interrupt or bulk endpoint
        @param buffer place to get the transferred data
        @param address Address of the device on the USB bus
        @param endpoint description of endpoint
        @param completion describes action to take when buffer has been emptied
    */
    virtual IOReturn write(IOMemoryDescriptor *	buffer,
                           USBDeviceAddress 	address,
                           Endpoint *	endpoint,
                           IOUSBCompletion *	completion);

    /*!
        @function isocIO
        Read from or write to an isochronous endpoint
        @param buffer place to put the transferred data
        @param frameStart USB frame number of the frame to start transfer
        @param numFrames Number of frames to transfer
        @param frameList Bytes to transfer and result for each frame
        @param address Address of the device on the USB bus
        @param endpoint description of endpoint
        @param completion describes action to take when buffer has been filled
    */
    virtual IOReturn isocIO(IOMemoryDescriptor * buffer,
                          UInt64 frameStart,
			  UInt32 numFrames,
                          IOUSBIsocFrame *frameList,
                          USBDeviceAddress 	address,
                          Endpoint *	endpoint,
                          IOUSBIsocCompletion *	completion);
    /*!
        @function deviceRequest
        Make a control request to the specified endpoint
	There are two versions of this method, one uses a simple void *
        to point to the data portion of the transfer, the other uses an
	IOMemoryDescriptor to point to the data.
	@param request parameter block for the control request
	@param completion describes action to take when the request has been executed
        @param address Address of the device on the USB bus
	@param epNum endpoint number
    */
    virtual IOReturn deviceRequest(IOUSBDevRequest *	request,
                                   IOUSBCompletion *	completion,
                                   USBDeviceAddress address, UInt8 epNum);
    virtual IOReturn deviceRequest(IOUSBDevRequestDesc *request,
                                   IOUSBCompletion *	completion,
                                   USBDeviceAddress address, UInt8 epNum);

    /*
     * Methods used by the hub driver to initialize a device
     */
    /*!
	@function AcquireDeviceZero
	Get the device zero lock - call this before resetting a device, to ensure there's
	only one device with address 0
    */
    virtual IOReturn AcquireDeviceZero();
    /*!
        @function ReleaseDeviceZero
        Release the device zero lock - call this to release the device zero lock,
	when there is no longer a device at address 0
    */
    virtual void ReleaseDeviceZero(void);

    /*!
        @function WaitForReleaseDeviceZero
        Block until the device zero lock is released
    */
    void	WaitForReleaseDeviceZero();

    /*!
        @function ConfigureDeviceZero
        create a pipe to the default pipe for the device at address 0
        @param maxPacketSize max packet size for the pipe
	@param speed Device speed
    */
    IOReturn	ConfigureDeviceZero(UInt8 maxPacketSize, UInt8 speed);
    /*!
        @function GetDeviceZeroDescriptor
        read the device descriptor of the device at address 0
	@param desc pointer to descriptor
    */
    IOReturn	GetDeviceZeroDescriptor(IOUSBDeviceDescriptor *desc);
    /*!
        @function SetDeviceZeroAddress
        Set the device address of the device currently at address 0.
	When this routine returns, it's safe to release the device zero lock.
	@param address New address for the device
	@param maxPacketSize maximum packet size for the default pipe
	@param speed speed of the device
    */
    IOReturn	SetDeviceZeroAddress(USBDeviceAddress address,
				UInt8 maxPacketSize, UInt8 speed);
    /*!
	@function MakeDevice
	Create a new device object for the device currently at address 0.
	This routine calls SetDeviceZeroAddress() with a new, unique, address for the device
	and adds the device into the registry.
	@param desc Device descriptor of the device
	@param speed Device speed
	@param power Bus power available to the device
	@result Pointer to the newly-created device, 0 if the object coudn't be created. 
    */
    IOUSBDevice *MakeDevice(IOUSBDeviceDescriptor *desc,
                                            UInt8 speed, UInt32 power);

    /*!
	@function GetBandwidthAvailable
        Returns the available bandwidth (in bytes) per frame for
	isochronous transfers.
	@result maximum number of bytes that a new iso pipe could transfer
	per frame given current allocations.
    */
    virtual UInt32 GetBandwidthAvailable() = 0;

    /*!
        @function GetFrameNumber
        Returns the full current frame number.
        @result The frame number.
    */
    virtual UInt64 GetFrameNumber() = 0;

    /*!
        @function GetFrameNumber32
        Returns the least significant 32 bits of the frame number.
        @result The lsb 32 bits of the frame number.
    */
    virtual UInt32 GetFrameNumber32() = 0;

    // Debugger polled mode
    virtual void pollInterrupts(IOUSBCompletionAction safeAction=0) = 0;
    virtual IOReturn PolledRead(
            short				functionNumber,
            short				endpointNumber,
            IOUSBCompletion			completion,
            IOMemoryDescriptor *		CBP,
            bool				bufferRounding,
            UInt32				bufferSize);
};

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Inline Functions

inline void IOUSBController::complete(IOUSBCompletion	completion,
                                      IOReturn		status,
                                      UInt32		actualByteCount)
{
    if (completion.action)  (*completion.action)(completion.target,
                                                 completion.parameter,
                                                 status,
                                                 actualByteCount);
}

#endif /* ! _IOKIT_IOUSBCONTROLLER_H */

