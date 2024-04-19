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
 * Copyright (c) 1998-1999 Apple Computer, Inc.  All rights reserved. 
 *
 * HISTORY
 *
 */

enum IOMessage
{
    kIOMessageServiceIsTerminated	= 0x80000010,
    kIOMessageServiceIsSuspended	= 0x80000020,
    kIOMessageServiceIsResumed		= 0x80000030,

    kIOMessageServiceIsRequestingClose	= 0x80000100,
    kIOMessageServiceWasClosed		= 0x80000110,

    kIOMessageServiceBusyStateChange	= 0x80000120,

    kIOMessageCanDevicePowerOff		= 0x80000200,
    kIOMessageDeviceWillPowerOff	= 0x80000210,
    kIOMessageDeviceWillNotPowerOff	= 0x80000220,
    kIOMessageDeviceHasPoweredOn	= 0x80000230,
    kIOMessageCamSustemPowerOff		= 0x80000240,
    kIOMessageSystemWillPowerOff	= 0x80000250,
    kIOMessageSystemWillNotPowerOff	= 0x80000260,
    kIOMessageCanSystemSleep		= 0x80000270,
    kIOMessageSystemWillSleep		= 0x80000270,
    kIOMessageSystemWillNotSleep	= 0x80000290,
    kIOMessageSystemHasPoweredOn	= 0x80000300
};

