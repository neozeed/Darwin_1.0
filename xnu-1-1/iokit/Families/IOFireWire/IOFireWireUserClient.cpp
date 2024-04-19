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
 * 8 June 1999 wgulland created.
 *
 */
#include <IOKit/assert.h>
#include <IOKit/IOLib.h>
#include <IOKit/firewire/IOFireWireDevice.h>
#include <IOKit/firewire/IOFireWireController.h>
#include <IOKit/firewire/IOFWIsochChannel.h>
#include <IOKit/firewire/IOFWIsochPort.h>
#include <IOKit/firewire/IOFWIsoch.h>

#include <IOKit/IOBufferMemoryDescriptor.h>

#include "IOFireWireUserClient.h"

#define super IOUserClient
#define DEBUGGING_LEVEL 0

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

OSDefineMetaClassAndStructors(IOFireWireUserClient, IOUserClient)

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

class DVPort : public IOFWIsochPort
{
    OSDeclareDefaultStructors(DVPort)

public:
    virtual bool init(IOFireWireDevice *device);
        // Return maximum speed and channels supported
        // (bit n set = chan n supported)
    virtual IOReturn getSupported(IOFWSpeed &maxSpeed, UInt64 &chanSupported);

        // Allocate hardware resources for port
    virtual IOReturn allocatePort(IOFWSpeed speed, UInt32 chan);
    virtual IOReturn releasePort();	// Free hardware resources
    virtual IOReturn start();		// Start port processing packets
    virtual IOReturn stop();		// Stop processing packets
};

OSDefineMetaClassAndStructors(DVPort, IOFWIsochPort)

bool DVPort::init(IOFireWireDevice *device)
{
    return IOFWIsochPort::init();
}

IOReturn DVPort::getSupported(IOFWSpeed &maxSpeed, UInt64 &chanSupported)
{
//kprintf("DVPort::getSupported\n");
    maxSpeed = kFWSpeed100MBit;
    chanSupported = (UInt64)1;
    return kIOReturnSuccess;
}

IOReturn DVPort::allocatePort(IOFWSpeed speed, UInt32 chan)
{
//kprintf("DVPort::allocatePort(%d, %d)\n", speed, chan);
    return kIOReturnSuccess;
}

IOReturn DVPort::releasePort()
{
//kprintf("DVPort::releasePort()\n");
    return kIOReturnSuccess;
}

IOReturn DVPort::start()
{
//kprintf("DVPort::start()\n");
    return kIOReturnSuccess;
}

IOReturn DVPort::stop()
{
//kprintf("DVPort::stop()\n");
    return kIOReturnSuccess;
}


struct dclStuff {
    int fState;
    IOFWIsochChannel *chan;
    IOFWIsochPort *port;
    DVPort * dvPort;
    OSAsyncReference asyncRef;
};

static dclStuff theProg;
/*static*/ int donePings;

IOFireWireUserClient *IOFireWireUserClient::withTask(task_t owningTask)
{
    IOFireWireUserClient *me;

    me = new IOFireWireUserClient;
    if(me) {
        if(!me->init()) {
            me->release();
            return NULL;
        }
        me->fTask = owningTask;
    }
    return me;
}

bool IOFireWireUserClient::start( IOService * provider )
{
    assert(OSDynamicCast(IOFireWireDevice, provider));
    if(!super::start(provider))
        return false;
    fOwner = (IOFireWireDevice *)provider;

    // Got the owner, so initialize the call structures
    fMethods[kFireWireRead].object = this;
    fMethods[kFireWireRead].func =
        (IOMethod)&IOFireWireUserClient::Read;
    fMethods[kFireWireRead].count0 = 2;
    fMethods[kFireWireRead].count1 = 0xffffffff; // variable
    fMethods[kFireWireRead].flags = kIOUCScalarIStructO;

    fMethods[kFireWireWrite].object = this;
    fMethods[kFireWireWrite].func =
        (IOMethod)&IOFireWireUserClient::Write;
    fMethods[kFireWireWrite].count0 = 2;
    fMethods[kFireWireWrite].count1 = 0xffffffff; // variable
    fMethods[kFireWireWrite].flags = kIOUCScalarIStructI;

    fMethods[kFireWireCompareSwap].object = this;
    fMethods[kFireWireCompareSwap].func =
        (IOMethod)&IOFireWireUserClient::CompareSwap;
    fMethods[kFireWireCompareSwap].count0 = 4;
    fMethods[kFireWireCompareSwap].count1 = 0;
    fMethods[kFireWireCompareSwap].flags = kIOUCScalarIScalarO;

    fMethods[kFireWireBusReset].object = this;
    fMethods[kFireWireBusReset].func =
        (IOMethod)&IOFireWireUserClient::BusReset;
    fMethods[kFireWireBusReset].count0 = 0;
    fMethods[kFireWireBusReset].count1 = 0;
    fMethods[kFireWireBusReset].flags = kIOUCScalarIScalarO;

    fMethods[kFireWireTest].object = this;
    fMethods[kFireWireTest].func =
        (IOMethod)&IOFireWireUserClient::Test;
    fMethods[kFireWireTest].count0 = 0;
    fMethods[kFireWireTest].count1 = 0;
    fMethods[kFireWireTest].flags = kIOUCScalarIScalarO;

    fMethods[kFireWireCycleTime].object = fOwner->fControl;
    fMethods[kFireWireCycleTime].func =
        (IOMethod)&IOFireWireController::getCycleTime;
    fMethods[kFireWireCycleTime].count0 = 0;
    fMethods[kFireWireCycleTime].count1 = 1;
    fMethods[kFireWireCycleTime].flags = kIOUCScalarIScalarO;

    fMethods[kFireWireCompileDCL].object = this;
    fMethods[kFireWireCompileDCL].func =
        (IOMethod)&CompileDCL;
    fMethods[kFireWireCompileDCL].count0 = 5;
    fMethods[kFireWireCompileDCL].count1 = 1;
    fMethods[kFireWireCompileDCL].flags = kIOUCScalarIScalarO;

    fMethods[kFireWireStopDCL].object = this;
    fMethods[kFireWireStopDCL].func =
        (IOMethod)&StopDCL;
    fMethods[kFireWireStopDCL].count0 = 1;
    fMethods[kFireWireStopDCL].count1 = 0;
    fMethods[kFireWireStopDCL].flags = kIOUCScalarIScalarO;

    fAsyncMethods[kFireWireRunDCL].object = this;
    fAsyncMethods[kFireWireRunDCL].func =
        (IOAsyncMethod)&RunDCL;
    fAsyncMethods[kFireWireRunDCL].count0 = 1;
    fAsyncMethods[kFireWireRunDCL].count1 = 0;
    fAsyncMethods[kFireWireRunDCL].flags = kIOUCScalarIScalarO;



    return true;
}

IOReturn IOFireWireUserClient::clientClose( void )
{
    if(theProg.fState != 0)
	IOLog("Client close, DCL state = %d\n", theProg.fState);

    if(theProg.fState == 2) {
	StopDCL((UInt32)&theProg);
    }
    if(theProg.fState == 1) {
        StopDCL((UInt32)&theProg);
    }
    detach( fOwner);

    return kIOReturnSuccess;
}

IOReturn IOFireWireUserClient::clientDied( void )
{
    return( clientClose());
}

IOExternalMethod *
IOFireWireUserClient::getExternalMethodForIndex( UInt32 index )
{
    if(index >= kNumFireWireMethods)
    	return NULL;
    else
        return &fMethods[index];
}

IOExternalAsyncMethod *
IOFireWireUserClient::getExternalAsyncMethodForIndex( UInt32 index )
{
    if(index >= kNumFireWireMethods || fAsyncMethods[index].object == NULL)
        return NULL;
    else
        return &fAsyncMethods[index];
}



IOReturn
IOFireWireUserClient::Read(UInt32 addrHi, UInt32 addrLo, void *buf, UInt32 *size)
{
    IOReturn 			res;
    IOMemoryDescriptor *	mem = NULL;
    IOFWCommand *		cmd = NULL;
    UInt32			req = *size;

    do {
        *size = 0;
        if(req == 4 || req == 8) {
            cmd = fOwner->createReadQuadCommand(FWAddress(addrHi, addrLo), (UInt32 *)buf, req/4);
            if(!cmd) {
                res = kIOReturnNoMemory;
                break;
            }
	}
        else {
            mem = IOMemoryDescriptor::withAddress(buf, req, kIODirectionIn);
            if(!mem) {
                res = kIOReturnNoMemory;
                break;
            }

            cmd = fOwner->createReadCommand(FWAddress(addrHi, addrLo), mem);
            if(!cmd) {
                res = kIOReturnNoMemory;
                break;
            }
	}
        res = cmd->submit();
        // We block here until the command finishes
        if(kIOReturnSuccess == res)
            res = cmd->fStatus;
        if(kIOReturnSuccess == res) {
            if(mem)
                *size = cmd->fBytesTransferred;
            else
		*size = req;
	}
    
    } while(false);

    if(cmd)
	cmd->release();
    if(mem)
	mem->release();

    return res;
}

IOReturn
IOFireWireUserClient::Write(UInt32 addrHi, UInt32 addrLo, void *buf, UInt32 size)
{
    IOReturn 			res;
    IOMemoryDescriptor *	mem = NULL;
    IOFWCommand *		cmd = NULL;

    do {
        if(size == 4 || size == 8) {
            cmd = fOwner->createWriteQuadCommand(FWAddress(addrHi, addrLo), (UInt32 *)buf, size/4);
            if(!cmd) {
                res = kIOReturnNoMemory;
                break;
            }
	}
	else {
            mem = IOMemoryDescriptor::withAddress(buf, size, kIODirectionOut);
            if(!mem) {
                res = kIOReturnNoMemory;
                break;
            }

            cmd = fOwner->createWriteCommand(FWAddress(addrHi, addrLo), mem);
            if(!cmd) {
                res = kIOReturnNoMemory;
                break;
            }
	}
        res = cmd->submit();
        // We block here until the command finishes
        if(kIOReturnSuccess == res)
            res = cmd->fStatus;
    
    } while(false);

    if(cmd)
	cmd->release();
    if(mem)
	mem->release();

    return res;
}

IOReturn
IOFireWireUserClient::CompareSwap(UInt32 addrHi, UInt32 addrLo, UInt32 cmpVal, UInt32 newVal)
{
    IOReturn 			res;
    IOFWCompareAndSwapCommand *	cmd = NULL;

    cmd = fOwner->createCompareAndSwapCommand(FWAddress(addrHi, addrLo), &cmpVal, &newVal, 1);
    if(!cmd) {
        return kIOReturnNoMemory;
    }

    res = cmd->submit();
    // We block here until the command finishes
    if(kIOReturnSuccess == res) {
        UInt32 oldVal;
        res = cmd->fStatus;
        if(kIOReturnSuccess == res && !cmd->locked(&oldVal))
            res = kIOReturnCannotLock;
    }

    if(cmd)
	cmd->release();

    return res;
}


IOReturn
IOFireWireUserClient::clientMemoryForType( UInt32 type,
    UInt32 * flags, IOMemoryDescriptor ** memory )
{
    // This is a wonderful hack.
    // Create some memory, map it out to the user and off we go!!
    // We abuse the type as the low part of a fire wire address.
    // The high part is 0xf1d0. Woof Woof!!
    // The size is one page.
    IOBufferMemoryDescriptor *	desc;
    IOFWAddressSpace *		space;
    IOFWAllocAddressCommand *	cmd = NULL;
    void *			buf;
    IOReturn			res;

    desc = IOBufferMemoryDescriptor::withCapacity(PAGE_SIZE,
			kIODirectionOutIn, true);
    if(!desc)
        return kIOReturnNoMemory;
    desc->setLength(PAGE_SIZE);
    buf = desc->getBytesNoCopy();
    desc->retain();
    *memory = desc;

    space = IOFWPseudoAddressSpace::simpleRW(FWAddress(0xf1d0, type), PAGE_SIZE, buf);
    if(!space)
        return kIOReturnNoMemory;
    cmd = fOwner->createAllocAddrCommand(space);
    if(!cmd)
        return kIOReturnNoMemory;
    res = cmd->submit();
    // We block here until the command finishes
    if(kIOReturnSuccess == res)
        res = cmd->fStatus;
    if(cmd)
	cmd->release();

    if(kIOReturnSuccess == res) {
        space = fOwner->createPhysicalAddressSpace(desc);
        if(space) {
            cmd = fOwner->createAllocAddrCommand(space);
            if(!cmd)
                return kIOReturnNoMemory;
            res = cmd->submit();
            // We block here until the command finishes
            if(kIOReturnSuccess == res)
                res = cmd->fStatus;
            if(cmd)
                cmd->release();
	}
    }
    return res;
}

IOReturn IOFireWireUserClient::BusReset()
{
    IOFWUpdateROM *reset;
    IOReturn res = kIOReturnNoMemory;
    kprintf("Bus Reset!!!\n");
    reset = new IOFWUpdateROM;
    if(reset) {
        if(reset->initWithController(fOwner->fControl)) {
            res = reset->submit();
            // We block here until the command finishes
            if(kIOReturnSuccess == res)
                res = reset->fStatus;
	}
    }
    if(reset)
        reset->release();

    return kIOReturnSuccess;
}

static int state;
IOReturn IOFireWireUserClient::message( UInt32 type, IOService * provider, void * arg )
{
    kprintf("IOFireWireUserClient(0x%x)::message(0x%x, 0x%x, 0x%x)\n",
	this, type, provider, arg);
    kprintf("state = 0x%x\n", state);
    return kIOReturnSuccess;
}

#if 1
static void PingPongProc(DCLCommandPtr pDCLCommand)
{
    if(donePings++ < 2)
        IOLog("Ping! (or Pong!) %0x\n", pDCLCommand);
}

#if 0
IOReturn IOFireWireUserClient::Test()
{
    IOFWIsochChannel *chan;
    IOFWIsochPort *port;
    DVPort * dvPort;
    DCLCommandStruct *opcodes;

    enum
    {
        kNumPingPongs			= 2,
        kNumPacketsPerPingPong		= 10,
        kNumDCLsPerPingPongPacket	= 1,
        kRecordNumDCLs			=
                kNumPingPongs * kNumPacketsPerPingPong * kNumDCLsPerPingPongPacket,
        kMaxDCLSize			= 32,
        kRecordDCLProgramSize		= kMaxDCLSize * kRecordNumDCLs,
        kReceiveDVPacketSize		= 492,
        kPingPongBufferSize		=
                kNumPingPongs * kNumPacketsPerPingPong * kReceiveDVPacketSize
    };

    const UInt32 blockSize = kRecordDCLProgramSize;

    UInt8 *			pingPongBuffer = NULL;
    UInt8 *			pingPongPtr;
    UInt8 *			pDCLCommand;
    DCLLabelPtr			pStartDCLLabel;
    DCLTransferPacketPtr	pDCLTransferPacket;
    DCLCallProcPtr		pDCLPingPongProc;
    DCLJumpPtr			pDCLPingPongLoop;
    int				pingPongNum, packetNum;

    kprintf("IOFireWireUserClient::Test()!!\n");

    opcodes = (DCLCommandStruct *)IOMalloc(blockSize);

    // Create ping pong buffer.
    //zzz should allocate in initialization routine.
    pingPongBuffer = (UInt8 *)IOMalloc(kPingPongBufferSize);

    // Get pointer to start of DCL commands and update list.
    pDCLCommand = (UInt8 *)opcodes;

    // Create label for start of loop.
    pStartDCLLabel = (DCLLabelPtr) pDCLCommand;
    pDCLCommand += sizeof (DCLLabel);
    pStartDCLLabel->pNextDCLCommand = (DCLCommandPtr) pDCLCommand;
    pStartDCLLabel->opcode = kDCLLabelOp;
    pingPongPtr = pingPongBuffer;

    // Create 2 ping pong buffer lists of 10 packets each.
    for (pingPongNum = 0; pingPongNum < kNumPingPongs; pingPongNum++)
    {

            // Create transfer DCL for each packet.
            for (packetNum = 0; packetNum < kNumPacketsPerPingPong; packetNum++)
            {
                    // Receive one packet up to kReceiveDVPacketSize bytes.
                    pDCLTransferPacket = (DCLTransferPacketPtr) pDCLCommand;
                    pDCLCommand += sizeof (DCLTransferPacket);
                    pDCLTransferPacket->pNextDCLCommand = (DCLCommandPtr) pDCLCommand;
                    pDCLTransferPacket->opcode = kDCLReceivePacketStartOp;
                    pDCLTransferPacket->buffer = pingPongPtr;
                    pDCLTransferPacket->size = kReceiveDVPacketSize;

                    pingPongPtr += kReceiveDVPacketSize;
            }

            // Call the ping pong proc.
            pDCLPingPongProc = (DCLCallProcPtr) pDCLCommand;
            pDCLCommand += sizeof (DCLCallProc);
            pDCLPingPongProc->pNextDCLCommand = (DCLCommandPtr) pDCLCommand;
            pDCLPingPongProc->opcode = kDCLCallProcOp;
            pDCLPingPongProc->proc = PingPongProc;
            pDCLPingPongProc->procData = (UInt32) this;
    }

    // Loop to start of ping pong.
    pDCLPingPongLoop = (DCLJumpPtr) pDCLCommand;
    pDCLPingPongLoop->pNextDCLCommand = NULL;
    pDCLPingPongLoop->opcode = kDCLJumpOp;
    pDCLPingPongLoop->pJumpDCLLabel = pStartDCLLabel;

    donePings = 0;

    chan = fOwner->fControl->createIsochChannel(false, 488*8*8000, kFWSpeed100MBit);
    port = fOwner->fControl->createLocalIsochPort(false, opcodes);
    dvPort = new DVPort;
    dvPort->init(fOwner);
    chan->addListener(port);
    chan->setTalker(dvPort);
    chan->allocateChannel();
    chan->start();

    IOSleep(10000);

    chan->stop();
    chan->releaseChannel();
    dvPort->release();
    port->release();
    chan->release();

/*
    for (packetNum = 0; packetNum < kNumPacketsPerPingPong*kNumPingPongs; packetNum++) {
kprintf("Pack Header: 0x%x 0x%x 0x%x 0x%x\n",
*(UInt32 *)(pingPongBuffer+packetNum*kReceiveDVPacketSize),
*(UInt32 *)(pingPongBuffer+packetNum*kReceiveDVPacketSize+4),
*(UInt32 *)(pingPongBuffer+packetNum*kReceiveDVPacketSize+8),
*(UInt32 *)(pingPongBuffer+packetNum*kReceiveDVPacketSize+12)
);
    }
*/

    IOFree(pingPongBuffer, kPingPongBufferSize);
    IOFree(opcodes, blockSize);

    return kIOReturnSuccess;
}

#else

IOReturn IOFireWireUserClient::Test()
{
    IOFWIsochChannel *chan;
    IOFWIsochPort *port;
    DVPort * dvPort;
    DCLCommandStruct *opcodes;

    enum
    {
        kNumPingPongs			= 2,
        kNumPacketsPerPingPong		= 10,
        kNumDCLsPerPingPongPacket	= 2,
        kPlayNumDCLs			=
                kNumPingPongs * kNumPacketsPerPingPong * kNumDCLsPerPingPongPacket,
        kMaxDCLSize			= 32,
        kPlayDCLProgramSize		= kMaxDCLSize * kPlayNumDCLs,
        kSendDVPacketSize		= 492,
	kDVPacketHdrSize		= 8,
	kDVPacketDataSize		= 480,
        kPingPongBufferSize		=
                kNumPingPongs * kNumPacketsPerPingPong * kSendDVPacketSize
    };

    const UInt32 blockSize = kPlayDCLProgramSize;

    UInt8 *			pingPongBuffer = NULL;
    UInt8 *			pingPongPtr;
    UInt8 *			pDCLCommand;
    DCLLabelPtr			pStartDCLLabel;
    DCLTransferPacketPtr	pDCLTransferPacket;
    DCLCallProcPtr		pDCLPingPongProc;
    DCLJumpPtr			pDCLPingPongLoop;
    int				pingPongNum, packetNum;

    kprintf("IOFireWireUserClient::Test()!!\n");

    opcodes = (DCLCommandStruct *)IOMalloc(blockSize);

    // Create ping pong buffer.
    //zzz should allocate in initialization routine.
    pingPongBuffer = (UInt8 *)IOMalloc(kPingPongBufferSize);

    // Get pointer to start of DCL commands and update list.
    pDCLCommand = (UInt8 *)opcodes;

    // Create label for start of loop.
    pStartDCLLabel = (DCLLabelPtr) pDCLCommand;
    pDCLCommand += sizeof (DCLLabel);
    pStartDCLLabel->pNextDCLCommand = (DCLCommandPtr) pDCLCommand;
    pStartDCLLabel->opcode = kDCLLabelOp;
    pingPongPtr = pingPongBuffer;

    // Create 2 ping pong buffer lists of 10 packets each.
    for (pingPongNum = 0; pingPongNum < kNumPingPongs; pingPongNum++)
    {

            // Create transfer DCL for each packet.
            for (packetNum = 0; packetNum < kNumPacketsPerPingPong; packetNum++)
            {
                    // Send a packet with a header (CIP) and Mystery D/V Data
                    pDCLTransferPacket = (DCLTransferPacketPtr) pDCLCommand;
                    pDCLCommand += sizeof (DCLTransferPacket);
                    pDCLTransferPacket->pNextDCLCommand = (DCLCommandPtr) pDCLCommand;
                    pDCLTransferPacket->opcode = kDCLSendPacketStartOp;
                    pDCLTransferPacket->buffer = pingPongPtr;
                    pDCLTransferPacket->size = kDVPacketHdrSize;
                    pingPongPtr += kDVPacketHdrSize;

                    pDCLTransferPacket = (DCLTransferPacketPtr) pDCLCommand;
                    pDCLCommand += sizeof (DCLTransferPacket);
                    pDCLTransferPacket->pNextDCLCommand = (DCLCommandPtr) pDCLCommand;
                    pDCLTransferPacket->opcode = kDCLSendPacketOp;
                    pDCLTransferPacket->buffer = pingPongPtr;
                    pDCLTransferPacket->size = kDVPacketDataSize;
                    pingPongPtr += kDVPacketDataSize;

            }

            // Call the ping pong proc.
            pDCLPingPongProc = (DCLCallProcPtr) pDCLCommand;
            pDCLCommand += sizeof (DCLCallProc);
            pDCLPingPongProc->pNextDCLCommand = (DCLCommandPtr) pDCLCommand;
            pDCLPingPongProc->opcode = kDCLCallProcOp;
            pDCLPingPongProc->proc = PingPongProc;
            pDCLPingPongProc->procData = (UInt32) this;
    }

    // Loop to start of ping pong.
    pDCLPingPongLoop = (DCLJumpPtr) pDCLCommand;
    pDCLPingPongLoop->pNextDCLCommand = NULL;
    pDCLPingPongLoop->opcode = kDCLJumpOp;
    pDCLPingPongLoop->pJumpDCLLabel = pStartDCLLabel;

    donePings = 0;

    chan = fOwner->fControl->createIsochChannel(true, 488*8*8000, kFWSpeed100MBit);
    port = fOwner->fControl->createLocalIsochPort(true, opcodes);
    dvPort = new DVPort;
    dvPort->init(fOwner);
    chan->addListener(dvPort);
    chan->setTalker(port);
    chan->allocateChannel();
    chan->start();

    IOSleep(10);

    chan->stop();
    chan->releaseChannel();
    dvPort->release();
    port->release();
    chan->release();

/*
    for (packetNum = 0; packetNum < kNumPacketsPerPingPong*kNumPingPongs; packetNum++) {
kprintf("Pack Header: 0x%x 0x%x 0x%x 0x%x\n",
*(UInt32 *)(pingPongBuffer+packetNum*kReceiveDVPacketSize),
*(UInt32 *)(pingPongBuffer+packetNum*kReceiveDVPacketSize+4),
*(UInt32 *)(pingPongBuffer+packetNum*kReceiveDVPacketSize+8),
*(UInt32 *)(pingPongBuffer+packetNum*kReceiveDVPacketSize+12)
);
    }
*/

    IOFree(pingPongBuffer, kPingPongBufferSize);
    IOFree(opcodes, blockSize);

    return kIOReturnSuccess;
}
#endif

#endif
#if 0

static IOFWCommand *cmd1 = 0;
static IOFWCommand *cmd2 = 0;
static IOFWCommand *cmd3 = 0;

static void Complete(void *refcon, IOReturn status, IOFireWireDevice *device, IOFWCommand *fwCmd)
{
//    if(status != kIOReturnSuccess)
//	kprintf("%x Completed with error %x\n", refcon, status);
    if(fwCmd == cmd1) {
        state |= 2;
        cmd1->release();
        cmd1 = 0;
	cmd2->execute();
    }
    else if(fwCmd == cmd2) {
	cmd2->release();
        cmd2 = 0;
        state |= 4;
    }
    else
	kprintf("Complete(): Bad refcon: 0x%x\n", refcon);
}

IOReturn IOFireWireUserClient::Test()
{
    FWAddress testAddr(0xf1d0, 0);
    UInt32 quad = 0x12345677;
    UInt32 quad1 = 0x12345678;
    UInt32 quad2 = 0x12345679;
    IOReturn status;

    if(cmd1 != 0 || cmd2 != 0)
	return state;	// Still busy.

    cmd1 = fOwner->createWriteQuadCommand(testAddr, &quad, 1, Complete, (void *)quad);
    cmd2 = fOwner->createWriteQuadCommand(testAddr, &quad1, 1, Complete, (void *)(quad+1));
    cmd3 = fOwner->createWriteQuadCommand(testAddr, &quad2, 1);
    state = 0;
    cmd1->submit();
    state |= 1;
    //cmd2->submit();
    cmd3->submit();
    state |= 8;
    status = cmd3->fStatus;

    cmd3->release();
    return status;
}
#endif

#if 0
IOReturn IOFireWireUserClient::Test()
{
    IOBufferMemoryDescriptor *	desc;
    IOFWAddressSpace *		space;
    IOFWBusCommand *		cmd = NULL;

    desc = IOBufferMemoryDescriptor::withCapacity(PAGE_SIZE,
                        kIODirectionOutIn, true);
    if(!desc)
        return kIOReturnNoMemory;
    desc->setLength(PAGE_SIZE);
    space = fOwner->createPhysicalAddressSpace(desc);
    if(!space)
        return kIOReturnNoMemory;
    cmd = fOwner->createAllocAddrCommand(space);
    if(!cmd)
        return kIOReturnNoMemory;
    cmd->submit();
    cmd->release();
    cmd = fOwner->createDeallocAddrCommand(space);
    if(!cmd)
        return kIOReturnNoMemory;
    cmd->submit();
    cmd->release();
    space->release();
    desc->release();

    return kIOReturnSuccess;
}
#endif

// from IOKitLibPrivate.h!
enum {
    kIOAsyncReservedIndex 	= 0,

    kIOAsyncCalloutFuncIndex 	= 1,
    kIOAsyncCalloutRefconIndex,
    kIOAsyncCalloutCount,
};

void IOFireWireUserClient::kernPongProc(void *refcon, void * userProc, void * dclCommand)
{
    IOReturn kr;
    IOFireWireUserClient * me;
#if 0
    if(donePings++ < 2)
        IOLog("Ping! (or Pong!) %0x ->0x%x\n", dclCommand,
              userProc);
#endif
    me = (IOFireWireUserClient *)refcon;
    theProg.asyncRef[kIOAsyncCalloutFuncIndex]   = (natural_t) userProc;
    theProg.asyncRef[kIOAsyncCalloutRefconIndex] = (natural_t) dclCommand;
    kr = me->sendAsyncResult(theProg.asyncRef, kIOReturnSuccess, NULL, 0);
}


IOReturn IOFireWireUserClient::CompileDCL(UInt32 dclStart, UInt32 dclBase,
    UInt32 dclSize, UInt32 dataBase, UInt32 dataSize, UInt32 *program)
{
    IOReturn res;
//    IOLog("DCL start:0x%x base:0x%x size:%d, Data base:0x%x, size:%d\n",
//	dclStart, dclBase, dclSize, dataBase, dataSize);

    // Check that DCL program and data buffers are in single regions
    vm_region_basic_info_data_t regionInfo;
    mach_msg_type_number_t      regionInfoSize = sizeof(regionInfo);
    vm_size_t                   regionSize;
    vm_offset_t checkBase = trunc_page(dclBase);
    vm_size_t   checkSize = round_page(dclSize);

    res = vm_region(
              /* map         */ get_task_map(fTask),
              /* address     */ &checkBase,
              /* size        */ &regionSize,
              /* flavor      */ VM_REGION_BASIC_INFO,
              /* info        */ (vm_region_info_t) &regionInfo,
              /* info size   */ &regionInfoSize,
              /* object name */ 0 );
    if(res != kIOReturnSuccess)
        return res;

    if(checkBase + regionSize < dclBase + dclSize)
        return kIOReturnVMError;

    checkBase = trunc_page(dataBase);
    checkSize = round_page(dataSize);
    res = vm_region(
              /* map         */ get_task_map(fTask),
              /* address     */ &checkBase,
              /* size        */ &regionSize,
              /* flavor      */ VM_REGION_BASIC_INFO,
              /* info        */ (vm_region_info_t) &regionInfo,
              /* info size   */ &regionInfoSize,
              /* object name */ 0 );
    if(res != kIOReturnSuccess)
        return res;

    if(checkBase + regionSize < dataBase + dataSize)
        return kIOReturnVMError;

    *program = &theProg;
    IOFireWireBus::DCLTaskInfo info;
    info.fTask = fTask;
    info.fDCLBaseAddr = dclBase;
    info.fDCLSize = dclSize;
    info.fDataBaseAddr = dataBase;
    info.fDataSize = dataSize;
    info.fCallUser = kernPongProc;
    info.fCallRefCon = this;
    theProg.port = fOwner->fControl->createLocalIsochPort(false, (DCLCommand *)dclStart, &info);

    theProg.chan = fOwner->fControl->createIsochChannel(false, 488*8*8000, kFWSpeed100MBit);
    theProg.dvPort = new DVPort;
    theProg.dvPort->init(fOwner);
    theProg.chan->addListener(theProg.port);
    theProg.chan->setTalker(theProg.dvPort);
    theProg.fState = 1;

    return kIOReturnSuccess;
}

IOReturn IOFireWireUserClient::RunDCL(OSAsyncReference asyncRef, UInt32 program)
{
    donePings = 0;
    if(theProg.fState == 1) {
        bcopy(asyncRef, theProg.asyncRef, sizeof(OSAsyncReference));
        theProg.chan->allocateChannel();
        theProg.chan->start();
        theProg.fState = 2;
        return kIOReturnSuccess;
    }
    else {
        IOLog("RunDCL, state is %d not 1!\n", theProg.fState);
        return kIOReturnNotReady;
    }

}

IOReturn IOFireWireUserClient::StopDCL(UInt32 program)
{
    if(theProg.fState == 2) {
        theProg.chan->stop();
        theProg.chan->releaseChannel();
    }
    theProg.chan->release();
    theProg.dvPort->release();
    theProg.port->release();

    theProg.fState = 0;
    return kIOReturnSuccess;
}


