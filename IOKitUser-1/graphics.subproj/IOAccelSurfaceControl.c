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

#include <IOKit/IOKitLib.h>

#include <IOKit/graphics/IOAccelSurfaceControl.h>

extern kern_return_t io_connect_method_structureI_structureO(
	io_connect_t connect,
	UInt32 index,
	void *dataIn,
	SInt32 bytesIn,
	void *dataOut,
	SInt32 *bytesInOut); 

extern kern_return_t io_connect_method_scalarI_scalarO(
	io_connect_t connect,
	UInt32 index,
	SInt32 *scalarsIn,
	SInt32 countIn,
	SInt32 *scalarsOut,
	SInt32 *countInOut); 

extern kern_return_t io_connect_method_scalarI_structureO(
	io_connect_t connect,
	UInt32 index,
	SInt32 *scalarsIn,
	SInt32 countIn,
	void *dataOut,
	SInt32 *bytesInOut); 

extern kern_return_t io_connect_method_scalarI_structureI(
	io_connect_t connect,
	UInt32 index,
	SInt32 *scalarsIn,
	SInt32 countIn,
	void *dataIn,
	SInt32 bytesIn); 

IOReturn IOAccelCreateSurface( io_service_t device, UInt32 wid, eIOAccelSurfaceModeBits modebits, IOAccelConnect *glconnect )
{
	IOReturn      kr;
	io_connect_t  window = MACH_PORT_NULL;
	SInt32        data[2];
	io_service_t  service;
	io_iterator_t iter;

	*glconnect = NULL;

	kr = IORegistryEntryGetChildIterator( device, kIOServicePlane, &iter );
	if( kr != kIOReturnSuccess)
	{
		return kr;
	}

	for( kr = kIOReturnUnsupported;
		(!window && (service = IOIteratorNext(iter)));
		IOObjectRelease(service))
	{
		if( !IOObjectConformsTo( service, kIOAcceleratorClassName ))
			continue;

		/* Create a context */
		kr = IOServiceOpen( service,
			    mach_task_self(),
			    kIOAccelSurfaceClientType,
			   &window );

		if( kr != kIOReturnSuccess)
		{
			window = MACH_PORT_NULL;
		}
	}

	IOObjectRelease(iter);

	if( !window )
	{
		return kr;
	}

	/* Set the window id */
	data[0] = wid;
	data[1] = modebits;
	kr = io_connect_method_scalarI_structureI(window, kIOAccelSurfaceSetIDMode,
		data, 2, NULL, 0);
	if(kr != kIOReturnSuccess)
	{
		IOServiceClose(window);
		return kr;
	}

	*glconnect = (IOAccelConnect) window;

	return kIOReturnSuccess;
}

IOReturn IOAccelDestroySurface( IOAccelConnect glconnect )
{
	IOReturn kr;

	if(!glconnect)
		return kIOReturnError;

	kr = IOServiceClose((io_connect_t) glconnect);

	return kr;
}

IOReturn IOAccelSetSurfaceShape( IOAccelConnect glconnect, IOAccelDeviceRegion *rgn, eIOAccelSurfaceShapeBits options )
{
	return io_connect_method_scalarI_structureI((io_connect_t) glconnect, kIOAccelSurfaceSetShape,
		(SInt32 *) &options, 1, (SInt32 *) rgn, IOACCEL_SIZEOF_DEVICE_REGION(rgn));
}

IOReturn IOAccelLockSurface( IOAccelConnect glconnect, UInt32 *offset, UInt32 *rowbytes, UInt32 *width, UInt32 *height )
{
	SInt32 vars[4];
	SInt32 countio = 4;
	IOReturn ret;

	ret = io_connect_method_scalarI_scalarO((io_connect_t) glconnect, kIOAccelSurfaceLock,
		NULL, 0, vars, &countio);

	if(ret == kIOReturnSuccess && countio == 4)
	{
		*offset   = vars[0];
		*rowbytes = vars[1];
		*width    = vars[2];
		*height   = vars[3];
	}

	return ret;
}

IOReturn IOAccelUnlockSurface( IOAccelConnect glconnect )
{
	SInt32 countio = 0;

	return io_connect_method_scalarI_scalarO((io_connect_t) glconnect, kIOAccelSurfaceUnlock,
		NULL, 0, NULL, &countio);
}

IOReturn IOAccelWaitForSurface( IOAccelConnect glconnect )
{
	return kIOReturnSuccess;
}

