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
 *  16 Feb 1998 suurballe  Created.
 */

#include <IOKit/IOLib.h>
#include "AppleNVRAM.h"


#define super IONVRAMController
OSDefineMetaClassAndStructors(AppleNVRAM, IONVRAMController)


// **********************************************************************************
// start
//
// **********************************************************************************
bool AppleNVRAM::start ( IOService * nub )
{
    IOItemCount numRanges;
    IOMemoryMap * map;
        
    nvram_kind = NVRAM_NONE;
    nvram_data = NULL;
    nvram_port = NULL;

    numRanges = nub->getDeviceMemoryCount( );
    
    if ( numRanges == 1 ) {
        nvram_kind = NVRAM_IOMEM;
        if( 0 == (map = nub->mapDeviceMemoryWithIndex( 0 )) ) {
                 return false;
        }
        nvram_data = (unsigned char *)map->getVirtualAddress();
        return super::start( nub );
    }
    if ( numRanges == 2 ) {
        nvram_kind = NVRAM_PORT;
        if( 0 == (map = nub->mapDeviceMemoryWithIndex( 0 )) ) {
                 return false;
        }
        nvram_port = (unsigned char *)map->getVirtualAddress();
        if( 0 == (map = nub->mapDeviceMemoryWithIndex( 1 )) ) {
                 return false;
        }
        nvram_data = (unsigned char *)map->getVirtualAddress();
        return super::start( nub );
    }
    return false;
}



// **********************************************************************************
// openNVRAM
//
//
// **********************************************************************************
IOReturn AppleNVRAM::openNVRAM ( void )
{
    if (nvram_kind == NVRAM_NONE) {
        return (kNoNVRAM);
    }
    return kNoError;
}

// **********************************************************************************
// readNVRAM
//
// The NVRAM driver is calling to read part of the NVRAM. 
//
// **********************************************************************************
IOReturn AppleNVRAM::readNVRAM ( UInt32 Offset, IOByteCount * Length, UInt8 * Buffer )
{
IOByteCount	i;
UInt8 *		client_buffer = Buffer;
UInt32		our_offset = Offset;

if (nvram_kind == NVRAM_NONE) {
    return (kNoNVRAM);
}

if ( (Buffer == NULL) ||
         (*Length == 0) ||
         (*Length > 8192) ||
         (Offset > 8192) ||
         ((*Length + Offset) > 8192) ) {
        return kParameterError;
}

switch (nvram_kind) {
    case	NVRAM_IOMEM:
        for (i = 0; i < *Length; i++,our_offset++)  {
            *client_buffer++ = nvram_data[our_offset << 4];
            eieio();
        }
        break;

    case	NVRAM_PORT:
        for (i = 0; i < *Length; i++,our_offset++) {
            *nvram_port = our_offset >> 5;
            eieio();
            *client_buffer++ = nvram_data[(our_offset & 0x1f) << 4];
            eieio();
        }
        break;
}

return kNoError;
}


// **********************************************************************************
// writeNVRAM
//
// The NVRAM driver is calling to write part of the NVRAM.  We translate this into
// single-byte PMU commands and enqueue them to our command queue.
//
// **********************************************************************************
IOReturn AppleNVRAM::writeNVRAM ( UInt32 Offset, IOByteCount * Length, UInt8 * Buffer )
{
IOByteCount	i;
UInt32		our_offset = Offset;
UInt8 *		client_buffer = Buffer;

if (nvram_kind == NVRAM_NONE) {
    return (kNoNVRAM);
}

if ( (Buffer == NULL) ||
         (*Length == 0) ||
         (*Length > 8192) ||
         (Offset > 8192) ||
         ((*Length + Offset) > 8192) ) {
        return kParameterError;
}

switch (nvram_kind) {
    case	NVRAM_IOMEM:
        for (i = 0; i < *Length; i++,our_offset++)  {
            nvram_data[our_offset << 4] = *client_buffer++;
            eieio();
        }
        break;

    case	NVRAM_PORT:
        for (i = 0; i < *Length; i++, our_offset++) {
            *nvram_port = our_offset >> 5;
            eieio();
            nvram_data[(our_offset & 0x1f) << 4] = *client_buffer++;
            eieio();
        }
        break;
}
return kNoError;
}
