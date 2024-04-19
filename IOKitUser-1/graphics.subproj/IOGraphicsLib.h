/*
 * Copyright (c) 1999-2000 Apple Computer, Inc. All rights reserved.
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

#ifndef _IOKIT_IOGRAPHICSLIB_H
#define _IOKIT_IOGRAPHICSLIB_H

#include <IOKit/IOKitLib.h>
#include <IOKit/graphics/IOFramebufferShared.h>
#include <IOKit/graphics/IOGraphicsInterface.h>

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

extern kern_return_t
IOFBCreateSharedCursor( io_connect_t connect,
	unsigned int version,
	unsigned int maxWidth, unsigned int maxHeight );

extern kern_return_t
IOFBGetFramebufferInformationForAperture( io_connect_t connect,
	    IOPixelAperture		  aperture,
	    IOFramebufferInformation	* info );

extern kern_return_t
IOFBGetFramebufferOffsetForAperture( mach_port_t connect,
	    IOPixelAperture		  aperture,
	    IOByteCount			* offset );

extern kern_return_t
IOFBSetBounds( io_connect_t connect,
	    IOGBounds	* rect );

extern kern_return_t
IOFBGetCurrentDisplayModeAndDepth( io_connect_t connect,
	IODisplayModeID * displayMode,
	IOIndex 	* depth );

extern kern_return_t
IOFBGetPixelFormat( io_connect_t connect,
	IODisplayModeID displayMode,
	IOIndex 	depth,
        IOPixelAperture aperture,
	IOPixelEncoding * pixelFormat );

extern kern_return_t
IOFBSetCLUT( io_connect_t connect,
	UInt32		startIndex,
	UInt32		numEntries,
	IOOptionBits	options,
	IOColorEntry *	colors );

extern kern_return_t
IOFBSetGamma( io_connect_t connect,
	UInt32		channelCount,
	UInt32		dataCount,
	UInt32		dataWidth,
	void *		data );

extern kern_return_t
IOFBSet888To256Table( io_connect_t connect,
	const unsigned char *	table );

extern kern_return_t
IOFBSet256To888Table( io_connect_t connect,
	const unsigned int *	table );

extern kern_return_t
IOFBSet444To555Table( io_connect_t connect,
	const unsigned char *	table );

extern kern_return_t
IOFBSet555To444Table( io_connect_t connect,
	const unsigned char *	table );

// Array of supported display modes

kern_return_t
IOFBGetDisplayModeCount( io_connect_t connect,
	UInt32 * count );

kern_return_t
IOFBGetDisplayModes( io_connect_t connect,
	UInt32			count,
	IODisplayModeID	*	allDisplayModes );

// Info about a display mode

kern_return_t
IOFBGetDisplayModeInformation( io_connect_t connect,
	IODisplayModeID		displayMode,
	IODisplayModeInformation * info );


// Mask of pixel formats available in mode and depth

kern_return_t
IOFBGetPixelFormats( io_connect_t connect,
	IODisplayModeID		displayMode,
	IOIndex			depth,
	UInt32 * 		mask );

kern_return_t
IOFBGetPixelInformation( io_connect_t connect,
	IODisplayModeID 	displayMode,
	IOIndex 		depth,
        IOPixelAperture		aperture,
	IOPixelInformation *	pixelInfo );

kern_return_t
IOFBSetDisplayModeAndDepth( io_connect_t connect,
	IODisplayModeID		displayMode,
	IOIndex 		depth );

kern_return_t
IOFBSetStartupDisplayModeAndDepth( io_connect_t connect,
	IODisplayModeID		displayMode,
	IOIndex 		depth );

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

CFDictionaryRef
IOFBCreateDisplayModeDictionary(
	io_service_t		framebuffer,
	IODisplayModeID		displayMode );

CFDictionaryRef
IOFBGetPixelInfoDictionary( 
	CFDictionaryRef		modeDictionary,
	IOIndex 		depth,
	IOPixelAperture		aperture );

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

kern_return_t
IOFBSetNewCursor( io_connect_t connect,
	void *			cursor,
	IOIndex			frame,
	IOOptionBits		options );

kern_return_t
IOFBSetCursorVisible( io_connect_t connect,
	int			visible );

kern_return_t
IOFBSetCursorPosition( io_connect_t connect,
	long int		x,
	long int		y );

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

kern_return_t
IOFBAcknowledgePM( io_connect_t connect );

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

kern_return_t
IOPSAllocateBlitEngine( io_connect_t framebuffer,
		void ** blitterRef, int * quality);

kern_return_t
IOPSBlitReset( void * blitterRef);

kern_return_t
IOPSBlitDeallocate( void * blitterRef);

kern_return_t
IOPSBlitIdle( void * blitterRef);

kern_return_t
IOPSBlitFill( void * blitterRef,
		int dst_x, int dst_y, int width, int height, int data );

kern_return_t
IOPSBlitCopy( void * blitterRef,
		int src_x, int src_y, int width, int height,
		int dst_x, int dst_y );

kern_return_t
IOPSBlitInvert( void * blitterRef,
		int x, int y, int w, int h );

enum {
    // options for IOFBSynchronize
    kIOFBSynchronizeWaitBeamExit	= kIOBlitSynchronizeWaitBeamExit,
    kIOFBSynchronizeFlushWrites		= kIOBlitSynchronizeFlushHostWrites
};

enum {
    // options for IOFBBlitVRAMCopy
    kIOFBBlitBeamSync			= kIOBlitBeamSync,
    kIOFBBlitBeamSyncAlways		= kIOBlitBeamSyncAlways,
    kIOFBBlitBeamSyncSpin		= kIOBlitBeamSyncSpin
};

kern_return_t
IOFBBlitVRAMCopy( void * blitterRef,
		int src_x, int src_y, int width, int height,
		int dst_x, int dst_y, IOOptionBits options );

kern_return_t
IOFBSynchronize( void * blitterRef,
               	 UInt32 x, UInt32 y, UInt32 w, UInt32 h, UInt32 options );

kern_return_t
IOFBBeamPosition( void * blitterRef, UInt32 options, SInt32 * position );

kern_return_t
IOFBSetupFIFOBurst( void * blitterRef,
			UInt32 x, UInt32 y, UInt32 w, UInt32 h,
			UInt32 options, void ** burstRef );

void
IOFBBurstWrite32( void * p1, void * p2, void * p3, void * p4,
		  void * p5, void * p6, void * p7, void * p8 );

void
IOFBSetBurstRef( void * burstRef );

kern_return_t
IOFBCommitMemory( void * blitterRef,
		vm_address_t start, vm_size_t length, IOOptionBits options,
		void ** memoryRef, IOByteCount * offset );

kern_return_t
IOFBReleaseMemory( void * blitterRef, void * memoryRef );

kern_return_t
IOFBWaitForCompletion( void * blitterRef, SInt32 token );

kern_return_t
IOFBMemoryCopy( void * blitterRef,
			UInt32 destLeft, UInt32 destTop,
			UInt32 width, UInt32 height, 
			UInt32 srcByteOffset, UInt32 srcRowBytes,
			SInt32 * token);

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */


#endif /* ! _IOKIT_IOGRAPHICSLIB_H */

