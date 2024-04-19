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
 * SCSIDevice.h
 *
 */

#ifndef _SCSIDEVICE_H
#define _SCSIDEVICE_H

#define kDefaultInquirySize	255

typedef struct SCSITargetLun
{
    UInt8	target;
    UInt8	lun;
    UInt8	reserved[2];
} SCSITargetLun;

typedef struct SCSILunParms
{
    bool	disableDisconnect;

    UInt32	reserved[16];
} SCSILunParms;

typedef struct SCSITargetParms
{
    UInt32		transferPeriodpS;
    UInt32		transferOffset;
    UInt32		transferWidth;

    bool		enableTagQueuing;
    bool                disableParity;

    UInt32	reserved[16];
} SCSITargetParms;
    
enum SCSIDeviceTimeouts
{
    kSCSITimerIntervalmS 	= 500,
    kSCSIProbeTimeoutmS	 	= 5000,
    kSCSIResetIntervalmS 	= 3000,
    kSCSIAbortTimeoutmS  	= 5000,
    kSCSIReqSenseTimeoutmS	= 5000,
    kSCSIDisableTimeoutmS	= 5000,
};

enum SCSIClientMessage
{
    kSCSIClientMsgNone			=  0x00005000,
    kSCSIClientMsgDeviceAbort,
    kSCSIClientMsgDeviceReset,
    kSCSIClientMsgBusReset,		

    kSCSIClientMsgDone			= 0x80000000,
};

enum SCSIQueueType
{
    kQTypeNormalQ		= 0,
    kQTypeBypassQ		= 1,
};

enum SCSIQueuePosition
{
    kQPositionTail		= 0,
    kQPositionHead		= 1,
};


#define kSCSIMaxProperties		8

#define kSCSIPropertyTarget		"SCSI Target"		/* IOOffset	*/
#define kSCSIPropertyLun		"SCSI LUN"		/* IOOffset	*/
#define kSCSIPropertyIOUnit		"IOUnit"		/* IOOffset	*/
#define kSCSIPropertyDeviceTypeID	"SCSI Type ID"		/* IOOffset	*/
#define kSCSIPropertyDeviceTypeName	"SCSI Type Name"	/* IOString	*/
#define kSCSIPropertyDeviceRemovable	"SCSI Removable Device"	/* IOBoolean	*/
#define kSCSIPropertyVendorName		"SCSI Vendor Name"	/* IOString	*/
#define kSCSIPropertyProductName	"SCSI Product Name"	/* IOString	*/
#define kSCSIPropertyProductRevision	"SCSI Product Revision"	/* IOString	*/

#endif
