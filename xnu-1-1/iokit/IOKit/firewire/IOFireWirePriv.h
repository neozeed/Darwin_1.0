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
 * Copyright (c) 1999 Apple Computer, Inc.  All rights reserved.
 *
 * HISTORY
 *
 */

#ifndef __IOFIREWIREPRIV__
#define __IOFIREWIREPRIV__

////////////////////////////////////////////////////////////////////////////////
//
// Private FireWire defs.
//

////////////////////////////////////////////////////////////////////////////////
//
// CSR ROM defs.
//

// Apple specific key values.

enum
{
	kCSRGenerationKey			= 0x38,
	kCSRInvalidKey				= 0x39
};

// Defs for local CSR ROM tree entries.

struct CSRROMEntryDataStruct
{
	struct CSRROMEntryDataStruct	// Links to related entries.
				*pParentCSRROMEntryData,
				*pPrevCSRROMEntryData,
				*pNextCSRROMEntryData;
	UInt32			entryType;			// Type of entry.
};
typedef struct CSRROMEntryDataStruct
				CSRROMEntryData,
				*CSRROMEntryDataPtr;

struct CSRROMBasicEntryDataStruct
{
	UInt32	keyValue;				// Key value of this entry.
};
typedef struct CSRROMBasicEntryDataStruct
				CSRROMBasicEntryData,
				*CSRROMBasicEntryDataPtr;

struct CSRROMImmediateDataStruct
{
	CSRROMEntryData		csrROMEntryData;		// Data common to all CSR ROM entries.
	CSRROMBasicEntryData	csrROMBasicEntryData;	// Data common to all basic entries.
	UInt32			immediateData;			// Data for this entry (only 24-bits worth).
};
typedef struct CSRROMImmediateDataStruct
				CSRROMImmediateData,
				*CSRROMImmediateDataPtr;

struct CSRROMOffsetDataStruct
{
	CSRROMEntryData		csrROMEntryData;	// Data common to all CSR ROM entries.
	CSRROMBasicEntryData	csrROMBasicEntryData;	// Data common to all basic entries.
	UInt32			address;		// Lower 32-bits of address of offset entry.
};
typedef struct CSRROMOffsetDataStruct
				CSRROMOffsetData,
				*CSRROMOffsetDataPtr;

struct CSRROMLeafDataStruct
{
	CSRROMEntryData		csrROMEntryData;	// Data common to all CSR ROM entries.
	CSRROMBasicEntryData	csrROMBasicEntryData;	// Data common to all basic entries.
	UInt8 *			leafData;		// Data for this leaf.
	UInt32			leafDataSize;		// Size of above data.
};
typedef struct CSRROMLeafDataStruct
				CSRROMLeafData,
				*CSRROMLeafDataPtr;

struct CSRROMDirectoryDataStruct
{
	CSRROMEntryData		csrROMEntryData;	// Data common to all CSR ROM entries.
	CSRROMBasicEntryData	csrROMBasicEntryData;	// Data common to all basic entries.
	CSRROMEntryDataPtr	pChildCSRROMEntryData;	// Pointer to first child.
	UInt32			numChildren;		// Number of children in this directory.
};
typedef struct CSRROMDirectoryDataStruct
				CSRROMDirectoryData,
				*CSRROMDirectoryDataPtr;


// Defs for CSR ROM entry IDs.

enum
{
	kInvalidCSRROMEntryIDType	= 0,
	kLocalCSRROMEntryIDType		= 1,
	kRemoteCSRROMEntryIDType	= 2
};
enum
{
	kCSRROMPathBlockSize	= 10
};


struct CSRROMEntryIDDataStruct
{
	UInt32						entryType;				// Type of entry ID.
};
typedef struct CSRROMEntryIDDataStruct
								CSRROMEntryIDData,
								*CSRROMEntryIDDataPtr;

struct CSRROMLocalIDDataStruct
{
	CSRROMEntryIDData			csrROMEntryIDData;		// Data common to all CSR ROM entry IDs.
	CSRROMEntryDataPtr			pCSRROMEntryData;		// Pointer to local entry data.
};
typedef struct CSRROMLocalIDDataStruct
								CSRROMLocalIDData,
								*CSRROMLocalIDDataPtr;

struct CSRROMRemoteIDDataStruct
{
	CSRROMEntryIDData	csrROMEntryIDData;	// Data common to all CSR ROM entry IDs.
	const UInt32 *		data;			// pointer to ROM.
	UInt32			physicalPath[kCSRROMPathBlockSize];
	UInt32			pathSize;		// Size of above paths.
};
typedef struct CSRROMRemoteIDDataStruct
					CSRROMRemoteIDData,
					*CSRROMRemoteIDDataPtr;


////////////////////////////////////////////////////////////////////////////////
//
// CSR ROM search defs.
//

struct CSRROMDirectoryEntry
{
	UInt32			entryAddress;
	UInt32			directoryAddress;
	UInt32			directorySize;
};
typedef struct CSRROMDirectoryEntry
				CSRROMDirectoryEntry,
				*CSRROMDirectoryEntryPtr;

struct CSRROMEntryIteratorRecStruct
{
	const UInt32 *		data;
	UInt32			logicalPath[kCSRROMPathBlockSize];
	UInt32			physicalPath[kCSRROMPathBlockSize];
	UInt32			pathSize;
	CSRROMIterationOp	relationship;
	bool			reset;
};
typedef struct CSRROMEntryIteratorRecStruct
					CSRROMEntryIteratorRec,
					*CSRROMEntryIteratorRecPtr;


#if 0
////////////////////////////////////////////////////////////////////////////////
// PCR defs
 
enum
{
	kFWPCRCount			= 31,
	kFWPCRSpaceSize		= 0xFF,
	kFWInputPCROffset	= 0x80,
	kFWPCRBase			= 0x0900,


	kFWPCRAllocated		= 1
};

typedef struct
{
	FWAddressSpaceID 		addressSpace;
	UInt32					*buffer;
	UInt32					oPCRFlags[kFWPCRCount];
	UInt32					iPCRFlags[kFWPCRCount];
} FWPCRSpaceData, *FWPCRSpaceDataPtr;

#endif

struct FWIRMRegistersStruct
{
    UInt32		busManagerID;
    UInt32		bandwidthAvailable;
    UInt32		channelsAvailableHi;
    UInt32		channelsAvailableLo;
};
typedef struct FWIRMRegistersStruct FWIRMRegisters, *FWIRMRegistersPtr;

#if 0
struct FWIMDataStruct
{
	FWReferenceData				fwReferenceData;		// Reference data.
	struct FWIMDataStruct		*pPrevFWIMData;			// Link to previous FWIM data record.
	struct FWIMDataStruct		*pNextFWIMData;			// Link to next FWIM data record.
	FWIMID						fwimID;					// Our ID for the FWIM.
	FWIMPluginDispatchTablePtr	pFWIMPluginDispatchTable;	// FWIM disaptch table (maybe dummy).
	FWIMPluginDispatchTablePtr	pRealFWIMPluginDispatchTable;	// Real FWIM disaptch table.
	RegEntryID					fwimRegistryID;			// Name registry ID of FireWire bus device.
	RegEntryID					baseRegistryID;			// ID to hang devices off of.

	QHdrPtr						fwimCommandQueue;		// Queue for commands to FWIM.
	QHdrPtr						fwimPriorityCommandQueue;	// Queue for commands to FWIM.
	QHdrPtr						fwimResponseCommandQueue;	// Queue for response commands to FWIM.
	QHdrPtr						fwCommandQueue;			// Queue for commands from FireWire services.
	SInt32						fwCommandSIHQueued;		// Number of SIH's we have queued for the FW command queue.
	SInt32						fwimCommandSIHQueued;	// Number of SIH's we have queued for the FWIM command queue.

	struct FWAddressHeapDataStruct
								fwAddressHeapData;		// Address heap.

	struct FWDeviceDataStruct	*fwDeviceDataList;		// Pointer to list of data for all devices
														// controlled by this FWIM.
	struct FWUnitDataStruct		*fwUnitDataList;		// Pointer to list of data for all units
														// controlled by this FWIM.
	struct FWClientDataStruct	*fwClientDataList;		// Pointer to list of data for all clients
														// controlled by this FWIM.
	struct FWDriverDataStruct	*fwDriverDataList;		// Pointer to list of data for all drivers
														// controlled by this FWIM.
	struct FWVDeviceDataStruct	*fwVDeviceDataList;		// Pointer to lisf of data for all virtual devices
														// controlled by this FWIM.
	struct FWPDriverDataStruct	*fwPDriverDataList;		// Pointer to list of data for all protocol drivers
														// controlled by this FWIM.

	FWTopologyMapPtr			pFWTopologyMap;			// FireWire bus topology map.
	UInt32						localNodeID;			// Local node ID of host adapter.
	FWSpeedMapPtr				pFWSpeedMap;			// FireWire speed map.
	bool						validFWTopologyMapTimerSIHQueued;	// True if we've queued an SIH to start the valid topology map
																	// timeout timer.
	bool						validFWTopologyMapTimeoutResetCommandObjectIDInUse;	// True if above command object is in use.
	bool						dialogInUse;				// True if the dialog is already on screen
	bool						needDSAT;					// True if we need to put up the dialog

	TimerID						validFWTopologyMapTimer;	// Timer for receiving a valid topology map.
	SInt32						validFWTopologyMapTimersSet;	// Number of above timers that have been set.
	FWCommandObjectID			validFWTopologyMapTimeoutResetCommandObjectID;	// Command object for initiating a bus reset after
																				// a valid topology map timeout.

	FWDeviceID					localFWDeviceID;		// Reference to local FireWire device.

	FWCommandObjectID			resetCommandObjectID;	// Command object for resetting bus.

	UInt8 *							configurationROM;		// Storage for configuration ROM.
	CSRROMDirectoryData			csrROMRootDirectory;	// Local CSR ROM root directory.
	UInt32						csrROMUpdateInProgress;	// Non-zero indicates CSR ROM being revised

	struct FWDeviceDataStruct	*fwDeviceDataAllocation;	// Pre-allocated block of device data record.
	struct FWDeviceDataStruct	*fwDeviceDataPool;		// Pool of pre-allocated device data records.

	FWIsochResourceManagerID	fwIsochResourceManagerID;	// Reference to isochronous resource manager for this FWIM.
	struct FWIsochResourceManagerDataStruct
								*pFWIsochResourceManagerData;	// Allocated data for isochronous resource manager.
	FWIRMRegisters				fwIRMRegisters;			// FireWire isochronous resource manager registers.
	FWCommandObjectID			busManagementAsynchCommandObjectID;	// Command object used for bus management.
	struct IsochChannelDataStruct
								*isochChannelDataList;	// List of isochronous channels operating on this FWIM.

	UInt32						csrCoreState;			// Value of core CSR state register.
	FWIMCommandResources		cmstrFWIMCommandResources;	// FWIM command resources for executing cmstr state bit.
	FWIMCommandParams			cmstrFWIMCommandParams;	// FWIM command params for executing cmstr state bit.
	UInt32						cmstrFWIMCommandParamsInUse;	// Lock for cmstrFWIMCommandParams.

	FWCommandObjectID			fwDeviceScanAsynchCommandObjectID;	// Command object for doing device scanning.
	UInt32						fwDeviceScanAsynchCommandObjectIDInUse;	// Lock for command object for doing device scanning.
	CSRNodeUniqueID				deviceScanUniqueID;		// Storage for unique ID for device scan.
	UInt32						deviceScanFiller;		//zzz really need a better way to get CSR ROM generation.
	UInt32						deviceScanCSRROMGeneration;	// Storage for CSR ROM generation for device scan.//zzz must come afterdeviceScanUniqueID
	UInt32						*pCurrentSelfID;		// Pointer to current self ID in device scan.
	UInt32						nextNodeNum;			// Next node num in device scan.

	FWClientCommandResources	resetNotifyFWClientCommandResources;	// Resources for notifying clients of bus restes.
	FWClientInterfaceParams		resetNotifyFWClientInterfaceParams;	// Interface params for notifying clients of bus resets.
	UInt32						resetNotifyFWClientInterfaceParamsInUse;	// Lock for resetNotifyFWClientInterfaceParams.

	FWClientCommandResources	busManagementNotifyFWClientCommandResources;	// Resources for notifying clients of bus
																				// resets when bus management has been established.
	FWClientInterfaceParams		busManagementNotifyFWClientInterfaceParams;	// Interface params for notifying clients of bus
																			// resets when bus management has been established.
	UInt32						busManagementNotifyFWClientInterfaceParamsInUse;	// Lock for busManagementNotifyFWClientInterfaceParams.

	QHdrPtr						fwimProcessQueue;		// Queue for request processing.
	SInt32						fwimProcessSIHQueued;	// Number of SIH's we have queued for the FWIM process queue.
	FWIMProcessResourcesPtr		fwimProcessResourcesPool;	// Pool of resources for processing requests.
	QHdrPtr						fwimProcessResourcesInactive;	// List of inactive resources for processing requests.

	SoftwareInterruptID			processBusResetSWIntID;	// Software interrupt ID for processing bus resets.

	FWIMProcessAsynchParams		fwimProcessLocalAsynchParams;	// Params for processing local asynch requests.
	FWPCRSpaceData				pcrSpaceData;
	FWAddressSpaceID			csrAddressSpaceID;		// Address space ID for Config ROM
	FWAddressSpaceID			irmAddressSpaceID;		// ... For Isoch Resource Manager registers
	FWAddressSpaceID			coreCSRStateAddressSpaceID;	// ... For core CSR state variablers
	FWAddressSpaceID			fcpAddressSpaceID;		// ... For FCP registers

	UInt32						fwimFeatures;			// Store feature flags provided by FWIM
	UInt32						fwimCSRROMMapLength;	// ... also provided by FWIM
	UInt32						fwimDeciWatts;			// Deciwatts provided by hardware
	UInt32						fwimDeciVoltsMinimum;	// Minimum decivolts at above load
	UInt32						fwimDeciVoltsMaximum;	// Maximum decivolts at no load

	UInt32						deciWattsAvailable;		// How much local power is not being used

	FWAddressSpaceDataPtr		fwPhysicalAddressSpaceDataList;	// List of Physical allocations

	QHdrPtr						fwDialogQueue;			// Queue of "Hey put that back!" requests.
	DeferredTask				dialogDT;				// Stupid Secondary Interrupts dont' always do what
														// you want (see FWUIServices.c for gory details)

	UInt32						lastLocalSelfIDQuads[16];	// Last local self ID's we saw
	UInt32						lastLocalSelfIDSize;	// Last size returned (should never change)

	UInt8						retryOnAckDataErrorNodeIDs[64];

	UInt32						fwimSpecificData;		// Data specific to FWIM.
};
typedef struct FWIMDataStruct	FWIMData, *FWIMDataPtr;



////////////////////////////////////////////////////////////////////////////////
//
// FireWire device defs.
//

struct FWDeviceDataStruct
{
	FWReferenceData				fwReferenceData;		// Reference data.

	struct FWDeviceDataStruct	*pPrevFWDeviceData;		// Link to previous FireWire device data record
	struct FWDeviceDataStruct	*pNextFWDeviceData;		// Link to next FireWire device data record

	FWIMID						fwimID;					// ID of our FWIM.

	UInt16						processingCommand;			
	UInt16						nextCommand;			// command and pending command fields

	bool						newDevice;				// True if this device was just created.
	bool						scanningDevice;			// True if we're still scanning for device.
	bool						deviceConnected;		// True if device is connected.
	bool						driverRequestedReplug;	// True if we need the "Hey put that back dialog"

// zzz Need to go through all these structures to make sure everything is aligned right. Why
// make life difficult for the poor µP?

	UInt32						numConnections;			// Number of connections to this device.

	UInt32						nodeID;					// Node ID for this device.
	UInt32						selfIDs[8];				// Self IDs sent by this device.
	UInt32						numSelfIDs;				// Number of self IDs above.
	UInt32						csrROMGeneration;		// Generation number of CSR ROM.
	CSRNodeUniqueID				uniqueID;				// ID unique to this device.

	UInt32						maxPayloadSize;			// Maximum payload size to use for device's bus transactions.

	RegEntryID					fwDeviceRegistryID;		// Name registry ID of FireWire device.

	UInt32						minimumDeciVolts;		// Minimum volts to operate device
	UInt32						totalDeciWattsAllocated;// Total power previously allocated to device
	UInt32						totalDeciWattsRequested;// Notify device if this becomes available
	UInt32						powerFlags;				// Flags for power behavior

	bool						retryOnAckDataErr;		// Violate 1394-1995 and just send the packet again
};
typedef struct FWDeviceDataStruct
								FWDeviceData,
								*FWDeviceDataPtr;


////////////////////////////////////////////////////////////////////////////////
//
// FireWire unit defs.
//

struct FWUnitDataStruct
{
	FWReferenceData				fwReferenceData;		// Reference data.

	struct FWUnitDataStruct		*pPrevFWUnitData;		// Link to previous FireWire unit data record
	struct FWUnitDataStruct		*pNextFWUnitData;		// Link to next FireWire unit data record

	FWDeviceID					fwDeviceID;				// ID of device this unit belongs to.
	CSRROMEntryID				csrUnitID;				// CSR ROM entry ID of FireWire unit directory.
	UInt32						specID;					// Spec ID for this unit.
	UInt32						swVersion;				// Software version for this unit.
	RegEntryID					unitRegEntryID;			// Name registry ID of FireWire unit.
	UInt32						numConnections;			// Number of connections to unit.

	bool						newUnit;				// True if this unit was just created.
	bool						unitAdded;				// True if this unit was just added.
	bool						unitConnected;			// True if unit is connected.
};
typedef struct FWUnitDataStruct	FWUnitData,
								*FWUnitDataPtr;

#endif

////////////////////////////////////////////////////////////////////////////////
//
// Private FireWire Services procedure defs.
//

UInt16	FWComputeCRC16 (UInt32 *pQuads, UInt32 numQuads);

CSRROMEntryID	FWCSRROMCreateEntryID(void);
void  FWCSRROMInvalidateEntryIDType(CSRROMEntryID csrROMEntryID);
void  FWCSRROMDisposeEntryID(CSRROMEntryID csrROMEntryID);

#endif /* __IOFIREWIREPRIV__ */

