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

#ifndef _IOGL_WINDOW_CONNECT_H
#define _IOGL_WINDOW_CONNECT_H

#include <IOKit/graphics/IOGLTypes.h>

/*
** The IOGLAccelerator service name
*/
#define kIOGLAcceleratorClassName "IOGLAccelerator"


/*
** Window visible region in device coordinates.
**
** num_rects:	The number of rectangles in the rect array.  If num_rects
**		is zero the bounds rectangle is used for the visible rectangle.
**		If num_rects is zero the window must be completely contained
**		by the device.
**
** bounds:	The unclipped window rectangle in device coords.  Extends
**		beyond the device bounds if the window is not totally on
**		the device.
**
** rect[]:	An array of visible rectangles in device coords.  If num_rects
**		is non-zero only the region described by these rectangles is
**		copied to the frame buffer during a flush operation.
*/
typedef struct
{
        UInt32     num_rects;
        IOGLBounds bounds;
        IOGLBounds rect[0];
} IOGLDeviceRegion;


/*
** Determine the size of a region.
*/
#define IOGL_SIZEOF_DEVICE_REGION(_rgn_) (sizeof(IOGLDeviceRegion) + (_rgn_)->num_rects * sizeof(IOGLBounds))


/*
** IOGLAccelerator public client types.  Private client types start with kIOGLNumClientTypes.
*/
enum eIOGLAcceleratorClientTypes {
	kIOGLDummyClientType,
	kIOGLWindowClientType,
	kIOGLNumClientTypes,
};


/*
** Window client public memory types.  Private memory types start with kIOGLNumWindowMemoryTypes.
*/
enum eIOGLWindowMemoryTypes {
	kIOGLNumWindowMemoryTypes,
};


/*
** Window client public methods.  Private methods start with kIOGLNumWindowMethods.
*/
enum eIOGLWindowMethods {
	kIOGLWindowSetIDMode,
	kIOGLWindowSetShape,
	kIOGLWindowGetState,
	kIOGLWindowLock,
	kIOGLWindowUnlock,
	kIOGLNumWindowMethods,
};


/*
** Option bits for IOGLCreateWindow and the kIOGLWindowSetIDMode method.
** The color depth field can take any value of the _CGSDepth enumeration.
*/
typedef enum {
        kIOGLWindowModeColorDepthBits = 0x0000000F,
} eIOGLWindowModeBits;


/*
** Options bits for IOGLSetWindowShape and the kIOGLWindowSetShape method.
*/
typedef enum {
        kIOGLWindowShapeNone         = 0x00000000,
        kIOGLWindowShapeBlockingBit  = 0x00000001,
        kIOGLWindowShapeNonSimpleBit = 0x00000002,
} eIOGLWindowShapeBits;


/*
** Return bits for the kIOGLWindowGetState method.
*/
typedef enum {
	kIOGLWindowStateNone    = 0x00000000,
	kIOGLWindowStateIdleBit = 0x00000001,
} eIOGLWindowStateBits;

#endif /* _IOGL_WINDOW_CONNECT_H */
