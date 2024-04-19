
/* DiskArbitrationTypes.h */

#ifndef __DISKARBITRATIONTYPES_H
#define __DISKARBITRATIONTYPES_H

/* The disk arbitration server registers with the bootstrap server under this name */

#define DISKARB_SERVER_NAME "DiskArbitration"

/* For the errorCode return value. */

enum
{
	kDiskArbitrationNoError								= 0,
};

/* For the <flags> parameter to DiskArbitrationRegister. */

enum
{
	kDiskArbitrationNotifyNone							= 0x00000000,
	kDiskArbitrationNotifyAll							= 0xFFFFFFFF,

	kDiskArbitrationNotifyDiskAppeared					= 1 << 0,
	kDiskArbitrationNotifyUnmount						= 1 << 1,
	kDiskArbitrationNotifyDiskAppearedWithMountpoint	= 1 << 2,
};

enum
{
	kDiskArbNotifyNone							= 0x00000000,
	kDiskArbNotifyAll							= 0xFFFFFFFF,

	kDiskArbNotifyDiskAppearedWithoutMountpoint				= 1 << 0,
	kDiskArbNotifyUnmount									= 1 << 1,
	kDiskArbNotifyDiskAppearedWithMountpoint				= 1 << 2,
	kDiskArbNotifyDiskAppeared								= 1 << 3,
};

/* Beware: these definitions must be kept in sync with ClientToServer.defs and ServerToClient.defs */

typedef char DiskArbitrationDiskIdentifier[ 1024 ];
typedef char DiskArbitrationMountpoint[ 1024 ];
typedef char DiskArbitrationIOContent[ 1024 ];

/* For the <flags> parameter to DiskArbitrationDiskAppeared. */

enum
{
	kDiskArbitrationDiskAppearedNoFlags					= 0x00000000,

	kDiskArbitrationDiskAppearedLockedMask				= 1 << 0,
	kDiskArbitrationDiskAppearedEjectableMask			= 1 << 1,
	kDiskArbitrationDiskAppearedWholeDiskMask			= 1 << 2,
	kDiskArbitrationDiskAppearedNetworkDiskMask			= 1 << 3,
	kDiskArbitrationDiskAppearedBeingCheckedMask		= 1 << 4,
	kDiskArbitrationDiskAppearedNonLeafDiskMask			= 1 << 5,
};

enum
{
	kDiskArbDiskAppearedNoFlags					= 0x00000000,

	kDiskArbDiskAppearedLockedMask				= 1 << 0,
	kDiskArbDiskAppearedEjectableMask			= 1 << 1,
	kDiskArbDiskAppearedWholeDiskMask			= 1 << 2,
	kDiskArbDiskAppearedNetworkDiskMask			= 1 << 3,
	kDiskArbDiskAppearedBeingCheckedMask		= 1 << 4,
	kDiskArbDiskAppearedNonLeafDiskMask			= 1 << 5,
};


#endif

