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
 * IONetworkStack.h - An IOKit proxy for the BSD network stack.
 *
 * HISTORY
 *
 */

#ifndef _IONETWORKSTACK_H
#define _IONETWORKSTACK_H

#include <libkern/c++/OSString.h>

class IONetworkInterface;

class IONetworkStack : public IOService
{
    OSDeclareDefaultStructors(IONetworkStack)

private:
    IONetworkInterface *    _netif;     // provider.
    struct ifnet *          _ifp;       // provider's ifnet pointer.
    IOLock *                _lock;      // serialization lock.

    enum {
        kIONetworkStackStateInit,
        kIONetworkStackStateAttached,
        kIONetworkStackStateDetaching,
        kIONetworkStackStateDetached,
    } _state;

    OSString * _assignInterfaceName(IONetworkInterface * netif);

    static void initialize();

    static void ifDetachCallbackHandler(struct ifnet * ifp);

protected:
    virtual void free();    

    virtual bool sendIfDetachRequest();
    virtual void ifDetachCallback();

public:
    virtual bool init(OSDictionary * properties);

    virtual IOService * probe(IOService * provider, SInt32 * score);
    
    virtual bool start(IOService * provider);
    
    virtual void stop(IOService * provider);
    
    virtual IOReturn message(UInt32 type, IOService * provider,
                             void * argument = 0);
};

#endif /* !_IONETWORKSTACK_H */
