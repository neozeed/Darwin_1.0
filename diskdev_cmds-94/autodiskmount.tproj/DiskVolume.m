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
 * DiskVolume.m
 * - objects for DiskVolume and DiskVolumes
 * - a convenient way to store information about filesystems (volumes)
 *   and perform actions on them
 */

/*
 * Modification History:
 *
 * Dieter Siegmund (dieter@apple.com) Thu Aug 20 18:31:29 PDT 1998
 * - initial revision
 * Dieter Siegmund (dieter@apple.com) Thu Oct  1 13:42:34 PDT 1998
 * - added support for hfs and cd9660 filesystems
 * Brent Knight (bknight@apple.com) Thu Apr  1 15:54:48 PST 1999
 * - adapted for Beaker, with caveats described in Radar 2320396
 * Brent Knight (bknight@apple.com) Fri Apr  9 11:16:04 PDT 1999
 * - [2320396] added support for ioWritable/ioRemovable
 * Brent Knight (bknight@apple.com) Thu Sep  9 11:36:01 PDT 1999
 * - added support for fsck_hfs, disk insertion and ejection
 * Dieter Siegmund (dieter@apple.com) Wed Nov 10 10:58:43 PST 1999
 * - added support for named UFS volumes
 * - added logic to use the NeXT label name if a "new" ufs label is
 *   not present
 * - homogenized the filesystem probing logic for ufs, hfs, and cd9660
 * - homogenized the fsck logic for ufs and hfs
 */


#import <libc.h>
#import <stdlib.h>
#import <unistd.h>
#import <string.h>
#import <sys/ioctl.h>
#import <bsd/dev/disk.h>
#import <errno.h>
#import <sys/wait.h>
#import <sys/param.h>
#import <sys/mount.h>
#import <grp.h>
#import <ufs/ufs/ufsmount.h>

#import <objc/Object.h>
#import	<mach/boolean.h>
#import <objc/List.h>
#import <sys/loadable_fs.h>

#import "DiskVolume.h"
#import "GetRegistry.h"
#import "DiskArbitrationServerMain.h"

#define MAXNAMELEN	256

static void
eject_media(char * dev)
{
    int		fd;
    char	specName[MAXNAMELEN];

    sprintf(specName, "/dev/r%s", dev);
    fd = open(specName, O_RDONLY | O_NDELAY);
	if ( fd >= 0 )
    {
		if (ioctl(fd, DKIOCEJECT, 0) < 0)
		{
		    fprintf(stderr, "eject %s failed: %s\n", dev, strerror(errno));
		}
		close(fd);
    }
    return;
}

static struct statfs *
get_fsstat_list(int * number)
{
    int n;
    struct statfs * stat_p;

    n = getfsstat(NULL, 0, MNT_NOWAIT);
    if (n <= 0)
    {
		return (NULL);
    }

    stat_p = (struct statfs *)malloc(n * sizeof(*stat_p));
    if (stat_p == NULL)
    {
		return (NULL);
    }

    if (getfsstat(stat_p, n * sizeof(*stat_p), MNT_NOWAIT) <= 0)
    {
		free(stat_p);
		return (NULL);
    }

    *number = n;

    return (stat_p);
}

static struct statfs *
fsstat_lookup_spec(struct statfs * list_p, int n, char * spec, char * fstype)
{
    char 				alt_spec[MAXNAMELEN];
    int 				i;
    struct statfs * 	scan;

    sprintf(alt_spec, "/private%s", spec);
    for (i = 0, scan = list_p; i < n; i++, scan++)
    {
		if (strcmp(scan->f_fstypename, fstype) == 0
		    && (strcmp(scan->f_mntfromname, spec) == 0
			|| strcmp(scan->f_mntfromname, alt_spec) == 0))
		{
		    return (scan);
		}
    }
    return (NULL);
}

#if 0
int 
getDeviceInfo(const char *devName, char *type, 
	      BOOL * isWritable, BOOL * isRemovable) 
{
#if 1
    int rtn = 0;
	if ( type )
	{
		char * result;
		result = "scsi";
		strcpy( type, result );
	}
    *isWritable = 0;
    *isRemovable = 0;
    return rtn;
#else
    IODeviceMaster *	dm;
    IOObjectNumber	objNum;
    IOString 		deviceKind;
    IOReturn		rtn;
    char 		str[1024];

    dm = [IODeviceMaster new];

    rtn = [dm lookUpByDeviceName:(char *)devName
	   objectNumber:&objNum
	   deviceKind:&deviceKind];
    if (rtn == kIOReturnSuccess) {
        rtn = [dm getStringPropertyList:objNum
	       names:IOPropDeviceType
	       results:str maxLength:sizeof(str) ];
    }
    [dm free];
    if (rtn == kIOReturnSuccess) {
        if ( isWritable != NULL ) {
            *isWritable = strstr(str,IOTypeWriteProtectedDisk) == NULL;
        }
        
        if ( isRemovable != NULL ) {
            *isRemovable = strstr(str,IOTypeRemovableDisk) != NULL;
        }
        if ( type != NULL ) {
            char * result = "";
            if (strstr(str,IOTypeFloppy)) {
                result = "floppy";
	    } 
	    else if (strstr(str, IOTypeCDROM )) {
	        result = "cdrom";
            } 
	    else if (strstr(str,IOTypeOptical)) {
                result = "optical";
            } 
	    else if (strstr(str, IOTypeIDE)) { // ??? should we check IOTypeATAPI instead?
                result = "ide"; //** old code returned "hard" in this case
            } 
	    else if (strstr(str,IOTypeSCSI) || strstr(str,"scsi")) { //** look for "scsi" to workaround PEx bug, do so last to prefer IO string constants
                result = "scsi";
            } 
            strcpy(type,result);
        }
    }
    return rtn;
#endif
}
#endif

boolean_t
fsck_needed(char * devname, char * fstype)
{
	char * cmd = NULL;
    char command[1024];
    FILE * f;
    int ret;

	if (strcmp(fstype, FS_TYPE_HFS) == 0)
	{
		cmd = HFS_FSCK;
	}
	else if (strcmp(fstype, FS_TYPE_UFS) == 0)
	{
		cmd = UFS_FSCK;
	}
	else
	{
		return (FALSE);
	}
	
    sprintf(command, "%s -q /dev/r%s", cmd, devname);
    
    if ( gDebug ) printf("%s('%s'): '%s'...\n", __FUNCTION__, devname, command);
    
    f = popen(command, "w");
    if (f == NULL)
    {
		fprintf(stderr, "%s('%s'): popen('%s') failed", __FUNCTION__, devname, command);
		return (FALSE);
    }
    fflush(f);
    ret = pclose(f);

    if ( gDebug ) printf("%s('%s'): '%s' => %d\n", __FUNCTION__, devname, command, ret);

    if (ret == 0)
    {
		return (FALSE);
    }
    return (TRUE);
}

#define FILESYSTEM_ERROR	 		0
#define FILESYSTEM_MOUNTED 			1
#define FILESYSTEM_MOUNTED_ALREADY	2
#define FILESYSTEM_NEEDS_REPAIR 	3

/** ripped off from workspace - start **/
static void cleanUpAfterFork(void)
{
    int fd, maxfd = getdtablesize();

	/* Close all inherited file descriptors */

    for (fd = 0; fd < maxfd; fd++)
    {
		close(fd);
    }

	/* Disassociate ourselves from any controlling tty */

    if ((fd = open("/dev/tty", O_NDELAY)) >= 0)
    {
		ioctl(fd, TIOCNOTTY, 0);
		close(fd);
    }
    
    /* Reset the user and group id's to their real values */

    setgid(getgid());
    setuid(getuid());

	/* stdin = /dev/null */
	/* stdout = /dev/console */
	/* stderr = stdout = /dev/console */

    fd = open("/dev/null", O_RDONLY);
    fd = open("/dev/console", O_WRONLY);
    dup2(1, 2);

}

/* foreignLabel: return the label written to the file by the -p(robe) option of the fs.util program */

static char * 
foreignLabel(char * fsName)
{
    int fd;
    static char theLabel[MAXNAMELEN], buf[MAXPATHLEN];

    sprintf(buf, "%s/%s%s/%s%s", FS_DIR_LOCATION, fsName, FS_DIR_SUFFIX, fsName, FS_LABEL_SUFFIX);
    fd = open(buf, O_RDONLY, 0);
    if (fd >= 0)
    {
		int i = read(fd, theLabel, 255);
		close(fd);
		if (i > 0)
		{
			theLabel[i] = '\0';
			return (theLabel);
		}
    }

	return (NULL);
}

/* foreignProbe: run the -p(robe) option of the given <fsName>.util program in a child process */

static int
foreignProbe(const char *fsName, const char *devName, int removable, int writable, int forMount)
{
	int result;
    char cmd[] = {'-', forMount ? FSUC_PROBE : FSUC_PROBEFORINIT, 0};
    char execPath[MAXPATHLEN];
    const char *childArgv[] = {	execPath,
    							cmd,
    							devName,
    							removable ? DEVICE_REMOVABLE : DEVICE_FIXED,
    							writable? DEVICE_WRITABLE : DEVICE_READONLY,
    							0 };
    char fsDir[MAXPATHLEN];
    int pid;

    sprintf(fsDir, "%s/%s%s", FS_DIR_LOCATION, fsName, FS_DIR_SUFFIX);
    sprintf(execPath, "%s/%s%s", fsDir, fsName, FS_UTIL_SUFFIX);
    if (access(execPath,F_OK) == 0)
    {
		if ( gDebug )
			printf("%s('%s', '%s', removable=%d, writable=%d, forMount=%d):\n'%s %s %s %s %s'\n", __FUNCTION__, fsName, devName, removable, writable, forMount, execPath, childArgv[1], childArgv[2], childArgv[3], childArgv[4]);

        if ((pid = fork()) == 0)
        {
            cleanUpAfterFork();
            chdir(fsDir);
            execve(execPath, childArgv, 0);
            exit(-127);
        }
        else if (pid > 0)
        {
			int statusp;
			
            if (wait4(pid,&statusp,0,NULL) > 0)
            {
                if (WIFEXITED(statusp))
                {
                	result = (int)(char)(WEXITSTATUS(statusp));
	                goto Return;
                }
            }
        }
    }

    result = FSUR_IO_FAIL;

Return:
	if ( gDebug ) printf("%s(...) => %d\n", __FUNCTION__, result);
    return result;
}

/* foreignMountDevice: run the -(mount) option of the given <fsName>.util program in a child process */

static int foreignMountDevice(const char *fsName, const char *devName, int removable, int writable, const char *mountPoint)
{
	int result;
    char cmd[] = {'-', FSUC_MOUNT, 0};
    char execPath[MAXPATHLEN];
    const char *childArgv[] = {	execPath,
    							cmd,
    							devName,
    							mountPoint,
    							removable ? DEVICE_REMOVABLE : DEVICE_FIXED,
    							writable ? DEVICE_WRITABLE : DEVICE_READONLY,
    							0 };
    char fsDir[MAXPATHLEN];
    int pid;

    sprintf(fsDir, "%s/%s%s", FS_DIR_LOCATION, fsName, FS_DIR_SUFFIX);
    sprintf(execPath, "%s/%s%s", fsDir, fsName, FS_UTIL_SUFFIX);

	if ( gDebug )
		printf("%s('%s', '%s', removable=%d, writable=%d, '%s'):\n'%s %s %s %s %s %s'\n", __FUNCTION__, fsName, devName, removable, writable, mountPoint, execPath, childArgv[1], childArgv[2], childArgv[3], childArgv[4], childArgv[5]);

    if ((pid = fork()) == 0)
    {
		cleanUpAfterFork();
		chdir(fsDir);
		execve(execPath, childArgv, 0);
		exit(-127);
    }
    else if (pid > 0)
    {
		int statusp;
		if (wait4(pid,&statusp,0,NULL) > 0)
		{
			if (WIFEXITED(statusp))
			{
				int i = (int)(char)(WEXITSTATUS(statusp));
				if (i == FSUR_IO_SUCCESS)
				{
					result = FILESYSTEM_MOUNTED;
					goto Return;
				}
				else if (i == FSUR_IO_UNCLEAN)
				{
					result = FILESYSTEM_NEEDS_REPAIR;
					goto Return;
				}
			}
		}
    }

    result = FILESYSTEM_ERROR;

Return:
	if ( gDebug ) printf("%s(...) => %d\n", __FUNCTION__, result);
    return result;
}
/** ripped off from workspace - end **/

@implementation DiskVolume

- (boolean_t) mount_foreign
{
    int ret;

    ret = foreignMountDevice(fs_type, disk_dev_name, removable, writable, mount_point);
    if (ret == FILESYSTEM_MOUNTED)
    {
		printf("Mounted %s /dev/%s on %s\n", fs_type, disk_dev_name, mount_point);
		[self setMounted:TRUE];
		return (TRUE);
    }
	else if (ret == FILESYSTEM_NEEDS_REPAIR)
	{
		/* We should never get here, thanks to the "fsck -q" calls. */

    	char hfs_command[1024];

		sprintf(hfs_command, "/sbin/fsck_hfs -y /dev/r%s", disk_dev_name);

		{
			FILE *		f;
			int 		ret;
			
			printf("%s: command to execute is '%s'\n", __FUNCTION__, hfs_command);
			f = popen(hfs_command, "w");
			if (f == NULL)
			{
				fprintf(stderr, "%s: popen('%s') failed", __FUNCTION__, hfs_command);
				return (FALSE);
			}
			fflush(f);
			ret = pclose(f);
			if ( ret != 0 )
			{
				fprintf(stderr, "%s: pclose('%s') failed\n", __FUNCTION__, hfs_command);
				return (FALSE);
			}
		}

		ret = foreignMountDevice(fs_type, disk_dev_name, removable, writable, mount_point);
		if (ret == FILESYSTEM_MOUNTED)
		{
			printf("Mounted %s /dev/%s on %s\n", fs_type, disk_dev_name, mount_point);
			[self setMounted:TRUE];
			return (TRUE);
		}
		else
		{
			return (FALSE);
		}

    }
	else
	{
		fprintf(stderr, "%s: unrecognized return code from foreignMountDevice: %d\n", __FUNCTION__, ret);
	}

    return (FALSE);
}

- (boolean_t) mount_ufs
{
    struct ufs_args 	args;
    int 		mntflags = 0;
    char 		specname[MAXNAMELEN];

    sprintf(specname, "/dev/%s", disk_dev_name);
    args.fspec = specname;		/* The name of the device file. */

    if (writable == FALSE)
    {
		mntflags |= MNT_RDONLY;
    }

    if (removable)
    {
		mntflags |= MNT_NOSUID | MNT_NODEV;
    }
#define DEFAULT_ROOTUID	-2
    args.export.ex_root = DEFAULT_ROOTUID;

    if (mntflags & MNT_RDONLY)
    {
		args.export.ex_flags = MNT_EXRDONLY;
    }
    else
    {
		args.export.ex_flags = 0;
    }
    
    if (mount(FS_TYPE_UFS, mount_point, mntflags, &args) < 0)
    {
		fprintf(stderr, "mount %s on %s failed: %s\n", specname, mount_point, strerror(errno));
		return (FALSE);
    }

    [self setMounted:TRUE];
    printf("Mounted ufs %s on %s\n", specname, mount_point);
    return (TRUE);
}

- (boolean_t) mount
{
    if (strcmp(fs_type, FS_TYPE_UFS) == 0)
    {
		return [self mount_ufs];
    }
    else if (strcmp(fs_type, FS_TYPE_HFS) == 0)
    {
		return [self mount_foreign];
	}
    else if (strcmp(fs_type, FS_TYPE_CD9660) == 0)
    {
		return [self mount_foreign];
    }
    return (FALSE);
}

#define FORMAT_STRING		"%-10s %-6s %-8s %-5s %-5s %-16s %-16s\n"
- print
{
    printf(FORMAT_STRING,
    		disk_dev_name,
    		dev_type,
    		fs_type,
			!removable ? "yes" : "no",
			writable ? "yes" : "no",
			disk_name, 
			mounted ? mount_point : "[not mounted]" );
    return self;
}

- set:(char * *) var Str:(char *)val
{
    if (*var)
    {
		free(*var);
    }
    if (val == NULL)
    {
		*var = NULL;
    }
    else 
    {
		*var = strdup(val);
    }
    return (self);
}

- setDeviceType:(char *)t
{
    return [self set:&dev_type Str:t];
}

- setFSType:(char *)t
{
    return [self set:&fs_type Str:t];
}

- setDiskName:(char *)d
{
    return [self set:&disk_name Str:d];
}

- setDiskDevName:(char *)d
{
    return [self set:&disk_dev_name Str:d];
}

- setMountPoint:(char *)m
{
    return [self set:&mount_point Str:m];
}

- setMounted:(boolean_t)val
{
    mounted = val;
    return self;
}

- setWritable:(boolean_t)val
{
    writable = val;
    return self;
}

- setRemovable:(boolean_t)val
{
    removable = val;
    return self;
}

- setDirtyFS:(boolean_t)val
{
    dirty = val;
    return self;
}

- free
{
    int 		i;
    char * *	l[] = {	&fs_type,
    					&disk_dev_name,
    					&dev_type,
						&disk_name,
						&mount_point,
						NULL };

    for (i = 0; l[i] != NULL; i++)
    {
		if (*(l[i]))
		{
		    free(*(l[i]));
		}
		*(l[i]) = NULL;
    }
    return [super free];
}
@end

@implementation DiskVolumes

- newVolume:(DiskPtr)media FSType:(char *)fstype
  Removable:(boolean_t) isRemovable Writable:(boolean_t) isWritable
	   Type:(char *)type Stat:(struct statfs *)stat_p Number:(int)stat_number
	
{
	char *			devname = media->ioBSDName;
	struct statfs *	fs_p;
	char * 			fsname = fstype;
	int 			ret;
	char			specName[MAXNAMELEN];
	id 				volume = nil;
		
	sprintf(specName, "/dev/%s", devname);

	ret = foreignProbe(fstype, devname, isRemovable, isWritable, TRUE);
	if (ret == FSUR_RECOGNIZED || ret == -9)
	{
		fsname = foreignLabel(fstype);
		if (fsname == NULL)
		{
			if (strcmp(fstype, FS_TYPE_UFS) == 0)
			{
				fsname = media->ioMediaNameOrNull; /* UFS has old-style label */
			}
			else
			{
				fsname = fstype;
			}
		}
		volume = [[DiskVolume alloc] init];
		[volume setDiskDevName:devname];
		[volume setFSType:fstype];
		[volume setDiskName:fsname];
		[volume setDeviceType:type];
		[volume setWritable:isWritable];
		[volume setRemovable:isRemovable];
		fs_p = fsstat_lookup_spec(stat_p, stat_number, specName, fstype);
		if (fs_p)
		{
			/* already mounted */
			[volume setMounted:TRUE];
			[volume setMountPoint:fs_p->f_mntonname];
		}
		else if (isWritable)
		{
			[volume setDirtyFS:fsck_needed(devname, fstype)];
		}
	}
	return (volume);

}

- init
{
	if ([super init] == nil)
	{
		return nil;
	}
	
	list = [[List alloc] init];
	
	return self;
}

-	do_removable:(boolean_t)do_removable
	eject_removable:(boolean_t)eject_removable
{
	DiskPtr				diskPtr;
	boolean_t			success = FALSE;
	struct statfs *		stat_p;
	int					stat_number;
	char * 				type = "???"; /* Radar 2323887 */
    
	stat_p = get_fsstat_list(&stat_number);
	if (stat_p == NULL || stat_number == 0)
	{
		goto Return;
	}

	for (diskPtr = g.Disks; diskPtr != NULL; diskPtr = diskPtr->next )
	{
		int isWhole, isWritable, isRemovable;
		id volume = nil;

		/* Skip non-new disks */

		if ( kDiskStateNew != diskPtr->state )
		{
			continue;
		}
		
		/* Initialize some convenient flags */
		
		isWhole = ( diskPtr->flags & kDiskArbitrationDiskAppearedWholeDiskMask ) != 0;
		isWritable = ( diskPtr->flags & kDiskArbitrationDiskAppearedLockedMask ) == 0;
		isRemovable = ( diskPtr->flags & kDiskArbitrationDiskAppearedEjectableMask ) != 0;

		/* Eject removable disk and move on to the next one, if that was requested */

		if (isRemovable && do_removable == FALSE)
		{
			if (eject_removable)
			{
				eject_media(diskPtr->ioBSDName);
			}
			continue;
		}

		/* Take appropriate action based on the <ioContent> field */
		if (0 == strcmp( "Apple_UFS", diskPtr->ioContent))
		{
			volume = [self newVolume:diskPtr FSType:FS_TYPE_UFS
						   Removable:isRemovable Writable:isWritable
						   Type:type Stat:stat_p Number:stat_number];
		}
		else if (0 == strcmp( "Apple_HFS", diskPtr->ioContent))
		{
			volume = [self newVolume:diskPtr FSType:FS_TYPE_HFS
						   Removable:isRemovable Writable:isWritable
						   Type:type Stat:stat_p Number:stat_number];
		}
		else if (0 == strcmp( "", diskPtr->ioContent ) && isWhole)
		{
			volume = [self newVolume:diskPtr FSType:FS_TYPE_CD9660
						   Removable:isRemovable Writable:isWritable
						   Type:type Stat:stat_p Number:stat_number];
		}
		else
		{
			if ( gDebug ) printf("autodiskmount: unrecognized device: '%s' / '%s'\n", diskPtr->ioBSDName, diskPtr->ioContent);
			continue;
		}
		if (volume != nil)
		{
			[list addObject:volume];
		}


	} /* for */

    success = TRUE;

Return:
	if (stat_p)
	{
		free(stat_p);
	}

	if (success)
	{
		return self;
	}

	return [self free];

}

- print
{
    int i;

    printf(	FORMAT_STRING,
    		"DiskDev",
    		"Type",
    		"FileSys",
    		"Fixed",
			"Write",
			"Volume Name",
			"Mounted On" );
    for (i = 0; i < [list count]; i++)
    {
		[[list objectAt:i] print];
    }
    return self;
}

- (unsigned) count
{
    return [list count];
}

- objectAt:(unsigned)i
{
    return [list objectAt:i];
}

- free
{
    [[list freeObjects] free];
    list = nil;
    return [super free];
}

- volumeWithMount:(char *) path
{
    int i;

    for (i = 0; i < [list count]; i++) {
	DiskVolume * d = (DiskVolume *)[list objectAt:i];
	if (d->mounted && d->mount_point && strcmp(d->mount_point, path) == 0){
	    return (d);
	}
    }
    return (nil);
}

- (boolean_t) setVolumeMountPoint:d
{
    DiskVolume *	disk = (DiskVolume *)d;
    int 			i = 1;
    boolean_t		is_hfs = (strcmp(disk->fs_type, FS_TYPE_HFS) == 0);
    mode_t			mode = 0755;
    char 			mount_path[MAXLBLLEN + 32];
    struct stat 	sb;

    sprintf(mount_path, "/%s", disk->disk_name);

    while (1)
    {
		if (stat(mount_path, &sb) < 0)
		{
		    if (errno == ENOENT)
		    {
				break;
		    }
		    else
		    {
				fprintf(stderr, "stat(%s) failed, %s\n", mount_path, strerror(errno));
				return (FALSE);
		    }
		}
		else if ([self volumeWithMount:mount_path])
		{
			/* do nothing */
		}
		else if (rmdir(mount_path) == 0)
		{
		    /* it was an empty directory */
		    break;
		}
		sprintf(mount_path, "/%s %d", disk->disk_name, i);
		i++;
    }

    /*
     * For HFS and HFS+ volumes, we change the mode of the mount point.
     * The reason is that this mode is used to determine the default
     * mode for files and directories which have no mode (Mac OS created
     * them). We also set the group owner to "macos" to Blue Box users
     * can open and edit these modeless files.
     */
     
    /*
     * The group-changing code has been removed.
     * And the mode for the mountpoint has been changed to 0777 from 0775
     * since non-owner, non-group users will need write access.
     */
     
    if (is_hfs)
    {
		mode = 0777; /* Radar 2359912 */
    }

    if (mkdir(mount_path, mode) < 0)
    {
		fprintf(stderr, "mountDisks: mkdir(%s) failed, %s\n", mount_path, strerror(errno));
		return (FALSE);
    }

	/* Set the mode again, just in case the umask interfered with the mkdir() */

	if (chmod(mount_path, mode) < 0)
	{
		fprintf(stderr, "mountDisks: chmod(%s) failed: %s\n", mount_path, strerror(errno));
	}

    [disk setMountPoint:mount_path];

    return (TRUE);
}

@end

