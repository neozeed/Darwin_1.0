/*
 * Copyright (c) 1997-1998 Apple Computer, Inc. All Rights Reserved
 *
 *		MODIFICATION HISTORY (most recent first):
 *
 *	   20-Aug-1998	Scott Roberts	Added uid, gid, and mask to hfs_mount_args.
 *	   24-Jun-1998	Don Brady		Added time zone info to hfs_mount_args (radar #2226387).
 *	   30-Jul-1997	Pat Dirks		created
 */

#ifndef _HFS_ENCODINGS_H_
#define _HFS_ENCODINGS_H_

/*
 * HFS Filename Encoding Converters Interface
 *
 * Private Interface for adding hfs filename
 * encoding converters. These are not needed
 * for HFS Plus volumes (since they already
 * have Unicode filenames).
 *
 * Used by HFS Encoding Converter Kernel Modules
 * (like HFS_Japanese.kmod) to register their
 * encoding conversion routines.
 */

typedef int (* hfs_to_unicode_func_t)(Str31 hfs_str, UniChar *uni_str,
		UInt32 maxCharLen, UInt32 *usedCharLen);

typedef int (* unicode_to_hfs_func_t)(UniChar *uni_str, UInt32 unicodeChars,
		Str31 hfs_str);


int hfs_addconverter(int kmod_id, UInt32 encoding, hfs_to_unicode_func_t get_unicode,
			unicode_to_hfs_func_t get_hfsname);

int hfs_remconverter(int kmod_id, UInt32 encoding);


#endif /* ! _HFS_ENCODINGS_H_ */
