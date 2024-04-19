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

//
// Notes:
// -----
// o the on-disk fdisk structure is packed for i386 and has little endian fields
//
// o the relsect block value is relative to the containing media's boundary,
//   and the implicit block size is the drive's hard block size
//
// o the numsect block value's implicit block size is the drive's hard block
//   size
//
// Policies:
// --------
// o the default probe score of the fdisk partition scheme is 1100, only if
//   it contains one or more valid partitions
//

#include <IOKit/assert.h>
#include <IOKit/IOLib.h>
#include <IOKit/storage/IOFDiskPartitionScheme.h>
#include <libkern/OSByteOrder.h>

#define super IOPartitionScheme
OSDefineMetaClassAndStructors(IOFDiskPartitionScheme, IOPartitionScheme);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// FDisk Partition Types

static struct { UInt8 type; char * name; } partitionTypes[] =
{
    { 0x01, "DOS_FAT_12"                     },
    { 0x04, "DOS_FAT_16_S"                   },
    { 0x05, "DOS_Extended"                   },
    { 0x06, "DOS_FAT_16"                     },
    { 0x07, "Windows_NTFS"                   },
    { 0x0a, "Boot_Manager"                   },
    { 0x0b, "DOS_FAT_32"                     },
    { 0x0c, "Windows_FAT_32"                 },
    { 0x0e, "Windows_FAT_16"                 },
    { 0x0f, "Windows_Extended"               },
    { 0x11, "DOS_FAT_12_Hidden"              },
    { 0x14, "DOS_FAT_16_S_Hidden"            },
    { 0x16, "DOS_FAT_16_Hidden"              },
    { 0x17, "Windows_NTFS_Hidden"            },
    { 0x1b, "DOS_FAT_32_Hidden"              },
    { 0x1c, "Windows_FAT_32_Hidden"          },
    { 0x1e, "Windows_FAT_16_Hidden"          },
    { 0x63, "UNIX"                           },
    { 0x82, "Linux_Swap"                     },
    { 0x83, "Linux_Ext2FS"                   },
    { 0x84, "Hibernation"                    },
    { 0x85, "Linux_Extended"                 },
    { 0x86, "Windows_FAT_16_FT"              },
    { 0x87, "Windows_NTFS_FT"                },
    { 0xa5, "FreeBSD"                        },
    { 0xa6, "OpenBSD"                        },
    { 0xa7, "Apple_Rhapsody_UFS"             },
    { 0xa8, "Apple_UFS"                      },
    { 0xa9, "NetBSD"                         },
    { 0xab, "Apple_Boot"                     },
    { 0xb7, "BSDI"                           },
    { 0xb8, "BSDI_Swap"                      },
    { 0xc6, "Windows_FAT_16_FT_Corrupt"      },
    { 0xc7, "Windows_NTFS_FT_Corrupt"        },
    { 0xeb, "BeOS"                           },
    { 0xf2, "DOS_Secondary"                  },
    { 0xfd, "Linux_RAID"                     },
    { 0x00, 0                                }
};

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

bool IOFDiskPartitionScheme::init(OSDictionary * properties = 0)
{
    //
    // Initialize this object's minimal state.
    //

    if (super::init(properties) == false)  return false;

    _buffer     = 0;
    _bufferSize = 0;

    // Validate the compiled size of our important fixed-size structures.

    assert(sizeof(fdisk_part) == 16);
    assert(sizeof(disk_blk0) == 512);

    return true;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void IOFDiskPartitionScheme::free()
{
    //
    // Free all of this object's outstanding resources.
    //

    if (_buffer)  _buffer->release();

    super::free();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

IOService * IOFDiskPartitionScheme::probe(IOService * provider, SInt32 * score)
{
    //
    // Determine whether the provider media contains an fdisk partition map.  If
    // it does, we return "this" to indicate success, otherwise we return zero.
    //

    IOMedia * media = (IOMedia *) provider;

    // State our assumptions.

    assert(OSDynamicCast(IOMedia, provider));

    // Ask superclass' opinion about this probe.

    if (super::probe(provider, score) == 0)  return 0;

    // Determine whether this media object is unformatted.

    if (media->isFormatted() == false)  return 0;

    // Determine whether this media's block size is below our assumed minimum.

    if (media->getPreferredBlockSize() < sizeof(disk_blk0))  return 0;

    // Allocate a buffer large enough to hold one media block.

    _bufferSize = media->getPreferredBlockSize();
    _buffer     = IOBufferMemoryDescriptor::withCapacity(_bufferSize,
                                                         kIODirectionIn);

    if (_buffer == 0)  return 0;

    // Search for a valid fdisk partition on the provider media.

    return identify(media) ? this : 0;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

bool IOFDiskPartitionScheme::start(IOService * provider)
{
    //
    // This method is called once we have been attached to the media object.  We
    // generate the new media objects that will represent our partitions here.
    //

    disk_blk0 * blockZero;
    IOMedia *   media   = (IOMedia *) provider;
    bool        success = false;

    // State our assumptions.

    assert(_buffer);

    // Ask our superclass' opinion.

    if ( !super::start(provider) )  return false;

    // Scan through all the partition entries.

    blockZero = (disk_blk0 *) _buffer->getBytesNoCopy();

    for ( unsigned index = 0; index < DISK_NPART; index++ )
    {
        // Skip all null partition entries (have system id of 0).

        if ( blockZero->parts[index].systid == 0 )
            continue; // (skip)

        // Compute the relative position and size of the new partition.

        UInt64 base = OSSwapLittleToHostInt32(blockZero->parts[index].relsect) *
                      media->getPreferredBlockSize();
        UInt64 size = OSSwapLittleToHostInt32(blockZero->parts[index].numsect) *
                      media->getPreferredBlockSize();

        if ( base == 0 || size == 0 )
            continue; // (skip)

        // Look up a name and a type for this partition.

        const char * aName = "Untitled";
        const char * aHint = 0;

        for (unsigned n = 0; partitionTypes[n].type; n++)
        {
            if ( blockZero->parts[index].systid == partitionTypes[n].type )
            {
                aHint = partitionTypes[n].name;
                break;
            }
        }

        // Ensure the partition definition does not leave the confines of
        // the containing media (they were absolute values after all).

        if ( base + size > media->getSize() )
        {
            IOLog("%s on %s: \"%s\" (partition %d) exceeds confines of "
                  "containing media.\n",
                  getName(), media->getName(), aName, index+1);
            continue; // (skip)
        }

        // Create the new media object.

        IOMedia * newMedia = new IOMedia;

        if ( !newMedia ||
             !newMedia->init( // base (bytes; relative to provider media)
                              base,
                              // size (bytes)
                              size,
                              // natural block size (bytes)
                              media->getPreferredBlockSize(),
                              // is ejectable
                              media->isEjectable(),
                              // is whole
                              false,
                              // is writable
                              media->isWritable(),
                              // content hint
                              aHint ) ||
             !newMedia->attach(this) )
        {
            IOLog("%s on %s: Unable to create media object for \"%s\" "
                  "(partition %d).\n",
                  getName(), media->getName(), aName, index + 1);

            if ( newMedia )  newMedia->release();
            continue; // (skip)
        }

        newMedia->setName(aName);

        // Set a location value (the partition number) for this partition.

        char location[12];
        sprintf(location, "%d", index + 1);
        newMedia->setLocation(location);

        // Create the "Partition ID" key.

        newMedia->setProperty(kIOMediaPartitionID, index + 1, 32);

        // Register the media object with the matching system.

        newMedia->registerService();

        // Release our retain on the new media object (in registry).

        newMedia->release();

        success = true;
    } // (for all partition entries)

    // Release our buffer now that we no longer need it.

    _buffer->release();

    _buffer     = 0;
    _bufferSize = 0;

    // Return success if we found at least one partition.

    return success;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

bool IOFDiskPartitionScheme::identify(IOMedia * media)
{
    //
    // Searches for the existence of an fdisk partition map on the given media.
    //

    bool success = false;

    // State our assumptions.

    assert(_buffer);

    // Open the media object for access.

    if ( media->open(this, 0, kAccessReader) == false )  return false;

    // Read the first block into our buffer.

    _buffer->setDirection(kIODirectionIn);       // (a read)
    _buffer->setLength(_bufferSize);             // (transfer one full block)

    if ( media->read(this, 0, _buffer) == kIOReturnSuccess )
    {
        // Determine whether the partition map signature 0xAA55 is present.

        disk_blk0 * blockZero = (disk_blk0 *) _buffer->getBytesNoCopy();

        if ( OSSwapLittleToHostInt16(blockZero->signature) == DISK_SIGNATURE )
        {
            // Note that media created by Mac OS X Server and OpenStep on i386
            // machines have a valid,  but EMPTY,  fdisk structure co-existing
            // with a valid NeXT label (at the second location).    We resolve
            // the conflict by returning success only for the fdisk structures
            // that do contain partitions.

            for ( unsigned index = 0; index < DISK_NPART; index++ )
            {
                if ( blockZero->parts[index].systid )
                {
                    // We found a used partition entry; set success.
                    success = true;
                    break;
                }
            }
        }
    }

    // Close the media object we opened earlier.

    media->close(this);

    return success;
}
