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
 * IOOQLockFIFOQueue.h
 * 
 * HISTORY
 *
 */

#ifndef _IOOQLOCKFIFOQUEUE_H
#define _IOOQLOCKFIFOQUEUE_H

/*
 * Aliases.
 */
#define IOLockedFIFOQueue IOOQLockFIFOQueue

#include <IOKit/IOLocks.h>
#include <IOKit/network/IOOutputQueue.h>

/*! @class IOOQLockFIFOQueue
    @abstract A concrete implementation of an IOOutputQueue. This object
    provides a FIFO packet queue to hold the incoming packets provided
    by the client transmit threads. A mutex lock is used to serialize
    access to the target's output handler. The mutex lock is local to the
    queue and there is no serialization performed between calls to the
    target's output handler, and any other threads that may be running on
    the target. See IOOutputQueue for additional information. */

class IOOQLockFIFOQueue : public IOOutputQueue
{
    OSDeclareDefaultStructors(IOOQLockFIFOQueue)

protected:
    IOLock *                _mutex;
    volatile bool           _serviceActive;
    volatile UInt32         _enqueueCount;

/*! @function serviceThread
    @discussion Provide an implementation for the interface defined in
    IOOutputQueue. This method is called by a callout thread when
    service() is performed asynchronously. */

    virtual void serviceThread();

/*! @function dequeue
    @discussion Responsible for removing all packets from the queue and calling
    the target's output handler. This method returns when the queue becomes
    empty or if the queue's state is no longer kIOOQStateRunning. The target's
    output handler is called for every packet removed from the queue. Only a
    single packet is sent to the target for every call, the packets are never
    chained. A mutex lock enforces single threaded access to the target's
    output handler. */

    virtual void dequeue();

/*! @function free
    @discussion Frees the IOOQLockFIFOQueue instance. */

    virtual void free();

public:

/*! @function init
    @discussion Initialize an IOOQLockFIFOQueue instance.
    @param target The object that shall receive packets from the queue,
    and is usually a subclass of IONetworkController. If the target is
    not an IONetworkController instance, then the target must immediately
    call registerOutputHandler() after initializing the queue.
    @param capacity The initial capacity of the output queue, defined as
    the number of packets that the queue can hold without dropping.
    @result true if initialized successfully, false otherwise. */

    virtual bool init(OSObject * target, UInt32 capacity = 0);

/*! @function withTarget
    @discussion Factory method that will construct and initialize an
    IOOQLockFIFOQueue instance.
    @param target Same as init().
    @param capacity Same as init().
    @result An IOOQLockFIFOQueue instance upon success, or 0 otherwise. */

    static IOOQLockFIFOQueue * withTarget(OSObject * target,
                                          UInt32     capacity = 0);

/*! @function enqueue
    @discussion Handles packet (or a packet chain) sent to the queue.
    This method can handle calls from multiple simultaneous client threads.
    A client thread that calls enqueue() will acquire a mutex lock and
    call dequeue() if it detects that no other thread is actively
    dequeueing packets from the queue. The dequeue() method will return
    when the queue becomes empty, or if the target stalls the queue.
    This method may block its caller.
    @param m The packet (or a packet chain) to be queued for transmission.
    @result The number of dropped packets. */

    virtual UInt32 enqueue(struct mbuf * m);

/*! @function start
    @discussion This method is called by the target to start the queue.
    Once started, the queue will be allowed to call the target's
    output handler. Before that, with the queue stopped, the queue will
    absorb incoming packets sent to the enqueue() method, but no packets
    will be dequeued, and the target's output handler will not be called.
    @result true if the queue was successfully started, false otherwise. */

    virtual bool start();

/*! @function stop
    @discussion Stops the queue to prevent it from calling the target's
    output handler. This call is synchronous the caller may block.
    The target's output handler must never call this method, or any other
    queue methods. */

    virtual void stop();

/*! @function service
    @discussion If the queue becomes stalled, then service() must be called
    to restart the queue when the target is ready to accept more packets.
    If the queue is not empty, this method will also call dequeue().
    Note that if the target never sends a kIOOQReturnActionStall action 
    code to the queue, then the queue will never stall on its own accord.
    Calling this method on a running queue that is not stalled is harmless.
    @param sync True if the service action should be performed synchronously,
    false to perform the action asynchronously without blocking the caller,
    but with a much higher latency cost.
    @result true if the queue needed servicing, false otherwise. */

    virtual bool service(bool sync = true);
};

#endif /* !_IOOQLOCKFIFOQUEUE_H */
