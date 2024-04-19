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
#ifndef _USB_H
#define _USB_H

#include <IOKit/IOTypes.h>

#if !defined(__USB__)
#    include <IOKit/usb/USBSpec.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define EndianSwap16(data16)                            \
(                                                       \
        ((((UInt16) data16) >> 8) & 0x00FF)     |       \
        ((((UInt16) data16) << 8) & 0xFF00)             \
)
#define USBToHostWord EndianSwap16
#define HostToUSBWord EndianSwap16

/*!
    @typedef USBWord
    A 16 bit value in little-endian format, best accessed using
    the OSRead/WriteLittleInt16 functions
    @field loByte least significant byte
    @field hiByte most significant byte
*/
typedef struct USBWord {
    UInt8 loByte;
    UInt8 hiByte;
} USBWord;

enum {
    kUSBDeviceIDShift = 7,
    kUSBMaxDevices = 128,
    kUSBMaxDevice = kUSBMaxDevices-1,
    kUSBDeviceIDMask = 0x7f,

    kUSBPipeIDMask = 0xf,
    kUSBMaxPipes = 32,	// In and Out pipes can have same pipe number.

    kUSBInterfaceIDShift = 8,
    kUSBMaxInterfaces = 1 << kUSBInterfaceIDShift,
    kUSBInterfaceIDMask = kUSBMaxInterfaces-1,

    kUSBEndPtShift = 7,
    kUSBDeviceMask = ((1 << kUSBEndPtShift) -1),

    kUSBNoPipeIdx = -1,
};

enum {
    kUSBRqDirnShift = 7,
    kUSBRqDirnMask = 1,

    kUSBRqTypeShift = 5,
    kUSBRqTypeMask = 3,

    kUSBRqRecipientMask = 0X1F
};

enum {
    kUSBHighSpeed = 0,
    kUSBLowSpeed = 1
};

enum {
    kUSBMaxIsocFrameReqCount = 1023 // max size (bytes) of any one Isoc frame
};

#define EncodeRequest(request, direction, type, recipient)		\
    (((UInt16)request << 8) +	    					\
              ((UInt16)recipient +					\
                        ((UInt16)type << kUSBRqTypeShift) +		\
                        ((UInt16)direction << kUSBRqDirnShift)))


enum {
/*
Standard Requests

bmRequestType bRequest      wValue  wIndex    wLength Data
00000000B CLEAR_FEATURE     Feature Zero       Zero   None (device)
00000001B                   Feature Interface  Zero   None (Interface)
00000010B                   Feature Endpoint   Zero   None (Endpoint)

10000000B GET_CONFIGURATION Zero    Zero       One    Configuration
10000000B GET_DESCRIPTOR    Type    LangID     Length Descriptor
10000001B GET_INTERFACE     Zero    Interface  One    Alternate

10000000B GET_STATUS        Zero    Zero       Two    status (device)
10000001B                   Zero    Interface  Two    status (Interface)
10000010B                   Zero    Endpoint   Two    status (Endpoint)

00000000B SET_ADDRESS       Address Zero       Zero   None
00000000B SET_CONFIGURATION Configuration Zero Zero   None
00000000B SET_DESCRIPTOR    Type   LangID      Length Descriptor

00000000B SET_FEATURE       Feature Zero       Zero   None (device)
00000001B                   Feature Interface  Zero   None (Interface)
00000010B                   Feature Endpoint   Zero   None (Endpoint)

00000001B SET_INTERFACE   Alternate Interface  Zero   None
10000010B SYNCH_FRAME       Zero    Endpoint   Two    Frame Number
*/
kClearDeviceFeature
    = EncodeRequest(kUSBRqClearFeature,  kUSBOut, kUSBStandard, kUSBDevice),
kClearInterfaceFeature
    = EncodeRequest(kUSBRqClearFeature,  kUSBOut, kUSBStandard, kUSBInterface),
kClearEndpointFeature
    = EncodeRequest(kUSBRqClearFeature,  kUSBOut, kUSBStandard, kUSBEndpoint),
kGetConfiguration
    = EncodeRequest(kUSBRqGetConfig,     kUSBIn,  kUSBStandard, kUSBDevice),
kGetDescriptor
    = EncodeRequest(kUSBRqGetDescriptor, kUSBIn,  kUSBStandard, kUSBDevice),
kGetInterface
    = EncodeRequest(kUSBRqGetInterface,  kUSBIn,  kUSBStandard, kUSBInterface),
kGetDeviceStatus
    = EncodeRequest(kUSBRqGetStatus,     kUSBIn,  kUSBStandard, kUSBDevice),
kGetInterfaceStatus
    = EncodeRequest(kUSBRqGetStatus,     kUSBIn,  kUSBStandard, kUSBInterface),
kGetEndpointStatus
    = EncodeRequest(kUSBRqGetStatus,     kUSBIn,  kUSBStandard, kUSBEndpoint),
kSetAddress
    = EncodeRequest(kUSBRqSetAddress,    kUSBOut, kUSBStandard, kUSBDevice),
kSetConfiguration
    = EncodeRequest(kUSBRqSetConfig,     kUSBOut, kUSBStandard, kUSBDevice),
kSetDescriptor
    = EncodeRequest(kUSBRqSetDescriptor, kUSBOut, kUSBStandard, kUSBDevice),
kSetDeviceFeature
    = EncodeRequest(kUSBRqSetFeature,    kUSBOut, kUSBStandard, kUSBDevice),
kSetInterfaceFeature
    = EncodeRequest(kUSBRqSetFeature,    kUSBOut, kUSBStandard, kUSBInterface),
kSetEndpointFeature
    = EncodeRequest(kUSBRqSetFeature,    kUSBOut, kUSBStandard, kUSBEndpoint),
kSetInterface
    = EncodeRequest(kUSBRqSetInterface,  kUSBOut, kUSBStandard, kUSBInterface),
kSyncFrame
    = EncodeRequest(kUSBRqSyncFrame,     kUSBIn,  kUSBStandard, kUSBEndpoint),

};

// TYPES

typedef struct IOUSBIsocFrame {
    IOReturn                        frStatus;
    UInt16                          frReqCount;
    UInt16                          frActCount;
} IOUSBIsocFrame;

typedef UInt16 USBDeviceAddress;

/*!
    @typedef IOUSBCompletionAction
    Function called when USB I/O completes
    @param target The target specified in the IOUSBCompletion struct.
    @param parameter The parameter specified in the IOUSBCompletion struct.
    @param status Completion status
    @param bufferSizeRemaining bytes left to be transferred
*/
typedef void (*IOUSBCompletionAction)(
                void *			target,
                void *			parameter,
                IOReturn		status,
                UInt32			bufferSizeRemaining);

typedef void (*IOUSBIsocCompletionAction)(
                void *			target,
                void *			parameter,
                IOReturn		status,
                IOUSBIsocFrame		*pFrames);

/*!
    @typedef IOUSBCompletion
    Struct spefifying action to perform when a USB I/O completes
    @param target The target to pass to the action function.
    @param action The function to call.
    @param parameter The parameter to pass to the action function.
    @param bufferSizeRemaining bytes left to be transferred
*/
typedef struct IOUSBCompletion {
    void * 			target;
    IOUSBCompletionAction	action;
    void *			parameter;
} IOUSBCompletion;

typedef struct IOUSBIsocCompletion {
    void * 			target;
    IOUSBIsocCompletionAction	action;
    void *			parameter;
} IOUSBIsocCompletion;

/* ************* Constants ************* */

#define	iokit_usb_err(return) (sys_iokit|sub_iokit_usb|return)
#define kIOUSBUnknownPipeErr	iokit_usb_err(97) /*  Pipe ref not recognised */
#define kIOUSBTooManyPipesErr	iokit_usb_err(96) /*  Too many pipes */
#define kIOUSBEndpointNotFound	iokit_usb_err(87) /*  Not found */
#define kIOUSBConfigNotFound	iokit_usb_err(86) /*  Not found */
#define kIOUSBPipeStalled	iokit_usb_err(79) /*  Pipe has stalled, error needs to be cleared */
#define kIOUSBInterfaceNotFound	iokit_usb_err(78) /*  Interface ref not recognised */

/* ************* types ************* */

struct IOUSBMouseData {
    UInt16 				buttons;
    SInt16 				XDelta;
    SInt16 				YDelta;
};
typedef struct IOUSBMouseData		IOUSBMouseData;
typedef IOUSBMouseData *		IOUSBMouseDataPtr;

struct IOUSBKeyboardData {
    UInt16 				keycount;
    UInt16 				usbkeycode[32];
};
typedef struct IOUSBKeyboardData	IOUSBKeyboardData;
typedef IOUSBKeyboardData *		IOUSBKeyboardDataPtr;

union IOUSBHIDData {
    IOUSBKeyboardData 			kbd;
    IOUSBMouseData 			mouse;
};
typedef union IOUSBHIDData		IOUSBHIDData;
typedef IOUSBHIDData *			IOUSBHIDDataPtr;

struct IOUSBDeviceDescriptor {
	UInt8 			length;
	UInt8 			descType;
	UInt16 			usbRel;
	UInt8 			deviceClass;
	UInt8 			deviceSubClass;
	UInt8 			protocol;
	UInt8 			maxPacketSize;
	UInt16 			vendor;
	UInt16 			product;
	UInt16 			devRel;
	UInt8 			manuIdx;
	UInt8 			prodIdx;
	UInt8 			serialIdx;
	UInt8 			numConf;
};
typedef struct IOUSBDeviceDescriptor		IOUSBDeviceDescriptor;
typedef IOUSBDeviceDescriptor *			IOUSBDeviceDescriptorPtr;

struct IOUSBDescriptorHeader {
	UInt8 			length;
	UInt8 			descriptorType;
};
typedef struct IOUSBDescriptorHeader		IOUSBDescriptorHeader;
typedef IOUSBDescriptorHeader *			IOUSBDescriptorHeaderPtr;

struct IOUSBConfigurationDescriptor {
	UInt8 			length;
	UInt8 			descriptorType;
	UInt16 			totalLength;
	UInt8 			numInterfaces;
	UInt8 			configValue;
	UInt8 			configStrIndex;
	UInt8 			attributes;
	UInt8 			maxPower;
};
typedef struct IOUSBConfigurationDescriptor 	IOUSBConfigurationDescriptor;
typedef IOUSBConfigurationDescriptor *		IOUSBConfigurationDescriptorPtr;

struct IOUSBInterfaceDescriptor {
	UInt8 			length;
	UInt8 			descriptorType;
	UInt8 			interfaceNumber;
	UInt8 			alternateSetting;
	UInt8 			numEndpoints;
	UInt8 			interfaceClass;
	UInt8 			interfaceSubClass;
	UInt8 			interfaceProtocol;
	UInt8 			interfaceStrIndex;
};
typedef struct IOUSBInterfaceDescriptor	IOUSBInterfaceDescriptor;
typedef IOUSBInterfaceDescriptor *	IOUSBInterfaceDescriptorPtr;

struct IOUSBEndpointDescriptor {
	UInt8 			length;
	UInt8 			descriptorType;
	UInt8 			endpointAddress;
	UInt8 			attributes;
	/* Split up maxPacketSize to avoid packing & alignment problems */
	UInt8 			maxPacketSizeLo;	
        UInt8 			maxPacketSizeHi;
	UInt8 			interval;
};
typedef struct IOUSBEndpointDescriptor	IOUSBEndpointDescriptor;
typedef IOUSBEndpointDescriptor *	IOUSBEndpointDescriptorPtr;

struct IOUSBHIDDescriptor {
	UInt8 			descLen;
	UInt8 			descType;
	UInt16 			descVersNum;
	UInt8 			hidCountryCode;
	UInt8 			hidNumDescriptors;
	UInt8 			hidDescriptorType;
        /* can't make this a single 16bit value or the compiler will add a filler byte*/
	UInt8 			hidDescriptorLengthLo;
	UInt8 			hidDescriptorLengthHi;
};
typedef struct IOUSBHIDDescriptor	IOUSBHIDDescriptor;
typedef IOUSBHIDDescriptor *IOUSBHIDDescriptorPtr;

struct IOUSBHIDReportDesc {
	UInt8 			hidDescriptorType;
    	/* can't make this a single 16bit value or the compiler will add a filler byte*/
	UInt8 			hidDescriptorLengthLo;	
	UInt8 			hidDescriptorLengthHi;
};
typedef struct IOUSBHIDReportDesc	IOUSBHIDReportDesc;
typedef IOUSBHIDReportDesc *		IOUSBHIDReportDescPtr;

typedef UInt16				USBStatus;
typedef USBStatus *			USBStatusPtr;

enum {
    kIOUSBAnyClass	= 0xFFFF,
    kIOUSBAnySubClass	= 0xFFFF,
    kIOUSBAnyProtocol	= 0xFFFF,
    kIOUSBAnyVendor	= 0xFFFF,
    kIOUSBAnyProduct	= 0xFFFF
};
typedef struct IOUSBMatch {
    UInt16	usbClass;
    UInt16	usbSubClass;
    UInt16	usbProtocol;
    UInt16	usbVendor;
    UInt16	usbProduct;
} IOUSBMatch;

/*!
    @typedef IOUSBFindEndpointRequest
    Struct used to find endpoints of an interface
    type and direction are used to match endpoints,
    type, direction, maxPacketSize and interval are updated
    with the properties of the found endpoint.
    @field type Type of endpoint: kUSBControl, kUSBIsoc, kUSBBulk, kUSBInterrupt, kUSBAnyType
    @field direction Direction of endpoint: kUSBOut, kUSBIn, kUSBAnyDirn
    @field maxPacketSize maximum packet size of endpoint.
    @field interval Polling interval in mSec for endpoint.
*/
typedef struct {
	UInt8 type;		// kUSBAnyType = don't care
	UInt8 direction;	// kUSBAnyDirn = don't care
        UInt16 maxPacketSize;
        UInt8 interval;
} IOUSBFindEndpointRequest;

/*
 * Stuff for IOUserClient
 */
enum {
    // for IOServiceOpen
    kIOUSBDeviceConnectType	= 10
};

enum {
    kUSBSetConfig = 0,
    kUSBGetConfig,
    kUSBGetConfigDescriptor,
    kUSBControlReqOut,
    kUSBControlReqIn,
    kUSBOpenPipe,
    kUSBClosePipe,
    kUSBReadPipe,
    kUSBWritePipe,
    kUSBGetPipeStatus,
    kUSBAbortPipe,
    kUSBResetPipe,
    kUSBSetPipeIdle,
    kUSBSetPipeActive,
    kUSBClearPipeStall,
    kUSBReadPipeOOL,
    kUSBWritePipeOOL,
    kUSBControlReqOutOOL,
    kUSBControlReqInOOL,
    kUSBResetDevice,
    kNumUSBMethods
};

// use a structure because there's a limit of 6 total arguments
// to a user client method.
typedef struct devReqOOL {
    void *buf;
    UInt32 pipe;
    UInt32 sizeIn;
    UInt16 wValue;
    UInt16 wIndex;
    UInt8 bmreqtype;
    UInt8 request;
} DevReqOOL;

#ifdef __cplusplus
}       
#endif

#endif /* _USB_H */
