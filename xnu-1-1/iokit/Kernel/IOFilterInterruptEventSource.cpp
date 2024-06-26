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
Copyright (c) 1999 Apple Computer, Inc.  All rights reserved.

HISTORY
    1999-4-15	Godfrey van der Linden(gvdl)
        Created.
*/
#include <IOKit/IOFilterInterruptEventSource.h>
#include <IOKit/IOService.h>
#include <IOKit/IOTimeStamp.h>
#include <IOKit/IOWorkLoop.h>

#if KDEBUG

#define IOTimeTypeStampS(t)						\
do {									\
    IOTimeStampStart(IODBG_INTES(t),					\
                     (unsigned int) this, (unsigned int) owner);	\
} while(0)

#define IOTimeTypeStampE(t)						\
do {									\
    IOTimeStampEnd(IODBG_INTES(t),					\
                   (unsigned int) this, (unsigned int) owner);		\
} while(0)

#define IOTimeStampLatency()						\
do {									\
    IOTimeStampEnd(IODBG_INTES(IOINTES_LAT),				\
                   (unsigned int) this, (unsigned int) owner);		\
} while(0)

#else /* !KDEBUG */
#define IOTimeTypeStampS(t)
#define IOTimeTypeStampE(t)
#define IOTimeStampLatency()
#endif /* KDEBUG */

#define super IOInterruptEventSource

OSDefineMetaClassAndStructors
    (IOFilterInterruptEventSource, IOInterruptEventSource)

/*
 * Implement the call throughs for the private protection conversion
 */
bool IOFilterInterruptEventSource::init(OSObject *inOwner,
                                        Action inAction = 0,
                                        IOService *inProvider = 0,
                                        int inIntIndex = 0)
{
    return false;
}

IOInterruptEventSource *
IOFilterInterruptEventSource::interruptEventSource(OSObject *inOwner,
                                                   Action inAction,
                                                   IOService *inProvider,
                                                   int inIntIndex)
{
    return 0;
}

bool
IOFilterInterruptEventSource::init(OSObject *inOwner,
                                   Action inAction,
                                   Filter inFilterAction,
                                   IOService *inProvider,
                                   int inIntIndex = 0)
{
    if ( !super::init(inOwner, inAction, inProvider, inIntIndex) )
        return false;

    if (!inFilterAction)
        return false;

    filterAction = inFilterAction;
    return true;
}

IOFilterInterruptEventSource *IOFilterInterruptEventSource
::filterInterruptEventSource(OSObject *inOwner,
                             Action inAction,
                             Filter inFilterAction,
                             IOService *inProvider,
                             int inIntIndex = 0)
{
    IOFilterInterruptEventSource *me = new IOFilterInterruptEventSource;

    if (me
    && !me->init(inOwner, inAction, inFilterAction, inProvider, inIntIndex)) {
        me->free();
        return 0;
    }

    return me;
}

IOFilterInterruptEventSource::Filter
IOFilterInterruptEventSource::getFilterAction() const
{
    return filterAction;
}




void IOFilterInterruptEventSource::normalInterruptOccurred
    (void */*refcon*/, IOService */*prov*/, int /*source*/)
{
    bool filterRes;

IOTimeTypeStampS(IOINTES_INTCTXT);

IOTimeTypeStampS(IOINTES_INTFLTR);
    IOTimeStampConstant(IODBG_INTES(IOINTES_FILTER),
                        (unsigned int) filterAction, (unsigned int) owner);
    filterRes = (*filterAction)(owner, this);
IOTimeTypeStampE(IOINTES_INTFLTR);

    if (filterRes) {
    IOTimeStampLatency();

        producerCount++;

    IOTimeTypeStampS(IOINTES_SEMA);
        workLoop->signalWorkAvailable();
    IOTimeTypeStampE(IOINTES_SEMA);

    }
IOTimeTypeStampE(IOINTES_INTCTXT);
}

void IOFilterInterruptEventSource::disableInterruptOccurred
    (void */*refcon*/, IOService *prov, int source)
{
    bool filterRes;

IOTimeTypeStampS(IOINTES_INTCTXT);

IOTimeTypeStampS(IOINTES_INTFLTR);
    IOTimeStampConstant(IODBG_INTES(IOINTES_FILTER),
                        (unsigned int) filterAction, (unsigned int) owner);
    filterRes = (*filterAction)(owner, this);
IOTimeTypeStampE(IOINTES_INTFLTR);

    if (filterRes) {
    IOTimeStampLatency();

        prov->disableInterrupt(source);	/* disable the interrupt */

        producerCount++;

    IOTimeTypeStampS(IOINTES_SEMA);
        workLoop->signalWorkAvailable();
    IOTimeTypeStampE(IOINTES_SEMA);

    }
IOTimeTypeStampE(IOINTES_INTCTXT);
}
