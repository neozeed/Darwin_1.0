#include <stdio.h>
#include <stdlib.h>

#include <mach/port.h>
#include <mach/kern_return.h>
#include <mach/mach_error.h>

#include "DiskArbitrationServer.h"
#include "DiskArbitrationTypes.h"
#include "DiskArbitrationServerMain.h"

/********************************************************************************************************/
/******************************************* Private Prototypes *****************************************/
/********************************************************************************************************/

/********************************************************************************************************/
/******************************************* Private Functions ******************************************/
/********************************************************************************************************/

/********************************************************************************************************/
/******************************************* Public Functions *******************************************/
/********************************************************************************************************/

/******************************************* ClientDeath *******************************************/


void ClientDeath(mach_port_t client);

void ClientDeath(mach_port_t client)
{
	ClientPtr thisPtr, previousPtr;

	/* Deletes the first client record whose port matches. */

	if ( gDebug ) printf("%s(client = $%08x)\n", __FUNCTION__, client);
	
	/* Is the list empty? */

	if ( g.Clients == NULL )
	{
		goto NotFound;
	}
	
	/* Is it the first element of the list? */
	
	if ( g.Clients->port == client )
	{
		/* Save the pointer */
		thisPtr = g.Clients;
		/* Unlink the record from the list */
		g.Clients = g.Clients->next;
		/* Assert: thisPtr->port == client */
		goto FoundIt;
	}
	
	/* Is it somewhere in the body of the list? */

	previousPtr = g.Clients;
	/* Assert: previousPtr != NULL since g.Clients != NULL */
	thisPtr = previousPtr->next;
	while ( thisPtr != NULL )
	{
		/* Invariant: previousPtr != NULL and thisPtr != NULL and previousPtr->next == thisPtr */
		if ( thisPtr->port == client )
		{
			/* Unlink the record from the list */
			previousPtr->next = thisPtr->next;
			/* Assert: thisPtr->port == client */
			goto FoundIt;
		}
		/* Advance to the next element, reestablishing the invariant */
		previousPtr = thisPtr;
		thisPtr = previousPtr->next;
	}
	
	/* If we get here, it means we never found a matching client record */
	
	goto NotFound;

FoundIt:

	/* Precondition: thisPtr->port == client */
	
	if ( gDebug )
	{
		printf("Found client record to delete:\n");
		PrintClient( thisPtr );
	}

	/* Deallocate the port once, free the memory, decrement the count, and return */

	{
		kern_return_t r;

		r = mach_port_deallocate( mach_task_self(), thisPtr->port );
		if ( r && gDebug ) printf("%s(client = $%08x): mach_port_deallocate(...,$%08x) => $%08x: %s\n", __FUNCTION__, client, client, r, mach_error_string(r));
	}
	free( thisPtr );
	g.NumClients--;
	goto Return;

NotFound:

	if ( gDebug ) printf("%s(client = $%08x): no matching client found!\n", __FUNCTION__, client);
	
	goto Return;

Return:

	if ( gDebug )
	{
		printf("%s($%08x), After:\n", __FUNCTION__, client);
		PrintClients();
	}
	
	return;

} // ClientDeath

/******************************************* DiskArbRegister_rpc *******************************************/

kern_return_t DiskArbRegister_rpc (
	mach_port_t server,
	mach_port_t client,
	unsigned flags,
	int * errorCodePtr)
{
	kern_return_t err = 0;
	ClientPtr newClient = NULL;

	if ( gDebug ) printf("%s(client = $%08x, flags = $%08x)\n", __FUNCTION__, client, flags);

	/*
		First, attempt to register for death notifications.
		If it fails, then deallocate the client-supplied port
		and exit with an error without creating a client record.
	*/
	
	if ( EnableDeathNotifications( client ) )
	{
		if ( gDebug ) printf("%s(client = $%08x, flags = $%08x): EnableDeathNotifications() failed!\n", __FUNCTION__, (int)client, (int)flags);
		{
			kern_return_t r;
	
			r = mach_port_deallocate( mach_task_self(), client );
			if ( gDebug ) printf("%s(client = $%08x): mach_port_deallocate(...,$%08x) => $%08x: %s\n", __FUNCTION__, client, client, r, mach_error_string(r));
		}
		err = -1;
		goto Return;
	}
	
	/*
		If we got this far, we can safely create a client record knowing that if the client
		dies we will be sent a MACH_NOTIFY_DEAD_NAME message.
	*/

	/* Allocate a new client record. */

	newClient = NewClient(client, 0, flags);
	if ( newClient == NULL )
	{
		if ( gDebug ) printf("%s(client = $%08x, flags = $%08x): NewClient failed!\n", __FUNCTION__, client, flags);
		err = -1;
		goto Return;
	}

	if ( gDebug ) PrintClients();

Return:
	*errorCodePtr = err;
	return err;
}

/******************************************* DiskArbRequestEject_rpc *******************************************/

kern_return_t DiskArbRequestEject_rpc(
	mach_port_t server,
	DiskArbitrationDiskIdentifier diskIdentifier,
	int * errorCodePtr)
{
	kern_return_t err = 0;
	DiskPtr diskPtr;

	if ( gDebug ) printf("%s(diskIdentifier='%s')\n", __FUNCTION__, diskIdentifier);

	diskPtr = LookupDiskByIOBSDName( diskIdentifier );
	if ( NULL == diskPtr )
	{
		if ( gDebug ) printf("%s(diskIdentifier = '%s'): LookupDiskByIOBDSName failed\n", __FUNCTION__, diskIdentifier);
		err = -1;
		goto Return;		
	}

	/* If any unmount fails, this will return an error and we will skip the eject */

	err = UnmountAllPartitions( diskPtr );
	if ( err ) goto Return;
	
	err = EjectDisk( diskPtr );
	if ( err ) goto Return;

	/* Our record of the disks are not deleted until the next time around the main loop */
	
Return:
	*errorCodePtr = err;
	return err;
}

/******************************************* DiskArbRequestUnmountOne_rpc *******************************************/

kern_return_t DiskArbRequestUnmountOne_rpc(
	mach_port_t server,
	DiskArbitrationDiskIdentifier diskIdentifier,
	int * errorCodePtr)
{
	kern_return_t err = 0;
	DiskPtr diskPtr;

	if ( gDebug ) printf("%s(diskIdentifier='%s')\n", __FUNCTION__, diskIdentifier);

	diskPtr = LookupDiskByIOBSDName( diskIdentifier );
	if ( NULL == diskPtr )
	{
		if ( gDebug ) printf("%s(diskIdentifier = '%s'): LookupDiskByIOBDSName failed\n", __FUNCTION__, diskIdentifier);
		err = -1;
		goto Return;		
	}

	err = UnmountDisk( diskPtr );
	
Return:
	*errorCodePtr = err;
	return err;
}

/******************************************* DiskArbDiskAppearedWithMountpointPing_rpc *******************************************/

kern_return_t DiskArbDiskAppearedWithMountpointPing_rpc (
	mach_port_t server,
	DiskArbitrationDiskIdentifier diskIdentifier,
	unsigned flags,
	DiskArbitrationMountpoint mountpoint,
	int * errorCodePtr)
{
	kern_return_t err = 0;

	if ( gDebug ) printf("%s(diskIdentifier = '%s', flags = $%08x, mountpoint = '%s')\n", __FUNCTION__, diskIdentifier, flags, mountpoint);
	
	if ( ! NewDisk( diskIdentifier /*ioBSDName*/, 0 /*ioBSDUnit*/, NULL /*ioContentOrNull*/, kDiskFamily_AFP /*family*/, mountpoint /*mountpoint*/, NULL /*ioMediaNameOrNull*/, flags /*flags*/ ) )
	{
		fprintf(stderr, "%s: NewDisk() failed!\n", __FUNCTION__);
	}
	
	if ( gDebug ) PrintDisks();

	goto Return;

Return:
	*errorCodePtr = kDiskArbitrationNoError;
	return err;
}

/******************************************* DiskArbRequestUnmountAll_rpc *******************************************/

kern_return_t DiskArbRequestUnmountAll_rpc (
	mach_port_t server,
	DiskArbitrationDiskIdentifier diskIdentifier,
	int * errorCodePtr)
{
	kern_return_t err = 0;
	int errorCode;
	DiskPtr diskPtr;

	if ( gDebug ) printf("%s(diskIdentifier = '%s')\n", __FUNCTION__, diskIdentifier);
	
	diskPtr = LookupDiskByIOBSDName( diskIdentifier );
	if ( NULL == diskPtr )
	{
		if ( gDebug ) printf("%s(diskIdentifier = '%s'): LookupDiskByIOBDSName failed\n", __FUNCTION__, diskIdentifier);
		errorCode = -1;
		goto Return;		
	}

	errorCode = UnmountAllPartitions( diskPtr );

Return:
	*errorCodePtr = errorCode;
	return err;
}

/******************************************* DiskArbRefresh_rpc *******************************************/

kern_return_t DiskArbRefresh_rpc (
	mach_port_t server,
	int * errorCodePtr)
{
	kern_return_t err = 0;
	int errorCode = 0;

	if ( gDebug ) printf("%s()\n", __FUNCTION__);
	
	/*
	--
	-- TO BE IMPLEMENTED
	--
	*/
	
Return:
	*errorCodePtr = errorCode;
	return err;
}

/******************************************* DiskArbRegisterWithPID_rpc *******************************************/

kern_return_t DiskArbRegisterWithPID_rpc (
	mach_port_t server,
	mach_port_t client,
	unsigned pid,
	unsigned flags,
	int * errorCodePtr)
{
	kern_return_t err = 0;
	ClientPtr newClient = NULL;

	if ( gDebug ) printf("%s(client = $%08x, pid = %d, flags = $%08x)\n", __FUNCTION__, client, pid, flags);

	/*
		First, attempt to register for death notifications.
		If it fails, then deallocate the client-supplied port
		and exit with an error without creating a client record.
	*/
	
	if ( EnableDeathNotifications( client ) )
	{
		if ( gDebug ) printf("%s(client = $%08x, pid = %d, flags = $%08x): EnableDeathNotifications() failed!\n", __FUNCTION__, (int)client, pid, (int)flags);
		{
			kern_return_t r;
	
			r = mach_port_deallocate( mach_task_self(), client );
			if ( gDebug ) printf("%s(client = $%08x): mach_port_deallocate(...,$%08x) => $%08x: %s\n", __FUNCTION__, client, client, r, mach_error_string(r));
		}
		err = -1;
		goto Return;
	}
	
	/*
		If we got this far, we can safely create a client record knowing that if the client
		dies we will be sent a MACH_NOTIFY_DEAD_NAME message.
	*/

	/* Allocate a new client record. */

	newClient = NewClient(client, pid, flags);
	if ( newClient == NULL )
	{
		if ( gDebug ) printf("%s(client = $%08x, pid = %d, flags = $%08x): NewClient failed!\n", __FUNCTION__, client, pid, flags);
		err = -1;
		goto Return;
	}

	if ( gDebug ) PrintClients();

Return:
	*errorCodePtr = err;
	return err;
}

