/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 1999-2000 Apple Computer, Inc.  All rights reserved.
 *
 *  DRI: Josh de Cesare
 */


#include <ppc/proc_reg.h>

#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IODeviceMemory.h>
#include <IOKit/IOPlatformExpert.h>

#include "AppleMPIC.h"

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#undef  super
#define super IOInterruptController

OSDefineMetaClassAndStructors(AppleMPICInterruptController, IOInterruptController);

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool AppleMPICInterruptController::start(IOService *provider)
{
  long              cnt, regTemp;
  IOInterruptAction handler;
  
  if (!super::start(provider))
    return false;
  
  // If this is a PowerExpress get a different name.
  if (getPlatform()->getFamily() == 3) {
    interruptControllerName = gIODTDefaultInterruptController;
  } else {
    interruptControllerName = IODTInterruptControllerName(provider);
  }
  
  // Map the MPIC's memory.
  mpicMemoryMap = provider->mapDeviceMemoryWithIndex(0);
  
  if (mpicMemoryMap == 0) return false;
  
  // get the base address of the MPIC.
  mpicBaseAddress = mpicMemoryMap->getVirtualAddress();
  
  // Read the Feature Reporting Register
  regTemp = lwbrx(mpicBaseAddress + kFeatureOffset);
  numCPUs =    ((regTemp & kFRRNumCPUMask) >> kFRRNumCPUShift) + 1;
  numVectors = ((regTemp & kFRRNumIRQsMask) >> kFRRNumIRQsShift) + 1;
  
  // Allocate the memory for the senses.
  senses = (long *)IOMalloc(((numVectors + numCPUs + 31) / 32)*sizeof(long));
  if (senses == NULL) return false;
  for (cnt = 0; cnt < ((numVectors + numCPUs + 31) / 32); cnt++)
    senses[cnt] = 0;
  
  // Allocate the memory for the vectors.
  vectors = (IOInterruptVector *)IOMalloc((numVectors + numCPUs) *
					  sizeof(IOInterruptVector));
  if (vectors == NULL) return false;
  bzero(vectors, (numVectors + numCPUs) * sizeof(IOInterruptVector));
  
  // Allocate locks for the vectors.
  for (cnt = 0; cnt < (numVectors + numCPUs) ; cnt++) {
    vectors[cnt].interruptLock = IOLockAlloc();
    if (vectors[cnt].interruptLock == NULL) {
      for (cnt = 0; cnt < (numVectors + numCPUs); cnt++) {
	if (vectors[cnt].interruptLock != NULL)
	  IOLockFree(vectors[cnt].interruptLock);
      }
      return false;
    }
  }
  
  // Reset the MPIC.
  stwbrx(kGCR0Reset, mpicBaseAddress + kGlobal0Offset);
  eieio();
  
  // Wait for the reset to complete.
  do {
    regTemp = lwbrx(mpicBaseAddress + kGlobal0Offset);
    eieio();
  }  while (regTemp & kGCR0Reset);
  
  // Clear and mask all the interrupt vectors.
  for (cnt = 0; cnt < (numVectors + numCPUs); cnt++) {
    stwbrx(kIntnVPRMask, mpicBaseAddress+kIntnVecPriOffset+kIntnStride*cnt);
  }
  eieio();
  
  // Set the Spurious Vector Register.
  stwbrx(kSpuriousVectorNumber, mpicBaseAddress + kSpurVectOffset);
  
  // Set 8259 Cascade Mode.
  stwbrx(kGCR0Cascade, mpicBaseAddress + kGlobal0Offset);
  
  // Set the all CPUs Current Task Priority to zero.
  for(cnt = 0; cnt < numCPUs; cnt++) {		/* Do 'em all */
    stwbrx(0, mpicBaseAddress + kPnCurrTskPriOffset + (kPnStride * cnt));
    eieio();
  }
  
  registerService();  
  
  getPlatform()->setCPUInterruptProperties(provider);
  
  // register the interrupt handler so it can receive interrupts.
  handler = getInterruptHandlerAddress();
  for (cnt = 0; cnt < numCPUs; cnt++) {
    provider->registerInterrupt(cnt, this, handler, 0);
    provider->enableInterrupt(cnt);
  }
  
  // Register this interrupt controller so clients can find it.
  getPlatform()->registerInterruptController(interruptControllerName, this);
  
  return true;
}

IOReturn AppleMPICInterruptController::getInterruptType(IOService *nub,
							int source,
							int *interruptType)
{
  IOInterruptSource *interruptSources;
  OSData            *vectorData;
  long              vectorNumber;
  
  if (interruptType == 0) return kIOReturnBadArgument;
  
  interruptSources = nub->_interruptSources;
  vectorData = interruptSources[source].vectorData;
  vectorNumber = *(long *)vectorData->getBytesNoCopy();
  *interruptType = ((long *)vectorData->getBytesNoCopy())[1];
  
  return kIOReturnSuccess;
}

IOInterruptAction AppleMPICInterruptController::getInterruptHandlerAddress(void)
{
  return (IOInterruptAction)&AppleMPICInterruptController::handleInterrupt;
}

IOReturn AppleMPICInterruptController::handleInterrupt(void */*refCon*/,
						       IOService */*nub*/,
						       int source)
{
  long              vectorNumber, level;
  IOInterruptVector *vector;
  
  do {
    vectorNumber = lwbrx(mpicBaseAddress + source*kPnStride + kPnIntAckOffset);
    eieio();
    
    if (vectorNumber == kSpuriousVectorNumber) break;
    
    level = senses[vectorNumber / 32] & (1 << (vectorNumber & 31));    
    
    if (!level) {
      stwbrx(0, mpicBaseAddress + source * kPnStride + kPnEOIOffset);
      eieio();
    }
    
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
	
	if (level && vector->interruptDisabledSoft) {
	  // Hard disable the source.
	  vector->interruptDisabledHard = 1;
	  disableVectorHard(vectorNumber, vector);
	}
      }
    } else {
      // Hard disable the source.
      vector->interruptDisabledHard = 1;
      disableVectorHard(vectorNumber, vector);
    }
    
    if (level) {
      stwbrx(0, mpicBaseAddress + source * kPnStride + kPnEOIOffset);
      eieio();
    }
    
    vector->interruptActive = 0;
    
  } while (1);
  
  return kIOReturnSuccess;
}

bool AppleMPICInterruptController::vectorCanBeShared(long /*vectorNumber*/, IOInterruptVector */*vector*/)
{
  return true;
}

void AppleMPICInterruptController::initVector(long vectorNumber, IOInterruptVector *vector)
{
  IOInterruptSource *interruptSources;
  long              vectorType;
  OSData            *vectorData;
  long              regTemp, vectorBase;
  
  // Get the vector's type.
  interruptSources = vector->nub->_interruptSources;
  vectorData = interruptSources[vector->source].vectorData;
  vectorType = ((long *)vectorData->getBytesNoCopy())[1];
  
  if (vectorType == kIOInterruptTypeEdge) {
    senses[vectorNumber / 32] &= ~(1 << (vectorNumber & 31));
  } else {
    senses[vectorNumber / 32] |= (1 << (vectorNumber & 31));
  }
  
  // Set the Vector/Priority and Destination Registers
  if (vectorNumber < numVectors) {
    vectorBase = mpicBaseAddress + kIntnStride * vectorNumber;
    
    regTemp = 0x1; // CPU 0 only.
    stwbrx(regTemp, vectorBase + kIntnDestOffset);
    eieio();
    
    // Vectors start masked with priority 8.
    regTemp  = kIntnVPRMask | (8 << kIntnVPRPriorityShift);
    regTemp |= (vectorType == kIOInterruptTypeLevel) ? kIntnVPRSense : 0;
    regTemp |= vectorNumber << kIntnVPRVectorShift;
    stwbrx(regTemp, vectorBase + kIntnVecPriOffset);
    eieio();
  } else {
    vectorBase = mpicBaseAddress + kIPInVecPriStride*(vectorNumber-numVectors);
    
    // IPI Vectors start masked with priority 14.
    regTemp  = kIntnVPRMask | (14 << kIntnVPRPriorityShift);
    regTemp |= vectorNumber << kIntnVPRVectorShift;
    stwbrx(regTemp, vectorBase + kIPInVecPriOffset);
    eieio();
  }
}

void AppleMPICInterruptController::disableVectorHard(long vectorNumber, IOInterruptVector */*vector*/)
{
  long     regTemp, vectorBase;
  
  if (vectorNumber < numVectors) {
    vectorBase = mpicBaseAddress + kIntnVecPriOffset +
      kIntnStride * vectorNumber;
  } else {
    vectorBase = mpicBaseAddress + kIPInVecPriOffset +
      kIPInVecPriStride * (vectorNumber - numVectors);
  }
  
  regTemp = lwbrx(vectorBase);
  regTemp |= kIntnVPRMask;
  stwbrx(regTemp, vectorBase);
  eieio();
}

void AppleMPICInterruptController::enableVector(long vectorNumber,
						IOInterruptVector */*vector*/)
{
  long     regTemp, vectorBase;
  
  if (vectorNumber < numVectors) {
    vectorBase = mpicBaseAddress + kIntnVecPriOffset +
      kIntnStride * vectorNumber;
  } else {
    vectorBase = mpicBaseAddress + kIPInVecPriOffset +
      kIPInVecPriStride * (vectorNumber - numVectors);
  }
  
  regTemp = lwbrx(vectorBase);
  regTemp &= ~kIntnVPRMask;
  stwbrx(regTemp, vectorBase);
  eieio();
}

OSData *AppleMPICInterruptController::getIPIVector(long physCPU)
{
  long   tmpLongs[2];
  OSData *tmpData;
  
  if ((physCPU < 0) && (physCPU >= numCPUs)) return 0;
  
  tmpLongs[0] = numVectors + physCPU;
  tmpLongs[1] = kIOInterruptTypeEdge;
  
  tmpData = OSData::withBytes(tmpLongs, 2 * sizeof(long));
  
  return tmpData;
}

void AppleMPICInterruptController::dispatchIPI(long source, long targetMask)
{
  long ipiBase, cnt;
  
  ipiBase = mpicBaseAddress + kPnIPImDispOffset + kPnStride * source;
  
  for (cnt = 0; cnt < numCPUs; cnt++) {
    if (targetMask & (1 << cnt)) {
      stwbrx((1 << cnt), ipiBase + kPnIPImDispStride * cnt);
      eieio();
    }
  }
}
