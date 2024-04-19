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
/* OSMetaClass.cpp created by gvdl on Fri 1998-11-17 */

#include <string.h>
#include <sys/systm.h>

#include <libkern/OSReturn.h>

#include <libkern/c++/OSCollectionIterator.h>
#include <libkern/c++/OSDictionary.h>
#include <libkern/c++/OSMetaClass.h>
#include <libkern/c++/OSArray.h>   
#include <libkern/c++/OSSet.h>	 
#include <libkern/c++/OSSymbol.h>
#include <libkern/c++/OSNumber.h>
#include <libkern/c++/OSSerialize.h>
#include <libkern/c++/OSLib.h>

enum {
    kSuperIsCString = 0x80000000,
};

__BEGIN_DECLS

#include <mach/etap_events.h>
#include <kern/lock.h>

#ifdef DEBUG
extern int debug_container_malloc_size;
#define ACCUMSIZE(s) do { debug_container_malloc_size += (s); } while(0)
#else
#define ACCUMSIZE(s)
#endif /* DEBUG */

__END_DECLS

static enum {
    kCompletedBootstrap = 0,
    kNoDictionaries = 1,
    kMakingDictionaries = 2
} sBootstrapState = kNoDictionaries;

static const int kClassCapacityIncrement = 40;
static const int kKModCapacityIncrement = 10;
static OSDictionary *sAllClassesDict, *sKModClassesDict;

static mutex_t *loadLock;
static struct StalledData {
    const char *kmodName;
    OSReturn result;
    unsigned int capacity;
    unsigned int count;
    OSMetaClass **classes;
} *sStalled;

void OSMetaClass::logError(OSReturn result)
{
    const char *msg;

    switch (result) {
    case kOSMetaClassNoInit:
	msg="OSMetaClass::preModLoad wasn't called, runtime internal error";
	break;
    case kOSMetaClassNoDicts:
	msg="Allocation failure for Metaclass internal dictionaries"; break;
    case kOSMetaClassNoKModSet:
	msg="Allocation failure for internal kmodule set"; break;
    case kOSMetaClassNoInsKModSet:
	msg="Can't insert the KMod set into the module dictionary"; break;
    case kOSMetaClassNoSuper:
	msg="Can't associate a class with its super class"; break;
    case kOSMetaClassInstNoSuper:
	msg="Instance construction, unknown super class."; break;
    default:
    case kOSMetaClassInternal:
	msg="runtime internal error"; break;
    case kOSReturnSuccess:
	return;
    }
    printf("%s\n", msg);
}

OSMetaClass::OSMetaClass(const char *inClassName,
			 const char *inSuperClassName,
			 unsigned int inClassSize)
{
    instanceCount = 0;
    classSize = inClassSize;
    if (inSuperClassName && *inSuperClassName) {
	superClass = (OSMetaClass *) inSuperClassName;
	classSize |= kSuperIsCString;
    }
    else
	superClass = 0;

    className = (const OSSymbol *) inClassName;

    if (!sStalled) {
	printf("OSMetaClass::preModLoad wasn't called for %s, "
	       "runtime internal error\n", inClassName);
    } else if (!sStalled->result) {
	// Grow stalled array if neccessary
	if (sStalled->count >= sStalled->capacity) {
	    OSMetaClass **oldStalled = sStalled->classes;
	    int oldSize = sStalled->capacity * sizeof(OSMetaClass *);
	    int newSize = oldSize
			+ kKModCapacityIncrement * sizeof(OSMetaClass *);

	    sStalled->classes = (OSMetaClass **) kalloc(newSize);
	    if (!sStalled->classes) {
		sStalled->classes = oldStalled;
		sStalled->result = kOSMetaClassNoTempData;
		return;
	    }

	    sStalled->capacity += kKModCapacityIncrement;
	    memmove(sStalled->classes, oldStalled, oldSize);
	    kfree((vm_offset_t)oldStalled, oldSize);
	    ACCUMSIZE(newSize - oldSize);
	}

	sStalled->classes[sStalled->count++] = this;
    }
}

OSMetaClass::~OSMetaClass()
{
    do {
	OSCollectionIterator *iter;

	if (sAllClassesDict)
	    sAllClassesDict->removeObject(className);

	iter = OSCollectionIterator::withCollection(sKModClassesDict);
	if (!iter)
	    break;

	OSSymbol *iterKey;
	while ( (iterKey = (OSSymbol *) iter->getNextObject()) ) {
	    OSSet *kmodClassSet;
	    kmodClassSet = (OSSet *) sKModClassesDict->getObject(iterKey);
	    if (kmodClassSet && kmodClassSet->containsObject(this)) {
		kmodClassSet->removeObject(this);
		break;
	    }
	}
	iter->release();
    } while (false);

    if (sStalled) {
	unsigned int i;

	// First pass find class in stalled list
	for (i = 0; i < sStalled->count; i++)
	    if (this == sStalled->classes[i])
		break;

	if (i < sStalled->count) {
	    sStalled->count--;
	    if (i < sStalled->count)
		memmove(&sStalled->classes[i], &sStalled->classes[i+1],
			    (sStalled->count - i) * sizeof(OSMetaClass *));
	}
	return;
    }
}

// Don't do anything as these classes must be statically allocated
void OSMetaClass::free() { }
void OSMetaClass::retain() { }
void OSMetaClass::release() { }

const char *OSMetaClass::getClassName() const
{
    return className->getCStringNoCopy();
}

unsigned int OSMetaClass::getClassSize() const
{
    return classSize & ~kSuperIsCString;
}

void *OSMetaClass::preModLoad(const char *kmodName)
{
    if (!loadLock) {
        loadLock = mutex_alloc(ETAP_IO_AHA);
	_mutex_lock(loadLock);
    }
    else
	_mutex_lock(loadLock);

    sStalled = (StalledData *) kalloc(sizeof(*sStalled));
    if (sStalled) {
	sStalled->classes  = (OSMetaClass **)
			kalloc(kKModCapacityIncrement * sizeof(OSMetaClass *));
	if (!sStalled->classes) {
	    kfree((vm_offset_t) sStalled, sizeof(*sStalled));
	    return 0;
	}
	ACCUMSIZE((kKModCapacityIncrement * sizeof(OSMetaClass *)) + sizeof(*sStalled));

        sStalled->result   = kOSReturnSuccess;
	sStalled->capacity = kKModCapacityIncrement;
	sStalled->count	   = 0;
	sStalled->kmodName = kmodName;
	bzero(sStalled->classes, kKModCapacityIncrement * sizeof(OSMetaClass *));
    }

    return sStalled;
}

bool OSMetaClass::checkModLoad(void *loadHandle)
{
    return sStalled && loadHandle == sStalled
	&& sStalled->result == kOSReturnSuccess;
}

static inline const OSSymbol *copySymbol(bool copy, const char *cString)
{
    if (copy)
	return OSSymbol::withCString(cString);
    else
	return OSSymbol::withCStringNoCopy(cString);
}

OSReturn OSMetaClass::postModLoad(void *loadHandle)
{
    OSReturn result = kOSReturnSuccess;
    OSSet *kmodSet = 0;

    if (!sStalled || loadHandle != sStalled) {
	logError(kOSMetaClassInternal);
	return kOSMetaClassInternal;
    }

    if (sStalled->result)
	result = sStalled->result;
    else switch (sBootstrapState) {
    case kNoDictionaries:
	sBootstrapState = kMakingDictionaries;
	// No break; fall through

    case kMakingDictionaries:
	sKModClassesDict = OSDictionary::withCapacity(kKModCapacityIncrement);
	sAllClassesDict = OSDictionary::withCapacity(kClassCapacityIncrement);
	if (!sAllClassesDict || !sKModClassesDict) {
	    result = kOSMetaClassNoDicts;
	    break;
	}
	// No break; fall through

    case kCompletedBootstrap:
    {
	bool copyStrings = (0 != strcmp("__kernel__", sStalled->kmodName));

	if (!sStalled->count)
	    break;	// Nothing to do so just get out

	kmodSet = OSSet::withCapacity(sStalled->count);
	if (!kmodSet) {
	    result = kOSMetaClassNoKModSet;
	    break;
	}

	if (!sKModClassesDict->setObject(sStalled->kmodName, kmodSet)) {
	    result = kOSMetaClassNoInsKModSet;
	    break;
	}

	// First pass symbolling strings and inserting classes in dictionary
	for (unsigned int i = 0; i < sStalled->count; i++) {
	    OSMetaClass *me = sStalled->classes[i];

	    me->className = copySymbol(copyStrings,
				       (const char *) me->className);
	    if (me->classSize & kSuperIsCString)
		me->superClass = (OSMetaClass *)
		    copySymbol(copyStrings, (const char *) me->superClass);

	    sAllClassesDict->setObject(me->className, me);
	    kmodSet->setObject(me);
	}

	// Second pass to connect up superclasses.
	for (unsigned int i = 0; !result && i < sStalled->count; i++) {
	    OSMetaClass *me = sStalled->classes[i];
	    OSSymbol *superSym;

	    if (me->classSize & kSuperIsCString) {

		me->classSize &= ~kSuperIsCString;
		superSym = (OSSymbol *) me->superClass;
		me->superClass = (OSMetaClass *)
				    sAllClassesDict->getObject(superSym);
		superSym->release();
		if (!me->superClass)
		    result = kOSMetaClassNoSuper;
	    }
	}
	sBootstrapState = kCompletedBootstrap;
	break;
    }

    default:
	result = kOSMetaClassInternal;
	break;
    }

    if (kmodSet)
	kmodSet->release();

    if (sStalled) {
	ACCUMSIZE(-(sStalled->capacity * sizeof(OSMetaClass *)
		     + sizeof(*sStalled)));
	kfree((vm_offset_t) sStalled->classes,
	      sStalled->capacity * sizeof(OSMetaClass *));
	kfree((vm_offset_t) sStalled, sizeof(*sStalled));
	sStalled = 0;
    }

    logError(result);
    mutex_unlock(loadLock);
    return result;
}


void OSMetaClass::instanceConstructed() const
{
    if (sStalled && (classSize & kSuperIsCString) && superClass)
    {
	OSMetaClass *me = (OSMetaClass *) this;	 // overide const this
	const char * const superName = (const char *) superClass;

	// The superClass may be in the class dictionary already.
	if (sBootstrapState == kCompletedBootstrap && sAllClassesDict)
	    me->superClass = (OSMetaClass *)
		sAllClassesDict->getObject(superName);
	else
	    me->superClass = 0;

	if (!me->superClass) {
	    // Oh dear we have to scan the stalled list and walk the
	    // the super class chain manually.
	    unsigned int i;

	    // find superclass in stalled list
	    for (i = 0; i < sStalled->count; i++) {
		if (0 == strcmp(superName,
				(const char *) sStalled->classes[i]->className))
		    break;
	    }

	    if (i < sStalled->count) {
		me->superClass = sStalled->classes[i];
		me->classSize &= ~kSuperIsCString;
	    }
	    else
		sStalled->result = kOSMetaClassInstNoSuper;

	}
    }

    if ((0 == instanceCount++) && superClass)
	superClass->instanceConstructed();
}

void OSMetaClass::instanceDestructed() const
{
    if ((1 == instanceCount--) && superClass)
	superClass->instanceDestructed();

    if( ((int) instanceCount) < 0)
	printf("%s: bad retain(%d)", getClassName(), instanceCount);
}

bool OSMetaClass::modHasInstance(const char *kmodName)
{
    bool result = false;

    if (!loadLock) {
        loadLock = mutex_alloc(ETAP_IO_AHA);
	_mutex_lock(loadLock);
    }
    else
	_mutex_lock(loadLock);

    do {
	OSSet *kmodClasses;
	OSCollectionIterator *iter;
	OSMetaClass *checkClass;

	kmodClasses = OSDynamicCast(OSSet,
				    sKModClassesDict->getObject(kmodName));
	if (!kmodClasses)
	    break;

	iter = OSCollectionIterator::withCollection(kmodClasses);
	if (!iter)
	    break;

	while ( (checkClass = (OSMetaClass *) iter->getNextObject()) )
	    if (checkClass->getInstanceCount()) {
		result = true;
		break;
	    }

	iter->release();
    } while (false);

    mutex_unlock(loadLock);

    return result;
}

void OSMetaClass::reportModInstances(const char *kmodName)
{
    OSSet *kmodClasses;
    OSCollectionIterator *iter;
    OSMetaClass *checkClass;

    kmodClasses = OSDynamicCast(OSSet,
				 sKModClassesDict->getObject(kmodName));
    if (!kmodClasses)
	return;

    iter = OSCollectionIterator::withCollection(kmodClasses);
    if (!iter)
	return;

    while ( (checkClass = (OSMetaClass *) iter->getNextObject()) )
	if (checkClass->getInstanceCount()) {
	    printf("%s: %s has %d instance(s)\n",
		  kmodName,
		  checkClass->getClassName(),
		  checkClass->getInstanceCount());
	}

    iter->release();
}

const OSMetaClass *OSMetaClass::getMetaClassWithName(const OSSymbol *name)
{
    OSMetaClass *retMeta = 0;

    if (!name)
	return 0;

    if (sAllClassesDict)
	retMeta = (OSMetaClass *) sAllClassesDict->getObject(name);

    if (!retMeta && sStalled)
    {
	// Oh dear we have to scan the stalled list and walk the
	// the stalled list manually.
	const char *cName = name->getCStringNoCopy();
	unsigned int i;

	// find class in stalled list
	for (i = 0; i < sStalled->count; i++) {
	    retMeta = sStalled->classes[i];
	    if (0 == strcmp(cName, (const char *) retMeta->className))
		break;
	}

	if (i < sStalled->count)
	    retMeta = 0;
    }

    return retMeta;
}

const OSMetaClass *OSMetaClass::getMetaClassWithName(const OSString *name)
{
    const OSSymbol *tmpKey = OSSymbol::withString(name);
    const OSMetaClass *ret = getMetaClassWithName(tmpKey);

    tmpKey->release();
    return ret;
}

const OSMetaClass *OSMetaClass::getMetaClassWithName(const char *name)
{
    const OSSymbol *tmpKey = OSSymbol::withCStringNoCopy(name);
    const OSMetaClass *ret = getMetaClassWithName(tmpKey);

    tmpKey->release();
    return ret;
}

OSObject *OSMetaClass::allocClassWithName(const OSSymbol *name)
{
    const OSMetaClass * const meta = getMetaClassWithName(name);

    if (meta)
	return meta->alloc();

    return 0;
}

OSObject *OSMetaClass::allocClassWithName(const OSString *name)
{
    const OSMetaClass * const meta = getMetaClassWithName(name);

    if (meta)
	return meta->alloc();

    return 0;
}

OSObject *OSMetaClass::allocClassWithName(const char *name)
{
    const OSMetaClass * const meta = getMetaClassWithName(name);

    if (meta)
	return meta->alloc();

    return 0;
}


OSObject *
OSMetaClass::checkMetaCastWithName(const OSSymbol *name, const OSObject *in)
{
    const OSMetaClass * const meta = getMetaClassWithName(name);

    if (meta)
	return meta->checkMetaCast(in);

    return 0;
}

OSObject *
OSMetaClass::checkMetaCastWithName(const OSString *name, const OSObject *in)
{
    const OSMetaClass * const meta = getMetaClassWithName(name);

    if (meta)
	return meta->checkMetaCast(in);

    return 0;
}

OSObject *
OSMetaClass::checkMetaCastWithName(const char *name, const OSObject *in)
{
    const OSMetaClass * const meta = getMetaClassWithName(name);

    if (meta)
	return meta->checkMetaCast(in);

    return 0;
}

/*
OSMetaClass::checkMetaCast
    checkMetaCast(OSObject *check)

Check to see if the 'check' object has this object in it's metaclass chain.  Returns check if it is indeed a kind of the current meta class, 0 otherwise.

Generally this method is not invoked directly but is used to implement the OSObject::metaCast member function.

See also OSObject::metaCast

 */
OSObject *OSMetaClass::checkMetaCast(const OSObject *check) const
{
    const OSMetaClass * const toMeta = this;
    const OSMetaClass *fromMeta;

    for (fromMeta = check->getMetaClass(); ; fromMeta = fromMeta->superClass) {
	if (toMeta == fromMeta)
	    return (OSObject *) check; // Discard const

	if (!fromMeta->superClass || (fromMeta->classSize & kSuperIsCString))
	    break;
    }

    return 0;
}

const OSMetaClass *OSMetaClass::getSuperClass() const
{
    return superClass;
}

unsigned int OSMetaClass::getInstanceCount() const
{
    return instanceCount;
}

void OSMetaClass::printInstanceCounts()
{
    OSCollectionIterator *classes;
    OSSymbol		 *className;
    OSMetaClass		 *meta;

    classes = OSCollectionIterator::withCollection(sAllClassesDict);
    if (!classes)
	return;

    while( (className = (OSSymbol *)classes->getNextObject())) {
	meta = (OSMetaClass *) sAllClassesDict->getObject(className);
	assert(meta);

	printf("%24s count: %03d x 0x%03x = 0x%06x\n",
	    className->getCStringNoCopy(),
	    meta->getInstanceCount(),
	    meta->getClassSize(),
	    meta->getInstanceCount() * meta->getClassSize() );
    }
    printf("\n");
    classes->release();
}

OSDictionary * OSMetaClass::getClassDictionary()
{
    return sAllClassesDict;
}

bool OSMetaClass::serialize(OSSerialize *s) const
{
    OSDictionary *	dict;
    OSNumber *		off;
    bool		ok = false;

    if (s->previouslySerialized(this)) return true;

    dict = 0;// IODictionary::withCapacity(2);
    off = OSNumber::withNumber(getInstanceCount(), 32);

    if (dict) {
	dict->setObject("InstanceCount", off );
	ok = dict->serialize(s);
    } else if( off)
	ok = off->serialize(s);

    if (dict)
	dict->release();
    if (off)
	off->release();

    return ok;
}
