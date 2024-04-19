#import <libc.h>
#import <mach/mach.h>
#import <mach/mach_error.h>
#import <mach/message.h>
#import <servers/bootstrap.h>
#import <mach/mach_error.h>

#include <sys/types.h>
#include <errno.h>
#include <stdio.h>
#include <dev/disk.h>

#include <IOKit/OSMessageNotification.h>
#include <IOKit/IOKitLib.h>

#include "ServerToClient.h"
#include "DiskArbitrationServer.h"
#include "DiskArbitrationTypes.h"
#include "GetRegistry.h"

#include <mach/mach_interface.h>
#include <IOKit/IOBSD.h>
#include <IOKit/storage/IOMedia.h>

#include <syslog.h> /* bek - 12/18/98 */

#include "DiskArbitrationServerMain.h"

void autodiskmount(void);
	
/*
	External globals
*/

#ifdef DEBUG
int gDebug = 1;
#else
int gDebug = 0;
#endif

int gDaemon;

/*
	External routines
*/

extern void ClientDeath(mach_port_t clientPort);

extern boolean_t DiskArbitrationServer_server(mach_msg_header_t * msg, mach_msg_header_t * reply);

/*
	Public globals
*/

/*
	Static constants
*/

enum {
	kMsgSize = 2048, // bek - 7/15/99 - Messages can contain path names as long as PATH_MAX = 1024
};

/*
	Static globals
*/

static char * programName = NULL;

static mach_port_t gNotifyPort = MACH_PORT_NULL;

/*
	Static function prototypes
*/

static void LogErrorMessage(const char *format, ...);

static kern_return_t InitNotifyPort(void);
static mach_port_t GetNotifyPort(void);
static boolean_t MessageIsNotificationDeath(const mach_msg_header_t *msg, mach_port_t *deadPort);

struct IONotificationMsg {
	mach_msg_header_t		msgHdr;
	OSNotificationHeader	notifyHeader;
	mach_msg_trailer_t		trailer;
};

typedef struct IONotificationMsg IONotificationMsg, * IONotificationMsgPtr;

static void HandleIONotificationMsg( IONotificationMsgPtr ioNotificationMsgPtr );

/*
	Static functions
*/

static kern_return_t InitNotifyPort(void)
{
	kern_return_t r;

	r = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &gNotifyPort);
	if (r != KERN_SUCCESS)
	{
		LogErrorMessage("(%s:%d) mach_port_allocate failed: {0x%x} %s\n", __FILE__, __LINE__, r, mach_error_string(r));
		return MACH_PORT_NULL;
	}

#if DEBUG
	printf("%s: gNotifyPort = $%08x\n", programName, (int)gNotifyPort);
#endif

	return r;

} // InitNotifyPort
	
static mach_port_t GetNotifyPort(void)
{

	return gNotifyPort;
	
} // GetNotifyPort

//------------------------------------------------------------------------

static boolean_t MessageIsNotificationDeath(const mach_msg_header_t *msg, mach_port_t *deadPort)
{
	boolean_t result;
	
	if ( GetNotifyPort() == msg->msgh_local_port )
	{
		if ( MACH_NOTIFY_DEAD_NAME == msg->msgh_id )
		{
			// If the caller supplied a pointer, fill it with the dead port's name
			if ( deadPort != NULL )
			{
				const mach_dead_name_notification_t *deathMessage = (const mach_dead_name_notification_t *)msg;
				*deadPort = deathMessage->not_port;
			}
			result = TRUE;
			goto Return;
		}
		else
		{
			/* A message for the notification port other than dead name notification */
			// This should never happen.  Log it.
			LogErrorMessage("(%s:%d) received unrecognized message (id=0x%x) on notify port\n", __FILE__, __LINE__, (int)msg->msgh_id);
			result = FALSE;
			goto Return;
		}
	}
	else
	{
		/* Not a message for the notification port */
		result = FALSE;
		goto Return;
	}

Return:
	return result;

} // MessageIsNotificationDeath

//------------------------------------------------------------------------

static void LogErrorMessage(const char *format, ...)
{
        va_list ap;
        
        va_start(ap, format);

        // syslog first....
        openlog(programName, LOG_PID | LOG_CONS, LOG_DAEMON);
        // This should be LOG_WARNING instead of LOG_ERR according to "man 3 syslog", but experiments show that would prevent it from appearing in the log.
        vsyslog(LOG_ERR, format, ap);
        closelog();

        // ...printf second
        printf("%s [%d]", programName, getpid());
        vprintf(format, ap);

        va_end(ap);

} // LogErrorMessage

//------------------------------------------------------------------------

static void HandleIONotificationMsg( IONotificationMsgPtr msg )
{
	kern_return_t r;
	unsigned long int  	notifyType;
	unsigned long int  	ref;
	
	r = OSGetNotificationFromMessage(
		&msg->msgHdr,	//	mach_msg_header_t     * msg,
		0,				//	unsigned int            index,
		&notifyType,	//	unsigned long int     * type,
		&ref,			//	unsigned long int     * reference,
		0,				//	void                 ** content,
		0 );			//	vm_size_t             * size
	if ( r )
	{
		if ( gDebug ) printf("%s(%d): OSGetNotificationFromMessage returned error %d\n", __FUNCTION__, __LINE__, (int)r);
		goto Return;
	}

	if ( gDebug )
	{
		// we passed a string for the refcon
		printf("got notification, type=%d(%s), local=%d, remote=%d\n",
				(int)notifyType, (char*)ref,
				(int)msg->msgHdr.msgh_local_port,
				(int)msg->msgHdr.msgh_remote_port );
	}
			
	// remote port is the notification (an iterator_t) that fired

	GetDisksFromRegistry( (io_iterator_t) msg->msgHdr.msgh_remote_port );
	
	autodiskmount();

Return:
	return;

} // HandleIONotificationMsg

//------------------------------------------------------------------------

/*
	Public functions
*/

//------------------------------------------------------------------------

/*
-- Allocates memory for new client record.
-- Links it into the existing list.
-- Fills in the <port> and <flags> fields and sets <isNew>.
-- Increments the global count of client records.
*/

ClientPtr NewClient( mach_port_t port, unsigned pid, unsigned flags )
{
	ClientPtr result;

	/* Allocate memory */

	result = (ClientPtr) malloc( sizeof( * result ) );
	if ( result == NULL )
	{
		if ( gDebug ) printf("%s(port = $%08x, pid = %d, flags = $%08x): malloc failed!\n", __FUNCTION__, port, pid, flags);
		/* result = NULL; */
		goto Return;
	}

	/* Link it onto the front of the list */

	result->next = g.Clients;
	g.Clients = result;

	/* Fill in the fields */

	result->port = port;
	result->pid = pid;
	result->flags = flags;
	result->isNew = 1;
	
	/* Increment count */

	g.NumClients++;
	
Return:
	return result;

} // NewClient

//------------------------------------------------------------------------

void PrintClient(ClientPtr clientPtr)
{

	printf("port = $%08x, pid = %5d, flags = $%08x\n", clientPtr->port, clientPtr->pid, clientPtr->flags);

} // PrintClients

//------------------------------------------------------------------------

void PrintClients(void)
{
	ClientPtr clientPtr;
	int i;

	printf("g.NumClients = %d\n", g.NumClients);
	for (clientPtr = g.Clients, i = 0; clientPtr != NULL; clientPtr = clientPtr->next, i++)
	{
		PrintClient( clientPtr );
	}

} // PrintClients

//------------------------------------------------------------------------

/*
-- Allocates memory for new disk record.
-- Links it into the existing list.
-- Fills in the <ioBSDName>, <ioContent>, <mountpoint>, and <flags> fields and sets <state> = kDiskStateNew.
-- Makes its own copies of input strings - doesn't retain pointers to its arguments.
-- Increments the global count of disk records.
*/

DiskPtr NewDisk( char * ioBSDName, int ioBSDUnit, char * ioContentOrNull, DiskFamily family, char * mountpoint, char * ioMediaNameOrNull, unsigned flags )
{
	DiskPtr result;
	
	/* Allocate memory */

	result = (DiskPtr) malloc( sizeof( * result ) );
	if ( result == NULL )
	{
		if ( gDebug ) printf("%s(ioBSDName = '%s', ioContentOrNull = '%s', family = %d, mountpoint = '%s', ioMediaNameOrNull = '%s', flags = $%08x): malloc failed!\n", __FUNCTION__, ioBSDName, ioContentOrNull ? ioContentOrNull : "(NULL)", family, mountpoint, ioMediaNameOrNull ? ioMediaNameOrNull : "(NULL)", flags);
		/* result = NULL; */
		goto Return;
	}

	/* Link it onto the front of the list */

	result->next = g.Disks;
	g.Disks = result;

	/* Fill in the fields */

	result->ioBSDName = strdup( ioBSDName ? ioBSDName : "" );
	result->ioBSDUnit = ioBSDUnit;
	result->ioContent = strdup( ioContentOrNull ? ioContentOrNull : "" );
	result->family = family;
	result->mountpoint = strdup( mountpoint ? mountpoint : "" );
	if ( ioMediaNameOrNull )
	{
	    result->ioMediaNameOrNull = strdup( ioMediaNameOrNull );
	}
	else
	{
	    result->ioMediaNameOrNull = NULL;
	}
	
	result->flags = flags;
	result->state = kDiskStateNew;
	
	/* Increment count */

	g.NumDisks++;
	
Return:
	return result;

} // NewDisk


//------------------------------------------------------------------------

void DiskSetMountpoint( DiskPtr diskPtr, const char * mountpoint )
{
	if ( diskPtr->mountpoint )
	{
		free( diskPtr->mountpoint );
	}
	diskPtr->mountpoint = strdup( mountpoint ? mountpoint : "" );

} // DiskSetMountpoint

//------------------------------------------------------------------------

DiskPtr LookupDiskByIOBSDName( char * ioBSDName )
{
	DiskPtr diskPtr;
	
//    if ( gDebug ) printf("%s('%s')\n", __FUNCTION__,  ioBSDName);

	for (diskPtr = g.Disks; diskPtr != NULL; diskPtr = diskPtr->next)
	{
		/* Special case: if diskPtr->ioBSDName is longer than 31 chars, and ioBSDName is 31 chars */

		if ( strlen( ioBSDName ) == 31 && strlen( diskPtr->ioBSDName ) > 31 )
		{
			if ( 0 == strncmp( ioBSDName, diskPtr->ioBSDName, 31 ) )
			{
				goto Return;
			}
		}
		else
		if ( 0 == strcmp( ioBSDName, diskPtr->ioBSDName ) )
		{
			goto Return;
		}
	}

	/* Assert: diskPtr == NULL */

Return:
	return diskPtr;

} // LookupDiskByIOBSDName

//------------------------------------------------------------------------

/*
-- Unmount the disk and delete the mountpoint that was created for it
*/

int UnmountDisk( DiskPtr diskPtr )
{
	int result = 0;

	if ( gDebug ) printf("%s('%s')\n", __FUNCTION__, diskPtr->ioBSDName);

	if ( 0 == strcmp( diskPtr->mountpoint, "" ) )
	{
		if ( gDebug ) printf("%s('%s'): disk not mounted\n", __FUNCTION__, diskPtr->ioBSDName);
		goto Return;
	}

	result = unmount( diskPtr->mountpoint, 0 );
	if ( -1 == result )
	{
		result = errno;
		LogErrorMessage("%s('%s') unmount('%s') failed: %d (%s)\n", __FUNCTION__, diskPtr->ioBSDName, diskPtr->mountpoint, errno, strerror(errno));
		goto Return;
	}

	result = rmdir( diskPtr->mountpoint );
	if ( -1 == result )
	{
		result = errno;
		LogErrorMessage("%s('%s') rmdir('%s') failed: %d (%s)\n", __FUNCTION__, diskPtr->ioBSDName, diskPtr->mountpoint, errno, strerror(errno));
		goto Return;
	}
	
	free( diskPtr->mountpoint );
	diskPtr->mountpoint = strdup( "" ); // We need to send this in messages via MIG, so it cannot be NULL

	diskPtr->state = kDiskStateUnmounted;

Return:
	if ( result && gDebug ) printf("%s('%s') error: %d (%s)\n", __FUNCTION__, diskPtr->ioBSDName, result, strerror(result));
	return result;

} // UnmountDisk

//------------------------------------------------------------------------

/*
-- For each disk, <d>, that comes from the same device (i.e., whose "family"
-- and "device number" match the given disk), call Unmount(<d>).
-- Ignore errors - attempt to unmount each partition.  This should help with
-- debugging problems, since only the "problem" partition(s) will remain mounted.
--
-- Handle network volumes (kDiskArbitrationDiskAppearedNetworkDiskMask) specially:
-- do not try to parse their names, and only unmount the one disk.
--
*/

int UnmountAllPartitions( DiskPtr diskPtr )
{
	int result = 0; /* mandatory initialization */
	DiskPtr dp;
	int	family1, family2;
	int	deviceNum1, deviceNum2;
	int isNetwork;
	
    if ( gDebug ) printf("%s('%s')\n", __FUNCTION__, diskPtr->ioBSDName);

	family1 = diskPtr->family;
	deviceNum1 = diskPtr->ioBSDUnit;

	isNetwork = ( 0 != ( diskPtr->flags & kDiskArbitrationDiskAppearedNetworkDiskMask ) );

	if ( isNetwork )
	{
		result = UnmountDisk( diskPtr ); // Ignore errors - attempt to unmount each partition
	}
	else
	{
		for (dp = g.Disks; dp != NULL; dp = dp->next)
		{
			family2 = dp->family;
			deviceNum2 = dp->ioBSDUnit;
	
			if ( family1 == family2 && deviceNum1 == deviceNum2 )
			{
				int err;
	
				err = UnmountDisk( dp ); // Ignore errors - attempt to unmount each partition
				if ( err && ! result )
				{
					/* Only return the first error we encounter */
					result = err;
				}
			}
		}
	}
	
	
//Return:
	return result;

} // UnmountAllPartitions

//------------------------------------------------------------------------

/*
-- Eject the disk.
-- Precondition: the disk must not be in use (e.g., by a mounted filesystem)
--
-- Handle network volumes (kDiskArbitrationDiskAppearedNetworkDiskMask) specially:
-- do not try to parse their names, and just mark them as ejected (this routine
-- only gets called if the unmount succeeded).
--
*/

int EjectDisk( DiskPtr diskPtr )
{
	int		result = 0; /* mandatory initialization */
    int	fd = -1; /* mandatory initialization */
    char	livePartitionPathname[PATH_MAX];
    
	DiskPtr	dp;
	int		family1, family2;
	int		deviceNum1, deviceNum2;

	int isNetwork;

    if ( gDebug ) printf("%s('%s')\n", __FUNCTION__, diskPtr->ioBSDName);

	isNetwork = ( 0 != ( diskPtr->flags & kDiskArbitrationDiskAppearedNetworkDiskMask ) );
	
	if ( isNetwork )
	{
		/* If the unmount succeeded, then mark the disk as ejected. */
		diskPtr->state = kDiskStateEjected;
		goto Return; /* result == 0 */
	}

	/* Find the correct LIVE partition (whole media) for this device */

	family1 = diskPtr->family;
	deviceNum1 = diskPtr->ioBSDUnit;

	for (dp = g.Disks; dp != NULL; dp = dp->next)
	{
		family2 = dp->family;
		deviceNum2 = dp->ioBSDUnit;
		if ( family1 == family2 && deviceNum1 == deviceNum2 && (dp->flags & kDiskArbitrationDiskAppearedWholeDiskMask) != 0 )
		{
			sprintf(livePartitionPathname, "/dev/r%s", dp->ioBSDName);
			break;
		}
	}
	if ( NULL == dp )
	{
		result = EINVAL;
		LogErrorMessage("%s('%s') can't find whole media\n", __FUNCTION__, diskPtr->ioBSDName);
		goto Return;
	}

	if ( gDebug ) printf("%s: livePartitionPathname = '%s'\n", __FUNCTION__, livePartitionPathname);

	/* IOKit has requested that we open with O_RDONLY and not use O_NDELAY */

    fd = open(livePartitionPathname, O_RDONLY);
    if ( -1 == fd )
    {
    	result = errno;
		LogErrorMessage("%s('%s') open('%s') failed: %d (%s)\n", __FUNCTION__, diskPtr->ioBSDName, livePartitionPathname, errno, strerror(errno));
    	goto Return;
    }

	result = ioctl(fd, DKIOCEJECT, 0);
	if ( -1 == result )
	{
		LogErrorMessage("%s('%s') ioctl(DKIOCEJECT) failed: %d (%s)\n", __FUNCTION__, diskPtr->ioBSDName, errno, strerror(errno));
    	result = errno;
    	goto Return;
	}

	/* Mark each partition on this disk with <state> = kDiskStateEjected */

	family1 = diskPtr->family;
	deviceNum1 = diskPtr->ioBSDUnit;

	for (dp = g.Disks; dp != NULL; dp = dp->next)
	{
		family2 = dp->family;
		deviceNum2 = dp->ioBSDUnit;
		if ( family1 == family2 && deviceNum1 == deviceNum2 )
		{
			dp->state = kDiskStateEjected;
		}
	}
	
Return:

	if ( fd >= 0 )
	{
		close( fd );
	}

//	if ( result && gDebug ) printf("%s('%s') error: %d (%s)\n", __FUNCTION__, diskPtr->ioBSDName, result, strerror(result));
	return result;

} // EjectDisk

//------------------------------------------------------------------------

/*
-- Unlink the given disk record from the global list and free the strings
-- it points to as well as the record itself.
*/

void DeleteDisk( DiskPtr diskPtr )
{
	DiskPtr dp;
	DiskPtr * dpp;

	if ( gDebug ) printf("%s('%s')\n", __FUNCTION__, diskPtr->ioBSDName);
	
	for (	dpp = & g.Disks, dp = * dpp;
			dp != NULL;
			dpp = & (dp->next), dp = * dpp )
	{
		if ( dp == diskPtr )
		{
			*dpp = dp->next;

			free( diskPtr->ioBSDName );
			diskPtr->ioBSDName = NULL; // Attempt to catch accidental re-use.
			free( diskPtr->ioContent );
			diskPtr->ioContent = NULL; // Attempt to catch accidental re-use.
			free( diskPtr->mountpoint );
			diskPtr->mountpoint = NULL; // Attempt to catch accidental re-use.
			if ( diskPtr->ioMediaNameOrNull)
			{
			    free( diskPtr->ioMediaNameOrNull);
				diskPtr->ioMediaNameOrNull = NULL; // Attempt to catch accidental re-use.
			}

			free( diskPtr );
			
			g.NumDisks--;
			
			goto Return;

		}
	}

	/* If we get here, it means we couldn't find <diskPtr> in the list */

Return:
	return;

} // DeleteDisk

//------------------------------------------------------------------------

void PrintDisks(void)
{
	DiskPtr diskPtr;
	int i;

	printf("g.NumDisks = %d\n", g.NumDisks);
	printf("%2s %10s %9s %5s %8s %25s %20s %40s\n", "#", "BSDName", "flags", "state", "BSDUnit", "Content", "MediaName", "mountpoint");
	for (diskPtr = g.Disks, i = 0; diskPtr != NULL; diskPtr = diskPtr->next, i++)
	{
		printf("%2d %10s $%08x %5d %8d %25s %20s %40s\n",
				i,
				diskPtr->ioBSDName,
				diskPtr->flags,
				diskPtr->state,
				diskPtr->ioBSDUnit,
				diskPtr->ioContent,
				diskPtr->ioMediaNameOrNull ? diskPtr->ioMediaNameOrNull : "<null>",
				diskPtr->mountpoint ? diskPtr->mountpoint : "<not mounted>"
		);
	}

} // PrintDisks

//------------------------------------------------------------------------

/* Enable dead name notifications for the given port (send them to our global notification port) */

kern_return_t EnableDeathNotifications(mach_port_t port)
{
	kern_return_t r;
	mach_port_t oldPort;
	
	if ( gDebug) printf("%s($%08x)\n", __FUNCTION__, port);
	
	r = mach_port_request_notification(mach_task_self(), port, MACH_NOTIFY_DEAD_NAME, 0, GetNotifyPort(), MACH_MSG_TYPE_MAKE_SEND_ONCE, &oldPort);
	if ( r != KERN_SUCCESS)
	{
		/* 5/20/99 - This can happen if the client died after sending the message but before we received it */
		LogErrorMessage("(%s:%d) failed to set up death notifications for port %d: {0x%x} %s\n", __FILE__, __LINE__, (int)port, r, mach_error_string(r));
	}
	
	return r;

} // EnableDeathNotifications

//------------------------------------------------------------------------

int DiskArbitrationServerMain(int argc, char* argv[])
{
	kern_return_t r;
	mach_port_t serverPort;

	mach_port_t ioMasterPort;
	mach_port_t ioNotificationPort;
	const char * ioNotificationType;
	io_iterator_t ioIterator;

	mach_port_t portSet;
	
	mach_msg_header_t* msg;
	mach_msg_header_t* reply;

	// Handle command-line arguments

	programName = argv[0];
	
	if ((argc > 1) && (argv[1][0] == '-') && (argv[1][1] == 'd'))
	{
		/* Do nothing.  Do not daemonize yourself.  Good for "-d"(ebugging). */
		gDebug = 1;
		gDaemon = 0;
		if ( gDebug ) printf("DiskArbitration is not a daemon\n");
	}
	else
	{
		gDaemon = 1;
		if ( gDebug ) printf("DiskArbitration is a daemon\n");
	}

	// Incoming message buffer needs room for message header, body, and trailer.
	msg = (mach_msg_header_t*)malloc(kMsgSize);

	// Outgoing message buffer needs room for only message header and body
	reply = (mach_msg_header_t*)malloc(kMsgSize);
	
	/*
	-- Phase 1 - handle fixed disks
	*/

	// Create and register a notify port for this task
	
	r = InitNotifyPort();
	if (r != KERN_SUCCESS)
	{
		return -1;
	}
	
	// Get the IO Master Port
	
	r = IOMasterPort(bootstrap_port, &ioMasterPort);
	if (r != KERN_SUCCESS)
	{
		LogErrorMessage("(%s:%d) IOMasterPort failed: {0x%x} %s\n", __FILE__, __LINE__, r, mach_error_string(r));
		return -1;
	}
	
	// Allocate a port for receiving IO notifications

	r = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &ioNotificationPort);
	if (r != KERN_SUCCESS)
	{
		LogErrorMessage("(%s:%d) mach_port_allocate failed: {0x%x} %s\n", __FILE__, __LINE__, r, mach_error_string(r));
		return -1;
	}

	// Register for IO notifications of "IOMedia" objects and get an IO iterator in return

	ioNotificationType = kIOFirstMatchNotification;
	r = IOServiceAddNotification(	ioMasterPort, ioNotificationType,
									IOServiceMatching( "IOMedia" ),
									ioNotificationPort, (unsigned int) ioNotificationType,
									&ioIterator );
	if (r != KERN_SUCCESS)
	{
		LogErrorMessage("(%s:%d) IOServiceAddNotification failed: {0x%x} %s\n", __FILE__, __LINE__, r, mach_error_string(r));
		return -1;
	}

	// Get the existing disks from the IO Registry and arm the notification

	GetDisksFromRegistry( ioIterator );

	// Perform auto-mounting (including probing and fsck) for each disk we discovered

	autodiskmount();
	
	/*
	-- Phase 2 - optionally daemonize and re-initialize ports in the child process
	*/

	// Daemonize
	
	if ( gDebug ) printf("Non-debug version calls daemon() here...\n");
	
	if ( gDaemon )
	{
		if (-1 == daemon(0, 0))
		{
			return -1;
		}

		/*
		-- Port re-initialization for child process
		*/
	
		// Create and register a notify port for this task
		
		r = InitNotifyPort();
		if (r != KERN_SUCCESS)
		{
			return -1;
		}
		
		// Get the IO Master Port
		
		r = IOMasterPort(bootstrap_port, &ioMasterPort);
		if (r != KERN_SUCCESS)
		{
			LogErrorMessage("(%s:%d) IOMasterPort failed: {0x%x} %s\n", __FILE__, __LINE__, r, mach_error_string(r));
			return -1;
		}
		
		// Allocate a port for receiving IO notifications
	
		r = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &ioNotificationPort);
		if (r != KERN_SUCCESS)
		{
			LogErrorMessage("(%s:%d) mach_port_allocate failed: {0x%x} %s\n", __FILE__, __LINE__, r, mach_error_string(r));
			return -1;
		}
	
		// Register for IO notifications of "IOMedia" objects and get an IO iterator in return
	
		ioNotificationType = kIOFirstMatchNotification;
		r = IOServiceAddNotification(	ioMasterPort, ioNotificationType,
										IOServiceMatching( "IOMedia" ),
										ioNotificationPort, (unsigned int) ioNotificationType,
										&ioIterator );
		if (r != KERN_SUCCESS)
		{
			LogErrorMessage("(%s:%d) IOServiceAddNotification failed: {0x%x} %s\n", __FILE__, __LINE__, r, mach_error_string(r));
			return -1;
		}
	
		// Get the existing disks from the IO Registry and arm the notification
	
		GetDisksFromRegistry( ioIterator );

		// Perform auto-mounting (including probing and fsck) for each disk we discovered
		// This should be a no-op here, since we probably discovered all these disks before daemonizing
	
		autodiskmount();
	
	}
	
	/*
	-- Phase 3 - become a server
	*/

	// Create the server port with receive and send rights
	
	r = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &serverPort);
	if (r != KERN_SUCCESS)
	{
		LogErrorMessage("(%s:%d) mach_port_allocate failed: {0x%x} %s\n", __FILE__, __LINE__, r, mach_error_string(r));
		return -1;
	}
	
	r = mach_port_insert_right(mach_task_self(), serverPort, serverPort, MACH_MSG_TYPE_MAKE_SEND);
	if (r != KERN_SUCCESS)
	{
		LogErrorMessage("(%s:%d) mach_port_insert_right failed: {0x%x} %s\n", __FILE__, __LINE__, r, mach_error_string(r));
		return -1;
	}

	if ( gDebug ) printf("%s: serverPort = %d\n", programName, (int)serverPort);

	// Register the server port with the bootstrap server so clients can find it
	
	r = bootstrap_register(bootstrap_port, DISKARB_SERVER_NAME, serverPort);
	if (r != KERN_SUCCESS)
	{
		return -1;
	}
	
	// Create a port set....

	r = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_PORT_SET, &portSet);
	if (r != KERN_SUCCESS)
	{
		LogErrorMessage("(%s:%d) mach_port_allocate failed: {0x%x} %s\n", __FILE__, __LINE__, r, mach_error_string(r));
		return -1;
	}

	// ... and add the notify, server, and IO notification ports to it.

	r = mach_port_move_member(mach_task_self(), GetNotifyPort(), portSet);
	if (r != KERN_SUCCESS)
	{
		LogErrorMessage("(%s:%d) mach_port_move_member (notify port) failed: {0x%x} %s\n", __FILE__, __LINE__, r, mach_error_string(r));
		return -1;
	}

	r = mach_port_move_member(mach_task_self(), serverPort, portSet);
	if (r != KERN_SUCCESS)
	{
		LogErrorMessage("(%s:%d) mach_port_move_member (server port) failed: {0x%x} %s\n", __FILE__, __LINE__, r, mach_error_string(r));
		return -1;
	}

	r = mach_port_move_member(mach_task_self(), ioNotificationPort, portSet);
	if (r != KERN_SUCCESS)
	{
		LogErrorMessage("(%s:%d) mach_port_move_member (IO notification port) failed: {0x%x} %s\n", __FILE__, __LINE__, r, mach_error_string(r));
		return -1;
	}

	// Server loop

	while( 1 )
	{
		mach_port_t deadPort;
		
		/* First, send out notifications about to any new clients and about any new disks */

		{
			ClientPtr clientPtr;
			DiskPtr diskPtr;
			int errorCode;
			
			/* For each client, and disk, send a notification if either of them is new */

			for (clientPtr = g.Clients; clientPtr != NULL; clientPtr = clientPtr->next)
			{
				for (diskPtr = g.Disks; diskPtr != NULL; diskPtr = diskPtr->next)
				{
					if ( clientPtr->isNew || ( kDiskStateNew == diskPtr->state ) )
					{

						/* New semantics: DiskAppearedWithoutMountpoint and DiskAppearedWithMountpoint *are* mutually exclusive */
						
						/* But: DiskAppeared gets sent for *every* disk ... */

						if ( (clientPtr->flags & kDiskArbNotifyDiskAppearedWithoutMountpoint) && 0 == strcmp("",diskPtr->mountpoint) )
						{
							if ( gDebug ) printf("DiskArbDiskAppeared_rpc($%08x, '%s', $%08x, $%08x) ...\n", clientPtr->port, diskPtr->ioBSDName, diskPtr->flags, errorCode);
							r = DiskArbDiskAppearedWithoutMountpoint_rpc(clientPtr->port, diskPtr->ioBSDName, diskPtr->flags, & errorCode);
							if ( gDebug && r ) printf("... $%08x: %s\n", r, mach_error_string(r));
							if ( r == MACH_SEND_INVALID_DEST )
							{
								/* The client has died */
								/* Don't do anything here ... wait for the notification */
								if ( gDebug ) printf("Dead client!  Port = $%08x\n", clientPtr->port);
							}
						}

						if ( (clientPtr->flags & kDiskArbNotifyDiskAppearedWithMountpoint) && 0 != strcmp("",diskPtr->mountpoint) )
						{
							if ( gDebug ) printf("DiskArbDiskAppearedWithMountpoint_rpc($%08x, '%s', $%08x, '%s', $%08x) ...\n", clientPtr->port, diskPtr->ioBSDName, diskPtr->flags, diskPtr->mountpoint, errorCode);
							r = DiskArbDiskAppearedWithMountpoint_rpc(clientPtr->port, diskPtr->ioBSDName, diskPtr->flags, diskPtr->mountpoint, & errorCode);
							if ( gDebug && r ) printf("... $%08x: %s\n", r, mach_error_string(r));
							if ( r == MACH_SEND_INVALID_DEST )
							{
								/* The client has died */
								/* Don't do anything here ... wait for the notification */
								if ( gDebug ) printf("Dead client!  Port = $%08x\n", clientPtr->port);
							}
						}

						if ( clientPtr->flags & kDiskArbNotifyDiskAppeared )
						{
							if ( gDebug ) printf("DiskArbDiskAppeared_rpc($%08x, '%s', $%08x, '%s', '%s', $%08x) ...\n", clientPtr->port, diskPtr->ioBSDName, diskPtr->flags, diskPtr->mountpoint, diskPtr->ioContent, errorCode);
							r = DiskArbDiskAppeared_rpc(clientPtr->port, diskPtr->ioBSDName, diskPtr->flags, diskPtr->mountpoint, diskPtr->ioContent, & errorCode);
							if ( gDebug && r ) printf("... $%08x: %s\n", r, mach_error_string(r));
							if ( r == MACH_SEND_INVALID_DEST )
							{
								/* The client has died */
								/* Don't do anything here ... wait for the notification */
								if ( gDebug ) printf("Dead client!  Port = $%08x\n", clientPtr->port);
							}
						}

					} // IF client is new OR disk is new

				} // FOREACH disk

			} // FOREACH client
			
			/* For each disk and client, if it was new, then it is no longer new */
			
			for (clientPtr = g.Clients; clientPtr != NULL; clientPtr = clientPtr->next)
			{
				if ( clientPtr->isNew ) clientPtr->isNew = 0;
			}
			for (diskPtr = g.Disks; diskPtr != NULL; diskPtr = diskPtr->next)
			{
				if ( kDiskStateNew == diskPtr->state ) diskPtr->state = kDiskStateIdle;
			}

		}

		/* Second, send out notifications about unmounted/ejected disks to existing clients */

		{
			ClientPtr clientPtr;
			DiskPtr diskPtr;
			int errorCode;
			
			/* For each client, and disk, send a notification if the disk was unmounted/ejected */

			for (clientPtr = g.Clients; clientPtr != NULL; clientPtr = clientPtr->next)
			{
				for (diskPtr = g.Disks; diskPtr != NULL; diskPtr = diskPtr->next)
				{
					if ( kDiskStateUnmounted == diskPtr->state || kDiskStateEjected == diskPtr->state )
					{

						if ( clientPtr->flags & kDiskArbitrationNotifyUnmount )
						{
							if ( gDebug ) printf("DiskArbUnmountCommit_obsolete($%08x, '%s', $%08x, $%08x) ...\n", clientPtr->port, diskPtr->ioBSDName, diskPtr->flags, errorCode);
							r = DiskArbUnmountCommit_obsolete(clientPtr->port, diskPtr->ioBSDName, & errorCode);
							if ( gDebug && r ) printf("... $%08x: %s\n", r, mach_error_string(r));
							if ( r == MACH_SEND_INVALID_DEST )
							{
								/* The client has died */
								/* Don't do anything here ... wait for the notification */
								if ( gDebug ) printf("Dead client!  Port = $%08x\n", clientPtr->port);
							}
						}

					}
				}
			}
			
			/* For each disk, if it was Ejected, then delete it, and if it was Unmounted, then mark it Idle again */
			
			for (diskPtr = g.Disks; diskPtr != NULL; diskPtr = diskPtr->next)
			{
				switch ( diskPtr->state )
				{
					case kDiskStateUnmounted:
						diskPtr->state = kDiskStateIdle;
					break;

					case kDiskStateEjected:
						DeleteDisk( diskPtr );
					break;

					case kDiskStateIdle: /* do nothing */
					case kDiskStateNew: /* do nothing */
					default:
						/* do nothing */
					break;
				}
			}

		}

		if ( gDebug ) PrintDisks();

		/* Wait for: client requests, disk insertions, and death notifications */

		if ( gDebug ) printf("%s: mach_msg\n", programName);
		r = mach_msg(msg, MACH_RCV_MSG, 0, kMsgSize, portSet, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
		if (r != KERN_SUCCESS) {
			LogErrorMessage("(%s:%d) mach_msg failed: {0x%x} %s\n", __FILE__, __LINE__, r, mach_error_string(r));
			continue;
		}
		
		/* Demultiplex based on the port: notification port or server port */

		if ( msg->msgh_local_port == GetNotifyPort() )
		{
			if ( gDebug ) printf("%s: Message received on notification port\n", programName);

			/* This test is partially redundant, and that's alright */
			
			if ( MessageIsNotificationDeath( msg, & deadPort ) == TRUE )
			{
				if ( gDebug) printf("Death of port %d\n", (int)deadPort);
				
				/* Deallocate the send right that came in the dead name notification */
	
				{
					kern_return_t r;
			
					r = mach_port_deallocate( mach_task_self(), deadPort );
					if ( gDebug && r ) printf("%s(client = $%08x): mach_port_deallocate(...,$%08x) => $%08x: %s\n", __FUNCTION__, (int)deadPort, (int)deadPort, r, mach_error_string(r));
				}
				
				/* Clean up the resources consumed by the client record */
				
				ClientDeath( deadPort );
			}
			else
			{
				/* Do nothing - MessageIsNotificationDeath() will log an error in this case */
			}
		}
		else if ( msg->msgh_local_port == serverPort )
		{
			if ( gDebug ) printf("%s: Message received on server port from port $%08x\n", programName, (int)(msg->msgh_remote_port));
			(void) DiskArbitrationServer_server(msg, reply);
			///reply->msgh_local_port = serverPort;
			reply->msgh_local_port = MACH_PORT_NULL;
			(void) mach_msg_send(reply);
		}
		else if ( msg->msgh_local_port == ioNotificationPort )
		{
			if ( gDebug ) printf("%s: Message received on IO notification port from port $%08x\n", programName, (int)(msg->msgh_remote_port));
			HandleIONotificationMsg( (IONotificationMsgPtr) msg );
		}
		else
		{
			// This should never happen.  Log it.
			LogErrorMessage("Unrecognized receive port %d.\n", (int)(msg->msgh_local_port));
		}

	} /* while (1) */
	
	// We only exit the server loop if something goes wrong

#if DEBUG
	printf("%s: exit\n", programName);
#endif

	return -1;

}


