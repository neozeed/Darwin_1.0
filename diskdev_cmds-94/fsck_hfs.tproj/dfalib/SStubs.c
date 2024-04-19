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
/* SStubs.c */

//#include <MacMemory.h>
//#include <DateTimeUtils.h>

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include "Scavenger.h"



OSErr FlushVol(ConstStr63Param volName, short vRefNum)
{
	sync();
	
	return (0);
}


OSErr MemError()
{
	return (0);
}


UInt32 TickCount()
{
	return (0);
}													ONEWORDINLINE(0xA975);


OSErr GetVolumeFeatures( SGlobPtr GPtr )
{
	GPtr->volumeFeatures = supportsTrashVolumeCacheFeatureMask + supportsHFSPlusVolsFeatureMask;

	return( noErr );
}


Handle NewHandleClear(Size byteCount)
{
	return NewHandle(byteCount);
}

Handle NewHandle(Size byteCount)
{
	Handle h;
	Ptr p = NULL;

	if (byteCount < 0)
		return NULL;
		
	if (!(h = malloc(sizeof(Ptr) + sizeof(Size))))
		return NULL;
		
	if (byteCount)
		if (!(p = calloc(1, byteCount)))
		{
			free(h);
			return NULL;
		}
	
	*h = p;
	
	*((Size *)(h + 1)) = byteCount;	
	
	return h;
}

void DisposeHandle(Handle h)
{
	if (h)
	{
		if (*h)
			free(*h);
		free(h);
	}
}

Size GetHandleSize(Handle h)
{
	return h ? *((Size *)(h + 1)) : 0;
}

void SetHandleSize(Handle h, Size newSize)
{
	Ptr p = NULL;

	if (!h || newSize < 0)
		return;

	if ((p = realloc(*h, newSize)))
	{
		*h = p;
		*((Size *)(h + 1)) = newSize;
	}
}


OSErr PtrAndHand(const void *ptr1, Handle hand2, long size)
{
	Ptr p = NULL;
	Size old_size = 0;

	if (!hand2)
		return -109;
	
	if (!ptr1 || size < 1)
		return 0;
		
	old_size = *((Size *)(hand2 + 1));

	if (!(p = realloc(*hand2, size + old_size)))
		return -108;

	*hand2 = p;
	*((Size *)(hand2 + 1)) = size + old_size;
	
	memcpy(*hand2 + old_size, ptr1, size);
	
	return 0;
}


/* error messages (500 - 570 */
char *errstr[] =
{
	"Invalid PEOF",
	"Invalid LEOF",
	"Invalid directory valence",
	"Invalid CName",
	"Invalid node height",
	"Missing file record for file thread",
	"Invalid allocation block size",
	"Invalid number of allocation blocks",
	"Invalid VBM start block",
	"Invalid allocation block start",
	"Invalid extent entry",
	"Overlapped extent allocation",
	"Invalid BTH length",
	"BT map too short during repair",
	"Invalid root node number",
	"Invalid node type",
	"",
	"Invalid record count",
	"Invalid index key",
	"Invalid index link",
	"Invalid sibling link",
	"Invalid node structure",
	"Overlapped node allocation",
	"Invalid map node linkage",
	"Invalid key length",
	"Keys out of order",
	"Invalid map node",
	"Invalid header node",
	"Exceeded maximum B-tree depth",
	"",
	"Invalid catalog record type",
	"Invalid directory record length",
	"Invalid thread record length",
	"Invalid file record length",
	"Missing thread record for root dir",
	"Missing thread record",
	"Missing directory record",
	"Invalid key for thread record",
	"Invalid parent CName in thread record",
	"Invalid catalog record length",
	"Loop in directory hierarchy",
	"Invalid root directory count",
	"Invalid root file count",
	"Invalid volume directory count",
	"Invalid volume file count",
	"Invalid catalog PEOF",
	"Invalid extent file PEOF",
	"Nesting of folders has exceeded the recommended limit of 100",
	"",
	"File thread flag not set in file rec",
	"Missing folder detected",
	"Invalid file name",
	"Invalid file clump size",
	"Invalid B-tree Header",
	"Directory name locked",
	"Catalog file entry not found for extent",
	"Custom icon missing",
	"Master Directory Block needs minor repair",
	"Volume Header needs minor repair",
	"Volume Bit Map needs minor repair",
	"Invalid B-tree node size",
	"Invalid catalog record type found",
	"",
	"Reserved fields in the catalog record have incorrect data",
	"Invalid file or directory ID found",
	"The version in the VolumeHeader is not compatable with this version of Disk First Aid",
	"Disk full error"
};

void WriteError( SGlobPtr GPtr, short msgID, UInt32 tarID, UInt32 tarBlock )
{
	short errID;

	errID = abs(msgID);

	if (GPtr->logLevel > 0 && errID >= E_FirstError && errID <= E_LastError) {
		errID -= E_FirstError;
		printf("  Problem: %s, %ld, %ld\n", errstr[errID], tarID, tarBlock);
	}
}


/* summary messages (1 - 22) */
char *msgstr[] =
{
	"",
	"Checking disk volume.",
	"Checking extent file B-tree.",
	"Checking extent file.",
	"Checking catalog B-tree.",
	"Checking catalog file.",
	"Checking catalog hierarchy.",
	"Checking volume information.",
	"Checking for missing folders.",
	"Repairing volume.",
	"",
	"Checking for orphaned extents.",
	"Checking desktop database.",
	"Checking volume bit map.",
	"Checking HFS volume structures.",
	"Checking HFS Plus volume structures.",
	"Checking Attributes B-tree.",
	"Rebuilding Extents B-tree.",
	"Rebuilding Catalog B-tree.",
	"Rebuilding Attributes B-tree.",
	"MountCheck found serious errors.",
	"MountCheck found minor errors.",
	"Internal files overlap.",
	"Filesystem clean; skipping checks.",
	"Filesystem dirty; skipping checks."
};



void WriteMsg( SGlobPtr GPtr, short messageID, short messageType )
{
    if (GPtr->logLevel > 1 && messageID > 0 &&  messageID <= M_LastMessage) {
        printf("** %s\n", msgstr[messageID]);
        // This flush is there so that better gui output can be recieved from DFA.app and DiskUtility
        fflush(stdout);
    }
}


