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
 *
 * ATACommand.h
 *
 */

enum ATADeviceType
{
    ataDeviceNone,
    ataDeviceATA,
    ataDeviceATAPI,
};


enum ATATimingProtocol
{
    ataTimingPIO		= (1 << 0),
    ataTimingDMA     		= (1 << 1),
    ataTimingUltraDMA33		= (1 << 2),    ataTimingUltraDMA66		= (1 << 3),
    ataMaxTimings		= 4,
};

enum ATAProtocol
{
   ataProtocolSetRegs		= (1 << 0),
   ataProtocolPIO		= (1 << 1),
   ataProtocolPIOMultiple	= (1 << 2),
   ataProtocolDMA		= (1 << 3),
   ataProtocolDMAQueued		= (1 << 4),
   ataProtocolDMAQueuedRelease	= (1 << 5),

   atapiProtocolPIO		= (1 << 16),
   atapiProtocolDMA		= (1 << 17),
};

typedef struct ATATiming
{
    ATATimingProtocol	timingProtocol;

    UInt32		featureSetting;

    UInt32		mode;    UInt32		minDataAccess;    UInt32		minDataCycle;    UInt32		minCmdAccess;    UInt32		minCmdCycle;
    UInt32		reserved_3[9];} ATATiming;


enum ATAReturnCode
{
    ataReturnNoError,
    ataReturnNotSupported,
    ataReturnNoResource,
    ataReturnErrorRetryPIO,
    ataReturnErrorBusy,
    ataReturnErrorInterruptTimeout,
    ataReturnErrorStatus,
    ataReturnErrorProtocol,
    ataReturnErrorDMA,
    ataReturnBusReset,
};

#define ATARegtoMask(reg) (1<<(reg))

typedef struct ATATaskfile	{
    ATAProtocol		protocol;

    UInt32		resultmask;
    UInt32		regmask;
    UInt32              ataRegs[MAX_ATA_REGS];
} ATATaskfile;

typedef struct ATAResults
{
    ATAReturnCode	returnCode;

    UInt32		bytesTransferred;
    UInt32              ataRegs[MAX_ATA_REGS];

    UInt32		senseLength;
} ATAResults;


typedef struct ATAPICmd
{
    UInt32		cdbLength;
    UInt8		cdb[16];
} ATAPICmd;

enum ATAMessages
{
    ataMessageInitDevice	= 1,
    ataMessageResetStarted	= 2, 		
    ataMessageResetComplete	= 3,
};	

