/*
 * Copyright (c) 2000 Apple Computer, Inc.  All rights reserved.
 *
 * AppleI386CPU.h
 * 
 * March 6, 2000 jliu
 *    Created based on AppleCPU.
 */

#ifndef _IOKIT_APPLEI386CPU_H
#define _IOKIT_APPLEI386CPU_H

#include <IOKit/IOCPU.h>

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

class AppleI386CPU : public IOCPU
{
    OSDeclareDefaultStructors(AppleI386CPU);

private:
    IOCPUInterruptController * cpuIC;

public:
    virtual bool       start(IOService * provider);
    virtual void       initCPU(void);
    virtual OSSymbol * getCPUName(void);
};

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

class AppleI386CPUInterruptController : public IOCPUInterruptController
{
    OSDeclareDefaultStructors(AppleI386CPUInterruptController);

public:
    virtual IOReturn handleInterrupt(void *      refCon,
                                     IOService * nub,
                                     int         source);
};

#endif /* ! _IOKIT_APPLEI386CPU_H */
