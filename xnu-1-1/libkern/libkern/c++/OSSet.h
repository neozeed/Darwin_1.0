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
/* IOSet.h created by rsulack on Thu 11-Jun-1998 */
/* IOSet.h converted to C++ by gvdl on Fri 1998-10-30 */

#ifndef _OS_OSSET_H
#define _OS_OSSET_H

#include <libkern/c++/OSCollection.h>

class OSArray;

/*!
    @class OSSet
    @abstract A collection class for storing OSObject derived objects.
    @discussion
    Instances of OSSet store unique OSObject derived objects in a non-ordered manner.
*/
class OSSet : public OSCollection
{
    OSDeclareDefaultStructors(OSSet)

private:
    OSArray *members;

protected:
    /*
     * OSCollectionIterator interfaces.
     */
    virtual unsigned int iteratorSize() const;
    virtual bool initIterator(void *iterator) const;
    virtual bool getNextObjectForIterator(void *iterator, OSObject **ret) const;

public:
    /*!
        @function withCapacity
        @abstract A static constructor function to create and initialize an instance of OSSet with a given capacity.
        @param capacity The initial capacity of the collection. The capacity is the total number of objects that can be stored in the collection.
        @result Returns an instance of OSSet or 0 on failure.
    */
    static OSSet *withCapacity(unsigned int capacity);
    /*!
        @function withObjects
        @abstract A static constructor function to create and initialize an instance of OSSet and populate it with the objects provied.
        @param objects A static array of OSObject derived objects which are used to populate the collection.
        @param count The number of objects passed to the collection.
        @param capacity The initial storage size of the collection.  The capacity is the total number of objects that can be stored in the collection.  This value must be equal to or larger than the count parameter.
        @result Returns an instance of OSSet or 0 on failure.
    */
    static OSSet *withObjects(OSObject *objects[],
                                 unsigned int count,
                                 unsigned int capacity = 0);
    /*!
        @function withArray
        @abstract A static constructor function to create and initialize an instance of OSSet and populate it with the objects from an OSSArray obeject.
        @param array An OSArray object containing a list of OSObject derived objects which are used to initially populate the OSSet object.
        @param capacity The initial storage size of the collection.  This value must be equal to or larger than the number of objects provided by the OSArray object passed as the first parameter.
        @result Returns an instance of OSSet or 0 on failure.
    */
    static OSSet *withArray(const OSArray *array,
                            unsigned int capacity = 0);
    /*!
        @function withSet
        @abstract A static constructor function to create an instance of OSSet and populate it with the objects from another OSSet obeject.
        @param array An OSSet object containing OSObject derived objects which are used to initially populate the new OSSet object.
        @param capacity The initial storage size of the collection.  This value must be equal to or larger than the number of objects provided by the OSSet object passed as the first parameter.
        @result Returns an instance of OSSet or 0 on failure.
    */
    static OSSet *withSet(const OSSet *set,
                          unsigned int capacity = 0);

    /*!
        @function initWithCapacity
        @abstract A member function to initialize an instance of OSSet with a given capacity.
        @param capacity The initial storage size of the collection.
        @result Returns true if initialization successful or false on failure.
    */
    virtual bool initWithCapacity(unsigned int capacity);
    /*!
        @function initWithObjects
        @abstract A member function to initialize an instance of OSSet with a given capacity and populate the collection with the objects provided.
        @param object A static array containing OSObject derived objects used to populate the collection.
        @param count The number of objects provided.
     @param capacity The initial storage size of the collection. This value must be equal to or larger than the 'count' parameter.
        @result Returns true if initialization successful or false on failure.
    */
    virtual bool initWithObjects(OSObject *objects[],
                                 unsigned int count,
                                 unsigned int capacity = 0);
    /*!
        @function initWithArray
        @abstract A member function to initialize a new instance of OSSet and populate it with the contents of the OSArray object provided.
        @param array The OSArray object contining OSObject derived objects used to populate the new OSSet object.
        @param capacity The initial storage capacity of the object.  This value must be equal to or larger than the number of objects provided by the OSArray object passed as the first parameter.
        @result Returns true if initialization successful or false on failure.
    */
    virtual bool initWithArray(const OSArray *array,
                               unsigned int capacity = 0);
    /*!
        @function initWithSet
        @abstract A member function to initialize a new instance of OSSet and populate it with the contents of the OSSet object provided.
        @param array The OSSet object contining OSObject derived objects used to populate the new OSSet object.
        @param capacity The initial storage capacity of the object.  This value must be equal to or larger than the number of objects provided by the OSSet object passed as the first parameter.
        @result Returns true if initialization successful or false on failure.
        @discussion This function should not be called directly, use release() instead.
    */
    virtual bool initWithSet(const OSSet *set,
                             unsigned int capacity = 0);
    /*!
        @function free
        @abstract A member function to release all resources created or used by the OSArray instance.
    */
    virtual void free();

    /*!
        @function getCount
        @abstract A member function which returns the number of objects current in the collection.
        @result Returns the number of objects in the collection.
    */
    virtual unsigned int getCount() const;
    /*!
        @function getCapacity
        @abstract A member function which returns the storage capacity of the collection.
        @result Returns the storage size of the collection.
    */
    virtual unsigned int getCapacity() const;
    /*!
        @function getCapacityIncrement
        @abstract A member function which returns the growth factor of the collection.
        @result Returns the size by which the collection will grow.
    */
    virtual unsigned int getCapacityIncrement() const;
    /*!
        @function setCapacityIncrement
        @abstract A member function which sets the growth factor of the collection.
        @result Returns the new increment.
    */
    virtual unsigned int setCapacityIncrement(unsigned increment);

    /*!
        @function ensureCapacity
        @abstract A member function to grow the size of the collection.
        @param newCapacity The new capacity for the collection to expand to.
        @result Returns the new capacity of the collecion or the previous capacity upon error.
    */
    virtual unsigned int ensureCapacity(unsigned int newCapacity);

    /*!
        @function flushCollection
        @abstract A member function which removes and releases all objects within the collection.
    */
    virtual void flushCollection();

    /*!
        @function setObject
        @abstract A member function to place objects into the collection.
        @param anObject The OSObject derived object to be placed into the collecion.
        @result Returns true if the object was successfully placed into the collection, false otherwise.
        @discussion The object added to the collecion is automatically retained.
    */
    virtual bool setObject(OSObject *anObject);
    /*!
        @function merge
        @abstract A member function to merge the contents of an OSArray object with set.
        @param array The OSArray object which contains the objects to be merged.
        @result Returns true if the contents of the OSArray were successfully merged into the reciever.
    */
    virtual bool merge(const OSArray *array);
    /*!
        @function merge
        @abstract A member function to merge the contents of an OSSet object with receiver.
        @param set The OSSet object which contains the objects to be merged.
        @result Returns true if the contents of the OSSet were successfully merged into the reciever.
    */
    virtual bool merge(const OSSet *set);

    /*!
        @function removeObject
        @abstract A member function to remove objects from the collection.
        @param anObject The OSObject derived object to be removed from the collecion.
        @discussion The object removed from the collecion is automatically released.
    */
    virtual void removeObject(OSObject * anObject);

    /*!
        @function containsObject
        @abstract A member function to query the collection for the presence of an object.
        @param anObject The OSObject derived object to be removed from the collecion.
        @result Returns true if the object is present within the set, false otherwise.
    */
    virtual bool containsObject(const OSObject *anObject) const;
    /*!
        @function member
        @abstract A member function to query the collection for the presence of an object.
        @param anObject The OSObject derived object to be removed from the collecion.
        @result Returns true if the object is present within the set, false otherwise.
    */
    virtual bool member(const OSObject *anObject) const;
    /*!
        @function getAnyObject
        @abstract A member function which returns an object from the set.
        @result Returns an object if one exists within the set.
    */
    virtual OSObject *getAnyObject() const;

    /*!
        @function isEqualTo
        @abstract A member function to test the equality between the receiver and an OSSet object.
        @param aSet An OSSet object to be compared against the receiver.
        @result Returns true if the objects are equivalent.
    */
    virtual bool isEqualTo(OSSet *aSet) const;
    /*!
        @function isEqualTo
        @abstract A member function to test the equality between the receiver and an unknown object.
        @param anObject An object to be compared against the receiver.
        @result Returns true if the objects are equal.
    */
    virtual bool isEqualTo(const OSObject *anObject) const;

    /*!
        @function serialize
        @abstract A member function which archives the receiver.
        @param s The OSSerialize object.
        @result Returns true if serialization was successful, false if not.
    */
    virtual bool serialize(OSSerialize *s) const;
};

#endif /* !_OS_OSSET_H */
