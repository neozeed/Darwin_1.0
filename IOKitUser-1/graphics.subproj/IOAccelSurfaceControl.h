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

#ifndef _IOACCEL_SURFACE_CONTROL_H
#define _IOACCEL_SURFACE_CONTROL_H

#include <IOKit/graphics/IOAccelSurfaceConnect.h>


typedef struct IOAccelConnectStruct *IOAccelConnect;


/* Create an accelerated surface and attach it to a CGS surface */
IOReturn IOAccelCreateSurface( io_service_t service, UInt32 wid, eIOAccelSurfaceModeBits modebits, IOAccelConnect *glconnect );

/* Detach an an accelerated surface from a CGS surface and destroy it*/
IOReturn IOAccelDestroySurface( IOAccelConnect glconnect );

/* Change the visible region of the accelerated surface */
IOReturn IOAccelSetSurfaceShape( IOAccelConnect glconnect, IOAccelDeviceRegion *region, eIOAccelSurfaceShapeBits options );

/* Block until the last visible region change applied to an accelerated surface is complete */
IOReturn IOAccelWaitForSurface( IOAccelConnect glconnect );

/* Get the back buffer of the surface in vram.  Offset is relative to the base of the vram aperture. */
IOReturn IOAccelLockSurface( IOAccelConnect glconnect, UInt32 *offset, UInt32 *rowbytes, UInt32 *width, UInt32 *height );

/* Release the lock on the surface's vram */
IOReturn IOAccelUnlockSurface( IOAccelConnect glconnect );


#endif /* _IOACCEL_SURFACE_CONTROL_H */

