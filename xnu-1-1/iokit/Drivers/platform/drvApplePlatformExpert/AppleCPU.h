/*
 * Copyright (c) 1999 Apple Computer, Inc.  All rights reserved.
 *
 *  DRI: Josh de Cesare
 *
 */

#ifndef _IOKIT_APPLECPU_H
#define _IOKIT_APPLECPU_H

#include <IOKit/IOCPU.h>

class AppleCPU : public IOCPU
{
  OSDeclareDefaultStructors(AppleCPU);
  
private:
  IOCPUInterruptController *cpuIC;
  
public:
  virtual bool     start(IOService *provider);
  virtual void     initCPU(void);
  virtual OSSymbol *getCPUName(void);
};

#endif /* ! _IOKIT_APPLECPU_H */
