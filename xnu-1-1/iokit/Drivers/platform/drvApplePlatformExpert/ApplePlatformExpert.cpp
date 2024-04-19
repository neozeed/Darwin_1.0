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
 * HISTORY
 * 
 */

#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IORangeAllocator.h>
#include <IOKit/nvram/IONVRAMController.h>

#include "ApplePlatformExpert.h"


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define super IODTPlatformExpert

OSDefineMetaClass(ApplePlatformExpert, IODTPlatformExpert);
OSDefineAbstractStructors(ApplePlatformExpert, IODTPlatformExpert);

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool ApplePlatformExpert::start( IOService * provider )
{
  if( provider->getProperty( gIODTNWInterruptMappingKey))
    setEpoch(1); // new world interrupt mapping => new world, for now
  else
    setEpoch(0); // old world
  
  return super::start(provider);
}

bool ApplePlatformExpert::configure( IOService * provider )
{
  IORangeAllocator *	physicalRanges;

  if((physicalRanges = getPhysicalRangeAllocator())) {
    physicalRanges->allocateRange(0,0x80000000);		// RAM
    physicalRanges->allocateRange(0xff000000,0x01000000);	// ROM
  }
  return(super::configure(provider));
}

const char * ApplePlatformExpert::deleteList ( void )
{
    return( "('packages', 'psuedo-usb', 'psuedo-hid', 'multiboot', 'rtas')" );
}

const char * ApplePlatformExpert::excludeList( void )
{
    return( "('chosen', 'memory', 'openprom', 'AAPL,ROM', 'rom', 'options', 'aliases')");
}

IOReturn ApplePlatformExpert::getNVRAMPartitionOffset(
		IOItemCount partition, UInt32 * offset )
{
    if( (partition < kIOMaxNVRAMPartitions)
      && (nvramPartitions[ partition ] != 0xffffffff)) {

	*offset = nvramPartitions[ partition ];
	return( kIOReturnSuccess );
    } else
	return( kIOReturnBadArgument );
}

void ApplePlatformExpert::registerNVRAMController( IONVRAMController * nvram )
{
    UInt32	curOffset = 0;
    IOByteCount	len;
    char	buf[17];

    if( getEpoch()) {
	// Should this go in a newworld specific PE?
        // Look at the NVRAM partitons and find the right ones.

        nvramPartitions[ kIOOpenFirmwareNVRAMPartition ] = 0xffffffff;
        nvramPartitions[ kIOXPRAMNVRAMPartition ]	 = 0xffffffff;
        nvramPartitions[ kIONameRegistryNVRAMPartition ] = 0xffffffff;

        if( kIOReturnSuccess == nvram->openNVRAM()) {
            buf[16] = '\0';
            while (curOffset < 0x2000) {

		len = 16;
                nvram->readNVRAM( curOffset, &len, (UInt8 *) buf);

                if (strcmp(buf + 4, "common") == 0) {
                    nvramPartitions[ kIOOpenFirmwareNVRAMPartition ]
			= curOffset + 16;
                } else if (strcmp(buf + 4, "APL,MacOS75") == 0) {
                    nvramPartitions[ kIOXPRAMNVRAMPartition ]
			= curOffset + 16;
                    nvramPartitions[ kIONameRegistryNVRAMPartition ]
			= curOffset + 16 + 0x100;
                }
                curOffset += ((short *)buf)[1] * 16;
            }
            nvram->closeNVRAM();
        }
    } else {
        // Fixed offset for legacy machines.
        nvramPartitions[ kIOOpenFirmwareNVRAMPartition ] = 0x1800;
        nvramPartitions[ kIOXPRAMNVRAMPartition ] = 0x1300;
        nvramPartitions[ kIONameRegistryNVRAMPartition ] = 0x1400;
    }

    // Here we are saving off the time zone info that's in PRAM.
    // This probably should be a separeate call that the
    // ApplePlatformExpert does in it's initialization.  -ECH
#define kXPRAMTimeToGMTOffset 0xEC

    if( kIOReturnSuccess == nvram->openNVRAM()) {
        len = sizeof(_timeToGMT);
        nvram->readNVRAM( nvramPartitions[ kIOXPRAMNVRAMPartition ] + kXPRAMTimeToGMTOffset,
                          &len, (UInt8 *) &_timeToGMT);
        _timeToGMT &= 0x00FFFFFF; // convert from a SInt24
        _timeToGMT |= (_timeToGMT & 0x00800000) ? 0xFF000000 : 0x00000000; // or in the sign
	nvram->closeNVRAM();
    }

    super::registerNVRAMController( nvram );
}

#define	SECS_BETWEEN_1904_1970	2082844800

long ApplePlatformExpert::getGMTTimeOfDay(void)
{
   long localtime;

    if (PE_read_write_time_of_day(kPEReadTOD, &localtime) == 0)
        return (localtime - _timeToGMT - SECS_BETWEEN_1904_1970);

    return(0);
}

void ApplePlatformExpert::setGMTTimeOfDay(long secs)
{
    secs += SECS_BETWEEN_1904_1970;
    secs += _timeToGMT;
    PE_read_write_time_of_day(kPEWriteTOD, &secs);
}

bool ApplePlatformExpert::getMachineName(char *name, int maxLength)
{
  strncpy(name, "Power Macintosh", maxLength);
  
  return true;
}
