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
 * 24 Nov 1998 suurballe  Created.
 */

#include <IOKit/rtc/IORTCController.h>

class ApplePMU;


//class IOPMURTCController : public IORTCController
class IOPMURTCController : public IOService
{
OSDeclareDefaultStructors(IOPMURTCController)

private:

ApplePMU * PMUdriver;

public:

RTC_tick_handler clientHandler;
IOService *	tickClient;

bool init ( OSDictionary * regEntry, ApplePMU * driver );
void registerForClockTicks ( IOService * client, RTC_tick_handler handler );
IOReturn getRealTimeClock ( UInt8 * currentTime, IOByteCount * length );
IOReturn setRealTimeClock ( UInt8 * newTime );
};
