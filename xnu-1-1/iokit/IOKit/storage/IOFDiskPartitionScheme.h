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

#ifndef _IOFDISKPARTITIONSCHEME_H
#define _IOFDISKPARTITIONSCHEME_H

#include <IOKit/IOTypes.h>

/*
 * FDisk Partition Map Definitions
 */

#pragma pack(2)   /* (enable 16-bit struct packing for fdisk_part, disk_blk0) */

/*
 * General constants.
 */

#define DISK_BLK0SZ    sizeof(struct disk_blk0)       /* (size of block zero) */
#define DISK_BOOTSZ    446               /* (size of boot code in block zero) */
#define DISK_SIGNATURE 0xAA55                /* (unique value for block zero) */
#define DISK_NPART     4                     /* (number of partition entries) */

/*
 * Partition map entry, as found in blocks zero of the disk.
 */

struct fdisk_part
{
    UInt8   bootid;    /* (is bootable?)                                      */
    UInt8   beghead;   /* (beginning head)                                    */
    UInt8   begsect;   /* (beginning sector; beginning cylinder, high 2 bits) */
    UInt8   begcyl;    /* (beginning cylinder, low 8 bits)                    */
    UInt8   systid;    /* (type of partition)                                 */
    UInt8   endhead;   /* (ending head)                                       */
    UInt8   endsect;   /* (ending sector; ending cylinder, high 2 bits)       */
    UInt8   endcyl;    /* (ending cylinder, low 8 bits)                       */
    UInt32  relsect;   /* (physical block start of partition)                 */
    UInt32  numsect;   /* (physical block count of partition)                 */
};

/*
 * Block zero of the disk.
 */

struct disk_blk0
{
    UInt8             bootcode[DISK_BOOTSZ]; /* (boot code)                   */
    struct fdisk_part parts[DISK_NPART];     /* (partition entries)           */
    UInt16            signature;             /* (unique value for block zero) */
};

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

class IOFDiskPartitionScheme : public IOPartitionScheme
{
    OSDeclareDefaultStructors(IOFDiskPartitionScheme);

protected:
    IOBufferMemoryDescriptor * _buffer;      /* (necessarily one media block) */
    UInt32                     _bufferSize;  /* (necessarily one media block) */

    /*
     * Free all of this object's outstanding resources.
     */

    virtual void free(void);

    /*
     * Searches for the existence of an fdisk partition map on the given media.
     */

    virtual bool identify(IOMedia * media);

public:

    /*
     * Initialize this object's minimal state.
     */

    virtual bool init(OSDictionary * properties = 0);

    /*
     * Determine whether the provider media contains an fdisk partition map.  If
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

#endif /* !_IOFDISKPARTITIONSCHEME_H */
