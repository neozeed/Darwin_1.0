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
Copyright (c) 1998 Apple Computer, Inc.  All rights reserved.

HISTORY
    1998-7-13	Godfrey van der Linden(gvdl)
        Created.
*/
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOEventSource.h>
#include <IOKit/IOInterruptEventSource.h>
#include <IOKit/IOCommandGate.h>
#include <IOKit/IOTimeStamp.h>

#define super OSObject

OSDefineMetaClassAndStructors(IOWorkLoop, OSObject)

void IOWorkLoop::launchThreadMain(void *self)
{
    IOWorkLoop *me = (IOWorkLoop *) self;
    register thread_t mythread = current_thread();

    // Make sure that this thread always has a kernel stack
    stack_privilege(mythread);
    thread_set_cont_arg((int) me);
    threadMainContinuation();
}

bool IOWorkLoop::init()
{
    // The super init and gateLock allocation MUST be done first
    if ( !super::init() || !(gateLock = IORecursiveLockAlloc()) )
        return false;

    if ( !(workToDoLock = IOSimpleLockAlloc()) )
        return false;
    IOSimpleLockInit(workToDoLock);
    workToDo = false;

    controlG = IOCommandGate::commandGate(this,
                                          (IOCommandGate::Action) &IOWorkLoop::_maintRequest);
    if ( !controlG )
        return false;

    // Point the controlGate at the workLoop.  Usually addEventSource
    // does this automatically.  The problem is in this case addEventSource
    // uses the control gate and it has to be bootstrapped.
    controlG->setWorkLoop(this);
    if (addEventSource(controlG) != kIOReturnSuccess)
        return false;

    workThread = IOCreateThread(launchThreadMain, (void *) this);
    if (!workThread)
        return false;

    return true;
}

IOWorkLoop *
IOWorkLoop::workLoop()
{
    IOWorkLoop *me = new IOWorkLoop;

    if (me && !me->init()) {
        me->free();
        return 0;
    }

    return me;
}

void IOWorkLoop::free()
{
    // Hmm, we didn't even acquire the gate lock succesfully...
    if (!gateLock) {
	super::free();	// ... so just issue a super free
	return;		// and return immediately
    }

    if (workThread && !onThread())
        closeGate();

    if (controlG) {
        IOCommandGate *tmpControlG = controlG;
        controlG = 0;
        tmpControlG->release();
    }

    disableAllEventSources();

    // Partial initialisation no thread yet so get rid of event sources.
    if (!workThread && eventChain) {
        IOEventSource *event, *next;

        for (event = eventChain; event; event = next) {
            next = event->getNext();
            event->release();
        }
        eventChain = 0;
    }

    // This is safe as we have disabled all of our interrupt event sources.
    if (workToDoLock) {
        IOSimpleLockFree(workToDoLock);
        workToDoLock = 0;
    }

    //
    // Three cases:
    // 1> If I don't have a work thread yet just delete the command gate and
    // free the super class
    // 2> If I have a work loop and I'm on the thread, then one of the event
    // sources must be issueing a free request.  In which case we issue a
    // loop restart request and return for the main loop to detect.
    // 3> Client free request.  The workToDoLock being set to 0 is a signal
    // for the main thread to wake up and kill itself.
    // 
    if (!workThread) {
	IORecursiveLockFree(gateLock);
        super::free();
    }
    else if (onThread())
        loopRestart = true;
    else {
        thread_wakeup_one((void *) &workToDo);	// Wakeup any sleepers
        openGate();
    }
}

IOReturn IOWorkLoop::addEventSource(IOEventSource *newEvent)
{
    return controlG->runCommand((void *) mAddEvent, (void *) newEvent);
}
    
IOReturn IOWorkLoop::removeEventSource(IOEventSource *toRemove)
{
    return controlG->runCommand((void *) mRemoveEvent, (void *) toRemove);
}

void IOWorkLoop::enableAllEventSources() const
{
    IOEventSource *event;

    for (event = eventChain; event; event = event->getNext())
        event->enable();
}

void IOWorkLoop::disableAllEventSources() const
{
    IOEventSource *event;

    for (event = eventChain; event; event = event->getNext())
        event->disable();
}

void IOWorkLoop::enableAllInterrupts() const
{
    IOEventSource *event;

    for (event = eventChain; event; event = event->getNext())
        if (OSDynamicCast(IOInterruptEventSource, event))
            event->enable();
}

void IOWorkLoop::disableAllInterrupts() const
{
    IOEventSource *event;

    for (event = eventChain; event; event = event->getNext())
        if (OSDynamicCast(IOInterruptEventSource, event))
            event->disable();
}

#if KDEBUG
#define IOTimeClientS()							\
do {									\
    IOTimeStampStart(IODBG_WORKLOOP(IOWL_CLIENT),			\
                     (unsigned int) this, (unsigned int) event);	\
} while(0)

#define IOTimeClientE()							\
do {									\
    IOTimeStampEnd(IODBG_WORKLOOP(IOWL_CLIENT),				\
                   (unsigned int) this, (unsigned int) event);		\
} while(0)

#define IOTimeWorkS()							\
do {									\
    IOTimeStampStart(IODBG_WORKLOOP(IOWL_WORK),	(unsigned int) this);	\
} while(0)

#define IOTimeWorkE()							\
do {									\
    IOTimeStampEnd(IODBG_WORKLOOP(IOWL_WORK),(unsigned int) this);	\
} while(0)

#else /* !KDEBUG */

#define IOTimeClientS()
#define IOTimeClientE()
#define IOTimeWorkS()
#define IOTimeWorkE()

#endif /* KDEBUG */

void IOWorkLoop::threadMainContinuation()
{
  IOWorkLoop* self;
  self = (IOWorkLoop *) thread_get_cont_arg();

  self->threadMain();
}

void IOWorkLoop::threadMain()
{
    loopRestart = false;

    for (;;) {
        bool more;

    IOTimeWorkS();

        closeGate();
        if (!workToDoLock)
            goto exitThread;

        do {
            IOEventSource *event;

restartLoop:

            workToDo = more = false;
            for (event = eventChain; event; event = event->getNext()) {

            IOTimeClientS();
                more |= event->checkForWork();
            IOTimeClientE();

                if (!workToDoLock)
                    goto exitThread;
                else if (loopRestart) {
                    loopRestart = false;

                    goto restartLoop;
                }
            }
        } while (more);

    IOTimeWorkE();

        openGate();

        if (workToDoLock) {
            IOInterruptState is;

            is = IOSimpleLockLockDisableInterrupt(workToDoLock);
            if (!workToDo) {
                assert_wait((void *) &workToDo, false);
                IOSimpleLockUnlockEnableInterrupt(workToDoLock, is);
                thread_block(&threadMainContinuation);
                /* NOTREACHED */
            }
            else
                IOSimpleLockUnlockEnableInterrupt(workToDoLock, is);
        }

        if (!workToDoLock) {
            IOEventSource *event, *next;
exitThread:

            for (event = eventChain; event; event = next) {
                next = event->getNext();
                event->release();
            }
            eventChain = 0;

	    IORecursiveLockFree(gateLock);
            super::free();
            IOExitThread(0);
        }
    }
}

IOThread IOWorkLoop::getThread() const
{
    return workThread;
}

bool IOWorkLoop::onThread() const
{
    return (IOThreadSelf() == workThread);
}

bool IOWorkLoop::inGate() const
{
    return IORecursiveLockHaveLock(gateLock);
}

// Internal APIs used by event sources to control the thread
void IOWorkLoop::signalWorkAvailable()
{
    if (workToDoLock) {
        IOInterruptState is = IOSimpleLockLockDisableInterrupt(workToDoLock);
        workToDo = true;
        thread_wakeup_one((void *) &workToDo);
        IOSimpleLockUnlockEnableInterrupt(workToDoLock, is);
    }
}

void IOWorkLoop::openGate()
{
    IORecursiveLockUnlock(gateLock);
}

void IOWorkLoop::closeGate()
{
    IORecursiveLockLock(gateLock);
}

IOReturn IOWorkLoop::_maintRequest(void *inC, void *inD, void *, void *)
{
    maintCommandEnum command = (maintCommandEnum) (vm_address_t) inC;
    IOEventSource *inEvent = (IOEventSource *) inD;
    IOReturn res = kIOReturnSuccess;

    switch (command)
    {
    case mAddEvent:
        loopRestart = true;
        inEvent->retain();
        inEvent->setWorkLoop(this);
        inEvent->setNext(0);

        if (!eventChain)
            eventChain = inEvent;
        else {
            IOEventSource *event, *next;

            for (event = eventChain; (next = event->getNext()); event = next)
                ;
            event->setNext(inEvent);
        }
        break;

    case mRemoveEvent:
        if (eventChain == inEvent)
            eventChain = inEvent->getNext();
        else {
            IOEventSource *event, *next;

            event = eventChain;
            while ((next = event->getNext()) && next != inEvent)
                event = next;

            if (!next) {
                res = kIOReturnBadArgument;
                break;
            }
            event->setNext(inEvent->getNext());
        }

        inEvent->setWorkLoop(0);
        inEvent->setNext(0);
        inEvent->release();
        loopRestart = true;
        break;

    default:
        return kIOReturnUnsupported;
    }

    return res;
}
