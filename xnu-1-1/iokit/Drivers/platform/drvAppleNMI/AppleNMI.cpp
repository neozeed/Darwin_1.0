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
 * Copyright (c) 1998-9 Apple Computer, Inc.  All rights reserved.
 *
 *  DRI: Josh de Cesare
 *
 */

#include <IOKit/IOTypes.h>
#include <IOKit/IOLib.h>

extern "C" {
#include <pexpert/pexpert.h>
}

#include "AppleNMI.h"


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define super IOService

OSDefineMetaClassAndStructors(AppleNMI, IOService);

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool AppleNMI::start(IOService *provider)
{
  if (!super::init()) return false;
  
  // Register the interrupt.
    provider->registerInterrupt(0, this,
                            (IOInterruptAction) &AppleNMI::handleInterrupt, 0);
  provider->enableInterrupt(0);
  
  return true;
}

IOReturn AppleNMI::initNMI(IOInterruptController *parentController,
			   OSData *parentSource)
{
  // Allocate the IOInterruptSource so this can act like a nub.
  _interruptSources = (IOInterruptSource *)IOMalloc(sizeof(IOInterruptSource));
  if (_interruptSources == 0) return kIOReturnNoMemory;
  _numInterruptSources = 1;
  
  // Set up the IOInterruptSource to point at this.
  _interruptSources[0].interruptController = parentController;
  _interruptSources[0].vectorData = parentSource;
  
  // call start using itself as its provider.
  if (!start(this)) return kIOReturnError;
  
  return kIOReturnSuccess;
}

IOReturn AppleNMI::handleInterrupt(void * /*refCon*/, IOService * /*nub*/,
				   int /*source*/)
{
  PE_enter_debugger("NMI");
  
  return kIOReturnSuccess;
}
