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
 * IOOutputQueue.cpp
 *
 * HISTORY
 * 2-Feb-1999       Joe Liu (jliu) created.
 *
 */

#include <IOKit/assert.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/network/IOPacketQueue.h>
#include <IOKit/network/IOOutputQueue.h>
#include <IOKit/network/IOOQLockFIFOQueue.h>
#include <IOKit/network/IOOQGateFIFOQueue.h>
#include <IOKit/network/IONetworkStats.h>
#include <IOKit/network/IONetworkController.h>

//===========================================================================
// IOOutputQueue class.
//
// An object that queues output packets on behalf of its target, usually an 
// IONetworkController instance, and allows the target to control the packet
// flow. IOOutputQueue is is an abstract class that provides an interface for
// its subclasses. Concrete subclasses will complete the implementation while
// providing unique synchronization options.
//===========================================================================

#warning need real atomic operators
#define ATOMIC_SET(x, v)    ((x) = (v))
#define ATOMIC_ADD(x, y)    ((x) += (y))

#undef  super
#define super OSObject
OSDefineMetaClass( IOOutputQueue, OSObject )
OSDefineAbstractStructors( IOOutputQueue, OSObject )

#define kIOOQParamMagic     ((void *) 0xfeedbeef)

//---------------------------------------------------------------------------
// Initialize an IOOutputQueue instance.
//
// target: The object that shall receive packets from the queue,
//   and is usually a subclass of IONetworkController. If the target is
//   not an IONetworkController instance, then the target must immediately
//   call registerOutputHandler() after initializing the queue.
//
// capacity: The initial capacity of the output queue, defined as
//   the number of packets that the queue can hold without dropping.
//
// Returns true if initialized successfully, false otherwise.

bool IOOutputQueue::init(OSObject * target,
                         UInt32     capacity)
{
#if 0
    _target    = 0;
    _outAction = 0;
    _stats     = 0;
    _param     = 0;
    _callEntry = 0;
    _queue     = 0;
#endif

    if (!super::init())
        return false;

    if (OSDynamicCast(IONetworkController, target))
    {
        // If the target is a type of IONetworkController, then call
        // its getOutputHandler() method to obtain a pointer to its
        // output method.

        if (registerOutputHandler(target,
            ((IONetworkController *) target)->getOutputHandler()) == false)
            return false;
    }

    // Allocate an IOPacketQueue (or a subclass).
    //
    _queue = createPacketQueue(capacity);
    if (!_queue)
        return false;

    // Allocate and initialize the callout entry.
    //
    _callEntry = thread_call_allocate((thread_call_func_t)&runServiceThread,
                                      (void *)this);
    if (!_callEntry)
        return false;

    // Create a data object for queue statistics.
    //
    _statsData = IONetworkData::withName(
                 kIOOutputQueueStatsKey,
                 sizeof(IOOutputQueueStats),
                 0,
                 kIONDBasicAccessTypes | kIONDAccessTypeReset,
                 this,
                 (IONetworkData::Action)
                     &IOOutputQueue::dataReadAndResetHandler,
                 kIOOQParamMagic);
    if (!_statsData)
        return false;

    _stats = (IOOutputQueueStats *) _statsData->getBuffer();

    return true;
}

//---------------------------------------------------------------------------
// Frees the IOOutputQueue instance.

void IOOutputQueue::free()
{
    if (_statsData)
        _statsData->release();

    if (_callEntry)
    {
        cancelServiceThread();
        thread_call_free(_callEntry);
    }
    
    if (_queue)
        _queue->release();

    super::free();
}

//---------------------------------------------------------------------------
// This method is called by our IONetworkData object when it receives
// a read or a reset request. We need to be notified in order to intervene
// in the request handling.
//
// data: The IONetworkData object.
// opFlag: The operation requested.
// arg: Argument for the request.

IOReturn IOOutputQueue::dataReadAndResetHandler(IONetworkData * data,
                                                UInt32          type,
                                                void *          arg)
{
    IOReturn ret = kIOReturnSuccess;

    assert(data && (arg == kIOOQParamMagic));

    // Check the type of data request.
    //
    switch (type)
    {
        case kIONDAccessTypeRead:
        case kIONDAccessTypeSerialize:
            _stats->capacity = getCapacity();
            _stats->size     = getSize();
            _stats->peakSize = getPeakSize();
            break;

        case kIONDAccessTypeReset:
            getPeakSize(true);
            getDropCount(true);
            getOutputCount(true);
            getRetryCount(true);
            getStallCount(true);
            break;

        default:
            ret = kIOReturnNotWritable;
            break;
    }

    return ret;
}

//---------------------------------------------------------------------------
// Returns the current state of the queue object.

IOOQState IOOutputQueue::getState() const
{
    return _state;
}

//---------------------------------------------------------------------------
// Schedule a service thread callout, which will then execute the
// serviceThread() method. Subclasses should not override this method.

bool IOOutputQueue::scheduleServiceThread()
{
    thread_call_enter(_callEntry);
    return true;
}

//---------------------------------------------------------------------------
// Cancel the service thread callout. Subclasses should not override this
// method.

bool IOOutputQueue::cancelServiceThread()
{
    return thread_call_cancel(_callEntry);
}

//---------------------------------------------------------------------------
// A glue function that is registered as the service thread callout handler.
// This function in turn will call the serviceThread() method.

void IOOutputQueue::runServiceThread(IOOutputQueue * self, thread_call_param_t)
{
    assert(self);
    self->serviceThread();
}

//---------------------------------------------------------------------------
// Must be implemented by subclasses that calls scheduleServiceThread().
// The default implementation is a placeholder and performs no useful action.

void IOOutputQueue::serviceThread()
{
}

//---------------------------------------------------------------------------
// Release all packets held in the queue. The size of the queue is reset
// to zero. The drop packet counter is incremented by the number of packets
// freed. See getDropCount().
//
// Returns the number of packets freed.

UInt32 IOOutputQueue::flush()
{
    UInt32 flushCount = _queue->lockFlush();
    ATOMIC_ADD(_stats->dropCount, flushCount);
    return flushCount;
}

//---------------------------------------------------------------------------
// capacity: The new capacity of the queue.
//
// Returns true if the new capacity was accepted, false otherwise.

bool IOOutputQueue::setCapacity(UInt32 capacity)
{
    return _queue->setCapacity(capacity);
}

//---------------------------------------------------------------------------
// Returns the current queue capacity.

UInt32 IOOutputQueue::getCapacity() const
{
    return _queue->getCapacity();
}

//---------------------------------------------------------------------------
// Returns the current queue size.

UInt32 IOOutputQueue::getSize() const
{
    return _queue->getSize();
}

//---------------------------------------------------------------------------
// reset: Resets the counter if true.
//
// Return the peak queue size.

UInt32 IOOutputQueue::getPeakSize(bool reset = false)
{
    return _queue->getPeakSize(reset);
}

//---------------------------------------------------------------------------
// This method returns the number of times that a kIOOQReturnStatusDropped 
// status code is received from the target's output handler, indicating
// that the packet given was dropped. This count is also incremented when
// the queue drops a packet due to overcapacity, or by an explicit flush()
// call.
//
// reset: Resets the counter if true.
//
// Returns the number of dropped packets.
//
// FIXME - need atomic exchange

UInt32 IOOutputQueue::getDropCount(bool reset = false)
{
    UInt32 count = _stats->dropCount;
    if (reset)
        _stats->dropCount = 0;
    return count;
}

//---------------------------------------------------------------------------
// This method returns the number of times that a kIOOQReturnStatusSuccess 
// status code is received from the target's output handler, indicating
// that the packet given was accepted, and is ready to be (or already has been)
// transmitted.
//
// reset: Resets the counter if true.
//
// Returns the number of packets accepted by our target.
//
// FIXME - need atomic exchange

UInt32 IOOutputQueue::getOutputCount(bool reset = false)
{
    UInt32 count = _stats->outputCount;
    if (reset)
        _stats->outputCount = 0;
    return count;
}

//---------------------------------------------------------------------------
// This method returns the number of times that a kIOOQReturnStatusRetry
// status code is received from the target's output handler, indicating
// that the target is temporarily unable to handle the packet given, and
// the queue should try to resend the same packet at some later time.
//
// reset: Resets the counter if true.
//
// Returns the number of retries issued by the target.
//
// FIXME - need atomic exchange

UInt32 IOOutputQueue::getRetryCount(bool reset = false)
{
    UInt32 count = _stats->retryCount;
    if (reset)
        _stats->retryCount = 0;
    return count;
}

//---------------------------------------------------------------------------
// Each time the queue is stalled, when a kIOOQReturnActionStall action code
// is received from the target's output handler, a counter is incremented.
// This method returns the value stored in this counter.
//
// reset: Resets the counter if true.
//
// Returns the number of times that the queue was stalled.
//
// FIXME - need atomic exchange

UInt32 IOOutputQueue::getStallCount(bool reset = false)
{
    UInt32 count = _stats->stallCount;
    if (reset)
        _stats->stallCount = 0;
    return count;
}

//---------------------------------------------------------------------------
// Allows subclasses to override the default action, which is to allocate
// and return an IOPacketQueue instance. The returned object is used to 
// implement the queueing behavior of the IOOutputQueue.
//
// capacity: The initial capacity of the queue.
//
// Returns a newly allocated and initialized IOPacketQueue instance.

IOPacketQueue * IOOutputQueue::createPacketQueue(UInt32 capacity) const
{
    return IOPacketQueue::withCapacity(capacity);
}

//---------------------------------------------------------------------------
// Return an address of a method that is designated to handle
// packets sent to the queue object.
//
// Returns the address of the output packet handler.

IOOutputAction IOOutputQueue::getOutputHandler() const
{
    return (IOOutputAction) &IOOutputQueue::enqueue;
}

//---------------------------------------------------------------------------
// Register the target object and method to call to handle packets
// removed from the queue.
//
// handler: Pointer to an initialized IOOutputHandler structure passed in
//          by the caller.
//
// Returns true if the structure given was accepted, otherwise return false.

bool IOOutputQueue::registerOutputHandler(OSObject *      target,
                                          IOOutputAction  action)
{
    if (!target || !action)
        return false;

    // Cache the structure fields to instance variables.
    //
    _target    = target;
    _outAction = action;

    return true;
}

//---------------------------------------------------------------------------
// Return an IONetworkData object containing an IOOutputQueueStats structure.

IONetworkData * IOOutputQueue::getStatisticsData() const
{
    return _statsData;
}

//===========================================================================
// IOOQLockFIFOQueue
//===========================================================================

#undef  super
#define super IOOutputQueue
OSDefineMetaClassAndStructors( IOOQLockFIFOQueue, IOOutputQueue )

#define LOCK     IOTakeLock(_mutex)
#define UNLOCK   IOUnlock(_mutex)

//---------------------------------------------------------------------------
// Initialize an IOOQLockFIFOQueue instance.
//
// target: The object that shall receive packets from the queue,
//   and is usually a subclass of IONetworkController. If the target is
//   not an IONetworkController instance, then the target must immediately
//   call registerOutputHandler() after initializing the queue.
//
// capacity: The initial capacity of the output queue, defined as
//   the number of packets that the queue can hold without dropping.
//
// Returns true if initialized successfully, false otherwise.

bool IOOQLockFIFOQueue::init(OSObject * target, UInt32 capacity = 0)
{
    if (!super::init(target, capacity))
        return false;

    // Allocate the mutex lock.
    //
    _mutex = IOLockAlloc();
    if (!_mutex)
        return false;

    ATOMIC_SET(_state, kIOOQStateStopped);

    return true;
}

//---------------------------------------------------------------------------
// Factory method that will construct and initialize an IOOQLockFIFOQueue 
// instance.

IOOQLockFIFOQueue * IOOQLockFIFOQueue::withTarget(OSObject * target,
                                                  UInt32     capacity = 0)
{
    IOOQLockFIFOQueue * queue = new IOOQLockFIFOQueue;
    
    if (queue && !queue->init(target, capacity))
    {
        queue->release();
        queue = 0;
    }
    return queue;
}

//---------------------------------------------------------------------------
// Frees the IOOQLockFIFOQueue instance.

void IOOQLockFIFOQueue::free()
{
    cancelServiceThread();

    if (_mutex) IOLockFree(_mutex);

    super::free();
}

//---------------------------------------------------------------------------
// Provide an implementation for the interface defined in IOOutputQueue.
// This method is called by a callout thread when service() is performed 
// asynchronously.

void IOOQLockFIFOQueue::serviceThread()
{
    // Bump the enqueue count, and service the queue if 'serviceActive'
    // is false.

    ATOMIC_ADD(_enqueueCount, 1);

    if ((_state == kIOOQStateRunning) && !_serviceActive)
    {
        dequeue();
    }
}

//---------------------------------------------------------------------------
// Handles packet (or a packet chain) sent to the queue. This method can
// handle calls from multiple simultaneous client threads. A client thread
// that calls enqueue() will acquire a mutex lock and call dequeue() if it
// detects that no other thread is actively dequeueing packets from the queue.
// The dequeue() method will return when the queue becomes empty, or if the 
// target stalls the queue. This method may block its caller.
//
// m: The packet (or a packet chain) to be queued for transmission.
//
// Returns: The number of dropped packets.

UInt32 IOOQLockFIFOQueue::enqueue(struct mbuf * m)
{
    UInt32 dropped = _queue->lockEnqueue(m);

    // Increment the dropped packet counter.
    if (dropped)
        ATOMIC_ADD(_stats->dropCount, dropped);

    // Bump the enqueue count, and service the queue if 'serviceActive'
    // is false.

    ATOMIC_ADD(_enqueueCount, 1);

    if ((_state == kIOOQStateRunning) && !_serviceActive)
    {
        dequeue();
    }

    return dropped;
}

//---------------------------------------------------------------------------
// Responsible for removing all packets from the queue and calling
// the target's output handler. This method returns when the queue becomes
// empty or if the queue's state is no longer kIOOQStateRunning. The target's
// output handler is called for every packet removed from the queue. Only a
// single packet is sent to the target for every call, the packets are never
// chained. A mutex lock enforces single threaded access to the target's
// output handler.

void IOOQLockFIFOQueue::dequeue()
{
    struct mbuf *  m;
    UInt32         r;
    UInt32         dequeueCount;

    LOCK;

    do {
        // Take a snapshot of the current enqueueCount.
        //
        dequeueCount = _enqueueCount;

        _serviceActive = true;  // -- mark dequeuing active --

        while ((_state == kIOOQStateRunning) &&
                _queue->getSize() && (m = _queue->lockDequeue()))
        {
            ATOMIC_SET(_state, kIOOQStateStalled);

            // Call the target's output handler.
            //
            r = (_target->*_outAction)(m);

            // Decide the fate of the dequeued packet and
            // update statistics counters.
            //
            switch (r & kIOOQReturnStatusMask)
            {
                default:
                case kIOOQReturnStatusDropped:
                    ATOMIC_ADD(_stats->dropCount, 1);
                    break;

                case kIOOQReturnStatusSuccess:
                    ATOMIC_ADD(_stats->outputCount, 1);
                    break;

                case kIOOQReturnStatusRetry:
                    _queue->lockPrepend(m);
                    ATOMIC_ADD(_stats->retryCount, 1);
                    break;
            }

            // Handle the requested action.
            //
            switch (r & kIOOQReturnActionMask)
            {
                default:
                case kIOOQReturnActionNone:
                    ATOMIC_SET(_state, kIOOQStateRunning);
                    break;

                case kIOOQReturnActionStall:
                    ATOMIC_ADD(_stats->stallCount, 1);
                    break;
            }

            // Consume all enqueueCounts before looping.
            //
            dequeueCount = _enqueueCount;

        } /* while [ running and queue has packets ] */

        _serviceActive = false; // -- mark dequeuing inactive --

    } while (dequeueCount != _enqueueCount);

    UNLOCK;
}

//---------------------------------------------------------------------------
// This method is called by the target to start the queue. Once started,
// the queue will be allowed to call the target's output handler.
// Before that, with the queue stopped, the queue will absorb incoming
// packets sent to the enqueue() method, but no packets will be dequeued,
// and the target's output handler will not be called.
//
// Returns true if the queue was successfully started, false otherwise.

bool IOOQLockFIFOQueue::start()
{
    service(true);
    return true;    // always return success
}

//---------------------------------------------------------------------------
// Stops the queue to prevent it from calling the target's output handler.
// This call is synchronous the caller may block. The target's output handler 
// must never call this method, or any other queue methods.

void IOOQLockFIFOQueue::stop()
{
    LOCK;
    ATOMIC_SET(_state, kIOOQStateStopped);
    UNLOCK;
}

//---------------------------------------------------------------------------
// If the queue becomes stalled, then service() must be called to restart
// the queue when the target is ready to accept more packets. If the queue
// is not empty, this method will also call dequeue(). Note that if the
// target never sends a kIOOQReturnActionStall action code to the queue,
// then the queue will never stall on its own accord. Calling this method
// on a running queue that is not stalled is harmless.
//
// sync: True if the service action should be performed synchronously,
//   false to perform the action asynchronously without blocking the caller,
//   but with a much higher latency cost.
//
// Returns true if the queue needed servicing, false otherwise.

bool IOOQLockFIFOQueue::service(bool sync = true)
{
    bool started = false;

    ATOMIC_SET(_state, kIOOQStateRunning);

    if (_queue->getSize())
    {
        ATOMIC_ADD(_enqueueCount, 1);

        if (!_serviceActive)
        {
            if (sync)
                dequeue();
            else
                scheduleServiceThread();

            started = true;
        }
    }

    return started;
}

//===========================================================================
// IOOQGateFIFOQueue
//===========================================================================

#undef  super
#define super IOOutputQueue
OSDefineMetaClassAndStructors( IOOQGateFIFOQueue, IOOutputQueue )

#define GATED_DEQUEUE   _gate->runCommand()

//---------------------------------------------------------------------------
// Initialize an IOOQGateFIFOQueue instance.
//
// target: The object that shall receive packets from the queue,
//   and is usually a subclass of IONetworkController. If the target is
//   not an IONetworkController instance, then the target must immediately
//   call registerOutputHandler() after initializing the queue.
//
// workloop: A workloop object provided by the target that the
//   queue will use to add an internal IOCommandGate as an event source.
//
// capacity: The initial capacity of the output queue, defined as
//   the number of packets that the queue can hold without dropping.
//
// Returns true if initialized successfully, false otherwise.

bool IOOQGateFIFOQueue::init(OSObject *        target,
                             IOWorkLoop *      workloop,
                             UInt32            capacity = 0)
{
    if (!super::init(target, capacity))
        return false;

    // Verify that the IOWorkLoop is valid.
    //
    if (!OSDynamicCast(IOWorkLoop, workloop))
        return false;

    // Allocate and attach an IOCommandGate instance to the workloop.
    // Set the default action to gatedDequeue().
    //
    _gate = IOCommandGate::commandGate(this,
               (IOCommandGate::Action) &IOOQGateFIFOQueue:: gatedDequeue);
    if (!_gate || (workloop->addEventSource(_gate) != kIOReturnSuccess))
        return false;

    ATOMIC_SET(_state, kIOOQStateStopped);

    return true;
}

//---------------------------------------------------------------------------
// Factory method that will construct and initialize an IOOQGateFIFOQueue 
// instance.
//
// Returns an IOOQGateFIFOQueue instance upon success, or 0 otherwise.

IOOQGateFIFOQueue *
IOOQGateFIFOQueue::withTarget(OSObject *   target,
                              IOWorkLoop * workloop,
                              UInt32       capacity = 0)
{
    IOOQGateFIFOQueue * queue = new IOOQGateFIFOQueue;
    
    if (queue && !queue->init(target, workloop, capacity))
    {
        queue->release();
        queue = 0;
    }
    return queue;
}

//---------------------------------------------------------------------------
// Frees the IOOQGateFIFOQueue instance.

void IOOQGateFIFOQueue::free()
{
    cancelServiceThread();

    if (_gate)
        _gate->release();

    super::free();
}

//---------------------------------------------------------------------------
// Provide an implementation for the interface defined in IOOutputQueue.
// This method is called by a callout thread when service() is performed 
// asynchronously.

void IOOQGateFIFOQueue::serviceThread()
{
    // Bump the enqueue count, and service the queue if 'serviceActive'
    // is false.

    ATOMIC_ADD(_enqueueCount, 1);

    if ((_state == kIOOQStateRunning) && !_serviceActive)
    {
        GATED_DEQUEUE;
    }
}

//---------------------------------------------------------------------------
// Handles packet (or a packet chain) sent to the queue. This method can
// handle calls from multiple simultaneous client threads. A client thread
// that calls enqueue() will acquire a mutex lock and call dequeue() if it
// detects that no other thread is actively dequeueing packets from the queue.
// The dequeue() method will return when the queue becomes empty, or if the 
// target stalls the queue. This method may block its caller.
//
// m: The packet (or a packet chain) to be queued for transmission.

UInt32 IOOQGateFIFOQueue::enqueue(struct mbuf * m)
{
    UInt32 dropped = _queue->lockEnqueue(m);

    // Increment the dropped packet counter.
    if (dropped)
        ATOMIC_ADD(_stats->dropCount, dropped);

    // Bump the enqueue count, and service the queue if 'serviceActive'
    // is false.

    ATOMIC_ADD(_enqueueCount, 1);

    if ((_state == kIOOQStateRunning) && !_serviceActive)
    {
        GATED_DEQUEUE;
    }

    return dropped;
}

//---------------------------------------------------------------------------
// Responsible for removing all packets from the queue and calling
// the target's output handler. This method returns when the queue becomes
// empty or if the queue's state is no longer kIOOQStateRunning. The target's
// output handler is called for every packet removed from the queue. Only a
// single packet is sent to the target for every call, the packets are never
// chained. An IOCommandGate attached to an workloop provided by the target
// ensures mutual exclusion between the dequeueing action (and calls to the
// target's output handler), and any other action performed by the workloop's
// thread.

void IOOQGateFIFOQueue::dequeue()
{
    GATED_DEQUEUE;
}

void IOOQGateFIFOQueue::gatedDequeue()
{
    struct mbuf *  m;
    UInt32         r;
    UInt32         dequeueCount;

    do {
        // Take a snapshot of the current enqueueCount.
        //
        dequeueCount = _enqueueCount;

        _serviceActive = true;  // -- mark dequeuing active --

        while ((_state == kIOOQStateRunning) &&
                _queue->getSize() && (m = _queue->lockDequeue()))
        {
            ATOMIC_SET(_state, kIOOQStateStalled);

            // Call the controller's output handler.
            //
            r = (_target->*_outAction)(m);

            // Decide what should happen to the dequeued packet and
            // update statistics counters.
            //
            switch (r & kIOOQReturnStatusMask)
            {
                default:
                case kIOOQReturnStatusDropped:
                    ATOMIC_ADD(_stats->dropCount, 1);
                    break;

                case kIOOQReturnStatusSuccess:
                    ATOMIC_ADD(_stats->outputCount, 1);
                    break;

                case kIOOQReturnStatusRetry:
                    _queue->lockPrepend(m);
                    ATOMIC_ADD(_stats->retryCount, 1);
                    break;
            }

            // Handle the requested action.
            //
            switch (r & kIOOQReturnActionMask)
            {
                default:
                case kIOOQReturnActionNone:
                    ATOMIC_SET(_state, kIOOQStateRunning);
                    break;

                case kIOOQReturnActionStall:
                    ATOMIC_ADD(_stats->stallCount, 1);
                    break;
            }

            // Consume all enqueueCounts before looping.
            //
            dequeueCount = _enqueueCount;

        } /* while [ running and queue has packets ] */

        _serviceActive = false; // -- mark dequeuing inactive --

    } while (dequeueCount != _enqueueCount);
}

//---------------------------------------------------------------------------
// This method is called by the target to start the queue. Once started,
// the queue will be allowed to call the target's output handler.
// Before that, with the queue stopped, the queue will absorb incoming
// packets sent to the enqueue() method, but no packets will be dequeued,
// and the target's output handler will not be called.
//
// Returns true if the queue was successfully started, false otherwise.

bool IOOQGateFIFOQueue::start()
{
    service(true);
    return true;    // always return success
}

//---------------------------------------------------------------------------
// Stops the queue to prevent it from calling the target's output handler.
// This call is synchronous the caller may block. The target's output handler 
// must never call this method, or any other queue methods.

void IOOQGateFIFOQueue::stop()
{   
    _gate->runAction( (IOCommandGate::Action) &IOOQGateFIFOQueue::gatedStop );
}

void IOOQGateFIFOQueue::gatedStop()
{
    ATOMIC_SET(_state, kIOOQStateStopped);
}

//---------------------------------------------------------------------------
// If the queue becomes stalled, then service() must be called to restart
// the queue when the target is ready to accept more packets. If the queue
// is not empty, this method will also call dequeue(). Note that if the
// target never sends a kIOOQReturnActionStall action code to the queue,
// then the queue will never stall on its own accord. Calling this method
// on a running queue that is not stalled is harmless.
//
// sync: True if the service action should be performed synchronously,
//   false to perform the action asynchronously without blocking the caller,
//   but with a much higher latency cost.
//
// Returns true if the queue needed servicing, false otherwise.

bool IOOQGateFIFOQueue::service(bool sync = true)
{
    bool started = false;

    ATOMIC_SET(_state, kIOOQStateRunning);

    if (_queue->getSize())
    {
        ATOMIC_ADD(_enqueueCount, 1);

        if (!_serviceActive)
        {
            if (sync)
                GATED_DEQUEUE;
            else
                scheduleServiceThread();

            started = true;
        }
    }

    return started;
}
