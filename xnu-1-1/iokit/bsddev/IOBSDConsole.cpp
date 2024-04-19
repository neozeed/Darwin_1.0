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
#include <IOKit/assert.h>
#include <IOKit/IOMessage.h>
#include <IOKit/IOLib.h>
#include "IOBSDConsole.h"
#include <IOKit/hidsystem/IOHIKeyboard.h>
#include <IOKit/hidsystem/IOLLEvent.h>
#include <IOKit/audio/IOAudio.h>

static IOBSDConsole * gBSDConsoleInst = 0;
bool displayWranglerPublished( OSObject *, void *, IOService * );

#define super IOService
OSDefineMetaClassAndStructors(IOBSDConsole, IOService);

//************************************************************************

bool IOBSDConsole::start(IOService * provider)
{
    OSObject *	notify;

    if (!super::start(provider))  return false;

    assert( gBSDConsoleInst == 0 );
    gBSDConsoleInst = this;

    notify = addNotification( gIOPublishNotification,
        serviceMatching("IOHIKeyboard"),
        (IOServiceNotificationHandler) &IOBSDConsole::publishNotificationHandler,
        this, 0 );
    assert( notify );

    notify = addNotification( gIOPublishNotification,
        serviceMatching("IODisplayWrangler"),
                              (IOServiceNotificationHandler)displayWranglerPublished,
         this, 0 );
    assert( notify ); 

    notify = addNotification( gIOPublishNotification,
        serviceMatching("IOAudioStream"),
        (IOServiceNotificationHandler) &IOBSDConsole::publishNotificationHandler,
        this, this );
    assert( notify );

    return( true );
}

bool IOBSDConsole::publishNotificationHandler(
			    IOBSDConsole * self,
                            void * ref,
                            IOService * newService )

{
    IOHIKeyboard *	keyboard;
    IOAudioStream *	audio;

    if( ref) {
        keyboard = 0;
        audio = OSDynamicCast(IOAudioStream, newService);
        if(audio) {
            SInt32	isOut;
            audio->isOutput(&isOut);
            if(isOut)
                self->fAudioOut = audio;	// A stream we can write to
	}

    } else {
	audio = 0;
        keyboard = OSDynamicCast( IOHIKeyboard, newService );

        if( keyboard && self->attach( keyboard ))
            self->arbitrateForKeyboard( keyboard );
    }

    if( !keyboard && !audio)
	IOLog("%s: strange service notify \"%s\"\n",
		self->getName(), newService->getName());

    return true;
}

// **********************************************************************************
// displayWranglerPublished
//
// The Display Wrangler has appeared.  We will be calling its
// ActivityTickle method when there is user activity.
// **********************************************************************************
bool displayWranglerPublished( OSObject * us, void * ref, IOService * yourDevice )
{
 if ( yourDevice != NULL ) {
     ((IOBSDConsole *)us)->displayManager = yourDevice;
 }
 return true;
}


//************************************************************************
// Keyboard client stuff
//************************************************************************

void IOBSDConsole::arbitrateForKeyboard( IOHIKeyboard * nub )
{
  nub->open(this, 0,
	keyboardEvent, keyboardSpecialEvent, updateEventFlags);
  // failure can be expected if the HID system already has it
}

IOReturn IOBSDConsole::message(UInt32 type, IOService * provider,
				void * argument)
{
  IOReturn     status = kIOReturnSuccess;

  switch (type)
  {
    case kIOMessageServiceIsTerminated:
    case kIOMessageServiceIsRequestingClose:
      provider->close( this );
      break;

    case kIOMessageServiceWasClosed:
      arbitrateForKeyboard( (IOHIKeyboard *) provider );
      break;

    default:
      status = super::message(type, provider, argument);
      break;
  }

  return status;
}

extern "C" {
  void cons_cinput( char c);
}
#warning REMOVE cons_cinput DECLARATION FROM HERE

void IOBSDConsole::keyboardEvent(OSObject * target,
          /* eventType */        unsigned   eventType,
          /* flags */            unsigned   /* flags */,
          /* keyCode */          unsigned   /* key */,
          /* charCode */         unsigned   charCode,
          /* charSet */          unsigned   charSet,
          /* originalCharCode */ unsigned   /* origCharCode */,
          /* originalCharSet */  unsigned   /* origCharSet */,
          /* repeat */           bool       /* repeat */,
          /* atTime */           AbsoluteTime /* ts */)
{
    static const char cursorCodes[] = { 'D', 'A', 'C', 'B' };

    if ( ((IOBSDConsole *)target)->displayManager != NULL ) {				// if there is a display manager,
        ((IOBSDConsole *)target)->displayManager->activityTickle(kIOPMSuperclassPolicy1);	// tell it there is user activity
    }

    if( eventType == NX_KEYDOWN) {
        if( (charSet == NX_SYMBOLSET)
            && (charCode >= 0xac) && (charCode <= 0xaf)) {
            cons_cinput( '\033');
            cons_cinput( 'O');
            charCode = cursorCodes[ charCode - 0xac ];
        }
        cons_cinput( charCode);
    }
}

void IOBSDConsole::keyboardSpecialEvent(OSObject * target,
                        /* eventType */ unsigned   /* eventType */,
                        /* flags */     unsigned   /* flags */,
                        /* keyCode */   unsigned   /* key */,
                        /* specialty */ unsigned   /* flavor */,
                        /* atTime */    AbsoluteTime /* ts */)
{
    if ( ((IOBSDConsole *)target)->displayManager != NULL ) {				// if there is a display manager,
        ((IOBSDConsole *)target)->displayManager->activityTickle(kIOPMSuperclassPolicy1);	// tell it there is user activity
    }
  return;
}
void IOBSDConsole::updateEventFlags(OSObject * /*target*/, unsigned /*flags*/)
{
  return;
}

//************************************************************************
// Utility sound making stuff, callable from C
//************************************************************************
extern "C" {
  int asc_ringbell();
}

/*
* Make some sort of noise if possible, else return false
*/
int asc_ringbell()
{
    IOAudioStream *output;

    if(gBSDConsoleInst && (output = gBSDConsoleInst->getAudioOut())) {
        int dmaBlockStart, dmaBlockEnd;
        int i;
        IOAudioStreamStatus *status;
        short *sampleBuffer;
        IOByteCount bufSize;
        IOUserClient *handler;
	IOMemoryMap *statusMap;
	IOMemoryMap *sampleMap;
        IOReturn ret;
        int j, div, mul;
        int size;
        int val;

        ret = output->newUserClient( kernel_task, NULL, 0, &handler );
        if(kIOReturnSuccess != ret)
            return false;

	sampleMap = output->mapClientMemory( kSampleBuffer, kernel_task  );
	assert(sampleMap);
	sampleBuffer = (short *) sampleMap->getVirtualAddress();
	bufSize = sampleMap->getLength();

	statusMap = output->mapClientMemory( kStatus, kernel_task );
	assert(statusMap);
	status = (IOAudioStreamStatus *) statusMap->getVirtualAddress();

        assert(status->fSampleSize == 2);
        // Put 1/10 second of 440Hz sound in the buffer, starting just after the current block.
        div = status->fDataRate/status->fSampleSize/status->fChannels/440;
        mul = 0x4000/div;
        size = status->fDataRate/status->fSampleSize/status->fChannels/10;
        dmaBlockStart = status->fCurrentBlock * status->fBlockSize;
        dmaBlockEnd = dmaBlockStart + status->fBlockSize;
        for(i=dmaBlockEnd/2; i<status->fBufSize/2; i += status->fChannels){
            if(!--size)
		break;
            val = (size) % div;
            if(val > div/2)
                val = div/2 - val;
            val = val*mul - 0x1000;
            for(j=0; j<status->fChannels; j++) {
            	sampleBuffer[i+j] = val;
            }
        }
        if(size)
            for(i=0; i<dmaBlockStart/2; i += status->fChannels){
                if(!--size)
                    break;
                val = (size) % div;
                if(val > div/2)
                    val = div/2 - val;
                val = val*mul - 0x1000;
                for(j=0; j<status->fChannels; j++) {
                    sampleBuffer[i+j] = val;
                }
            }

	sampleMap->release();
	statusMap->release();
        handler->clientClose();
        return true;
    }
    else
        return false;
}
