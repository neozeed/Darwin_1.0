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
 * Copyright (c) 1998 Apple Computer, Inc.  All rights reserved.
 *
 * HISTORY
 *
 */


#ifndef _IOKIT_APPLEKEYBOARD_H
#define _IOKIT_APPLEKEYBOARD_H

#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>

#include <IOKit/usb/IOUSBBus.h>
#include <IOKit/usb/IOUSBDevice.h>
#include <IOKit/usb/USB.h>
#include <IOKit/hidsystem/IOHIKeyboard.h>
#include <IOKit/IOBufferMemoryDescriptor.h>

enum {
    kUSB_LEFT_CONTROL_BIT = 0x01,
    kUSB_LEFT_SHIFT_BIT = 0x02,
    kUSB_LEFT_ALT_BIT = 0x04,
    kUSB_LEFT_FLOWER_BIT = 0x08,

    kUSB_RIGHT_CONTROL_BIT = 0x10,
    kUSB_RIGHT_SHIFT_BIT = 0x20,
    kUSB_RIGHT_ALT_BIT = 0x040,
    kUSB_RIGHT_FLOWER_BIT = 0x80
};

enum {
    kUSB_LOWSPEED_MAXPACKET = 8,
    kUSB_CAPSLOCKLED_SET = 2,
    kUSB_NUMLOCKLED_SET = 1
};

class AppleKeyboard : public IOHIKeyboard
{
    OSDeclareDefaultStructors(AppleKeyboard)

   
    IOUSBInterface *		_interface;
    const IOUSBDeviceDescriptor *_deviceDescriptor;
    IOUSBPipe * 		_interruptPipe;
    IOBufferMemoryDescriptor *	_buffer;
    IOUSBCompletion		_completion;
    UInt16			_maxPacketSize;
    UInt8			_capsLockState;
    bool			_prevent_LED_set;
    IOUSBDevRequest  		_request;
    UInt8       		_hid_report[8];
    bool			_flower_key;  //Mac Command key
    bool			_control_key; //Control needed for 3-finger reboot
    UInt8			prev_bytes_read;
    UInt8			oldmodifier;
    UInt8			old_array[kUSB_LOWSPEED_MAXPACKET];


    
    //A.W. The following 5 methods are required for subclasses of IOHIKeyboard
    UInt32 maxKeyCodes ( void );
    const unsigned char * defaultKeymapOfLength (UInt32 * length );
    UInt32 interfaceID ( void );
    UInt32 handlerID ( void );
    UInt32 deviceType ( void );

    void setLEDFeedback ( UInt8 LED_state);



public:

    UInt8			_ledState;
    virtual bool	start(IOService * provider);
    virtual void 	stop(IOService *  provider);
    void                setLEDHandler(void * 	parameter,
                                      IOReturn	status,
                                      UInt32	bufferSizeRemaining);
    void                readHandler(void * 	parameter,
                                    IOReturn	status,
                                    UInt32	bufferSizeRemaining);

    void 		Simulate_ADB_Event(UInt8 *key_ptr, UInt32 bytes_read);
    void		test_myusb_f1(void);
    void   		Set_LED_States( UInt8);

    bool		turnLEDon;	// used by setAlphaLockFeedback mechanism

};

#endif _IOKIT_APPLEKEYBOARD_H
