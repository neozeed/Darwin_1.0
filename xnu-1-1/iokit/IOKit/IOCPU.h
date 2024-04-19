/*
 * Copyright (c) 1999 Apple Computer, Inc.  All rights reserved.
 *
 *  DRI: Josh de Cesare
 *
 */

#ifndef _IOKIT_CPU_H
#define _IOKIT_CPU_H

extern "C" {
#include <machine/machine_routines.h>
#include <pexpert/pexpert.h>
}

#include <IOKit/IOService.h>
#include <IOKit/IOInterruptController.h>

#define kIOCPUStateUnregistered (0)
#define kIOCPUStateUninitalized (1)
#define kIOCPUStateStopped      (2)
#define kIOCPUStateRunning      (3)

class IOCPUInterruptController;

extern IOCPUInterruptController *gIOCPUInterruptController;

class IOCPU : public IOService
{
  OSDeclareAbstractStructors(IOCPU);
  
protected:
  OSDictionary  *cpuGroup;
  IOService     *cpuNub;
  int           physCPU;
  int           cpuState;
  processor_t   machProcessor;
  ipi_handler_t ipi_handler;
  
public:
  static  void          initCPUs(void);
  
  virtual bool          start(IOService *provider);
  virtual void          initCPU(void);
  virtual kern_return_t startCPU(vm_offset_t start_paddr,
				 vm_offset_t arg_paddr);
  virtual void          haltCPU(vm_offset_t start_paddr,
				vm_offset_t arg_paddr);
  virtual void          signalCPU(IOCPU *target);
  
  virtual int           getCPUNumber(void);
  virtual int           getCPUState(void);
  virtual OSDictionary  *getCPUGroup(void);
  virtual int           getCPUGroupSize(void);
  
  virtual OSSymbol      *getCPUName(void) = 0;
};

class IOCPUInterruptController : public IOInterruptController
{
  OSDeclareDefaultStructors(IOCPUInterruptController);
  
private:
  int   enabledCPUs;
  
protected:  
  int   numCPUs;
  IOCPU **cpus;
  
  
public:
  virtual IOReturn initCPUInterruptController(int sources);
  virtual void     registerCPUInterruptController(void);
  virtual void     setCPUInterruptProperties(IOService *service);
  virtual void     enableCPUInterrupt(IOCPU *cpu);
  
  virtual IOReturn registerInterrupt(IOService *nub, int source,
				     void *target,
				     IOInterruptHandler handler,
				     void *refCon);
  
  virtual IOReturn getInterruptType(IOService *nub, int source,
				    int *interruptType);
  
  virtual IOReturn enableInterrupt(IOService *nub, int source);
  virtual IOReturn disableInterrupt(IOService *nub, int source);
  virtual IOReturn causeInterrupt(IOService *nub, int source);
  
  virtual IOReturn handleInterrupt(void *refCon, IOService *nub,
				   int source);
};

#endif /* ! _IOKIT_CPU_H */
