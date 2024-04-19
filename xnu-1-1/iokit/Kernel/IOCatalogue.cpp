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
 * Copyright (c) 1998 Apple Computer, Inc.  All rights reserved. 
 *
 * HISTORY
 *
 */

#include <IOKit/IOService.h>
#include <libkern/c++/OSContainers.h>
#include <IOKit/IOCatalogue.h>
#include <libkern/c++/OSUnserialize.h>
#include <mach/kmod.h>

#include <IOKit/IOLib.h>

#include <IOKit/assert.h>


#define super OSObject
#define kModuleKey "Module"

OSDefineMetaClassAndStructors(IOCatalogue, OSObject)

#define CATALOGTEST 0

IOCatalogue                   * gIOCatalogue;
const OSSymbol                * gIOClassKey;
const OSSymbol                * gIOProbeScoreKey;

static void UniqueProperties( OSDictionary * dict )
{
    OSString             * data;

    data = OSDynamicCast( OSString, dict->getObject( gIOClassKey ));
    if( data) {
        const OSSymbol *classSymbol = OSSymbol::withString(data);

        dict->setObject( gIOClassKey, (OSSymbol *) classSymbol);
        classSymbol->release();
    } else
        IOLog("Missing or bad \"%s\" key\n",
                    gIOClassKey->getCStringNoCopy());

    data = OSDynamicCast( OSString, dict->getObject( gIOMatchCategoryKey ));
    if( data) {
        const OSSymbol *classSymbol = OSSymbol::withString(data);

        dict->setObject( gIOMatchCategoryKey, (OSSymbol *) classSymbol);
        classSymbol->release();
    }
}

void IOCatalogue::initialize( void )
{
    OSArray              * array;
    OSString             * errorString;
    bool		   rc;

    extern const char * gIOKernelConfigTables;

    array = OSDynamicCast(OSArray, OSUnserialize(gIOKernelConfigTables, &errorString));
    if (!array && errorString) {
	IOLog("KernelConfigTables syntax error: %s\n",
		errorString->getCStringNoCopy());
	errorString->release();
    }

    gIOClassKey = OSSymbol::withCStringNoCopy( kIOClassKey );
    gIOProbeScoreKey = OSSymbol::withCStringNoCopy( kIOProbeScoreKey );
    assert( array && gIOClassKey && gIOProbeScoreKey);

    gIOCatalogue = new IOCatalogue;
    assert(gIOCatalogue);
    rc = gIOCatalogue->init(array);
    assert(rc);
    array->release();
}

// Initialize the IOCatalog object.
bool IOCatalogue::init(OSArray * initArray)
{
    IORegistryEntry      * entry;
    OSDictionary         * dict;
    
    if ( !super::init() )
        return false;

    generation = 1;
    
    array = initArray;
    array->retain();
    kernelTables = OSCollectionIterator::withCollection( array );

    lock = IOLockAlloc();

    kernelTables->reset();
    while( (dict = (OSDictionary *) kernelTables->getNextObject())) {
        UniqueProperties(dict);
    }

#if CATALOGTEST
    AbsoluteTime deadline;
    clock_interval_to_deadline( 1000, kMillisecondScale );
    thread_call_func_delayed( ping, this, deadline );
#endif

    entry = IORegistryEntry::getRegistryRoot();
    if ( entry )
        entry->setProperty(kIOCatalogueKey, this);

    return true;
}

// Release all resources used by IOCatalogue and deallocate.
// This will probably never be called.
void IOCatalogue::free( void )
{
    if ( array )
        array->release();

    if ( kernelTables )
        kernelTables->release();
    
    super::free();
}

#if CATALOGTEST

static int hackLimit;

enum { kDriversPerIter = 4 };

void IOCatalogue::ping( thread_call_param_t arg, thread_call_param_t)
{
    IOCatalogue 	 * self = (IOCatalogue *) arg;
    OSOrderedSet         * set;
    OSDictionary         * table;
    int	                   newLimit;

    set = OSOrderedSet::withCapacity( 1 );

    IOTakeLock( &self->lock );

    for( newLimit = 0; newLimit < kDriversPerIter; newLimit++) {
	table = (OSDictionary *) self->array->getObject(
					hackLimit + newLimit );
	if( table) {
	    set->setLastObject( table );

	    OSSymbol * sym = (OSSymbol *) table->getObject( gIOClassKey );
	    kprintf("enabling %s\n", sym->getCStringNoCopy());

	} else {
	    newLimit--;
	    break;
	}
    }

    IOService::catalogNewDrivers( set );

    hackLimit += newLimit;
    self->generation++;

    IOUnlock( &self->lock );

    if( kDriversPerIter == newLimit) {
        AbsoluteTime deadline;
        clock_interval_to_deadline( 500, kMillisecondScale );
        thread_call_func_delayed( ping, this, deadline );
    }
}
#endif

OSOrderedSet * IOCatalogue::findDrivers( IOService * service,
					SInt32 * generationCount )
{
    OSDictionary         * nextTable;
    OSOrderedSet         * set;
    OSString             * imports;

    set = OSOrderedSet::withCapacity( 1, IOServiceOrdering,
                                      (void *)gIOProbeScoreKey );
    if( !set )
	return( 0 );

    IOTakeLock( lock );
    kernelTables->reset();

#if CATALOGTEST
    int hackIndex = 0;
#endif
    while( (nextTable = (OSDictionary *) kernelTables->getNextObject())) {
#if CATALOGTEST
	if( hackIndex++ > hackLimit)
	    break;
#endif
        imports = OSDynamicCast( OSString,
			nextTable->getObject( gIOProviderClassKey ));
	if( imports && service->metaCast( imports ))
            set->setObject( nextTable );
    }

    *generationCount = getGenerationCount();

    IOUnlock( lock );

    return( set );
}

// Is personality already in the catalog?
OSOrderedSet * IOCatalogue::findDrivers( OSDictionary * matching,
                                         SInt32 * generationCount)
{
    OSDictionary         * dict;
    OSOrderedSet         * set;

    UniqueProperties(matching);

    set = OSOrderedSet::withCapacity( 1, IOServiceOrdering,
                                      (void *)gIOProbeScoreKey );

    IOTakeLock( lock );
    kernelTables->reset();
    while ( (dict = (OSDictionary *) kernelTables->getNextObject()) ) {
        if ( dict->isEqualTo(matching, matching) )
            set->setObject(dict);
    }
    *generationCount = getGenerationCount();
    IOUnlock( lock );

    return set;
}

// Add a new personality to the set if it has a unique IOResourceMatchKey value.
// XXX -- svail: This should be optimized.
static void AddNewImports( OSOrderedSet * set, OSDictionary * dict )
{
    OSCollectionIterator * iter;
    OSDictionary         * entry;
    OSString             * imports1;
    OSString             * imports2;
    bool                   foundMatch;

    imports1 = (OSString *)OSDynamicCast(OSString,
                                         dict->getObject(gIOProviderClassKey));
    if ( !imports1 )
        return;

    foundMatch = false;
    iter = OSCollectionIterator::withCollection(set);
    if ( !iter )
        return;
    
    while ( (entry = (OSDictionary *)iter->getNextObject()) ) {
        imports2 = (OSString *)OSDynamicCast(OSString,
                                             entry->getObject(gIOProviderClassKey));
        if ( !imports2 )
            continue;

        if ( imports1->isEqualTo(imports2) ) {
            foundMatch = true;
            break;
        }
    }
    iter->release();

    if ( !foundMatch )
        set->setObject(dict);
}

// Add driver config tables to catalog and start matching process.
bool IOCatalogue::addDrivers( OSArray * drivers,
                              bool doNubMatching = true )
{
    OSCollectionIterator * iter;
    OSDictionary         * dict;
    OSOrderedSet         * set;
    OSArray              * persons;
    bool                   ret;

    ret = true;
    persons = OSDynamicCast(OSArray, drivers);
    if ( !persons )
        return false;

    iter = OSCollectionIterator::withCollection( persons );
    if (!iter )
        return false;
    
    set = OSOrderedSet::withCapacity( 10, IOServiceOrdering,
                                      (void *)gIOProbeScoreKey );
    if ( !set ) {
        iter->release();
        return false;
    }

    IOTakeLock( lock );
    while ( (dict = (OSDictionary *) iter->getNextObject()) ) {
        UInt count;
        
        UniqueProperties( dict );

        // Add driver personality to catalogue.
        count = array->getCount();
        while ( count-- ) {
            OSDictionary         * driver;

            // Be sure not to double up on personalities.
            driver = (OSDictionary *)array->getObject(count);
            if ( dict->isEqualTo(driver, driver) ) {
                array->removeObject(count);
                break;
            }
        }
        
        ret = array->setObject( dict );
        if ( !ret )
            break;

        AddNewImports( set, dict );
    }
    // Start device matching.
    if ( doNubMatching && (set->getCount() > 0) ) {
        IOService::catalogNewDrivers( set );
        generation++;
    }
    IOUnlock( lock );

    set->release();
    iter->release();
    
    return ret;
}

// Remove drivers from the catalog which match the
// properties in the matching dictionary.
bool IOCatalogue::removeDrivers( OSDictionary * matching,
                                 bool doNubMatching = true)
{
    OSCollectionIterator * tables;
    OSDictionary         * dict;
    OSOrderedSet         * set;
    OSArray              * arrayCopy;

    if ( !matching )
        return false;

    set = OSOrderedSet::withCapacity(10,
                                     IOServiceOrdering,
                                     (void *)gIOProbeScoreKey);
    if ( !set )
        return false;

    arrayCopy = OSArray::withCapacity(100);
    if ( !arrayCopy )
        return false;
    
    tables = OSCollectionIterator::withCollection(arrayCopy);
    arrayCopy->release();
    if ( !tables )
        return false;

    UniqueProperties( matching );

    IOTakeLock( lock );
    kernelTables->reset();
    arrayCopy->merge(array);
    array->flushCollection();
    tables->reset();
    while ( (dict = (OSDictionary *)tables->getNextObject()) ) {
        if ( dict->isEqualTo(matching, matching) ) {
            AddNewImports( set, dict );
            continue;
        }

        array->setObject(dict);
    }
    // Start device matching.
    if ( doNubMatching && (set->getCount() > 0) ) {
        IOService::catalogNewDrivers(set);
        generation++;
    }
    IOUnlock( lock );
    
    set->release();
    tables->release();
    
    return true;
}

// Return the generation count.
SInt32 IOCatalogue::getGenerationCount( void ) const
{
    return( generation );
}

bool IOCatalogue::isModuleLoaded( OSString * moduleName ) const
{
    return isModuleLoaded(moduleName->getCStringNoCopy());
}

bool IOCatalogue::isModuleLoaded( const char * moduleName ) const
{
    kmod_info_t          * k_info;

    if ( !moduleName )
        return false;

    // Is the module already loaded?
    k_info = kmod_lookupbyname((char *)moduleName);
    if ( !k_info ) {
        kern_return_t            ret;

        // If the module hasn't been loaded, then load it.
        ret = kmod_load_extension((char *)moduleName);
        if  ( ret != kIOReturnSuccess ) {
            IOLog("IOCatalogue: %s cannot be loaded.\n", moduleName);
        }

        return false;
    }

    return true;
}

// Check to see if module has been loaded already.
bool IOCatalogue::isModuleLoaded( OSDictionary * driver ) const
{
    OSString             * moduleName;

    if ( !driver )
        return false;
    
    // If a personality doesn't have a module key, it is assumed to
    // be an "in-kernel" driver.
    moduleName = OSDynamicCast(OSString, driver->getObject(kModuleKey));
    if ( moduleName )
        return isModuleLoaded(moduleName);

    return true;
}

// This function is called after a module has been loaded.
void IOCatalogue::moduleHasLoaded( OSString * moduleName )
{
    OSDictionary         * dict;

    dict = OSDictionary::withCapacity(2);
    dict->setObject(kModuleKey, moduleName);
    startMatching(dict);
    dict->release();
}

void IOCatalogue::moduleHasLoaded( const char * moduleName )
{
    OSString             * name;

    name = OSString::withCString(moduleName);
    moduleHasLoaded(name);
    name->release();
}

IOReturn IOCatalogue::unloadModule( OSString * moduleName ) const
{
    kmod_info_t          * k_info;
    kern_return_t          ret;
    const char           * name;

    ret = kIOReturnBadArgument;
    if ( moduleName ) {
        name = moduleName->getCStringNoCopy();
        k_info = kmod_lookupbyname((char *)name);
        if ( k_info && (k_info->reference_count < 1) ) {
            if ( k_info->stop &&
                 !((ret = k_info->stop(k_info, 0)) == kIOReturnSuccess) )
                return ret;
            
           ret = kmod_destroy((host_t) 1, k_info->id);
        }
    }

    return ret;
}

static IOReturn _terminateDrivers( OSArray * array, OSDictionary * matching )
{
    OSCollectionIterator * tables;
    OSCollectionIterator * props;
    OSDictionary         * dict;
    OSIterator           * iter;
    OSArray              * arrayCopy;
    IOService            * service;
    IOReturn               ret;

    if ( !matching )
        return kIOReturnBadArgument;

    ret = kIOReturnSuccess;
    dict = 0;
    iter = IORegistryIterator::iterateOver(gIOServicePlane,
                                kIORegistryIterateRecursively);
    if ( !iter )
        return kIOReturnNoMemory;

    UniqueProperties( matching );

    props = OSCollectionIterator::withCollection(matching);
    if ( !props ) {
        iter->release();
        return kIOReturnNoMemory;
    }

    // terminate instances.
    do {
        iter->reset();
        while( (service = (IOService *)iter->getNextObject()) ) {
            dict = service->getPropertyTable();
            if ( !dict )
                continue;

            if ( !dict->isEqualTo(matching, matching) )
                 continue;

            if ( !service->terminate(kIOServiceRequired|kIOServiceSynchronous) ) {
                ret = kIOReturnUnsupported;
                break;
            }
        }
    } while( !service && !iter->isValid());
    iter->release();

    // remove configs from catalog.
    if ( ret != kIOReturnSuccess ) 
        return ret;

    arrayCopy = OSArray::withCapacity(100);
    if ( !arrayCopy )
        return kIOReturnNoMemory;

    tables = OSCollectionIterator::withCollection(arrayCopy);
    arrayCopy->release();
    if ( !tables )
        return kIOReturnNoMemory;

    arrayCopy->merge(array);
    array->flushCollection();
    tables->reset();
    while ( (dict = (OSDictionary *)tables->getNextObject()) ) {
        if ( dict->isEqualTo(matching, matching) )
            continue;

        array->setObject(dict);
    }

    tables->release();

    return ret;
}

IOReturn IOCatalogue::terminateDrivers( OSDictionary * matching )
{
    IOReturn ret;

    ret = kIOReturnSuccess;
    IOTakeLock( lock );
    ret = _terminateDrivers(array, matching);
    kernelTables->reset();
    IOUnlock( lock );

    return ret;
}

IOReturn IOCatalogue::terminateDriversForModule(
                                      OSString * moduleName,
                                      bool unload )
{
    IOReturn ret;
    OSDictionary * dict;

    dict = OSDictionary::withCapacity(1);
    if ( !dict )
        return kIOReturnNoMemory;

    dict->setObject(kModuleKey, moduleName);
    
    IOTakeLock( lock );

    ret = _terminateDrivers(array, dict);
    kernelTables->reset();

    // Unload the module itself.
    if ( unload && ret == kIOReturnSuccess ) {
        // Do kmod stop first.
        ret = unloadModule(moduleName);
    }

    IOUnlock( lock );

    dict->release();

    return ret;
}

IOReturn IOCatalogue::terminateDriversForModule(
                                      const char * moduleName,
                                      bool unload )
{
    OSString * name;
    IOReturn ret;

    name = OSString::withCString(moduleName);
    if ( !name )
        return kIOReturnNoMemory;

    ret = terminateDriversForModule(name, unload);
    name->release();
    
    return ret;
}

bool IOCatalogue::startMatching( OSDictionary * matching )
{
    OSDictionary         * dict;
    OSOrderedSet         * set;
    
    if ( !matching )
        return false;

    set = OSOrderedSet::withCapacity(10, IOServiceOrdering,
                                     (void *)gIOProbeScoreKey);
    if ( !set )
        return false;

    IOTakeLock( lock );
    kernelTables->reset();

    while ( (dict = (OSDictionary *)kernelTables->getNextObject()) ) {
        if ( dict->isEqualTo(matching, matching) )
            AddNewImports(set, dict);
    }
    // Start device matching.
    if ( set->getCount() > 0 ) {
        IOService::catalogNewDrivers(set);
        generation++;
    }

    IOUnlock( lock );

    set->release();

    return true;
}

void IOCatalogue::reset(void)
{
    OSArray              * tables;
    OSDictionary         * entry;
    unsigned int           count;

    IOLog("Resetting IOCatalogue.\n");
    
    IOTakeLock( lock );
    tables = OSArray::withArray(array);
    array->flushCollection();
    
    count = tables->getCount();
    while ( count-- ) {
        entry = (OSDictionary *)tables->getObject(count);
        if ( entry && !entry->getObject(kModuleKey) ) {
            array->setObject(entry);
        }
    }
    
    kernelTables->reset();
    IOUnlock( lock );
    
    tables->release();
}

bool IOCatalogue::serialize(OSSerialize * s) const
{
    bool                   ret;
    
    if ( !s )
        return false;

    IOTakeLock( lock );
    ret = array->serialize(s);
    IOUnlock( lock );

    return ret;
}
