/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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
/* OSBoolean.cpp created by rsulack on Tue Oct 12 1999 */

#include <libkern/c++/OSBoolean.h>
#include <libkern/c++/OSString.h>
#include <libkern/c++/OSSerialize.h>
#include <libkern/c++/OSLib.h>

#define super OSObject

OSDefineMetaClassAndStructors(OSBoolean, OSObject)

bool OSBoolean::init(bool inValue)
{
    if (!super::init())
        return false;

    value = inValue;

    return true;
}

void OSBoolean::free() { super::free(); }

OSBoolean *OSBoolean::withBoolean(bool inValue)
{
    OSBoolean *me = new OSBoolean;

    if (me && !me->init(inValue)) {
        me->free();
        return 0;
    }

    return me;
}

bool OSBoolean::isTrue() const { return value; }
bool OSBoolean::isFalse() const { return !value; }

bool OSBoolean::isEqualTo(const OSBoolean *boolean) const
{
    return (value == boolean->value);
}

bool OSBoolean::isEqualTo(const OSObject *obj) const
{
    OSBoolean *	boolean;
    if ((boolean = OSDynamicCast(OSBoolean, (OSObject *)obj )))
	return isEqualTo(boolean);
    else
	return false;
}

bool OSBoolean::serialize(OSSerialize *s) const
{
    if (s->previouslySerialized(this)) return true;

    return s->addString(value ? "<true/>" : "<false/>");
}
