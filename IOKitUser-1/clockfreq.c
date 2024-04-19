/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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
cc clockfreq.c -o clockfreq -Wno-four-char-constants -framwork IOKit
*/

#include <ctype.h>
#include <stdlib.h>
#include <assert.h>
#include <limits.h>


#include <IOKit/IOKitLib.h>


mach_port_t	masterPort;


void printInt32( io_registry_entry_t entry, char * name )
{
    unsigned int	value;
    unsigned int	size;

    printf("\"%s\" ", name);

    size = sizeof( value );
    if( (KERN_SUCCESS == IORegistryEntryGetProperty( entry, name,
		(char *)&value, &size ))
     && (size == sizeof( value )))

        printf("= %d = %08x\n", value, value );

    else
	printf("not found\n");
}

void getClockFrequency( void )
{
    kern_return_t	kr;
    unsigned int	size;
    io_registry_entry_t	root;
    io_registry_entry_t	cpus;
    io_registry_entry_t	cpu;
    io_iterator_t	iter;
    io_name_t		name;

    assert( (
    root = IORegistryEntryFromPath( masterPort,
		kIODeviceTreePlane ":/" )
    ));
    printf("motherboard\n");
    printf("-----------\n");
    printInt32( root, "clock-frequency");

    // go looking for a cpu

    if( (cpus = IORegistryEntryFromPath( masterPort,
		kIODeviceTreePlane ":/cpus" ))) {
        assert( KERN_SUCCESS == (
	kr = IORegistryEntryGetChildIterator( cpus, kIODeviceTreePlane, &iter )
        ));
        IOObjectRelease( cpus );
    } else {
        assert( KERN_SUCCESS == (
	kr = IORegistryEntryGetChildIterator( root, kIODeviceTreePlane, &iter )
        ));
    }

    while( (cpu = IOIteratorNext( iter ))) {
        size = sizeof( name );
	if( (KERN_SUCCESS == IORegistryEntryGetProperty( cpu, "device_type",
		name, &size ))
	 && (0 == strcmp("cpu", name))) {

            printf("\nprocessor ");
            if( KERN_SUCCESS == IORegistryEntryGetName( cpu, name))
		printf(name);
            printf("\n---------------------\n");

	    printInt32(cpu, "clock-frequency");
	    printInt32(cpu, "bus-frequency");
	    printInt32(cpu, "timebase-frequency");
	    printInt32(cpu, "d-cache-size");
	    printInt32(cpu, "i-cache-size");
	    printInt32(cpu, "cpu-version");
	}
	IOObjectRelease(cpu);
    }

    IOObjectRelease( root );
}

int
main(int argc, char **argv)
{
	kern_return_t		kr;

	/*
	 * Get master device port
	 */
	assert( KERN_SUCCESS == (
	kr = IOMasterPort(   bootstrap_port,
			     &masterPort)
	));

	getClockFrequency();
}
