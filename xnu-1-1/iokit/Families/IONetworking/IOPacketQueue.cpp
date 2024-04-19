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
 * IOPacketQueue.cpp - Implements a FIFO mbuf packet queue.
 *
 * HISTORY
 * 9-Dec-1998       Joe Liu (jliu) created.
 *
 */

#include <IOKit/assert.h>
#include <IOKit/IOLib.h>    // IOLog
#include <IOKit/network/IOPacketQueue.h>

// Get the mbuf definitions from BSD headers.
//
extern "C" {
#include <sys/param.h>
#include <sys/mbuf.h>
}

#define super OSObject
OSDefineMetaClassAndStructors( IOPacketQueue, OSObject )

//---------------------------------------------------------------------------
// Lock macros.

#define LOCK         IOSimpleLockLock(_lock)
#define UNLOCK       IOSimpleLockUnlock(_lock)

//---------------------------------------------------------------------------
// Factory method that will construct and initialize an IOPacketQueue instance.
//
// capacity: The initial capacity of the queue. Can be changed
//           later through setCapacity().
//
// Returns an IOPacketQueue instance upon success, or 0 otherwise.

bool IOPacketQueue::initWithCapacity(UInt32 capacity)
{
    _head = _tail = (struct mbuf *) 0;
    _size = _peakSize = 0;
    _capacity = capacity;

    if (!super::init()) {
        IOLog("IOPacketQueue: super::init() failed\n");
        return false;
    }

    if ((_lock = IOSimpleLockAlloc()) == 0)
        return false;

    IOSimpleLockInit(_lock);

    return true;
}

//---------------------------------------------------------------------------
// Factory method that will construct and initialize an IOPacketQueue instance.
//
// capacity: The initial capacity of the queue. Can be changed later
//           through setCapacity().
//
// Returns an IOPacketQueue instance on success, or 0 otherwise.

IOPacketQueue * IOPacketQueue::withCapacity(UInt32 capacity)
{
    IOPacketQueue * queue = new IOPacketQueue;

    if (queue && !queue->initWithCapacity(capacity)) {
        queue->release();
        return 0;
    }
    return queue;
}

//---------------------------------------------------------------------------
// Frees the IOPacketQueue instance. Release all packets in the queue to the
// free mbuf pool, then call super::free().

void IOPacketQueue::free()
{
    if (_lock) {
        setCapacity(0);
        lockFlush();
        IOSimpleLockFree(_lock);
    }
    super::free();
}

//---------------------------------------------------------------------------
// Get the size of the queue.
// Returns the number of packets currently held in the queue.

UInt32 IOPacketQueue::getSize() const
{
    return (_size);
}

//---------------------------------------------------------------------------
// The queue contains a counter that records the peak size of the queue.
// This method returns the value in the counter, and can optionally reset
// the counter back to zero.
//
// reset: Reset the peak size counter to zero if true.
//
// Returns the peak size count.

UInt32 IOPacketQueue::getPeakSize(bool reset = false)
{
    UInt32 peak = _peakSize;
    if (reset)
        _peakSize = 0;
    return peak;
}

//---------------------------------------------------------------------------
// Adjust the queue's capacity. If the capacity is set to a value that
// is smaller than its current size, then the queue will drop incoming packets,
// but existing packets in the queue will remain intact.
//
// capacity: The new capacity.
//
// Returns true if the new capacity was accepted, false otherwise.

bool IOPacketQueue::setCapacity(UInt32 capacity)
{
    LOCK;
    _capacity = capacity;
    UNLOCK;
    return true;
}

//---------------------------------------------------------------------------
// Get the capacity of the queue.
// Returns the current queue capacity.

UInt32 IOPacketQueue::getCapacity() const
{
    return _capacity;
}

//---------------------------------------------------------------------------
// Peek at the head of the queue without dequeueing the packet.
// An ensuing call to peek() or dequeue() will return the same packet.
// Returns the packet at the head of the queue. The caller must not modify
// the packet returned.
// Return the packet at the head of the queue.

const struct mbuf * IOPacketQueue::peek() const
{
    return (_head);
}

//---------------------------------------------------------------------------
// Queueing macros.

static inline UInt32 _freePacketChain(struct mbuf * m)
{
    UInt32 count = 0;
    struct mbuf * ml;

    while ((ml = m)) {
        m = ml->m_nextpkt;
        ml->m_nextpkt = 0;
        m_freem(ml);
        count++;
    }

    return count;
}

static inline void _prepend(
    struct mbuf * m,        /* not modified */
    struct mbuf * & head,   /* modified */
    struct mbuf * & tail,   /* modified */
    UInt32 & size)          /* modified */
{
    assert(m);

    if (size++ > 0) {
        struct mbuf * mtail = m;
        while (mtail->m_nextpkt) {
            mtail = mtail->m_nextpkt;
            size++;
        }

        assert(head);
        mtail->m_nextpkt = head;
        head = m;
    }
    else {
        assert(!head && !tail);
        head = tail = m;
        while (tail->m_nextpkt) {
            tail = tail->m_nextpkt;
            size++;
        }
    }
} 

static inline struct mbuf * _enqueue(
    struct mbuf * m,        /* mbuf chain to append to tail */
    struct mbuf * & head,   /* modified */
    struct mbuf * & tail,   /* modified */
    UInt32 capacity,        /* not modified */
    UInt32 & size,          /* modified */
    UInt32 & peakSize)      /* modified */
{
    assert(m);

    if (size >= capacity)
        goto overflow;

    if (size++ > 0) {
        assert(tail);
        tail->m_nextpkt = m;
        tail = m;
    }
    else {
        assert(!head && !tail);
        head = tail = m;
    }
    
    m = 0;

    while (tail->m_nextpkt) {
        if (size >= capacity) {
            m = tail->m_nextpkt;
            tail->m_nextpkt = 0;
            goto overflow;
        }

        tail = tail->m_nextpkt;
        size++;
    }

    if (size > peakSize)
        peakSize = size;

overflow:
    return m;
}

static inline struct mbuf * _dequeue(
    struct mbuf * & head,   /* modified */
    struct mbuf * & tail,   /* modified */
    UInt32 & size)          /* modified */
{
    if (size == 0)
        return 0;

    assert(head);

    struct mbuf *m = head;
    head = head->m_nextpkt;
    m->m_nextpkt = 0;
    if (--size == 0)
        head = tail = 0;

    return m;
}

static inline struct mbuf * _dequeueAll(
    struct mbuf * & head,   /* modified */
    struct mbuf * & tail,   /* modified */
    UInt32 & size)          /* modified */
{
    if (size == 0)
        return 0;
    
    assert(head);

    struct mbuf *m = head;
    head = tail = 0;
    size = 0;

    return m;
}

//---------------------------------------------------------------------------
// Traverse a packet chain linked through the mbuf nextpkt field, and free
// each packet.
//
// m: The mbuf at the head of the mbuf chain.
//
// Returns the number of mbufs freed.

UInt32 IOPacketQueue::freePacketChain(struct mbuf * m)
{
    return _freePacketChain(m);
}

//---------------------------------------------------------------------------
// Add a packet (or a packet chain) to the head of the queue. Unlike enqueue(),
// the capacity is not checked, and the input packet is never dropped.
//
// m: A single packet, or packet chain, to be added to the head of the queue.

void IOPacketQueue::prepend(struct mbuf * m)
{
    _prepend(m, _head, _tail, _size);
}

//---------------------------------------------------------------------------
// Add a packet (or a packet chain) to the tail of the queue.
//
// m: A single packet, or a packet chain, to be added to the tail of the queue.

UInt32 IOPacketQueue::enqueue(struct mbuf * m)
{
    m = _enqueue(m, _head, _tail, _capacity, _size, _peakSize);
    return (m ? _freePacketChain(m) : 0);
}

//---------------------------------------------------------------------------
// Remove a single packet from the head of the queue.
//
// Returns the dequeued packet. A NULL is returned if the queue is empty.

struct mbuf * IOPacketQueue::dequeue()
{   
    return _dequeue(_head, _tail, _size);
}

//---------------------------------------------------------------------------
// Removes all packets from the queue and return the first packet of the
// packet chain.
//
// Returns the head of the dequeued packet chain.

struct mbuf * IOPacketQueue::dequeueAll()
{
    return _dequeueAll(_head, _tail, _size);
}

//---------------------------------------------------------------------------
// Removes all packets from the queue and put them back to the free mbuf
// pool. The size of the queue will be zero after the call.
//
// Returns the number of packets freed.

UInt32 IOPacketQueue::flush()
{
    return (freePacketChain(dequeueAll()));
}

//---------------------------------------------------------------------------
// Locked forms of prepend/enqueue/dequeue/dequeueAll methods.
// A spinlock is used to enforce mutual exclusion between methods that
// with a "lock" prefix.

void IOPacketQueue::lockPrepend(struct mbuf * m)
{
    LOCK;
    _prepend(m, _head, _tail, _size);
    UNLOCK;  
}

UInt32 IOPacketQueue::lockEnqueue(struct mbuf * m)
{
    LOCK;
    m = _enqueue(m, _head, _tail, _capacity, _size, _peakSize);
    UNLOCK;
    return (m ? _freePacketChain(m) : 0);
}

struct mbuf * IOPacketQueue::lockDequeue()
{
    struct mbuf * m;
    LOCK;
    m = _dequeue(_head, _tail, _size);
    UNLOCK;
    return m;
}

struct mbuf * IOPacketQueue::lockDequeueAll()
{
    struct mbuf * m;
    LOCK;
    m = _dequeueAll(_head, _tail, _size);
    UNLOCK;
    return m;
}

UInt32 IOPacketQueue::lockFlush()
{
    return (freePacketChain(lockDequeueAll()));
}
