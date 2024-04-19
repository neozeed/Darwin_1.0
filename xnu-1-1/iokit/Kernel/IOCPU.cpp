/*
 * Copyright (c) 1999 Apple Computer, Inc.  All rights reserved.
 *
 *  DRI: Josh de Cesare
 *
 */

extern "C" {
#include <machine/machine_routines.h>
#include <pexpert/pexpert.h>
}

#include <IOKit/IOLib.h>
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/IOCPU.h>


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

kern_return_t PE_cpu_start(cpu_id_t target,
			   vm_offset_t start_paddr, vm_offset_t arg_paddr)
{
  IOCPU *targetCPU = OSDynamicCast(IOCPU, (OSObject *)target);
  
  if (targetCPU == 0) return KERN_FAILURE;
  return targetCPU->startCPU(start_paddr, arg_paddr);
}

void PE_cpu_halt(cpu_id_t target,
		 vm_offset_t start_paddr, vm_offset_t arg_paddr)
{
  IOCPU *targetCPU = OSDynamicCast(IOCPU, (OSObject *)target);
  
  if (targetCPU) targetCPU->haltCPU(start_paddr, arg_paddr);
}

void PE_cpu_signal(cpu_id_t source, cpu_id_t target)
{
  IOCPU *sourceCPU = OSDynamicCast(IOCPU, (OSObject *)source);
  IOCPU *targetCPU = OSDynamicCast(IOCPU, (OSObject *)target);
  
  if (sourceCPU && targetCPU) sourceCPU->signalCPU(targetCPU);
}

void PE_cpu_machine_init(cpu_id_t target)
{
  IOCPU *targetCPU = OSDynamicCast(IOCPU, (OSObject *)target);
  
  if (targetCPU) targetCPU->initCPU();
}


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define super IOService

OSDefineMetaClass(IOCPU, IOService);
OSDefineAbstractStructors(IOCPU, IOService);

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static OSDictionary *gIOCPUs;

void IOCPU::initCPUs(void)
{
  gIOCPUs = OSDictionary::withCapacity(1);
}

bool IOCPU::start(IOService *provider)
{
  if (!super::start(provider)) return false;
  
  cpuGroup = gIOCPUs;  
  cpuNub = provider;
  
  cpuState = kIOCPUStateUnregistered;
  
  return true;
}

void IOCPU::initCPU(void)
{
  // Do default interrupt stuff.
  
  
  cpuState = kIOCPUStateRunning;
}

kern_return_t IOCPU::startCPU(vm_offset_t /*start_paddr*/,
			      vm_offset_t /*arg_paddr*/)
{
  return KERN_FAILURE;
}

void IOCPU::haltCPU(vm_offset_t /*start_paddr*/,
		    vm_offset_t /*arg_paddr*/)
{
}

void IOCPU::signalCPU(IOCPU */*target*/)
{
}

int IOCPU::getCPUNumber(void)
{
  return physCPU;
}

int IOCPU::getCPUState(void)
{
  return cpuState;
}

OSDictionary *IOCPU::getCPUGroup(void)
{
  return cpuGroup;
}

int IOCPU::getCPUGroupSize(void)
{
  return cpuGroup->getCount();
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#undef super
#define super IOInterruptController

OSDefineMetaClassAndStructors(IOCPUInterruptController, IOInterruptController);

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */


IOReturn IOCPUInterruptController::initCPUInterruptController(int sources)
{
  int cnt;
  
  if (!super::init()) return kIOReturnInvalid;
  
  numCPUs = sources;
  
  cpus = (IOCPU **)IOMalloc(numCPUs * sizeof(IOCPU *));
  if (cpus == 0) return kIOReturnNoMemory;
  bzero(cpus, numCPUs * sizeof(IOCPU *));
  
  vectors = (IOInterruptVector *)IOMalloc(numCPUs * sizeof(IOInterruptVector));
  if (vectors == 0) return kIOReturnNoMemory;
  bzero(vectors, numCPUs * sizeof(IOInterruptVector));
  
  // Allocate locks for the
  for (cnt = 0; cnt < numCPUs; cnt++) {
    vectors[cnt].interruptLock = IOLockAlloc();
    if (vectors[cnt].interruptLock == NULL) {
      for (cnt = 0; cnt < numCPUs; cnt++) {
	if (vectors[cnt].interruptLock != NULL)
	  IOLockFree(vectors[cnt].interruptLock);
      }
      return kIOReturnNoResources;
    }
  }
  
  return kIOReturnSuccess;
}

void IOCPUInterruptController::registerCPUInterruptController(void)
{
  registerService();
  
  getPlatform()->registerInterruptController(gPlatformInterruptControllerName,
					     this);
}

void IOCPUInterruptController::setCPUInterruptProperties(IOService *service)
{
  int          cnt;
  OSArray      *controller;
  OSArray      *specifier;
  OSData       *tmpData;
  long         tmpLong;
  
  // Create the interrupt specifer array.
  specifier = OSArray::withCapacity(numCPUs);
  for (cnt = 0; cnt < numCPUs; cnt++) {
    tmpLong = cnt;
    tmpData = OSData::withBytes(&tmpLong, sizeof(tmpLong));
    specifier->setObject(tmpData);
    tmpData->release();
  };
  
  // Create the interrupt controller array.
  controller = OSArray::withCapacity(numCPUs);
  for (cnt = 0; cnt < numCPUs; cnt++) {
    controller->setObject(gPlatformInterruptControllerName);
  }
  
  // Put the two arrays into the property table.
  service->setProperty(gIOInterruptControllersKey, controller);
  service->setProperty(gIOInterruptSpecifiersKey, specifier);
  controller->release();
  specifier->release();
}

void IOCPUInterruptController::enableCPUInterrupt(IOCPU *cpu)
{
  ml_install_interrupt_handler(cpu, cpu->getCPUNumber(), this,
                               (IOInterruptHandler)&IOCPUInterruptController::handleInterrupt, 0);
  
  enabledCPUs++;
  
  if (enabledCPUs == numCPUs) thread_wakeup(this);
}

IOReturn IOCPUInterruptController::registerInterrupt(IOService *nub,
						     int source,
						     void *target,
						     IOInterruptHandler handler,
						     void *refCon)
{
  IOInterruptVector *vector;
  
  if (source >= numCPUs) return kIOReturnNoResources;
  
  vector = &vectors[source];
  
  // Get the lock for this vector.
  IOTakeLock(vector->interruptLock);
  
  // Make sure the vector is not in use.
  if (vector->interruptRegistered) {
    IOUnlock(vector->interruptLock);
    return kIOReturnNoResources;
  }
  
  // Fill in vector with the client's info.
  vector->handler = handler;
  vector->nub     = nub;
  vector->source  = source;
  vector->target  = target;
  vector->refCon  = refCon;
  
  // Get the vector ready.  It starts hard disabled.
  vector->interruptDisabledHard = 1;
  vector->interruptDisabledSoft = 1;
  vector->interruptRegistered   = 1;
  
  IOUnlock(vector->interruptLock);
  
  if (enabledCPUs != numCPUs) {
    assert_wait(this, THREAD_UNINT);
    thread_block(0);
  }
  
  return kIOReturnSuccess;
}

IOReturn IOCPUInterruptController::getInterruptType(IOService */*nub*/,
						    int /*source*/,
						    int *interruptType)
{
  if (interruptType == 0) return kIOReturnBadArgument;
  
  *interruptType = kIOInterruptTypeLevel;
  
  return kIOReturnSuccess;
}

IOReturn IOCPUInterruptController::enableInterrupt(IOService */*nub*/,
						   int /*source*/)
{
//  ml_set_interrupts_enabled(true);
  return kIOReturnSuccess;
}

IOReturn IOCPUInterruptController::disableInterrupt(IOService */*nub*/,
						    int /*source*/)
{
//  ml_set_interrupts_enabled(false);
  return kIOReturnSuccess;
}

IOReturn IOCPUInterruptController::causeInterrupt(IOService */*nub*/,
						  int /*source*/)
{
  ml_cause_interrupt();
  return kIOReturnSuccess;
}

IOReturn IOCPUInterruptController::handleInterrupt(void */*refCon*/,
						   IOService */*nub*/,
						   int source)
{
  IOInterruptVector *vector;
  
  vector = &vectors[source];
  
  if (!vector->interruptRegistered) return kIOReturnInvalid;
  
  vector->handler(vector->target, vector->refCon,
		  vector->nub, vector->source);
  
  return kIOReturnSuccess;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
