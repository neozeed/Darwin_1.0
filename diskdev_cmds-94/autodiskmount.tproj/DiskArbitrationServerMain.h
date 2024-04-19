//------------------------------------------------------------------------

#include <mach/mach.h>

#include "DiskArbitrationTypes.h"

//------------------------------------------------------------------------

extern int gDebug;

//------------------------------------------------------------------------

struct Client {
	struct Client * next;
	mach_port_t		port;
	unsigned		pid;
	unsigned		flags;
	int				isNew;
};

typedef struct Client Client;
typedef struct Client * ClientPtr;

enum {
	kMaxNumDiskArbitrationDisks = 64,
};

enum {
	kDiskStateIdle = 0,		/* may be mounted or unmounted */
	kDiskStateNew,			/* not yet probed for mounting */
	kDiskStateUnmounted,	/* newly unmounted - report this only to existing clients */
	kDiskStateEjected		/* newly ejected - report this only to existing clients */
};

enum DiskFamily {
	kDiskFamily_Null = 0,
	kDiskFamily_SCSI,
	kDiskFamily_IDE,
	kDiskFamily_Floppy,
	kDiskFamily_File,
	kDiskFamily_AFP,
};

typedef enum DiskFamily DiskFamily;

struct Disk {
	struct Disk *	next;
	char *			ioBSDName;
	int				ioBSDUnit;
	char *			ioContent;
    char *			ioMediaNameOrNull;
	char *			mountpoint;
	DiskFamily		family;
	unsigned		flags;
	int				state;
};

typedef struct Disk Disk;
typedef struct Disk * DiskPtr;

//------------------------------------------------------------------------

ClientPtr NewClient( mach_port_t port, unsigned pid, unsigned flags );

void PrintClient(ClientPtr clientPtr);
void PrintClients(void);

DiskPtr NewDisk( char * ioBSDName, int ioBSDUnit, char * ioContentOrNull, DiskFamily family, char * mountpoint, char * ioMediaNameOrNull, unsigned flags );

void DiskSetMountpoint( DiskPtr diskPtr, const char * mountpoint );

DiskPtr LookupDiskByIOBSDName( char * ioBSDName );

int UnmountDisk( DiskPtr diskPtr );

int UnmountAllPartitions( DiskPtr diskPtr );

int EjectDisk( DiskPtr diskPtr );

void DeleteDisk( DiskPtr diskPtr );

void PrintDisks(void);

//------------------------------------------------------------------------

int DiskArbitrationServerMain(int argc, char* argv[]);

kern_return_t EnableDeathNotifications(mach_port_t port);

//------------------------------------------------------------------------

typedef struct {
    boolean_t	eject_removable;
    boolean_t	do_removable;
    boolean_t	verbose;
    boolean_t	do_mount;
    boolean_t	debug;
    DiskPtr		Disks;
    unsigned	NumDisks;
    ClientPtr	Clients;
    unsigned	NumClients;
} GlobalStruct;

extern GlobalStruct g;

//------------------------------------------------------------------------

