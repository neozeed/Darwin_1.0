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


#include <ppc/proc_reg.h>

#include <IOKit/IOLib.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IODeviceMemory.h>
#include <IOKit/IOPlatformExpert.h>

#include "../drvAppleNMI/AppleNMI.h"

#include "Heathrow.h"

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define super AppleMacIO

OSDefineMetaClassAndStructors(Heathrow, AppleMacIO);

bool Heathrow::start(IOService *provider)
{
  bool		     ret;
  
  // Call MacIO's start.
  if (!super::start(provider))
    return false;
  
  // Figure out which heathrow this is.
  if (IODTMatchNubWithKeys(provider, "heathrow"))
    heathrowNum = kPrimaryHeathrow;
  else if (IODTMatchNubWithKeys(provider, "gatwick"))
    heathrowNum = kSecondaryHeathrow;
  else return false; // This should not happen.
  
  if (heathrowNum == kPrimaryHeathrow) {
    if (getPlatform()->getFamily() != 3)
      getPlatform()->setCPUInterruptProperties(provider);
  }
  
  // get the base address of the this heathrow.
  heathrowBaseAddress = fMemory->getVirtualAddress();
  
  // Make nubs for the children.
  publishBelow( provider );

  ret = installInterrupts(provider);

  return ret;
}

bool Heathrow::installInterrupts(IOService *provider)
{
  IORegistryEntry    *regEntry;
  OSSymbol           *interruptControllerName;
  IOInterruptAction  handler;
  AppleNMI           *appleNMI;
  long               nmiSource;
  OSData             *nmiData;
  IOReturn           error;

  // Everything below here is for interrupts, return true if
  // interrupts are not needed.
  if (getPlatform()->getFamily() == 3) return true;
  
  // get the name of the interrupt controller
  if( (regEntry = provider->childFromPath("interrupt-controller",
	gIODTPlane))) {
    interruptControllerName = (OSSymbol *)IODTInterruptControllerName(regEntry);
    regEntry->release();
  } else
    interruptControllerName = getInterruptControllerName();
  
  // Allocate the interruptController instance.
  interruptController = new HeathrowInterruptController;
  if (interruptController == NULL) return false;
  
  // call the interruptController's init method.
  error = interruptController->initInterruptController(provider, heathrowBaseAddress);
  if (error != kIOReturnSuccess) return false;
  
  handler = interruptController->getInterruptHandlerAddress();
  provider->registerInterrupt(0, interruptController, handler, 0);
  
  provider->enableInterrupt(0);
  
  // Register the interrupt controller so clients can find it.
  getPlatform()->registerInterruptController(interruptControllerName,
					     interruptController);
  
  if (heathrowNum != kPrimaryHeathrow) return true;
  
  // Create the NMI Driver.
  nmiSource = 20;
  nmiData = OSData::withBytes(&nmiSource, sizeof(long));
  appleNMI = new AppleNMI;
  if ((nmiData != 0) && (appleNMI != 0)) {
    appleNMI->initNMI(interruptController, nmiData);
  } 
  
  return true;
}

OSSymbol *Heathrow::getInterruptControllerName(void)
{
  OSSymbol *interruptControllerName;
  
  switch (heathrowNum) {
  case kPrimaryHeathrow :
    interruptControllerName = (OSSymbol *)gIODTDefaultInterruptController;
    break;
    
  case kSecondaryHeathrow :
    interruptControllerName = (OSSymbol *)OSSymbol::withCStringNoCopy("SecondaryInterruptController");
    break;
    
  default:
    interruptControllerName = (OSSymbol *)OSSymbol::withCStringNoCopy("UnknownInterruptController");
    break;
  }
  
  return interruptControllerName;
}

void Heathrow::processNub(IOService *nub)
{
  int           cnt, numSources;
  OSArray       *controllerNames, *controllerSources;
  OSSymbol      *interruptControllerName;
  char          *nubName;
  unsigned long heathrowIDs, heathrowFCR;
  
  nubName = (char *)nub->getName();
  
  // Turn on the media bay if this a PowerBook 101.
  if (IODTMatchNubWithKeys(getPlatform()->getProvider(), "'PowerBook1,1'")) {
    if (!strcmp(nubName, "media-bay")) {
      heathrowIDs = lwbrx(heathrowBaseAddress + 0x34);
      if ((heathrowIDs & 0x0000FF00) == 0x00003000) {
	heathrowFCR = lwbrx(heathrowBaseAddress + 0x38);
	heathrowFCR |= 0x00800000;
	stwbrx(heathrowFCR, heathrowBaseAddress + 0x38);
	IOSleep(500);
      }
    }
  }
  
  // change the interrupt controller name for this nub
  // if it is on the secondary heathrow.
  if (heathrowNum == kPrimaryHeathrow) return;
  
  interruptControllerName = getInterruptControllerName();
  
  if (!strcmp(nubName, "ch-a")) {
    controllerSources = OSDynamicCast(OSArray, getProperty("vectors-escc-ch-a"));
  } else if (!strcmp(nubName, "floppy")) {
      controllerSources = OSDynamicCast(OSArray, getProperty("vectors-floppy"));
  } else if (!strcmp(nubName, "ata4")) {
      controllerSources = OSDynamicCast(OSArray, getProperty("vectors-ata4"));
  } else return;
  
  numSources = controllerSources->getCount();
  
  controllerNames = OSArray::withCapacity(numSources);
  for (cnt = 0; cnt < numSources; cnt++) {
    controllerNames->setObject(interruptControllerName);
  }

  nub->setProperty(gIOInterruptControllersKey, controllerNames);
  nub->setProperty(gIOInterruptSpecifiersKey, controllerSources);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#undef  super
#define super IOInterruptController

OSDefineMetaClassAndStructors(HeathrowInterruptController, IOInterruptController);

IOReturn HeathrowInterruptController::initInterruptController(IOService *provider, IOLogicalAddress interruptControllerBase)
{
  int cnt;
  
  parentNub = provider;
  
  // Allocate the task lock.
  taskLock = IOLockAlloc();
  if (taskLock == 0) return kIOReturnNoResources;
  
  // Allocate the memory for the vectors
  vectors = (IOInterruptVector *)IOMalloc(kNumVectors * sizeof(IOInterruptVector));
  if (vectors == NULL) {
    IOLockFree(taskLock);
    return kIOReturnNoMemory;
  }
  bzero(vectors, kNumVectors * sizeof(IOInterruptVector));
  
  // Allocate locks for the 
  for (cnt = 0; cnt < kNumVectors; cnt++) {
    vectors[cnt].interruptLock = IOLockAlloc();
    if (vectors[cnt].interruptLock == NULL) {
      IOLockFree(taskLock);
      for (cnt = 0; cnt < kNumVectors; cnt++) {
	if (vectors[cnt].interruptLock != NULL)
	  IOLockFree(vectors[cnt].interruptLock);
      }
      return kIOReturnNoResources;
    }
  }
  
  // Setup the registers accessors
  events1Reg = (unsigned long)(interruptControllerBase + kEvents1Offset);
  events2Reg = (unsigned long)(interruptControllerBase + kEvents2Offset);
  mask1Reg   = (unsigned long)(interruptControllerBase + kMask1Offset);
  mask2Reg   = (unsigned long)(interruptControllerBase + kMask2Offset);
  clear1Reg  = (unsigned long)(interruptControllerBase + kClear1Offset);
  clear2Reg  = (unsigned long)(interruptControllerBase + kClear2Offset);
  levels1Reg = (unsigned long)(interruptControllerBase + kLevels1Offset);
  levels2Reg = (unsigned long)(interruptControllerBase + kLevels2Offset);
  
  // Initialize the registers.
  
  // Disable all interrupts.
  stwbrx(0x00000000, mask1Reg);
  stwbrx(0x00000000, mask2Reg);
  eieio();
  
  // Clear all pending interrupts.
  stwbrx(0xFFFFFFFF, clear1Reg);
  stwbrx(0xFFFFFFFF, clear2Reg);
  eieio();
  
  // Disable all interrupts. (again?)
  stwbrx(0x00000000, mask1Reg);
  stwbrx(0x00000000, mask2Reg);
  eieio();
  
  return kIOReturnSuccess;
}

IOInterruptAction HeathrowInterruptController::getInterruptHandlerAddress(void)
{
  return (IOInterruptAction)&HeathrowInterruptController::handleInterrupt;
}

IOReturn HeathrowInterruptController::handleInterrupt(void * /*refCon*/,
						      IOService * /*nub*/,
						      int /*source*/)
{
  int               done;
  long              events, vectorNumber;
  IOInterruptVector *vector;
  unsigned long     maskTmp;
  
  do {
    done = 1;
    
    // Do all the sources for events1, plus any pending interrupts.
    // Also add in the "level" sensitive sources
    maskTmp = lwbrx(mask1Reg);
    events = lwbrx(events1Reg) & ~kTypeLevelMask;
    events |= lwbrx(levels1Reg) & maskTmp & kTypeLevelMask;
    events |= pendingEvents1 & maskTmp;
    pendingEvents1 = 0;
    eieio();
    
    // Since we have to clear the level'd one clear the current edge's too.
    stwbrx(kTypeLevelMask | events, clear1Reg);
    eieio();
    
    if (events) done = 0;
    
    while (events) {
      vectorNumber = 31 - cntlzw(events);
      events ^= (1 << vectorNumber);
      vector = &vectors[vectorNumber];
      
      vector->interruptActive = 1;
      sync();
      isync();
      if (!vector->interruptDisabledSoft) {
	isync();
	
	// Call the handler if it exists.
	if (vector->interruptRegistered) {
	  vector->handler(vector->target, vector->refCon,
			  vector->nub, vector->source);
	}
      } else {
	// Hard disable the source.
	vector->interruptDisabledHard = 1;
	disableVectorHard(vectorNumber, vector);
      }
      
      vector->interruptActive = 0;
    }
    
    // Do all the sources for events2, plus any pending interrupts.
    maskTmp = lwbrx(mask2Reg);
    events = lwbrx(events2Reg);
    events |= pendingEvents1 & maskTmp;
    pendingEvents2 = 0;
    eieio();
    
    if (events) {
      done = 0;
      stwbrx(events, clear2Reg);
      eieio();
    }
    
    while (events) {
      vectorNumber = 31 - cntlzw(events);
      events ^= (1 << vectorNumber);
      vector = &vectors[vectorNumber + kVectorsPerReg];
      
      vector->interruptActive = 1;
      sync();
      isync();
      if (!vector->interruptDisabledSoft) {
	isync();
	
	// Call the handler if it exists.
	if (vector->interruptRegistered) {
	  vector->handler(vector->target, vector->refCon,
			  vector->nub, vector->source);
	}
      } else {
	// Hard disable the source.
	vector->interruptDisabledHard = 1;
	disableVectorHard(vectorNumber + kVectorsPerReg, vector);
      }
      
      vector->interruptActive = 0;
    }
  } while (!done);
  
  return kIOReturnSuccess;
}

bool HeathrowInterruptController::vectorCanBeShared(long /*vectorNumber*/, IOInterruptVector */*vector*/)
{
  return true;
}

int HeathrowInterruptController::getVectorType(long vectorNumber, IOInterruptVector */*vector*/)
{
  int interruptType;
  
  if (kTypeLevelMask & (1 << vectorNumber)) {
    interruptType = kIOInterruptTypeLevel;
  } else {
    interruptType = kIOInterruptTypeEdge;
  }
  
  return interruptType;
}

void HeathrowInterruptController::disableVectorHard(long vectorNumber, IOInterruptVector */*vector*/)
{
  unsigned long     maskTmp;
  
  // Turn the source off at hardware.
  if (vectorNumber < kVectorsPerReg) {
    maskTmp = lwbrx(mask1Reg);
    maskTmp &= ~(1 << vectorNumber);
    stwbrx(maskTmp, mask1Reg);
    eieio();
  } else {
    vectorNumber -= kVectorsPerReg;
    maskTmp = lwbrx(mask2Reg);
    maskTmp &= ~(1 << vectorNumber);
    stwbrx(maskTmp, mask2Reg);
    eieio();
  }
}

void HeathrowInterruptController::enableVector(long vectorNumber,
					       IOInterruptVector *vector)
{
  unsigned long     maskTmp;
  
  if (vectorNumber < kVectorsPerReg) {
    maskTmp = lwbrx(mask1Reg);
    maskTmp |= (1 << vectorNumber);
    stwbrx(maskTmp, mask1Reg);
    eieio();
    if (lwbrx(levels1Reg) & (1 << vectorNumber)) {
      // lost the interrupt
      causeVector(vectorNumber, vector);
    }
  } else {
    vectorNumber -= kVectorsPerReg;
    maskTmp = lwbrx(mask2Reg);
    maskTmp |= (1 << vectorNumber);
    stwbrx(maskTmp, mask2Reg);
    eieio();
    if (lwbrx(levels1Reg) & (1 << vectorNumber)) {
      // lost the interrupt
      causeVector(vectorNumber + kVectorsPerReg, vector);
    }
  }
}

void HeathrowInterruptController::causeVector(long vectorNumber,
					      IOInterruptVector */*vector*/)
{
  if (vectorNumber < kVectorsPerReg) {
    pendingEvents1 |= 1 << vectorNumber;
  } else {
    vectorNumber -= kVectorsPerReg;
    pendingEvents2 |= 1 << vectorNumber;
  }

  parentNub->causeInterrupt(0);
}
