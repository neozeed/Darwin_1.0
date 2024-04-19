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
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/mount.h>

/* mount_hfs.h should be exported from kernel! */
#if 0
#include <hfs/mount_hfs.h>
#else

	struct hfs_mount_args {
	    char 		*ma_fspec;	/* block special device to mount */
	    struct export_args	ma_export;
	    uid_t		ma_uid;		/* (HFS only) */
	    gid_t		ma_gid;		/* (HFS only) */
	    mode_t  		ma_mask;	/* (HFS only) */
	    u_long		ma_encoding;	/* (HFS only) */
	    struct timezone 	ma_timezone;	/* (HFS only) */
	    int			ma_flags;
	};
#endif

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "fsck_hfs.h"
#include "dfalib/CheckHFS.h"


/* Global Variables for front end */
const char *cdevname;		/* name of device being checked */
char	*progname;
char	nflag;			/* assume a no response */
char	yflag;			/* assume a yes response */
char	preen;			/* just fix normal inconsistencies */
char	force;			/* force fsck even if clean (preen only) */
char	quick;			/* quick check returns clean, dirty, or failure */
char	debug;			/* output debugging info */
char	hotroot;		/* checking root device */
char guiControl; /* this app should output info for gui control */

int	upgrading;		/* upgrading format */

int	fsmodified;		/* 1 => write done to file system */
int	fsreadfd;		/* file descriptor for reading file system */
int	fswritefd;		/* file descriptor for writing file system */



static int checkfilesys __P((char * filesys));
static int setup __P((char *dev));
static void usage __P((void));

void
main(argc, argv)
 int	argc;
 char	*argv[];
{
	int ch;
	int ret;
	extern int optind;

	if (progname = strrchr(*argv, '/'))
		++progname;
	else
		progname = *argv;


	while ((ch = getopt(argc, argv, "acdfnpquvyg")) != EOF) {
		switch (ch) {
		case 'd':
			debug++;
			break;

		case 'f':
			force++;
			break;

		case 'c':
			(void) fprintf(stderr,
			    "%s: deprecated option \"-c\" (use -n instead)\n",
			    progname);
			/* fall through */
		case 'n':
			nflag++;
			yflag = 0;
			break;

		case 'p':
			preen++;
			break;

		case 'q':
		    	quick++;
			break;

		case 'a':
			(void) fprintf(stderr,
			    "%s: deprecated option \"-a\" (use -y instead)\n",
			    progname);
			/* fall through */
		case 'y':
			yflag++;
			nflag = 0;
			break;

		case 'u':
			(void) fprintf(stderr,
			    "%s: upgrade not yet supported\n",  progname);
			exit(1);
			break;

		case 'v':
			(void) fprintf(stderr,
			    "%s: deprecated option \"-%c\" (ignored)\n",
			    progname, ch);
			break;

  case 'g':
    guiControl++;
    break;
			
		default:
			usage();
		}
	}

	argc -= optind;
	argv += optind;

	if (signal(SIGINT, SIG_IGN) != SIG_IGN)
		(void)signal(SIGINT, catch);

	if (argc < 1) {
		(void) fprintf(stderr, "%s: missing special-device\n", progname);
		usage();
	}

	ret = 0;
	while (argc-- > 0)
		ret |= checkfilesys(blockcheck(*argv++));

	exit(ret);
}

static int
checkfilesys(char * filesys)
{
	int flags;
	int result;
	int chkLev, repLev, logLev;

	flags = 0;
	cdevname = filesys;

	if (debug && preen)
		pwarn("starting\n");
	
	if (setup(filesys) == 0) {
		if (preen)
			pfatal("CAN'T CHECK FILE SYSTEM.");
		return (0);
	}

	if (preen == 0) {
	//	printf("** Last Mounted on %s\n", sblock.fs_fsmnt);
		if (hotroot)
			printf("** Root file system\n");
	}

	/* start with defaults for dfa back-end */
	chkLev = kAlwaysCheck;
	repLev = kMajorRepairs;
	logLev = kVerboseLog;

	if (yflag)	
		repLev = kMajorRepairs;

	if (quick) {
		chkLev = kNeverCheck;
		repLev = kNeverRepair;
		logLev = kFatalLog;
	}
	if (preen) {
		repLev = kMinorRepairs;
		chkLev = force ? kAlwaysCheck : kDirtyCheck;
		logLev = kFatalLog;
	}
	if (debug)
		logLev = kDebugLog;

	if (nflag)
		repLev = kNeverRepair;
		
	/*
	 * go check HFS volume...
	 */
	result = CheckHFS(fsreadfd, fswritefd, chkLev, repLev, logLev, guiControl, &fsmodified);
	if (!hotroot) {
		ckfini(1);
		if (quick) {
			if (result == 0) {
				pwarn("FILESYSTEM CLEAN; SKIPPING CHECKS\n");
				return (0);
			} else if (result == R_Dirty) {
				pwarn("FILESYSTEM DIRTY; SKIPPING CHECKS\n");
				return (DIRTYEXIT);
			} else if (result == R_BadSig) {
				pwarn("BAD MDB:  NON HFS SIGNATURE\n");
				return (DIRTYEXIT);
			} else {
				return (EEXIT);
			}
		}
	} else {
		struct statfs stfs_buf;
		/*
		 * Check to see if root is mounted read-write.
		 */
		if (statfs("/", &stfs_buf) == 0)
			flags = stfs_buf.f_flags;
		else
			flags = 0;
		ckfini(flags & MNT_RDONLY);
	}
	
	/* XXX free any allocated memory here */

	if (!fsmodified)
		return ((result == 0) ? 0 : EEXIT);
	if (!preen)
		printf("\n***** FILE SYSTEM WAS MODIFIED *****\n");
	if (hotroot) {
		struct hfs_mount_args args;
		/*
		 * We modified the root.  Do a mount update on
		 * it, unless it is read-write, so we can continue.
		 */
		if (flags & MNT_RDONLY) {		
			bzero(&args, sizeof(args));
			flags |= MNT_UPDATE | MNT_RELOAD;
			if (mount("hfs", "/", flags, &args) == 0)
				return (0);
		}
		if (!preen)
			printf("\n***** REBOOT NOW *****\n");
		sync();
		return (FIXEDROOTEXIT);
	}

	return (0);
}


/*
 * Setup for I/O to device
 * Return 1 if successful, 0 if unsuccessful.
 */
static int
setup(char *dev)
{
	struct stat statb;

	fswritefd = -1;

	if (stat(dev, &statb) < 0) {
		printf("Can't stat %s: %s\n", dev, strerror(errno));
		return (0);
	}
	if ((statb.st_mode & S_IFMT) != S_IFCHR) {
		pfatal("%s is not a character device", dev);
		if (reply("CONTINUE") == 0)
			return (0);
	}
	if ((fsreadfd = open(dev, O_RDONLY)) < 0) {
		printf("Can't open %s: %s\n", dev, strerror(errno));
		return (0);
	}
	if (preen == 0)
		printf("** %s", dev);
	if (nflag || (fswritefd = open(dev, O_WRONLY)) < 0) {
		fswritefd = -1;
		if (preen)
			pfatal("NO WRITE ACCESS");
		printf(" (NO WRITE)");
	}
	if (preen == 0)
		printf("\n");

	return (1);
}



static void
usage()
{
	(void) fprintf(stderr, "usage: %s [-dfnpquy] special-device\n", progname);
	exit(1);
}
