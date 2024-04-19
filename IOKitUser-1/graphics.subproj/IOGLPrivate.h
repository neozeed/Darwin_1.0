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

#ifndef _IOGL_PRIVATE_H
#define _IOGL_PRIVATE_H

#include <IOKit/IOTypes.h>

#include "IOGraphicsLibPrivate.h"


extern kern_return_t ioglProbe(
	io_service_t device, io_connect_t connect, void ** ref, BlitterProcs * procs );

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

#endif /* _IOGL_PRIVATE_H */

