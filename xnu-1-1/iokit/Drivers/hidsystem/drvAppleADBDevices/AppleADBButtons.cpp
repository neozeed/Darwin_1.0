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
#include "AppleADBButtons.h"
#include <IOKit/IOLib.h>
#include <IOKit/pwr_mgt/IOPM.h>
#include <IOKit/hidsystem/IOHIDTypes.h>
#include <IOKit/hidsystem/IOHIDParameter.h>

#define super IOHIKeyboard
OSDefineMetaClassAndStructors(AppleADBButtons,IOHIKeyboard)

bool displayWranglerFound( OSObject *, void *, IOService * );
void button_data ( IOService * us, UInt8 adbCommand, IOByteCount length, UInt8 * data );
void asyncFunc ( void * );

// **********************************************************************************
// start
//
// **********************************************************************************
bool AppleADBButtons::start ( IOService * theNub )
{
    int i;

    for ( i = 0; i < kMax_registrations; i++ ) {
        keycodes[i] = kNullKey;
        downHandlers[i] = NULL;
    }
    
    if( !super::start(theNub))
        return false;

    adbDevice = (IOADBDevice *)theNub;
    if( !adbDevice->seizeForClient(this, button_data) ) {
        IOLog("%s: Seize failed\n", getName());
        return false;
    }
    registerService();

    addNotification( gIOPublishNotification,serviceMatching("IODisplayWrangler"),	// look for the display wrangler
                     (IOServiceNotificationHandler)displayWranglerFound, this, 0 );

return true;
}


// **********************************************************************************
// displayWranglerFound
//
// The Display Wrangler has appeared.  We will be calling its
// ActivityTickle method when there is user activity.
// **********************************************************************************
bool displayWranglerFound( OSObject * us, void * ref, IOService * yourDevice )
{
 if ( yourDevice != NULL ) {
     ((AppleADBButtons *)us)->displayManager = yourDevice;
 }
 return true;
}

UInt32 AppleADBButtons::interfaceID()
{
    return NX_EVS_DEVICE_INTERFACE_ADB;
}

UInt32 AppleADBButtons::deviceType()
{
    return adbDevice->handlerID();
}

// **********************************************************************************
// registerForButton
//
// Clients call here, specifying a button and a routine to call when that
// button is pressed or released.
// **********************************************************************************
IOReturn AppleADBButtons::registerForButton ( unsigned int keycode, IOService * registrant, button_handler handler, bool down )
{
    int i;

    for ( i = 0; i < kMax_registrations; i++ ) {
        if ( keycodes[i] == kNullKey ) {
            if ( down ) {
                registrants[i] = registrant;
                downHandlers[i] = handler;
                keycodes[i] = keycode;
                break;
            }
        }
    }
    return kIOReturnSuccess;
}

// **********************************************************************************
// button_data
//
// **********************************************************************************
void button_data ( IOService * us, UInt8 adbCommand, IOByteCount length, UInt8 * data )
{
((AppleADBButtons *)us)->packet(data,length,adbCommand);
}


// **********************************************************************************
// packet
//
// **********************************************************************************
IOReturn AppleADBButtons::packet (UInt8 * data, IOByteCount, UInt8 adbCommand )
{
    unsigned int	keycode;
    bool		down;

    keycode = *data;
    down = ((keycode & 0x80) == 0);
    keycode &= 0x7f;

    dispatchButtonEvent(keycode,down);

    keycode = *(data + 1);
    if( keycode != 0xff ) {
        down = ((keycode & 0x80) == 0);
        keycode &= 0x7f;
        dispatchButtonEvent(keycode,down);
    }
    
    if ( displayManager != NULL ) {			// if there is a display manager, tell
        displayManager->activityTickle(kIOPMSuperclassPolicy1);	// it there is user activity
    }
    
    return kIOReturnSuccess;
}


// **********************************************************************************
// dispatchButtonEvent
//
// Look for any registered handlers for this button and notify them.
// **********************************************************************************
void AppleADBButtons::dispatchButtonEvent (unsigned int keycode, bool down )
{
    int i;
    AbsoluteTime now;

    clock_get_uptime(&now);
    
    for ( i = 0; i < kMax_registrations; i++ ) {
        if ( keycodes[i] == keycode ) {
            if ( down ) {
                if (downHandlers[i] != NULL ) {
                    thread_call_func((thread_call_func_t)downHandlers[i],
                                     (thread_call_param_t)registrants[i],
                                     true);
                }
            }
        }
    }

    dispatchKeyboardEvent(keycode, down, now);
}

const unsigned char *AppleADBButtons::defaultKeymapOfLength(UInt32 *length)
{
    static const unsigned char appleADBButtonsKeyMap[] = {
        0x00, 0x00,	// chars
        0x00,		// no modifier keys
        0x00,		// no defs
        0x00,		// no seqs
        0x05,		// 5 special keys
        NX_KEYTYPE_SOUND_UP, kVolume_up,
        NX_KEYTYPE_SOUND_DOWN, kVolume_down,
        NX_KEYTYPE_MUTE, kMute,
        NX_KEYTYPE_BRIGHTNESS_UP, kBrightness_up,
        NX_KEYTYPE_BRIGHTNESS_DOWN, kBrightness_down
    };
    
    *length = sizeof(appleADBButtonsKeyMap);
    
    return appleADBButtonsKeyMap;
}

IOReturn AppleADBButtons::setParamProperties(OSDictionary *dict)
{
    if (dict->getObject(kIOHIDKeyMappingKey)) {
        dict->removeObject(kIOHIDKeyMappingKey);
        // Do we also need to release this object? - hopefully not
    }

    return super::setParamProperties(dict);
}
