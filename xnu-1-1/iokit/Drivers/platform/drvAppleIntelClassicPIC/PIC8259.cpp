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
 *  DRI: Michael Burg
 */


#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>
#include <IOKit/IODeviceMemory.h>
#include <IOKit/IOPlatformExpert.h>

#include "AppleIntelClassicPIC.h"

#define outb(port, datum) \
      __asm__ volatile("outb %b0, %1" : : "a" ((unsigned char)(datum)), "i" ((unsigned short)port))

#define send_duel_eoi(port1, port2)  \
	__asm__ volatile("outb %b0, %1 ;  outb %b0, %2" : : "a" (kEOICommand), "i" ((unsigned short)port1), "i" ((unsigned short)port2))


#define kIntelReservedIntVectors	0x40

extern "C" {
/* HACK, HACK, HACK.. for rtclock on intel.. FIXME */
extern void (*IOKitRegisterInterruptHook)(void *, int, int);
};

static AppleIntelClassicPIC	*picObjectHook;

IOInterruptController		*gIntelInterruptController;

extern	OSSymbol	*gIntelPICName;

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#undef  super
#define super IOInterruptController

OSDefineMetaClassAndStructors(AppleIntelClassicPIC, IOInterruptController);

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool AppleIntelClassicPIC::start(IOService *provider)
{
  IOInterruptAction    handler;
  int cnt;

  if (!super::start(provider))
    return false;
  
  
  // Allocate the memory for the vectors.
  vectors = (IOInterruptVector *)IOMalloc(kNumVectors *
					  sizeof(IOInterruptVector));
  if (vectors == NULL) return false;

  bzero(vectors, kNumVectors * sizeof(IOInterruptVector));
  

  // Allocate locks for the vectors.
  for (cnt = 0; cnt < kNumVectors; cnt++) {
    vectors[cnt].interruptLock = IOLockAlloc();
    if (vectors[cnt].interruptLock == NULL) {
      for (cnt = 0; cnt < kNumVectors; cnt++) {
	if (vectors[cnt].interruptLock != NULL)
	  IOLockFree(vectors[cnt].interruptLock);
      }
      return false;
    }
  }
  
  maskInterrupts = 0xffff & ~(1<<kPICSlaveID);	/* Mask out the interrupts except for the casacde line */

  /* Setup the interrupt controller.. */
  /* Original code taken from FreeBSD's sys/i386/isa/intr_machdep.c */

  /* initialize 8259's */
  outb(kPIC1BasePort, 0x11);		/* reset; program device, four bytes */

  /* starting at this vector index */
  outb(kPIC1BasePort+kPICDataPortOffset, kIntelReservedIntVectors);

  outb(kPIC1BasePort+kPICDataPortOffset, kPICSlaveIRQID);	/* slave on line 7 */

  outb(kPIC1BasePort+kPICDataPortOffset, 1);		/* 8086 mode */

  outb(kPIC1BasePort+kPICDataPortOffset, 0xff);		/* leave interrupts masked */
  outb(kPIC1BasePort, 0x0a);		/* default to IRR on read */
  outb(kPIC1BasePort, 0xc0 | (3 - 1));	/* pri order 3-7, 0-2 (com2 first) */

  outb(kPIC2BasePort, 0x11);		/* reset; program device, four bytes */
  outb(kPIC2BasePort+kPICDataPortOffset, kIntelReservedIntVectors+8); /* staring at this vector index */
  outb(kPIC2BasePort+kPICDataPortOffset, kPICSlaveID);         /* my slave id is 7 */
  outb(kPIC2BasePort+kPICDataPortOffset,1);		/* 8086 mode */
  outb(kPIC2BasePort+kPICDataPortOffset, 0xff);	/* leave interrupts masked */
  outb(kPIC2BasePort, 0x0a);			/* default to IRR on read */

  // Primary interrupt controller
  getPlatform()->setCPUInterruptProperties(provider);
    
  // setup clock interrupt back door hook
  IOKitRegisterInterruptHook = getRegisterInterruptHookAddress();
  picObjectHook = this;

  // register the interrupt handler so it can receive interrupts.
  handler = getInterruptHandlerAddress();
  if (provider->registerInterrupt(0, this, handler, 0) != kIOReturnSuccess) 
	panic("AppleIntelClassicPIC: Failed to install platform interrupt handler");
  provider->enableInterrupt(0);
  
  // Register this interrupt controller so clients can find it.
  getPlatform()->registerInterruptController(gIntelPICName, this) ;
  gIntelInterruptController = this;

  return true;
}

IOReturn AppleIntelClassicPIC::getInterruptType(IOService */*nub*/,
							int /*source*/,
							int *interruptType)
{
  if (interruptType == 0) return kIOReturnBadArgument;

   *interruptType = kIOInterruptTypeEdge;

  return kIOReturnSuccess;
}

IOInterruptAction AppleIntelClassicPIC::getInterruptHandlerAddress(void)
{
  void	*value;

  value = (void *) &AppleIntelClassicPIC::handleInterrupt;

  return (IOInterruptAction)(value);
}

IOReturn AppleIntelClassicPIC::handleInterrupt(void *savedState,
						       IOService */*nub*/,
						       int vectorNumber)
{
  IOInterruptVector *vector;
  typedef void (*IntelClockFuncType)(void *);
  IntelClockFuncType	clockFunc;

  vectorNumber -= kIntelReservedIntVectors;

  if (vectorNumber >= kNumVectors) return kIOReturnSuccess;
    
  maskInterrupts |= (1 << vectorNumber);

  if (vectorNumber > 7) {
        /* Mask out interrupt */
	outb(kPIC2BasePort+kPICDataPortOffset, maskInterrupts >> 8);

  	/* Acknowledge interrupt */
	send_duel_eoi(kPIC2BasePort+kPICCmdPortOffset, kPIC1BasePort+kPICCmdPortOffset);
  } else {
	outb(kPIC1BasePort+kPICDataPortOffset, maskInterrupts & 0xff);
  	outb(kPIC1BasePort+kPICCmdPortOffset, kEOICommand);
  }

  vector = &vectors[vectorNumber];

  vector->interruptActive = 1;
  if (!vector->interruptDisabledSoft) {
      if (vector->interruptRegistered) {
	if (vectorNumber == kClockIRQ) {
		clockFunc = (IntelClockFuncType) vector->handler;
		clockFunc(savedState);
	} else {
		vector->handler(vector->target, vector->refCon,
			vector->nub, vector->source);
	}
        maskInterrupts &= ~(1 << vectorNumber);
     }
  } else {
      printf("*** DISABLING INT %d\n", vectorNumber);
      // Hard disable the source.
      vector->interruptDisabledHard = 1;
      disableVectorHard(vectorNumber, vector);
   }

  if (vectorNumber > 7) 
	outb(kPIC2BasePort+kPICDataPortOffset, maskInterrupts >> 8);
  else
	outb(kPIC1BasePort+kPICDataPortOffset, maskInterrupts & 0xff );


  vector->interruptActive = 0;
    
  return kIOReturnSuccess;
}

bool AppleIntelClassicPIC::vectorCanBeShared(long /*vectorNumber*/, IOInterruptVector */*vector*/)
{
  /* FIXME! Need to account for PCI sharable interrupts */
  return false;
}

void AppleIntelClassicPIC::initVector(long /*vectorNumber*/,
				IOInterruptVector * /*vector*/)
{
	/* Do nothing.. */
}

void AppleIntelClassicPIC::disableVectorHard(long vectorNumber, IOInterruptVector */*vector*/)
{
  if (vectorNumber == kPICSlaveID)	/* Sorry, cacade interrupt cannot be disable */
	return;	

  maskInterrupts |= (1<<vectorNumber);

  /* Load up mask interrupts */
  if (vectorNumber > 7)
  	outb(kPIC2BasePort + kPICDataPortOffset, maskInterrupts >> 8);
  else
  	outb(kPIC1BasePort + kPICDataPortOffset, maskInterrupts & 0xff);
}

void AppleIntelClassicPIC::enableVector(long vectorNumber,
						IOInterruptVector *vector)
{
  maskInterrupts &= ~(1<<vectorNumber);

  /* Load up mask interrupts */
  if (vectorNumber > 7)
  	outb(kPIC2BasePort + kPICDataPortOffset, maskInterrupts >> 8);
  else
  	outb(kPIC1BasePort + kPICDataPortOffset, maskInterrupts & 0xff);
}

static void AppleIntelClassicPICRegisterHook(void *func, int source, int isclock)
{
  picObjectHook->registerInterruptHook(func, source, isclock);
}

void AppleIntelClassicPIC::registerInterruptHook(void *func, int source, int isclock)
{
	IOInterruptVector	*vector;

	vector = &vectors[source];

	vector->handler = (IOInterruptHandler) func;
	vector->interruptRegistered = TRUE;
	vector->interruptDisabledSoft = FALSE;
	vector->interruptDisabledHard = FALSE;

	enableVector(source, vector);
}


IOKitInterruptHookType
AppleIntelClassicPIC::getRegisterInterruptHookAddress(void)
{
	void *value;
	value = (void *) AppleIntelClassicPICRegisterHook;
	return (IOKitInterruptHookType) value;
}
