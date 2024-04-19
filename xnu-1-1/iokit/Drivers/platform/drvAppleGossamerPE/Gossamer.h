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


#ifndef _IOKIT_GOSSAMER_H
#define _IOKIT_GOSSAMER_H

#include "../drvApplePlatformExpert/ApplePlatformExpert.h"

#define kGossamerTypeGossamer   (1)
#define kGossamerTypeSilk       (2)
#define kGossamerTypeWallstreet (3)
#define kGossamerTypeiMac       (4)
#define kGossamerTypeYosemite   (5)
#define kGossamerType101        (6)

#define kGossamerMachineIDReg   (0xFF000004)
#define kGossamerAllInOneMask   (0x00100000)

class GossamerPE : public ApplePlatformExpert
{
  OSDeclareDefaultStructors(GossamerPE);
  
private:
  int machineType;
  int allInOne;
  
public:
  virtual bool start(IOService *provider);
  
  virtual void getDefaultBusSpeeds(int *numSpeeds,
				   unsigned long **speedList);
  
  virtual bool platformAdjustService(IOService *service);
};


#endif /* ! _IOKIT_GOSSAMER_H */
