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
 * IOTimerEventSource.h
 *
 * HISTORY
 * 2-Feb-1999		Joe Liu (jliu) created.
 *
 */

#ifndef _IOTIMEREVENTSOURCE
#define _IOTIMEREVENTSOURCE

#include <sys/cdefs.h>

__BEGIN_DECLS
#include <kern/clock.h>
__END_DECLS

#include <IOKit/IOEventSource.h>

/*!
    @class IOTimerEventSource : public IOEventSource
    @abstract Time based event source mechanism.
    @discussion An event source that implements a simple timer.	 A timeout handler is called once the timeout period expires.  This timeout handler will be called by the work-loop that this event source is attached to.
<br><br>
	Usually a timer event source will be used to implement a timeout.  In general when a driver makes a request it will need to setup a call to keep track of when the I/O doesn't complete.  This class is designed to make that somewhat easier.
<br><br>
	Remember the system doesn't guarantee the accuracy of the callout.	It is possible that a higher priority thread is running which will delay the execution of the action routine.  In fact the thread will be made runable at the exact requested time, within the accuracy of the CPU's decrementer based interrupt, but the scheduler will then control execution.
*/
class IOTimerEventSource : public IOEventSource
{
    OSDeclareDefaultStructors(IOTimerEventSource)

protected:
/*! @var calloutEntry thread_call entry for preregistered thread callouts */
    void *calloutEntry;

/*! @var abstime time to wake up next, see enable. */
    AbsoluteTime abstime;

/*! @function timeout
    @abstract Function that routes the call from the OS' timeout mechanism into a work-loop context.
    @discussion timeout will normally not be called nor overridden by a subclass.  If the event source is enabled then close the work-loop's gate and call the action routine.
    @param self This argument will be cast to an IOTimerEventSource. */
    static void timeout(void *self);

/*! @function setTimeoutFunc
    @abstract Set's timeout as the function of calloutEntry.
    @discussion IOTimerEventSource is based upon the kern/thread_call.h APIs currently.	 This function allocates the calloutEntry member variable by using thread_call_allocate(timeout, this).	 If you need to write your own subclass of IOTimerEventSource you probably should override this method to allocate an entry that points to your own timeout routine. */
    virtual void setTimeoutFunc();

/*! @function free
    @abstract Sub-class implementation of free method, frees calloutEntry */
    virtual void free();

/*! @function checkForWork
    @abstract Have to implement it is mandatory in $link IOEventSource, but IOTimerEventSources don't actually use this work-loop mechanism. */
    virtual bool checkForWork();

public:

/*! @enum Scale Factors
    @discussion Used when a scale_factor parameter is required to define a unit of time.
    @constant kNanosecondScale Scale factor for nanosecond based times.
    @constant kMicrosecondScale Scale factor for microsecond based times.
    @constant kMillisecondScale Scale factor for millisecond based times. */
    enum {
	kNanosecondScale  = 1,
	kMicrosecondScale = 1000,
	kMillisecondScale = 1000 * 1000
    };

/*! @typedef Action
    @discussion 'C' Function pointer defining the callout routine of this event source.
    @param owner Owning target object.	Note by a startling coincidence the first parameter in a C callout is currently used to define the target of a C++ member function.
    @param sender The object that timed out. */
    typedef void (*Action)(OSObject *owner, IOTimerEventSource *sender);

/*! @function timerEventSource
    @abstract Allocates and returns an initialized timer instance.
    @param owner
    @param action */
    static IOTimerEventSource *
	timerEventSource(OSObject *owner, Action action = 0);

/*! @function init
    @abstract Initializes the timer with an owner, and a handler to call when the timeout expires.
    @param owner 
    @param action */
    virtual bool init(OSObject *owner, Action action = 0);

/*! @function enable
    @abstract Enables a call to the action.
    @discussion Allows the action function to be called.  If the timer event source was disabled while a call was outstanding and the call wasn't cancelled then it will be rescheduled.  So a disable/enable pair will disable calls from this event source. */
    virtual void enable();

/*! @function disable
    @abstract Disable a timed callout.
    @discussion When disable returns the action will not be called until the next time enable(qv) is called. */
    virtual void disable();


/*! @function setTimeoutTicks
    @abstract Setup a callback at after the delay in scheduler ticks.  See wakeAtTime(AbsoluteTime).
    @param interval Delay from now to wake up, in scheduler ticks, whatever that may be.
    @result kIOReturnSuccess if everything is fine, kIOReturnNoResources if action hasn't been declared. */
    virtual IOReturn setTimeoutTicks(UInt32 ticks);

/*! @function setTimeoutMS
    @abstract Setup a callback at after the delay in milliseconds.  See wakeAtTime(AbsoluteTime).
    @param interval Delay from now to wake up, time in milliseconds.
    @result kIOReturnSuccess if everything is fine, kIOReturnNoResources if action hasn't been declared. */
    virtual IOReturn setTimeoutMS(UInt32 ms);

/*! @function setTimeoutUS
        @abstract Setup a callback at after the delay in microseconds.	 See wakeAtTime(AbsoluteTime).
    @param interval Delay from now to wake up, time in microseconds.
    @result kIOReturnSuccess if everything is fine, kIOReturnNoResources if action hasn't been declared. */
    virtual IOReturn setTimeoutUS(UInt32 us);

/*! @function setTimeout
    @abstract Setup a callback at after the delay in some unit.	 See wakeAtTime(AbsoluteTime).
    @param interval Delay from now to wake up in some defined unit.
    @param scale_factor Define the unit of interval, default to nanoseconds.
    @result kIOReturnSuccess if everything is fine, kIOReturnNoResources if action hasn't been declared. */
    virtual IOReturn setTimeout(UInt32 interval,
				UInt32 scale_factor = kNanosecondScale);

/*! @function setTimeout
    @abstract Setup a callback at after the delay in decrementer ticks.	 See wakeAtTime(AbsoluteTime).
    @param interval Delay from now to wake up.
    @result kIOReturnSuccess if everything is fine, kIOReturnNoResources if action hasn't been declared. */
    virtual IOReturn setTimeout(mach_timespec_t interval);

/*! @function setTimeout
    @abstract Setup a callback at after the delay in decrementer ticks.	 See wakeAtTime(AbsoluteTime).
    @param interval Delay from now to wake up in decrementer ticks.
    @result kIOReturnSuccess if everything is fine, kIOReturnNoResources if action hasn't been declared. */
    virtual IOReturn setTimeout(AbsoluteTime interval);

/*! @function wakeAtTimeTicks
    @abstract Setup a callback at this absolute time.  See wakeAtTime(AbsoluteTime).
    @param abstime Time to wake up in scheduler quantums, whatever that is?
    @result kIOReturnSuccess if everything is fine, kIOReturnNoResources if action hasn't been declared. */
    virtual IOReturn wakeAtTimeTicks(UInt32 ticks);

/*! @function wakeAtTimeMS
    @abstract Setup a callback at this absolute time.  See wakeAtTime(AbsoluteTime).
    @param abstime Time to wake up in milliseconds.
    @result kIOReturnSuccess if everything is fine, kIOReturnNoResources if action hasn't been declared. */
    virtual IOReturn wakeAtTimeMS(UInt32 ms);

/*! @function wakeAtTimeUS
    @abstract Setup a callback at this absolute time.  See wakeAtTime(AbsoluteTime).
    @param abstime Time to wake up in microseconds.
    @result kIOReturnSuccess if everything is fine, kIOReturnNoResources if action hasn't been declared. */
    virtual IOReturn wakeAtTimeUS(UInt32 us);

/*! @function wakeAtTime
    @abstract Setup a callback at this absolute time.  See wakeAtTime(AbsoluteTime).
    @param abstime Time to wake up in some unit.
    @param scale_factor Define the unit of abstime, default to nanoseconds.
    @result kIOReturnSuccess if everything is fine, kIOReturnNoResources if action hasn't been declared. */
    virtual IOReturn wakeAtTime(UInt32 abstime,
				UInt32 scale_factor = kNanosecondScale);

/*! @function wakeAtTime
    @abstract Setup a callback at this absolute time.  See wakeAtTime(AbsoluteTime).
    @param abstime mach_timespec_t of the desired callout time.
    @result kIOReturnSuccess if everything is fine, kIOReturnNoResources if action hasn't been declared. */
    virtual IOReturn wakeAtTime(mach_timespec_t abstime);

/*! @function wakeAtTime
    @abstract Setup a callback at this absolute time.
    @discussion Starts the timer, which will expire at abstime. After it expires, the timer will call the 'action' registered in the init() function. This timer is not periodic, a further call is needed to reset and restart the timer after it expires.  
    @param abstime Absolute Time when to wake up, counted in 'decrementer' units and starts at zero when system boots.
    @result kIOReturnSuccess if everything is fine, kIOReturnNoResources if action hasn't been declared by init or IOEventSource::setAction (qqv). */
    virtual IOReturn wakeAtTime(AbsoluteTime abstime);

/*! @function cancelTimeout
    @abstract Disable any outstanding calls to this event source.
    @discussion Clear down any oustanding calls.  By the time this function completes it is guaranteed that the action will not be called again. */
   virtual void cancelTimeout();
};

#endif /* !_IOTIMEREVENTSOURCE */
