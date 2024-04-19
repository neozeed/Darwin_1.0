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

#ifndef _IOAPPLEPARTITIONSCHEME_H
#define _IOAPPLEPARTITIONSCHEME_H

#include <IOKit/IOTypes.h>

/*
 * Apple Partition Map Definitions
 *
 * o  "dpme" is short for disk partition map entry
 * o  "dpme" and "Block0" are, and shall remain, 512 bytes long
 * o  for more information see:
 *    o  "Inside Macintosh: Devices" pages 3-12 to 3-15
 *    o  "Inside Macintosh - Volume V" pages V-576 to V-582
 *    o  "Inside Macintosh - Volume IV" page IV-292
 */

#pragma pack(4)     /* (enable 32-bit struct packing for dpme, DDMap, Block0) */

/*
 * General constants.
 */

#define BLOCK0_SIGNATURE  0x4552  /* (unique value for block zero, 'ER')      */
#define DPME_SIGNATURE    0x504D  /* (unique value for partition entry, 'PM') */
#define DPISTRLEN         32

/*
 * Partition map entry, as found in blocks 1..n of the disk.
 */

struct dpme
{
    UInt16  dpme_signature;       /* (unique value for partition entry, 'PM') */
    UInt16  dpme_reserved_1;      /* (reserved for future use)                */
    UInt32  dpme_map_entries;     /* (number of partition entries)            */
    UInt32  dpme_pblock_start;    /* (physical block start of partition)      */
    UInt32  dpme_pblocks;         /* (physical block count of partition)      */
    char    dpme_name[DPISTRLEN]; /* (name of partition)                      */
    char    dpme_type[DPISTRLEN]; /* (type of partition, eg. Apple_HFS)       */
    UInt32  dpme_lblock_start;    /* (logical block start of partition)       */
    UInt32  dpme_lblocks;         /* (logical block count of partition)       */
    UInt32  dpme_flags;           /* (partition flags, see defines below)     */
    UInt32  dpme_boot_block;
    UInt32  dpme_boot_bytes;
    UInt8 * dpme_load_addr;
    UInt8 * dpme_load_addr_2;
    UInt8 * dpme_goto_addr;
    UInt8 * dpme_goto_addr_2;
    UInt32  dpme_checksum;
    UInt8   dpme_process_id[16];
    UInt32  dpme_boot_args[32];
    UInt32  dpme_reserved_3[62];
};
typedef struct dpme DPME;

/*
 * Partition map entry flags (dpme_flags).
 */

#define DPME_FLAGS_VALID          0x00000001                   /* (bit 0)     */
#define DPME_FLAGS_ALLOCATED      0x00000002                   /* (bit 1)     */
#define DPME_FLAGS_IN_USE         0x00000004                   /* (bit 2)     */
#define DPME_FLAGS_BOOTABLE       0x00000008                   /* (bit 3)     */
#define DPME_FLAGS_READABLE       0x00000010                   /* (bit 4)     */
#define DPME_FLAGS_WRITABLE       0x00000020                   /* (bit 5)     */
#define DPME_FLAGS_OS_PIC_CODE    0x00000040                   /* (bit 6)     */
#define DPME_FLAGS_OS_SPECIFIC_2  0x00000080                   /* (bit 7)     */
#define DPME_FLAGS_OS_SPECIFIC_1  0x00000100                   /* (bit 8)     */
#define DPME_FLAGS_RESERVED_2     0xFFFFFE00                   /* (bit 9..31) */

/*
 * Driver descriptor map entry, as found in block zero of the disk. 
 */

struct DDMap
{
    UInt32  ddBlock;              /* (driver's block count, sbBlkSize-blocks) */
    UInt16  ddSize;               /* (driver's block count, 512-blocks)       */
    UInt16  ddType;               /* (driver's system type)                   */
};
typedef struct DDMap DDMap;

/*
 * Block zero of the disk.
 */

struct Block0
{
    UInt16  sbSig;                     /* (unique value for block zero, 'ER') */
    UInt16  sbBlkSize;                 /* (block size for this device)        */
    UInt32  sbBlkCount;                /* (block count for this device)       */
    UInt16  sbDevType;                 /* (device type)                       */
    UInt16  sbDevId;                   /* (device id)                         */
    UInt32  sbData;                    /* (not used)                          */
    UInt16  sbDrvrCount;               /* (driver descriptor count)           */
    UInt16  sbMap[247];                /* (driver descriptor table)           */
};
typedef struct Block0 Block0;

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

class IOApplePartitionScheme : public IOPartitionScheme
{
    OSDeclareDefaultStructors(IOApplePartitionScheme);

protected:
    IOBufferMemoryDescriptor * _buffer;      /* (necessarily one media block) */
    UInt32                     _bufferSize;  /* (necessarily one media block) */

    bool                       _masteredAt512;

    /*
     * Free all of this object's outstanding resources.
     */

    virtual void free(void);

    /*
     * Searches for the existence of an apple partition map on the given media.
     */

    virtual bool identify(IOMedia * media);

public:

    /*
     * Initialize this object's minimal state.
     */

    virtual bool init(OSDictionary * properties = 0);

    /*
     * Determine whether the provider media contains an apple partition map.  If
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

#endif /* !_IOAPPLEPARTITIONSCHEME_H */
