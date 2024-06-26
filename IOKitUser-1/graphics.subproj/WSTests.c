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
cc Tests.c -o tests -lIOKit -Wno-four-char-constants -fno-rtti -fno-exceptions -fcheck-new -fvtable-thunks
*/

#include <ctype.h>
#include <stdlib.h>
#include <assert.h>
#include <limits.h>

#include <mach/mach_interface.h>
#include <mach/vm_region.h>
#include <mach/thread_switch.h>


/*
 */

#define OLDCONNECT	0
#define TRAP 1

#include <mach/clock_types.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/hidsystem/IOHIDShared.h>
//#include <dev/evio.h>
#include <IOKit/graphics/IOFramebufferShared.h>

#include <IOKit/graphics/IOGraphicsEngine.h>

EvGlobals * 	evg;
StdFBShmem_t *  fbshmem;
mach_port_t	masterPort;
unsigned int	clock_freq;

/*
 */

inline UInt64 CFReadTSR() {
    union {
	UInt64 time64;
	UInt32 word[2];
    } now;

    /* Read from PowerPC 64-bit time base register. The increment */
    /* rate of the time base is implementation-dependent, but is */
    /* 1/4th the bus clock cycle on 603/604/750 processors. */
    UInt32 t3;
    do {
	__asm__ volatile("mftbu %0" : "=r" (now.word[0]));
	__asm__ volatile("mftb %0" : "=r" (now.word[1]));
	__asm__ volatile("mftbu %0" : "=r" (t3));
    } while (now.word[0] != t3);

    return now.time64;
}

void getClockFrequency( void )
{
    kern_return_t	kr;
    unsigned int	size;
    io_registry_entry_t	root;

    assert( (
    root = IORegistryEntryFromPath( masterPort, "IODeviceTree:/" )
    ));

    size = sizeof( clock_freq );
    assert( KERN_SUCCESS == (
    kr = IORegistryEntryGetProperty( root, "clock-frequency", (char *)&clock_freq, &size )
    ));
    assert( size == sizeof( clock_freq ));

    printf("clock-frequency = %d\n", clock_freq);

    IOObjectRelease( root );
}

void printElapsed( UInt64 start, UInt64 end )
{
#define numer 1000
#define divisor 1000
    printf("elapsed %d us\n", (int)((end - start) / (clock_freq / divisor / 4 ) * numer ));
}

void AccelTests( int reset, int count )
{
	io_iterator_t	iter;
	io_service_t 	fb;
	io_name_t	name;
	kern_return_t	kr;
	void *		blitterRef;
	int		quality, i;
	int		color, x, y;
	thread_port_t	tself;
        volatile UInt64	start, end;
	int		waited, busy;
	boolean_t	didit;

UInt32 * vars;
IOGraphicsEngineContext * shmem;

	if( reset) {
	}
	if( !count)
		count = 10000;

	assert( KERN_SUCCESS == (
	kr = IOServiceGetMatchingServices( masterPort,
			 IOServiceMatching( IOFRAMEBUFFER_CONFORMSTO ), &iter)
	));

	assert(
	fb = IOIteratorNext( iter )
	);

	assert( KERN_SUCCESS == (
	kr = IORegistryEntryGetName( fb, name )
	));

	assert( KERN_SUCCESS == (
	kr = IOPSAllocateBlitEngine( fb, &blitterRef, &quality )
	));

#if 0
vars = (UInt32 *) blitter;
shmem = (IOGraphicsEngineContext *) vars[ 3 ];
srandom( shmem->reserved[0] );

	tself = random();
	x = tself % 600;
	tself = random();
	y = tself % 500;
        shmem->reserved[0] = tself;

	waited = busy = 0;

        start = CFReadTSR();
	for( i = 0; i < count; i++ ) {
//	    IOPSBlitFill( blitter, x, y, 200, 200, color );
	    IOPSBlitInvert( blitter, x, y, 100, 1 );
	}
	IOPSBlitIdle( blitter );

        end = CFReadTSR();

//        thread_switch( 0, SWITCH_OPTION_WAIT, 2 * 1000 );

	printf("%s: Count: %d, ", name, count );
	printElapsed( start, end );
#endif

    if( 1 ) {

	void *	burstRef;
	int	count;
	int 	i,j = 0, lines;
	UInt64 	start,end;
	int 	w = 1280;
	int 	h = 100;
	int	bytesPerPixel = 2;
	int	slice = 1;
	int 	NUMBLITS = 300;
	void * 	fill1;
	void * 	fill2;

	count = ((slice * h * bytesPerPixel) + 31) / 32;
	fill1 = (void *)0x3c0001e0;
	fill2 = (void *)0x3c0001e0;

	start = CFReadTSR();

	for( i=0; i < NUMBLITS; i++) {
	    for( j=0; j < w; j += slice) {
		assert( KERN_SUCCESS == (
    		    kr = IOFBSetupFIFOBurst( blitterRef, j, 200, slice, h, 0,
						&burstRef )
		));
		lines = count;
		while( lines--)
		    IOFBBurstWrite32( fill1, fill2, fill1, fill2,
			  	  	fill1, fill2, fill1, fill2 );
	   }
	}

	end = CFReadTSR();

	{
	double bytes = w * h * 2 * NUMBLITS;
	double secs = (end - start);

	secs /= 24932500;

	printf("%d x %d * %d = %qd ticks\n", w, h, NUMBLITS, end - start);
	printf("%f bytes %f secs, %f Mbytes per sec\n", bytes, secs,
			bytes / secs / 1024 / 1024);
	}
    }

	IOObjectRelease( iter );
	IOObjectRelease( fb );
}


void SetCursor( StdFBShmem_t * fbshmem);
void SetCursor( StdFBShmem_t * fbshmem)
{
        UInt32 cursor[] = {
            0x00004000,0x60007000,0x78007C00,0x7E007F00,
            0x7F807C00,0x6C004600,0x06000300,0x03000000,
            0xC000E000,0xF000F800,0xFC00FE00,0xFF00FF80,
            0xFFC0FFE0,0xFE00EF00,0xCF008780,0x07800380,
            0x00080001,
        };
        UInt32 * 	dataPtr;
        UInt32   	data, mask;
        volatile UInt8 *  dataOut;
	int	 	i;

        dataPtr = cursor;
        dataOut = fbshmem->cursor.bw8.image[0];
        for( i = 0; i < 8; i++) {
            data = *dataPtr++;
            for( mask = 0x80000000; mask; mask >>= 1)
                *dataOut++ = (data & mask) ? 0xff : 0x00;
        }
        dataOut = fbshmem->cursor.bw8.mask[0];
        for( i = 0; i < 8; i++) {
            data = *dataPtr++;
            for( mask = 0x80000000; mask; mask >>= 1)
                *dataOut++ = (data & mask) ? 0xff : 0x00;
        }
        data = *dataPtr++;
        fbshmem->hotSpot[0].x = data >> 16;
        fbshmem->hotSpot[0].y = data & 0xffff;

	fbshmem->cursorShow = 1;
}



void SetupFBandHID( void )
{
	register kern_return_t	kr;
	mach_port_t		ev, fb, enumer, iter;
	io_name_t		name;
	vm_address_t		shmem, vram;
	vm_size_t		shmemSize;
	int			i,j;
	IODisplayModeID		displayMode;
        IOIndex			displayDepth;
	IOFramebufferInformation fbInfo;
	IOPixelEncoding		format;

#if OLDCONNECT

	assert( KERN_SUCCESS == (
	kr = IORegistryCreateEnumerator( masterPort,
			 &enumer)
	));

	assert( KERN_SUCCESS == (
	kr = IORegistryEnumeratorNextConforming( enumer,
			 (char *) IOFRAMEBUFFER_CONFORMSTO, TRUE)
	));

	assert( KERN_SUCCESS == (
	kr = IOOpenConnection( enumer,
			mach_task_self(),
			kIOFBServerConnectType,
			&fb)
	));

#else
	assert( KERN_SUCCESS == (
	kr = IOServiceGetMatchingServices( masterPort,
			 IOServiceMatching( IOFRAMEBUFFER_CONFORMSTO ), &iter)
	));

	assert(
	enumer = IOIteratorNext( iter )
	);

	assert( KERN_SUCCESS == (
	kr = IORegistryEntryGetName( enumer, name )
	));

	printf("Opening %s\n", name);

	assert( KERN_SUCCESS == (
            kr = IOServiceOpen( enumer,
			mach_task_self(),
			kIOFBServerConnectType,
			&fb)

	));

	IOObjectRelease( enumer );
	enumer = 0;
	assert( KERN_SUCCESS == (
	kr = IOConnectGetService( fb, &enumer )
	));

	assert( KERN_SUCCESS == (
	kr = IORegistryEntryGetName( enumer, name )
	));

	printf("Opened %s\n", name);

	IOObjectRelease( iter );
	IOObjectRelease( enumer );
#endif

	assert( KERN_SUCCESS == (
	kr = IOFBCreateSharedCursor( fb,
			kIOFBCurrentShmemVersion, 32, 32)
	));
//exit(0);

	assert( KERN_SUCCESS == (
	kr = IOFBGetFramebufferInformationForAperture( fb, 0, &fbInfo)
	));

	assert( KERN_SUCCESS == (
	kr = IOFBGetCurrentDisplayModeAndDepth( fb,
			&displayMode, &displayDepth)
	));

	assert( KERN_SUCCESS == (
	kr = IOFBGetPixelFormat( fb, displayMode, displayDepth,
		kIOFBSystemAperture, &format)
	));

	assert( KERN_SUCCESS == (
	kr = IOMapMemory( fb, kIOFBCursorMemory, mach_task_self(),
			&shmem, &shmemSize, TRUE)
	));

	fbshmem = (StdFBShmem_t *)shmem;

//	assert( sizeof( StdFBShmem_t) == fbshmem->structSize );
	SetCursor( fbshmem);

	assert( KERN_SUCCESS == (
	kr = IOConnectMapMemory( fb, kIOFBSystemAperture, mach_task_self(),
		&vram, &shmemSize, kIOMapDefaultCache | kIOMapAnywhere)
	));

	assert( KERN_SUCCESS == (
	kr = IOConnectMapMemory( fb, kIOFBSystemAperture, mach_task_self(),
		&vram, &shmemSize, kIOMapCopybackCache | kIOMapAnywhere)
	));

	if( 0 ) {
            printf("%ld x %ld", fbInfo.activeWidth, fbInfo.activeHeight);
            printf(", \"%s\"", format);
            printf(", mode %ld-%ld", displayMode, displayDepth);
            printf(", fbshmem mapped @ %x", fbshmem);
            printf(", vram mapped @ %x", vram);
            printf(", hidshmem mapped @ %x\n", kr, evg );
	}

        if( 1 ) {
            unsigned char * bits = (unsigned char *) vram;

            bits += (fbInfo.activeWidth / 2)
                    + fbInfo.bytesPerRow * (fbInfo.activeHeight / 2);
            for( i=0; i < 128; i++) {
                for( j=0; j < 128; j++)
                    bits[j] = ((i << 1) & 0xf0) | (j >> 3);
                bits += fbInfo.bytesPerRow;
            }
        }

	if( 0 ) {
            unsigned char data[ 768 ];
            for( i=0; i<256; i++) {
                data[ i ] = 255 - i;
                data[ i+ 256 ] = 255 - i;
                data[ i+ 512 ] = 255 - i;
            }
            assert( KERN_SUCCESS == (
            kr = IOFBSetGamma( fb, 3, 256, 8, data )
            ));
	}

if(0)	{
	vm_address_t 			vs = vram;
	vm_size_t			vl;
	vm_region_basic_info_data_t	vminfo;
	mach_msg_type_number_t		len = VM_REGION_BASIC_INFO_COUNT;
	mach_port_t			obj;

	kr = vm_region( mach_task_self(),
		&vs, &vl,
		VM_REGION_BASIC_INFO,
		(vm_region_info_t) &vminfo,
		&len,
		&obj );

	printf("vm_region = %lx, start: %08x, len: %08x\n", kr, vs, vl );
	}

#if OLDCONNECT
	assert( KERN_SUCCESS == (
	kr = IORegistryEnumeratorReset( enumer)
	));

	assert( KERN_SUCCESS == (
	kr = IORegistryEnumeratorNextConforming( enumer,
			 (char *) kIOHIDSystemClass, TRUE)
	));

	assert( KERN_SUCCESS == (
	kr = IOOpenConnection( enumer,
			mach_task_self(),
			kIOFBServerConnectType,
			&ev)
	));
#else
	assert( KERN_SUCCESS == (
	kr = IOServiceGetMatchingServices( masterPort,
			 IOServiceMatching( kIOHIDSystemClass ), &iter)
	));

	assert(
	enumer = IOIteratorNext( iter )
	);

	assert( KERN_SUCCESS == (
	kr = IORegistryEntryGetName( enumer, name )
	));

	printf("Opening %s\n", name);

	assert( KERN_SUCCESS == (
        kr = IOServiceOpen( enumer,
			mach_task_self(),
			kIOHIDServerConnectType,
			&ev)
	));

	IOObjectRelease( iter );
#endif

	assert( KERN_SUCCESS == (
	kr = IOHIDCreateSharedMemory( ev, kIOHIDCurrentShmemVersion)
	));

	assert( KERN_SUCCESS == (
	kr = IOHIDSetEventsEnable( ev, TRUE)
	));

	assert( KERN_SUCCESS == (
	kr = IORegisterClient( ev, fb)
	));

	assert( KERN_SUCCESS == (
	kr = IOHIDSetCursorEnable( ev, TRUE)
	));

	assert( KERN_SUCCESS == (
	kr = IOMapMemory( ev, kIOHIDGlobalMemory, mach_task_self(),
			&shmem, &shmemSize, TRUE)
	));

	evg = (EvGlobals *)
		(shmem + ((EvOffsets *)shmem)->evGlobalsOffset);

	assert( sizeof( EvGlobals) == evg->structSize );

//        printf("=%d, IOSetNotificationPort\n", kr);
//        kr = IOSetNotificationPort( ev, kIOHIDEventNotification,
//		bootstrap_event_port);

	assert( KERN_SUCCESS == (
	kr = IORegistryDisposeEnumerator( enumer)
	));

	if( 1 ) {
            printf("%ld x %ld", fbInfo.activeWidth, fbInfo.activeHeight);
            printf(", \"%s\"", format);
            printf(", mode %ld-%ld", displayMode, displayDepth);
            printf(", fbshmem mapped @ %x", fbshmem);
            printf(", vram mapped @ %x", vram);
            printf(", hidshmem mapped @ %x\n", kr, evg );
	}
}

void dumpIter( io_iterator_t iter )
{
    kern_return_t	kr;
    io_object_t		obj;
    io_name_t		name;
    io_string_t		path;

    while( (obj = IOIteratorNext( iter))) {
        assert( KERN_SUCCESS == (
        kr = IORegistryEntryGetName( obj, name )
        ));
	printf("name:%s(%d)\n", name, obj);
        kr = IORegistryEntryGetPath( obj, "IOService", path );
	if( KERN_SUCCESS == kr)
	    // if the object is detached, getPath is expected to fail
            printf("path:%s\n", path);
//	IOObjectRelease( obj );
    }
}

void regTest( void )
{
    UInt64		start, end;
    kern_return_t	kr;
    io_registry_entry_t	entry, root;
    io_iterator_t	iter;
    io_name_t		name;

    assert( (
    root = IORegistryEntryFromPath( masterPort, kIODeviceTreePlane ":/" )
    ));

    assert( KERN_SUCCESS == (
    kr = IORegistryEntryGetChildIterator( root, kIODeviceTreePlane, &iter )
    ));

    printf("TOT: ");
    entry = root;
    do {
        assert( KERN_SUCCESS == (
        kr = IORegistryEntryGetName( entry, name )
        ));
	printf("%s, ", name);
	IOObjectRelease( entry );
    } while( (entry = IOIteratorNext( iter)));
    printf("\n");

    IOObjectRelease( iter );

    assert( KERN_SUCCESS == (
    kr = IORegistryCreateIterator( masterPort, kIOServicePlane, TRUE, &iter )
    ));

    printf("All service: ");
    start = CFReadTSR();
    dumpIter( iter );
    end = CFReadTSR();

    printElapsed( start, end );

    IOObjectRelease( iter );
}

int ofPathTest(char * arg)
{
    io_name_t     bsdName;
    kern_return_t status;

    // Convert an open firmware name into a BSD name.

    if (IOServiceOFPathToBSDName(masterPort,arg,bsdName) == KERN_SUCCESS)
        printf("found = %s\n", bsdName);
    else
        printf("found = <no match>\n");
}

int
main(int argc, char **argv)
{
	kern_return_t		kr;
	boolean_t		reset = 0;
	int			count = 0;

	/*
	 * Get master device port
	 */
	assert( KERN_SUCCESS == (
	kr = IOMasterPort(   bootstrap_port,
			     &masterPort)
	));

//	getClockFrequency();
//	regTest();
//	ofPathTest( argv[1] );

	if( argc > 1) {
            reset = (0 == strcmp("reset", argv[1]));
	    if( !reset)
		count = strtol( argv[1], 0, 10 );
	}

  	AccelTests( reset, count );

//	SetupFBandHID();

//	printf("Done, sleeping...\n"); thread_switch( 0, SWITCH_OPTION_WAIT, 10 * 1000 );
	printf("Exit\n");

}

