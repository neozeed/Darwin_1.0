/*
 * Copyright (c) 2000 Apple Computer, Inc.  All rights reserved.
 *
 * AppleI386CPU.cpp
 * 
 * March 6, 2000 jliu
 *    Created based on AppleCPU.
 */

#include "AppleI386CPU.h"

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#undef super
#define super IOCPU

OSDefineMetaClassAndStructors(AppleI386CPU, IOCPU);

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool AppleI386CPU::start(IOService * provider)
{
//  kern_return_t result;

    if (!super::start(provider)) return false;

    cpuIC = new AppleI386CPUInterruptController;
    if (cpuIC == 0) return false;

    if (cpuIC->initCPUInterruptController(1) != kIOReturnSuccess)
        return false;

    cpuIC->attach(this);
    
    cpuIC->registerCPUInterruptController();

#ifdef NOTYET
    // Register this CPU with mach.
    result = ml_processor_register((cpu_id_t)this, 0,
                    &machProcessor, &ipi_handler, true);
    if (result == KERN_FAILURE) return false;
#endif

    cpuState = kIOCPUStateUninitalized;

#ifdef NOTYET
    processor_start(machProcessor);
#endif

    // Hack. Call initCPU() ourself since no one else will.
    initCPU();

    return true;
}

void AppleI386CPU::initCPU(void)
{
    cpuIC->enableCPUInterrupt(this);

    cpuState = kIOCPUStateRunning;
}

OSSymbol * AppleI386CPU::getCPUName(void)
{
    return (OSSymbol *) OSSymbol::withCStringNoCopy("Primary0");
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#undef super
#define super IOCPUInterruptController

OSDefineMetaClassAndStructors(AppleI386CPUInterruptController, 
                              IOCPUInterruptController);

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOReturn AppleI386CPUInterruptController::handleInterrupt(void * /*refCon*/,
                                                          IOService * /*nub*/,
                                                          int source)
{
    IOInterruptVector * vector;

    // Override the implementation in IOCPUInterruptController to
    // dispatch interrupts the old way.
    //
    // source argument is ignored, use the first IOCPUInterruptController
    // in the vector array.
    //
    vector = &vectors[0];

    if (!vector->interruptRegistered)
        return kIOReturnInvalid;
  
    vector->handler(vector->target,
                    vector->refCon,
                    vector->nub,
                    source);

    return kIOReturnSuccess;
}
