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

#include "KeyLargo.h"

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define super AppleMacIO

OSDefineMetaClassAndStructors(KeyLargo, AppleMacIO);

bool KeyLargo::start(IOService *provider)
{
  // Call MacIO's start.
  if (!super::start(provider))
    return false;
  
  // Make nubs for the children.
  publishBelow(provider);
  
  // get the base address of KeyLargo.
  keyLargoBaseAddress = fMemory->getVirtualAddress();
  
  registerService();
    
  return true;
}


long long KeyLargo::syncTimeBase(void)
{
  long          cnt;
  unsigned long gtLow, gtHigh, gtHigh2;
  unsigned long tbLow, tbHigh;
  long long     tmp, diffTicks, ratioLow, ratioHigh;
  
  ratioLow = (24907667ULL << 32) / kKeyLargoGTimerFreq;
  ratioHigh = ratioLow >> 32;
  ratioLow &= 0xFFFFFFFFULL;
  
  // Save the old time base.
  diffTicks = ((long long)mftbu() << 32) | mftb();
  
  // Do the sync twice to make sure it is cached.
  for (cnt = 0; cnt < 2; cnt++) {
    // Read the Global Counter.
    do {
      gtHigh  = readRegUInt32(kKeyLargoCounterHiOffset);
      gtLow   = readRegUInt32(kKeyLargoCounterLoOffset);
      gtHigh2 = readRegUInt32(kKeyLargoCounterHiOffset);
    } while (gtHigh != gtHigh2);
    
    tmp = gtHigh * ratioLow + gtLow * ratioHigh +
      ((gtLow * ratioLow + 0x80000000UL) >> 32);
    tbHigh = gtHigh * ratioHigh + (tmp >> 32);
    tbLow = tmp & 0xFFFFFFFFULL;
    
    mttb(tbLow);
    mttbu(tbHigh);
  }
  
  diffTicks = (((long long)tbHigh << 32) | tbLow) - diffTicks;
  
  return diffTicks;
}

UInt8 KeyLargo::readRegUInt8(unsigned long offest)
{
  return *(UInt8 *)(keyLargoBaseAddress + offest);
}

void KeyLargo::writeRegUInt8(unsigned long offest, UInt8 data)
{
  *(UInt8 *)(keyLargoBaseAddress + offest) = data;
  eieio();
}

UInt32 KeyLargo::readRegUInt32(unsigned long offest)
{
  return lwbrx(keyLargoBaseAddress + offest);
}

void KeyLargo::writeRegUInt32(unsigned long offest, UInt32 data)
{
  stwbrx(data, keyLargoBaseAddress + offest);
  eieio();
}
