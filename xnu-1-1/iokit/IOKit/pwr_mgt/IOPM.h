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
#ifndef _IOKIT_IOPM_H
#define _IOKIT_IOPM_H

#define IOPMMaxPowerStates 10

typedef unsigned long IOPMPowerFlags;

#define IOPMNotAttainable	0x0001
#define IOPMPowerOn		0x0002
#define IOPMClockNormal	0x0004
#define IOPMClockRunning	0x0008
#define IOPMWakeupEnabled	0x0010
                        // following "capabilites" exist for the convenience
                        // of the "interested drivers"
#define IOPMDeviceUsable	0x8000
#define IOPMMaxPerformance	0x4000
#define IOPMContextRetained	0x2000
#define IOPMConfigRetained	0x1000

#define IOPMNextHigherState	1
#define IOPMHighestState	2
#define IOPMNextLowerState	3
#define IOPMLowestState		4


enum {		// commands on power managment command queue
    kPMbroadcastAggressiveness = 1,
    kPMunIdleDevice,
    kPMsleepDemand,
    kPMwakeSignal
};



                                        // Return codes

#define IOPMNoErr		0	// normal return

                        // returned by powerStateWillChange and powerStateDidChange:
#define IOPMAckImplied		0	// acknowledgement of power state change is implied
#define IOPMWillAckLater	1	// acknowledgement of power state change will come later

                        // returned by requestDomainState
#define IOPMBadSpecification	4	// unrecognized specification parameter
#define IOPMNoSuchState		5	// no power state matches search specification

#define IOPMCannotRaisePower	6	// a device cannot change its power for some reason

                        // returned by changeStateTo
#define IOPMParameterError	7	// requested state doesn't exist
#define IOPMNotYetInitialized	8	// device not yet fully hooked into power management "graph"


						// used by Root Domain UserClient
#define kNumPMMethods 4

enum {
    kPMGeneralAggressiveness = 0,
    kPMMinutesToDim,
    kPMMinutesToSpinDown,
    kPMMinutesToSleep
};
#define kMaxType kPMMinutesToSleep


#define kIOBatteryInfoKey		"IOBatteryInfo"
#define kIOBatteryCurrentChargeKey	"Current"
#define kIOBatteryCapacityKey		"Capacity"
#define kIOBatteryFlagsKey		"Flags"
enum {
    kIOBatteryInstalled		= (1 << 2),
    kIOBatteryCharge		= (1 << 1),
    kIOBatteryChargerConnect	= (1 << 0)
};


#if KERNEL && __cplusplus
class IOService;

enum {
    kIOPowerEmergencyLevel = 1000
};

enum {
    kIOPMSubclassPolicy,
    kIOPMSuperclassPolicy1
};

extern void IOPMRegisterDevice(const char *, IOService *);
#endif /* KERNEL && __cplusplus */

#endif /* ! _IOKIT_IOPM_H */

