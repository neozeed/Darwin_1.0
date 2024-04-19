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
#include <mach/mach.h>
#include <mach/thread_switch.h>
#include <string.h>
#include <stdlib.h>

#include <IOKit/IOKitLib.h>
#include <IOKit/graphics/IOGraphicsLib.h>
#include <IOKit/graphics/IOGraphicsEngine.h>
#include <IOKit/iokitmig.h>

#ifndef kIOFramebufferInfoKey
#define kIOFramebufferInfoKey	"IOFramebufferInfo"
#endif


/* --------------------------------------------------------- */

kern_return_t
IOFBCreateSharedCursor( mach_port_t connect,
	unsigned int version,
	unsigned int maxWidth, unsigned int maxHeight )
{
    kern_return_t	err;
    unsigned int	len;
    int			params[ 3 ];

    params[0] = version;
    params[1] = maxWidth;
    params[2] = maxHeight;
    len = 0;
    err = io_connect_method_scalarI_scalarO( connect, 0, /*index*/
                    params, 3, NULL, &len);

    return( err);
}

extern kern_return_t
IOFBGetFramebufferInformationForAperture( mach_port_t connect,
	    IOPixelAperture		  aperture,
	    IOFramebufferInformation	* info )
{
    IOPixelInformation	pixelInfo;
    IODisplayModeID	mode;
    IOIndex		depth;
    kern_return_t	err;

    err = IOFBGetCurrentDisplayModeAndDepth( connect, &mode, &depth );
    if( err)
	return( err);
    err = IOFBGetPixelInformation( connect, mode, depth, aperture, &pixelInfo );
    if( err)
	return( err);

    err = IOFBGetFramebufferOffsetForAperture( connect, aperture,
						&info->baseAddress);
    info->activeWidth	= pixelInfo.activeWidth;
    info->activeHeight	= pixelInfo.activeHeight;
    info->bytesPerRow	= pixelInfo.bytesPerRow;
    info->bytesPerPlane	= pixelInfo.bytesPerPlane;
    info->bitsPerPixel	= pixelInfo.bitsPerPixel;
    info->pixelType	= pixelInfo.pixelType;
    info->flags		= pixelInfo.flags;

    return( err);
}

extern kern_return_t
IOFBGetFramebufferOffsetForAperture( mach_port_t connect,
	    IOPixelAperture		  aperture,
	    IOByteCount			* offset )
{
    kern_return_t	err;
    unsigned int	len;

    len = 1;
    err = io_connect_method_scalarI_scalarO( connect, 8, /*index*/
                    (int *) &aperture, 1, (int *) offset, &len);

    return( err);
}

extern kern_return_t
IOFBSetBounds( mach_port_t connect,
	    IOGBounds	* rect )
{
    kern_return_t	err;
    unsigned int	len;

    len = 0;
    err = io_connect_method_structureI_structureO( connect, 9, /*index*/
                    (void *) rect, sizeof( *rect), 0, &len);

    return( err);
}

kern_return_t
IOFBGetCurrentDisplayModeAndDepth( mach_port_t connect,
	IODisplayModeID * displayMode,
	IOIndex 	* depth )
{
    kern_return_t	err;
    unsigned int	len;
    int			params[ 2 ];

    len = 2;
    err = io_connect_method_scalarI_scalarO( connect, 2, /*index*/
                    NULL, 0, params, &len);
    if( err == kIOReturnSuccess) {
        *displayMode = params[0];
        *depth = params[1];
    }
    return( err);
}

extern kern_return_t
IOFBGetPixelFormat( mach_port_t connect,
	IODisplayModeID mode,
	IOIndex 	depth,
        IOPixelAperture aperture,
	IOPixelEncoding * pixelFormat )
{
    IOPixelInformation	pixelInfo;
    kern_return_t	err;

    err = IOFBGetPixelInformation( connect, mode, depth, aperture, &pixelInfo );
    if( err)
	return( err);

    strncpy( *pixelFormat, pixelInfo.pixelFormat, kIOMaxPixelBits );

    return( err);
}

extern kern_return_t
IOFBSetCLUT( mach_port_t connect,
	UInt32		startIndex,
	UInt32		numEntries,
	IOOptionBits	options,
	IOColorEntry *	colors )
{
    kern_return_t	err;
    int			params[2];

    params[0] = startIndex;
    params[1] = options;
    err = io_connect_method_scalarI_structureI( connect, 16, /* index */
            params, 2, (char *)colors, numEntries * sizeof( IOColorEntry) );


    return( err);
}

extern kern_return_t
IOFBSetGamma( mach_port_t connect,
	UInt32		channelCount,
	UInt32		dataCount,
	UInt32		dataWidth,
	void *		data )
{
    kern_return_t	err;
    unsigned int	len;
    int			params[ 3 ];

    params[ 0 ]	= channelCount;
    params[ 1 ]	= dataCount;
    params[ 2 ]	= dataWidth;

    len = ((dataWidth + 7) / 8) * dataCount * channelCount;

    err = io_connect_method_scalarI_structureI( connect, 11, /* index */
            params, 3, data, len );

    return( err );
}

extern kern_return_t
IOFBAcknowledgePM( io_connect_t connect )
{
    kern_return_t	err;
    unsigned int	len;

    err = io_connect_method_scalarI_scalarO( connect, 14, /* index */
            NULL, 0, NULL,& len );

    return( err );
}

extern kern_return_t
IOFBSet444To555Table( io_connect_t connect,
	const unsigned char *	table )
{
    kern_return_t	err;
    int			params = 0;

    err = io_connect_method_scalarI_structureI( connect, 15, /* index */
            &params, 1, (char *)table, 16 * sizeof( UInt8) );

    return( err );
}

extern kern_return_t
IOFBSet555To444Table( io_connect_t connect,
	const unsigned char *	table )
{
    kern_return_t	err;
    int			params = 1;

    err = io_connect_method_scalarI_structureI( connect, 15, /* index */
            &params, 1, (char *)table, 32 * sizeof( UInt8) );

    return( err );
}

extern kern_return_t
IOFBSet256To888Table( io_connect_t connect,
	const unsigned int *	table )
{
    kern_return_t	err;
    int			params = 2;

    err = io_connect_method_scalarI_structureI( connect, 15, /* index */
            &params, 1, (char *)table, 256 * sizeof( UInt32) );

    return( err );
}

extern kern_return_t
IOFBSet888To256Table( io_connect_t connect,
	const unsigned char *	table )
{
    kern_return_t	err;
    int			params = 3;

    err = io_connect_method_scalarI_structureI( connect, 15, /* index */
            &params, 1, (char *)table, 5 * 256 * sizeof( UInt8) );

    return( err );
}

kern_return_t
IOFBGetDisplayModeCount( io_connect_t connect,
	UInt32 * count )
{
    IOReturn		err;
    unsigned int	len;

    len = 1;
    err = io_connect_method_scalarI_scalarO( connect, 6, /*index*/
                    NULL, 0, (int *)count, &len);
    return( err);
}

kern_return_t
IOFBGetDisplayModes( io_connect_t connect,
	UInt32			count,
	IODisplayModeID	*	allDisplayModes )
{
    IOReturn		err;
    unsigned int	len;

    len = count * sizeof( IODisplayModeID);
    err = io_connect_method_structureI_structureO( connect, 7, /*index*/
                    NULL, 0, (void *) allDisplayModes, &len);

    return( err);
}

// Info about a display mode

kern_return_t
IOFBGetDisplayModeInformation( io_connect_t connect,
	IODisplayModeID		displayMode,
	IODisplayModeInformation * info )
{
    kern_return_t	r;
    unsigned int	len;

    len = sizeof( IODisplayModeInformation);
    r = io_connect_method_scalarI_structureO( connect, 5, /*index*/
                    (int *) &displayMode, 1, (void *) info, &len);

    return( r);
}

// Mask of pixel formats available in mode and depth

kern_return_t
IOFBGetPixelFormats( io_connect_t connect,
	IODisplayModeID		displayMode,
	IOIndex			depth,
	UInt32 * 		mask )
{
    *mask = 1;
    return( kIOReturnSuccess);
}

kern_return_t
IOFBGetPixelInformation( io_connect_t connect,
	IODisplayModeID 	displayMode,
	IOIndex 		depth,
        IOPixelAperture		aperture,
	IOPixelInformation *	pixelInfo )
{
    kern_return_t	r;
    unsigned int	len;
    int			params[ 3 ];

    params[0] = displayMode;
    params[1] = depth;
    params[2] = aperture;

    len = sizeof( IOPixelInformation);
    r = io_connect_method_scalarI_structureO( connect, 1, /*index*/
                    params, 3, (void *) pixelInfo, &len);

    return( r);
}

kern_return_t
IOFBSetDisplayModeAndDepth( io_connect_t connect,
	IODisplayModeID		displayMode,
	IOIndex 		depth )
{
    kern_return_t	err;
    unsigned int	len;
    int			params[ 2 ];

    params[0] = displayMode;
    params[1] = depth;
    len = 0;
    err = io_connect_method_scalarI_scalarO( connect, 4, /*index*/
                    params, 2, NULL, &len);

    return( err);
}

kern_return_t
IOFBSetStartupDisplayModeAndDepth( io_connect_t connect,
	IODisplayModeID		displayMode,
	IOIndex 		depth )
{
    kern_return_t	err;
    unsigned int	len;
    int			params[ 2 ];

    params[0] = displayMode;
    params[1] = depth;
    len = 0;
    err = io_connect_method_scalarI_scalarO( connect, 3, /*index*/
                    params, 2, NULL, &len);

    return( err);
}

kern_return_t
IOFBSetNewCursor( io_connect_t connect,
	void *			cursor,
	IOIndex			frame,
	IOOptionBits		options )
{
    kern_return_t	err;
    unsigned int	len;
    int			params[ 3 ];

    params[0] = (unsigned int) cursor;
    params[1] = frame;
    params[2] = options;
    len = 0;

    err = io_connect_method_scalarI_scalarO( connect, 10, /*index*/
                    params, 3, NULL, &len);

    return( err );
}

kern_return_t
IOFBSetCursorVisible( io_connect_t connect,
	int			visible )
{
    kern_return_t	err;
    unsigned int	len;

    len = 0;
    err = io_connect_method_scalarI_scalarO( connect, 12, /*index*/
                    &visible, 1, NULL, &len);

    return( err );
}

kern_return_t
IOFBSetCursorPosition( io_connect_t connect,
	long int		x,
	long int		y )
{
    kern_return_t	err;
    unsigned int	len;
    int			params[ 2 ];

    params[0] = x;
    params[1] = y;

    len = 0;
    err = io_connect_method_scalarI_scalarO( connect, 13, /*index*/
                    params, 2, NULL, &len);

    return( err );
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

CFDictionaryRef
IOFBCreateDisplayModeDictionary( io_service_t framebuffer,
	IODisplayModeID		displayMode )
{
    CFDictionaryRef	regDict;
    CFDictionaryRef	infoDict;
    CFDictionaryRef	modeDict = 0;
    char		keyBuf[12];
    kern_return_t	kr;

    kr = IORegistryEntryCreateCFProperties( framebuffer,
			&regDict, kCFAllocatorDefault, kNilOptions );
    if( KERN_SUCCESS != kr)
	return( 0 );

    infoDict = CFDictionaryGetValue( regDict, kIOFramebufferInfoKey );
    if( infoDict ) {
        sprintf( keyBuf, "%lx", displayMode );
        modeDict = CFDictionaryGetValue( infoDict, keyBuf );
        if( modeDict)
            CFRetain( modeDict );
        CFRelease( infoDict );
    }
    CFRelease( regDict );

    return( modeDict );
}

CFDictionaryRef
IOFBGetPixelInfoDictionary( 
	CFDictionaryRef		modeDictionary,
	IOIndex 		depth,
	IOPixelAperture		aperture )
{
    char		keyBuf[12];
    CFDictionaryRef	pixelInfo;

    if( !modeDictionary)
	return( 0 );

    sprintf( keyBuf, "%lx", depth + (aperture << 16) );
    pixelInfo = CFDictionaryGetValue( modeDictionary, keyBuf );

    return( pixelInfo );
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOReturn
IOFBGetInterruptSemaphore( io_connect_t connect,
                           IOSelect interruptType,
                           semaphore_t * semaphore )
{
    kern_return_t	err;
    unsigned int	len;

    len = 1;
    err = io_connect_method_scalarI_scalarO( connect, 15, /*index*/
                    (int *)&interruptType, 1, (int *)semaphore, &len);

    return( err );
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include <IOKit/graphics/IOGraphicsInterface.h>

#ifndef NO_CFPLUGIN

struct _BlitterVars {
    IOGraphicsAcceleratorInterface ** interface;
    SInt32			lastCompletionToken;
    IOBlitProcPtr		copyProc;
    IOBlitProcPtr		fillProc;
    IOBlitProcPtr		memCopyProc;
};
typedef struct _BlitterVars _BlitterVars;

static inline UInt64 CFReadTSR() {
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

#if 0

static __inline__ void dcbf( void * addr, vm_size_t offset )
{
	__asm__("dcbf %0,%1" : : "r" (offset), "r" (addr));
}
static __inline__ void dcbz( void * addr, vm_size_t offset )
{
	__asm__("dcbz %0,%1" : : "r" (offset), "r" (addr));
}

static kern_return_t
__tests( void * blitterRef )
{
    _BlitterVars *	vars = (_BlitterVars *) blitterRef;
    volatile UInt32 *	mem;
    UInt32		data;
    UInt32		offset;
    kern_return_t	err;
    void *		memRef;
    int			i = 0, j = 0, k;
    SInt32		token1, token2;
    int 		w = 1280;
    int 		h = 1024;
    int 		NUMBLITS = 300;
    UInt64 		start,end;
    IOByteCount		buffSize = 1280 * 1024 * 2;

    vm_allocate( mach_task_self(), (vm_offset_t *) &mem,
			buffSize, VM_FLAGS_ANYWHERE );

    err = IOFBCommitMemory( blitterRef, mem, buffSize, 0, &memRef, &offset );
    if( err)
	return;

    for( i = 0; i < (512 / 4); i += 8) {

	if( 0 == (i & 15))
	    data = 0x7c007c;
	else
	    data = 0x1f001f00;

	dcbz( mem, i * 4 );
	    data = 0x007c1f00;
	mem[i+0] = data;
	    data = 0x1f001f00;
	mem[i+1] = data;
	mem[i+2] = data;
	mem[i+3] = data;
	mem[i+4] = data;
	mem[i+5] = data;
	mem[i+6] = data;
	mem[i+7] = data;
	dcbf( mem, i * 4 );
    }
    __asm__("sync");
    __asm__("isync");

start = CFReadTSR();
#define TILE 16
  for( k = 0; k < 300; k++) {
   for( i = 0; i < 1024; i += TILE)
   {
    for( j = 0; j < 1280; j += TILE)
    {
	err = IOFBMemoryCopy( blitterRef, j, i, TILE, TILE, offset, TILE * 2, &token2 );
//	err = IOFBMemoryCopy( blitterRef, j, i, 640, 1024, offset + 640, 1280 * 2, &token1 );
//    err = IOFBWaitForCompletion( blitterRef, token2 );
    }
   }
 }

//    err = IOFBMemoryCopyMono( blitterRef, 0, 0, 256, 128, offset, 32,
//				0x7c007c, 0x1f001f00, &token );

//    printf("IOFBMemoryCopy = %08x, %08x, %d\n", err, offset, memRef );

    err = IOFBWaitForCompletion( blitterRef, token2 );
end = CFReadTSR();
    printf("IOFBWaitForCompletion = %08x, %d\n", err, token2 );
{
double bytes = w * h * 2 * NUMBLITS;
double secs = (end - start);

//secs /= 24932500;
secs /= 24907667;

printf("%d x %d * %d = %qd ticks\n", w, h, NUMBLITS, end - start);
printf("%f bytes %f secs, %f Mbytes per sec\n", bytes, secs, bytes / secs / 1024 / 1024);
}

    err = IOFBReleaseMemory( blitterRef, memRef );
    printf("IOFBReleaseMemory = %08x\n", err );
sleep(5);
}

#endif
#if 0
static kern_return_t
__tests( void * blitterRef )
{
    _BlitterVars *	vars = (_BlitterVars *) blitterRef;

    kern_return_t	ret;
//    volatile UInt8 * 	io = vars->io;
    void *		ref;
    int			count;
    void *		fill1;
    void *		fill2;

    int w = 1280;
    int h = 1024;
    int NUMBLITS = 300;

    int i,j=0, lines;
    UInt64 start,end;

start = CFReadTSR();

    count = (((16 * h * 2) + 31) >> 5);

fill1 = (void *)0x3c0001e0;
fill2 = (void *)0x3c0001e0;

for( i=0; i < NUMBLITS; i++)
{
for( j=0; j < w; j+=16) 
{
#if 1
    ret = IOFBSetupFIFOBurst( blitterRef, j, 0, 16, h, 0, &ref );
    if( kIOReturnSuccess != ret)
	return( ret );
#else
    ref = (void *) vars->fifo;
#endif
    lines = count;
    while( lines--) {

	IOFBBurstWrite32( fill1, fill2, fill1, fill2,
			  fill1, fill2, fill1, fill2 );
//	ref = (void *)(((int)ref)+32);
    }
}
}
end = CFReadTSR();
{
double bytes = w * h * 2 * NUMBLITS;
double secs = (end - start);

//secs /= 24932500;
secs /= 24907667;

printf("%d x %d * %d = %qd ticks\n", w, h, NUMBLITS, end - start);
printf("%f bytes %f secs, %f Mbytes per sec\n", bytes, secs, bytes / secs / 1024 / 1024);
}
    return( kIOReturnSuccess );
}
#endif

extern void ATIMach64GARegister(void);
extern void ATIRage128PIOGARegister(void);

kern_return_t
IOPSAllocateBlitEngine( io_service_t service,
		void ** blitterRef, int * quality)
{
    IOReturn				err = kIOReturnSuccess;
    _BlitterVars *			vars;
    IOGraphicsAcceleratorInterface **	interface = 0;

    // -- builtins
    static int builtinsDone = 0;
    if( !builtinsDone) {
        ATIRage128PIOGARegister();
        ATIMach64GARegister();
        builtinsDone = 1;
    }
    // --
    
    vars = (_BlitterVars *) malloc( sizeof( _BlitterVars ));
    if( !vars)
	return( kIOReturnNoMemory);

    do {
        err = IOCreatePlugInInterfaceForService( service,
                            kIOGraphicsAcceleratorTypeID,
                            kIOGraphicsAcceleratorInterfaceID,
                            (IOCFPlugInInterface ***)&interface, (SInt32 *) quality );
        if( err)
	    continue;
        vars->interface = interface;
        err = (*interface)->GetBlitProc(interface,
                                    kIOBlitAllOptions,
                                    (kIOBlitTypeCopyRects | kIOBlitCopyOperation),
                                    (kIOBlitSourceFramebuffer | kIOBlitDestFramebuffer),
                                    &vars->copyProc);
        if( err)
            continue;
        err = (*interface)->GetBlitProc(interface,
                                    kIOBlitAllOptions,
                                    (kIOBlitTypeRects | kIOBlitCopyOperation),
                                    (kIOBlitSourceSolid | kIOBlitDestFramebuffer),
                                    &vars->fillProc);
        if( err)
            continue;
        if( kIOReturnSuccess != (*interface)->GetBlitProc(interface,
                                    kIOBlitAllOptions,
                                    (kIOBlitTypeCopyRects | kIOBlitCopyOperation),
                                    (kIOBlitSourceMemory | kIOBlitDestFramebuffer),
                                    &vars->memCopyProc))
            vars->memCopyProc = 0;
        
    } while( FALSE );

    if( err) {
        if (interface)
            IODestroyPlugInInterface((IOCFPlugInInterface **)interface);
        free( vars );
        vars = 0;
    }
    
    *blitterRef = (void *) vars;

//__tests( *blitterRef );

    return( err);
}

kern_return_t
IOPSBlitReset( void * blitterRef)
{
    _BlitterVars *	vars = (_BlitterVars *) blitterRef;
    IOGraphicsAcceleratorInterface ** interface = vars->interface;
    kern_return_t	err;

    err = (*interface)->Reset(interface, kNilOptions);

    return( err );
}

kern_return_t
IOPSBlitDeallocate( void * blitterRef)
{
    _BlitterVars *	vars = (_BlitterVars *) blitterRef;
    IOGraphicsAcceleratorInterface ** interface = vars->interface;
    kern_return_t	err;

    err = IODestroyPlugInInterface((IOCFPlugInInterface **)interface);
    free( vars );

    return( err );
}

kern_return_t
IOPSBlitIdle( void * blitterRef)
{
    _BlitterVars *	vars = (_BlitterVars *) blitterRef;
    IOGraphicsAcceleratorInterface ** interface = vars->interface;
    kern_return_t	err;

    err = (*interface)->WaitForCompletion(interface, kIOBlitWaitGlobal,
                                          vars->lastCompletionToken );

    return( err );
}

kern_return_t
IOFBWaitForCompletion( void * blitterRef, SInt32 token )
{
    _BlitterVars *	vars = (_BlitterVars *) blitterRef;
    IOGraphicsAcceleratorInterface ** interface = vars->interface;
    kern_return_t	err;

    err = (*interface)->WaitForCompletion(interface, kNilOptions, token );

    return( err );
}

kern_return_t
IOFBSynchronize( void * blitterRef,
                UInt32 x, UInt32 y, UInt32 w, UInt32 h, UInt32 options )
{
    _BlitterVars *	vars = (_BlitterVars *) blitterRef;
    IOGraphicsAcceleratorInterface ** interface;
    IOReturn		err;

    if( !vars)
        return( kIOReturnBadArgument);
    interface = vars->interface;
    err = (*interface)->Synchronize(interface, options, x, y, w, h );

    return( err );
}

kern_return_t
IOFBBeamPosition( void * blitterRef, UInt32 options, SInt32 * position )
{
    _BlitterVars *	vars = (_BlitterVars *) blitterRef;
    IOGraphicsAcceleratorInterface ** interface = vars->interface;
    IOReturn		err;

    err = (*interface)->GetBeamPosition(interface, options, position);

    return( err );
}


kern_return_t
IOPSBlitFill( void * blitterRef,
		int x, int y, int w, int h, int data )
{
    _BlitterVars *		vars = (_BlitterVars *) blitterRef;
    IOGraphicsAcceleratorInterface ** interface = vars->interface;
    IOReturn			err;
    IOBlitRectangles		rects;

    rects.count = 1;
    rects.rects[0].x = x;
    rects.rects[0].y = y;
    rects.rects[0].width = w;
    rects.rects[0].height = h;

    err = (*vars->fillProc)(interface,
                    kNilOptions,
                    (kIOBlitTypeRects | kIOBlitCopyOperation),
                    (kIOBlitSourceSolid | kIOBlitDestFramebuffer),
                    &rects.operation,
                    (void *) data, 0,
                    &vars->lastCompletionToken);
    
    if( kIOReturnSuccess == err)
        (*interface)->Flush(interface, kNilOptions);

    return( err );
}

kern_return_t
IOPSBlitInvert( void * blitterRef,
		int x, int y, int w, int h )
{
    _BlitterVars *		vars = (_BlitterVars *) blitterRef;
    IOGraphicsAcceleratorInterface ** interface = vars->interface;
    IOReturn			err;
    IOBlitRectangles		rects;

    rects.count = 1;
    rects.rects[0].x = x;
    rects.rects[0].y = y;
    rects.rects[0].width = w;
    rects.rects[0].height = h;

    err = (*vars->fillProc)(interface,
                    kNilOptions,
                    (kIOBlitTypeRects | kIOBlitCopyOperation),
                    (kIOBlitSourceSolid | kIOBlitDestFramebuffer),
                    &rects.operation,
                    (void *) 0xffffffff, 0,
                    &vars->lastCompletionToken);
    
    if( kIOReturnSuccess == err)
        (*interface)->Flush(interface, kNilOptions);

    return( err );
}


kern_return_t
IOPSBlitCopy( void * blitterRef,
		int src_x, int src_y, int width, int height,
		int dst_x, int dst_y )
{
    return( IOFBBlitVRAMCopy( blitterRef, src_x, src_y, width, height,
				dst_x, dst_y, 1 * (kIOFBBlitBeamSync) ));
}

kern_return_t
IOFBBlitVRAMCopy( void * blitterRef,
                  int sourceX, int sourceY, int width, int height,
		  int x, int y, IOOptionBits options )
{
    _BlitterVars *			vars = (_BlitterVars *) blitterRef;
    IOGraphicsAcceleratorInterface ** 	interface = vars->interface;
    IOReturn				err;
    IOBlitCopyRectangles		rects;

    rects.count = 1;
    rects.rects[0].x = x;
    rects.rects[0].y = y;
    rects.rects[0].width = width;
    rects.rects[0].height = height;
    rects.rects[0].sourceX = sourceX;
    rects.rects[0].sourceY = sourceY;

    err = (*vars->copyProc)(interface,
                    options,
                    (kIOBlitTypeCopyRects | kIOBlitCopyOperation),
                    (kIOBlitSourceFramebuffer | kIOBlitDestFramebuffer),
                    &rects.operation,
                    0, 0,
                    &vars->lastCompletionToken);
    
    if( kIOReturnSuccess == err)
        (*interface)->Flush(interface, kNilOptions);

    return( err );
}

#if 0

kern_return_t
IOFBSetupFIFOBurst( void * blitterRef,
		UInt32 x, UInt32 y, UInt32 w, UInt32 h,
		UInt32 options, void ** burstRef )
{
    _BlitterVars *	vars = (_BlitterVars *) blitterRef;
    IOReturn		err;
    boolean_t		wait;

    do {
        IOSharedLockLock( &vars->context->contextLock );
        wait = (kIOReturnBusy == (
		err = vars->procs.setupFIFOBurst( vars->chipRef, x, y, w, h,
							options, burstRef )));
        IOSharedLockUnlock( &vars->context->contextLock, wait );
    } while( wait );

    return( err );
}

kern_return_t
IOFBCommitMemory( void * blitterRef,
		vm_address_t start, vm_size_t length, IOOptionBits options,
		void ** memoryRef, IOByteCount * offset )
{
    _BlitterVars *	vars = (_BlitterVars *) blitterRef;
    IOReturn		err;
    unsigned int	len;
    int			params[ 3 ];

    params[0] = start;
    params[1] = length;
    params[2] = options;
    len = 2;
    err = io_connect_method_scalarI_scalarO( vars->connect, 2, /*index*/
                    params, 3, params, &len);

    if( kIOReturnSuccess == err) {
	*memoryRef = (void *) params[0];    
	*offset = params[1];    
    }

    return( err );
}

kern_return_t
IOFBReleaseMemory( void * blitterRef, void * memoryRef )
{
    _BlitterVars *	vars = (_BlitterVars *) blitterRef;
    IOReturn		err;
    unsigned int	len;

    IOPSBlitIdle( blitterRef );

    len = 0;
    err = io_connect_method_scalarI_scalarO( vars->connect, 3, /*index*/
                    (int *) &memoryRef, 1, NULL, &len);

    return( err );
}

#endif

kern_return_t
IOFBMemoryCopy( void * blitterRef,
			UInt32 x, UInt32 y,
			UInt32 width, UInt32 height, 
			UInt32 srcByteOffset, UInt32 srcRowBytes,
			SInt32 * token)
{
    _BlitterVars *			vars = (_BlitterVars *) blitterRef;
    IOGraphicsAcceleratorInterface ** 	interface = vars->interface;
    IOReturn				err;
    IOBlitMemory			source;
    IOBlitCopyRectangles		rects;

    rects.count = 1;
    rects.rects[0].x = x;
    rects.rects[0].y = y;
    rects.rects[0].width = width;
    rects.rects[0].height = height;
    rects.rects[0].sourceX = 0;
    rects.rects[0].sourceY = 0;

    source.memory.ref = 0;	// !!
    source.byteOffset = srcByteOffset;
    source.rowBytes = srcRowBytes;

    err = (*vars->memCopyProc)(interface,
                    kNilOptions,
                    (kIOBlitTypeCopyRects | kIOBlitCopyOperation),
                    (kIOBlitSourceMemory | kIOBlitDestFramebuffer),
                    &rects.operation,
                    (void *) &source, 0,
                    &vars->lastCompletionToken);

    return( err );
}

#else /* NO_CFPLUGIN */

/* We need these symbols to exist to prevent link errors in clients.  Have them all return an error. */

kern_return_t
IOPSAllocateBlitEngine( io_connect_t framebuffer, void ** blitterRef, int * quality)
{ return kIOReturnUnsupported; }

kern_return_t
IOPSBlitReset( void * blitterRef)
{ return kIOReturnUnsupported; }

kern_return_t
IOPSBlitDeallocate( void * blitterRef )
{ return kIOReturnUnsupported; }

kern_return_t
IOPSBlitIdle( void * blitterRef )
{ return kIOReturnUnsupported; }

kern_return_t
IOFBWaitForCompletion( void * blitterRef, SInt32 token )
{ return kIOReturnUnsupported; }

kern_return_t
IOFBSynchronize( void * blitterRef, UInt32 x, UInt32 y, UInt32 w, UInt32 h, UInt32 options )
{ return kIOReturnUnsupported; }
     
kern_return_t
IOFBBeamPosition( void * blitterRef, UInt32 options, SInt32 * position )
{ return kIOReturnUnsupported; }
     
kern_return_t
IOPSBlitFill( void * blitterRef, int dst_x, int dst_y, int width, int height, int data )
{ return kIOReturnUnsupported; }

kern_return_t
IOPSBlitInvert( void * blitterRef, int x, int y, int w, int h )
{ return kIOReturnUnsupported; }

kern_return_t
IOPSBlitCopy( void * blitterRef, int src_x, int src_y, int width, int height, int dst_x, int dst_y )
{ return kIOReturnUnsupported; }

kern_return_t
IOFBBlitVRAMCopy( void * blitterRef, int sourceX, int sourceY, int width, int height, int x, int y, IOOptionBits options )
{ return kIOReturnUnsupported; }

kern_return_t
IOFBMemoryCopy( void * blitterRef, UInt32 x, UInt32 y, UInt32 width, UInt32 height, UInt32 srcByteOffset, UInt32 srcRowBytes, SInt32 * token)
{ return kIOReturnUnsupported; }

kern_return_t
IOFBSetupFIFOBurst( void * blitterRef, UInt32 x, UInt32 y, UInt32 w, UInt32 h, UInt32 options, void ** burstRef )
{ return kIOReturnUnsupported; }

void
IOFBBurstWrite32( void * p1, void * p2, void * p3, void * p4, void * p5, void * p6, void * p7, void * p8 )
{ return kIOReturnUnsupported; }

void
IOFBSetBurstRef( void * burstRef )
{ return kIOReturnUnsupported; }

kern_return_t
IOFBCommitMemory( void * blitterRef, vm_address_t start, vm_size_t length, IOOptionBits options, void ** memoryRef, IOByteCount * offset )
{ return kIOReturnUnsupported; }

kern_return_t
IOFBReleaseMemory( void * blitterRef, void * memoryRef )
{ return kIOReturnUnsupported; }

#endif /* !NO_CFPLUGIN */
