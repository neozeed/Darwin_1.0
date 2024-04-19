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
 * IOFWIsochPort is an abstract object that represents hardware on the bus
 * (locally or remotely) that sends or receives isochronous packets.
 * Local ports are implemented by the local device driver,
 * Remote ports are implemented by the driver for the remote device.
 *
 * HISTORY
 *
 */
#include <IOKit/firewire/IOFWIsochPort.h>
#include <IOKit/firewire/IOFWLocalIsochPort.h>
#include <IOKit/firewire/IOFWDCLProgram.h>
#include <IOKit/firewire/IOFWCommand.h>

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
// Commands to control isoch bus hardware

class IOFWAllocIsoHWCommand : public IOFWBusCommand
{
    OSDeclareDefaultStructors(IOFWAllocIsoHWCommand)

protected:
    IODCLProgram *	fProgram;
    IOFWSpeed		fSpeed;
    UInt32		fChan;

public:
    virtual IOReturn	execute();
    virtual bool	init(IODCLProgram *program, IOFireWireController *control,
				IOFWSpeed speed, UInt32 chan,
				FWBusCallback completion=NULL, void *refcon=NULL);
};

OSDefineMetaClassAndStructors(IOFWAllocIsoHWCommand, IOFWBusCommand)

bool IOFWAllocIsoHWCommand::init(IODCLProgram *program,
				IOFireWireController *control,
				IOFWSpeed speed, UInt32 chan,
				FWBusCallback completion, void *refcon)
{
    if(!IOFWBusCommand::initWithController(control, completion, refcon))
	return false;
    fProgram = program;
    fSpeed = speed;
    fChan = chan;
    return true;
}

IOReturn IOFWAllocIsoHWCommand::execute()
{
    return complete(fProgram->allocateHW(fSpeed, fChan));
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

class IOFWReleaseIsoHWCommand : public IOFWBusCommand
{
    OSDeclareDefaultStructors(IOFWReleaseIsoHWCommand)

protected:
    IODCLProgram *	fProgram;

public:
    virtual IOReturn	execute();
    virtual bool	init(IODCLProgram *program, IOFireWireController *control,
				FWBusCallback completion=NULL, void *refcon=NULL);
};

OSDefineMetaClassAndStructors(IOFWReleaseIsoHWCommand, IOFWBusCommand)

bool IOFWReleaseIsoHWCommand::init(IODCLProgram *program,
				IOFireWireController *control,
				FWBusCallback completion, void *refcon)
{
    if(!IOFWBusCommand::initWithController(control, completion, refcon))
	return false;
    fProgram = program;
    return true;
}

IOReturn IOFWReleaseIsoHWCommand::execute()
{
    return complete(fProgram->releaseHW());
}


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

class IOFWStartIsoHWCommand : public IOFWBusCommand
{
    OSDeclareDefaultStructors(IOFWStartIsoHWCommand)

protected:
    IODCLProgram *	fProgram;

public:
    virtual IOReturn	execute();
    virtual bool	init(IODCLProgram *program, IOFireWireController *control,
				FWBusCallback completion=NULL, void *refcon=NULL);
};

OSDefineMetaClassAndStructors(IOFWStartIsoHWCommand, IOFWBusCommand)

bool IOFWStartIsoHWCommand::init(IODCLProgram *program,
				IOFireWireController *control,
				FWBusCallback completion, void *refcon)
{
    if(!IOFWBusCommand::initWithController(control, completion, refcon))
	return false;
    fProgram = program;
    return true;
}

IOReturn IOFWStartIsoHWCommand::execute()
{
    return complete(fProgram->start());
}


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
class IOFWStopIsoHWCommand : public IOFWBusCommand
{
    OSDeclareDefaultStructors(IOFWStopIsoHWCommand)

protected:
    IODCLProgram *	fProgram;

public:
    virtual IOReturn	execute();
    virtual bool	init(IODCLProgram *program, IOFireWireController *control,
				FWBusCallback completion=NULL, void *refcon=NULL);
};

OSDefineMetaClassAndStructors(IOFWStopIsoHWCommand, IOFWBusCommand)

bool IOFWStopIsoHWCommand::init(IODCLProgram *program,
				IOFireWireController *control,
				FWBusCallback completion, void *refcon)
{
    if(!IOFWBusCommand::initWithController(control, completion, refcon))
	return false;
    fProgram = program;
    return true;
}

IOReturn IOFWStopIsoHWCommand::execute()
{
    fProgram->stop();
    return complete(kIOReturnSuccess);
}


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
OSDefineMetaClass( IOFWIsochPort, OSObject )
OSDefineAbstractStructors(IOFWIsochPort, OSObject)

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
OSDefineMetaClassAndStructors(IOFWLocalIsochPort, IOFWIsochPort)

bool IOFWLocalIsochPort::init(IODCLProgram *program, IOFireWireController *control)
{
    if(!IOFWIsochPort::init())
	return false;
    fProgram = program; // belongs to us.
    fControl = control;
    return true;
}

void IOFWLocalIsochPort::free()
{
    if(fProgram)
	fProgram->release();
    IOFWIsochPort::free();
}

// Return maximum speed and channels supported
// (bit n set = chan n supported)
IOReturn IOFWLocalIsochPort::getSupported(IOFWSpeed &maxSpeed, UInt64 &chanSupported)
{
    maxSpeed = kFWSpeedMaximum;
    chanSupported = ~(UInt64)0;
    return kIOReturnSuccess;
}

/*
 * Allocate hardware resources for port, via workloop
 * Then compile program, not on workloop.
 */
IOReturn IOFWLocalIsochPort::allocatePort(IOFWSpeed speed, UInt32 chan)
{
    IOReturn res;
    IOFWAllocIsoHWCommand *allocHW;

    allocHW = new IOFWAllocIsoHWCommand();
    if(NULL == allocHW)
	return kIOReturnNoMemory;
    if(!allocHW->init(fProgram, fControl, speed, chan)) {
	allocHW->release();
	return kIOReturnNoMemory;
    }
    allocHW->submit();

    res = allocHW->fStatus;
    allocHW->release();

    if(kIOReturnSuccess != res)
	return res; 
    return fProgram->compile(speed, chan);	// Not on workloop
}

IOReturn IOFWLocalIsochPort::releasePort()
{
    IOFWReleaseIsoHWCommand *releaseHW;
    IOReturn res;

    releaseHW = new IOFWReleaseIsoHWCommand();
    if(NULL == releaseHW)
	return kIOReturnNoMemory;
    if(!releaseHW->init(fProgram, fControl)) {
        releaseHW->release();
	return kIOReturnNoMemory;
    }
    releaseHW->submit();

    res = releaseHW->fStatus;
    releaseHW->release();
    return res;
}

IOReturn IOFWLocalIsochPort::start()
{
    IOFWStartIsoHWCommand *startHW;
    IOReturn res;

    startHW = new IOFWStartIsoHWCommand();
    if(NULL == startHW)
	return kIOReturnNoMemory;
    if(!startHW->init(fProgram, fControl)) {
        startHW->release();
	return kIOReturnNoMemory;
    }
    startHW->submit();

    res = startHW->fStatus;
    startHW->release();
    return res;
}

IOReturn IOFWLocalIsochPort::stop()
{
    IOFWStopIsoHWCommand *stopHW;
    IOReturn res;

    stopHW = new IOFWStopIsoHWCommand();
    if(NULL == stopHW)
	return kIOReturnNoMemory;
    if(!stopHW->init(fProgram, fControl)) {
        stopHW->release();
	return kIOReturnNoMemory;
    }
    stopHW->submit();

    res = stopHW->fStatus;
    stopHW->release();
    return res;
}


