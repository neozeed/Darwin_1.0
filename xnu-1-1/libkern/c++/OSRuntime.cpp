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
/*
 * Copyright (c) 1997 Apple Computer, Inc.
 *
 */
#include <libkern/c++/OSMetaClass.h>
#include <libkern/c++/OSLib.h>

#include <sys/cdefs.h>

__BEGIN_DECLS

#include <string.h>

struct mach_header;

#include <mach/mach_types.h>
#include <mach-o/mach_header.h>
#include <mach-o/loader.h>
#include <stdarg.h>

#ifdef DEBUG
extern int debug_iomalloc_size;
#endif

#define	MDECL(reqlen)					\
typedef union {						\
	struct	_mhead hdr;				\
	char	_m[(reqlen) + sizeof (struct _mhead)];	\
} hdr_t;						\
hdr_t

struct _mhead {
	size_t	mlen;
	char	dat[0];
};

void *kern_os_malloc(
	size_t		size)
{
	MDECL(size)	*mem;
	size_t		memsize = sizeof (*mem);

	if (size == 0)
		return (0);

	mem = (hdr_t *)kalloc(memsize);
	if (!mem)
		return (0);

#ifdef DEBUG
	debug_iomalloc_size += memsize;
#endif

	mem->hdr.mlen = memsize;
	(void) memset(mem->hdr.dat, 0, size);

	return  (mem->hdr.dat);
}

void kern_os_free(
	void		*addr)
{
	struct _mhead	*hdr;

	if (!addr)
		return;

	hdr = (struct _mhead *) addr; hdr--;

#ifdef DEBUG
	debug_iomalloc_size -= hdr->mlen;
#endif

#if 0
	memset((vm_offset_t)hdr, 0xbb, hdr->mlen);
#else
	kfree((vm_offset_t)hdr, hdr->mlen);
#endif
}

void *kern_os_realloc(
	void		*addr,
	size_t		nsize)
{
	struct _mhead	*ohdr;
	MDECL(nsize)	*nmem;
	size_t		nmemsize, osize;

	if (!addr)
		return (kern_os_malloc(nsize));

	ohdr = (struct _mhead *) addr; ohdr--;
	osize = ohdr->mlen - sizeof (*ohdr);
	if (nsize == osize)
		return (addr);

	if (nsize == 0) {
		kfree((vm_offset_t)ohdr, ohdr->mlen);
		return (0);
	}

	nmemsize = sizeof (*nmem);
	nmem = (hdr_t *) kalloc(nmemsize);
	if (!nmem){
		kern_os_free(addr);
		return (0);
	}

#ifdef DEBUG
	debug_iomalloc_size += nmemsize;
#endif

	nmem->hdr.mlen = nmemsize;
	if (nsize > osize)
		(void) memset(&nmem->hdr.dat[osize], 0, nsize - osize);
	(void) memcpy(nmem->hdr.dat, ohdr->dat,
					(nsize > osize) ? osize : nsize);
	kfree((vm_offset_t)ohdr, ohdr->mlen);

	return (nmem->hdr.dat);
}

size_t kern_os_malloc_size(
	void		*addr)
{
	struct _mhead	*hdr;

	if (!addr)
		return( 0);

	hdr = (struct _mhead *) addr; hdr--;
	return( hdr->mlen - sizeof (struct _mhead));
}

void __pure_virtual( void )	{ panic(__FUNCTION__); }

typedef void (*structor_t)(void);

void OSRuntimeUnloadCPP(kmod_info_t *ki, void *)
{
    structor_t *destructors = 0;
    int size;

    if (ki && ki->address)
        destructors = (structor_t *) getsectdatafromheader(
           (struct mach_header *) ki->address, SEG_TEXT, "__destructor", &size);

    if (destructors) {
        size /= sizeof(structor_t);

        for (int i = 0; i < size; i++)
            (*destructors[i])();
    }
}

kern_return_t OSRuntimeFinalizeCPP(kmod_info_t *ki, void *)
{
    void *metaHandle;

    if (OSMetaClass::modHasInstance(ki->name)) {
        // @@@ gvdl shourld have a verbose flag
        printf("Can't unload %s due to -\n", ki->name);
        OSMetaClass::reportModInstances(ki->name);
        return kOSMetaClassHasInstances;
    }

    // Tell the meta class system that we are starting to unload
    metaHandle = OSMetaClass::preModLoad(ki->name);
    OSRuntimeUnloadCPP(ki, 0);	// Do the actual unload
    (void) OSMetaClass::postModLoad(metaHandle);

    return KMOD_RETURN_SUCCESS;
}

// Functions used by the extenTools/kmod library project
kern_return_t OSRuntimeInitializeCPP(kmod_info_t *ki, void *)
{
    struct mach_header *header;
    void *metaHandle;
    structor_t *constructors;
    int size;
    bool loadSuccess;

    if (!ki || !ki->address)
        return KMOD_RETURN_FAILURE;
    else
        header = (struct mach_header *) ki->address;

    // Tell the meta class system that we are starting the load
    metaHandle = OSMetaClass::preModLoad(ki->name);
    assert(metaHandle);
    if (!metaHandle)
        return KMOD_RETURN_FAILURE;

    loadSuccess = true;
    constructors = (structor_t *)
        getsectdatafromheader(header, SEG_TEXT, "__constructor", &size);
    if (constructors) {
        size /= sizeof(structor_t);

        for (int i = 0; OSMetaClass::checkModLoad(metaHandle)
                && i < size; i++)
            (*constructors[i])();
        loadSuccess = OSMetaClass::checkModLoad(metaHandle);
    }

    // We failed so call all of the destructors
    if (!loadSuccess)
        OSRuntimeUnloadCPP(ki, 0);

    return OSMetaClass::postModLoad(metaHandle);
}

static KMOD_LIB_DECL(__kernel__, 0);
void OSlibkernInit(void)
{
    vm_address_t *headerArray = (vm_address_t *) getmachheaders();

    KMOD_INFO_NAME.address = headerArray[0]; assert(!headerArray[1]);
    if (kOSReturnSuccess != OSRuntimeInitializeCPP(&KMOD_INFO_NAME, 0))
        panic("OSRuntime: C++ runtime failed to initialize");
}

__END_DECLS

void * operator new( size_t size)
{
    void * result;

    result = (void *) kern_os_malloc( size);
    if( result)
	bzero( result, size);
    return( result);
}

void operator delete( void * addr)
{
    kern_os_free( addr);
}

