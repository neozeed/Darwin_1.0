/*
 * Copyright (c) 1999-2000 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 1997-2000 Apple Computer, Inc. All Rights Reserved
 *
 *		MODIFICATION HISTORY (most recent first):
*	   28-Jan-2000	Don Brady		Add support for hfs encoding modules (radar #2428608).
 *	   26-Jan-1999	Don Brady		Synchronize volume/root create dates (radar #2299171).
 *	   24-Jun-1998	Don Brady		Pass time zone info to hfs on mount (radar #2226387).
 */
#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/vnode.h>
#include <sys/wait.h>
#include <bsd/sys/time.h> // gettimeofday

#include <ctype.h>
#include <err.h>
#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <bsd/fcntl.h>
#include <hfs/hfs_mount.h>
#include <sys/attr.h>

/* the following will all be replaced by <hfs/hfs_format.h> */
#include <hfs/hfscommon/headers/system/ConditionalMacros.h>
#include <hfs/hfscommon/headers/system/MacOSTypes.h>
#include <hfs/hfscommon/headers/system/MacOSStubs.h>
#include <hfs/hfscommon/headers/HFSVolumes.h>


/* bek 5/20/98 - [2238317] - mntopts.h needs to be installed in a public place */

#define Radar_2238317 1

#if ! Radar_2238317

#include <mntopts.h>

#else //  Radar_2238317

/*-
 * Copyright (c) 1994
 *      The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)mntopts.h	8.7 (Berkeley) 3/29/95
 */

struct mntopt {
	const char *m_option;	/* option name */
	int m_inverse;		/* if a negative option, eg "dev" */
	int m_flag;		/* bit to set, eg. MNT_RDONLY */
	int m_altloc;		/* 1 => set bit in altflags */
};

/* User-visible MNT_ flags. */
#define MOPT_ASYNC		{ "async",	0, MNT_ASYNC, 0 }
#define MOPT_NODEV		{ "dev",	1, MNT_NODEV, 0 }
#define MOPT_NOEXEC		{ "exec",	1, MNT_NOEXEC, 0 }
#define MOPT_NOSUID		{ "suid",	1, MNT_NOSUID, 0 }
#define MOPT_RDONLY		{ "rdonly",	0, MNT_RDONLY, 0 }
#define MOPT_SYNC		{ "sync",	0, MNT_SYNCHRONOUS, 0 }
#define MOPT_UNION		{ "union",	0, MNT_UNION, 0 }
#define MOPT_USERQUOTA		{ "userquota",	0, 0, 0 }
#define MOPT_GROUPQUOTA		{ "groupquota",	0, 0, 0 }

/* Control flags. */
#define MOPT_FORCE		{ "force",	0, MNT_FORCE, 0 }
#define MOPT_UPDATE		{ "update",	0, MNT_UPDATE, 0 }
#define MOPT_RO			{ "ro",		0, MNT_RDONLY, 0 }
#define MOPT_RW			{ "rw",		1, MNT_RDONLY, 0 }

/* This is parsed by mount(8), but is ignored by specific mount_*(8)s. */
#define MOPT_AUTO		{ "auto",	0, 0, 0 }

#define MOPT_FSTAB_COMPAT						\
	MOPT_RO,							\
	MOPT_RW,							\
	MOPT_AUTO

/* Standard options which all mounts can understand. */
#define MOPT_STDOPTS							\
	MOPT_USERQUOTA,							\
	MOPT_GROUPQUOTA,						\
	MOPT_FSTAB_COMPAT,						\
	MOPT_NODEV,							\
	MOPT_NOEXEC,							\
	MOPT_NOSUID,							\
	MOPT_RDONLY,							\
	MOPT_UNION

void getmntopts __P((const char *, const struct mntopt *, int *, int *));
extern int getmnt_silent;

#endif // Radar_2238317

struct mntopt mopts[] = {
	MOPT_STDOPTS,
	MOPT_UPDATE,
	{ NULL }
};

#define HFS_MOUNT_TYPE				"hfs"

#define DEFAULT_ROOTUID	-2
#define DEFAULT_ANON_UID -2

gid_t	a_gid __P((char *));
uid_t	a_uid __P((char *));
mode_t	a_mask __P((char *));
struct hfs_mnt_encoding * a_encoding __P((char *));
void	usage __P((void));


typedef struct CreateDateAttrBuf {
    u_long size;
    struct timespec creationTime;
} CreateDateAttrBuf;

#define	HFS_BLOCK_SIZE	512

/*
 *	This is the straight GMT conversion constant:
 *	00:00:00 January 1, 1970 - 00:00:00 January 1, 1904
 *	(3600 * 24 * ((365 * (1970 - 1904)) + (((1970 - 1904) / 4) + 1)))
 */
#define MAC_GMT_FACTOR		2082844800UL

#define KMOD_LOAD_COMMAND	"/sbin/kmodload"

#define ENCODING_MODULE_PATH	"/System/Library/Filesystems/hfs.fs/Encodings/"

#define MXENCDNAMELEN	16	/* Maximun length of encoding name string */

struct hfs_mnt_encoding {
	char	encoding_name[MXENCDNAMELEN];	/* encoding type name */
	u_long	encoding_id;			/* encoding type number */
};


/*
 * Lookup table for hfs encoding names
 */
struct hfs_mnt_encoding hfs_mnt_encodinglist[] = {
	{ "Roman",	0 },	/* default */
	{ "Japanese",	1 },
	{ "Korean",	3 },
	{ "Arabic",	4 },
	{ "Hebrew",	5 },
	{ "Greek",	6 },
	{ "Cyrillic",	7 }
};


u_long getVolumeCreateDate(const char *device)
{
	int fd = 0;
	off_t offset;
	char * bufPtr;
	HFSMasterDirectoryBlock * mdbPtr;
	u_long volume_create_time = 0;

	bufPtr = (char *)malloc(HFS_BLOCK_SIZE);
	if ( ! bufPtr ) goto exit;

	fd = open( device, O_RDONLY | O_NDELAY, 0 );
	if( fd <= 0 ) goto exit;

	offset = (off_t)(2 * HFS_BLOCK_SIZE);
	if (lseek(fd, offset, SEEK_SET) != offset) goto exit;

	if (read(fd, bufPtr, HFS_BLOCK_SIZE) != HFS_BLOCK_SIZE) goto exit;

	mdbPtr = (HFSMasterDirectoryBlock *) bufPtr;

	/* get the create date from the MDB (embedded case) or Volume Header */

	if ((mdbPtr->drSigWord == kHFSSigWord)  &&  (mdbPtr->drEmbedSigWord == kHFSPlusSigWord)) {
		/* Embedded volume*/
		volume_create_time = mdbPtr->drCrDate;
	} else if (mdbPtr->drSigWord == kHFSPlusSigWord ) {
		HFSPlusVolumeHeader * volHdrPtr = (HFSPlusVolumeHeader *) bufPtr;

		volume_create_time = volHdrPtr->createDate;
	} else {
		goto exit;	/* cound not match signature */
	}

	if (volume_create_time > MAC_GMT_FACTOR)
		volume_create_time -= MAC_GMT_FACTOR;
	else
		volume_create_time = 0;	/* don't let date go negative! */

exit:
	if ( fd > 0 )
		close( fd );

	if ( bufPtr )
		free( bufPtr );

	return volume_create_time;
}

void syncCreateDate(const char *mntpt, u_long localCreateTime)
{
	int result;
	char path[256];
	struct attrlist	attributes;
	CreateDateAttrBuf attrReturnBuffer;
	int64_t gmtCreateTime;
	int32_t gmtOffset;
	int32_t newCreateTime;

	snprintf(path, sizeof(path), "%s/", mntpt);

	attributes.bitmapcount	= ATTR_BIT_MAP_COUNT;
	attributes.reserved		= 0;
	attributes.commonattr	= ATTR_CMN_CRTIME;
	attributes.volattr 		= 0;
	attributes.dirattr 		= 0;
	attributes.fileattr 	= 0;
	attributes.forkattr 	= 0;

	result = getattrlist(path, &attributes, &attrReturnBuffer, sizeof(attrReturnBuffer), 0 );
	if (result) return;

	gmtCreateTime = attrReturnBuffer.creationTime.tv_sec;
	gmtOffset = gmtCreateTime - (int64_t) localCreateTime + 900;
	if (gmtOffset > 0) {
		gmtOffset = 1800 * (gmtOffset / 1800);
	} else {
		gmtOffset = -1800 * ((-gmtOffset + 1799) / 1800);
	}
	
	newCreateTime = localCreateTime + gmtOffset;

	/*
	 * if the root directory's create date doesn't match
	 * and its within +/- 15 seconds, then update it
	 */
	if ((newCreateTime != attrReturnBuffer.creationTime.tv_sec) &&
		(( newCreateTime - attrReturnBuffer.creationTime.tv_sec) > -15) &&
		((newCreateTime - attrReturnBuffer.creationTime.tv_sec) < 15)) {

		attrReturnBuffer.creationTime.tv_sec = (u_long) newCreateTime;
		(void) setattrlist (path,
				    &attributes,
				    &attrReturnBuffer.creationTime,
				    sizeof(attrReturnBuffer.creationTime),
				    0);
	}
}

/*
 * load_encoding
 * loads an hfs encoding converter module into the kernel
 *
 * Note: unloading of encoding converter modules is done in the kernel
 */
static void
load_encoding(struct hfs_mnt_encoding *encp)
{
	int pid;
	int loaded;
	union wait status;
	struct stat sb;
	char kmodfile[MAXPATHLEN];
	
	/* MacRoman encoding (0) is built into the kernel */
	if (encp->encoding_id == 0) return;

	sprintf(kmodfile, "%sHFS_%s.kmod", ENCODING_MODULE_PATH, encp->encoding_name);
	if (stat(kmodfile, &sb) == -1)
		errx(1, "unable to find: %s", kmodfile);

	loaded = 0;
	pid = fork();
	if (pid == 0) {
		(void) execl(KMOD_LOAD_COMMAND, KMOD_LOAD_COMMAND, kmodfile, NULL);

		exit(1);	/* We can only get here if the exec failed */
	} else if (pid != -1) {
		if ((waitpid(pid, (int *)&status, 0) == pid) && WIFEXITED(status)) {
			/* we attempted a load */
			loaded = 1;
		}
	}

	if (!loaded)
		errx(1, "unable to load: %s", kmodfile);
}

int
main(argc, argv)
	int argc;
	char **argv;
{
	struct hfs_mount_args args;
	int ch, mntflags;
	char *dev, *dir;
	int mountStatus;
	struct timeval dummy_timeval; /* gettimeofday() crashes if the first argument is NULL */
	u_long localCreateTime;
	struct hfs_mnt_encoding *encp;

	mntflags = 0;
	encp = NULL;
	(void)memset(&args, '\0', sizeof(struct hfs_mount_args));

   	/*
   	 * For a mount update, the following args must be explictly
   	 * passed in as options to change their value.  On a new
   	 * mount, default values will be computed for all args.
   	 */
	args.flags = VNOVAL;
	args.hfs_uid = (uid_t)VNOVAL;
	args.hfs_gid = (gid_t)VNOVAL;
	args.hfs_mask = (mode_t)VNOVAL;
	args.hfs_encoding = (u_long)VNOVAL;


	optind = optreset = 1;		/* Reset for parse of new argv. */
	while ((ch = getopt(argc, argv, "xu:g:m:e:o:")) != EOF)
		switch (ch) {
		case 'x':
			if (args.flags == VNOVAL)
				args.flags = 0;
			args.flags |= HFSFSMNT_NOXONFILES;
			break;
		case 'u':
			args.hfs_uid = a_uid(optarg);
			break;
		case 'g':
			args.hfs_gid = a_gid(optarg);
			break;
		case 'm':
			args.hfs_mask = a_mask(optarg);
			break;
		case 'e':
			encp = a_encoding(optarg);
			break;
		case 'o':
			{
				int dummy;
				getmntopts(optarg, mopts, &mntflags, &dummy);
			}
			break;
		case '?':
			usage();
			break;
		default:
#if DEBUG
			printf("mount_hfs: ERROR: unrecognized ch = '%c'\n", ch);
#endif
			usage();
		}; /* switch */
    
	argc -= optind;
	argv += optind;

	if (argc != 2) {
#if DEBUG
		printf("mount_hfs: ERROR: argc == %d != 2\n", argc);
#endif
		usage();
	}

	dev = argv[0];
	dir = argv[1];

	args.fspec = dev;
	args.export.ex_root = DEFAULT_ROOTUID;
	args.export.ex_anon.cr_uid = DEFAULT_ANON_UID;		/* mapping for anonymous users */
	if (mntflags & MNT_RDONLY)
		args.export.ex_flags = MNT_EXRDONLY;
	else
		args.export.ex_flags = 0;

	/* HFS volumes need timezone info to convert local to GMT */
	(void) gettimeofday( &dummy_timeval, &args.hfs_timezone );

	/* load requested encoding (if any) for hfs volume */
	if (encp != NULL) {
		load_encoding(encp);
		args.hfs_encoding = encp->encoding_id;
	}
	
	/*
	 * For a new mount (non-update case) fill in default values for all args
	 */
	if ((mntflags & MNT_UPDATE) == 0) {
		struct stat sb;

           	if (args.flags == VNOVAL)
            		args.flags = 0;

		if (args.hfs_encoding == (u_long)VNOVAL) 
			args.hfs_encoding = 0;

		/* when the mountpoint is root, use default values */
		if (strcmp(dir, "/") == 0) {
			sb.st_mode = 0777;
			sb.st_uid = 0;
			sb.st_gid = 0;
			
		/* otherwise inherit from the mountpoint */
		} else if (stat(dir, &sb) == -1)
			err(1, "stat %s", dir);

		if (args.hfs_uid == (uid_t)VNOVAL)
			args.hfs_uid = sb.st_uid;

		if (args.hfs_gid == (gid_t)VNOVAL)
			args.hfs_gid = sb.st_gid;

		if (args.hfs_mask == (mode_t)VNOVAL)
			args.hfs_mask = sb.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO);
	}
#if DEBUG
    printf("mount_hfs: calling mount: \n" );
    printf("\tdevice = %s\n", dev);
    printf("\tmount point = %s\n", dir);
    printf("\tmount flags = 0x%x\n", mntflags);
    printf("\targ flags = 0x%x\n", args.flags);
    printf("\tuid = %d\n", args.hfs_uid);
    printf("\tgid = %d\n", args.hfs_gid);
    printf("\tmode = %o\n", args.hfs_mask);
    printf("\tencoding = %ld\n", args.hfs_encoding);

#endif
	if ((mntflags & MNT_RDONLY) == 0) {
		/*
		 * get the volume's create date so we can synchronize
		 * it with the root directory create date
		 */
		localCreateTime = getVolumeCreateDate(dev);
	}
	else {
		localCreateTime = 0;
	}

    if ((mountStatus = mount(HFS_MOUNT_TYPE, dir, mntflags, &args)) < 0) {
#if DEBUG
        printf("mount_hfs: error on mount(): error = %d.\n", mountStatus);
#endif
        err(1, NULL);
        };
   
	/*
	 * synchronize the root directory's create date
	 * with the volume's create date
	 */
	if (localCreateTime)
		syncCreateDate(dir, localCreateTime);

    exit(0);
}


gid_t
a_gid(s)
    char *s;
{
    struct group *gr;
    char *gname;
    gid_t gid = 0;

    if ((gr = getgrnam(s)) != NULL)
        gid = gr->gr_gid;
    else {
        for (gname = s; *s && isdigit(*s); ++s);
        if (!*s)
            gid = atoi(gname);
        else
            errx(1, "unknown group id: %s", gname);
    }
    return (gid);
}

uid_t
a_uid(s)
    char *s;
{
    struct passwd *pw;
    char *uname;
    uid_t uid = 0;

    if ((pw = getpwnam(s)) != NULL)
        uid = pw->pw_uid;
    else {
        for (uname = s; *s && isdigit(*s); ++s);
        if (!*s)
            uid = atoi(uname);
        else
            errx(1, "unknown user id: %s", uname);
    }
    return (uid);
}

mode_t
a_mask(s)
    char *s;
{
    int done, rv;
    char *ep;

    done = 0;
    rv = -1;
    if (*s >= '0' && *s <= '7') {
        done = 1;
        rv = strtol(optarg, &ep, 8);
    }
    if (!done || rv < 0 || *ep)
        errx(1, "invalid file mode: %s", s);
    return (rv);
}

struct hfs_mnt_encoding *
a_encoding(s)
	char *s;
{
	char *uname;
	int i;
	u_long encoding;
	struct hfs_mnt_encoding *enclist = hfs_mnt_encodinglist;
	int maxencodingslots = sizeof(hfs_mnt_encodinglist) / sizeof(struct hfs_mnt_encoding);

	for (i=0, enclist = hfs_mnt_encodinglist; i < maxencodingslots; i++, enclist++) {
		if (strcmp(enclist->encoding_name, s) == 0)
			return (enclist);
	}

	for (uname = s; *s && isdigit(*s); ++s);

	if (*s) goto unknown;

	encoding = atoi(uname);
	for (i=0, enclist = hfs_mnt_encodinglist; i < maxencodingslots; i++, enclist++) {
		if (enclist->encoding_id == encoding)
			return (enclist);
	}

unknown:
	errx(1, "unknown encoding: %s", uname);
	return (NULL);
}

void
usage()
{
	(void)fprintf(stderr,
               "usage: mount_hfs [-x] [-u user] [-g group] [-m mask] [-e encoding] [-o options] special-device filesystem-node\n");
	exit(1);
}
