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
Copyright (c) 1998 Apple Computer, Inc.	 All rights reserved.
HISTORY
    1998-7-13	Godfrey van der Linden(gvdl)
	Created.
    1998-10-30	Godfrey van der Linden(gvdl)
	Converted to C++
*/
#ifndef _IOKIT_IOEVENTSOURCE_H
#define _IOKIT_IOEVENTSOURCE_H

#include <sys/cdefs.h>

#include <libkern/c++/OSObject.h>

#include <IOKit/IOLib.h>
#include <IOKit/system.h>

__BEGIN_DECLS
#include <mach/clock_types.h>
#include <kern/clock.h>
__END_DECLS

class IOWorkLoop;

/*!
    @class IOEventSource : public OSObject
    @abstract Abstract class for all work-loop event sources.
    @discussion The IOEventSource declares the abstract super class that all
event sources must inherit from if an IOWorkLoop is to receive events from them.
<br><br>
	An event source can represent any event that should cause the work-loop of a
device to wake up and perform work.  Two examples of event sources are the
IOInterruptEventSource which delivers interrupt notifications and IOCommandGate
which delivers command requests.
<br><br>
	A kernel module can always use the work-loop model for serialising access to
anything at all.  The IOEventSource is used for communicating events to the
work-loop, and the chain of event sources should be used to walk the possible
event sources and demultipex them.  Note a particular instance of an event
source may only be a member of 1 linked list chain.  If you need to move it
between chains than make sure it is removed from the original chain before
attempting to move it.
<br><br>
	The IOEventSource makes no attempt to maintain the consitency of it's internal data across multi-threading.  It is assumed that the user of these basic tools will protect the data that these objects represent in some sort of device wide instance lock.	For example the IOWorkLoop maintains the event chain by handing off change request to its own thread and thus single threading access to its state.
<br><br>
	All subclasses of the IOEventSource are expected to implement the checkForWork() member function.

<br><br>
	checkForWork() is the key method in this class.	 It is called by some work-loop when convienient and is expected to evaluate it's internal state and determine if an event has occured since the last call.  In the case of an event having occurred then the instance defined target(owner)/action will be called.	 The action is stored as an ordinary C function pointer but the first parameter is always the owner.  This means that a C++ member function can be used as an action function though this depends on the ABI.
<br><br>
	Although the eventChainNext variable contains a reference to the next event source in the chain this reference is not retained.  The list 'owner' i.e. the client that creates the event, not the work-loop, is expected to retain the source.
*/
class IOEventSource : public OSObject
{
    OSDeclareAbstractStructors(IOEventSource)
    friend class IOWorkLoop;

public:
/*!
    @typedef Action
    @discussion Placeholder type for C++ function overloading discrimination.
As the all event sources require an action and it has to be stored somewhere
and be of some type, this is that type.
    @param owner
	Target of the function, can be used as a refcon.  The owner is set
during initialisation.	 Note if a C++ function was specified this parameter
is implicitly the first paramter in the target member function's parameter list.
*/
    typedef void (*Action)(OSObject *owner, ...);

/*! @defined IOEventSourceAction
    @discussion Backward compatibilty define for the old non-class scoped type definition.  See $link IOEventSource::Action */
 #define IOEventSourceAction IOEventSource::Action

protected:
/*! @var eventChainNext
	The next event source in the event chain. nil at end of chain. */
    IOEventSource *eventChainNext;

/*! @var owner The owner object called when an event has been delivered. */
    OSObject *owner;

/*! @var action
	The action method called when an event has been delivered */
    Action action;

/*! @var enabled
	Is this event source enabled to deliver requests to the work-loop. */
    bool enabled;

/*! @var workLoop What is the work-loop for this event source. */
    IOWorkLoop *workLoop;

/*! @var refcon What ever the client wants to do, see $link setRefcon. */
    void *refcon;

/*! @function init
    @abstract Primary initialiser for the IOEventSource class.
    @param owner
	Owner of this instance of an event source.  Used as the first parameter
of the action callout.	Owner will generally be an OSObject it doesn't have to
be as no member functions will be called directly in it.  It can just be a
refcon for a client routine.
    @param action
	Pointer to C call out function.	 Action is a pointer to a C function
that gets called when this event source has outstanding work.  It will usually
be called by the checkForWork member function.	The first parameter of the
action call out will always be the owner, this allows C++ member functions to
be used as actions.  Defaults to 0.
    @result true if the inherited classes and this instance initialise
successfully.
*/
    virtual bool init(OSObject *owner, IOEventSource::Action action = 0);

/*! @function checkForWork
    @abstract Pure Virtual member function used by IOWorkLoop for work
scheduling.
    @discussion This function will be called to request a subclass to check
it's internal state for any work to do and then to call out the owner/action.
    @result Return true if this function needs to be called again before all its outstanding events have been processed.
        */
    virtual bool checkForWork() = 0;

/*! @function setWorkLoop
    @abstract Set'ter for $link workLoop variable.
    @param workLoop
	Target work-loop of this event source instance.	 A subclass of
IOWorkLoop that at least reacts to signalWorkAvailable() and onThread functions. 
*/
    virtual void setWorkLoop(IOWorkLoop *workLoop);

/*! @function setNext
    @abstract Set'ter for $link eventChainNext variable.
    @param next
	Pointer to another IOEventSource instance.
*/
    virtual void setNext(IOEventSource *next);

/*! @function getNext
    @abstract Get'ter for $link eventChainNext variable.
    @result value of eventChainNext.
*/
    virtual IOEventSource *getNext() const;

public:
/*! @function setAction
    @abstract Set'ter for $link action variable.
    @param action Pointer to a C function of type IOEventSource::Action. */
    virtual void setAction(IOEventSource::Action action);

/*! @function getAction
    @abstract Get'ter for $link action variable.
    @result value of action. */
    virtual IOEventSource::Action getAction() const;

/*! @function enable
    @abstract Enable event source.
    @discussion A subclass implementation is expected to respect the enabled
state when checkForWork is called.  Calling this function will cause the
work-loop to be signalled so that a checkForWork is performed. */
    virtual void enable();

/*! @function disable
    @abstract Disable event source.
    @discussion A subclass implementation is expected to respect the enabled
state when checkForWork is called. */
    virtual void disable();

/*! @function isEnabled
    @abstract Get'ter for $link enable variable.
    @result true if enabled. */
    virtual bool isEnabled() const;

/*! @function getWorkLoop
    @abstract Get'ter for $link workLoop variable.
    @result value of workLoop. */
    virtual IOWorkLoop *getWorkLoop() const;

/*! @function onThread
    @abstract Convenience function for workLoop->onThread.
    @result true if called on the work-loop thread.
*/
    virtual bool onThread() const;
};

#endif /* !_IOKIT_IOEVENTSOURCE_H */
