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

#include <ppc/machine_routines.h>

#include <IOKit/IODeviceTreeSupport.h>

#include "Core99.h"

static unsigned long core99Speed[] = { 0, 1 };

#include <IOKit/pwr_mgt/RootDomain.h>
#include "IOPMSlots99.h"
#include "IOPMUSB99.h"

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define super ApplePlatformExpert

OSDefineMetaClassAndStructors(Core99PE, ApplePlatformExpert);

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool Core99PE::start(IOService *provider)
{
  OSData          *tmpData;
  IORegistryEntry *uniNRegEntry;
  
  setFamily(4); // Core99...
  
  // Set the machine type.
  if (IODTMatchNubWithKeys(provider, "'PowerMac2,1'"))
    machineType = kCore99TypeKihei;
  else if (IODTMatchNubWithKeys(provider, "'PowerMac3,1'"))
    machineType = kCore99TypeSawtooth;
  else if (IODTMatchNubWithKeys(provider, "'PowerBook2,1'"))
    machineType = kCore99TypeP1;
  else if (IODTMatchNubWithKeys(provider, "'PowerBook3,1'"))
    machineType = kCore99TypePismo;
  
  // Find out if this an all in one.
  allInOne = 0;
  switch (machineType) {
  case kCore99TypeKihei :
    allInOne = 1;
    break;
  }
  
  // Get the bus speed from the Device Tree.
  tmpData = OSDynamicCast(OSData, provider->getProperty("clock-frequency"));
  if (tmpData == 0) return false;
  core99Speed[0] = *(unsigned long *)tmpData->getBytesNoCopy();
  
  // Get a memory mapping for Uni-N's registers.
  uniNRegEntry = provider->childFromPath("uni-n", gIODTPlane);
  if (uniNRegEntry == 0) return false;
  tmpData = OSDynamicCast(OSData, uniNRegEntry->getProperty("reg"));
  if (tmpData == 0) return false;
  uniNBaseAddress = ((unsigned long *)tmpData->getBytesNoCopy())[0];
  uniNBaseAddress = ml_io_map(uniNBaseAddress, 0x1000);
  if (uniNBaseAddress == 0) return false;
  
  return super::start(provider);
}


void Core99PE::getDefaultBusSpeeds(int *numSpeeds,
				   unsigned long **speedList)
{
  if ((numSpeeds == 0) || (speedList == 0)) return;
  
  *numSpeeds = 1;
  *speedList = core99Speed;
}


bool Core99PE::platformAdjustService(IOService *service)
{
  
  if (!strcmp(service->getName(), "pmu")) {
    // Change the interrupt mapping for pmu source 4.
    OSArray              *tmpArray;
    OSCollectionIterator *extIntList;
    IORegistryEntry      *extInt;
    OSObject             *extIntControllerName;
    OSObject             *extIntControllerData;
    
    // Set the no-nvram property.
    service->setProperty("no-nvram", service);
    
    // Set the no-adb property if not on P1.
    if ((machineType != kCore99TypeP1) &&
	(machineType != kCore99TypePismo)) {
      service->setProperty("no-adb", service);
    }
    
    // Find the new interrupt information.
    extIntList = IODTFindMatchingEntries(getProvider(), kIODTRecursive,
					 "'extint-gpio1'");
    extInt = (IORegistryEntry *)extIntList->getNextObject();
    
    tmpArray = (OSArray *)extInt->getProperty(gIOInterruptControllersKey);
    extIntControllerName = tmpArray->getObject(0);
    tmpArray = (OSArray *)extInt->getProperty(gIOInterruptSpecifiersKey);
    extIntControllerData = tmpArray->getObject(0);
    
    // Replace the interrupt infomation for pmu source 4.
    tmpArray = (OSArray *)service->getProperty(gIOInterruptControllersKey);
    tmpArray->replaceObject(4, extIntControllerName);
    tmpArray = (OSArray *)service->getProperty(gIOInterruptSpecifiersKey);
    tmpArray->replaceObject(4, extIntControllerData);
    
    extIntList->release();
  }

  // Publish out the dual display heads on Pismo.
  if (machineType == kCore99TypePismo) {
    if (!strcmp(service->getName(), "ATY,RageM3pParent")) {
      if (kIOReturnSuccess == IONDRVLibrariesInitialize(service)) {
        createNubs(this, service->getChildIterator( gIODTPlane ));
      }
      return true;
    }
  }
  
  return true;
}


unsigned long Core99PE::readUniNReg(unsigned long offest)
{
  return uniNBaseAddress[offest / 4];
}

void Core99PE::writeUniNReg(unsigned long offest, unsigned long data)
{
  uniNBaseAddress[offest / 4] = data;
}


//*********************************************************************************
// PMInstantiatePowerDomains
//
// This overrides the vanilla implementation in IOPlatformExpert.  It instantiates
// a root domain with two children, one for the USB bus (to handle the USB idle
// power budget), and one for the expansions slots on the PCI bus (to handle
// the idle PCI power budget)
//*********************************************************************************

void Core99PE::PMInstantiatePowerDomains ( void )
{    
   root = new IOPMrootDomain;
   root->init();
   root->attach(this);
   root->start(this);
   root->youAreRoot();

   slot_domain = new IOPMSlots99;
   slot_domain->init();
   slot_domain->attach(this);
   slot_domain->start(this);
   root->addChild(slot_domain);

   usb_domain = new IOPMUSB99;
   usb_domain->init();
   usb_domain->attach(this);
   usb_domain->start(this);
   root->addChild(usb_domain);
}


//*********************************************************************************
// PMRegisterDevice
//
// This overrides the vanilla implementation in IOPlatformExpert.  When a device
// registers and has a "AAPL,slot-name" property, it is added to the hierarchy
// as a child of the IOPMSlots99 power domain.
// If it has a "name" property equal to "usb", it is added to the hierarchy
// as a child of the IOPMUSB99 power domain.
// Any other callers are added as children of the root.
//*********************************************************************************

void Core99PE::PMRegisterDevice(IOService * theNub, IOService * theDevice)
{
    OSObject * anObject;
    OSData * aString;

    aString = (OSData *)theNub ->getProperty("name");
    if ( aString != NULL ) {
        if ( strcmp(aString->getBytesNoCopy(),"usb") == 0 ) {
            usb_domain->addChild ( theDevice );
            return;
        }
    }
    anObject = theNub ->getProperty("AAPL,slot-name");
    if ( anObject != NULL ) {
        slot_domain->addChild ( theDevice );
        return;
    }
   root->addChild ( theDevice );
}
