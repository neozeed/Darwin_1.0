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

#include <libkern/c++/OSOrderedSet.h>
#include <libkern/c++/OSLib.h>

#define super OSCollection

OSDefineMetaClassAndStructors(OSOrderedSet, OSCollection)

#ifdef DEBUG
extern "C" {
    extern int debug_container_malloc_size;
};
#define ACCUMSIZE(s) do { debug_container_malloc_size += (s); } while(0)
#else
#define ACCUMSIZE(s)
#endif

struct _Element {
    OSObject *		obj;
//    unsigned int	pri;
};


bool OSOrderedSet::initWithCapacity(unsigned int inCapacity,
                                OSOrderFunction ordering, void * orderingRef)
{
    int size;

    if (!super::init())
        return false;

    size = sizeof(_Element) * inCapacity;
    array = (_Element *) kalloc(size);
    if (!array)
        return false;

    count = 0;
    capacity = inCapacity;
    capacityIncrement = capacity;
    this->ordering = ordering;
    this->orderingRef = orderingRef;

    bzero(array, size);
    ACCUMSIZE(size);

    return this;	
}

OSOrderedSet * OSOrderedSet::withCapacity(unsigned int capacity,
                                OSOrderFunction ordering, void * orderingRef)
{
    OSOrderedSet *me = new OSOrderedSet;

    if (me && !me->initWithCapacity(capacity, ordering, orderingRef)) {
        me->free();
	me = 0;
    }

    return me;
}

void OSOrderedSet::free()
{
    flushCollection();

    if (array) {
        kfree((vm_offset_t)array, sizeof(_Element) * capacity);
        ACCUMSIZE( -(sizeof(_Element) * capacity) );
    }

    super::free();
}

unsigned int OSOrderedSet::getCount() const { return count; }
unsigned int OSOrderedSet::getCapacity() const { return capacity; }
unsigned int OSOrderedSet::getCapacityIncrement() const
	{ return capacityIncrement; }
unsigned int OSOrderedSet::setCapacityIncrement(unsigned int increment)
{
    return capacityIncrement = increment;
}

unsigned int OSOrderedSet::ensureCapacity(unsigned int newCapacity)
{
    _Element *newArray;
    int oldSize, newSize;

    if (!capacityIncrement || newCapacity <= capacity)
        return capacity;

    // round up
    newCapacity = (((newCapacity - 1) / capacityIncrement) + 1)
                * capacityIncrement;
    newSize = sizeof(_Element) * newCapacity;

    newArray = (_Element *) kalloc(newSize);
    if (newArray) {
        oldSize = sizeof(_Element) * capacity;

        ACCUMSIZE(newSize - oldSize);

        bcopy(array, newArray, oldSize);
        bzero(&newArray[capacity], newSize - oldSize);
        kfree((vm_offset_t)array, oldSize);
        array = newArray;
        capacity = newCapacity;
    }

    return capacity;
}

void OSOrderedSet::flushCollection()
{
    unsigned int i;

    haveUpdated();

    for (i = 0; i < count; i++)
        array[i].obj->release();

    count = 0;
}

/* internal */
bool OSOrderedSet::setObject(unsigned int index, OSObject *anObject)
{
    unsigned int i;
    unsigned int newCount = count + 1;

    if ((index > count) || !anObject)
        return false;

    if (containsObject(anObject))
        return false;

    // do we need more space?
    if (newCount > capacity && newCount > ensureCapacity(newCount))
        return false;

    haveUpdated();
    if (index != count) {
        for (i = count; i > index; i--)
            array[i] = array[i-1];
    }
    array[index].obj = anObject;
//    array[index].pri = pri;
    anObject->retain();
    count++;

    return true;
}


bool OSOrderedSet::setFirstObject(OSObject *anObject)
{
    return( setObject(0, anObject));
}

bool OSOrderedSet::setLastObject(OSObject *anObject)
{
    return( setObject( count, anObject));
}


#define ORDER(obj1,obj2) \
    (ordering ? ((*ordering)( obj1, obj2, orderingRef)) : 0)

bool OSOrderedSet::setObject(OSObject *anObject )
{
    unsigned int i;

    // queue it behind those with same priority
    for( i = 0;
	(i < count) && (ORDER(array[i].obj, anObject) >= 0);
	i++ ) {}

    return( setObject(i, anObject));
}

void OSOrderedSet::removeObject(OSObject *anObject)
{
    bool		deleted = false;
    unsigned int 	i;

    for (i = 0; i < count; i++) {

        if( deleted)
            array[i-1] = array[i];
        else if( (array[i].obj == anObject)) {
            array[i].obj->release();
            deleted = true;
        }
    }

    if( deleted) {
        count--;
        haveUpdated();
    }
}

bool OSOrderedSet::containsObject(const OSObject *anObject) const
{
    return anObject && member(anObject);
}

bool OSOrderedSet::member(const OSObject *anObject) const
{
    unsigned int i;

    for( i = 0;
	(i < count) && (array[i].obj != anObject);
	i++ ) {}

    return( i < count);
}

/* internal */
OSObject *OSOrderedSet::getObject( unsigned int index ) const
{
    if (index >= count)
        return 0;

//    if( pri)
//	*pri = array[index].pri;

    return( array[index].obj );
}

OSObject *OSOrderedSet::getFirstObject() const
{
    if( count)
        return( array[0].obj );
    else
	return( 0 );
}

OSObject *OSOrderedSet::getLastObject() const
{
    if( count)
        return( array[count-1].obj );
    else
	return( 0 );
}

SInt32 OSOrderedSet::orderObject( OSObject * anObject )
{
    return( ORDER( anObject, 0 ));
}

void *OSOrderedSet::getOrderingRef()
{
    return orderingRef;
}

bool OSOrderedSet::isEqualTo(OSOrderedSet *anOrderedSet) const
{
    unsigned int i;
    
    if ( this == anOrderedSet )
        return true;

    if ( count != anOrderedSet->getCount() )
        return false;

    for ( i = 0; i < count; i++ ) {
        if ( !array[i].obj->isEqualTo(anOrderedSet->getObject(i)) )
            return false;
    }

    return true;
}

bool OSOrderedSet::isEqualTo(const OSObject *anObject) const
{
    OSOrderedSet *oSet;

    oSet = OSDynamicCast(OSOrderedSet, (OSObject *)anObject);
    if ( oSet )
        return isEqualTo(oSet);
    else
        return false;
}

unsigned int OSOrderedSet::iteratorSize() const
{
    return( sizeof(unsigned int));
}

bool OSOrderedSet::initIterator(void *inIterator) const
{
    unsigned int *iteratorP = (unsigned int *) inIterator;

    *iteratorP = 0;
    return true;
}

bool OSOrderedSet::getNextObjectForIterator(void *inIterator, OSObject **ret) const
{
    unsigned int *iteratorP = (unsigned int *) inIterator;
    unsigned int index = (*iteratorP)++;

    if (index < count)
        *ret = array[index].obj;
    else
        *ret = 0;

    return (*ret != 0);
}

