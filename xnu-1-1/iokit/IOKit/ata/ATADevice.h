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
 *    ATADevice.h
 *
 */

#define ATAPropertyProtocol		"ATA Protocol"			/* IOCString */
#define ATAPropertyDeviceNumber 	"ATA Device Number"    		/* OSNumber  */
#define ATAPropertyDeviceType		"ATA Device Type"		/* IOCString */
#define ATAPropertyDeviceId		"ATA Device Id"			/* OSNumber  */
#define ATAPropertyModelNumber		"ATA Device Model Number"	/* IOCString */
#define ATAPropertyFirmwareRev		"ATA Device Firmware Revision"  /* IOCString */
#define ATAPropertyVendorName		"ATA Device Vendor Name"	/* IOCString */
#define ATAPropertyProductName		"ATA Device Product Name"	/* IOCString */
#define ATAPropertyProductRevision	"ATA Device Product Revision"	/* IOCString */
#define ATAPropertyLocation		"IOUnit"			/* OSNumber  */

#define ATAMaxProperties		9

#define ATAPropertyProtocolATA		"ATA"
#define ATAPropertyProtocolATAPI	"ATAPI"

#define ATADeviceTypeDisk		"Disk"
#define ATADeviceTypeTape		"Tape"
#define ATADeviceTypeCDRom		"CDRom"
#define ATADeviceTypeScanner		"Scanner"
#define ATADeviceTypeOther		"Other"

#define ATADeviceIdDisk			0x00
#define ATADeviceIdTape			0x01
#define ATADeviceIdCDRom		0x05
#define ATADeviceIdScanner		0x06
