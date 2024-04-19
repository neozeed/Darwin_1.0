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
 * 16 Feb 1999 suurballe  Created.
 */

#include <IOKit/nvram/IONVRAMController.h>

enum {
        NVRAM_IOMEM,
        NVRAM_PORT,
        NVRAM_PMU,
        NVRAM_NONE
};

class AppleNVRAM : public IONVRAMController
{
    OSDeclareDefaultStructors(AppleNVRAM)

private:

    int nvram_kind;
    volatile unsigned char *nvram_data;
    volatile unsigned char *nvram_port;

public:

    bool start ( IOService * );
    IOReturn openNVRAM ( void );
    IOReturn readNVRAM ( UInt32 Offset, IOByteCount * Length, UInt8 * Buffer );
    IOReturn writeNVRAM ( UInt32 Offset, IOByteCount * Length, UInt8 * Buffer );
};
