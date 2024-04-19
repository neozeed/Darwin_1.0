/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 * autodiskmount.m
 * - fscks and mounts all currently unmounted ufs filesystems, and mounts any
 *   hfs or cd9660 filesystems
 * - by default, only filesystems on fixed disks are mounted
 * - option '-a' means mount removable media too
 * - option '-e' means eject removable media (overrides '-a')
 * - option '-n' means don't mount anything, can be combined
 *   with other options to eject all removable media without mounting anything
 * - option '-v' prints out what's already mounted (after doing mounts)
 * - option '-d' prints out debugging information
 */

/*
 * Modification History:
 *
 * Dieter Siegmund (dieter@apple.com) Thu Aug 20 18:31:29 PDT 1998
 * - initial revision
 * Dieter Siegmund (dieter@apple.com) Thu Oct  1 13:42:34 PDT 1998
 * - added support for hfs and cd9660 filesystems
 * Brent Knight (bknight@apple.com) Thu Apr  1 15:54:48 PST 1999
 * - adapted for Beaker
 * Brent Knight (bknight@apple.com) Thu Sep  9 11:36:01 PDT 1999
 * - added support for fsck_hfs, disk insertion and ejection
 * Dieter Siegmund (dieter@apple.com) Wed Nov 10 10:58:43 PST 1999
 * - added support for named UFS volumes
 */

#import <libc.h>
#import <stdlib.h>
#import <string.h>
#import <sys/ioctl.h>
#import <bsd/dev/disk.h>
#import <errno.h>
#import <sys/param.h>
#import <sys/mount.h>
#import <ufs/ufs/ufsmount.h>
#import <unistd.h>

#import <objc/Object.h>
#import	<mach/boolean.h>
#import <objc/List.h>
#import "DiskVolume.h"

#import "DiskArbitrationServerMain.h"


boolean_t
fsck_vols(id vols) 
{
    boolean_t		result = TRUE; /* mandatory initialization */
    char 		command[1024];
    int			dirty_ufs_fs = 0;
    int			dirty_hfs_fs = 0;
    int 		i;

    for (i = 0; i < [vols count]; i++)
    {

	DiskVolume * vol;

	vol = (DiskVolume *)[vols objectAt:i];

	if (vol->writable && vol->dirty && vol->mount_point == NULL)
	{
	    /* Construct an command line to invoke fsck/fsck_hfs on this dirty volume. */

	    if (0==strcmp(vol->fs_type, FS_TYPE_UFS))
	    {
		sprintf(command, "/sbin/fsck -y /dev/r%s", vol->disk_dev_name);
		dirty_ufs_fs++;
	    }
	    else if (0==strcmp(vol->fs_type, FS_TYPE_HFS))
	    {
		sprintf(command, "/sbin/fsck_hfs -y /dev/r%s", vol->disk_dev_name);
		dirty_hfs_fs++;
	    }
	    else
	    {
		fprintf(stderr, "%s: unrecognized volume format: '%s' on device: '%s'\n", __FUNCTION__, vol->fs_type, vol->disk_dev_name);
		continue;
	    }

	    /* Execute the command-line we constructed */
	    
	    {
		FILE *		f;
		int 		ret;
	
		if ( gDebug) printf("%s: '%s'...\n", __FUNCTION__, command);

		f = popen(command, "w");
		if (f == NULL)
		{
		    fprintf(stderr, "popen('%s') failed", command);
		    continue;
		}
		fflush(f);
		ret = pclose(f);

		if ( gDebug) printf("%s: '%s' => %d\n", __FUNCTION__, command, ret);

		if ( ret == 0 )
		{
			/* Mark the volume as clean so that it will be mounted */
			vol->dirty = FALSE;
		}
		else
		{
			if ( gDebug ) printf("'%s' failed: %d\n", command, ret);
		}

		/* Result will be TRUE iff each fsck command is successful */

		result = result && (ret == 0);
	    }

	} // if dirty

    } // for each volume

    return result;

}

boolean_t 
mount_vols(id vols)
{
    int i;

    for (i = 0; i < [vols count]; i++)
    {
		DiskVolume * d = (DiskVolume *)[vols objectAt:i];
		DiskPtr dp = LookupDiskByIOBSDName( d->disk_dev_name );
	
		/* If already mounted then skip this volume */
	
		if (d->mounted)
		{
			if (dp)
			{
				DiskSetMountpoint(dp, d->mount_point);
			}
			continue;
		}
	
		/* If still dirty then skip this volume */
	
		if (d->dirty)
		{
			continue;
		}
	
		/* Determine and create the mount point */
	
		if ([vols setVolumeMountPoint:d] == FALSE)
		{
			continue;
		}
	
		if ([d mount]) 
		{
			if (dp)
			{
				DiskSetMountpoint( dp, d->mount_point );
			}
		}
    }

    return (TRUE);
}

void autodiskmount(void)
{
    DiskVolumes * vols;

    vols = [[DiskVolumes alloc] init];

    [vols do_removable:g.do_removable eject_removable:g.eject_removable];

    if (g.do_mount)
    {
	if (fsck_vols(vols) == FALSE)
	{
	    fprintf(stderr, "Some fsck failed!");
	}

	if (mount_vols(vols) == FALSE)
	{
	    fprintf(stderr, "Some mount() failed!");
	}
    }

    if (g.verbose)
    {
	[vols print];
    }

    [vols free];
}

GlobalStruct g;

int
main(int argc, char * argv[])
{
    char * progname;
    char ch;

    /* Initialize globals */

    g.Clients = NULL;
    g.NumClients = 0;
	
    g.Disks = NULL;
    g.NumDisks = 0;
	
    g.eject_removable = FALSE;
    g.do_removable = FALSE;
    g.verbose = FALSE;
    g.do_mount = TRUE;
    g.debug = FALSE;
	
    /* Initialize <progname> */
	
    progname = argv[0];

    /* Must run as root */

    if (getuid() != 0)
    {
	fprintf(stderr, "%s: must be run as root\n", progname);
	exit(1);
    }

    /* Parse command-line arguments */

    while ((ch = getopt(argc, argv, "avned")) != -1)
    {
	switch (ch)
	{
	    case 'a':
		g.do_removable = TRUE;
	    break;
	    case 'v':
		g.verbose = TRUE;
	    break;
	    case 'n':
		g.do_mount = FALSE;
	    break;
	    case 'e':
		g.eject_removable = TRUE;
	    break;
	    case 'd':
		g.debug = TRUE;
	    break;
	}
    }

    if (g.eject_removable)
    {
	g.do_removable = FALSE; /* sorry, eject overrides use */
    }

    {	/* Radar 2323271 */
    	char * argv2[] = { argv[0], "-d" };
    	int argc2 = ( g.debug ? 2 : 1 );
    	(void) DiskArbitrationServerMain(argc2, argv2);
    }

    /* Not reached */

    exit(0);
}

