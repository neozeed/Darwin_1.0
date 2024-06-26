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


#ifdef __ppc__
#include <ppc/proc_reg.h> 
#endif

#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/IOInterrupts.h>
#include <IOKit/IOInterruptController.h>


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define super IOService

OSDefineMetaClass(IOInterruptController, IOService);
OSDefineAbstractStructors(IOInterruptController, IOService);

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOReturn IOInterruptController::registerInterrupt(IOService *nub, int source,
						  void *target,
						  IOInterruptHandler handler,
						  void *refCon)
{
  IOInterruptSource *interruptSources;
  long              vectorNumber;
  IOInterruptVector *vector;
  long              wasDisabledSoft;
  IOReturn          error;
  OSData            *vectorData;
  IOService         *originalNub;
  int               originalSource;
  
  interruptSources = nub->_interruptSources;
  vectorData = interruptSources[source].vectorData;
  vectorNumber = *(long *)vectorData->getBytesNoCopy();
  vector = &vectors[vectorNumber];
  
  // Get the lock for this vector.
  IOTakeLock(vector->interruptLock);
  
  // If this vector is already in use, and can be shared,
  // register as a shared interrupt.
  if (vector->interruptRegistered) {
    if (!vectorCanBeShared(vectorNumber, vector)) {
      IOUnlock(vector->interruptLock);
      return kIOReturnNoResources;
    }
    
    // If this vector is not already shared, break it out.
    if (vector->sharedController == 0) {
      // Make the IOShareInterruptController instance
      vector->sharedController = new IOSharedInterruptController;
      if (vector->sharedController == 0) {
        IOUnlock(vector->interruptLock);
        return kIOReturnNoMemory;
      }
      
      // Save the nub and source for the original consumer.
      originalNub = vector->nub;
      originalSource = vector->source;
      
      // Save the dis/enable state for the original consumer's interrupt.
      // Then disable the source
      wasDisabledSoft = vector->interruptDisabledSoft;
      disableInterrupt(originalNub, originalSource);
      
      // Initialize the new shared interrupt controller.
      error = vector->sharedController->initInterruptController(this,
                                                                vectorData);
      // If the IOSharedInterruptController could not be initalized,
      // put the original consumor's interrupt back to normal and
      // get rid of whats left of the shared controller.
      if (error != kIOReturnSuccess) {
        enableInterrupt(originalNub, originalSource);
        vector->sharedController->release();
        vector->sharedController = 0;
        IOUnlock(vector->interruptLock);
        return error;
      }
      
      // Try to register the original consumer on the shared controller.
      error = vector->sharedController->registerInterrupt(originalNub,
                                                          originalSource,
                                                          vector->target,
                                                          vector->handler,
                                                          vector->refCon);
      // If the original consumer could not be moved to the shared controller,
      // put the original consumor's interrupt back to normal and
      // get rid of whats left of the shared controller.
      if (error != kIOReturnSuccess) {
        enableInterrupt(originalNub, originalSource);
        vector->sharedController->release();
        vector->sharedController = 0;
        IOUnlock(vector->interruptLock);
        return error;
      }
      
      // Fill in vector with the shared controller's info.
      vector->handler = (IOInterruptHandler)vector->sharedController->getInterruptHandlerAddress();
      vector->nub     = vector->sharedController;
      vector->source  = 0;
      vector->target  = vector->sharedController;
      vector->refCon  = 0;
      
      // Enable the original consumer's interrupt if needed.
      if (!wasDisabledSoft) originalNub->enableInterrupt(originalSource);
    }
    
    error = vector->sharedController->registerInterrupt(nub, source, target,
                                                        handler, refCon);
    IOUnlock(vector->interruptLock);
    return error;
  }
  
  // Fill in vector with the client's info.
  vector->handler = handler;
  vector->nub     = nub;
  vector->source  = source;
  vector->target  = target;
  vector->refCon  = refCon;
  
  // Do any specific initalization for this vector.
  initVector(vectorNumber, vector);
  
  // Get the vector ready.  It starts hard disabled.
  vector->interruptDisabledHard = 1;
  vector->interruptDisabledSoft = 1;
  vector->interruptRegistered   = 1;
  
  IOUnlock(vector->interruptLock);
  return kIOReturnSuccess;
}

IOReturn IOInterruptController::unregisterInterrupt(IOService *nub, int source)
{
  IOInterruptSource *interruptSources;
  long              vectorNumber;
  IOInterruptVector *vector;
  OSData            *vectorData;
  
  interruptSources = nub->_interruptSources;
  vectorData = interruptSources[source].vectorData;
  vectorNumber = *(long *)vectorData->getBytesNoCopy();
  vector = &vectors[vectorNumber];
  
  // Get the lock for this vector.
  IOTakeLock(vector->interruptLock);
  
  // Return success if it is not already registered
  if (vector->interruptRegistered) {
    IOUnlock(vector->interruptLock);
    return kIOReturnSuccess;
  }
  
  // Soft disable the source.
  disableInterrupt(nub, source);
  
  // Turn the source off at hardware. 
  disableVectorHard(vectorNumber, vector);
  
  // Clear all the storage for the vector except for interruptLock.
  vector->interruptActive = 0;
  vector->interruptDisabledSoft = 0;
  vector->interruptDisabledHard = 0;
  vector->interruptRegistered = 0;
  vector->nub = 0;
  vector->source = 0;
  vector->handler = 0;
  vector->target = 0;
  vector->refCon = 0;
  
  IOUnlock(vector->interruptLock);
  return kIOReturnSuccess;
}

IOReturn IOInterruptController::getInterruptType(IOService *nub, int source,
						 int *interruptType)
{
  IOInterruptSource *interruptSources;
  long              vectorNumber;
  IOInterruptVector *vector;
  OSData            *vectorData;
  
  if (interruptType == 0) return kIOReturnBadArgument;
  
  interruptSources = nub->_interruptSources;
  vectorData = interruptSources[source].vectorData;
  vectorNumber = *(long *)vectorData->getBytesNoCopy();
  vector = &vectors[vectorNumber];
  
  *interruptType = getVectorType(vectorNumber, vector);
  
  return kIOReturnSuccess;
}

IOReturn IOInterruptController::enableInterrupt(IOService *nub, int source)
{
  IOInterruptSource *interruptSources;
  long              vectorNumber;
  IOInterruptVector *vector;
  OSData            *vectorData;
  
  interruptSources = nub->_interruptSources;
  vectorData = interruptSources[source].vectorData;
  vectorNumber = *(long *)vectorData->getBytesNoCopy();
  vector = &vectors[vectorNumber];
  
  if (vector->interruptDisabledSoft) {
    vector->interruptDisabledSoft = 0;
    
    if (vector->interruptDisabledHard) {
      vector->interruptDisabledHard = 0;
      
      enableVector(vectorNumber, vector);
    }
  }
  
  return kIOReturnSuccess;
}

IOReturn IOInterruptController::disableInterrupt(IOService *nub, int source)
{
  IOInterruptSource *interruptSources;
  long              vectorNumber;
  IOInterruptVector *vector;
  OSData            *vectorData;
  
  interruptSources = nub->_interruptSources;
  vectorData = interruptSources[source].vectorData;
  vectorNumber = *(long *)vectorData->getBytesNoCopy();
  vector = &vectors[vectorNumber];
  
  vector->interruptDisabledSoft = 1;
#ifdef __ppc__
  sync();
  isync();
#endif
  
  if (!getPlatform()->atInterruptLevel()) {
    while (vector->interruptActive);
#ifdef __ppc__
    isync();
#endif
  }
  
  return kIOReturnSuccess;
}

IOReturn IOInterruptController::causeInterrupt(IOService *nub, int source)
{
  IOInterruptSource *interruptSources;
  long              vectorNumber;
  IOInterruptVector *vector;
  OSData            *vectorData;

  interruptSources = nub->_interruptSources;
  vectorData = interruptSources[source].vectorData;
  vectorNumber = *(long *)vectorData->getBytesNoCopy();
  vector = &vectors[vectorNumber];
  
  causeVector(vectorNumber, vector);
  
  return kIOReturnSuccess;
}

IOInterruptAction IOInterruptController::getInterruptHandlerAddress(void)
{
  return 0;
}

IOReturn IOInterruptController::handleInterrupt(void *refCon, IOService *nub,
						int source)
{
  return kIOReturnInvalid;
}


// Methods to be overridden for simplifed interrupt controller subclasses.

bool IOInterruptController::vectorCanBeShared(long /*vectorNumber*/,
					      IOInterruptVector */*vector*/)
{
  return false;
}

void IOInterruptController::initVector(long /*vectorNumber*/,
				       IOInterruptVector */*vector*/)
{
}

int IOInterruptController::getVectorType(long /*vectorNumber*/,
					  IOInterruptVector */*vector*/)
{
  return kIOInterruptTypeEdge;
}

void IOInterruptController::disableVectorHard(long /*vectorNumber*/,
					      IOInterruptVector */*vector*/)
{
}

void IOInterruptController::enableVector(long /*vectorNumber*/,
					 IOInterruptVector */*vector*/)
{
}

void IOInterruptController::causeVector(long /*vectorNumber*/,
					IOInterruptVector */*vector*/)
{
}


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#undef  super
#define super IOInterruptController

OSDefineMetaClassAndStructors(IOSharedInterruptController, IOInterruptController);

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOReturn IOSharedInterruptController::initInterruptController(IOInterruptController *parentController, OSData *parentSource)
{
  int      cnt, interruptType;
  IOReturn error;
  
  if (!super::init())
    return kIOReturnNoResources;
  
  // Set provider to this so enable/disable nub stuff works.
  provider = this;
  
  // Allocate the IOInterruptSource so this can act like a nub.
  _interruptSources = (IOInterruptSource *)IOMalloc(sizeof(IOInterruptSource));
  if (_interruptSources == 0) return kIOReturnNoMemory;
  _numInterruptSources = 1;
  
  // Set up the IOInterruptSource to point at this.
  _interruptSources[0].interruptController = parentController;
  _interruptSources[0].vectorData = parentSource;
  
  sourceIsLevel = false;
  error = provider->getInterruptType(0, &interruptType);
  if (error == kIOReturnSuccess) {
    if (interruptType & kIOInterruptTypeLevel)
      sourceIsLevel = true;
  }
  
  // Allocate the memory for the vectors
  numVectors = 8; // For now a constant number.
  vectors = (IOInterruptVector *)IOMalloc(numVectors * sizeof(IOInterruptVector));
  if (vectors == NULL) {
    IOFree(_interruptSources, sizeof(IOInterruptSource));
    return kIOReturnNoMemory;
  }
  bzero(vectors, numVectors * sizeof(IOInterruptVector));
  
  // Allocate locks for the
  for (cnt = 0; cnt < numVectors; cnt++) {
    vectors[cnt].interruptLock = IOLockAlloc();
    if (vectors[cnt].interruptLock == NULL) {
      for (cnt = 0; cnt < numVectors; cnt++) {
	if (vectors[cnt].interruptLock != NULL)
	  IOLockFree(vectors[cnt].interruptLock);
      }
      return kIOReturnNoResources;
    }
  }
  
  vectorsRegistered = 0;
  vectorsEnabled = 0;
  controllerDisabled = 1;
  
  return kIOReturnSuccess;
}

IOReturn IOSharedInterruptController::registerInterrupt(IOService *nub,
							int source,
							void *target,
							IOInterruptHandler handler,
							void *refCon)
{
  IOInterruptSource *interruptSources;
  long              vectorNumber;
  IOInterruptVector *vector = 0;
  OSData            *vectorData;
  
  interruptSources = nub->_interruptSources;
  
  // Find a free vector.
  vectorNumber = numVectors;
  while (vectorsRegistered != numVectors) {
    for (vectorNumber = 0; vectorNumber < numVectors; vectorNumber++) {
      vector = &vectors[vectorNumber];
      
      // Get the lock for this vector.
      IOTakeLock(vector->interruptLock);
      
      // Is it unregistered?
      if (!vector->interruptRegistered) break;
      
      // Move along to the next one.
      IOUnlock(vector->interruptLock);
    }
    
    if (vectorNumber != numVectors) break;
  }
  
  // Could not find a free one, so give up.
  if (vectorNumber == numVectors) {
    return kIOReturnNoResources;
  }
  
  // Create the vectorData for the IOInterruptSource.
  vectorData = OSData::withBytes(&vectorNumber, sizeof(vectorNumber));
  if (vectorData == 0) {
    return kIOReturnNoMemory;
  }
  
  // Fill in the IOInterruptSource with the controller's info.
  interruptSources[source].interruptController = this;
  interruptSources[source].vectorData = vectorData;
  
  // Fill in vector with the client's info.
  vector->handler = handler;
  vector->nub     = nub;
  vector->source  = source;
  vector->target  = target;
  vector->refCon  = refCon;
  
  // Get the vector ready.  It start soft disabled.
  vector->interruptDisabledSoft = 1;
  vector->interruptRegistered   = 1;
  
  vectorsRegistered++;
  
  IOUnlock(vector->interruptLock);
  return kIOReturnSuccess;
}

IOReturn IOSharedInterruptController::unregisterInterrupt(IOService *nub,
							  int source)
{
  IOInterruptSource *interruptSources;
  long              vectorNumber;
  IOInterruptVector *vector;
  OSData            *vectorData;
  
  interruptSources = nub->_interruptSources;
  vectorData = interruptSources[source].vectorData;
  vectorNumber = *(long *)vectorData->getBytesNoCopy();
  vector = &vectors[vectorNumber];
  
  // Get the lock for this vector.
  IOTakeLock(vector->interruptLock);
  
  // Return success if it is not already registered
  if (vector->interruptRegistered) {
    IOUnlock(vector->interruptLock);
    return kIOReturnSuccess;
  }
  
  // Soft disable the source.
  disableInterrupt(nub, source);
  
  // Clear all the storage for the vector except for interruptLock.
  vector->interruptActive = 0;
  vector->interruptDisabledSoft = 0;
  vector->interruptDisabledHard = 0;
  vector->interruptRegistered = 0;
  vector->nub = 0;
  vector->source = 0;
  vector->handler = 0;
  vector->target = 0;
  vector->refCon = 0;
  
  vectorsRegistered--;
  
  IOUnlock(vector->interruptLock);
  return kIOReturnSuccess;
}

IOReturn IOSharedInterruptController::getInterruptType(IOService */*nub*/,
						       int /*source*/,
						       int *interruptType)
{
  return provider->getInterruptType(0, interruptType);
}

IOReturn IOSharedInterruptController::enableInterrupt(IOService *nub,
						      int source)
{
  IOInterruptSource *interruptSources;
  long              vectorNumber;
  IOInterruptVector *vector;
  OSData            *vectorData;
  
  interruptSources = nub->_interruptSources;
  vectorData = interruptSources[source].vectorData;
  vectorNumber = *(long *)vectorData->getBytesNoCopy();
  vector = &vectors[vectorNumber];
  
  if (vector->interruptDisabledSoft) {
    vector->interruptDisabledSoft = 0;
    
    vectorsEnabled++;
    
    if (controllerDisabled && (vectorsEnabled == vectorsRegistered)) {
      controllerDisabled = 0;
      provider->enableInterrupt(0);
    }
  }
  
  return kIOReturnSuccess;
}

IOReturn IOSharedInterruptController::disableInterrupt(IOService *nub,
						       int source)
{
  IOInterruptSource *interruptSources;
  long              vectorNumber;
  IOInterruptVector *vector;
  OSData            *vectorData;
  
  interruptSources = nub->_interruptSources;
  vectorData = interruptSources[source].vectorData;
  vectorNumber = *(long *)vectorData->getBytesNoCopy();
  vector = &vectors[vectorNumber];
  
  if (!vector->interruptDisabledSoft) {
    vector->interruptDisabledSoft = 1;
#ifdef __ppc__
    sync();
    isync();
#endif
    
    vectorsEnabled--;
  }
  
  if (!getPlatform()->atInterruptLevel()) {
    while (vector->interruptActive);
#ifdef __ppc__
    isync();
#endif
  }
  
  return kIOReturnSuccess;
}

IOInterruptAction IOSharedInterruptController::getInterruptHandlerAddress(void)
{
    return (IOInterruptAction)&IOSharedInterruptController::handleInterrupt;
}

IOReturn IOSharedInterruptController::handleInterrupt(void * /*refCon*/,
						      IOService * nub,
						      int /*source*/)
{
  long              vectorNumber;
  IOInterruptVector *vector;
  
  for (vectorNumber = 0; vectorNumber < numVectors; vectorNumber++) {
    vector = &vectors[vectorNumber];
    
    vector->interruptActive = 1;
#ifdef __ppc__
    sync();
    isync();
#endif
    if (!vector->interruptDisabledSoft) {
#ifdef __ppc__
      isync();
#endif
      
      // Call the handler if it exists.
      if (vector->interruptRegistered) {
	vector->handler(vector->target, vector->refCon,
			vector->nub, vector->source);
      }
    }
    
    vector->interruptActive = 0;
  }
  
  // if any fo the vectors are dissabled, then dissable this controller.
  if (vectorsEnabled != vectorsRegistered) {
    nub->disableInterrupt(0);
    controllerDisabled = 1;
  }
  
  return kIOReturnSuccess;
}
