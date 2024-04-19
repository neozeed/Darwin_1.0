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

extern "C" {
#include <machine/machine_routines.h>
}

#include <IOKit/IODeviceTreeSupport.h>

#include "Gossamer.h"

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define super ApplePlatformExpert

OSDefineMetaClassAndStructors(GossamerPE, ApplePlatformExpert);

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool GossamerPE::start(IOService *provider)
{
  unsigned int         tmpVal;
  
  setFamily(2); // Gossamer...
  
  // Set the machine type.
  if (IODTMatchNubWithKeys(provider, "'AAPL,Gossamer'"))
    machineType = kGossamerTypeGossamer;
  else if (IODTMatchNubWithKeys(provider, "'AAPL,PowerMac G3'"))
    machineType = kGossamerTypeSilk;
  else if (IODTMatchNubWithKeys(provider, "'AAPL,PowerBook1998'"))
    machineType = kGossamerTypeWallstreet;
  else if (IODTMatchNubWithKeys(provider, "'iMac,1'"))
    machineType = kGossamerTypeiMac;
  else if (IODTMatchNubWithKeys(provider, "('PowerMac1,1', 'PowerMac1,2')"))
    machineType = kGossamerTypeYosemite;
  else if (IODTMatchNubWithKeys(provider, "'PowerBook1,1'"))
    machineType = kGossamerType101;
  
  // Find out if this an all in one.
  allInOne = 0;
  if (ml_probe_read(kGossamerMachineIDReg, &tmpVal)) {
    switch (machineType) {
    case kGossamerTypeGossamer :
    case kGossamerTypeSilk :
      if (!(tmpVal & kGossamerAllInOneMask)) allInOne = 1;
      break;
      
    case kGossamerTypeiMac :
      allInOne = 1;
      break;
    }
  }
  
  return super::start(provider);
}


static unsigned long gossamerSpeed[] = { 66820000, 1 };
static unsigned long yosemiteSpeed[] = { 99730000, 1 };

void GossamerPE::getDefaultBusSpeeds(int *numSpeeds,
				     unsigned long **speedList)
{
  if ((numSpeeds == 0) || (speedList == 0)) return;
  
  switch (machineType) {
  case kGossamerTypeGossamer :
  case kGossamerTypeSilk :
    *numSpeeds = 1;
    *speedList = gossamerSpeed;
    break;
    
  case kGossamerTypeYosemite :
    *numSpeeds = 1;
    *speedList = yosemiteSpeed;
    break;
    
  default :
    *numSpeeds = 0;
    *speedList = 0;
    break;
  }
}


bool GossamerPE::platformAdjustService(IOService *service)
{
  long            tmpNum;
  OSData          *tmpData;
  
  // Add the extra sound properties for Gossamer AIO
  if (allInOne && ((machineType == kGossamerTypeGossamer) ||
		   (machineType == kGossamerTypeSilk))) {
    if (!strcmp(service->getName(), "sound")) {
      tmpNum = 3;
      tmpData = OSData::withBytes(&tmpNum, sizeof(tmpNum));
      if (tmpData) {
	service->setProperty("#-detects", tmpData);
	service->setProperty("#-outputs", tmpData);
	tmpData->release();
      }
      return true;
    }
  }
  
  // Set the loop snoop property for Wallstreet or Mainstreet.
  if (machineType == kGossamerTypeWallstreet) {
    if (IODTMatchNubWithKeys(service, "('grackle', 'MOT,PPC106')")) {
      // Add the property for set loop snoop.
      service->setProperty("set-loop-snoop", service);
      return true;
    }
  }

  // Publish out the dual display heads on 101.
  if (machineType == kGossamerType101) {
    if (!strcmp(service->getName(), "ATY,LTProParent")) {
      if (kIOReturnSuccess == IONDRVLibrariesInitialize(service)) {
        createNubs(this, service->getChildIterator( gIODTPlane ));
      }
      return true;
    }
  }
  
  return true;
}
