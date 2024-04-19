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
 * sdouglas  23 Jul 98 - start IOKit
 * sdouglas  08 Dec 98 - start cpp
 */

#include <IOKit/graphics/IODisplay.h>
#include <libkern/c++/OSContainers.h>
#include <IOKit/IOLib.h>

#include <IOKit/assert.h>

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

struct EDID {
    UInt8	header[8];
    UInt8	vendorProduct[4];
    UInt8	serialNumber[4];
    UInt8	weekOfManufacture;
    UInt8	yearOfManufacture;
    UInt8	version;
    UInt8	revision;
    UInt8	displayParams[5];
    UInt8	colorCharacteristics[10];
    UInt8	establishedTimings[3];
    UInt16	standardTimings[8];
    UInt8	detailedTimings[72];
    UInt8	extension;
    UInt8	checksum;
};

struct TimingToEDID {
    UInt32	timingID;
    UInt8	spare;
    UInt8	establishedBit;
    UInt16	standardTiming;
};

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

class AppleDDCDisplay : public IODisplay
{
    OSDeclareDefaultStructors(AppleDDCDisplay)

private:
    OSData *		edidData;
    OSData *		additions;
    TimingToEDID *	timingToEDID;
    int			numEDIDEntries;

public:
    virtual IOService * probe(	IOService * 	provider,
				SInt32 *	score );

    virtual bool start( IOService * provider );

    virtual IOReturn getConnectFlagsForDisplayMode(
		IODisplayModeID mode, UInt32 * flags );
};

#undef super
#define super IODisplay

OSDefineMetaClassAndStructors(AppleDDCDisplay, IODisplay)

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOService * AppleDDCDisplay::probe(	IOService * 	provider,
					SInt32 *	score )
{
    IODisplayConnect *	connect;
    IOFramebuffer *	framebuffer;
    IOService *		ret = 0;

    do {

	if( 0 == super::probe( provider, score ))
            continue;

	connect = getConnection();
	framebuffer = connect->getFramebuffer();
	assert( framebuffer );

        if( kIOReturnSuccess != framebuffer->getAttributeForConnection(
				connect->getConnection(),
				kConnectionSupportsHLDDCSense, NULL ))
            continue;

	if( framebuffer->hasDDCConnect( connect->getConnection()))
            ret = this;

    } while( false);

    return( ret );
}


bool AppleDDCDisplay::start( IOService * provider )
{
    IOReturn		err;
    IODisplayConnect *	connect;
    IOFramebuffer *	framebuffer;
    OSData *		data;
    OSArray *		array;
    OSDictionary *	dict;
    OSNumber *		off;
    IOByteCount		length;
    EDID 		readEDID;
    UInt32		vendorProd;
    UInt32		index;

    if (false == super::start( provider ))
	return( false);

    connect = getConnection();
    framebuffer = connect->getFramebuffer();
    assert( framebuffer );

    do {
	length = sizeof( EDID);
	err = framebuffer->getDDCBlock( connect->getConnection(),
		1, kIODDCBlockTypeEDID, 0, (UInt8 *) &readEDID, &length );
	if( err || (length != sizeof( EDID)))
	    continue;

	kprintf("%s EDID Version %d, Revision %d\n", framebuffer->getName(),
	    readEDID.version, readEDID.revision );
	if( readEDID.version != 1)
	    continue;

    	if( (data = (OSData *) getProperty( "appleDDC" ))) {
	    timingToEDID = (TimingToEDID *) data->getBytesNoCopy();
	    numEDIDEntries = data->getLength() / sizeof(TimingToEDID);
	} else
	   continue;

        setProperty( kIODisplayEDIDKey, &readEDID, sizeof( readEDID));

	vendorProd = (readEDID.vendorProduct[0] << 24)
		   | (readEDID.vendorProduct[1] << 16)
		   | (readEDID.vendorProduct[2] << 8)
		   | (readEDID.vendorProduct[3] << 0);

#if 1
        kprintf("Vendor/product 0x%08x, ", vendorProd );
	kprintf("Est: ");
	for( index = 0; index < 3; index++)
	    kprintf(" 0x%02x,", readEDID.establishedTimings[ index ] );
	kprintf("\nStd: " );
	for( index = 0; index < 8; index++)
	    kprintf(" 0x%04x,", readEDID.standardTimings[ index ] );
	kprintf("\n");
#endif

	data = 0;
	additions = 0;
	if( (array = OSDynamicCast(OSArray, getProperty("overrides")))) {
	    for(   index = 0;
		  (dict = OSDynamicCast(OSDictionary, array->getObject(index)));
		   index++ ) {
		if( 0 == (off = OSDynamicCast(OSNumber, dict->getObject("ID"))))
		    continue;
		if( vendorProd == off->unsigned32BitValue()) {
		    data = OSDynamicCast(OSData,
				dict->getObject( "EDID"));
		    additions = OSDynamicCast(OSData, 
				dict->getObject("additions"));
		    break;
		}
	    }
	}

	if( !data)
	   data = (OSData *) getProperty( kIODisplayEDIDKey );
	if( !data)
	    continue;
        edidData = data;

	return( true);

    } while( false);

    return( false);
}

IOReturn AppleDDCDisplay::getConnectFlagsForDisplayMode(
		IODisplayModeID mode, UInt32 * flags )
{
    IOReturn			err;
    IODisplayConnect *		connect;
    IOFramebuffer *		framebuffer;
    IOTimingInformation 	info;
    const TimingToEDID	*	lookTiming;
    UInt32			estBit, i;
    EDID *			edid;
    UInt32 *			addModes;
    UInt32			numAdditions;
    UInt32			appleTimingID;
    bool			supported = false;
    enum {			kSetFlags = (kDisplayModeValidFlag
					   | kDisplayModeSafeFlag) };


    connect = getConnection();
    framebuffer = connect->getFramebuffer();
    assert( framebuffer );

    if( kIOReturnSuccess != framebuffer->connectFlags( connect->getConnection(),
							mode, flags ))
	*flags = 0;

    err = framebuffer->getTimingInfoForDisplayMode( mode, &info );
    if( err != kIOReturnSuccess)
        return( err);

    appleTimingID = info.appleTimingID;

    if( additions) {
        numAdditions = additions->getLength() / sizeof( UInt32);
        addModes = (UInt32 *) additions->getBytesNoCopy();
        for( i = 0; (!supported) && (i < numAdditions); i++)
            supported = (addModes[ i ] == appleTimingID);
    }

    edid = (EDID *) edidData->getBytesNoCopy();
    assert( edid );
    for( lookTiming = timingToEDID;
        (!supported) && ((lookTiming - timingToEDID) < numEDIDEntries);
        lookTiming++ ) {

        if( lookTiming->timingID == appleTimingID) {
            estBit = lookTiming->establishedBit;
            if( estBit != 0xff)
                supported = (0 != (edid->establishedTimings[ estBit / 8 ]
                                    & (1 << (estBit % 8))));

            for( i = 0; (!supported) && (i < 8); i++ )
                supported = (lookTiming->standardTiming
			     == edid->standardTimings[i] );
        }
    }

    if( supported)
        *flags = kSetFlags;

    // Pass the existing flags (from framebuffer) thru
    return( err);
}

