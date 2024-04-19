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
 * 24 Nov  1998 suurballe  Created.
 */

#include <IOKit/IOService.h>

#define kNoError        (0)
#define kParameterError (-1)
#define kNoNVRAM        (-2)

class IONVRAMController: public IOService
{
    OSDeclareAbstractStructors(IONVRAMController)

public:

    bool start ( IOService * );
    virtual IOReturn openNVRAM ( void );
    virtual IOReturn closeNVRAM ( void );
    virtual IOReturn syncNVRAM ( void );
    virtual IOReturn readNVRAM ( UInt32 Offset, IOByteCount * Length, UInt8 * Buffer ) = 0;
    virtual IOReturn writeNVRAM ( UInt32 Offset, IOByteCount * Length, UInt8 * Buffer ) = 0;
};

