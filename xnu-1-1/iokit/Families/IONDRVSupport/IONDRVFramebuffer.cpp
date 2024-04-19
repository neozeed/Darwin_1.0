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
 * Copyright (c) 1997-1998 Apple Computer, Inc.
 *
 *
 * HISTORY
 *
 * sdouglas  22 Oct 97 - first checked in.
 * sdouglas  24 Jul 98 - start IOKit.
 * sdouglas  15 Dec 98 - cpp.
 *
 */

#include <IOKit/IOLib.h>
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOLocks.h>
#include <IOKit/ndrvsupport/IONDRVFramebuffer.h>
#include <IOKit/assert.h>

#include <libkern/c++/OSContainers.h>

#include "IONDRV.h"

#include <string.h>

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

class IOATINDRV : public IONDRVFramebuffer
{
    OSDeclareDefaultStructors(IOATINDRV)

public:
    virtual IOReturn getStartupDisplayMode( IODisplayModeID * displayMode,
                            IOIndex * depth );
    virtual IODeviceMemory * findVRAM( void );

};

class IOATI128NDRV : public IOATINDRV
{
    OSDeclareDefaultStructors(IOATI128NDRV)

public:
    virtual void flushCursor( void );
};

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

struct _VSLService {
    class IONDRVFramebuffer *	framebuffer;
    IOSelect			type;
    IOFBInterruptProc  		handler;
    OSObject *			target;
    void *			ref;
    _VSLService *		next;
};

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

// frame buffer has two power states, off and on
#define number_of_power_states 2

static IOPMPowerState ourPowerStates[number_of_power_states] = {
  {1,0,0,0,0,0,0,0,0,0,0,0},
  {1,IOPMDeviceUsable,IOPMPowerOn,IOPMPowerOn,0,0,0,0,0,0,0,0}
};

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define super IOFramebuffer

OSDefineMetaClassAndStructors(IONDRVFramebuffer, IOFramebuffer)

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

//============
//= External =
//============

IOService * IONDRVFramebuffer::probe( IOService * 	provider,
                                        SInt32 *	score )
{
    IOService *		inst = this;
    IOService *		newInst = 0;
    const char *	name;

    if( !super::probe( provider, score ))
	return( 0 );

    if( IONDRV::fromRegistryEntry( provider )) {

        // temporary for in-kernel acceleration
        name = provider->getName();
        if( 0 == strncmp("ATY,Rage128", name, strlen("ATY,Rage128")))
            newInst = new IOATI128NDRV;
        else if( 0 == strncmp("ATY,", name, strlen("ATY,")))
            newInst = new IOATINDRV;

	if( newInst) {
            if( ! newInst->init( inst->getPropertyTable())) {
                newInst->release();
                newInst = 0;
            }
	    inst = newInst;
	}
    } else
	inst = 0;

    return( inst );
}

bool IONDRVFramebuffer::start( IOService * provider )
{
    bool   		ok = false;
    IOService *		parent;
    
    do {
   	nub = provider;
        ndrv = IONDRV::fromRegistryEntry( provider );
	if( 0 == ndrv)
	    continue;

	setName( ndrv->driverName());
	startAt8 = 3;
        consoleDevice = (0 != provider->getProperty("AAPL,boot-display"));

        if( 0 == nub->getDeviceMemoryCount()) {
            parent = OSDynamicCast( IOService, nub->getParentEntry(gIODTPlane));
            if( parent) {
                parent->getResources();
                OSArray * array = parent->getDeviceMemory();
                array->retain();
                nub->setDeviceMemory( array);
                array->release();
            }
        }

        if( false == super::start( nub ))
	    continue;

	ok = true;			// Success

    } while( false);
    
    return( ok);
}

bool IONDRVFramebuffer::isConsoleDevice( void )
{
    return( consoleDevice );
}

// osfmk/ppc/mappings.h
extern "C" { extern void ignore_zero_fault(boolean_t); }

IOReturn IONDRVFramebuffer::enableController( void )
{
    IOReturn		err;
    const char *	logname;
    
    logname = getProvider()->getName();

    if( 0 == strcmp( "control", logname))
        waitForService( resourceMatching( "IOiic0" ));

    err = IONDRVLibrariesInitialize( getProvider() );

    if( kIOReturnSuccess == err) do {

        ignore_zero_fault( true );
	err = checkDriver();
        ignore_zero_fault( false );

        if( err) {
            IOLog("%s: Not usable\n", logname );
            if( err == -999)
                IOLog("%s: driver incompatible.\n", logname );
            continue;
        }
        getCurrentConfiguration();
        vramMemory = findVRAM();

    } while( false);

    // initialize power management of the device
    initForPM();
    
    return( err);
}

IODeviceMemory * IONDRVFramebuffer::getVRAMRange( void )
{
    if( vramMemory)
	vramMemory->retain();

    return( vramMemory );
}

void IONDRVFramebuffer::free( void )
{
    super::free();
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOReturn IONDRVFramebuffer::registerForInterruptType( IOSelect interruptType,
                        IOFBInterruptProc proc, OSObject * target, void * ref,
			void ** interruptRef )

{
    _VSLService *	service;
    IOReturn		err;

    if( (interruptType == kIOFBVBLInterruptType)
        && (getProvider()->getProperty("Ignore VBL")))
	return( kIOReturnUnsupported );

    for( service = vslServices;
	 service && (service->type != interruptType);
	 service = service->next ) {}

    if( service) {

	if( service->handler)
	    err = kIOReturnBusy;

	else {
	    service->target	= target;
	    service->ref	= ref;
	    service->handler	= proc;
	    *interruptRef = service;
	    err = kIOReturnSuccess;
	}

    } else
	err = kIOReturnNoResources;

    return( err );
}

IOReturn IONDRVFramebuffer::unregisterInterrupt( void * interruptRef )
{
    _VSLService *	service = (_VSLService *) interruptRef;

    service->handler = 0;

    return( kIOReturnSuccess );
}

IOReturn IONDRVFramebuffer::setInterruptState( void * interruptRef, 
						UInt32 state )
{
    return( kIOReturnUnsupported );
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

//// VSL calls

OSStatus IONDRVFramebuffer::VSLNewInterruptService(
                                        void * entryID,
                                        IOSelect serviceType,
                                        _VSLService ** vslService )
{
    IORegistryEntry *	regEntry;
    IONDRVFramebuffer *	fb;
    _VSLService *	service;
    IOReturn		err = kIOReturnSuccess;

    REG_ENTRY_TO_OBJ( (const RegEntryID *) entryID, regEntry)

    fb = OSDynamicCast( IONDRVFramebuffer,
		regEntry->getChildEntry( gIOServicePlane ));
    assert( fb );

    if( fb) {
	service = IONew( _VSLService, 1 );

	if( service) {
            service->framebuffer	= fb;
            service->type		= serviceType;
	    service->handler		= 0;
            service->next = fb->vslServices;
            fb->vslServices = service;

            *vslService = service;

	} else
	    err = kIOReturnNoMemory;

    } else
	err = kIOReturnBadArgument;

    return( err );
}

OSStatus IONDRVFramebuffer::VSLDisposeInterruptService(_VSLService * vslService)
{
    IONDRVFramebuffer *	fb;
    _VSLService * 	next;
    _VSLService * 	prev;

    if( vslService) {

	fb = vslService->framebuffer;

        prev = fb->vslServices;
	if( prev == vslService)
	    fb->vslServices = vslService->next;
	else {
	    while( ((next = prev->next) != vslService) && next)
		prev = next;
	    if( next)
		prev->next = vslService->next;
	}

	IODelete( vslService, _VSLService, 1 );
    }

    return( kIOReturnSuccess );
}

OSStatus IONDRVFramebuffer::VSLDoInterruptService( _VSLService * vslService )
{
    IOFBInterruptProc	proc;

    if( vslService) {
	if( (proc = vslService->handler))
	    (*proc) (vslService->target, vslService->ref);
    }

    return( kIOReturnSuccess );
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

struct _VSLCursorRef {
    IOFramebuffer *	framebuffer;
    void * 		cursorImage;
};

Boolean IONDRVFramebuffer::VSLPrepareCursorForHardwareCursor(
                                        void * cursorRef,
                                        IOHardwareCursorDescriptor * hwDesc,
                                        IOHardwareCursorInfo * hwCursorInfo )
{
    _VSLCursorRef *	cursor = (_VSLCursorRef *) cursorRef;
    bool		ok;

    if( hwCursorInfo->colorMap)
        hwCursorInfo->colorMap += 1;
    ok = cursor->framebuffer->convertCursorImage(
		cursor->cursorImage, hwDesc, hwCursorInfo );
    if( hwCursorInfo->colorMap)
        hwCursorInfo->colorMap -= 1;

    return( ok );
}

IOReturn IONDRVFramebuffer::setCursorImage( void * cursorImage )
{
    _VSLCursorRef		cursorRef;
    VDSetHardwareCursorRec	setCursor;
    IOReturn			err;

    cursorRef.framebuffer = this;
    cursorRef.cursorImage = cursorImage;

    setCursor.csCursorRef = (void *) &cursorRef;
    setCursor.csReserved1 = 0;
    setCursor.csReserved2 = 0;

    err = doControl( cscSetHardwareCursor, &setCursor );

    return( err );
}

IOReturn IONDRVFramebuffer::setCursorState( SInt32 x, SInt32 y, bool visible )
{
    VDDrawHardwareCursorRec	drawCursor;
    IOReturn			err;

    if( 0 == OSIncrementAtomic( &ndrvEnter))
    {

        drawCursor.csCursorX 	= x;
        drawCursor.csCursorY 	= y;
        drawCursor.csCursorVisible 	= visible;
        drawCursor.csReserved1 	= 0;
        drawCursor.csReserved2 	= 0;

        err = doControl( cscDrawHardwareCursor, &drawCursor );

    } else
	err = kIOReturnBusy;

    OSDecrementAtomic( &ndrvEnter );

    return( err );
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

//============
//= Internal =
//============

IOReturn IONDRVFramebuffer::doControl( UInt32 code, void * params )
{
    IOReturn	err;
    CntrlParam	pb;

    if( ndrvState == 0)
	return( kIOReturnNotOpen);

    pb.qLink = 0;
    pb.csCode = code;
    pb.csParams = params;

    OSIncrementAtomic( &ndrvEnter );
    err = ndrv->doDriverIO( /*ID*/ (UInt32) &pb, &pb,
                            kControlCommand, kImmediateIOCommandKind );
    OSDecrementAtomic( &ndrvEnter );

    return( err);
}

IOReturn IONDRVFramebuffer::doStatus( UInt32 code, void * params )
{
    IOReturn	err;
    CntrlParam	pb;

    if( ndrvState == 0)
	return( kIOReturnNotOpen);

    pb.qLink = 0;
    pb.csCode = code;
    pb.csParams = params;

    OSIncrementAtomic( &ndrvEnter );
    err = ndrv->doDriverIO( /*ID*/ (UInt32) &pb, &pb,
                            kStatusCommand, kImmediateIOCommandKind );
    OSDecrementAtomic( &ndrvEnter );

    return( err);
}


IOReturn IONDRVFramebuffer::checkDriver( void )
{
    OSStatus			err = noErr;
    struct DriverInitInfo	initInfo;
    CntrlParam          	pb;
    VDClutBehavior		clutSetting;
    VDGammaRecord		gammaRec;
    VDSwitchInfoRec		switchInfo;
    IOTimingInformation 	info;
    VDPageInfo			pageInfo;

    if( ndrvState == 0) {
	do {
	    initInfo.refNum = 0xffcd;			// ...sure.
	    MAKE_REG_ENTRY(initInfo.deviceEntry, nub )
    
	    err = ndrv->doDriverIO( 0, &initInfo,
				kInitializeCommand, kImmediateIOCommandKind );
	    if( err) continue;

	    err = ndrv->doDriverIO( 0, &pb,
				kOpenCommand, kImmediateIOCommandKind );

	} while( false);

	if( err)
	    return( err);

        // allow calls to ndrv
        ndrvState = 1;

        if( (noErr == doStatus( cscGetCurMode, &switchInfo ))
            && (noErr == getTimingInfoForDisplayMode( switchInfo.csData, &info))
            && (timingApple_0x0_0hz_Offline == info.appleTimingID)) {

            IOLog("%s: display offline\n", getName());
            err = kIOReturnOffline;
            return( err);
        } else
            ndrvState = 2;

        // duplicate QD InitGDevice
        pageInfo.csMode = switchInfo.csMode;
        pageInfo.csData = 0;
        pageInfo.csPage = 0;
        doControl( cscGrayPage, &pageInfo);

        clutSetting = kSetClutAtSetEntries;
        lastClutSetting = clutSetting;
        doControl( cscSetClutBehavior, &clutSetting);

#if 1
	// bogus for ROM control
	do {

	    VDGetGammaListRec	scan;
	    VDRetrieveGammaRec	get;
	    GammaTbl *		table;
	    char		name[ 64 ];

	    scan.csPreviousGammaTableID = kGammaTableIDFindFirst;
	    scan.csGammaTableName = name;
	    err = doStatus( cscGetGammaInfoList, &scan);
	    if( err || (scan.csGammaTableID == kGammaTableIDNoMoreTables))
		continue;

	    table = (GammaTbl *)IOMalloc( scan.csGammaTableSize);
	    if( 0 == table)
		continue;
	    get.csGammaTableID = scan.csGammaTableID;
	    get.csGammaTablePtr = table;
	    
	    err = doStatus( cscRetrieveGammaTable, &get );
	    if( noErr == err) {
		kprintf("Setting gamma %s\n", scan.csGammaTableName);
		gammaRec.csGTable = (Ptr) table;
		doControl( cscSetGamma, &gammaRec );
	    }

	    IOFree( table, scan.csGammaTableSize);

	} while( false);
#endif
    }
    return( noErr);
}


UInt32 IONDRVFramebuffer::iterateAllModes( IODisplayModeID * displayModeIDs )
{
    VDResolutionInfoRec	info;
    UInt32		num = 0;

    info.csPreviousDisplayModeID = kDisplayModeIDFindFirstResolution;

    while( 
 	   (noErr == doStatus( cscGetNextResolution, &info))
	&& ((SInt32) info.csDisplayModeID > 0) ) {

	    if( displayModeIDs)
		displayModeIDs[ num ] = info.csDisplayModeID;

	    info.csPreviousDisplayModeID = info.csDisplayModeID;
	    num++;
    }
    return( num);
}


IOReturn IONDRVFramebuffer::getResInfoForMode( IODisplayModeID modeID,
				VDResolutionInfoRec ** theInfo )
{
    // unfortunately, there is no "kDisplayModeIDFindSpecific"

    if( (SInt32) modeID <= 0)
        return( kIOReturnUnsupportedMode);

    *theInfo = &cachedVDResolution;

    if( cachedVDResolution.csDisplayModeID == (UInt32) modeID)
	return( noErr);

    // try the next after cached mode
    cachedVDResolution.csPreviousDisplayModeID = cachedVDResolution.csDisplayModeID;

    if( (noErr == doStatus( cscGetNextResolution, &cachedVDResolution))
    && (cachedVDResolution.csDisplayModeID == (UInt32) modeID) )
	return( noErr);

    // full blown iterate
    cachedVDResolution.csPreviousDisplayModeID = kDisplayModeIDFindFirstResolution;

    while(
	(noErr == doStatus( cscGetNextResolution, &cachedVDResolution))
    && (cachedVDResolution.csDisplayModeID != (UInt32) modeID) 
    && ((SInt32) cachedVDResolution.csDisplayModeID > 0)) {

	cachedVDResolution.csPreviousDisplayModeID = cachedVDResolution.csDisplayModeID;
    }

    if( cachedVDResolution.csDisplayModeID == (UInt32) modeID)
	return( noErr);

    cachedVDResolution.csDisplayModeID = 0xffffffff;
    return( kIOReturnUnsupportedMode);
}

void IONDRVFramebuffer::getCurrentConfiguration( void )
{
    IOReturn		err;
    VDSwitchInfoRec	switchInfo;
    VDGrayRecord	grayRec;

    grayRec.csMode = 0;			// turn off luminance map
    err = doControl( cscSetGray, &grayRec );
    // driver refused => mono display
    grayMode = ((noErr == err) && (0 != grayRec.csMode));

    err = doStatus( cscGetCurMode, &switchInfo );
    if( err == noErr) {
        currentDisplayMode	= switchInfo.csData;
        currentDepth		= switchInfo.csMode - kDepthMode1;
        currentPage		= switchInfo.csPage;
	if( 0 == (physicalFramebuffer = pmap_extract( kernel_pmap,
		((vm_address_t) switchInfo.csBaseAddr) )))
	    physicalFramebuffer = (UInt32) switchInfo.csBaseAddr;
    } else
	IOLog("%s: cscGetCurMode failed\n", nub->getName());
}

IODeviceMemory * IONDRVFramebuffer::makeSubRange( 
	IOPhysicalAddress	start,
	IOPhysicalLength	length ) 
{
    IODeviceMemory *	mem = 0;
    UInt32		numMaps, i;
    IOService *		device;

    device = nub;
    numMaps = device->getDeviceMemoryCount();

    for( i = 0; (!mem) && (i < numMaps); i++) {
	mem = device->getDeviceMemoryWithIndex(i);
	if( !mem)
	    continue;
	mem = IODeviceMemory::withSubRange( mem,
			start - mem->getPhysicalAddress(), length );
    }
    if( !mem)
	mem = IODeviceMemory::withRange( start, length );

    return( mem );
}

IODeviceMemory * IONDRVFramebuffer::getApertureRange( IOPixelAperture aper )
{
    IOReturn			err;
    IOPixelInformation		info;
    IOByteCount			bytes;

    err = getPixelInformation( currentDisplayMode, currentDepth, aper,
                                &info );
    if( err)
	return( 0 );

    bytes = (info.bytesPerRow * info.activeHeight) + 128;

    return( makeSubRange( physicalFramebuffer, bytes ));
}

IODeviceMemory * IONDRVFramebuffer::findVRAM( void )
{
    VDVideoParametersInfoRec	pixelParams;
    VPBlock			pixelInfo;
    VDResolutionInfoRec		vdRes;
    UInt32			size;
    IOPhysicalAddress		vramBase;
    IOByteCount			vramLength;
    IOReturn			err;

    vramLength = 0;
    vdRes.csPreviousDisplayModeID = kDisplayModeIDFindFirstResolution;
    while(
        (noErr == doStatus( cscGetNextResolution, &vdRes))
    && ((SInt32) vdRes.csDisplayModeID > 0) )
    {
        pixelParams.csDisplayModeID = vdRes.csDisplayModeID;
        pixelParams.csDepthMode = vdRes.csMaxDepthMode;
        pixelParams.csVPBlockPtr = &pixelInfo;
        err = doStatus( cscGetVideoParameters, &pixelParams);
        if( err)
            continue;

        // Control hangs its framebuffer off the end of the aperture to support
        // 832 x 624 @ 32bpp. The commented out version will correctly calculate
        // the vram length, but DPS needs the full extent to be mapped, so we'll
        // end up mapping an extra page that will address vram through the
        // little endian aperture. No other drivers like this known.
#if 1
        size = 0x40 + pixelInfo.vpBounds.bottom *
			(pixelInfo.vpRowBytes & 0x7fff);
#else
        size = ( (pixelInfo.vpBounds.right * pixelInfo.vpPixelSize) / 8)	// last line
                + (pixelInfo.vpBounds.bottom - 1) *
		(pixelInfo.vpRowBytes & 0x7fff);
#endif
        if( size > vramLength)
            vramLength = size;

        vdRes.csPreviousDisplayModeID = vdRes.csDisplayModeID;
    }

    vramBase = physicalFramebuffer;
    vramLength = (vramLength + (vramBase & 0xffff) + 0xffff) & 0xffff0000;
    vramBase &= 0xffff0000;

    return( makeSubRange( vramBase, vramLength ));
}

//============
//= External =
//============

const char * IONDRVFramebuffer::getPixelFormats( void )
{
    static const char * ndrvPixelFormats =
        IO1BitIndexedPixels "\0"
        IO2BitIndexedPixels "\0"
        IO4BitIndexedPixels "\0"
        IO8BitIndexedPixels "\0"
        IO16BitDirectPixels "\0"
        IO32BitDirectPixels "\0"
        "\0";

    return( ndrvPixelFormats);
}

IOItemCount IONDRVFramebuffer::getDisplayModeCount( void )
{
    return( iterateAllModes( 0 ));
}

IOReturn IONDRVFramebuffer::getDisplayModes( IODisplayModeID * allDisplayModes )
{
    iterateAllModes( allDisplayModes );
    return( kIOReturnSuccess );
}

IOReturn IONDRVFramebuffer::getInformationForDisplayMode(
		IODisplayModeID displayMode, IODisplayModeInformation * info )
{
    IOReturn			err;
    VDResolutionInfoRec	*	resInfo;

    bzero( info, sizeof( *info));
    do {
	err = getResInfoForMode( displayMode, &resInfo );
	if( err)
	    continue;
	info->maxDepthIndex	= resInfo->csMaxDepthMode - kDepthMode1;
	info->nominalWidth	= resInfo->csHorizontalPixels;
	info->nominalHeight	= resInfo->csVerticalLines;
	info->refreshRate	= resInfo->csRefreshRate;
	return( noErr);
    } while( false);

    return( kIOReturnUnsupportedMode);
}


UInt64 IONDRVFramebuffer::getPixelFormatsForDisplayMode(
		IODisplayModeID /* displayMode */, IOIndex depthIndex )
{
    return( 1 << (depthIndex + startAt8));
}

IOReturn IONDRVFramebuffer::getPixelInformation(
	IODisplayModeID displayMode, IOIndex depth,
	IOPixelAperture aperture, IOPixelInformation * info )
{
    SInt32			err;
    VDVideoParametersInfoRec	pixelParams;
    VPBlock			pixelInfo;
    const char *		formats;
    UInt32			mask;
    int				index;

    bzero( info, sizeof( *info));

    if( aperture)
        return( kIOReturnUnsupportedMode);

    do {
    	pixelParams.csDisplayModeID = displayMode;
	pixelParams.csDepthMode = depth + kDepthMode1;
	pixelParams.csVPBlockPtr = &pixelInfo;
	err = doStatus( cscGetVideoParameters, &pixelParams );
	if( err)
	    continue;

	//info->flags = kFramebufferSupportsCopybackCache;    

	info->activeWidth	= pixelInfo.vpBounds.right;
	info->activeHeight	= pixelInfo.vpBounds.bottom;
	info->bytesPerRow       = pixelInfo.vpRowBytes & 0x7fff;
	info->bytesPerPlane	= pixelInfo.vpPlaneBytes;
	info->bitsPerPixel 	= pixelInfo.vpPixelSize;

        formats = getPixelFormats();
        mask = getPixelFormatsForDisplayMode( displayMode, depth );

        for( index = 0; index < 32; index++) {
            if( (mask & (1 << index)) && ((aperture--) == 0)) {
                strcpy( info->pixelFormat, formats);
                break;
            }
            formats += strlen( formats) + 1;
        }

        if( 0 == strcmp("PPPPPPPP", info->pixelFormat)) {
            info->pixelType = kIOCLUTPixels;
            info->componentMasks[0] = 0xff;
            info->bitsPerPixel = 8;
            info->componentCount = 1;
            info->bitsPerComponent = 8;

        } else if( 0 == strcmp("-RRRRRGGGGGBBBBB", info->pixelFormat)) {
            info->pixelType = kIORGBDirectPixels;
            info->componentMasks[0] = 0x7c00;
            info->componentMasks[1] = 0x03e0;
            info->componentMasks[2] = 0x001f;
            info->bitsPerPixel = 16;
            info->componentCount = 3;
            info->bitsPerComponent = 5;

        } else if( 0 == strcmp("--------RRRRRRRRGGGGGGGGBBBBBBBB",
                                        info->pixelFormat)) {
            info->pixelType = kIORGBDirectPixels;
            info->componentMasks[0] = 0x00ff0000;
            info->componentMasks[1] = 0x0000ff00;
            info->componentMasks[2] = 0x000000ff;
            info->bitsPerPixel = 32;
            info->componentCount = 3;
            info->bitsPerComponent = 8;
        }

    } while( false);

    return( err);
}

IOReturn IONDRVFramebuffer::getTimingInfoForDisplayMode(
		IODisplayModeID displayMode, IOTimingInformation * info )
{
    VDTimingInfoRec		timingInfo;
    OSStatus			err;

    timingInfo.csTimingMode = displayMode;
    // in case the driver doesn't do it:
    timingInfo.csTimingFormat = kDeclROMtables;
    err = doStatus( cscGetModeTiming, &timingInfo);
    if( err == noErr) {
	if( timingInfo.csTimingFormat == kDeclROMtables)
	    info->appleTimingID = timingInfo.csTimingData;
	else
	    info->appleTimingID = timingInvalid;

	return( kIOReturnSuccess);
    }

    return( kIOReturnUnsupportedMode);
}

IOReturn IONDRVFramebuffer::getCurrentDisplayMode( 
				IODisplayModeID * displayMode, IOIndex * depth )
{
    if( displayMode)
	*displayMode = currentDisplayMode;
    if( depth)
	*depth = currentDepth;

    return( kIOReturnSuccess);
}

IOReturn IONDRVFramebuffer::setDisplayMode( IODisplayModeID displayMode, IOIndex depth )
{
    SInt32		err;
    VDSwitchInfoRec	switchInfo;
    VDPageInfo		pageInfo;

    switchInfo.csData = displayMode;
    switchInfo.csMode = depth + kDepthMode1;
    switchInfo.csPage = 0;
    err = doControl( cscSwitchMode, &switchInfo);
    if(err)
	IOLog("%s: cscSwitchMode:%d\n", nub->getName(), (int)err);

    // duplicate QD InitGDevice
    pageInfo.csMode = switchInfo.csMode;
    pageInfo.csData = 0;
    pageInfo.csPage = 0;
    doControl( cscSetMode, &pageInfo);
    doControl( cscGrayPage, &pageInfo);

    getCurrentConfiguration();

    return( err);
}

IOReturn IONDRVFramebuffer::setStartupDisplayMode(
			IODisplayModeID displayMode, IOIndex depth )
{
    SInt32		err;
    VDSwitchInfoRec	switchInfo;

    switchInfo.csData = displayMode;
    switchInfo.csMode = depth + kDepthMode1;
    err = doControl( cscSavePreferredConfiguration, &switchInfo);
    return( err);
}

IOReturn IONDRVFramebuffer::getStartupDisplayMode(
				IODisplayModeID * displayMode, IOIndex * depth )
{
    SInt32		err;
    VDSwitchInfoRec	switchInfo;

    err = doStatus( cscGetPreferredConfiguration, &switchInfo);
    if( err == noErr) {
	*displayMode	= switchInfo.csData;
	*depth		= switchInfo.csMode - kDepthMode1;
    }
    return( err);
}

IOReturn IONDRVFramebuffer::setApertureEnable( IOPixelAperture /* aperture */,
						IOOptionBits /* enable */ )
{
    return( kIOReturnSuccess);
}

IOReturn IONDRVFramebuffer::setCLUTWithEntries(
			IOColorEntry * colors, UInt32 index, UInt32 numEntries,
			IOOptionBits options )
{
    IOReturn		err;
    UInt32		code;
    VDSetEntryRecord	setEntryRec;
    VDClutBehavior	clutSetting;
    VDGrayRecord	grayRec;

    if( options & kSetCLUTWithLuminance)
        grayRec.csMode = 1;		// turn on luminance map
    else
        grayRec.csMode = 0;		// turn off luminance map

    if( grayRec.csMode != lastGrayMode) {
	doControl( cscSetGray, &grayRec);
	lastGrayMode = grayRec.csMode;
    }

    if( options & kSetCLUTImmediately)
        clutSetting = kSetClutAtSetEntries;
    else
        clutSetting = kSetClutAtVBL;

    if( clutSetting != lastClutSetting) {
	doControl( cscSetClutBehavior, &clutSetting);
	lastClutSetting = clutSetting;
    }

    if( options & kSetCLUTByValue)
        setEntryRec.csStart = -1;
    else
        setEntryRec.csStart = index;

    setEntryRec.csTable = (ColorSpec *) colors;
    setEntryRec.csCount = numEntries - 1;
    if( directMode)
        code = cscDirectSetEntries;
    else
        code = cscSetEntries;
    err = doControl( code, &setEntryRec);

    return( err);
}

IOReturn IONDRVFramebuffer::setGammaTable( UInt32 channelCount, UInt32 dataCount,
                                            UInt32 dataWidth, void * data )
{
    IOReturn		err;
    VDGammaRecord	gammaRec;
    struct GammaTbl {
        short gVersion;		/*gamma version number*/
        short gType;		/*gamma data type*/
        short gFormulaSize;	/*Formula data size*/
        short gChanCnt;		/*number of channels of data*/
        short gDataCnt;		/*number of values/channel*/
        short gDataWidth;	/*bits/corrected value */
				/* (data packed to next larger byte size)*/
        UInt8 gFormulaData[0];	/*data for formulas followed by gamma values*/
    };
    GammaTbl * 	table = NULL;
    IOByteCount	dataLen = 0;

    if( data) {
        dataLen = (dataWidth + 7) / 8;
        dataLen *= dataCount * channelCount;
        table = (GammaTbl *) IOMalloc( dataLen + sizeof( struct GammaTbl));
        if( NULL == table)
            return( kIOReturnNoMemory);

	table->gVersion		= 0;
	table->gType		= 0;
	table->gFormulaSize	= 0;
	table->gChanCnt		= channelCount;
	table-> gDataCnt	= dataCount;
	table->gDataWidth	= dataWidth;
	bcopy( data, table->gFormulaData, dataLen);
    }

    gammaRec.csGTable = (Ptr) table;
    err = doControl( cscSetGamma, &gammaRec);
    if( table)
        IOFree( table, dataLen + sizeof( struct GammaTbl));

    return( err);
}

IOReturn IONDRVFramebuffer::getAttribute( IOSelect attribute, UInt32 * value )
{
    IOReturn			err = kIOReturnSuccess;
    VDSupportsHardwareCursorRec	hwCrsrSupport;

    switch( attribute ) {

	case kIOHardwareCursorAttribute:

	    *value = ((kIOReturnSuccess ==
			doStatus( cscSupportsHardwareCursor, &hwCrsrSupport))
                    && (hwCrsrSupport.csSupportsHardwareCursor));
	    break;

	default:
	    err = super::getAttribute( attribute, value );
    }

    return( err );
}

UInt32 IONDRVFramebuffer::getConnectionCount( void )
{
    VDMultiConnectInfoRec	theRecord;

    if( doStatus(cscGetMultiConnect,&theRecord) == 0 ) {
        return theRecord.csDisplayCountOrNumber;
    }
    return 1;
}

IOReturn IONDRVFramebuffer::setAttributeForConnection( IOIndex connectIndex,
                                         IOSelect attribute, UInt32  info )
{
    IOReturn	ret;
    VDSyncInfoRec	theVDSyncInfoRec;

    switch( attribute ) {

        case kConnectionSyncEnable:
            theVDSyncInfoRec.csMode = (unsigned char)(info>>8);
            theVDSyncInfoRec.csFlags = (unsigned char)(info & 0xFF);
            doControl(cscSetSync,&theVDSyncInfoRec);
            ret = kIOReturnSuccess;
            break;
        default:
            ret = super::setAttributeForConnection( connectIndex,
					attribute, info );
            break;
    }
    return( ret );
}

            
IOReturn IONDRVFramebuffer::getAttributeForConnection( IOIndex connectIndex,
                                         IOSelect attribute, UInt32  * value )
{
    IOReturn	ret;
    VDSyncInfoRec	theVDSyncInfoRec;
    
    switch( attribute ) {

        case kConnectionSyncFlags:
            // find out current state of sync lines
            theVDSyncInfoRec.csMode = 0x00;
            doStatus(cscGetSync,&theVDSyncInfoRec);
            * value = theVDSyncInfoRec.csMode;
            ret = kIOReturnSuccess;
            break;
        case kConnectionSyncEnable:
            // what are the sync-controlling capabilities of the ndrv?
            theVDSyncInfoRec.csMode = 0xFF;
            doStatus(cscGetSync,&theVDSyncInfoRec);
            * value = (UInt32)theVDSyncInfoRec.csMode;
            ret = kIOReturnSuccess;
            break;
        case kConnectionSupportsHLDDCSense:
        case kConnectionSupportsAppleSense:
            ret = kIOReturnSuccess;
            break;
        default:
            ret = super::getAttributeForConnection( connectIndex,
				attribute, value );
            break;
    }

    return( ret );
}

IOReturn IONDRVFramebuffer::getAppleSense( IOIndex  connectIndex,
                                            UInt32 * senseType,
                                            UInt32 * primary,
                                            UInt32 * extended,
                                            UInt32 * displayType )
{
    OSStatus		err;
    VDMultiConnectInfoRec	multiConnect;
    UInt32			sense, extSense;

    if( connectIndex == 0 )
        err = doStatus( cscGetConnection, &multiConnect.csConnectInfo);

    else {
        multiConnect.csDisplayCountOrNumber = connectIndex;
        err = doControl( cscSetMultiConnect, &multiConnect);
    }
    if( err)
	return( err);

    if( multiConnect.csConnectInfo.csConnectFlags 
      & ((1<<kReportsTagging) | (1<<kTaggingInfoNonStandard))
	!= ((1<<kReportsTagging)) )

	err = kIOReturnUnsupported;

    else {

        sense 		= multiConnect.csConnectInfo.csConnectTaggedType;
        extSense 	= multiConnect.csConnectInfo.csConnectTaggedData;
	// bug fixes for really old ATI driver
        if( sense == 0) {
            if( extSense == 6) {
                sense          	= kRSCSix;
                extSense        = kESCSixStandard;
            }
            else
                if( extSense == 4) {
                sense		= kRSCFour;
                extSense        = kESCFourNTSC;
                }
            }
        if( primary)
            *primary = sense;
        if( extended)
            *extended = extSense;
        if( displayType)
            *displayType = multiConnect.csConnectInfo.csDisplayType;
        if( senseType)
            *senseType = 0;
    }
    return( err);
}

IOReturn IONDRVFramebuffer::connectFlags( IOIndex /* connectIndex */,
                             IODisplayModeID displayMode, IOOptionBits * flags )
{
    VDTimingInfoRec		timingInfo;
    OSStatus			err;

    timingInfo.csTimingMode = displayMode;
    // in case the driver doesn't do it:
    timingInfo.csTimingFormat = kDeclROMtables;
    err = doStatus( cscGetModeTiming, &timingInfo);
    *flags = timingInfo.csTimingFlags;
    return( err );
}


bool IONDRVFramebuffer::hasDDCConnect( IOIndex  connectIndex )
{
    OSStatus		err;
    VDMultiConnectInfoRec	multiConnect;
    enum		{	kNeedFlags = (1<<kReportsDDCConnection)
					   | (1<<kHasDDCConnection) };
    if( connectIndex == 0 )
        err = doStatus( cscGetConnection, &multiConnect.csConnectInfo);
    else {
        multiConnect.csDisplayCountOrNumber = connectIndex;
        err = doControl( cscSetMultiConnect, &multiConnect);
    }
    if( err)
        return( err);

    return( (multiConnect.csConnectInfo.csConnectFlags & kNeedFlags)
		== kNeedFlags );
}

IOReturn IONDRVFramebuffer::getDDCBlock( IOIndex /* connectIndex */, 
					UInt32 blockNumber,
                                        IOSelect blockType,
					IOOptionBits options,
                                        UInt8 * data, IOByteCount * length )

{
    OSStatus		err = 0;
    VDDDCBlockRec	ddcRec;
    ByteCount		actualLength = *length;

    ddcRec.ddcBlockNumber 	= blockNumber;
    ddcRec.ddcBlockType 	= blockType;
    ddcRec.ddcFlags 		= options;

    err = doStatus( cscGetDDCBlock, &ddcRec);

    if( err == noErr) {

	if( actualLength < kDDCBlockSize)
            actualLength = actualLength;
	else
            actualLength = kDDCBlockSize;
        bcopy( ddcRec.ddcBlockData, data, actualLength);
	*length = actualLength;
    }
    return( err);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
// initForPM
//
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
void IONDRVFramebuffer::initForPM ( void )
{
    // register ourselves with superclass policy-maker
    registerControllingDriver(this,ourPowerStates,number_of_power_states);
}


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
// maxCapabilityForDomainState
//
// This simple device needs only power.  If the power domain is supplying
// power, the frame buffer can be on.  If there is no power it can only be off.
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

unsigned long  IONDRVFramebuffer::maxCapabilityForDomainState(
					IOPMPowerFlags domainState )
{
   if( domainState &  IOPMPowerOn )
       return number_of_power_states-1;
   else
       return 0;
}


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
// initialPowerStateForDomainState
//
// The power domain may be changing state.  If power is on in the new
// state, that will not affect our state at all.  If domain power is off,
// we can attain only our lowest state, which is off.
//
// This implementation is incomplete.  It only works in a system where
// the frame buffer is never turned off.  When we cross that bridge,
// instead of returning 1, it should return 1 if the frame buffer
// is on, or 0 if it is off.
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
unsigned long IONDRVFramebuffer::initialPowerStateForDomainState(
					 IOPMPowerFlags domainState )
{
   if( domainState &  IOPMPowerOn )
       return number_of_power_states-1;
   else
       return 0;
}


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
// powerStateForDomainState
//
// The power domain may be changing state.  If power is on in the new
// state, that will not affect our state at all.  If domain power is off,
// we can attain only our lowest state, which is off.
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
unsigned long  IONDRVFramebuffer::powerStateForDomainState(
					IOPMPowerFlags domainState )
{
   if( domainState &  IOPMPowerOn )
       return pm_vars->myCurrentState;
   else
       return 0;
}


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
// setPowerState
//
// Called by the superclass to turn the frame buffer on and off.
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
IOReturn IONDRVFramebuffer::setPowerState( unsigned long powerStateOrdinal,
						IOService* whatDevice )
{
    if ( powerStateOrdinal == 0 ) {
        kprintf("frame buffer would be powered off here\n");
    }
    if ( powerStateOrdinal == 1 ) {
        kprintf("frame buffer would be powered on here\n");
    }
    return IOPMAckImplied;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

// ATI patches.
// Real problem : getStartupMode doesn't.

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#undef super
#define super IONDRVFramebuffer

OSDefineMetaClassAndStructors(IOATINDRV, IONDRVFramebuffer)
OSDefineMetaClassAndStructors(IOATI128NDRV, IOATINDRV)

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOReturn IOATINDRV::getStartupDisplayMode(
				IODisplayModeID * displayMode, IOIndex * depth )
{
    UInt16 *		nvram;
    OSData *		prop;

    prop = OSDynamicCast( OSData, nub->getProperty("Sime"));
    if( prop) {
	nvram = (UInt16 *) prop->getBytesNoCopy();
	*displayMode = nvram[ 0 ];	// 1 is physDisplayMode
	*depth = nvram[ 2 ] - kDepthMode1;
        return( kIOReturnSuccess);
    } else
        return(super::getStartupDisplayMode( displayMode, depth));
}

IODeviceMemory * IOATINDRV::findVRAM( void )
{
    OSData *		prop;
    IOPhysicalAddress	vramBase;
    IOByteCount		vramLength;

    prop = OSDynamicCast( OSData, nub->getProperty("APPL,VRAMSize"));
    if( !prop)
	prop = OSDynamicCast( OSData, nub->getProperty("ATY,memsize"));
    if( !prop)
	return( super::findVRAM());

    vramBase = physicalFramebuffer;
    vramLength = *((IOByteCount *)prop->getBytesNoCopy());
    if( !vramLength)
        return( super::findVRAM());
    vramLength = (vramLength + (vramBase & 0xffff)) & 0xffff0000;
    vramBase &= 0xffff0000;

    return( makeSubRange( vramBase, vramLength ));
}

static int g128ExtraCurs = 8;
static int g128DeltaCurs = 0x25c0;

void IOATI128NDRV::flushCursor( void )
{
    volatile UInt32 *	fb;
    UInt32		x;
    int			i;

    fb = (volatile UInt32 *) frameBuffer;
    for( i = 0; i < g128ExtraCurs; i++) {
	x += *(fb++);
	fb += g128DeltaCurs;
    }
}


    
