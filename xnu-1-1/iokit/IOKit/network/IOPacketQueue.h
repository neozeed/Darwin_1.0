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
 * Copyright (c) 1998 Apple Computer, Inc.  All rights reserved. 
 *
 * IOPacketQueue.h
 *
 * HISTORY
 * 9-Dec-1998       Joe Liu (jliu) created.
 *
 */

#ifndef _IOPACKETQUEUE_H
#define _IOPACKETQUEUE_H

#include <libkern/c++/OSObject.h>
#include <IOKit/IOLocks.h>

// Forward structure declarations.
//
struct mbuf;

// We do not want the enqueue/dequeue macros defined in queue.h.
//
// #warning queue.h should not be included
#undef enqueue(queue,elt)
#undef dequeue(queue)

/*! @class IOPacketQueue
    @abstract Implements a FIFO queue of mbuf packets.
    A spinlock allocated by this object is used to enforce mutual
    exclusion between methods with a "lock" prefix. */

class IOPacketQueue : public OSObject
{   
    OSDeclareDefaultStructors(IOPacketQueue)

private:
    struct mbuf *   _head;          // head of the mbuf queue
    struct mbuf *   _tail;          // tail of the mbuf queue
    UInt32          _size;          // size of the queue
    UInt32          _peakSize;      // peak size of the queue
    UInt32          _capacity;      // maximum size of the queue
    IOSimpleLock *  _lock;          // spinlock for locking methods

/*! @function free
    @abstract Free the IOPacketQueue instance.
    @discussion Release all packets in the queue to the free mbuf pool,
    deallocate the spinlock, then call super::free(). */

    virtual void free();

/*! @var IOPQDefaultCapacity Specifies the default capacity of the queue
    (number of packets that the queue can hold). Once the size of the queue
    grows to its capacity, the queue will begin to drop incoming packets.
    This capacity can be overridden by specifying the desired capacity when
    the queue is initialized, or the setCapacity() method can be called to 
    adjust the queue's capacity. */

    static const UInt32 IOPQDefaultCapacity = 100;

public:

/*! @function freePacketChain
    @abstract Free a packet chain.
    @discussion Traverse a packet chain linked through the mbuf
    nextpkt field, and free each packet.
    @param m The mbuf at the head of the mbuf chain.
    @result The number of mbufs freed. */

    static UInt32 freePacketChain(struct mbuf * m);

/*! @function withCapacity
    @abstract Factory method that will construct and initialize an
    IOPacketQueue instance.
    @param capacity The initial capacity of the queue. Can be changed
    later through setCapacity().
    @result An IOPacketQueue instance on success, or 0 otherwise. */

    static IOPacketQueue * withCapacity(UInt32 capacity = IOPQDefaultCapacity);

/*! @function initWithCapacity
    @abstract Initialize an IOPacketQueue instance.
    @discussion Initialize the queue and create a spinlock.
    @param capacity The initial capacity of the queue. Can be changed
    later through setCapacity().
    @result true if initialized successfully, false otherwise. */

    virtual bool initWithCapacity(UInt32 capacity = IOPQDefaultCapacity);

/*! @function getSize
    @abstract Get the size of the queue.
    @result The number of packets currently held in the queue. */

    UInt32 getSize() const;

/*! @function getPeakSize
    @abstract Returns the peak size of the queue.
    @discussion The queue contains a counter that records the peak
    size of the queue. This method returns the value in the counter,
    and can optionally reset the counter back to zero.
    @param reset Reset the peak size counter to zero if true.
    @result The peak size count. */

    UInt32 getPeakSize(bool reset = false);

/*! @function setCapacity
    @abstract Adjust the queue's capacity.
    @discussion If the capacity is set to a value that is smaller than its 
    current size, then the queue will drop incoming packets, but existing 
    packets in the queue will remain intact.
    @param capacity The new capacity.
    @result true if the new capacity was accepted, false otherwise. */

    bool setCapacity(UInt32 capacity);

/*! @function getCapacity
    @abstract Get the capacity of the queue.
    @result The current queue capacity. */

    UInt32 getCapacity() const;

/*! @function peek
    @discussion Peek at the head of the queue without dequeueing the packet.
    An ensuing call to peek() or dequeue() will return the same packet.
    The caller must not modify the packet returned.
    @result The packet at the head of the queue. */

    const struct mbuf * peek() const;

/*! @function prepend
    @abstract Add a packet (or a packet chain) to the head of the queue.
    @discussion Unlike enqueue(), the capacity is not checked, and the input
    packet is never dropped.
    @param m A single packet, or packet chain, to be added to the head of the
    queue. */

    void prepend(struct mbuf * m);

/*! @function lockPrepend
    @abstract Add a packet (or a packet chain) to the head of the queue.
    @discussion Same as prepend(). A spinlock lock is held while the queue
    is modified.
    @param m A single packet, or packet chain, to be added to the head of the
    queue. */

    void lockPrepend(struct mbuf * m);

/*! @function enqueue
    @abstract Add a packet (or a packet chain) to the tail of the queue.
    @param m A single packet, or a packet chain, to be added to the tail of the
    queue.
    @result The number of packets dropped from over-capacity. */

    UInt32 enqueue(struct mbuf * m);

/*! @function lockEnqueue
    @abstract Add a packet (or a packet chain) to the tail of the queue.
    @discussion Same as enqueue(). A spinlock lock is held while the queue
    is modified.
    @param m A single packet, or a packet chain, to be added to the tail of the
    queue.
    @result The number of packets dropped from over-capacity. */

    UInt32 lockEnqueue(struct mbuf * m);

/*! @function dequeue
    @abstract Remove a single packet from the head of the queue.
    @result The dequeued packet. A NULL is returned if the queue is empty. */

    struct mbuf * dequeue();

/*! @function lockDequeue
    @abstract Remove a single packet from the head of the queue.
    @discussion Same as dequeue(). A spinlock lock is held while the queue
    is modified.
    @result The dequeued packet. A NULL is returned if the queue is empty. */

    struct mbuf * lockDequeue();

/*! @function dequeueAll
    @abstract Removes all packets from the queue and return the first packet
    of the packet chain.
    @result The head of the dequeued packet chain. */

    struct mbuf * dequeueAll();

/*! @function lockDequeueAll
    @abstract Removes all packets from the queue and return the first packet
    of the packet chain.
    @discussion Same as dequeueAll(). A spinlock lock is held while the queue
    is modified.
    @result The head of the dequeued packet chain. */

    struct mbuf * lockDequeueAll();

/*! @function flush
    @abstract Removes all packets from the queue and release them to the
    free mbuf pool.
    @discussion The size of the queue will be zero after the call.
    @result The number of packets freed. */

    UInt32 flush();

/*! @function lockFlush
    @abstract Removes all packets from the queue and release them to the
    free mbuf pool.
    @discussion Same as flush(). A spinlock lock is held while the queue
    is modified.
    @result The number of packets freed. */

    UInt32 lockFlush();
};

#endif /* !_IOPACKETQUEUE_H */
