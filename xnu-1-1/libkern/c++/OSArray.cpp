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
/* IOArray.m created by rsulack on Fri 12-Sep-1997 */
/* IOArray.cpp converted to C++ by gvdl on Fri 1998-10-30 */


#include <libkern/c++/OSArray.h>
#include <libkern/c++/OSSerialize.h>
#include <libkern/c++/OSLib.h>

#define super OSCollection

OSDefineMetaClassAndStructors(OSArray, OSCollection)


#ifdef DEBUG
extern "C" {
    extern int debug_container_malloc_size;
};
#define ACCUMSIZE(s) do { debug_container_malloc_size += (s); } while(0)
#else
#define ACCUMSIZE(s)
#endif

bool OSArray::initWithCapacity(unsigned int inCapacity)
{
    int size;

    if (!super::init())
        return false;

    size = sizeof(OSObject *) * inCapacity;
    array = (OSObject **) kalloc(size);
    if (!array)
        return false;

    count = 0;
    capacity = inCapacity;
    capacityIncrement = capacity;

    bzero(array, size);
    ACCUMSIZE(size);

    return this;	
}

bool OSArray::initWithObjects(OSObject *objects[],
                              unsigned int theCount,
                              unsigned int theCapacity = 0)
{
    unsigned int capacity = theCount;

    if ( theCapacity ) {
        if ( theCount > theCapacity )
            return false;

        capacity = theCapacity;
    }

    if (!objects || !initWithCapacity(capacity))
        return false;

    for ( unsigned int i = 0; i < theCount; i++ ) {
        OSObject *newObject = *objects++;

        if (!newObject)
            return false;

        array[count++] = newObject;
        newObject->retain();
    }

    return true;	
}

bool OSArray::initWithArray(const OSArray *anArray,
                            unsigned int theCapacity = 0)
{
    if ( !anArray )
        return false;

    return initWithObjects(anArray->array, anArray->count,
                           theCapacity);
}

OSArray *OSArray::withCapacity(unsigned int capacity)
{
    OSArray *me = new OSArray;

    if (me && !me->initWithCapacity(capacity)) {
        me->free();
        return 0;
    }

    return me;
}

OSArray *OSArray::withObjects(OSObject *objects[],
                              unsigned int count,
                              unsigned int capacity = 0)
{
    OSArray *me = new OSArray;

    if (me && !me->initWithObjects(objects, count, capacity)) {
        me->free();
        return 0;
    }

    return me;
}

OSArray *OSArray::withArray(const OSArray *array,
                            unsigned int capacity = 0)
{
    OSArray *me = new OSArray;

    if (me && !me->initWithArray(array, capacity)) {
        me->free();
        return 0;
    }

    return me;
}

void OSArray::free()
{
    flushCollection();

    if (array) {
        kfree((vm_offset_t)array, sizeof(OSObject *) * capacity);
        ACCUMSIZE( -(sizeof(OSObject *) * capacity) );
    }

    super::free();
}


unsigned int OSArray::getCount() const { return count; }
unsigned int OSArray::getCapacity() const { return capacity; }
unsigned int OSArray::getCapacityIncrement() const { return capacityIncrement; }
unsigned int OSArray::setCapacityIncrement(unsigned int increment)
{
    return capacityIncrement = increment;
}

unsigned int OSArray::ensureCapacity(unsigned int newCapacity)
{
    OSObject **newArray;
    int oldSize, newSize;

    if (!capacityIncrement || newCapacity <= capacity)
        return capacity;

    // round up
    newCapacity = (((newCapacity - 1) / capacityIncrement) + 1)
                * capacityIncrement;
    newSize = sizeof(OSObject *) * newCapacity;

    newArray = (OSObject **) kalloc(newSize);
    if (newArray) {
        oldSize = sizeof(OSObject *) * capacity;

        ACCUMSIZE(newSize - oldSize);

        bcopy(array, newArray, oldSize);
        bzero(&newArray[capacity], newSize - oldSize);
        kfree((vm_offset_t)array, oldSize);
        array = newArray;
        capacity = newCapacity;
    }

    return capacity;
}

void OSArray::flushCollection()
{
    unsigned int i;

    haveUpdated();
    for (i = 0; i < count; i++)
        array[i]->release();
    count = 0;
}

bool OSArray::setObject(OSObject *anObject)
{
    return setObject(count, anObject);
}

bool OSArray::setObject(unsigned int index, OSObject *anObject)
{
    unsigned int i;
    unsigned int newCount = count + 1;

    if ((index > count) || !anObject)
        return false;

    // do we need more space?
    if (newCount > capacity && newCount > ensureCapacity(newCount))
        return false;

    haveUpdated();
    if (index != count) {
        for (i = count; i > index; i--)
            array[i] = array[i-1];
    }
    array[index] = anObject;
    anObject->retain();
    count++;

    return true;
}

bool OSArray::merge(const OSArray * otherArray)
{
    unsigned int otherCount = otherArray->getCount();
    unsigned int newCount = count + otherCount;

    if (!otherCount)
        return true;

    // do we need more space?
    if (newCount > capacity && newCount > ensureCapacity(newCount))
        return false;

    haveUpdated();
    for (unsigned int i = 0; i < otherCount; i++) {
        OSObject *newObject = otherArray->getObject(i);

        array[count++] = newObject;
        newObject->retain();
    }

    return true;
}

void OSArray::replaceObject(unsigned int index, OSObject *anObject)

{
    OSObject *oldObject;

    if ((index >= count) || !anObject)
        return;

    haveUpdated();
    oldObject = array[index];
    array[index] = anObject;
    anObject->retain();

    oldObject->release();
}

void OSArray::removeObject(unsigned int index)
{
    unsigned int i;
    OSObject *oldObject;

    if (index >= count)
        return;

    haveUpdated();
    oldObject = array[index];

    count--;
    for (i = index; i < count; i++)
        array[i] = array[i+1];

    oldObject->release();
}

bool OSArray::isEqualTo(OSArray *anArray) const
{
    unsigned int i;
    
    if ( this == anArray )
        return true;
    
    if ( count != anArray->getCount() )
        return false;

    for ( i = 0; i < count; i++ ) {
        if ( !array[i]->isEqualTo(anArray->getObject(i)) )
            return false;
    }

    return true;
}

bool OSArray::isEqualTo(const OSObject *anObject) const
{
    OSArray *otherArray;

    otherArray = OSDynamicCast(OSArray, (OSObject *)anObject);
    if ( otherArray )
        return isEqualTo(otherArray);
    else
        return false;
}

OSObject *OSArray::getObject(unsigned int index) const
{
    if (index >= count)
        return 0;
    else
        return array[index];
}

OSObject *OSArray::lastObject() const
{
    if (count == 0)
        return 0;
    else
        return array[count - 1];
}

unsigned int OSArray::getNextIndexOfObject(OSObject * anObject,
                                            unsigned int index) const
{
    while ((index < count) && (array[index] != anObject))
        {}
    if (index >= count)
        index = (unsigned int)-1;
    return index;
}

unsigned int OSArray::iteratorSize() const
{
    return sizeof(unsigned int);
}

bool OSArray::initIterator(void *inIterator) const
{
    unsigned int *iteratorP = (unsigned int *) inIterator;

    *iteratorP = 0;
    return true;
}

bool OSArray::getNextObjectForIterator(void *inIterator, OSObject **ret) const
{
    unsigned int *iteratorP = (unsigned int *) inIterator;
    unsigned int index = (*iteratorP)++;

    if (index < count) {
        *ret = array[index];
        return true;
    }
    else {
        *ret = 0;
        return false;
    }
}

bool OSArray::serialize(OSSerialize *s) const
{
    if (s->previouslySerialized(this)) return true;
    
    if (!s->addXMLStartTag(this, "array")) return false;

    for (unsigned i = 0; i < count; i++) { 
        if (!array[i]->serialize(s)) return false;
    }

    return s->addXMLEndTag("array");
}
