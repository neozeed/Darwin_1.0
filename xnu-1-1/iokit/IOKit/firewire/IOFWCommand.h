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
 *
 *	IOFWCommand.h
 *
 */
#ifndef _IOKIT_IOFWCOMMAND_H
#define _IOKIT_IOFWCOMMAND_H

#include <IOKit/IOCommandGate.h>
#include <IOKit/firewire/IOFWRegs.h>

class IOMemoryDescriptor;
class IOSyncer;
class IOFireWireBus;
class IOFireWireController;
class IOFireWireNub;
class IOFWAddressSpace;	// Description of chunk of local FW address space
class IOFWCommand;
class IOFWBusCommand;
struct AsyncPendingTrans;

// Callback when device command completes asynchronously
typedef void (*FWDeviceCallback)(void *refcon, IOReturn status, IOFireWireNub *device, IOFWCommand *fwCmd);

// Callback when bus command completes asynchronously
typedef void (*FWBusCallback)(void *refcon, IOReturn status, IOFireWireBus *bus, IOFWBusCommand *fwCmd);

/*
 * Base class for FireWire commands
 */
class IOFWCommand : public OSObject
{
    OSDeclareAbstractStructors(IOFWCommand)

protected:
    IOCommandGate *	fGate;
    IOFireWireController *fControl;
    IOSyncer *		fSyncWakeup;
    bool		fSync;

    virtual IOReturn	complete(IOReturn status) = 0;

public:
    IOReturn		fStatus;
    IOByteCount		fBytesTransferred;

    virtual bool	initWithGate(IOCommandGate *gate);

    /*	
     *	Submit the FWCommand to the FireWire command queue
     */                          
    virtual IOReturn 	submit();
    /*	
     *	Execute the FWCommand immediately (usually called from work loop)
     */                          
    virtual IOReturn	execute() = 0;
};

/*
 * Bus control commands
 */
class IOFWBusCommand : public IOFWCommand
{
    OSDeclareAbstractStructors(IOFWBusCommand)

protected:
    FWBusCallback	fComplete;
    void *		fRefCon;

    virtual IOReturn	complete(IOReturn status);

    virtual bool	initWithController(IOFireWireController *control,
				FWBusCallback completion=NULL, void *refcon=NULL);
    virtual IOReturn	reinit(FWBusCallback completion, void *refcon);
};

/*
 * Allocation of local address space, accessable to the
 * specified firewire device.
 * May also want the ability to change address mapping
 * instead of freeing old space and allocing new one.
 */
class IOFWAllocAddressCommand : public IOFWBusCommand
{
    OSDeclareDefaultStructors(IOFWAllocAddressCommand)

protected:
    IOFWAddressSpace *fSpace;

public:
    virtual IOReturn	execute();
    virtual bool	initWithSpace(IOFireWireNub *dev, IOFWAddressSpace *space,
				FWBusCallback completion, void *refcon);
    virtual IOReturn	reinit(IOFWAddressSpace *space,
				FWBusCallback completion=NULL, void *refcon=NULL);
};

/*
 * Deallocate the address range (make it inactive)
 */ 
class IOFWDeallocAddressCommand : public IOFWBusCommand
{
    OSDeclareDefaultStructors(IOFWDeallocAddressCommand)

protected:
    IOFWAddressSpace *fSpace;

public:
    virtual IOReturn	execute();
    virtual bool	initWithSpace(IOFireWireNub *dev, IOFWAddressSpace *space,
				FWBusCallback completion, void *refcon);
    virtual IOReturn	reinit(IOFWAddressSpace *space,
				FWBusCallback completion=NULL, void *refcon=NULL);
};

/*
 * Little command for local ROM instantiation
 */
class IOFWUpdateROM : public IOFWBusCommand
{
    OSDeclareDefaultStructors(IOFWUpdateROM)

public:
    virtual IOReturn	execute();
    virtual bool	initWithController(IOFireWireController *control,
				FWBusCallback completion=NULL, void *refcon=NULL);

};

/*
 * Send an async request to a device
 */
class IOFWAsyncCommand : public IOFWCommand
{
    OSDeclareAbstractStructors(IOFWAsyncCommand)

protected:
    IOFireWireNub *	fDevice;
    FWDeviceCallback	fComplete;
    void *		fRefCon;
    IOMemoryDescriptor *fMemDesc;
    AsyncPendingTrans *	fTrans;
    UInt32		fAddressHi;
    UInt32		fAddressLo;
    UInt32		fNodeID;
    UInt32		fGeneration;	// bus topology fNodeID is valid for.
    int			fSize;
    int			fSpeed;
    int			fMaxPack;
    bool		fFailOnReset;

    virtual IOReturn	complete(IOReturn status);
    virtual bool	initAll(IOFireWireController *control,
				IOFireWireNub *device, FWAddress devAddress,
				IOMemoryDescriptor *hostMem,
				FWDeviceCallback completion, void *refcon, bool failOnReset);
    virtual IOReturn	reinit(FWAddress devAddress, IOMemoryDescriptor *hostMem,
				FWDeviceCallback completion, void *refcon, bool failOnReset);

public:

    // To be called by IOFireWireController and derived classes.
    virtual void 	gotPacket(IOFireWireController* control,
				int rcode, UInt8* data, int size) = 0;
    virtual void	gotAck(int ackCode);


};

/*
 * Concrete async requests - read, write and hordes of read/modify/write
 */
class IOFWReadCommand : public IOFWAsyncCommand
{
    OSDeclareDefaultStructors(IOFWReadCommand)

protected:
    virtual void 	gotPacket(IOFireWireController* control,
				int rcode, UInt8* data, int size);

public:
    virtual bool	initAll(IOFireWireNub *device, FWAddress devAddress,
				IOMemoryDescriptor *hostMem,
				FWDeviceCallback completion, void *refcon, bool failOnReset);
    virtual IOReturn	reinit(FWAddress devAddress, IOMemoryDescriptor *hostMem,
				FWDeviceCallback completion=NULL, void *refcon=NULL,
				bool failOnReset=false);
    virtual IOReturn	execute();

};

class IOFWReadQuadCommand : public IOFWAsyncCommand
{
    OSDeclareDefaultStructors(IOFWReadQuadCommand)

protected:

    UInt32 *	fQuads;

    virtual void 	gotPacket(IOFireWireController* control,
				int rcode, UInt8* data, int size);

public:
    virtual bool	initAll(IOFireWireController *control,
				IOFireWireNub *device, FWAddress devAddress,
				UInt32 *quads, int numQuads,
				FWDeviceCallback completion, void *refcon, bool failOnReset);

    virtual IOReturn	reinit(FWAddress devAddress, UInt32 *quads, int numQuads,
				FWDeviceCallback completion=NULL, void *refcon=NULL,
				bool failOnReset=false);

    virtual IOReturn	execute();
};

class IOFWWriteCommand : public IOFWAsyncCommand
{
    OSDeclareDefaultStructors(IOFWWriteCommand)
protected:

    int				fPackSize;

    virtual void 	gotPacket(IOFireWireController* control,
				int rcode, UInt8* data, int size);

public:
    virtual bool	initAll(IOFireWireNub *device, FWAddress devAddress,
				IOMemoryDescriptor *hostMem,
				FWDeviceCallback completion, void *refcon, bool failOnReset);
    virtual IOReturn	reinit(FWAddress devAddress, IOMemoryDescriptor *hostMem,
				FWDeviceCallback completion=NULL, void *refcon=NULL,
				bool failOnReset=false);
    virtual IOReturn	execute();

};

class IOFWWriteQuadCommand : public IOFWAsyncCommand
{
    OSDeclareDefaultStructors(IOFWWriteQuadCommand)

protected:

    UInt32	fQuads[2];
    UInt32 *	fQPtr;
    int		fPackSize;

    virtual void 	gotPacket(IOFireWireController* control,
				int rcode, UInt8* data, int size);

public:
    virtual bool	initAll(IOFireWireController *control,
				IOFireWireNub *device, FWAddress devAddress,
				UInt32 *quads, int numQuads,
				FWDeviceCallback completion, void *refcon, bool failOnReset);

    virtual IOReturn	reinit(FWAddress devAddress, UInt32 *quads, int numQuads,
				FWDeviceCallback completion=NULL, void *refcon=NULL,
				bool failOnReset=false);

    virtual IOReturn	execute();
};


/*
 * May need more parameters for some of these,
 * and/or derive from a base Lock transaction command
 */
class IOFWCompareAndSwapCommand : public IOFWAsyncCommand
{
    OSDeclareDefaultStructors(IOFWCompareAndSwapCommand)

protected:
    UInt32 fInputVals[4];
    UInt32 fOldVal[2];

    virtual void 	gotPacket(IOFireWireController* control,
				int rcode, UInt8* data, int size);

public:
    // Compare to cmpVal, and if equal replace with newVal.
    // Size = 1 for 32 bit operation (one quad), 2 for 64 bit (two quads)
    virtual bool	initAll(IOFireWireNub *device, FWAddress devAddress,
				const UInt32 *cmpVal, const UInt32 *newVal, int size,
				FWDeviceCallback completion, void *refcon, bool failOnReset);

    // sets oldVal to the old value returned by the device, and
    // returns true if it was the expected value, ie. the lock succeeded
    virtual bool	locked(UInt32 *oldVal);
    virtual IOReturn	execute();

};

class IOFWBitAndCommand : public IOFWAsyncCommand
{
    OSDeclareDefaultStructors(IOFWBitAndCommand)

public:
    virtual IOReturn	execute();

};

class IOFWBitOrCommand : public IOFWAsyncCommand
{
    OSDeclareDefaultStructors(IOFWBitOrCommand)

public:
    virtual IOReturn	execute();

};

class IOFWBitXOrCommand : public IOFWAsyncCommand
{
    OSDeclareDefaultStructors(IOFWBitXOrCommand)

public:
    virtual IOReturn	execute();

};

class IOFWIncrementCommand : public IOFWAsyncCommand
{
    OSDeclareDefaultStructors(IOFWIncrementCommand)

public:
    virtual IOReturn	execute();

};

class IOFWDecrementCommand : public IOFWAsyncCommand
{
    OSDeclareDefaultStructors(IOFWDecrementCommand)

public:
    virtual IOReturn	execute();

};

class IOFWAddCommand : public IOFWAsyncCommand
{
    OSDeclareDefaultStructors(IOFWAddCommand)

public:
    virtual IOReturn	execute();

};

class IOFWThresholdAddCommand : public IOFWAsyncCommand
{
    OSDeclareDefaultStructors(IOFWThresholdAddCommand)

public:
    virtual IOReturn	execute();

};

class IOFWThresholdSubtractCommand : public IOFWAsyncCommand
{
    OSDeclareDefaultStructors(IOFWThresholdSubtractCommand)

public:
    virtual IOReturn	execute();

};

class IOFWClippedAddCommand : public IOFWAsyncCommand
{
    OSDeclareDefaultStructors(IOFWClippedAddCommand)

public:
    virtual IOReturn	execute();

};

class IOFWClippedSubtractCommand : public IOFWAsyncCommand
{
    OSDeclareDefaultStructors(IOFWClippedSubtractCommand)

public:
    virtual IOReturn	execute();

};

#endif /* _IOKIT_IOFWCOMMAND_H */
