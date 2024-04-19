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
 *	IOFireWireDevice.h
 *
 *
 */
#ifndef _IOKIT_IOFIREWIREDEVICE_H
#define _IOKIT_IOFIREWIREDEVICE_H

#include <IOKit/firewire/IOFireWireNub.h>

class IOFireWireDevice : public IOFireWireNub
{
    OSDeclareDefaultStructors(IOFireWireDevice)

/*------------------Useful info about device (also available in the registry)--------*/
public:
    int			fNumSelfIDs;	// Number we actually have (usually just one)
    UInt32		fSelfIDs[kMaxSelfIDs];

// More info to come - device type, CSR ROM etc.

protected:

/*-----------Methods provided to FireWire device clients-------------*/
public:
    /*
     * Create an iterator for the device ROM
     */
    virtual IOReturn CSRROMCreateIterator (CSRROMEntryIterator *pCSRROMIterator);

    // Called after init or bus reset to update nodeID and ROM
    virtual void setNodeROM(UInt32 generation, UInt16 nodeID, UInt16 localNodeID,
		OSData *rom, UInt32 *selfIDs, int numSelfIDs);

    /*
     * Matching language support
     * Match on the following properties of the device:
     * Vendor_ID
     * GUID
     */
    bool matchPropertyTable(OSDictionary * table);

    /*
     * Standard nub initialization
     */
    virtual bool attach(IOService * provider );
};

#endif /* ! _IOKIT_IOFIREWIREDEVICE_H */
