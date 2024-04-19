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


#ifndef _IOKIT_APPLEGRACKLEPCI_H
#define _IOKIT_APPLEGRACKLEPCI_H

#include <IOKit/pci/IOPCIBridge.h>

enum {
    kBridgeSelfDevice = 0
};

class AppleGracklePCI : public IOPCIBridge
{
    OSDeclareDefaultStructors(AppleGracklePCI)

protected:
    IOSimpleLock *		lock;
    IODeviceMemory *		ioMemory;
    IOMemoryMap *		configAddrMap;
    IOMemoryMap *		configDataMap;

    volatile UInt32	*	configAddr;
    volatile UInt32	*	configData;

    UInt8			primaryBus;

    inline void setConfigSpace( IOPCIAddressSpace space, UInt8 offset );
    virtual UInt8 firstBusNum( void );
    virtual UInt8 lastBusNum( void );

public:
    virtual bool start(	IOService * provider );
    virtual bool configure( IOService * provider );

    virtual void free();
    virtual IODeviceMemory * ioDeviceMemory( void );

    virtual UInt32 configRead32( IOPCIAddressSpace space, UInt8 offset );

    virtual void configWrite32( IOPCIAddressSpace space,
					UInt8 offset, UInt32 data );

    virtual IOPCIAddressSpace getBridgeSpace( void );
};

#endif /* ! _IOKIT_APPLEGRACKLEPCI_H */

