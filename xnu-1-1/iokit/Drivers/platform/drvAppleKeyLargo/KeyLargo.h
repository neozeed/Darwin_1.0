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

#ifndef _IOKIT_KEYLARGO_H
#define _IOKIT_KEYLARGO_H

#include <IOKit/platform/AppleMacIO.h>

#define kKeyLargoGTimerFreq      (18432000UL)
#define kKeyLargoCounterLoOffset (0x15038)
#define kKeyLargoCounterHiOffset (0x1503C)

class KeyLargo : public AppleMacIO
{
  OSDeclareDefaultStructors(KeyLargo);
  
private:
  IOLogicalAddress             keyLargoBaseAddress;
  
public:
  virtual bool      start(IOService *provider);
  
  virtual long long syncTimeBase(void);
  
  virtual UInt8     readRegUInt8(unsigned long offest);
  virtual void      writeRegUInt8(unsigned long offest, UInt8 data);
  virtual UInt32    readRegUInt32(unsigned long offest);
  virtual void      writeRegUInt32(unsigned long offest, UInt32 data);
};

#endif /* ! _IOKIT_KEYLARGO_H */
