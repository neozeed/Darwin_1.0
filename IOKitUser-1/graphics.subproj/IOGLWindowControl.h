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

#ifndef _IOGL_WINDOW_CONTROL_H
#define _IOGL_WINDOW_CONTROL_H

#include <IOKit/graphics/IOGLWindowConnect.h>


typedef struct IOGLConnectStruct *IOGLConnect;


/* Create an OpenGL window and attach it to a CGS window */
IOReturn IOGLCreateWindow( io_service_t service, UInt32 wid, eIOGLWindowModeBits modebits, IOGLConnect *glconnect );

/* Detach an an OpenGL window from a CGS window and destroy it*/
IOReturn IOGLDestroyWindow( IOGLConnect glconnect );

/* Change the visible region of the OpenGL window */
IOReturn IOGLSetWindowShape( IOGLConnect glconnect, IOGLDeviceRegion *region, eIOGLWindowShapeBits options );

/* Block until the last visible region change applied to an OpenGL window is complete */
IOReturn IOGLWaitForWindow( IOGLConnect glconnect );

/* Get the back buffer of the window in vram.  Offset is relative to the base of the vram aperture. */
IOReturn IOGLLockWindow( IOGLConnect glconnect, UInt32 *offset, UInt32 *rowbytes, UInt32 *width, UInt32 *height );

/* Release the lock on the vram */
IOReturn IOGLUnlockWindow( IOGLConnect glconnect );

#endif /* _IOGL_WINDOW_CONTROL_H */
