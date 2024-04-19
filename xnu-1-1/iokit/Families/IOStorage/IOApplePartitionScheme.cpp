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
// o the on-disk dpme structure is packed for PowerPC and has big endian fields
//
// o the dpme_pblock_start block value is relative to the containing media's
//   boundary, and the implicit block size is hardcoded to 512 bytes
//
// o the dpme_pblocks block value's implicit block size is 512 bytes
//
// Policies:
// --------
// o the apple partition scheme accepts probes only on "whole" media objects --
//   this is done in the spirit of minimizing reads, since in general practice,
//   subpartitions of the apple type do not exist
//
// o the default probe score of the apple partition scheme is 1200
//
// o the "Apple_partition_map" partition is always published as a non-writable
//   media -- this is because its contents are nobody's business but ours
//
// o the "Apple_Free" partition is always skipped -- this is because it would
//   never be used by a serious user application, and it cries out to harbour
//   viruses;  the partition also often exceeds the confines of the media for
//   CDs, which would arguably require ugly truncation logic to be consistent
//
// o the checksum for each partition entry is not validated -- this is done in
//   the spirit of non-necessity and perhaps a bit of laziness
//

#include <IOKit/assert.h>
#include <IOKit/IOLib.h>
#include <IOKit/storage/IOApplePartitionScheme.h>
#include <libkern/OSByteOrder.h>

#define super IOPartitionScheme
OSDefineMetaClassAndStructors(IOApplePartitionScheme, IOPartitionScheme);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

bool IOApplePartitionScheme::init(OSDictionary * properties = 0)
{
    //
    // Initialize this object's minimal state.
    //

    if (super::init(properties) == false)  return false;

    _buffer        = 0;
    _bufferSize    = 0;
    _masteredAt512 = false;

    // Validate the compiled size of our important fixed-size structures.

    assert(sizeof(dpme)   == 512);
    assert(sizeof(DDMap)  ==   8);
    assert(sizeof(Block0) == 512);

    return true;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void IOApplePartitionScheme::free()
{
    //
    // Free all of this object's outstanding resources.
    //

    if (_buffer)  _buffer->release();

    super::free();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

IOService * IOApplePartitionScheme::probe(IOService * provider, SInt32 * score)
{
    //
    // Determine whether the provider media contains an apple partition map.  If
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

    if (media->getPreferredBlockSize() < sizeof(dpme))  return 0;

    // Determine whether we can rule out the media object as an apple partition
    // map container without reading actual data from the media.    We rule out
    // all non-whole media objects.

    if (media->isWhole() == false)  return 0;

    // Allocate a buffer large enough to hold one media block.

    _bufferSize = media->getPreferredBlockSize();
    _buffer     = IOBufferMemoryDescriptor::withCapacity(_bufferSize,
                                                         kIODirectionIn);

    if (_buffer == 0)  return 0;

    // Search for a valid apple partition on the provider media.

    return identify(media) ? this : 0;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

bool IOApplePartitionScheme::start(IOService * provider)
{
    //
    // This method is called once we have been attached to the media object.  We
    // generate the new media objects that will represent our partitions here.
    //

    UInt64    block;    
    UInt32    entriesCount;
    IOMedia * media = (IOMedia *) provider;
    dpme *    partition;
    UInt32    partitionsPerBlock;
    UInt32    partitionsLeftOnThisBlock;
    bool      success = false;

    // State our assumptions.

    assert(_buffer);

    // Ask our superclass' opinion.

    if ( !super::start(provider) )  return false;

    // Open the media object for access.

    if ( media->open(this, 0, kAccessReader) == false )  return false;

    // Set up our iteration variables, based on whether the media was mastered
    // on a 512-byte-block medium or at the media's preferred block size.

    if ( _masteredAt512 )
    {
        block = 0;
        partition = (dpme*)((UInt8*)_buffer->getBytesNoCopy() + sizeof(Block0));
        partitionsPerBlock = _bufferSize / sizeof(dpme);
        partitionsLeftOnThisBlock = partitionsPerBlock - 1;
    }
    else
    {
        block = 1;
        partition = (dpme *) _buffer->getBytesNoCopy();
        partitionsPerBlock = 1;
        partitionsLeftOnThisBlock = 1;
    }

    // Scan through all the partition entries.

    entriesCount = OSSwapBigToHostInt32(partition->dpme_map_entries);

    for ( unsigned index = 0; index < entriesCount; index++, partition += 1 )
    {
        // Determine whether we've exhausted the current buffer of partitions.

        if ( partitionsLeftOnThisBlock == 0 )
        {
            // Read the next block into our buffer.

            block++;
            _buffer->setDirection(kIODirectionIn); // (a read)
            _buffer->setLength(_bufferSize);       // (transfer one full block)

            if ( media->read( /* client    */ this,
                              /* byteStart */ block * _bufferSize,
                              /* buffer    */ _buffer ) != kIOReturnSuccess )
            {
                IOLog("%s: %s: Unable to read block %d.\n", 
                      getName(), media->getName(), (int) block);
                break; // (failure)
            }

            partition = (dpme *) _buffer->getBytesNoCopy();
            partitionsLeftOnThisBlock = partitionsPerBlock;
        }

        partitionsLeftOnThisBlock--;

        // Determine whether this partition has a valid 'PM' signature.

        if ( OSSwapBigToHostInt16(partition->dpme_signature) != DPME_SIGNATURE )
        {
            IOLog("%s on %s: Partition %d has an invalid signature.\n",
                  getName(), media->getName(), index + 1);
            continue; // (skip)
        }

        // Determine whether this partition's type is 'Apple_Free'.

        if ( !strcmp(partition->dpme_type, "Apple_Free") )
            continue; // (skip)

        // Determine whether this partition's type is 'Apple_partition_map'.

        bool isWritable = media->isWritable();

        if ( !strcmp(partition->dpme_type, "Apple_partition_map") )
            isWritable = false;

        // Compute the relative position and size of the new partition.  The
        // block values are always in terms of 512-byte blocks.

        UInt64 base = OSSwapBigToHostInt32(partition->dpme_pblock_start)*512ULL;
        UInt64 size = OSSwapBigToHostInt32(partition->dpme_pblocks) * 512ULL;

        if ( base == 0 || size == 0 )
            continue; // (skip)

        // Look up a name and a type for this partition.

        const char * aName = partition->dpme_name;
        const char * aHint = partition->dpme_type;

        // Ensure the partition definition does not leave the confines of the
        // containing media.  Note the "Apple_Free" partition often leaves its
        // confines on CD media, however it is our policy to ignore Apple_Free
        // partitions anyway.

        if ( base + size > media->getSize() )
        {
            IOLog("%s on %s: \"%s\" (partition %d) exceeds confines of "
                  "containing media.\n",
                  getName(), media->getName(), aName, index+1);
            continue; // (skip)
        }

        // The partition base may be unaligned with respect the parent media's
        // block boundaries.   We warn the user in this event since every read
        // or write is going to cause deblocking.  We presume, of course, that
        // the parent media's base is aligned in making this determination.

        if ( base % media->getPreferredBlockSize() )
        {
            IOLog("%s on %s: Access to \"%s\" (partition %d) may be slow due "
                  "to a misaligned block boundary.\n",
                  getName(), media->getName(), aName, index + 1);
        }        

        // Create the new media object.

        IOMedia * newMedia = new IOMedia;

        if ( !newMedia ||
             !newMedia->init( // base (bytes; relative to provider media)
                              base,
                              // size (bytes)
                              size,
                              // natural block size (bytes)
///m:2361246:workaround:added:start
                              (!strcmp(aHint, "Apple_HFS")) ? (512) : 
///m:2361246:workaround:added:stop
                              media->getPreferredBlockSize(),
                              // is ejectable
                              media->isEjectable(),
                              // is whole
                              false,
                              // is writable
                              isWritable,
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

        // Release our retain on the new media object (in registry).

        newMedia->release();

        // We don't register the new media object with the matching system
        // until we've closed our open on the media object below us.

        success = true;
    } // (for all partition entries)

    // Release our buffer now that we no longer need it.

    _buffer->release();

    _buffer     = 0;
    _bufferSize = 0;

    // Close the media object we opened earlier.

    media->close(this);

    // We now register the new media objects with the matching system, now
    // that we've closed our open on the media object below us.

    if ( success )
    {
        OSIterator * clients = getClientIterator();

        if (clients)
        {
            IOService * client;
            while ( (client = (IOService *) clients->getNextObject()) )
                client->registerService();
            clients->release();
        }
    }

    // Return success if we found at least one partition.

    return success;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

bool IOApplePartitionScheme::identify(IOMedia * media)
{
    //
    // Searches for the existence of an apple partition map on the given media.
    //

    bool success = false;

    // State our assumptions.

    assert(_buffer);

    // Open the media object for access.

    if ( media->open(this, 0, kAccessReader) == false )  return false;

    // Determine whether this is a larger-than-512-byte-block media that was
    // mastered from a 512-byte-block media  --  the mastering of 2048-byte-
    // block CDs is the prime example, from a 512-byte-block hard drive.

    if ( _bufferSize > sizeof(Block0) )
    {
        // State our assumptions.

        assert(_bufferSize >= sizeof(Block0) + sizeof(dpme));

        // Read the first block into our buffer.

        _buffer->setDirection(kIODirectionIn);   // (a read)
        _buffer->setLength(_bufferSize);         // (transfer one full block)

        if ( media->read(this, 0, _buffer) != kIOReturnSuccess )
        {
            media->close(this);
            return false; // (failure)
        }

        // Determine whether the partition signature 'PM' is present at a 512
        // byte offset into the block.

        dpme * partition;
        partition = (dpme*)((UInt8*)_buffer->getBytesNoCopy() + sizeof(Block0));

        if ( OSSwapBigToHostInt16(partition->dpme_signature) == DPME_SIGNATURE )
        {
            _masteredAt512 = true;
            success        = true;
        }
    }

    // Determine whether there is a valid apple partition map on the media;
    // note if the masteredAt512 flag is set, we already confirmed it does.

    if ( _masteredAt512 == false )
    {
        // Read the second block into our buffer.

        _buffer->setDirection(kIODirectionIn);   // (a read)
        _buffer->setLength(_bufferSize);         // (transfer one full block)

        if ( media->read(this, _bufferSize, _buffer) == kIOReturnSuccess )
        {
            // Determine whether the partition map signature 'PM' is present.

            dpme * partition = (dpme *) _buffer->getBytesNoCopy();

            if (OSSwapBigToHostInt16(partition->dpme_signature)==DPME_SIGNATURE)
                success = true;
        }
    }

    // Close the media object we opened earlier.

    media->close(this);

    return success;
}
