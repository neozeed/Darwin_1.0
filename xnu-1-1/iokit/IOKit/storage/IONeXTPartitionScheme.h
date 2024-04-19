/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#ifndef _IONEXTPARTITIONSCHEME_H
#define _IONEXTPARTITIONSCHEME_H

#include <IOKit/IOTypes.h>

/*
 * NeXT Partition Map Definitions
 */

#pragma pack(2) /* (enable 16-bit struct packing for dl_un, disk[tab,_label]) */

#include <sys/disktab.h>

/*
 * General constants.
 */

#define	NLABELS    4                        /* (number of labels on the disk) */
#define	NBAD       1670                     /* (number of bad sector entries) */
#define	MAXLBLLEN  24

/*
 * Disk label definition.  If dl_version >= DL_V3, then the bad block table has
 * been relocated to a structure separate from the disk label.
 */

typedef union
{
    UInt16         DL_v3_checksum;   /* (ones complement checksum; version 3) */
    SInt32         DL_bad[NBAD];     /* (bad sector table; versions 1 and 2)  */
} dl_un_t;

typedef struct disk_label
{
    SInt32         dl_version;          /* (unique value for NeXT label)      */
    SInt32         dl_label_blkno;      /* (block number where this label is) */
    SInt32         dl_size;             /* (block count for this device)      */
    char           dl_label[MAXLBLLEN]; /* (name of this device)              */
    UInt32         dl_flags;            /* (label flags, see defines below)   */
    UInt32         dl_tag;              /* (volume tag)                       */
    struct disktab dl_dt;               /* (common info in disktab)           */
    dl_un_t        dl_un;
    UInt16         dl_checksum;         /* (ones complement checksum)         */
	
    /* (add things here so dl_checksum stays in a fixed place) */
} disk_label_t;

/*
 * Disk label versions (dl_version).
 */

#define DL_V1           0x4e655854                     /* (version 1: "NeXT") */
#define DL_V2           0x646c5632                     /* (version 2: "dlV2") */
#define DL_V3           0x646c5633                     /* (version 3: "dlV3") */
#define DL_VERSION      DL_V3                          /* (default version)   */

/*
 * Disk label flags (dl_flags).
 */

#define DL_UNINIT       0x80000000               /* (is label uninitialized?) */

/*
 * Aliases for disktab and dl_un fields.
 */

#define dl_name          dl_dt.d_name
#define dl_type          dl_dt.d_type
#define dl_part          dl_dt.d_partitions
#define dl_front         dl_dt.d_front
#define dl_back          dl_dt.d_back
#define dl_ngroups       dl_dt.d_ngroups
#define dl_ag_size       dl_dt.d_ag_size
#define dl_ag_alts       dl_dt.d_ag_alts
#define dl_ag_off        dl_dt.d_ag_off
#define dl_secsize       dl_dt.d_secsize
#define dl_ncyl          dl_dt.d_ncylinders
#define dl_nsect         dl_dt.d_nsectors
#define dl_ntrack        dl_dt.d_ntracks
#define dl_rpm           dl_dt.d_rpm
#define dl_bootfile      dl_dt.d_bootfile
#define dl_boot0_blkno   dl_dt.d_boot0_blkno
#define dl_hostname      dl_dt.d_hostname
#define dl_rootpartition dl_dt.d_rootpartition
#define dl_rwpartition   dl_dt.d_rwpartition
#define dl_v3_checksum   dl_un.DL_v3_checksum
#define dl_bad           dl_un.DL_bad

#pragma options align=reset              /* (reset to default struct packing) */

/*
 * Kernel
 */

#if defined(KERNEL) && defined(__cplusplus)

#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/storage/IOPartitionScheme.h>

/*
 * Class
 */

class IONeXTPartitionScheme : public IOPartitionScheme
{
    OSDeclareDefaultStructors(IONeXTPartitionScheme);

private:
    UInt16 checksum(UInt16 * buffer, UInt32 numberOfShorts) const;

protected:
    IOBufferMemoryDescriptor * _buffer;      /* (necessarily one media block) */
    UInt32                     _bufferSize;  /* (necessarily one media block) */

    UInt64                     _absoluteBase;

    /*
     * Free all of this object's outstanding resources.
     */

    virtual void free(void);

    /*
     * Searches for the existence of a NeXT partition map on the given media.
     */

    virtual bool identify(IOMedia * media);

public:

    /*
     * Initialize this object's minimal state.
     */

    virtual bool init(OSDictionary * properties = 0);

    /*
     * Determine whether the provider media contains an NeXT partition map.  If
     * it does, we return "this" to indicate success, otherwise we return zero.
     */

    virtual IOService * probe(IOService * provider, SInt32 * score);

    /*
     * This method is called once we have been attached to the media object.  We
     * generate the new media objects that will represent our partitions here.
     */

    virtual bool start(IOService * provider);
};

#endif /* defined(KERNEL) && defined(__cplusplus) */

#endif /* !_IONEXTPARTITIONSCHEME_H */
