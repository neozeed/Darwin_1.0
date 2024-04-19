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

#ifndef _IOKIT_HEATHROW_H
#define _IOKIT_HEATHROW_H

#include <IOKit/platform/AppleMacIO.h>

#include <IOKit/IOInterrupts.h>
#include <IOKit/IOInterruptController.h>

#define kPrimaryHeathrow   (0)
#define kSecondaryHeathrow (1)

#define kNumVectors        (64)
#define kVectorsPerReg     (32)

#define kTypeLevelMask     (0x1FF00000)

#define kEvents1Offset     (0x00020)
#define kEvents2Offset     (0x00010)
#define kMask1Offset       (0x00024)
#define kMask2Offset       (0x00014)
#define kClear1Offset      (0x00028)
#define kClear2Offset      (0x00018)
#define kLevels1Offset     (0x0002C)
#define kLevels2Offset     (0x0001C)


class HeathrowInterruptController;

class Heathrow : public AppleMacIO
{
  OSDeclareDefaultStructors(Heathrow);
  
private:
  IOLogicalAddress             heathrowBaseAddress;
  long                         heathrowNum;
  HeathrowInterruptController  *interruptController;
  
  virtual bool installInterrupts(IOService *provider);
  virtual OSSymbol *getInterruptControllerName(void);
  
  virtual void processNub(IOService *nub);
  
public:
  virtual bool start(IOService *provider);
};


class HeathrowInterruptController : public IOInterruptController
{
  OSDeclareDefaultStructors(HeathrowInterruptController);
  
private:
  IOService         *parentNub;
  IOLock            *taskLock;
  unsigned long     pendingEvents1;
  unsigned long     pendingEvents2;
  unsigned long     events1Reg;
  unsigned long     events2Reg;
  unsigned long     mask1Reg;
  unsigned long     mask2Reg;
  unsigned long     clear1Reg;
  unsigned long     clear2Reg;
  unsigned long     levels1Reg;
  unsigned long     levels2Reg;
  
public:
  virtual IOReturn initInterruptController(IOService *provider,
					   IOLogicalAddress interruptControllerBase);
  
  virtual IOInterruptAction getInterruptHandlerAddress(void);
  virtual IOReturn handleInterrupt(void *refCon, IOService *nub, int source);
  
  virtual bool vectorCanBeShared(long vectorNumber, IOInterruptVector *vector);
  virtual int  getVectorType(long vectorNumber, IOInterruptVector *vector);
  virtual void disableVectorHard(long vectorNumber, IOInterruptVector *vector);
  virtual void enableVector(long vectorNumber, IOInterruptVector *vector);
  virtual void causeVector(long vectorNumber, IOInterruptVector *vector);
};


#endif /* ! _IOKIT_HEATHROW_H */
