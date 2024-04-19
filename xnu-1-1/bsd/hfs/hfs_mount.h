/*
 * Copyright (c) 1997-1998 Apple Computer, Inc. All Rights Reserved
 *
 *		MODIFICATION HISTORY (most recent first):
 *
 *	   20-Aug-1998	Scott Roberts	Added uid, gid, and mask to hfs_mount_args.
 *	   24-Jun-1998	Don Brady		Added time zone info to hfs_mount_args (radar #2226387).
 *	   30-Jul-1997	Pat Dirks		created
 */

#ifndef _HFS_MOUNT_H_
#define _HFS_MOUNT_H_

#include <sys/mount.h>
#include <sys/time.h>

/*
 * Arguments to mount HFS-based filesystems
 */

struct hfs_mount_args {
	char	*fspec;			/* block special device to mount */
	struct	export_args export;	/* network export information */
	uid_t	hfs_uid;		/* uid that owns hfs files (standard HFS only) */
	gid_t	hfs_gid;		/* gid that owns hfs files (standard HFS only) */
	mode_t	hfs_mask;		/* mask to be applied for hfs perms  (standard HFS only) */
	u_long	hfs_encoding;		/* encoding for this volume (standard HFS only) */
	struct	timezone hfs_timezone;	/* user time zone info (standard HFS only) */
	int	flags;			/* mounting flags, see below */
};

#define HFSFSMNT_NOXONFILES	0x1	/* disable execute permissions for files */

#endif /* ! _HFS_MOUNT_H_ */
