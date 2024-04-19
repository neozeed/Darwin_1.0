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
 *  DRI: Josh de Cesare
 *
 */

#ifndef _IOKIT_APPLEINTELCLASSICPIC_H
#define _IOKIT_APPLEINTELCLASSICPIC_H

#include <IOKit/IOInterrupts.h>
#include <IOKit/IOInterruptController.h>

#define kClockIRQ	0	/* FIXME for SMP systems. */

#define	kPIC1BasePort	0x20
#define	kPIC2BasePort	0xa0

#define kPICCmdPortOffset	0
#define	kPICDataPortOffset	1

#define kEOICommand	0x20

#define kPICSlaveIRQID	0x04	/* Slave ID for second PIC on first pic */
#define	kPICSlaveID	2	/* Slave ID for second PIC */

#define	kNumVectors	(16)

typedef void (*IOKitInterruptHookType)(void *, int, int);

class AppleIntelClassicPIC : public IOInterruptController
{
  OSDeclareDefaultStructors(AppleIntelClassicPIC);
  
private:
  IOService              *parentNub;

  volatile unsigned short	maskInterrupts;		/* Which interrupts are masked out */
  IOKitInterruptHookType getRegisterInterruptHookAddress(void);

public:
  virtual bool start(IOService *provider);

  virtual IOReturn getInterruptType(IOService *nub, int source,
				    int *interruptType);
  
  virtual IOInterruptAction getInterruptHandlerAddress(void);
  virtual IOReturn handleInterrupt(void *refCon,
				   IOService *nub, int source);
  
  virtual bool vectorCanBeShared(long vectorNumber, IOInterruptVector *vector);
  virtual void initVector(long vectorNumber, IOInterruptVector *vector);
  virtual void disableVectorHard(long vectorNumber, IOInterruptVector *vector);
  virtual void enableVector(long vectorNumber, IOInterruptVector *vector);

  void		registerInterruptHook(void *func, int source, int isClock);
};


#endif /* ! _IOKIT_APPLEINTELCLASSICPIC_H */
