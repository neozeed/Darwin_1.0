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
/* IOCFPlugIn.h
 */

#include <CoreFoundation/CFPlugIn.h>
#include <IOKit/IOKitLib.h>


#define IOCFPLUGINBASE			\
    UInt16	version;		\
    UInt16	revision;		\
    IOReturn (*Probe)(void *thisPointer, CFDictionaryRef propertyTable,	\
                    io_service_t service, SInt32 * order);		\
    IOReturn (*Start)(void *thisPointer, CFDictionaryRef propertyTable,	\
                      io_service_t service);				\
    IOReturn (*Stop)(void *thisPointer)					\


typedef struct IOCFPlugInInterfaceStruct {
    IUNKNOWN_C_GUTS;
    IOCFPLUGINBASE;
} IOCFPlugInInterface;


kern_return_t
IOCreatePlugInInterfaceForService(io_service_t service,
                CFUUIDRef pluginType, CFUUIDRef interfaceType,
                IOCFPlugInInterface *** theInterface, SInt32 * theScore);

kern_return_t
IODestroyPlugInInterface(IOCFPlugInInterface ** interface);
