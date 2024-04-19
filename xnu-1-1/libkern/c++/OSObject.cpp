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
/* OSObject.cpp created by gvdl on Fri 1998-11-17 */

#include <libkern/c++/OSObject.h>
#include <libkern/c++/OSSerialize.h>
#include <libkern/c++/OSLib.h>
#include <libkern/c++/OSCPPDebug.h>
#include <libkern/OSAtomic.h>

__BEGIN_DECLS
int debug_ivars_size;
__END_DECLS

#ifdef DEBUG
#define ACCUMSIZE(s) do { debug_ivars_size += (s); } while(0)
#else
#define ACCUMSIZE(s)
#endif

/* MetaClass bootstrap for the Top level object */
class	OSObjectMetaClass : public OSMetaClass {
public:
    OSObjectMetaClass() : OSMetaClass("OSObject", "", sizeof(OSObject)) { };
    virtual OSObject *alloc() const;
};

OSObject *OSObjectMetaClass::alloc() const { return 0; }

static OSObjectMetaClass sOSObjectMetaClass;

const OSMetaClass * const
OSObject::metaClass = &sOSObjectMetaClass;
const OSMetaClass *
OSObject::getMetaClass() const { return &sOSObjectMetaClass; }

OSObject::OSObject()
{
    retainCount = 1;
}

OSObject::OSObject(const OSMetaClass *)
{
    retainCount = 1;
}

OSObject::~OSObject()
{
    void **thisVTable;

    thisVTable = (void **)
		    ((char *) this + sizeof(OSObject) - sizeof(thisVTable));
    *thisVTable = (void *) -1UL;
}

bool OSObject::init()
{
    return true;
}

void OSObject::free()
{
    const OSMetaClass *meta = getMetaClass();

    if (meta)
	meta->instanceDestructed();
    delete this;
}

OSObject *OSObject::metaCast(const OSMetaClass *toMeta) const
{
    return toMeta->checkMetaCast((OSObject *) this);
}

OSObject *OSObject::metaCast(const OSSymbol *toMetaSymb) const
{
    return OSMetaClass::checkMetaCastWithName(toMetaSymb, this);
}

OSObject *OSObject::metaCast(const OSString *toMetaStr) const
{
    return OSMetaClass::checkMetaCastWithName(toMetaStr, this);
}

OSObject *OSObject::metaCast(const char *toMetaCStr) const
{
    return OSMetaClass::checkMetaCastWithName(toMetaCStr, this);
}

int OSObject::getRetainCount() const
{
    return retainCount;
}

void OSObject::retain() const
{
    OSIncrementAtomic((SInt32 *)&(((OSObject *) this)->retainCount));
}

void OSObject::release(int when) const
{
    if (OSDecrementAtomic((SInt32 *)&(((OSObject *) this)->retainCount)) <= when)
	((OSObject *) this)->free();
}

void OSObject::release() const
{
    release(1);
}

bool OSObject::isEqualTo(const OSObject *anObj) const
{
    return this == anObj;
}

bool OSObject::serialize(OSSerialize *s) const
{
    if (s->previouslySerialized(this)) return true;

    if (!s->addXMLStartTag(this, "string")) return false;

    const OSMetaClass *meta = getMetaClass();
    if (meta) {
	if (!s->addString(meta->getClassName())) return false;
    } else {
	if (!s->addString("unknown class ?")) return false;
    }
    if (!s->addString(" is not serializable")) return false;
    
    return s->addXMLEndTag("string");
}

void *OSObject::operator new(size_t size)
{
    void *mem = (void *)kalloc(size);
    assert(mem);
    bzero(mem, size);

    ACCUMSIZE(size);

    return mem;
}

void OSObject::operator delete(void *mem, size_t size)
{
    kfree((vm_offset_t)mem, size);

    ACCUMSIZE(-size);
}
