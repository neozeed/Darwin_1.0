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


#ifndef _IOKIT_CORE99_H
#define _IOKIT_CORE99_H

#include "../drvApplePlatformExpert/ApplePlatformExpert.h"

#define kCore99TypeKihei      (1)
#define kCore99TypeSawtooth   (2)
#define kCore99TypeP1         (3)
#define kCore99TypePismo      (4)

class IOPMSlots99;
class IOPMUSB99;


class Core99PE : public ApplePlatformExpert
{
  OSDeclareDefaultStructors(Core99PE);
  
private:
  int 		machineType;
  int 		allInOne;
  unsigned long *uniNBaseAddress;
  IOPMSlots99   *slot_domain;
  IOPMUSB99     *usb_domain;

  void PMInstantiatePowerDomains ( void );
  void PMRegisterDevice(IOService * theNub, IOService * theDevice);

public:
  virtual bool start(IOService *provider);
  
  virtual void getDefaultBusSpeeds(int *numSpeeds,
				   unsigned long **speedList);
  
  virtual bool platformAdjustService(IOService *service);
  
  virtual unsigned long readUniNReg(unsigned long offest);
  virtual void writeUniNReg(unsigned long offest, unsigned long data);
};


#endif /* ! _IOKIT_CORE99_H */
