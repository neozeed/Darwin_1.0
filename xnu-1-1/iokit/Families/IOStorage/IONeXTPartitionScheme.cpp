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
// o the on-disk label structure is packed for M68xxx and has big endian fields
//
// o the on-disk label is stored four times in succession, each label taking up
//   round(sizeof(disk_label_t), drive's hard block size) bytes; CD's suffer of
//   the mastering-with-a-different-hard-block-size problem, however we presume
//   that all CDs produced that exhibit this problem have a valid first label
//   at relative byte zero, and hence the next label increment is irrelevant.
//
// o the dl_label_blkno block value is absolute with respect to the whole disk,
//   and the implicit block size is the drive's hard block size (eg. hard drive
//   is typically 512 bytes, CD is typically 2048 bytes)
//
// o the dl_dt.d_front block value is relative to the containing partition's
//   boundary (ie. relative to the very first label's dl_label_blkno value),
//   and the implicit block size is dl_dt.d_secsize bytes (eg. hd=1024, cd=2048)
//
// o the dl_dt.d_partitions[].p_base block value is absolute with respect to the
//   whole disk, and the implicit block size is dl_dt.d_secsize bytes; an offset
//   of dl_dt.d_front is not factored into the p_base value, but should be in
//   order to calculate the actual start position of the partition on the disk
//
// o the dl_dt.d_partitions[].p_size block value's implicit block size is
//   dl_dt.d_secsize bytes
//
// Policies:
// --------
// o the NeXT partition scheme accepts probes only on "whole" media objects
//   and non-whole media objects with a content hint of 'Apple_Rhapsody_UFS'
//
// o the first two out of the four possible on-disk labels are read while the
//   last two are ignored --  this is done in the spirit of minimizing reads;
//   we forfeit some redundancy, however that is acceptable; the second label
//   MUST be checked due to the fact that i386 machines commonly have the 1st
//   label missing (due to the FDISK boot structure), but the second is valid
//
// o the default probe score of the NeXT partition scheme is 1000
//
// o the eigth partition is always skipped -- this is due to the many varying
//   preconceptions associated with the 8th partition in unix land:   we just
//   ignore it altogether (+ permits us a significant coding simplification)
//
// o the checksum for each NeXT label is not validated -- this is done in the
//   spirit of minimizing reads and perhaps a bit of laziness
//

#include <IOKit/assert.h>
#include <IOKit/IOLib.h>
#include <IOKit/storage/IONeXTPartitionScheme.h>
#include <libkern/OSByteOrder.h>

#define super IOPartitionScheme
OSDefineMetaClassAndStructors(IONeXTPartitionScheme, IOPartitionScheme);

#define kMinimumBlockSize 512

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// NeXT Partition Types

static struct { char * type; char * name; } partitionTypes[] =
{
    { "4.4BSD", "Apple_UFS" },
//  { "4.1BSD", ... },          // V7 with 1K blocks (4.1, 2.9)
//  { "4.2BSD", ... },          // 4.2BSD fast file system
//  { "4.4LFS", ... },          // 4.4BSD log-structured file system
    { 0, 0 }
};

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

bool IONeXTPartitionScheme::init(OSDictionary * properties = 0)
{
    //
    // Initialize this object's minimal state.
    //

    if (super::init(properties) == false)  return false;

    _absoluteBase = 0;
    _buffer       = 0;
    _bufferSize   = 0;

    // Validate the compiled size of our important fixed-size structures.

    assert(sizeof(disktab_t)    ==  514);
    assert(sizeof(partition_t)  ==   46);
    assert(sizeof(disk_label_t) == 7240);

    return true;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void IONeXTPartitionScheme::free()
{
    //
    // Free all of this object's outstanding resources.
    //

    if (_buffer)  _buffer->release();

    super::free();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

IOService * IONeXTPartitionScheme::probe(IOService * provider, SInt32 * score)
{
    //
    // Determine whether the provider media contains an NeXT partition map.  If
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

    if (media->getPreferredBlockSize() < kMinimumBlockSize)  return 0;

    // Determine whether we can rule out the media object as a NeXT partition
    // map container without reading actual data from the media.  We rule out
    // all non-whole media objects without an 'Apple_Rhapsody_UFS' hint.

    if ( media->isWhole() == false )
    {
        if ( strcmp(media->getContentHint(), "Apple_Rhapsody_UFS") )  return 0;
    }

    // Compute this partition's absolute offset with respect to the whole
    // media, since the disk_label structure requires this information --
    // we go down the service hierarchy until we reach the whole media
    // object (or run off the end :-).

    for (IOService * service = media; service; service = service->getProvider())
    {
        if ( OSDynamicCast(IOMedia, service) ) // (is this a media object?)
        {
            _absoluteBase += ((IOMedia *)service)->getBase();
            if (((IOMedia *)service)->isWhole())  break;
        }
    }

    // Allocate a buffer large enough to hold one media block.

    _bufferSize = media->getPreferredBlockSize();
    _buffer     = IOBufferMemoryDescriptor::withCapacity(_bufferSize,
                                                         kIODirectionIn);

    if (_buffer == 0)  return 0;

    // Search for a valid NeXT label on the provider media.

    return identify(media) ? this : 0;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

bool IONeXTPartitionScheme::start(IOService * provider)
{
    //
    // This method is called once we have been attached to the media object.  We
    // generate the new media objects that will represent our partitions here.
    //

    disk_label_t * label;
    IOMedia *      media   = (IOMedia *) provider;
    bool           success = false;

    // State our assumptions.

    assert(_buffer);
    assert(_bufferSize >= kMinimumBlockSize);

    // Ask our superclass' opinion.

    if ( !super::start(provider) )  return false;

    // The block size we calculate for the partitions is important to
    // BSD and its filesystems  -- if we get it wrong, the filesystem
    // will end up reading and writing the wrong blocks.   We use the
    // NeXT label's sector size, NOT the partition's fragment size or
    // it won't work (speaking out of experience).  Period.

    label = (disk_label_t *) _buffer->getBytesNoCopy();

    UInt64 fsBlockSize = OSSwapBigToHostInt32(label->dl_secsize);

    if (fsBlockSize % media->getPreferredBlockSize())
    {
        IOLog("%s on %s: Bad block size is defined.\n",
              getName(), media->getName());
        return false;
    }

    // Scan through all the partition entries.
    //
    // Due to the different preconceptions associated with the eigth partition,
    // our policy is to ignore the eigth partition altogether.  This also makes
    // for a cool coding simplification  -- since the first 7 partition entries
    // completely fit in the first 512-byte block, a read of the next block and
    // caching useful global information from the first block is not necessary
    // just to parse the eigth partition entry.

    assert( kMinimumBlockSize >= ( (UInt8 *) &(label->dl_part[NPART-1]) -
                                   (UInt8 *) label) );

    for ( unsigned index = 0; index < NPART - 1; index++ )
    {
        // Skip all null partition entries (have base of -1 and size of -1).

        if ( (SInt32) OSSwapBigToHostInt32(label->dl_part[index].p_base) <  0 ||
             (SInt32) OSSwapBigToHostInt32(label->dl_part[index].p_size) <= 0 )
            continue; // (skip)

        // Compute the absolute position and size of the new partition.

        UInt64 base;
        UInt64 size;

        base = ( (UInt64) OSSwapBigToHostInt32(label->dl_part[index].p_base) +
                 (UInt64) OSSwapBigToHostInt16(label->dl_front) ) *
               (UInt64) OSSwapBigToHostInt32(label->dl_secsize);
        size = (UInt64) OSSwapBigToHostInt32(label->dl_part[index].p_size) *
               (UInt64) OSSwapBigToHostInt32(label->dl_secsize);

        // Look up a name and a type for this partition.

        const char * aName = "Untitled";
        const char * aHint = 0;

        if ( label->dl_part[index].p_mountpt[0] )
            aName = label->dl_part[index].p_mountpt;
        else if ( label->dl_label[0] )
            aName = label->dl_label;

        for (unsigned n = 0; partitionTypes[n].type; n++)
        {
            if ( !strcmp(label->dl_part[index].p_type, partitionTypes[n].type) )
            {
                aHint = partitionTypes[n].name;
                break;
            }
        }

        if (aHint == 0)  aHint = label->dl_part[index].p_type;

        // Ensure the partition definition does not leave the confines of the
        // containing media (especially that base is an absolute position).

        if ( base < _absoluteBase ||
             base - _absoluteBase + size > media->getSize() )
        {
            IOLog("%s on %s: \"%s\" (partition %d) exceeds confines of "
                  "containing media.\n",
                  getName(), media->getName(), aName, index+1);
            continue; // (skip)
        }

        // The partition base may be unaligned with respect the whole media's
        // block boundaries.  We warn the user in this event since every read
        // or write is going to cause deblocking.

        if ( (_absoluteBase + base) % media->getPreferredBlockSize() )
        {
            IOLog("%s on %s: Access to \"%s\" (partition %d) may be slow due "
                  "to a misaligned block boundary.\n",
                  getName(), media->getName(), aName, index + 1);
        }        

        // Create the new media object.

        IOMedia * newMedia = new IOMedia;

        if ( !newMedia ||
             !newMedia->init( // base (bytes; relative to provider media)
                              base - _absoluteBase,
                              // size (bytes)
                              size,
                              // natural block size (bytes)
                              fsBlockSize,
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

            if (newMedia)  newMedia->release();
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

bool IONeXTPartitionScheme::identify(IOMedia * media)
{
    //
    // Searches for the existence of a NeXT partition map on the given media.
    //

    UInt32 labelIncrement; // (in bytes)
    bool   success = false;

    // State our assumptions.

    assert(_buffer);
    assert(_bufferSize >= kMinimumBlockSize);

    // Open the media object for access.

    if ( media->open(this, 0, kAccessReader) == false )  return false;

    // Search through the four (NLABELS) possible label positions.
    //
    // In the spirit of minimizing reads, however, we cheat and only check the
    // first two of the four possible label positions.  Note that it is common
    // on i386 machines that the first label is non-existent, while the second
    // is valid, hence why we check the first TWO labels.

    labelIncrement=IORound(sizeof(disk_label_t),media->getPreferredBlockSize());

    for (unsigned index = 0; index < 2; index++)  // ("2" was "NLABELS")
    {
        // Read the appropriate block into our buffer.

        _buffer->setDirection(kIODirectionIn);   // (a read)
        _buffer->setLength(_bufferSize);         // (transfer one full block)

        if ( media->read( /* client    */ this,
                          /* byteStart */ labelIncrement * index,
                          /* buffer    */ _buffer ) == kIOReturnSuccess )
        {
            // Determine whether this buffer contains a valid NeXT label.  We
            // validate the version signature and the label's block position.

            disk_label_t * label = (disk_label_t *) _buffer->getBytesNoCopy();

            if ( OSSwapBigToHostInt32(label->dl_version) == DL_V3 ||
                 OSSwapBigToHostInt32(label->dl_version) == DL_V2 ||
                 OSSwapBigToHostInt32(label->dl_version) == DL_V1 )
            {
                if ( OSSwapBigToHostInt32(label->dl_label_blkno) ==
                     (UInt32) ( (_absoluteBase + labelIncrement * index) /
                                media->getPreferredBlockSize() ) )
                {
                    success = true; // (valid version and block position)
                    break;
                }
            }
        }
    }

    // Close the media object we opened earlier.

    media->close(this);

    return success;
}
