/*
 * Copyright (c) 1999 Apple Computer, Inc.  All rights reserved.
 *
 *  DRI: Josh de Cesare
 *
 */

#include "AppleCPU.h"

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#undef super
#define super IOCPU

OSDefineMetaClassAndStructors(AppleCPU, IOCPU);

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool AppleCPU::start(IOService *provider)
{
  kern_return_t result;
  
  if (!super::start(provider)) return false;
  
  cpuIC = new IOCPUInterruptController;
  if (cpuIC == 0) return false;
  
  if (cpuIC->initCPUInterruptController(1) != kIOReturnSuccess) return false;
  cpuIC->attach(this);
  
  cpuIC->registerCPUInterruptController();
  
  // Register this CPU with mach.
  result = ml_processor_register((cpu_id_t)this, 0,
				 &machProcessor, &ipi_handler, true);
  if (result == KERN_FAILURE) return false;
  
  cpuState = kIOCPUStateUninitalized;
  
  processor_start(machProcessor);
  
  return true;
}

void AppleCPU::initCPU(void)
{
  cpuIC->enableCPUInterrupt(this);
  
  cpuState = kIOCPUStateRunning;
}

OSSymbol *AppleCPU::getCPUName(void)
{
  return OSSymbol::withCStringNoCopy("Primary0");
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
