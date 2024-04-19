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
 * ATADevice.h
 *
 */

typedef struct ATAIdentify
{
    UInt16	generalConfiguration;
    UInt16	logicalCylinders;
    UInt16	reserved_1[1];
    UInt16	logicalHeads;
    UInt16	reserved_2[2];
    UInt16	logicalSectorsPerTrack;    
    UInt16	reserved_3[3];
    char	serialNumber[20];
    UInt16	reserved_4[3];
    char        firmwareRevision[8];
    char	modelNumber[40];
    UInt16	multipleModeSectors;
    UInt16	reserved_5[1];
    UInt16	capabilities1;
    UInt16	capabilities2;
    UInt16	pioMode;
    UInt16	reserved_6[1];
    UInt16	validFields;
    UInt16	currentLogicalCylinders;
    UInt16	currentLogicalHeads;
    UInt16	currentLogicalSectorsPerTrack;
    UInt16	currentAddressableSectors[2];
    UInt16	currentMultipleModeSectors;
    UInt16	userAddressableSectors[2];
    UInt16	reserved_7[1];
    UInt16	dmaModes;
    UInt16	advancedPIOModes;
    UInt16	minDMACycleTime;
    UInt16	recDMACycleTime;
    UInt16	minPIOCycleTimeNoIORDY;
    UInt16	minPIOCyclcTimeIORDY;
    UInt16	reserved_8[2];
    UInt16	busReleaseLatency;
    UInt16	serviceLatency;
    UInt16	reserved_9[2];
    UInt16	queueDepth;
    UInt16	reserved_10[4];
    UInt16	versionMajor;
    UInt16	versionMinor;
    UInt16	commandSetsSupported1;
    UInt16	commandSetsSupported2;
    UInt16	commandSetsSupported3;
    UInt16	commandSetsEnabled1;
    UInt16	commandSetsEnabled2;
    UInt16	commandSetsDefault;
    UInt16	ultraDMAModes;
    UInt16	securityEraseTime;
    UInt16	securityEnhancedEraseTime;
    UInt16	currentAdvPowerMgtValue;
    UInt16	reserved_11[35];
    UInt16	removableMediaSupported;
    UInt16	securityStatus;
    UInt16      reserved_12[127];
};


typedef struct ATAPIInquiry
{
    char        reserved[8];
    char	vendorName[8];
    char	productName[16];
    char	productRevision[4];
};

typedef struct ATAPISenseData
{
    char	reserved[32];
};

#define MAX_ATA_REGS		9

enum ATARegs
{
    /*
     * ATA Register ordinals
     */
    ataRegData			= 0x00,		
    ataRegFeatures		= 0x01,		
    ataRegSectorCount		= 0x02,		
    ataRegSectorNumber		= 0x03,		
    ataRegCylinderLow		= 0x04,		
    ataRegCylinderHigh		= 0x05,
    ataRegDriveHead		= 0x06,
    ataRegCommand		= 0x07,

    ataRegError			= 0x01,
    ataRegStatus		= 0x07,

    ataRegDeviceControl		= 0x08,

    ataRegAltStatus		= 0x08,

    /*
     * ATAPI Register ordinals
     */
    atapiRegData		= 0x00,
    atapiRegFeatures		= 0x01,
    atapiRegIntReason		= 0x02,
    atapiRegByteCountLow	= 0x04,
    atapiRegByteCountHigh	= 0x05,
    atapiRegDeviceSelect	= 0x06,
    atapiRegCommand		= 0x07,

    atapiRegError		= 0x01,
    atapiRegStatus		= 0x07,

    atapiRegDeviceControl	= 0x08,

    atapiRegAlternateStatus	= 0x08,
};

enum ATAPIIntReason
{
    atapiIntReasonCD		= 0x01,
    atapiIntReasonIO		= 0x02,
    atapiIntReasonREL		= 0x04,
    atapiIntReasonTagBit	= 0x08,
    atapiIntReasonTagMask	= 0xf8,
};

enum ATACommand
{
    ataModeCHS			= 0xa0, 
    ataModeLBA			= 0xe0,
 
    ataCommandSetFeatures	= 0xef,

    ataCommandIdentify		= 0xec,

    ataCommandReadSector	= 0x20,

    atapiCommandReset		= 0x08,
    atapiCommandPacket		= 0xa0,
    atapiCommandIdentify	= 0xa1,
};

enum ATAFeatures
{
    ataFeatureTransferMode		= 0x03,	
        ataTransferModePIODefault	= 0x00,		// SectorCount settings (or'd w/Mode)	
        ataTransferModePIOwFC		= 0x08,		
        ataTransferModeDMA		= 0x20,
        ataTransferModeUltraDMA33	= 0x40,
        ataTransferModeMask		= 0x07,
};


enum ATAStatus
{
    ataStatusERR		= 0x01,
    ataStatusIDX		= 0x02,
    ataStatusECC		= 0x04,
    ataStatusDRQ		= 0x08,
    ataStatusSC			= 0x10,
    ataStatusDF			= 0x20,
    ataStatusDRDY		= 0x40,
    ataStatusBSY		= 0x80,

    atapiStatusCHK		= 0x01,
    atapiStatusDRQ		= 0x08,
    atapiStatusSERV		= 0x10,
    atapiStatusDMRD		= 0x20,
    atapiStatusDRDY		= 0x40,
    atapiStatusBSY		= 0x80,
};

enum ATAError
{
   ataErrorNM			= 0x02,
   ataErrorABRT			= 0x04,
   ataErrorMCR			= 0x08,
   ataErrorIDNF			= 0x10,
   ataErrorMC			= 0x20,
   ataErrorWP			= 0x40,

   atapiErrorILI		= 0x01,
   atapiErrorEOM		= 0x02,
   atapiErrorABRT		= 0x04,
   atapiSenseKeyBit		= 0x10,
   atapiSenseKeyMask		= 0xf0,
};

enum ATADeviceControl
{
    ataDevControlnIEN		= 0x02,
    ataDevControlSRST		= 0x04,
};

enum ATASignatures
{
    ataSignatureSectorCount	= 0x01,
    ataSignatureSectorNumber	= 0x01,
    ataSignatureCylinderLow	= 0x00,
    ataSignatureCylinderHigh	= 0x00,

    atapiSignatureCylinderLow	= 0x14,
    atapiSignatureCylinderHigh  = 0xeb,
};
