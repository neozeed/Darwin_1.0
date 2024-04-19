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
 *	IOFireWireUnit.h
 *
 *
 */
#ifndef _IOKIT_IOFIREWIREUNIT_H
#define _IOKIT_IOFIREWIREUNIT_H

#include <IOKit/firewire/IOFireWireNub.h>
class IOFireWireDevice;

class IOFireWireUnit : public IOFireWireNub
{
    OSDeclareDefaultStructors(IOFireWireUnit)

/*------------------Useful info about device (also available in the registry)--------*/
public:

// More info to come - device type, CSR ROM etc.

protected:
    IOFireWireDevice *fDevice;	// The device unit i spart of

/*------------------Methods provided to FireWire device clients-----------------------*/
public:
    /*
     * Create an iterator for the unit directory in the device ROM
     */
    virtual IOReturn CSRROMCreateIterator (CSRROMEntryIterator *pCSRROMIterator);

    /*
     * Standard nub initialization
     */
    virtual bool attach(IOService * provider );

    /*
     * Matching language support
     * Match on the following properties of the unit:
     * Vendor_ID
     * GUID
     * Unit_Spec_ID
     * Unit_SW_Version
     */
    bool matchPropertyTable(OSDictionary * table);


    virtual IOReturn message( UInt32 type, IOService * provider, void * argument );

};

#endif /* ! _IOKIT_IOFIREWIREDEVICE_H */
