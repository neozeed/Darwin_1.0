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


#ifndef _IOKIT_IOFWDCLPROGRAM_H
#define _IOKIT_IOFWDCLPROGRAM_H

#include <libkern/c++/OSObject.h>
#include <IOKit/firewire/IOFWIsoch.h>
#include <IOKit/firewire/IOFireWireBus.h>
#include <IOKit/IOMemoryCursor.h>

class IODCLProgram : public OSObject
{
    OSDeclareAbstractStructors(IODCLProgram)

protected:
    SInt32 fDCLTaskToKernel;
    SInt32 fDataTaskToKernel;
    IOByteCount fDataBase;
    IOMemoryDescriptor *fDCLDesc;
    IOMemoryDescriptor *fDataDesc;
    IOMemoryCursor *fDataCursor;
    IOFireWireBus::CallUserProc * fCallUser;
    void *fCallRefCon;	// Refcon for user call

    DCLCommandPtr convertDCLPtrToKernel(DCLCommandPtr dcl)
      {if (dcl) return (DCLCommandPtr)((UInt32)dcl + fDCLTaskToKernel); else return dcl;};

    DCLCommandPtr convertDCLPtrFromKernel(DCLCommandPtr dcl)
      {if (dcl) return (DCLCommandPtr)((UInt32)dcl - fDCLTaskToKernel); else return dcl;};

    void * convertDataPtrToKernel(void *ptr)
        {if (ptr) return (void *)((UInt32)ptr + fDataTaskToKernel); else return ptr;};

    UInt32 getPhysicalSegs(void *addr, IOByteCount len,
                    IOMemoryCursor::PhysicalSegment segs[], UInt32 maxSegs);

    void dumpDCL(DCLCommand *op);

    virtual void free();

public:

    virtual bool init(IOFireWireBus::DCLTaskInfo *info=NULL);
    virtual IOReturn allocateHW(IOFWSpeed speed, UInt32 chan) = 0;
    virtual IOReturn releaseHW() = 0;
    virtual IOReturn compile(IOFWSpeed speed, UInt32 chan) = 0;
    virtual IOReturn notify(UInt32 notificationType,
	DCLCommandPtr *dclCommandList, UInt32 numDCLCommands) = 0;
    virtual IOReturn start() = 0;
    virtual void stop() = 0;

};

#endif /* ! _IOKIT_IOFWDCLPROGRAM_H */

