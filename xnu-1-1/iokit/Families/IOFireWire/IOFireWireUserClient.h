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


#ifndef _IOKIT_IOFIREWIREUSERCLIENT_H
#define _IOKIT_IOFIREWIREUSERCLIENT_H

#include <IOKit/IOUserClient.h>
#include <IOKit/firewire/IOFWIsoch.h>

// Temporary place for these
enum {
    kFireWireRead = 0,
    kFireWireWrite,
    kFireWireCompareSwap,
    kFireWireBusReset,
    kFireWireTest,
    kFireWireCycleTime,
    kFireWireCompileDCL,
    kFireWireRunDCL,
    kFireWireStopDCL,
    kNumFireWireMethods
};

class IOFireWireDevice;

class IOFireWireUserClient : public IOUserClient
{
    OSDeclareDefaultStructors(IOFireWireUserClient)

private:
    IOFireWireDevice *	fOwner;
    task_t		fTask;
    IOExternalMethod	fMethods[ kNumFireWireMethods ];
    IOExternalAsyncMethod fAsyncMethods[ kNumFireWireMethods ];

    static void kernPongProc(void *refcon, void * userProc, void * dclCommand);

public:
    static IOFireWireUserClient *withTask(task_t owningTask);

    virtual IOReturn clientClose( void );
    virtual IOReturn clientDied( void );

    virtual IOReturn clientMemoryForType( UInt32 type,
        UInt32 * flags, IOMemoryDescriptor ** memory );

    virtual IOExternalMethod * getExternalMethodForIndex( UInt32 index );
    virtual IOExternalAsyncMethod * getExternalAsyncMethodForIndex( UInt32 index );
    virtual bool start( IOService * provider );

    virtual IOReturn Read(UInt32 addrHi, UInt32 addrLo, void *buf, UInt32 *size);
    virtual IOReturn Write(UInt32 addrHi, UInt32 addrLo, void *buf, UInt32 size);
    virtual IOReturn CompareSwap(UInt32 addrHi, UInt32 addrLo, UInt32 cmpVal, UInt32 newVal);
    virtual IOReturn BusReset();

    virtual IOReturn message( UInt32 type, IOService * provider,
				void * argument );

    virtual IOReturn Test();

    virtual IOReturn CompileDCL(UInt32 dclStart, UInt32 dclBase,
	UInt32 dclSize, UInt32 dataBase, UInt32 dataSize, UInt32 *program);

    virtual IOReturn RunDCL(OSAsyncReference asyncRef, UInt32 program);

    virtual IOReturn StopDCL(UInt32 program);

};

#endif /* ! _IOKIT_IOFIREWIREUSERCLIENT_H */

