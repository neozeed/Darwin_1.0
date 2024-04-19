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
/*
 * Copyright (c) 1999 Apple Computer, Inc.  All rights reserved. 
 *
 * IOMBufMemoryCursor.h created by gvdl on 1999-1-20
 *
 */

#ifndef _IOKIT_NETWORK_IOMBUFMEMORYCURSOR_H
#define _IOKIT_NETWORK_IOMBUFMEMORYCURSOR_H

#include <IOKit/IOMemoryCursor.h>

struct mbuf;

/*! @class IOMBufMemoryCursor : public IOMemoryCursor
    @abstract A mechanism to convert mbuf chains to physical addresses.
    @discussion The IOMBufMemoryCursor defines the super class that all
    specific mbuf cursors must inherit from, but a mbuf cursor can be created   
    without a specific formal subclass by just providing a segment function to
    the initializers. This class performs the task of walking a given
    mbuf chain and creating a physical scatter/gather list appropriate for
    the target hardware. When necessary, this class may also coalesce
    mbuf chains when the generated scatter/gather list exceeds the specified 
    hardware limit. However, this should be avoided since it exacts a 
    performance cost.
    <br><br>
    A driver is expected to create a mbuf cursor and configure it to match the
    limitations of it's DMA hardware; for instance the mbuf cursor used by
    an Ethernet controller driver may have a maximum physical segment size
    of 1520, and allow for up to 6 physical segments. Thus it would create a 
    mbuf cursor with a maxSegmentSize of 1520 and a maxNumSegments of 6.
    The driver may choose to supply an OutputSegmentFunc function to
    format the output of each scatter/gather segment to match the
    hardware descriptor format, or it may use a subclass of 
    IOMBufMemoryCursor to generate IOPhysicalSegment segments with 
    various byte orders.
    <br><br>
    A driver may also create more than one mbuf cursor, perhaps one
    dedicated for the transmit thread, and the other for the receive thread.
    This becomes a requirement when the driver is multi-threaded, since the 
    mbuf cursor maintains state and does not support reentrancy. */

class IOMBufMemoryCursor : public IOMemoryCursor
{
    OSDeclareAbstractStructors(IOMBufMemoryCursor)

private:
    virtual bool initWithSpecification(OutputSegmentFunc outSeg,
                                       UInt maxSegmentSize,
                                       UInt maxTransferSize,
                                       UInt align);

protected:
    UInt maxNumSegments;
    UInt coalesceCount;
    UInt packetTooBigErrors;

/*! @function initWithSpecification
    @abstract Primary initializer for the IOMBufMemoryCursor class.
    @param outSeg Function to call to output one physical segment.
    @param maxSegmentSize Maximum allowable size for one segment.
    @param maxNumSegments Maximum number of segments.
    @result true if the inherited classes and this instance initialized
    successfully. */

    virtual bool initWithSpecification(OutputSegmentFunc outSeg,
                                       UInt maxSegmentSize,
                                       UInt maxNumSegments);

/*! @function genPhysicalSegments
    @abstract Generate a physical scatter/gather list given a mbuf packet.
    @discussion Generates a list of physical segments from the given mbuf.
    @param packet The mbuf packet.
    @param vector Void pointer to base of output physical scatter/gather list.
    Always passed directly onto the OutputSegmentFunc without interpretation
    by the cursor. 
    @param maxSegs Maximum number of segments that can be written to segments
    array.
    @param doCoalesce Set to true to perform coalescing when the required 
    number of segments exceeds the specified limit, otherwise abort and
    return 0.
    @result The number of segments that were filled in is returned, or
    0 if an error occurred. */

    virtual UInt genPhysicalSegments(struct mbuf * packet, void * vector,
                                     UInt maxSegs, bool doCoalesce);

public:

/*! @function genPhysicalSegments
    @abstract Returns a count of the total number of mbuf chains coalesced
    by genPhysicalSegments(). The counter is then reset to 0.
    @result The coalesce count. */

    UInt getAndResetCoalesceCount();
};


/*! @class IOMBufNaturalMemoryCursor : public IOMBufMemoryCursor
    @abstract A IOMBufMemoryCursor subclass that outputs a vector of
    IOPhysicalSegments in the natural byte orientation for the cpu.  
    @discussion The IOMBufNaturalMemoryCursor would be used when it is too
    difficult to implement an OutputSegmentFunc that is more appropriate for
    your hardware.  This cursor just outputs an array of IOPhysicalSegments. */

class IOMBufNaturalMemoryCursor : public IOMBufMemoryCursor
{
    OSDeclareDefaultStructors(IOMBufNaturalMemoryCursor)

public:

/*! @function withSpecification
    @abstract Factory function to create and initialize an 
    IOMBufNaturalMemoryCursor in one operation, see
    IOMBufMemoryCursor::initWithSpecification.
    @param maxSegmentSize Maximum allowable size for one segment.
    @param maxNumSegments Maximum number of segments.
    @result A new mbuf cursor if successfully created and initialized,
    0 otherwise. */

    static IOMBufNaturalMemoryCursor * withSpecification(UInt maxSegmentSize, 
                                                         UInt maxNumSegments);

/*! @function getPhysicalSegments
    @abstract Generate a cpu natural physical scatter/gather list from a given
    mbuf.
    @param packet The mbuf packet.
    @param vector Pointer to an array of IOPhysicalSegments for the output 
    physical scatter/gather list.
    @param numVectorSegments Maximum number of IOPhysicalSegments accepted.
    @result The number of segments that were filled in is returned, or
    0 if an error occurred. */

    UInt getPhysicalSegments(struct mbuf * packet,
                             struct IOPhysicalSegment * vector,
                             UInt numVectorSegments = 0);

/*! @function getPhysicalSegmentsWithCoalesce
    @abstract Generate a cpu natural physical scatter/gather list from a given
    mbuf.
    @discussion Generate a cpu natural physical scatter/gather list from a 
    given mbuf. Coalesce mbuf chain when the number of segments in the 
    scatter/gather list exceeds numVectorSegments.
    @param packet The mbuf packet.
    @param vector Pointer to an array of IOPhysicalSegments for the output 
    physical scatter/gather list.
    @param numVectorSegments Maximum number of IOPhysicalSegments accepted.
    @result The number of segments that were filled in is returned, or
    0 if an error occurred. */

    UInt getPhysicalSegmentsWithCoalesce(struct mbuf * packet,
                                         struct IOPhysicalSegment * vector,
                                         UInt numVectorSegments = 0);
};

//===========================================================================
//===========================================================================

/*! @class IOMBufBigMemoryCursor : public IOMBufMemoryCursor
    @abstract A IOMBufMemoryCursor subclass that outputs a vector of 
    IOPhysicalSegments in the big endian byte order.  
    @discussion The IOMBufBigMemoryCursor would be used when the DMA hardware 
    requires a big endian address and length pair.  This cursor outputs an 
    array of IOPhysicalSegments that are encoded in big-endian format. */

class IOMBufBigMemoryCursor : public IOMBufMemoryCursor
{
    OSDeclareDefaultStructors(IOMBufBigMemoryCursor)

public:

/*! @function withSpecification
    @abstract Factory function to create and initialize an 
    IOMBufBigMemoryCursor in one operation, see
    IOMBufMemoryCursor::initWithSpecification.
    @param maxSegmentSize Maximum allowable size for one segment.
    @param maxNumSegments Maximum number of segments.
    @result A new mbuf cursor if successfully created and initialized,
    0 otherwise. */

    static IOMBufBigMemoryCursor * withSpecification(UInt maxSegmentSize,
                                                     UInt maxNumSegments);

/*! @function getPhysicalSegments
    @abstract Generate a big endian physical scatter/gather list from a given
    mbuf.
    @param packet The mbuf packet.
    @param vector Pointer to an array of IOPhysicalSegments for the output 
    physical scatter/gather list.
    @param numVectorSegments Maximum number of IOPhysicalSegments accepted.
    @result The number of segments that were filled in is returned, or
    0 if an error occurred. */

    UInt getPhysicalSegments(struct mbuf * packet,
                             struct IOPhysicalSegment * vector,
                             UInt numVectorSegments = 0);

/*! @function getPhysicalSegmentsWithCoalesce
    @abstract Generate a big endian physical scatter/gather list from a given
    mbuf.
    @discussion Generate a big endian physical scatter/gather list from a 
    given mbuf. Coalesce mbuf chain when the number of segments in the 
    scatter/gather list exceeds numVectorSegments.
    @param packet The mbuf packet.
    @param vector Pointer to an array of IOPhysicalSegments for the output 
    physical scatter/gather list.
    @param numVectorSegments Maximum number of IOPhysicalSegments accepted.
    @result The number of segments that were filled in is returned, or
    0 if an error occurred. */

    UInt getPhysicalSegmentsWithCoalesce(struct mbuf * packet,
                                         struct IOPhysicalSegment * vector,
                                         UInt numVectorSegments = 0);
};

//===========================================================================
//===========================================================================

/*! @class IOMBufLittleMemoryCursor : public IOMBufMemoryCursor
    @abstract A IOMBufMemoryCursor subclass that outputs a vector of 
    IOPhysicalSegments in the little endian byte order.  
    @discussion The IOMBufLittleMemoryCursor would be used when the DMA 
    hardware requires a little endian address and length pair.  This cursor 
    outputs an array of IOPhysicalSegments that are encoded in little endian 
    format. */

class IOMBufLittleMemoryCursor : public IOMBufMemoryCursor
{
    OSDeclareDefaultStructors(IOMBufLittleMemoryCursor)

public:

/*! @function withSpecification
    @abstract Factory function to create and initialize an 
    IOMBufLittleMemoryCursor in one operation, see
    IOMBufMemoryCursor::initWithSpecification.
    @param maxSegmentSize Maximum allowable size for one segment.
    @param maxNumSegments Maximum number of segments.
    @result A new mbuf cursor if successfully created and initialized,
    0 otherwise. */

    static IOMBufLittleMemoryCursor * withSpecification(UInt maxSegmentSize, 
                                                        UInt maxNumSegments);

/*! @function getPhysicalSegments
    @abstract Generate a little endian physical scatter/gather list from a 
    given mbuf.
    @param packet The mbuf packet.
    @param vector Pointer to an array of IOPhysicalSegments for the output 
    physical scatter/gather list.
    @param numVectorSegments Maximum number of IOPhysicalSegments accepted.
    @result The number of segments that were filled in is returned, or
    0 if an error occurred. */

    UInt getPhysicalSegments(struct mbuf * packet,
                             struct IOPhysicalSegment * vector,
                             UInt numVectorSegments = 0);

/*! @function getPhysicalSegmentsWithCoalesce
    @abstract Generate a little endian physical scatter/gather list from a 
    given mbuf.
    @discussion Generate a little endian physical scatter/gather list from a 
    given mbuf. Coalesce mbuf chain when the number of segments in the 
    scatter/gather list exceeds numVectorSegments.
    @param packet The mbuf packet.
    @param vector Pointer to an array of IOPhysicalSegments for the output 
    physical scatter/gather list.
    @param numVectorSegments Maximum number of IOPhysicalSegments accepted.
    @result The number of segments that were filled in is returned, or
    0 if an error occurred. */

    UInt getPhysicalSegmentsWithCoalesce(struct mbuf * packet,
                                         struct IOPhysicalSegment * vector,
                                         UInt numVectorSegments = 0);
};

#ifdef __ppc__

struct IODBDMADescriptor;

//===========================================================================
//===========================================================================

/*! @class IOMBufDBDMAMemoryCursor : public IOMBufMemoryCursor
    @abstract A IOMBufMemoryCursor subclass that outputs a vector of 
    IODBDMADescriptors. */

class IOMBufDBDMAMemoryCursor : public IOMBufMemoryCursor
{
    OSDeclareDefaultStructors(IOMBufDBDMAMemoryCursor)

public:

/*! @function withSpecification
    @abstract Factory function to create and initialize an 
    IOMBufDBDMAMemoryCursor in one operation, see
    IOMBufMemoryCursor::initWithSpecification.
    @param maxSegmentSize Maximum allowable size for one segment.
    @param maxNumSegments Maximum number of segments.
    @result A new mbuf cursor if successfully created and initialized,
    0 otherwise. */

    static IOMBufDBDMAMemoryCursor * withSpecification(UInt maxSegmentSize, 
                                                       UInt maxNumSegments);

/*! @function getPhysicalSegments
    @abstract Generate a DBDMA descriptor list from a given mbuf.
    @param packet The mbuf packet.
    @param vector Pointer to an array of IODBDMADescriptor for the output list.
    @param numVectorSegments Maximum number of IODBDMADescriptors accepted.
    @result The number of segments that were filled in is returned, or
    0 if an error occurred. */

    UInt getPhysicalSegments(struct mbuf * packet,
                             struct IODBDMADescriptor *vector,
                             UInt numVectorSegments = 0);

/*! @function getPhysicalSegmentsWithCoalesce
    @abstract Generate a DBDMA descriptor list from a given mbuf.
    @discussion Generate a DBDMA descriptor list from a given mbuf.
    Coalesce mbuf chain when the number of elements in the list exceeds
    numVectorSegments.
    @param packet The mbuf packet.
    @param vector Pointer to an array of IODBDMADescriptor for the output list.
    @param numVectorSegments Maximum number of IODBDMADescriptors accepted.
    @result The number of segments that were filled in is returned, or
    0 if an error occurred. */

    UInt getPhysicalSegmentsWithCoalesce(struct mbuf * packet,
                                         struct IODBDMADescriptor * vector,
                                         UInt numVectorSegments = 0);
};

#endif /* __ppc__ */

inline UInt IOMBufMemoryCursor::getAndResetCoalesceCount()
{
    UInt cnt = coalesceCount; coalesceCount = 0; return cnt;
}

inline UInt
IOMBufNaturalMemoryCursor::getPhysicalSegments(struct mbuf *packet,
                       struct IOPhysicalSegment *vector,
                       UInt numVectorSegments = 0)
{
    return genPhysicalSegments(packet, vector, numVectorSegments, false);
}

inline UInt
IOMBufNaturalMemoryCursor::getPhysicalSegmentsWithCoalesce(struct mbuf *packet,
                                   struct IOPhysicalSegment *vector,
                                   UInt numVectorSegments = 0)
{
    return genPhysicalSegments(packet, vector, numVectorSegments, true);
}

inline UInt
IOMBufBigMemoryCursor::getPhysicalSegments(struct mbuf *packet,
                       struct IOPhysicalSegment *vector,
                       UInt numVectorSegments = 0)
{
    return genPhysicalSegments(packet, vector, numVectorSegments, false);
}

inline UInt
IOMBufBigMemoryCursor::getPhysicalSegmentsWithCoalesce(struct mbuf *packet,
                                   struct IOPhysicalSegment *vector,
                                   UInt numVectorSegments = 0)
{
    return genPhysicalSegments(packet, vector, numVectorSegments, true);
}

inline UInt
IOMBufLittleMemoryCursor::getPhysicalSegments(struct mbuf *packet,
                       struct IOPhysicalSegment *vector,
                       UInt numVectorSegments = 0)
{
    return genPhysicalSegments(packet, vector, numVectorSegments, false);
}

inline UInt
IOMBufLittleMemoryCursor::getPhysicalSegmentsWithCoalesce(struct mbuf *packet,
                                   struct IOPhysicalSegment *vector,
                                   UInt numVectorSegments = 0)
{
    return genPhysicalSegments(packet, vector, numVectorSegments, true);
}

#ifdef __ppc__
inline UInt
IOMBufDBDMAMemoryCursor::getPhysicalSegments(struct mbuf *packet,
                                   struct IODBDMADescriptor *vector,
                                   UInt numVectorSegments = 0)
{
    return genPhysicalSegments(packet, vector, numVectorSegments, false);
}

inline UInt
IOMBufDBDMAMemoryCursor::getPhysicalSegmentsWithCoalesce(struct mbuf *packet,
                                   struct IODBDMADescriptor *vector,
                                   UInt numVectorSegments = 0)
{
    return genPhysicalSegments(packet, vector, numVectorSegments, true);
}
#endif /* __ppc__ */

#endif /* !_IOKIT_NETWORK_IOMBUFMEMORYCURSOR_H */

