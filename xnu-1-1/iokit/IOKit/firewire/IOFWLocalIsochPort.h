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


#ifndef _IOKIT_IOFWLOCALISOCHPORT_H
#define _IOKIT_IOFWLOCALISOCHPORT_H

#include <IOKit/firewire/IOFWIsochPort.h>
class IOFireWireController;
class IODCLProgram;

class IOFWLocalIsochPort : public IOFWIsochPort
{
    OSDeclareDefaultStructors(IOFWLocalIsochPort)

protected:
    IOFireWireController *	fControl;
    IODCLProgram *		fProgram;

    virtual void free();

public:
    virtual bool init(IODCLProgram *program, IOFireWireController *control);

	// Return maximum speed and channels supported
	// (bit n set = chan n supported)
    virtual IOReturn getSupported(IOFWSpeed &maxSpeed, UInt64 &chanSupported);

	// Allocate hardware resources for port
    virtual IOReturn allocatePort(IOFWSpeed speed, UInt32 chan);
    virtual IOReturn releasePort();	// Free hardware resources
    virtual IOReturn start();		// Start port processing packets
    virtual IOReturn stop();		// Stop processing packets
};

#endif /* ! _IOKIT_IOFWLOCALISOCHPORT_H */

