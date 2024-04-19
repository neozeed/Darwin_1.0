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
 * 17 July 1998 sdouglas
 */

#include <IOKit/IOLib.h>
#include <IOKit/assert.h>
#include <IOKit/hidsystem/IOHIPointing.h>
#include <IOKit/hidsystem/IOHIDParameter.h>

#ifndef abs
#define abs(_a)	((_a >= 0) ? _a : -_a)
#endif

#define super IOHIDevice
OSDefineMetaClassAndStructors(IOHIPointing, IOHIDevice);

bool IOHIPointing::init(OSDictionary * properties)
{
  if (!super::init(properties))  return false;

  /*
   * Initialize minimal state.
   */

  _fractX	= 0;
  _fractY     	= 0;
  _acceleration	= -1;
  _convertAbsoluteToRelative = false;
  _contactToMove = false;
  _hadContact = false;
  _pressureThresholdToClick = 128;
  _previousLocation.x = 0;
  _previousLocation.y = 0;

  _deviceLock = IOLockAlloc();

  if (!_deviceLock)  return false;

  IOLockInit(_deviceLock);

  return true;
}

bool IOHIPointing::start(IOService * provider)
{
  if (!super::start(provider))  return false;

  /*
   * IOHIPointing serves both as a service and a nub (we lead a double
   * life).  Register ourselves as a nub to kick off matching.
   */

  registerService();

  return true;
}

void IOHIPointing::free()
// Description:	Go Away. Be careful when freeing the lock.
{
    if (_deviceLock)
    {
	IOLock * lock;

	IOTakeLock(_deviceLock);

	lock = _deviceLock;
	_deviceLock = NULL;

	IOUnlock(lock);
	IOLockFree(lock);
    }
    super::free();
}

bool IOHIPointing::open(IOService *                client,
			IOOptionBits	           options,
                        RelativePointerEventAction rpeAction,
                        AbsolutePointerEventAction apeAction,
                        ScrollWheelEventAction     sweAction)
{
  if ( (-1 == _acceleration) && (!resetPointer()))  return false;

  if (super::open(client, options))
  {
    // Note: client object is already retained by superclass' open()
    _relativePointerEventTarget = client;
    _relativePointerEventAction = rpeAction;
    _absolutePointerEventTarget = client;
    _absolutePointerEventAction = apeAction;
    _scrollWheelEventTarget = client;
    _scrollWheelEventAction = sweAction;
    return true;
  }

  return false;
}

void IOHIPointing::close(IOService * client, IOOptionBits)
{
  _relativePointerEventAction = NULL;
  _relativePointerEventTarget = 0;
  _absolutePointerEventAction = NULL;
  _absolutePointerEventTarget = 0;

  super::close(client);
} 

IOHIDKind IOHIPointing::hidKind()
{
  return kHIRelativePointingDevice;
}

void IOHIPointing::scalePointer(int * dxp, int * dyp)
// Description:	Perform pointer acceleration computations here.
//		Given the resolution, dx, dy, and time, compute the velocity
//		of the pointer over a Manhatten distance in inches/second.
//		Using this velocity, do a lookup in the pointerScaling table
//		to select a scaling factor. Scale dx and dy up as appropriate.
// Preconditions:
// *	_deviceLock should be held on entry
{
    int dx, dy;
    int absDx, absDy;
    unsigned delta;

    dx = *dxp;
    dy = *dyp;
    absDx = (dx < 0) ? -dx : dx;
    absDy = (dy < 0) ? -dy : dy;

#if 1
    if( absDx > absDy)
	delta = (absDx + (absDy / 2));
    else
	delta = (absDy + (absDx / 2));
#else
    delta = absDx + absDy;
#endif

    // scale
    if( delta > (MAXMAG - 1))
	delta = MAXMAG - 1;
    dx = IOFixedMultiply( dx << 16, _scaleValues[delta]);
    dy = IOFixedMultiply( dy << 16, _scaleValues[delta]);

    // if no direction changes add fract parts
    if( (dx ^ _fractX) >= 0)
	dx += _fractX;
    if( (dy ^ _fractY) >= 0)
	dy += _fractY;

    *dxp = dx / 65536;
    *dyp = dy / 65536;

    // get fractional part with sign extend
    if( dx >= 0)
	_fractX = dx & 0xffff;
    else
	_fractX = dx | 0xffff0000;
    if( dy >= 0)
	_fractY = dy & 0xffff;
    else
	_fractY = dy | 0xffff0000;
}


void IOHIPointing::setupForAcceleration(IOFixed accl)
{
    int				slope = 0, i, j = 0;
    int				x;
    int 			devSpeed, crsrSpeed;
    int 			lastCrsrSpeed = 0, nextCrsrSpeed;
    int 			lastDeviceSpeed = 0, nextDeviceSpeed;
    IOHIAccelerationPoint *	table;
    IOItemCount			numEntries;
    IOFixed                     res = resolution();

    assert(res);

    accelerationTable(&table, &numEntries);

    if( accl > (1 << 16))
	accl = (1 << 16);

    // scale for device speed
    // no vbl sync, so 90 autopolls /s
    devSpeed = IOFixedDivide( 90 << 16, res );
    // scale for cursor speed
    // screen is assumed 72 dpi, like MacOS
    crsrSpeed = IOFixedDivide( 72 << 16, res );

    nextCrsrSpeed = 0;
    nextDeviceSpeed = 0;

    // Precalculate fixed point scales. Not as accurate as MacOS,
    //	but no IOFixedDivide() in handler
    for ( i = 1; i < MAXMAG; i++ )
    {
        x = IOFixedMultiply( i << 16, devSpeed );
        if( x > table[j].deviceSpeed) {
            lastCrsrSpeed = nextCrsrSpeed;
            lastDeviceSpeed = nextDeviceSpeed;
            j++;
            nextDeviceSpeed = table[j].deviceSpeed;
            // Interpolate by accl between y=x and y=acclTable(x)
	    //  to get nextCrsrSpeed
            {
                int factorCursor;
                int factorDevice;
                if( table[j].cursorSpeed < nextDeviceSpeed) {
                    factorDevice = accl;
                    factorCursor = (0x10000 - accl);
                } else {
                    factorCursor = accl;
                    factorDevice = (0x10000 - accl);
                }
                nextCrsrSpeed = IOFixedMultiply( factorCursor, table[j].cursorSpeed)
			      + IOFixedMultiply( factorDevice, nextDeviceSpeed);
            }
            slope = IOFixedDivide( nextCrsrSpeed - lastCrsrSpeed,
			nextDeviceSpeed - lastDeviceSpeed);
        }
        _scaleValues[i] = IOFixedDivide( IOFixedMultiply( crsrSpeed,
                    IOFixedMultiply( slope, x - lastDeviceSpeed ) + lastCrsrSpeed), x);
    }
    _scaleValues[0] = _scaleValues[1];
    _fractX = _fractY = 0;
    _acceleration = accl;
}

bool IOHIPointing::resetPointer()
{
    IOTakeLock( _deviceLock);

    _buttonMode = NX_OneButton;

    setupForAcceleration(0x8000);
    updateProperties();

    IOUnlock( _deviceLock);
    return true;
}

void IOHIPointing::dispatchAbsolutePointerEvent(Point *		newLoc,
                                                Bounds *	bounds,
                                                UInt32		buttonState,
                                                bool		proximity,
                                                int		pressure,
                                                int		pressureMin,
                                                int		pressureMax,
                                                int		stylusAngle,
                                                AbsoluteTime	ts)
{
    int buttons = 0;
    int dx, dy;
    
    IOTakeLock(_deviceLock);

    if (buttonState & 1) {
        buttons |= EV_LB;
    }

    if (buttonCount() > 1) {
        if (buttonState & -2) {	// any other buttons
            buttons |= EV_RB;
        }
    }

    if ((_pressureThresholdToClick < 255) && ((pressure - pressureMin) > ((pressureMax - pressureMin) * _pressureThresholdToClick / 256))) {
        buttons |= EV_LB;
    }

    if (_buttonMode == NX_OneButton) {
        if ((buttons & (EV_LB|EV_RB)) != 0) {
            buttons = EV_LB;
        }
    }

    if (_convertAbsoluteToRelative) {
        dx = newLoc->x - _previousLocation.x;
        dy = newLoc->y - _previousLocation.y;
        
        if ((_contactToMove && !_hadContact && (pressure > pressureMin)) || (abs(dx) > ((bounds->maxx - bounds->minx) / 20)) || (abs(dy) > ((bounds->maxy - bounds->miny) / 20))) {
            dx = 0;
            dy = 0;
        } else {
            scalePointer(&dx, &dy);
        }
        
        _previousLocation.x = newLoc->x;
        _previousLocation.y = newLoc->y;
    }

    IOUnlock(_deviceLock);

    _hadContact = (pressure > pressureMin);

    if (!_contactToMove || (pressure > pressureMin)) {
        pressure -= pressureMin;
        if (pressure > 255) {
            pressure = 255;
        }
        if (_convertAbsoluteToRelative) {
            if (_relativePointerEventAction && _relativePointerEventTarget) {
                (*_relativePointerEventAction)(_relativePointerEventTarget,
                                              buttons,
                                              dx,
                                              dy,
                                              ts);
            }
        } else {
            if (_absolutePointerEventAction && _absolutePointerEventTarget) {
                (*_absolutePointerEventAction)(_absolutePointerEventTarget,
                                              buttons,
                                              newLoc,
                                              bounds,
                                              proximity,
                                              pressure,
                                              stylusAngle,
                                              ts);
            }
        }
    }

    return;
}

void IOHIPointing::dispatchRelativePointerEvent(int        dx,
                                                int        dy,
                                                UInt32     buttonState,
                                                AbsoluteTime ts)
{
    int buttons;

    IOTakeLock( _deviceLock);

    buttons = 0;

    if( buttonState & 1)
        buttons |= EV_LB;

    if( buttonCount() > 1) {
	if( buttonState & -2)	// any others down
	    buttons |= EV_RB;
    }

    // Perform pointer acceleration computations
    scalePointer(&dx, &dy);

    // Perform button tying and mapping.  This
    // stuff applies to relative posn devices (mice) only.
    if ( _buttonMode == NX_OneButton )
    {
	if ( (buttons & (EV_LB|EV_RB)) != 0 )
	    buttons = EV_LB;
    }
    else if ( (buttonCount() > 1) && (_buttonMode == NX_LeftButton) )	
    // Menus on left button. Swap!
    {
	int temp = 0;
	if ( buttons & EV_LB )
	    temp = EV_RB;
	if ( buttons & EV_RB )
	    temp |= EV_LB;
	buttons = temp;
    }
    IOUnlock( _deviceLock);

    if (_relativePointerEventAction)          /* upstream call */
    {
      (*_relativePointerEventAction)(_relativePointerEventTarget,
                       /* buttons */ buttons,
                       /* deltaX */  dx,
                       /* deltaY */  dy,
                       /* atTime */  ts);
    }
}

void IOHIPointing::dispatchScrollWheelEvent(short deltaAxis1,
                                            short deltaAxis2,
                                            short deltaAxis3,
                                            AbsoluteTime ts)
{
    if (_scrollWheelEventAction) {
        (*_scrollWheelEventAction)(_scrollWheelEventTarget,
                                   deltaAxis1,
                                   deltaAxis2,
                                   deltaAxis3,
                                   ts);
    }
}

bool IOHIPointing::updateProperties( void )
{
    bool	ok;
    UInt32	res = resolution();

    ok = setProperty( kIOHIDPointerResolutionKey, &res, sizeof( res))
    &    setProperty( kIOHIDPointerAccelerationKey, &_acceleration,
                        sizeof( _acceleration))
    &    setProperty( kIOHIDPointerConvertAbsoluteKey, &_convertAbsoluteToRelative,
                        sizeof( _convertAbsoluteToRelative))
    &    setProperty( kIOHIDPointerContactToMoveKey, &_contactToMove,
                        sizeof( _contactToMove));

    return( ok & super::updateProperties() );
}

IOReturn IOHIPointing::setParamProperties( OSDictionary * dict )
{
    OSData *	data;
    IOReturn	err = kIOReturnSuccess;
    bool	updated = false;
    UInt8 *	bytes;

    IOTakeLock( _deviceLock);
    if( (data = OSDynamicCast( OSData,
		dict->getObject(kIOHIDPointerAccelerationKey)))) {

	setupForAcceleration( *((IOFixed *)data->getBytesNoCopy()) );
	updated = true;
    }
    IOUnlock( _deviceLock);

    if( dict->getObject(kIOHIDResetPointerKey))
	resetPointer();

    if ((data = OSDynamicCast(OSData,
                              dict->getObject(kIOHIDPointerConvertAbsoluteKey)))) {
        bytes = (UInt8 *) data->getBytesNoCopy();
        _convertAbsoluteToRelative = (bytes[0] != 0) ? true : false;
        updated = true;
    }

    if ((data = OSDynamicCast(OSData,
                              dict->getObject(kIOHIDPointerContactToMoveKey)))) {
        bytes = (UInt8 *) data->getBytesNoCopy();
        _contactToMove = (bytes[0] != 0) ? true : false;
        updated = true;
    }

    if( updated )
        updateProperties();

    return( err );
}

// subclasses override

IOItemCount IOHIPointing::buttonCount()
{
    return (1);
}

IOFixed IOHIPointing::resolution()
{
    return (100 << 16);
}

void IOHIPointing::accelerationTable(IOHIAccelerationPoint ** table,
	                             IOItemCount * numEntries)
{
    static IOHIAccelerationPoint defaultTable[] = {
	{ 0x000000, 0x000000 },
	{ 0x7ffffff, 0x0960000 }
    };

    *table = defaultTable;
    *numEntries = sizeof( defaultTable) / sizeof( defaultTable[0]);
}
