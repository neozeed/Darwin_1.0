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
/*
 * Copyright (c) 1998,1999 Apple Computer, Inc.  All rights reserved. 
 *
 * HISTORY
 *
 */

#include <IOKit/IORegistryEntry.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOCatalogue.h>
#include <IOKit/IOUserClient.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOKitKeys.h>
#include <IOKit/IOKitDebug.h>
#include <IOKit/network/IONetworkController.h>

#include <IOKit/assert.h>

extern "C" {

extern void OSlibkernInit (void);
extern void IOLibInit(void);

#include <kern/clock.h>


void IOKitResetTime( void )
{
    mach_timespec_t	t;

    t.tv_sec = 30;
    t.tv_nsec = 0;
    IOService::waitForService(
        IOService::resourceMatching("IORTC"), &t );
#ifndef i386
    IOService::waitForService(
        IOService::resourceMatching("IONVRAM"), &t );
#endif

    clock_initialize_calendar();
}


void StartIOKit( void * p1, void * p2, void * p3, void * p4 )
{
    IOPlatformExpertDevice *	rootNub;
    int				debugFlags;
    IORegistryEntry *		root;
    OSObject *			obj;

    IOLog( iokit_version );

    if( PE_parse_boot_arg( "io", &debugFlags ))
	gIOKitDebug = debugFlags;

    //
    // Have to start IOKit environment before we attempt to start
    // the C++ runtime environment.  At some stage we have to clean up
    // the initialisation path so that OS C++ can initialise independantly
    // of iokit basic service initialisation, or better we have IOLib stuff
    // initialise as basic OS services.
    //
    IOLibInit(); 
    OSlibkernInit();

    IOLog("_cppInit done\n");

    root = IORegistryEntry::initialize();
    assert( root );
    IOService::initialize();
    IOCatalogue::initialize();
    IOUserClient::initialize();
    IOMemoryDescriptor::initialize();
    IONetworkController::initialize();

    obj = OSString::withCString( iokit_version );
    assert( obj );
    if( obj ) {
        root->setProperty( kIOKitBuildVersionKey, obj );
	obj->release();
    }
    obj = IOKitDiagnostics::diagnostics();
    if( obj ) {
        root->setProperty( kIOKitDiagnosticsKey, obj );
	obj->release();
    }

#ifdef i386
    // pretend there's no device-tree for intel
    p1 = 0;
#endif

    rootNub = new IOPlatformExpertDevice;

    if( rootNub && rootNub->initWithArgs( p1, p2, p3, p4)) {
        rootNub->attach( 0 );
        rootNub->registerService();
    }
}

}; /* extern "C" */