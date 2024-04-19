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
 * Copyright (c) 1998, 1999 Apple Computer, Inc.  All rights reserved.
 *
 * HISTORY
 *
 */
#include <libkern/OSByteOrder.h>

#include "AppleKeyboard.h"
#include <IOKit/hidsystem/IOHIPointing.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/usb/IOUSBInterface.h>
#include <IOKit/usb/IOUSBPipe.h>
#include <IOKit/usb/IOUSBController.h>

extern "C" {
#include <pexpert/pexpert.h>
}


#define super IOHIKeyboard
#define self this
#define DEBUGGING_LEVEL 0

OSDefineMetaClassAndStructors(AppleKeyboard, IOHIKeyboard)

extern unsigned char usb_2_adb_keymap[];  //In Cosmo_USB2ADB.cpp

static IOUSBController *sPollControl;
static IOUSBPipe *sPollPipe;
static UInt8 sBuffer[8];
static IOMemoryDescriptor *sBufDesc;
static bool sComplete = true;


static void bodgeComplete(void *target, void * parameter, IOReturn status, UInt32 bufferSizeRemaining)
{
    sComplete = true;
}

static int PE_usb_poll_input(unsigned int options, char * c)
{
    static char	adbkeycodes2ascii[] = "asdfhgzxcv_bqwer"//00
                                    "yt123465=97-80]o"	//10
                                    "u[ip\nlj'k;_,/nm."	//20
                                    "\t_";		//30
    *c = 0xff;

    if(!sPollPipe)
	return 0;
    if(sComplete) {
        IOUSBCompletion	completion;
 	sComplete = false;

        *c = adbkeycodes2ascii[usb_2_adb_keymap[sBuffer[2]]];
        completion.target = 0;
        completion.action = bodgeComplete;
        completion.parameter = 0;

        sPollControl->PolledRead(
                    sPollPipe->address()		/*functionAddress*/,
                    sPollPipe->endpoint()->number	/*endpointNumber*/,
                    completion   			/*completion*/,
                    sBufDesc				/*buffer*/,
                    true				/*bufferRounding*/,
                    sizeof(sBuffer)       		/*bufferSize*/);
    }
    else
        sPollControl->pollInterrupts(bodgeComplete);	// Only complete our transactions

    return 0;
}

void usb_asyncLED ( AppleKeyboard * me );
AppleKeyboard	*glob_usbkeyboard_obj;

bool AppleKeyboard::start(IOService * provider)
{
    IOReturn			err = 0;
    
    if( !super::start(provider))
        return (false);

    // remember my device
    _interface		= OSDynamicCast(IOUSBInterface, provider);
    _deviceDescriptor	= _interface->deviceDescriptor();
    _prevent_LED_set    = FALSE;

    //Following 3 lines moved here from Simulate_ADB_Event to make multiple 
    // keyboards work correctly.  The instance variables cannot be shared
    // between each instance of this driver for each USB keyboard.
    prev_bytes_read=8;
    oldmodifier = 0xff;
    bzero( old_array, kUSB_LOWSPEED_MAXPACKET);


    if (_interface->open(self) == false) return false;

    //Fix hardware bug in iMac USB keyboard mapping for ISO keyboards
    //kprintf("\nUSB product = %x, vendor = %x\n", _deviceDescriptor->product, _deviceDescriptor->vendor);
    if ( _deviceDescriptor)	//just to be safe
    {
        if (_deviceDescriptor->vendor == 0xac05)  //Apple Corp. vendor ID, not byte swapped
        {
            switch (_deviceDescriptor->product)
            {  
                case 0x0201: //North American iMac keyboard
                break;   // return 18 
                case 0x0203: //38=Japanese.  34 is adjustable JIS
                break;	 // return 34;
                case 0x0202: //Arabic, other ISO such as German
                //Swap two ISO keys around
                usb_2_adb_keymap[0x35] = 0x0a;  //Cosmo key18 swaps with key74, 0a is ADB keycode
                usb_2_adb_keymap[0x64] = 0x32;
		break;
                	 // return 21; //21 is Apple ISO Extended Keyboard
                default:
                //return 18;
	        break;
            }
        }



        if (_deviceDescriptor->vendor == 0x5e04)  //Microsoft ID
        {
	//Figure out what to do with Microsoft keyboard later

        //if (usb_kbd_product_id == 0x000b)   //Natural USB+PS/2 keyboard
            //return 18;
        }    
    }

    do {
        IOLog("%s: USB Generic Keyboard @ %d\n", getName(), _interface->address());

#if 0

kprintf("Adam: Class is %d\n", req.theClass);  //3 HID
kprintf("Adam: SubClass is %d\n", req.subClass);  //1
kprintf("Adam: Protocol is %d\n", req.protocol); //1 keyboard

kprintf("Adam: Type is %d\n", (_interface->endpoints[0])->transferType); //I get 1
kprintf("Adam: Number is %d\n", (_interface->endpoints[0])->number); // I get 1

#endif

        //setAlphaLockFeedback(false);  //Disable caps lock LED at first

        IOUSBFindEndpointRequest request;
        request.type = kUSBInterrupt;
        request.direction = kUSBIn;
        _interruptPipe = _interface->findNextPipe(NULL, &request);

        if(!_interruptPipe)
            return false;

        _maxPacketSize = request.maxPacketSize;
        _buffer = IOBufferMemoryDescriptor::withCapacity(8, kIODirectionIn);

        _completion.target = (void *)self;
        _completion.action = (IOUSBCompletionAction) &AppleKeyboard::readHandler;
        _completion.parameter = (void *)0;  // not used

        _buffer->setLength(_maxPacketSize); // shouldn't this be _maxPacketSize?

        if ((err = _interruptPipe->read(_buffer, &_completion)))
            break;

#if (DEBUGGING_LEVEL > 0)
        //printInterfaceDescriptor(_interface->descriptor);
#endif

        // Save stuff for polled mode
        sPollControl = _interface->bus();
        sPollPipe = _interruptPipe;
        sBufDesc = IOMemoryDescriptor::withAddress(sBuffer, 8, kIODirectionIn);
	PE_poll_input = PE_usb_poll_input;

        return(true);

    } while (false);

    IOLog("%s: aborting startup.  err = %d\n", getName(), err);

    provider->close(this);
    stop(provider);

    return(false);
}

void AppleKeyboard::stop(IOService * provider)
{
    if (_buffer) {
	_buffer->release();
        _buffer = 0;
    }
    if(sPollPipe == _interruptPipe) {
        sPollControl = NULL;
        sPollPipe = NULL;
        sBufDesc->release();
	sBufDesc = NULL;
    }
    super::stop(provider);
}

void AppleKeyboard::test_myusb_f1(void)
{
/*
    char 		dummy[8];
    IOReturn		err = kIOReturnSuccess;
    OSData 		*writebuf;
    IOUSBPipe * 	tinterruptPipe;

	writebuf = OSData::withCapacity(8);
	err = _device->openPipe(&tinterruptPipe, _interface->endpoints[0]);
	kprintf("Adam1: err is %x\n", err);
	dummy[0] = 0xff;
	dummy[1] = 0xff;
	dummy[2] = 0xff;
	dummy[3] = 0xff;
	dummy[4] = 0xff;
	dummy[5] = 0xff;
	dummy[6] = 0xff;
	dummy[7] = 0xff;

	writebuf->appendBytes(dummy, 8);
//	err = _device->write(writebuf, tinterruptPipe);
	kprintf("Adam2: err is %x\n", err);
	//deallocate writebuf
*/
}


//bits = 2 is Caps Lock, I haven't tried anything else.  0 clears all
void AppleKeyboard::Set_LED_States(UInt8 bits)
{
    IOReturn    err = kIOReturnSuccess;
    IOUSBCompletion 	completion;


    if (_prevent_LED_set)
        return; 

    //_request and _hid_report must be static because the deviceRequest() call
    // is asynchronous.

    _request.rqDirection = kUSBOut;
    _request.rqType = kUSBClass;
    _request.rqRecipient = kUSBInterface;
    _request.bRequest = kHIDRqSetReport;  //Set Report in HID specs
    OSWriteLittleInt16(&_request.wValue, 0, (UInt16) kHIDRtOutputReport << 8 );
    OSWriteLittleInt16(&_request.wIndex, 0,
		_interface->interfaceDescriptor()->interfaceNumber); // 0 for iMac keyboards
    OSWriteLittleInt16(&_request.wLength, 0, 1); // MacOS 8.5 hard-codes it to 1 byte length
    _hid_report[0] = bits;  // 2 to set LED, 0 to clear all LEDs
    _hid_report[1] = 0;
    _request.pData = (void *)_hid_report;

    completion.target = (void *)self;
    completion.action = &AppleKeyboard::setLEDHandler;
    completion.parameter = (void *)0;
    err = _interface->deviceRequest(&_request, &completion);
    return;

}


extern "C" { 
	void Debugger( const char * ); 
	void boot(int paniced, int howto, char * command);
#define RB_BOOT		1	/* Causes reboot, not halt.  Is in xnu/bsd/sys/reboot.h */

}


//This helper function is only called by StartHandler below.  key_ptr points
// to 8 (kUSB_LOWSPEED_MAXPACKET) valid bytes of data
void AppleKeyboard::Simulate_ADB_Event(UInt8 *key_ptr, UInt32 bytes_read)
{
    UInt8		alpha, modifier=0;
    bool		found;
    AbsoluteTime	now;
    UInt8		seq_key, i;//counter for alpha keys pressed.

/**
UInt8		*kbdData;
kbdData = key_ptr;
kprintf("Num = %d x%x  x%x  x%x  x%x  x%x  x%x  x%x  x%x\n",
bytes_read, *kbdData, *(kbdData +1),
*(kbdData+2), *(kbdData +3),
*(kbdData+4), *(kbdData +5),
*(kbdData+6), *(kbdData +7)
);
**/

    // Test for the keyboard bug where all the keys are 0x01. JDC.
    found = true;
    for (seq_key = 2; seq_key < prev_bytes_read; seq_key++) {
      if (*(key_ptr + seq_key) != 1) found = false;
    }
    if (found) return;


    if (bytes_read > kUSB_LOWSPEED_MAXPACKET)  // 8 bytes
    {
	bytes_read = kUSB_LOWSPEED_MAXPACKET;  //Limit myself to low-speed keyboards
    }
    modifier = *key_ptr;
    //alpha = *(key_ptr +2);  // byte +1 seems to be unused
    //adb_code = usb_2_adb_keymap[alpha];

    //Handle new key information.  The first byte is a set of bits describing
    //  which modifier keys are down.  The 2nd byte never seems to be used.
    //  The third byte is the first USB key down, and the fourth byte is the
    //  second key down, and so on.
    //When a key is released, there's no code... just a zero upon USB polling
    //8/2/99 A.W. fixed Blue Box's multiple modifier keys being pressed 
    //   simultaneously.  The trick is if a modifier key DOWN event is reported,
    //   and another DOWN is reported, then Blue Box loses track of it.  I must
    //   report a UP key event first, or else avoid resending the DOWN event.

 
    //SECTION 1. Handle modifier keys here first
    if (modifier == oldmodifier) 
    {
        //Do nothing.  Same keys are still pressed, or if 0 then none pressed
	// so don't overload the HID system with useless events.
    }
    else //Modifiers may or may not be pressed right now
    {
	//kprintf("mod is %x\n", modifier);
        clock_get_uptime(&now);

        //left-hand CONTROL modifier key
        if ((modifier & kUSB_LEFT_CONTROL_BIT) && !(oldmodifier & kUSB_LEFT_CONTROL_BIT))
        {
            dispatchKeyboardEvent(0x36, true, now);  //ADB left-hand CONTROL
	    _control_key = true;	//determine if we reboot CPU.  Is instance variable.
        }
	else if ((oldmodifier & kUSB_LEFT_CONTROL_BIT) && !(modifier & kUSB_LEFT_CONTROL_BIT))
	{
	    //Now check for released modifier keys
	    dispatchKeyboardEvent(0x36, false, now); 
	    _control_key = false;	
	}

        //right-hand CONTROL modifier
        if ((modifier & kUSB_RIGHT_CONTROL_BIT) && !(oldmodifier & kUSB_RIGHT_CONTROL_BIT))
        {
            dispatchKeyboardEvent(0x7d, true, now);  //right-hand CONTROL
	    _control_key = true;	//determine if we reboot CPU.  Is instance variable.
        }
	else if ((oldmodifier & kUSB_RIGHT_CONTROL_BIT) && !(modifier & kUSB_RIGHT_CONTROL_BIT))
	{
	    dispatchKeyboardEvent(0x7d, false, now); 
	    _control_key = false;	
	}

        //left-hand SHIFT
        if ((modifier & kUSB_LEFT_SHIFT_BIT) && !(oldmodifier & kUSB_LEFT_SHIFT_BIT))
        {
	    // Debugger("LEFT SHIFT");

            dispatchKeyboardEvent(0x38, true, now);
        }
	else if ((oldmodifier & kUSB_LEFT_SHIFT_BIT) && !(modifier & kUSB_LEFT_SHIFT_BIT))
	{
	    dispatchKeyboardEvent(0x38, false, now); 
	}

        //right-hand SHIFT
        if ((modifier & kUSB_RIGHT_SHIFT_BIT) && !(oldmodifier & kUSB_RIGHT_SHIFT_BIT))
        {
            dispatchKeyboardEvent(0x7b, true, now);
        }
	else if ((oldmodifier & kUSB_RIGHT_SHIFT_BIT) && !(modifier & kUSB_RIGHT_SHIFT_BIT))
	{
	    dispatchKeyboardEvent(0x7b, false, now); 
	}

        if ((modifier & kUSB_LEFT_ALT_BIT) && !(oldmodifier & kUSB_LEFT_ALT_BIT))
        {
            dispatchKeyboardEvent(0x3a, true, now);
        }
	else if ((oldmodifier & kUSB_LEFT_ALT_BIT) && !(modifier & kUSB_LEFT_ALT_BIT))
	{
	    dispatchKeyboardEvent(0x3a, false, now); 
	}

        if ((modifier & kUSB_RIGHT_ALT_BIT) && !(oldmodifier & kUSB_RIGHT_ALT_BIT))
        {
            dispatchKeyboardEvent(0x7c, true, now);
        }
	else if ((oldmodifier & kUSB_RIGHT_ALT_BIT) && !(modifier & kUSB_RIGHT_ALT_BIT))
	{
	    dispatchKeyboardEvent(0x7c, false, now); 
	}

        if ((modifier & kUSB_LEFT_FLOWER_BIT) && !(oldmodifier & kUSB_LEFT_FLOWER_BIT))
        {
            dispatchKeyboardEvent(0x37, true, now);
	    _flower_key = true;	//determine if we go into kernel debugger, or reboot CPU
        }
	else if ((oldmodifier & kUSB_LEFT_FLOWER_BIT) && !(modifier & kUSB_LEFT_FLOWER_BIT))
	{
	    dispatchKeyboardEvent(0x37, false, now); 
	    _flower_key = false;
	}

        if ((modifier & kUSB_RIGHT_FLOWER_BIT) && !(oldmodifier & kUSB_RIGHT_FLOWER_BIT))
        {
            //dispatchKeyboardEvent(0x7e, true, now);
	    //WARNING... NeXT only recognizes left-hand flower key, so
	    //  emulate that for now
	    dispatchKeyboardEvent(0x37, true, now);
	    _flower_key = true;	
        }
	else if ((oldmodifier & kUSB_RIGHT_FLOWER_BIT) && !(modifier & kUSB_RIGHT_FLOWER_BIT))
	{
	    dispatchKeyboardEvent(0x37, false, now); 
	    _flower_key = false;
	}

    }

    //SECTION 2. Handle regular alphanumeric keys now.  Look first at previous keystrokes.
    //  Alphanumeric portion of HID report starts at byte +2.

    for (seq_key = 2; seq_key < prev_bytes_read; seq_key++)
    {
        alpha = old_array[seq_key];
	if (alpha == 0) //No keys pressed
	{
	    break;
	}
	found = false;
	for (i = 2; i < bytes_read; i++)  //Look through current keypresses
	{
            if (alpha == *(key_ptr + i))
	    {
		found = true;	//This key has been held down for a while, so do nothing.
		break;		//   Autorepeat is taken care of by IOKit.
	    }
	}
	if (!found)
	{
	    clock_get_uptime(&now);
	    if (alpha == 0x39)    //This is USB Caps Lock scan code
	    {
	        if (!_capsLockState)
                {
	            dispatchKeyboardEvent(usb_2_adb_keymap[0x39], false, now);
		    //Only dispatch if we're in the correct state for Caps Lock
		    Set_LED_States(0);
		}
	    }
	    else  //For any other keys except LED-setting keys, dispatch immediately
	    {
	        dispatchKeyboardEvent(usb_2_adb_keymap[alpha], false, now);  //KEY UP
	    }
	}
    }

    //Now take care of KEY DOWN.  
    for (seq_key = 2; seq_key < bytes_read; seq_key++)
    {
        alpha = *(key_ptr + seq_key);
	if (alpha == 0) //No keys pressed
	{
	    break;
	}
	else if (alpha == 0x66)	   //POWER ON key
	{
	    if ((_control_key) && (_flower_key))  //User wants to reboot
	    {
		boot( RB_BOOT, 0, 0 );  //call to xnu/bsd/kern/kern_shutdown.c
	    }
	    if (_flower_key)  // Apple CMD modifier must be simultaneously held down
	    {
		PE_enter_debugger("USB Programmer Key");
		//In xnu/pexpert/ppc/pe_interrupt.c  and defined in pexpert.h above
	    }
	    //The reason this kernel debugger call is made here instead of KEY UP
	    //  is that the HID system will see the POWER key and bring up a
	    //  dialog box to shut down the computer, which is not what we want.
	}

	//Don't dispatch the same key again which was held down previously
	found = false;
	for (i = 2; i < prev_bytes_read; i++)
	{
            if (alpha == old_array[i])
	    {
		found = true;
		break;
	    }
	}
	if (!found)
	{
       	    clock_get_uptime(&now);
	    //If Debugger() is triggered then I shouldn't show the restart dialog
	    //  box, but I think developers doing kernel debugging can live with
	    //  this minor incovenience.  Otherwise I need to do more checking here.

	    if (alpha == 0x39)    //This is USB Caps Lock scan code
	    {
	        if (_capsLockState)
                {
		    _capsLockState = 0;
		    //Don't do anything else, wait for KEY_UP loop above to handle it
                }
	        else
	        {
		    _capsLockState = 1;
		    Set_LED_States(kUSB_CAPSLOCKLED_SET);  // kUSB_CAPSLOCKLED_SET  = 2
	        }
	    }

	    dispatchKeyboardEvent(usb_2_adb_keymap[alpha], true, now);
	}
    }

    //Save the history for next time
    oldmodifier = modifier;
    prev_bytes_read = bytes_read;
    for (i=0; i< bytes_read; i++)  //guaranteed not to exceed 8
    {
	old_array[i] = *(key_ptr + i);
    }

}



void AppleKeyboard::setLEDHandler(void * 		parameter,
                                  IOReturn		status,
                                  UInt32		bufferSizeRemaining)
{
    if (status)
    {       
        //Setting the LED causes problems on CMD USB controller on MacOSX, 
        //  so if error then prevent further attempts.  Log the failure 
        IOLog("USB Keyboard Caps Lock LED failed\n");
        kprintf("USB Keyboard Caps Lock LED failed\n");
        _prevent_LED_set = TRUE;
    }   
}

void AppleKeyboard::readHandler(void * 		parameter,
                                IOReturn	status,
                                UInt32		bufferSizeRemaining)
{
    IOReturn			err = kIOReturnSuccess;

    // Handle the error if any
    switch (status)
    {
        case kIOReturnSuccess:
            break;

        case kIOReturnOverrun:
            // Not sure what to do with this error.  It means more data
            // came back than the size of a descriptor.  Hmmm.  For now
            // just ignore it and assume the data that did come back is
            // useful.
            IOLog("%s: overrun error.  ignoring.\n", getName());
            break;

        case kIOReturnNotResponding:
            // This probably means the device was unplugged.  Now
            // we need to close the driver.
            IOLog("%s: Device unplugged.  Goodbye\n", getName());
            goto close;

        default:
            // We should handle other errors more intelligently, but
            // for now just return and assume the error is recoverable.
            IOLog("%s: error %d reading interrupt pipe\n", getName(), status);
            goto queueAnother;
    }

    // Handle the data
    Simulate_ADB_Event((UInt8 *)_buffer->getBytesNoCopy(), 
              (UInt32)  _maxPacketSize - bufferSizeRemaining);

queueAnother:
    // Reset the buffer
    _buffer->setLength(_maxPacketSize);

    // Queue up another one before we leave.
    if ((err = _interruptPipe->read(_buffer, &_completion)))
    {
        // This is bad.  We probably shouldn't continue on from here.
        IOLog("%s: immediate error %d queueing read\n", getName(), err);
        goto close;
    }

    return;
    
close:
    _interface->close(this);
    return;
}



// ***************************************************************************
// usb_asyncLED
//
// Called asynchronously to turn on/off the keyboard LED
//
// **************************************************************************
void usb_asyncLED( thread_call_param_t param, thread_call_param_t )
{
    AppleKeyboard * me = (AppleKeyboard *) param;
    me->Set_LED_States( me->_ledState ); 
}


//******************************************************************************
// COPIED from ADB keyboard driver 3/25/99
// This is usually called on a call-out thread after the caps-lock key is pressed.
// ADB operations to PMU are synchronous, and this is must not be done
// on the call-out thread since that is the PMU driver workloop thread, and
// it will block itself.
//
// Therefore, we schedule the ADB write to disconnect the call-out thread
// and the one that initiates the ADB write.
//
// *******************************************************************************
void AppleKeyboard::setLEDFeedback( UInt8 LED_state)
{
    _ledState = LED_state;

    thread_call_func(usb_asyncLED, (thread_call_param_t)this, true);
}


// *****************************************************************************
// maxKeyCodes
// A.W. copied 3/25/99 from ADB keyboard driver, I don't know what this does
// ***************************************************************************
UInt32 AppleKeyboard::maxKeyCodes (void )
{
return 0x80;
}


// *************************************************************************
// deviceType
//
// **************************************************************************
UInt32 AppleKeyboard::deviceType ( void )
{
    return 2;
}

// ************************************************************************
// interfaceID.  Fake ADB for now since USB defaultKeymapOfLength is too complex
//
// **************************************************************************
UInt32 AppleKeyboard::interfaceID ( void )
{
    //Return value must match "interface" line in .keyboard file
    return NX_EVS_DEVICE_INTERFACE_ADB;  // 2 This matches contents of AppleExt.keyboard
}


/***********************************************/
//Get handler ID 
//
//  I assume that this method is only called if a valid USB keyboard
//  is found. This method should return a 0 or something if there's
//  no keyboard, but then the USB keyboard driver should never have
//  been probed if there's no keyboard, so for now it won't return 0.
UInt32 AppleKeyboard::handlerID ( void )
{
    UInt32 ret_id = 0x201;  //513 decimal is ANSI USB iMac keyboard

    //Return value must match "handler_id" line in .keyboard file
    // For some reason the ADB and PS/2 keyboard drivers are missing this method
    // Also, Beaker Keyboard Prefs doesn't run properly
    //Fix hardware bug in iMac USB keyboard mapping for ISO keyboards
    //kprintf("\nUSB product = %x, vendor = %x\n", _deviceDescriptor->product, _deviceDescriptor->vendor);
    if ( _deviceDescriptor)	//just to be safe
    {
        if (_deviceDescriptor->vendor == 0xac05) //Apple USB keyboards
        {
            ret_id = _deviceDescriptor->product;  //0x201=ANSI, 0x202=ISO, or 0x203=JIS
	    if (ret_id == 0x202)
	    {
                usb_2_adb_keymap[0x35] = 0x0a;  //Cosmo key18 swaps with key74, 0a is ADB keycode
                usb_2_adb_keymap[0x64] = 0x32;
		IOLog("USB ISO keys swapped\n");
	    }
        }

        if (_deviceDescriptor->vendor == 0x045e)  //Microsoft ID
        {
            if (_deviceDescriptor->product == 0x000b)   //Natural USB+PS/2 keyboard
                ret_id = 18;  //18 is Apple Extended ADB keyboard
        }
    }

    return ret_id;
}


// *****************************************************************************
// defaultKeymapOfLength
// A.W. copied from ADB keyboard, I don't have time to make custom USB version
// *****************************************************************************
const unsigned char * AppleKeyboard::defaultKeymapOfLength (UInt32 * length )
{
    static const unsigned char appleUSAKeyMap[] = {
        0x00,0x00,0x07,
        0x00,0x01,0x39,
        0x01,0x02,0x38,0x7b,
        0x02,0x02,0x36,0x7d,0x03,0x02,0x3a,0x7c,0x04,
        0x01,0x37,0x05,0x15,0x52,0x41,0x4c,0x53,0x54,0x55,0x45,0x58,0x57,0x56,0x5b,0x5c,
        0x43,0x4b,0x51,0x3b,0x3d,0x3e,0x3c,0x4e,0x59,0x06,0x01,0x72,0x7b,0x0d,0x00,0x61,
        0x00,0x41,0x00,0x01,0x00,0x01,0x00,0xca,0x00,0xc7,0x00,0x01,0x00,0x01,0x0d,0x00,
        0x73,0x00,0x53,0x00,0x13,0x00,0x13,0x00,0xfb,0x00,0xa7,0x00,0x13,0x00,0x13,0x0d,
        0x00,0x64,0x00,0x44,0x00,0x04,0x00,0x04,0x01,0x44,0x01,0xb6,0x00,0x04,0x00,0x04,
        0x0d,0x00,0x66,0x00,0x46,0x00,0x06,0x00,0x06,0x00,0xa6,0x01,0xac,0x00,0x06,0x00,
            0x06,0x0d,0x00,0x68,0x00,0x48,0x00,0x08,0x00,0x08,0x00,0xe3,0x00,0xeb,0x00,0x00,
            0x18,0x00,0x0d,0x00,0x67,0x00,0x47,0x00,0x07,0x00,0x07,0x00,0xf1,0x00,0xe1,0x00,
            0x07,0x00,0x07,0x0d,0x00,0x7a,0x00,0x5a,0x00,0x1a,0x00,0x1a,0x00,0xcf,0x01,0x57,
            0x00,0x1a,0x00,0x1a,0x0d,0x00,0x78,0x00,0x58,0x00,0x18,0x00,0x18,0x01,0xb4,0x01,
            0xce,0x00,0x18,0x00,0x18,0x0d,0x00,0x63,0x00,0x43,0x00,0x03,0x00,0x03,0x01,0xe3,
            0x01,0xd3,0x00,0x03,0x00,0x03,0x0d,0x00,0x76,0x00,0x56,0x00,0x16,0x00,0x16,0x01,
            0xd6,0x01,0xe0,0x00,0x16,0x00,0x16,0x02,0x00,0x3c,0x00,0x3e,0x0d,0x00,0x62,0x00,
            0x42,0x00,0x02,0x00,0x02,0x01,0xe5,0x01,0xf2,0x00,0x02,0x00,0x02,0x0d,0x00,0x71,
            0x00,0x51,0x00,0x11,0x00,0x11,0x00,0xfa,0x00,0xea,0x00,0x11,0x00,0x11,0x0d,0x00,
            0x77,0x00,0x57,0x00,0x17,0x00,0x17,0x01,0xc8,0x01,0xc7,0x00,0x17,0x00,0x17,0x0d,
            0x00,0x65,0x00,0x45,0x00,0x05,0x00,0x05,0x00,0xc2,0x00,0xc5,0x00,0x05,0x00,0x05,
            0x0d,0x00,0x72,0x00,0x52,0x00,0x12,0x00,0x12,0x01,0xe2,0x01,0xd2,0x00,0x12,0x00,
            0x12,0x0d,0x00,0x79,0x00,0x59,0x00,0x19,0x00,0x19,0x00,0xa5,0x01,0xdb,0x00,0x19,
            0x00,0x19,0x0d,0x00,0x74,0x00,0x54,0x00,0x14,0x00,0x14,0x01,0xe4,0x01,0xd4,0x00,
            0x14,0x00,0x14,0x0a,0x00,0x31,0x00,0x21,0x01,0xad,0x00,0xa1,0x0e,0x00,0x32,0x00,
            0x40,0x00,0x32,0x00,0x00,0x00,0xb2,0x00,0xb3,0x00,0x00,0x00,0x00,0x0a,0x00,0x33,
            0x00,0x23,0x00,0xa3,0x01,0xba,0x0a,0x00,0x34,0x00,0x24,0x00,0xa2,0x00,0xa8,0x0e,
            0x00,0x36,0x00,0x5e,0x00,0x36,0x00,0x1e,0x00,0xb6,0x00,0xc3,0x00,0x1e,0x00,0x1e,
            0x0a,0x00,0x35,0x00,0x25,0x01,0xa5,0x00,0xbd,0x0a,0x00,0x3d,0x00,0x2b,0x01,0xb9,
            0x01,0xb1,0x0a,0x00,0x39,0x00,0x28,0x00,0xac,0x00,0xab,0x0a,0x00,0x37,0x00,0x26,
            0x01,0xb0,0x01,0xab,0x0e,0x00,0x2d,0x00,0x5f,0x00,0x1f,0x00,0x1f,0x00,0xb1,0x00,
            0xd0,0x00,0x1f,0x00,0x1f,0x0a,0x00,0x38,0x00,0x2a,0x00,0xb7,0x00,0xb4,0x0a,0x00,
            0x30,0x00,0x29,0x00,0xad,0x00,0xbb,0x0e,0x00,0x5d,0x00,0x7d,0x00,0x1d,0x00,0x1d,
            0x00,0x27,0x00,0xba,0x00,0x1d,0x00,0x1d,0x0d,0x00,0x6f,0x00,0x4f,0x00,0x0f,0x00,
            0x0f,0x00,0xf9,0x00,0xe9,0x00,0x0f,0x00,0x0f,0x0d,0x00,0x75,0x00,0x55,0x00,0x15,
            0x00,0x15,0x00,0xc8,0x00,0xcd,0x00,0x15,0x00,0x15,0x0e,0x00,0x5b,0x00,0x7b,0x00,
            0x1b,0x00,0x1b,0x00,0x60,0x00,0xaa,0x00,0x1b,0x00,0x1b,0x0d,0x00,0x69,0x00,0x49,
            0x00,0x09,0x00,0x09,0x00,0xc1,0x00,0xf5,0x00,0x09,0x00,0x09,0x0d,0x00,0x70,0x00,
            0x50,0x00,0x10,0x00,0x10,0x01,0x70,0x01,0x50,0x00,0x10,0x00,0x10,0x10,0x00,0x0d,
            0x00,0x03,0x0d,0x00,0x6c,0x00,0x4c,0x00,0x0c,0x00,0x0c,0x00,0xf8,0x00,0xe8,0x00,
            0x0c,0x00,0x0c,0x0d,0x00,0x6a,0x00,0x4a,0x00,0x0a,0x00,0x0a,0x00,0xc6,0x00,0xae,
            0x00,0x0a,0x00,0x0a,0x0a,0x00,0x27,0x00,0x22,0x00,0xa9,0x01,0xae,0x0d,0x00,0x6b,
            0x00,0x4b,0x00,0x0b,0x00,0x0b,0x00,0xce,0x00,0xaf,0x00,0x0b,0x00,0x0b,0x0a,0x00,
            0x3b,0x00,0x3a,0x01,0xb2,0x01,0xa2,0x0e,0x00,0x5c,0x00,0x7c,0x00,0x1c,0x00,0x1c,
            0x00,0xe3,0x00,0xeb,0x00,0x1c,0x00,0x1c,0x0a,0x00,0x2c,0x00,0x3c,0x00,0xcb,0x01,
            0xa3,0x0a,0x00,0x2f,0x00,0x3f,0x01,0xb8,0x00,0xbf,0x0d,0x00,0x6e,0x00,0x4e,0x00,
            0x0e,0x00,0x0e,0x00,0xc4,0x01,0xaf,0x00,0x0e,0x00,0x0e,0x0d,0x00,0x6d,0x00,0x4d,
            0x00,0x0d,0x00,0x0d,0x01,0x6d,0x01,0xd8,0x00,0x0d,0x00,0x0d,0x0a,0x00,0x2e,0x00,
            0x3e,0x00,0xbc,0x01,0xb3,0x02,0x00,0x09,0x00,0x19,0x0c,0x00,0x20,0x00,0x00,0x00,
            0x80,0x00,0x00,0x0a,0x00,0x60,0x00,0x7e,0x00,0x60,0x01,0xbb,0x02,0x00,0x7f,0x00,
            0x08,0xff,0x02,0x00,0x1b,0x00,0x7e,0xff,0xff,0xff,0xff,0xff,0x00,0x01,0xac,0x00,
            0x01,0xae,0x00,0x01,0xaf,0x00,0x01,0xad,0xff,0xff,0x00,0x00,0x2e,0xff,0x00,0x00,
            0x2a,0xff,0x00,0x00,0x2b,0xff,0x00,0x00,0x1b,0xff,0xff,0xff,0x0e,0x00,0x2f,0x00,
            0x5c,0x00,0x2f,0x00,0x1c,0x00,0x2f,0x00,0x5c,0x00,0x00,0x0a,0x00,0x00,0x00,0x0d, //XX03
            0xff,0x00,0x00,0x2d,0xff,0xff,0x0e,0x00,0x3d,0x00,0x7c,0x00,0x3d,0x00,0x1c,0x00,
            0x3d,0x00,0x7c,0x00,0x00,0x18,0x46,0x00,0x00,0x30,0x00,0x00,0x31,0x00,0x00,0x32,
            0x00,0x00,0x33,0x00,0x00,0x34,0x00,0x00,0x35,0x00,0x00,0x36,0x00,0x00,0x37,0xff,
            0x00,0x00,0x38,0x00,0x00,0x39,0xff,0xff,0xff,0x00,0xfe,0x24,0x00,0xfe,0x25,0x00,
            0xfe,0x26,0x00,0xfe,0x22,0x00,0xfe,0x27,0x00,0xfe,0x28,0xff,0x00,0xfe,0x2a,0xff,
            0x00,0xfe,0x32,0xff,0x00,0xfe,0x33,0xff,0x00,0xfe,0x29,0xff,0x00,0xfe,0x2b,0xff,
            0x00,0xfe,0x34,0xff,0x00,0xfe,0x2e,0x00,0xfe,0x30,0x00,0xfe,0x2d,0x00,0xfe,0x23,
            0x00,0xfe,0x2f,0x00,0xfe,0x21,0x00,0xfe,0x31,0x00,0xfe,0x20,0x0f,0x02,0xff,0x04,
            0x00,0x31,0x02,0xff,0x04,0x00,0x32,0x02,0xff,0x04,0x00,0x33,0x02,0xff,0x04,0x00,
            0x34,0x02,0xff,0x04,0x00,0x35,0x02,0xff,0x04,0x00,0x36,0x02,0xff,0x04,0x00,0x37,
            0x02,0xff,0x04,0x00,0x38,0x02,0xff,0x04,0x00,0x39,0x02,0xff,0x04,0x00,0x30,0x02,
            0xff,0x04,0x00,0x2d,0x02,0xff,0x04,0x00,0x3d,0x02,0xff,0x04,0x00,0x70,0x02,0xff,
            0x04,0x00,0x5d,0x02,0xff,0x04,0x00,0x5b,0x04,0x05,0x72,0x06,0x7f,0x07,
            0x3e,0x08,0x3d
    };

*length = sizeof(appleUSAKeyMap);
return appleUSAKeyMap;
}
