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
 */


#ifndef _IOKIT_IOFIREWIRECONTROLLER_H
#define _IOKIT_IOFIREWIRECONTROLLER_H

#include <IOKit/firewire/IOFireWireBus.h>
#include <IOKit/firewire/IOFWRegs.h>
#include <IOKit/firewire/IOFireWirePriv.h>

extern const OSSymbol *gFireWireROM;
extern const OSSymbol *gFireWireNodeID;
extern const OSSymbol *gFireWireSelfIDs;
extern const OSSymbol *gFireWireSpeed;
extern const OSSymbol *gFireWireUnit_Spec_ID;
extern const OSSymbol *gFireWireUnit_SW_Version;
extern const OSSymbol *gFireWireVendor_ID;
extern const OSSymbol *gFireWire_GUID;

class OSData;
class IOWorkLoop;
class IOTimerEventSource;
class IOCommandGate;
class IOMemoryDescriptor;
class IOFireWireController;
class IOFWAddressSpace;
class IOFireWireNub;
class IOFireWireDevice;
class IOFWCommand;
class IOFWAsyncCommand;
class IODCLProgram;
struct NodeScan;

typedef void (*PacketHandler)(void *refCon, IOFireWireController* control, int rcode, UInt8* data, int size);

struct AsyncPendingTrans {
    IOFWAsyncCommand *	fHandler;
    int			fTCode;
    bool		fInUse;
};

// May want to change this, to use two tcodes per request block.
#define kMaxPendingTransfers kFWAsynchTTotal

class IOFireWireController : public IOFireWireBus
{
    OSDeclareAbstractStructors(IOFireWireController)

protected:
    IOWorkLoop *	fWorkLoop;
    IOTimerEventSource *fTimer;
    IOCommandGate *	fGate;
    OSSet *		fLocalAddresses;	// Collection of local adress spaces
    OSIterator *	fSpaceIterator;		// Iterator over local addr spaces

    OSSet *		fAllocatedChannels;	// Need to be informed of bus resets
    OSIterator *	fAllocChannelIterator;	// Iterator over channels
    IOLock *		fChannelLock;

    // Bus management variables (although we aren't a FireWire Bus Manager...)
    AbsoluteTime	fResetTime;		// Time of last reset
    UInt32		fLocalNodeID;		// ID of the local node.
    UInt32		fBusGeneration;		// ID of current bus topology.
    UInt16		fRootNodeID;		// ID of root, ie. highest node id in use.
    UInt16		fIRMNodeID;		// ID of Isochronous resource manager, or kFWBadNodeID
    IOFireWireDevice *	fIRMDevice;		// NULL if no IRM.
    IORegistryEntry * 	fNodes[kFWMaxNodesPerBus];	// FireWire nodes on this bus
    UInt32 *	 	fNodeIDs[kFWMaxNodesPerBus+1];	// Pointer to SelfID list for each node
							// +1 so we know how many selfIDs the last node has
    UInt8		fSpeedCodes[(kFWMaxNodesPerBus+1)*kFWMaxNodesPerBus];
						// Max speed between two nodes
    bool		fInReset;		// True if processing a reset (waiting for selfIDs)
    int			fNumROMReads;		// Number of device ROMs we are still reading
    // SelfIDs
    int			fNumSelfIDs;		// Total number of SelfID packets
    UInt32		fSelfIDs[kMaxSelfIDs*kFWMaxNodesPerBus];

    // The local device's CSR ROM
    UInt32		fCSRROMGeneration;	// Generation number of CSR ROM.
    CSRROMDirectoryData	fCSRROMRootDirectory;	// Local CSR ROM root directory.
    CSRNodeUniqueID	fUniqueID;		// ID unique to this device.
    UInt32 		fROMHeader[5];		// More or less fixed header and bus info block
    IOFWAddressSpace *	fROMAddrSpace;

    // Array for outstanding requests (up to 64)
    AsyncPendingTrans	fTrans[kMaxPendingTransfers];
    int			fLastTrans;
    IOLock *		fTransAllocLock;

    static IOReturn execCommand(OSObject * obj, void *field0, void *field1,
						 void *field2, void *field3);
    static void clockTick(OSObject *, IOTimerEventSource *);
    static void readROMGlue(void *refcon, IOReturn status,
			IOFireWireNub *device, IOFWCommand *fwCmd);
    virtual IOWorkLoop * createWorkLoop();
    virtual bool startWorkLoop();

    virtual void processBusReset();
    virtual void processSelfIDs(UInt32 *IDs, int numIDs, UInt32 *ownIDs, int numOwnIDs);
    virtual void processRcvPacket(UInt32 *data, int numQuads);
    virtual void processWriteRequest(UInt16 sourceID, UInt32 tlabel,
				UInt32 *hdr, void *buf, int len);
    virtual void processLockRequest(UInt16 sourceID, UInt32 tlabel,
				UInt32 *hdr, void *buf, int len);

    virtual void pruneDevices();

    virtual void buildTopology(bool doFWPlane);

    virtual void readDeviceROM(NodeScan *refCon, IOReturn status);

// Hack so old OHCI driver will compile
    virtual IODCLProgram *createDCLProgram(bool talking, DCLCommandStruct *opcodes,
            UInt32 startEvent, UInt32 startState, UInt32 startMask){return NULL;};

    virtual IODCLProgram *createDCLProgram(bool talking, DCLCommandStruct *opcodes,
            DCLTaskInfo *info, UInt32 startEvent, UInt32 startState, UInt32 startMask)
	{return createDCLProgram(talking, opcodes, startEvent, startState,  startMask); };

    // Send a PHY packet
    virtual IOReturn sendPHYPacket(UInt32 quad) = 0;

public:

    static const IORegistryPlane * gIOFireWirePlane;

    // Initialization
    virtual bool init(OSDictionary * dict);

    // Implement IOService::getWorkLoop
    virtual IOWorkLoop *getWorkLoop() const;

    // Methods called by commands. Not really public.
    virtual void fireBugMsg(const char *msg) = 0;
    virtual IOReturn resetBus() = 0;
    virtual IOReturn asyncRead(UInt16 nodeID, UInt16 addrHi, UInt32 addrLo, 
				int speed, int label, int size, IOFWAsyncCommand *cmd) = 0;
    virtual IOReturn asyncReadQuadResponse(UInt16 nodeID, int speed, 
					int label, int rcode, UInt32 data) = 0;
    virtual IOReturn asyncReadResponse(UInt16 nodeID, int speed, 
					int label, int rcode, void *data, int len) = 0;
    virtual IOReturn asyncReadResponse(UInt16 nodeID, int speed,
                                       int label, int rcode, IOMemoryDescriptor *buf,
				       IOByteCount offset, int len) = 0;

    virtual IOReturn asyncWrite(UInt16 nodeID, UInt16 addrHi, UInt32 addrLo,
		int speed, int label, IOMemoryDescriptor *buf, IOByteCount offset,
		int size, IOFWAsyncCommand *cmd) = 0;
    virtual IOReturn asyncWrite(UInt16 nodeID, UInt16 addrHi, UInt32 addrLo,
                                int speed, int label, void *data, int size, IOFWAsyncCommand *cmd) = 0;
    virtual IOReturn asyncWriteResponse(UInt16 nodeID, int speed, 
					int label, int rcode, UInt16 addrHi) = 0;

    virtual IOReturn asyncLock(UInt16 nodeID, UInt16 addrHi, UInt32 addrLo, 
			int speed, int label, int type, void *data, int size, IOFWAsyncCommand *cmd) = 0;
    virtual IOReturn asyncLockResponse(UInt16 nodeID, int speed, 
				int label, int rcode, int type, void *data, int len) = 0;

    // Read Cycle time register. safe to call at any time.
    virtual IOReturn getCycleTime(UInt32 *cycleTime) = 0;

    virtual IOReturn allocAddress(IOFWAddressSpace *space);
    virtual void freeAddress(IOFWAddressSpace *space);
    virtual IOReturn UpdateROM();

    // Allocate struct for receiving a read response
    virtual AsyncPendingTrans *allocTrans(bool sleepOK = true);
    virtual void freeTrans(AsyncPendingTrans *trans);

    // Really public methods

    // Methods to manipulate the local CSR ROM
    virtual IOReturn CSRROMGetRootDirectory(CSRROMEntryID *pCSRROMEntryID);
    virtual IOReturn CSRROMCreateEntry(CSRROMEntryID parentCSRROMEntryID, CSRROMEntryID *pCSRROMEntryID,
	UInt32 entryType, UInt32 entryKeyValue, void *entryData, UInt32 entrySize);

    // Convert a firewire nodeID into the IOFireWireDevice for it
    virtual IOFireWireDevice * nodeIDtoDevice(UInt16 nodeID);

    // Add/remove a channel from the list informed of bus resets
    virtual void addAllocatedChannel(IOFWIsochChannel *channel);
    virtual void removeAllocatedChannel(IOFWIsochChannel *channel);

    // Create a device nub
    virtual IOFireWireDevice *createDeviceNub(OSDictionary *propTable);

    // Create an Isochronous Channel object
    virtual IOFWIsochChannel *createIsochChannel(
	bool doIRM, UInt32 bandwidth, IOFWSpeed prefSpeed,
	FWIsochChannelForceStopNotificationProc stopProc=NULL,
	void *stopRefCon=NULL);

    // Create a local isochronous port to run the given DCL program
    // if task is 0, the DCL program is for the kernel task,
    // otherwise all DCL pointers are valid in the specified task.
    // opcodes is also pointer valid in the specified task.
    virtual IOFWIsochPort *createLocalIsochPort(bool talking,
        DCLCommandStruct *opcodes, DCLTaskInfo *info = 0,
	UInt32 startEvent = 0, UInt32 startState = 0, UInt32 startMask = 0);

    // Inline accessors for protected member variables
    IOCommandGate *getGate() const {return fGate;};
    bool checkGeneration(UInt32 gen) const {return gen == fBusGeneration;};
    UInt32 getGeneration() const {return fBusGeneration;};
    UInt16 getIRMNodeID() const {return fIRMNodeID;};
    IOFireWireDevice * getIRMDevice() const {return fIRMDevice;};
    const AbsoluteTime * getResetTime() const {return &fResetTime;};

    IOFWSpeed FWSpeed(UInt16 nodeAddress) const
	{return (IOFWSpeed)fSpeedCodes[(kFWMaxNodesPerBus+1)*(nodeAddress & 63)+(fLocalNodeID & 63)];};
    IOFWSpeed FWSpeed(UInt16 nodeA, UInt16 nodeB) const
      {return (IOFWSpeed)fSpeedCodes[(kFWMaxNodesPerBus+1)*(nodeA & 63)+(nodeB & 63)];};

    // How big (as a power of two) can packets sent to/received from the node be?
    virtual int maxPackLog(bool forSend, UInt16 nodeAddress) const;

    // How big (as a power of two) can packets sent from A to B be?
    virtual int maxPackLog(UInt16 nodeA, UInt16 nodeB) const;
};

#endif /* ! _IOKIT_IOFIREWIRECONTROLLER_H */

