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
 *  1 Dec 1998 suurballe  Created.
 */

#include <IOKit/IOSyncer.h>
#include "IOPMURTCController.h"
#include "pmupriv.h"

//#define super IORTCController
//OSDefineMetaClassAndStructors(IOPMURTCController, IORTCController)

#define super IOService
OSDefineMetaClassAndStructors(IOPMURTCController, IOService)


// **********************************************************************************
// init
//
// **********************************************************************************
bool IOPMURTCController::init ( OSDictionary * regEntry, ApplePMU * driver )
{
PMUdriver = driver;

return super::init(regEntry);
}


// **********************************************************************************
// tickHandler
//
// **********************************************************************************
void tickHandler ( IOService * us, UInt32, UInt8 * )
{
((IOPMURTCController *)us)->clientHandler(((IOPMURTCController *)us)->tickClient);
}


// **********************************************************************************
// registerForClockTicks
//
// The RTC driver is calling to tell us that it is prepared to receive clock
// ticks every second.  The parameter block tells who to call when we get one.
//
// **********************************************************************************
void IOPMURTCController::registerForClockTicks ( IOService * client, RTC_tick_handler handler )
{
clientHandler = handler;
tickClient = client;
PMUdriver->registerForClockInterrupts ( tickHandler, this );
}


// **********************************************************************************
// setRealTimeClock
//
// The RTC driver is calling to set the real time clock.  We translate this into
// a PMU command and enqueue it to our command queue.
//
// **********************************************************************************
IOReturn IOPMURTCController::setRealTimeClock ( UInt8 * newTime )
{
PMUrequest request;

if ( newTime == NULL ) {
        return kPMUParameterError;
}

request.sync = IOSyncer::create();
request.pmCommand = kPMUtimeWrite;
request.pmFlag = false;
request.pmSLength1 = 0;
request.pmSBuffer2 = newTime;
request.pmSLength2 = 4;
request.pmRBuffer = NULL;

PMUdriver->enqueueCommand(&request);
request.sync->wait();			// wait till done

return kPMUNoError;
}


// **********************************************************************************
// getRealTimeClock
//
// The RTC driver is calling to read the real time clock.  We translate this into
// a PMU command and enqueue it to our command queue.
//
// The length parameter is ignored on entry.  On exit it is set to the length
// in bytes of the time read.
// **********************************************************************************
IOReturn IOPMURTCController::getRealTimeClock ( UInt8 * currentTime, IOByteCount * length )
{
PMUrequest  request;

if ( currentTime == NULL ) {
        return kPMUParameterError;
}

request.sync = IOSyncer::create();
request.pmCommand = kPMUtimeRead;
request.pmFlag = false;
request.pmSLength1 = 0;
request.pmSBuffer2 = NULL;
request.pmSLength2 = 0;
request.pmRBuffer = currentTime;

PMUdriver->enqueueCommand(&request);
request.sync->wait();			// wait till done

*length = request.pmRLength;				// set caller's length
    
return kPMUNoError;
}


