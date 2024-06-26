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
/* OSDictionary.m created by rsulack on Fri 12-Sep-1997 */
/* OSDictionary.cpp converted to C++ by gvdl on Fri 1998-10-30 */
/* OSDictionary.cpp rewritten by gvdl on Fri 1998-10-30 */


#include <libkern/c++/OSDictionary.h>
#include <libkern/c++/OSArray.h>
#include <libkern/c++/OSSymbol.h>
#include <libkern/c++/OSSerialize.h>
#include <libkern/c++/OSLib.h>
#include <libkern/c++/OSCollectionIterator.h>

#define super OSCollection

OSDefineMetaClassAndStructors(OSDictionary, OSCollection)

#ifdef DEBUG
extern "C" {
    extern int debug_container_malloc_size;
};
#define ACCUMSIZE(s) do { debug_container_malloc_size += (s); } while(0)
#else
#define ACCUMSIZE(s)
#endif

bool OSDictionary::initWithCapacity(unsigned int inCapacity)
{
    if (!super::init())
        return false;

    int size = inCapacity * sizeof(dictEntry);

    dictionary = (dictEntry *) kalloc(size);
    if (!dictionary)
        return false;

    bzero(dictionary, size);
    ACCUMSIZE(size);

    count = 0;
    capacity = capacityIncrement = inCapacity;

    return true;	
}

bool OSDictionary::initWithObjects(OSObject *objects[],
                                   const OSSymbol *keys[],
                                   unsigned int theCount,
                                   unsigned int theCapacity = 0)
{
    unsigned int capacity = theCount;

    if (!objects || !keys)
        return false;

    if ( theCapacity ) {
        if (theCount > theCapacity)
            return false;
        
        capacity = theCapacity;
    }

    if (!initWithCapacity(capacity))
        return false;

    for (unsigned int i = 0; i < theCount; i++) {
        OSObject *newObject = *objects++;

        if (!newObject || !keys[i] || !setObject(keys[i], newObject))
            return false;
    }

    return true;	
}

bool OSDictionary::initWithObjects(OSObject *objects[],
                                   OSString *keys[],
                                   unsigned int theCount,
                                   unsigned int theCapacity = 0)
{
    unsigned int capacity = theCount;

    if (!objects || !keys)
        return false;

    if ( theCapacity ) {
        if (theCount > theCapacity)
            return false;

        capacity = theCapacity;
    }

    if (!initWithCapacity(capacity))
        return false;

    for (unsigned int i = 0; i < theCount; i++) {
        const OSSymbol *key = OSSymbol::withString(*keys++);
        OSObject *newObject = *objects++;

        if (!key)
            return false;

        if (!newObject || !setObject(key, newObject)) {
            key->release();
            return false;
        }

        key->release();
    }

    return true;
}

bool OSDictionary::initWithDictionary(const OSDictionary *dict,
                                      unsigned int theCapacity = 0)
{
    unsigned int capacity;

    if ( !dict )
        return false;

    capacity = dict->count;

    if ( theCapacity ) {
        if ( dict->count > theCapacity )
            return false;
        
        capacity = theCapacity;
    }

    if (!initWithCapacity(capacity))
        return false;

    count = dict->count;
    bcopy(dict->dictionary, dictionary, count * sizeof(dictEntry));
    for (unsigned int i = 0; i < count; i++) {
        dictionary[i].key->retain();
        dictionary[i].value->retain();
    }

    return true;
}

OSDictionary *OSDictionary::withCapacity(unsigned int capacity)
{
    OSDictionary *me = new OSDictionary;

    if (me && !me->initWithCapacity(capacity)) {
        me->free();
        return 0;
    }

    return me;
}

OSDictionary *OSDictionary::withObjects(OSObject *objects[],
                                        const OSSymbol *keys[],
                                        unsigned int count,
                                        unsigned int capacity = 0)
{
    OSDictionary *me = new OSDictionary;

    if (me && !me->initWithObjects(objects, keys, count, capacity)) {
        me->free();
        return 0;
    }

    return me;
}

OSDictionary *OSDictionary::withObjects(OSObject *objects[],
                                        OSString *keys[],
                                        unsigned int count,
                                        unsigned int capacity = 0)
{
    OSDictionary *me = new OSDictionary;

    if (me && !me->initWithObjects(objects, keys, count, capacity)) {
        me->free();
        return 0;
    }

    return me;
}

OSDictionary *OSDictionary::withDictionary(const OSDictionary *dict,
                                           unsigned int capacity = 0)
{
    OSDictionary *me = new OSDictionary;

    if (me && !me->initWithDictionary(dict, capacity)) {
        me->free();
        return 0;
    }

    return me;
}

void OSDictionary::free()
{
    flushCollection();
    if (dictionary) {
        kfree((vm_offset_t)dictionary, capacity * sizeof(dictEntry));
        ACCUMSIZE( -(capacity * sizeof(dictEntry)) );
    }

    super::free();
}

unsigned int OSDictionary::getCount() const { return count; }
unsigned int OSDictionary::getCapacity() const { return capacity; }

unsigned int OSDictionary::getCapacityIncrement() const
{
    return capacityIncrement;
}

unsigned int OSDictionary::setCapacityIncrement(unsigned int increment)
{
    return capacityIncrement = increment;
}

unsigned int OSDictionary::ensureCapacity(unsigned int newCapacity)
{
    dictEntry *newDict;
    int oldSize, newSize;

    if (!capacityIncrement)
        return capacity;

    newCapacity = (((newCapacity - 1) / capacityIncrement) + 1)
                * capacityIncrement;

    if (newCapacity <= capacity)
        return capacity;

    // round up
    newSize = sizeof(dictEntry) * newCapacity;

    newDict = (dictEntry *) kalloc(newSize);
    if (newDict) {
        oldSize = sizeof(dictEntry) * capacity;

        bcopy(dictionary, newDict, oldSize);
        bzero(&newDict[capacity], newSize - oldSize);

        ACCUMSIZE(newSize - oldSize);
        kfree((vm_offset_t)dictionary, oldSize);

        dictionary = newDict;
        capacity = newCapacity;
    }

    return capacity;
}

void OSDictionary::flushCollection()
{
    haveUpdated();

    for (unsigned int i = 0; i < count; i++) {
        dictionary[i].key->release();
        dictionary[i].value->release();
    }
    count = 0;
}

bool OSDictionary::setObject(const OSSymbol *aKey, OSObject *anObject)
{
    if (!anObject || !aKey)
        return false;

    // if the key exists, replace the object
    for (unsigned int i = 0; i < count; i++) {
        if (aKey == dictionary[i].key) {
            OSObject *oldObject = dictionary[i].value;

            anObject->retain();
            dictionary[i].value = anObject;

            haveUpdated();

            oldObject->release();
            return true;
        }
    }

    // add new key, possibly extending our capacity
    if (count >= capacity && count >= ensureCapacity(count+1))
        return 0;

    aKey->retain();
    anObject->retain();
    dictionary[count].key = aKey;
    dictionary[count].value = anObject;
    count++;

    haveUpdated();

    return true;
}

void OSDictionary::removeObject(const OSSymbol *aKey)
{
    if (!aKey)
        return;

    // if the key exists, remove the object
    for (unsigned int i = 0; i < count; i++)
        if (aKey == dictionary[i].key) {
            dictEntry oldEntry = dictionary[i];

            haveUpdated();

            count--;
            for (; i < count; i++)
                dictionary[i] = dictionary[i+1];

            oldEntry.key->release();
            oldEntry.value->release();
            return;
        }
}


// Returns true on success, false on an error condition.
bool OSDictionary::merge(const OSDictionary *aDictionary)
{
    const OSSymbol * sym;
    OSCollectionIterator * iter;

    if ( !OSDynamicCast(OSDictionary, (OSDictionary *) aDictionary) )
        return false;
    
    iter = OSCollectionIterator::withCollection((OSDictionary *)aDictionary);
    if ( !iter )
        return false;

    while ( (sym = (const OSSymbol *)iter->getNextObject()) ) {
        OSObject * obj;

        obj = aDictionary->getObject(sym);
        if ( !setObject(sym, obj) ) {
            iter->release();
            return false;
        }
    }
    iter->release();

    return true;
}

OSObject *OSDictionary::getObject(const OSSymbol *aKey) const
{
    if (!aKey)
        return 0;

    // if the key exists, remove the object
    for (unsigned int i = 0; i < count; i++)
        if (aKey == dictionary[i].key)
            return dictionary[i].value;

    return 0;
}

// Wrapper macros
#define OBJECT_WRAP_1(cmd, k)						\
{									\
    const OSSymbol *tmpKey = k;						\
    OSObject *retObj = cmd(tmpKey);					\
									\
    tmpKey->release();							\
    return retObj;							\
}

#define OBJECT_WRAP_2(cmd, k, o)					\
{									\
    const OSSymbol *tmpKey = k;						\
    bool ret = cmd(tmpKey, o);						\
									\
    tmpKey->release();							\
    return ret;								\
}

#define OBJECT_WRAP_3(cmd, k)						\
{									\
    const OSSymbol *tmpKey = k;						\
    cmd(tmpKey);							\
    tmpKey->release();							\
}


bool OSDictionary::setObject(const OSString *aKey, OSObject *anObject)
    OBJECT_WRAP_2(setObject, OSSymbol::withString(aKey), anObject)
bool OSDictionary::setObject(const char *aKey, OSObject *anObject)
    OBJECT_WRAP_2(setObject, OSSymbol::withCString(aKey), anObject)

OSObject *OSDictionary::getObject(const OSString *aKey) const
    OBJECT_WRAP_1(getObject, OSSymbol::withString(aKey))
OSObject *OSDictionary::getObject(const char *aKey) const
    OBJECT_WRAP_1(getObject, OSSymbol::withCString(aKey))

void OSDictionary::removeObject(const OSString *aKey)
    OBJECT_WRAP_3(removeObject, OSSymbol::withString(aKey))
void OSDictionary::removeObject(const char *aKey)
    OBJECT_WRAP_3(removeObject, OSSymbol::withCString(aKey))

bool
OSDictionary::isEqualTo(OSDictionary *aDictionary, OSCollection *keys) const
{
    OSCollectionIterator * iter;
    unsigned int keysCount;
    OSObject * obj1;
    OSObject * obj2;
    OSString * aKey;
    bool ret;

    if ( this == aDictionary )
        return true;

    keysCount = keys->getCount();
    if ( (count < keysCount) || (aDictionary->getCount() < keysCount) )
        return false;

    iter = OSCollectionIterator::withCollection(keys);
    if ( !iter )
        return false;

    ret = true;
    while ( (aKey = OSDynamicCast(OSString, iter->getNextObject())) ) {
        obj1 = getObject(aKey);
        obj2 = aDictionary->getObject(aKey);
        if ( !obj1 || !obj2 ) {
            ret = false;
            break;
        }

        if ( !obj1->isEqualTo(obj2) ) {
            ret = false;
            break;
        }
    }
    iter->release();

    return ret;
}

bool OSDictionary::isEqualTo(OSDictionary *aDictionary) const
{
    unsigned int i;
    OSObject * obj;
    
    if ( this == aDictionary )
        return true;

    if ( count != aDictionary->getCount() )
        return false;

    for ( i = 0; i < count; i++ ) {
        obj = aDictionary->getObject(dictionary[i].key);
        if ( !obj )
            return false;

        if ( !dictionary[i].value->isEqualTo(obj) )
            return false;
    }
    
    return true;
}

bool OSDictionary::isEqualTo(const OSObject *anObject) const
{
    OSDictionary *dict;

    dict = OSDynamicCast(OSDictionary, (OSObject *)anObject);
    if ( dict )
        return isEqualTo(dict);
    else
        return false;
}

unsigned int OSDictionary::iteratorSize() const
{
    return sizeof(unsigned int);
}

bool OSDictionary::initIterator(void *inIterator) const
{
    unsigned int *iteratorP = (unsigned int *) inIterator;

    *iteratorP = 0;
    return true;
}

bool OSDictionary::getNextObjectForIterator(void *inIterator, OSObject **ret) const
{
    unsigned int *iteratorP = (unsigned int *) inIterator;
    unsigned int index = (*iteratorP)++;

    if (index < count)
        *ret = (OSObject *) dictionary[index].key;
    else
        *ret = 0;

    return (*ret != 0);
}

bool OSDictionary::serialize(OSSerialize *s) const
{
    if (s->previouslySerialized(this)) return true;

    if (!s->addXMLStartTag(this, "dict")) return false;

    for (unsigned i = 0; i < count; i++) {
        const OSSymbol *key = dictionary[i].key;

        // due the nature of the XML syntax, this must be a symbol
        if (!key->metaCast("OSSymbol")) {
            return false;
        }
        if (!s->addString("<key>")) return false;
        const char *c = key->getCStringNoCopy();
	while (*c) {
	    if (*c == '<') {
		if (!s->addString("&lt;")) return false;
	    } else if (*c == '>') {
		if (!s->addString("&gt;")) return false;
	    } else if (*c == '&') {
		if (!s->addString("&amp;")) return false;
	    } else {
		if (!s->addChar(*c)) return false;
	    }
	    c++;
	}   
        if (!s->addXMLEndTag("key")) return false;

        if (!dictionary[i].value->serialize(s)) return false;
    }

    return s->addXMLEndTag("dict");
}
