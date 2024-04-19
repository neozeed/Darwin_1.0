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
 * IOOutputQueue.h
 *
 * HISTORY
 * 2-Feb-1999       Joe Liu (jliu) created.
 *
 */

#ifndef _IOOUTPUTQUEUE_H
#define _IOOUTPUTQUEUE_H

#include <IOKit/network/IOPacketQueue.h>
#include <IOKit/network/IONetworkInterface.h>

// Forward declarations.
//
struct mbuf;
class IONetworkData;

// FIXME - We do not want the enqueue/dequeue macros defined in queue.h.
//
// #warning queue.h should not be included
#undef enqueue(queue,elt)
#undef dequeue(queue)


/*! @enum IOOQReturnStatus The target's output handler is responsible
    for generating a return code containing the status, and an action
    for the queue. The target's output handler must never call any of
    the queue methods. The defined status codes are:
    @constant kIOOQReturnStatusMask A mask for the status field in the
    32-bit return value.
    @constant kIOOQReturnStatusSuccess Packet was accepted.
    @constant kIOOQReturnStatusDropped Packet accepted, but was also
    dropped.
    @constant kIOOQReturnStatusRetry Packet not accepted, retry sending the
    same packet.
    */

enum IOOQReturnStatus {
    kIOOQReturnStatusMask     = 0x00ff,
    kIOOQReturnStatusSuccess  = 0x0000,
    kIOOQReturnStatusDropped  = 0x0001,
    kIOOQReturnStatusRetry    = 0x0002,
};

/*! @enum IOOQReturnAction The target's output handler is responsible
    for generating a return code containing the status, and an action
    for the queue. The target's output handler must never call any of
    the queue methods. The defined action codes are:
    @constant kIOOQReturnActionMask A mask for the action field in the
    32-bit return value.
    @constant kIOOQReturnActionNone No action is required.
    @constant kIOOQReturnActionStall Stall the queue. A service() call
    will restart the queue.
    */

enum IOOQReturnAction {
    kIOOQReturnActionMask     = 0xff00,
    kIOOQReturnActionNone     = 0x0000,
    kIOOQReturnActionStall    = 0x0100,
//  kIOOQReturnActionDefer    = 0x0200, /* (not used) */
};

/*
 * Commonly used return codes containing both a status and an action code.
 */
/*! @constant kIOOQReturnSuccess Same as 
    (kIOOQReturnStatusSuccess | kIOOQReturnActionNone). 
    Packet was accepted. */

#define kIOOQReturnSuccess  (kIOOQReturnStatusSuccess | kIOOQReturnActionNone)

/*! @constant kIOOQReturnDropped Same as 
    (kIOOQReturnStatusDropped | kIOOQReturnActionNone). 
    Packet was dropped. */

#define kIOOQReturnDropped  (kIOOQReturnStatusDropped | kIOOQReturnActionNone)

/*! @constant kIOOQReturnStall Same as
    (kIOOQReturnStatusRetry   | kIOOQReturnActionStall).
    Stall the queue and retry the same packet when the queue is restarted. */

#define kIOOQReturnStall    (kIOOQReturnStatusRetry   | kIOOQReturnActionStall)

/*
 * Output queue states.
 */
/*! @enum IOOQState
    @constant kIOOQStateStopped The queue is not running. A start() call
    will start the queue.
    @constant kIOOQStateRunning The queue is currently running. A stop()
    call will stop it.
    @constant kIOOQStateStalled A running queue was stalled. Call service()
    or stop(), to respectively restart or stop the queue. */

enum IOOQState {
    kIOOQStateStopped = 0,
    kIOOQStateRunning,
    kIOOQStateStalled,
};


/*! @class IOOutputQueue
    @abstract An object that queues output packets on behalf of its target,
    usually an IONetworkController instance, and allows the target to control 
    the packet flow. IOOutputQueue is is an abstract class that provides an 
    interface for its subclasses. Concrete subclasses will complete the 
    implementation while providing unique synchronization options.
    */

class IOOutputQueue : public OSObject
{
    OSDeclareAbstractStructors(IOOutputQueue)

private:
    IOReturn dataReadAndResetHandler(IONetworkData * data,
                                     UInt32          opFlag,
                                     void *          arg);

protected:
    OSObject *              _target;    // target object.
    IOOutputAction          _outAction; // target's output function.
    volatile IOOQState      _state;     // queue's state.
    IOOutputQueueStats *    _stats;     // output queue statistics.
    IONetworkData *         _statsData; // statistics data object.
    thread_call_t           _callEntry; // callout entry structure.
    IOPacketQueue *         _queue;     // packet queue.

/*! @function init
    @discussion Initialize an IOOutputQueue instance.
    @param target The object that shall receive packets from the queue,
    and is usually a subclass of IONetworkController. If the target is
    not an IONetworkController instance, then the target must immediately
    call registerOutputHandler() after initializing the queue.
    @param capacity The initial capacity of the output queue, defined as
    the number of packets that the queue can hold without dropping.
    @result true if initialized successfully, false otherwise. */

    virtual bool init(OSObject * target, UInt32 capacity);

/*! @function free
    @discussion Frees the IOOutputQueue instance. */

    virtual void free();

/*! @function runServiceThread
    @discussion A glue function that is registered as the service thread
    callout handler. This function in turn will call the serviceThread() 
    method.
    @param self An argument previously registered with the callout to
    identify the IOOutputQueue instance associated with the callout. */

    static void runServiceThread(IOOutputQueue * self, thread_call_param_t);

/*! @function scheduleServiceThread
    @discussion Schedule a service thread callout, which will then
    execute the serviceThread() method. Subclasses should not override
    this method.
    @result true if scheduling the thread callout was successful, false
    otherwise. */

    virtual bool scheduleServiceThread();

/*! @function cancelServiceThread
    @discussion Cancel the service thread callout. Subclasses should not 
    override this method.
    @result true if a previously scheduled thread callout was canceled,
    false otherwise. */

    virtual bool cancelServiceThread();

/*! @function serviceThread
    @discussion Must be implemented by subclasses that calls
    scheduleServiceThread(). The default implementation is a placeholder and
    performs no useful action. */

    virtual void serviceThread();

/*! @function createPacketQueue
    @discussion Allows subclasses to override the default action, which
    is to allocate and return an IOPacketQueue instance. The returned
    object is used to implement the queueing behavior of the IOOutputQueue.
    @param capacity The initial capacity of the queue.
    @result A newly allocated and initialized IOPacketQueue instance. */

    virtual IOPacketQueue * createPacketQueue(UInt32 capacity) const;

public:

/*! @function enqueue
    @discussion Handles packet (or a packet chain) sent to the queue.
    This method can handle calls from multiple simultaneous client threads.
    A pointer to this function is written to the "output" field in the 
    IOOutputHandler structure by getOutputHandler(), thus allowing client 
    objects to call this method in order to process the output packet.
    @param m The packet (or a packet chain) to be queued for transmission.
    @result The number of dropped packets. */

    virtual UInt32 enqueue(struct mbuf * m) = 0;

/*! @function start
    @discussion This method is called by the target to start the queue.
    Once started, the queue will be allowed to call the target's
    output handler. Before that, with the queue stopped, the queue will
    absorb incoming packets sent to the enqueue() method, but no packets
    will be dequeued, and the target's output handler will not be called.
    @result true if the queue was successfully started, false otherwise. */

    virtual bool start() = 0;

/*! @function stop
    @discussion Stops the queue to prevent it from calling the target's
    output handler. This call is synchronous the caller may block.
    The target's output handler must never call this method, or any other
    queue methods. */

    virtual void stop() = 0;

/*! @function service
    @discussion If the queue becomes stalled, then service() must be called
    to restart the queue when the target is ready to accept more packets.
    Note that if the target never sends a kIOOQReturnActionStall action code
    to the queue, then the queue will never stall on its own accord.
    Calling this method on a running queue that is not stalled is harmless.
    @param sync True if the service action should be performed synchronously,
    false to perform the action asynchronously without blocking the caller,
    but with a much higher latency cost.
    @result true if the queue needed servicing, false otherwise. */

    virtual bool service(bool sync = true) = 0;

/*! @function flush
    @discussion Release all packets held in the queue. The size of the queue
    is reset to zero. The drop packet counter is incremented by the number
    of packets freed. See getDropCount().
    @result The number of packets freed. */

    virtual UInt32 flush();

/*! @function getState
    @result The current state of the queue object. See IOOQState enumeration. 
    */ 

    virtual IOOQState getState() const;

/*! @function setCapacity
    @param capacity The new capacity of the queue.
    @result true if the new capacity was accepted, false otherwise. */

    virtual bool setCapacity(UInt32 capacity);

/*! @function getCapacity
    @result The current queue capacity. */

    virtual UInt32 getCapacity() const;

/*! @function getSize
    @result The current queue size. */

    virtual UInt32 getSize() const;

/*! @function getPeakSize
    @param reset Resets the counter if true.
    @result The peak queue size. */

    virtual UInt32 getPeakSize(bool reset = false);

/*! @function getDropCount
    @discussion This method returns the number of times that a
    kIOOQReturnStatusDropped status code is received from the target's
    output handler, indicating that the packet given was dropped. This
    count is also incremented when the queue drops a packet due to
    overcapacity, or by an explicit flush() call.
    @param reset Resets the counter if true.
    @result The number of dropped packets. */

    virtual UInt32 getDropCount(bool reset = false);

/*! @function getOutputCount
    @discussion This method returns the number of times that a
    kIOOQReturnStatusSuccess status code is received from the target's
    output handler, indicating that the packet given was accepted,
    and is ready to be (or already has been) transmitted.
    @param reset Resets the counter if true.
    @result The number of packets accepted by our target. */

    virtual UInt32 getOutputCount(bool reset = false);

/*! @function getRetryCount
    @discussion This method returns the number of times that a
    kIOOQReturnStatusRetry status code is received from the target's
    output handler, indicating that the target is temporarily unable
    to handle the packet given, and the queue should try to resend the
    same packet at some later time.
    @param reset Resets the counter if true.
    @result The number of retries issued by the target. */

    virtual UInt32 getRetryCount(bool reset = false);

/*! @function getStallCount
    @discussion Each time the queue is stalled, when a kIOOQReturnActionStall 
    action code is received from the target's output handler, a counter is 
    incremented. This method returns the value stored in this counter.
    @param reset Resets the counter if true.
    @result The number of times that the queue was stalled. */

    virtual UInt32 getStallCount(bool reset = false);

/*! @function registerOutputHandler
    @discussion Register the target object and method to call to
    handle packets removed from the queue.
    @param target Target object that implements the output action.
    @param action The action to call to handle the output packet.
    @result true if the handler provided was accepted, false otherwise. */

    virtual bool registerOutputHandler(OSObject *      target,
                                       IOOutputAction  action);

/*! @function getOutputHandler
    @discussion Return an address of a method that is designated to handle
    packets sent to the queue object.
    @result Address of the output packet handler. */

    IOOutputAction getOutputHandler() const;

/*! @function getStatisticsData
    @result An IONetworkData object containing an IOOutputQueueStats structure.
    */

    IONetworkData * getStatisticsData() const;
};

#endif /* !_IOOUTPUTQUEUE_H */
