/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.1 (the "License").  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON- INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
#ifdef SHLIB
#include "shlib.h"
#endif SHLIB
/*
 * This file contains the routines that drives pass1 of the link-editor.  In
 * pass1 the objects needed from archives are selected for loading and all of
 * the things that need to be merged from the input objects are merged into
 * tables (for output and relocation on the second pass).
 */
#include <libc.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <mach/mach.h>
#include "stuff/openstep_mach.h"
#include <mach-o/fat.h>
#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include <mach-o/reloc.h>
#include <mach-o/ldsyms.h>
#include <errno.h>
#include <limits.h>
#include <ctype.h>
#include <ar.h>
#ifndef AR_EFMT1
#define	AR_EFMT1	"#1/"		/* extended format #1 */
#endif
#include <mach-o/ranlib.h>
#include "stuff/arch.h"
#include "stuff/best_arch.h"

#include "ld.h"
#include "pass1.h"
#include "objects.h"
#include "fvmlibs.h"
#include "dylibs.h"
#include "sections.h"
#include "symbols.h"
#include "sets.h"

#ifndef RLD
/* the user specified directories to search for -lx names, and the number
   of them */
__private_extern__ char **search_dirs = NULL;
__private_extern__ unsigned long nsearch_dirs = 0;

/* the standard directories to search for -lx names */
__private_extern__ char *standard_dirs[] = {
    "/lib/",
    "/usr/lib/",
    "/usr/local/lib/",
    NULL
};

/*
 * The user specified directories to search for "-framework Foo" names, and the
 * number of them.  These are specified with -F options.
 */
__private_extern__ char **framework_dirs = NULL;
__private_extern__ unsigned long nframework_dirs = 0;

/* the standard framework directories to search for "-framework Foo" names */
__private_extern__ char *standard_framework_dirs[] = {
#ifdef __OPENSTEP__
    "/LocalLibrary/Frameworks/",
    "/NextLibrary/Frameworks/",
#else
    "/Local/Library/Frameworks/",
    "/Network/Library/Frameworks/",
    "/System/Library/Frameworks/",
#endif
    NULL
};

/* the pointer to the head of the base object file's segments */
__private_extern__ struct merged_segment *base_obj_segments = NULL;
#endif !defined(RLD)

#ifndef SA_RLD
/*
 * The stat buffer for input files.  This is needed both by pass1() and
 * pass1_archive() to check the times.
 */
static struct stat stat_buf = { 0 };

/*
 * These are pointers to strings and symbols used to search of the table of
 * contents of a library.  These have to be static and not local so that
 * pass1_archive() and search_dylibs() can set them and that ranlib_bsearch()
 * and dylib_bsearch() can use them.  This is done instead of assigning to
 * ran_name so that the library can be mapped read only and thus not get dirty
 * and maybe written to the swap area by the kernel.
 */
static char *strings = NULL;
#ifndef RLD
static struct nlist *symbols = NULL;

/*
 * The list of dynamic libraries to search.  The list of specified libraries
 * can contain archive libraries when archive libraries appear after dynamic
 * libraries on the link line.
 */
__private_extern__ struct dynamic_library *dynamic_libs = NULL;

static void search_for_file(
    char *base_name,
    char **file_name,
    int *fd);
static void search_for_framework(
    char *name,
    char **file_name,
    int *fd);
#endif /* !defined(RLD) */

static void pass1_fat(
    char *file_name,
    char *file_addr,
    unsigned long file_size,
    enum bool base_name,
    enum bool dylib_only);

static void pass1_archive(
    char *file_name,
    char *file_addr,
    unsigned long file_size,
    enum bool base_name,
    enum bool from_fat_file);

static enum bool check_archive_arch(
    char *file_name,
    char *file_addr,
    unsigned long file_size);

static enum bool check_extend_format_1(
    char *name,
    struct ar_hdr *ar_hdr,
    unsigned long size_left,
    unsigned long *member_name_size);

static void pass1_object(
    char *file_name,
    char *file_addr,
    unsigned long file_size,
    enum bool base_name,
    enum bool from_fat_file,
    enum bool dylib_only);

#ifndef RLD
static void check_dylibs_for_definition(
    struct merged_symbol *merged_symbol);
static enum bool check_dylibs_for_reference(
    struct merged_symbol *merged_symbol);

static enum bool open_dylib(
    struct dynamic_library *p);

static int dylib_bsearch(
    const char *symbol_name,
    const struct dylib_table_of_contents *toc);

static int nlist_bsearch(
    const char *symbol_name,
    const struct nlist *symbol);
#endif /* !defined(RLD) */

static int ranlib_bsearch(
    const char *symbol_name,
    const struct ranlib *ran);

#endif /* !defined(SA_RLD) */

static void check_cur_obj(
    enum bool dylib_only);

static void check_size_offset(
    unsigned long size,
    unsigned long offset,
    unsigned long align,
    char *size_str,
    char *offset_str,
    unsigned long cmd);

static void check_size_offset_sect(
    unsigned long size,
    unsigned long offset,
    unsigned long align,
    char *size_str,
    char *offset_str,
    unsigned long cmd,
    unsigned long sect,
    char *segname,
    char *sectname);

#ifndef RLD
static void collect_base_obj_segments(
	void);

static void add_base_obj_segment(
	struct segment_command *sg,
	char *filename);

static char *mkstr(
	const char *args,
	...);
#endif !defined(RLD)

#ifndef SA_RLD
/*
 * pass1() is called from main() and is passed the name of a file and a flag
 * indicating if it is a path searched abbrevated file name (a -lx argument).
 *
 * If the name is a path searched abbrevated file name (of the form "-lx")
 * then it is searched for in the search_dirs list (created from the -Ldir
 * arguments) and then in the list of standard_dirs.  The string "x" of the
 * "-lx" argument will be converted to "libx.a" if the string does not end in
 * ".o", in which case it is left alone.
 *
 * If the file is the object file for a the base of an incremental load then
 * base_name is TRUE and the pointer to the object_file structure for it,
 * base_obj, is set when it is allocated.
 * 
 * If the file turns out to be just a plain object then merge() is called to
 * merge its symbolic information and it will be unconditionally loaded.
 *
 * Object files in archives are loaded only if they resolve currently undefined
 * references or the -ObjC flag is set and their are symbols with the ".objc"
 * prefix defined.
 */
__private_extern__
void
pass1(
char *name,
enum bool lname,
enum bool base_name,
enum bool framework_name)
{
    int fd;
    char *file_name;
#ifndef RLD
    char *p, *type;
#endif !defined(RLD)
    kern_return_t r;
    unsigned long file_size;
    char *file_addr;
    struct fat_header *fat_header;
#ifdef __MWERKS__
    enum bool dummy;
        dummy = lname;
        dummy = base_name;
        dummy = framework_name;
#endif

#ifdef DEBUG
	/* The compiler "warning: `file_name' may be used uninitialized in */
	/* this function" can safely be ignored */
	file_name = NULL;
#endif DEBUG

	fd = -1;
#ifndef RLD
	if(lname){
	    if(name[0] != '-' || name[1] != 'l')
		fatal("Internal error: pass1() called with name of: %s and "
		      "lname == TRUE", name);
	    p = &name[2];
	    p = strrchr(p, '.');
	    if(p != NULL && strcmp(p, ".o") == 0){
		p = &name[2];
		search_for_file(p, &file_name, &fd);
		if(fd == -1)
		    fatal("can't locate file for: %s", name);
	    }
	    else{
		if(dynamic == TRUE){
		    p = mkstr("lib", &name[2], ".dylib", NULL);
		    search_for_file(p, &file_name, &fd);
		}
		if(fd == -1){
		    p = mkstr("lib", &name[2], ".a", NULL);
		    search_for_file(p, &file_name, &fd);
		}
		if(fd == -1)
		    fatal("can't locate file for: %s", name);
		free(p);
	    }
	}
	else if(framework_name){
	    type = strrchr(name, ',');
	    if(type != NULL && type[1] != '\0'){
		*type = '\0';
		type++;
		p = mkstr(name, ".framework/", name, type, NULL);
		search_for_framework(p, &file_name, &fd);
		if(fd == -1)
		    warning("can't locate framework for: -framework %s,%s "
			    "using suffix %s", name, type, type);
	    }
	    else
		type = NULL;
	    if(fd == -1){
		p = mkstr(name, ".framework/", name, NULL);
		search_for_framework(p, &file_name, &fd);
	    }
	    if(fd == -1){
		if(type != NULL)
		    fatal("can't locate framework for: -framework %s,%s",
			  name, type);
		else
		    fatal("can't locate framework for: -framework %s", name);
	    }
	}
	else
#endif !defined(RLD)
	{
	    if((fd = open(name, O_RDONLY, 0)) == -1){
		system_error("can't open: %s", name);
		return;
	    }
	    file_name = name;
	}

	/*
	 * Now that the file_name has been determined and opened get it into
	 * memory by mapping it.
	 */
	if(fstat(fd, &stat_buf) == -1){
	    system_fatal("can't stat file: %s", file_name);
	    close(fd);
	    return;
	}
	file_size = stat_buf.st_size;
	/*
	 * For some reason mapping files with zero size fails so it has to
	 * be handled specially.
	 */
	if(file_size == 0){
	    error("file: %s is empty (not an object or archive)", file_name);
	    close(fd);
	    return;
	}
	if((r = map_fd((int)fd, (vm_offset_t)0, (vm_offset_t *)&file_addr,
	    (boolean_t)TRUE, (vm_size_t)file_size)) != KERN_SUCCESS){
	    close(fd);
	    mach_fatal(r, "can't map file: %s", file_name);
	}
#ifdef RLD_VM_ALLOC_DEBUG
	print("rld() map_fd: addr = 0x%0x size = 0x%x\n",
	      (unsigned int)file_addr, (unsigned int)file_size);
#endif /* RLD_VM_ALLOC_DEBUG */
	/*
	 * The mapped file can't be made read-only because even in the case of
	 * errors where a wrong bytesex file is attempted to be loaded it must
	 * be writeable to detect the error.
	 *
	 *  if((r = vm_protect(mach_task_self(), (vm_address_t)file_addr,
	 * 		file_size, FALSE, VM_PROT_READ)) != KERN_SUCCESS){
	 *      close(fd);
	 *      mach_fatal(r, "can't make memory for mapped file: %s read-only",
	 *      	   file_name);
	 *  }
	 */
	close(fd);

	/*
	 * Determine what type of file it is (fat, archive or thin object file).
	 */
	if(sizeof(struct fat_header) > file_size){
	    error("truncated or malformed file: %s (file size too small to be "
		  "any kind of object or library)", file_name);
	    return;
	}
	fat_header = (struct fat_header *)file_addr;
#ifdef __BIG_ENDIAN__
	if(fat_header->magic == FAT_MAGIC)
#endif /* __BIG_ENDIAN__ */
#ifdef __LITTLE_ENDIAN__
	if(fat_header->magic == SWAP_LONG(FAT_MAGIC))
#endif /* __LITTLE_ENDIAN__ */
	{
#ifdef RLD
	    new_archive_or_fat(file_name, file_addr, file_size);
#endif RLD
	    pass1_fat(file_name, file_addr, file_size, base_name, FALSE);
	}
	else if(file_size >= SARMAG && strncmp(file_addr, ARMAG, SARMAG) == 0){
	    pass1_archive(file_name, file_addr, file_size, base_name, FALSE);
	}
	else{
	    pass1_object(file_name, file_addr, file_size, base_name, FALSE,
			 FALSE);
	}
}

#ifndef RLD
/*
 * search_for_file() takes base_name and trys to open a file with that base name
 * in the -L search directories and in the standard directories.  If it is
 * sucessful it returns a pointer to the file name indirectly through file_name
 * and the open file descriptor indirectly through fd.
 */
static
void
search_for_file(
char *base_name,
char **file_name,
int *fd)
{
    unsigned long i;

	*fd = -1;
	for(i = 0; i < nsearch_dirs ; i++){
	    *file_name = mkstr(search_dirs[i], "/", base_name, NULL);
	    if((*fd = open(*file_name, O_RDONLY, 0)) != -1)
		break;
	    free(*file_name);
	}
	if(*fd == -1){
	    for(i = 0; standard_dirs[i] != NULL ; i++){
		*file_name = mkstr(standard_dirs[i], base_name, NULL);
		if((*fd = open(*file_name, O_RDONLY, 0)) != -1)
		    break;
		free(*file_name);
	    }
	}
}

/*
 * search_for_framework() takes name and trys to open a file with that name
 * in the -F search directories and in the standard framework directories.  If
 * it is sucessful it returns a pointer to the file name indirectly through
 * file_name and the open file descriptor indirectly through fd.
 */
static
void
search_for_framework(
char *name,
char **file_name,
int *fd)
{
    unsigned long i;

	*fd = -1;
	for(i = 0; i < nframework_dirs ; i++){
	    *file_name = mkstr(framework_dirs[i], "/", name, NULL);
	    if((*fd = open(*file_name, O_RDONLY, 0)) != -1)
		break;
	    free(*file_name);
	}
	if(*fd == -1){
	    for(i = 0; standard_framework_dirs[i] != NULL ; i++){
		*file_name = mkstr(standard_framework_dirs[i], name, NULL);
		if((*fd = open(*file_name, O_RDONLY, 0)) != -1)
		    break;
		free(*file_name);
	    }
	}
}
#endif /* !defined(RLD) */

#ifndef RLD
/*
 * This is used in this file if we have told PB that there is an "Architecture
 * mismatch".
 */
static enum bool told_ProjectBuilder = FALSE;
#endif

/*
 * pass1_fat() is passed a fat file to process.  The reason the swapping of
 * the fat headers is not done in place is so that when running native on a
 * little endian machine and the output is also little endian we don't want
 * cause the memory for the input files to be written just because of the
 * fat headers.
 */
static
void
pass1_fat(
char *file_name,
char *file_addr,
unsigned long file_size,
enum bool base_name,
enum bool dylib_only)
{
    struct fat_header *fat_header;
#ifdef __LITTLE_ENDIAN__
    struct fat_header struct_fat_header;
#endif /* __LITTLE_ENDIAN__ */
    struct fat_arch *fat_archs, *best_fat_arch;
    unsigned long previous_errors;
    char *arch_addr;
    unsigned long arch_size;
    struct arch_flag host_arch_flag;
    const char *prev_arch;

	fat_header = (struct fat_header *)file_addr;
#ifdef __LITTLE_ENDIAN__
	struct_fat_header = *fat_header;
	swap_fat_header(&struct_fat_header, host_byte_sex);
	fat_header = &struct_fat_header;
#endif /* __LITTLE_ENDIAN__ */

	if(sizeof(struct fat_header) + fat_header->nfat_arch *
	   sizeof(struct fat_arch) > file_size){
	    error("fat file: %s truncated or malformed (fat_arch structs "
		  "would extend past the end of the file)", file_name);
	    return;
	}

#ifdef __BIG_ENDIAN__
	fat_archs = (struct fat_arch *)(file_addr + sizeof(struct fat_header));
#endif /* __BIG_ENDIAN__ */
#ifdef __LITTLE_ENDIAN__
	fat_archs = allocate(fat_header->nfat_arch * sizeof(struct fat_arch));
	memcpy(fat_archs, file_addr + sizeof(struct fat_header),
	       fat_header->nfat_arch * sizeof(struct fat_arch));
	swap_fat_arch(fat_archs, fat_header->nfat_arch, host_byte_sex);
#endif /* __LITTLE_ENDIAN__ */

	/*
	 * save the previous errors and only return out of here if something
	 * we do in here gets an error.
	 */
	previous_errors = errors;
	errors = 0;

	/* check the fat file */
	check_fat(file_name, file_size, fat_header, fat_archs, NULL, 0);
	if(errors)
	    goto pass1_fat_return;

	/* Now select an architecture out of the fat file to use if any */

	/*
	 * If the output file's cputype has been set then load the best fat_arch
	 * from it else it's an error.
	 */
	if(arch_flag.cputype != 0){
	    best_fat_arch = cpusubtype_findbestarch(
					arch_flag.cputype, arch_flag.cpusubtype,
					fat_archs, fat_header->nfat_arch);
	    if(best_fat_arch == NULL){
		if(no_arch_warnings == TRUE)
		    goto pass1_fat_return;
		if(arch_flag.name != NULL){
		    if(arch_errors_fatal == TRUE){
			error("fat file: %s does not contain an architecture "
			      "that matches the specified -arch flag: %s ",
			      file_name, arch_flag.name);
#ifndef RLD
			if(told_ProjectBuilder == FALSE){
			    tell_ProjectBuilder("Architecture mismatch");
			    told_ProjectBuilder = TRUE;
			}
#endif
		    }
		    else
			warning("fat file: %s does not contain an architecture "
				"that matches the specified -arch flag: %s "
				"(file ignored)", file_name, arch_flag.name);
		}
		else{
		    prev_arch = get_arch_name_from_types(arch_flag.cputype,
						         arch_flag.cpusubtype);
		    if(arch_errors_fatal == TRUE){
			error("fat file: %s does not contain an architecture "
			    "that matches the objects files (architecture %s) "
			    "previously loaded", file_name, prev_arch);
#ifndef RLD
			if(told_ProjectBuilder == FALSE){
			    tell_ProjectBuilder("Architecture mismatch");
			    told_ProjectBuilder = TRUE;
			}
#endif
		    }
		    else
			warning("fat file: %s does not contain an architecture "
			    "that matches the objects files (architecture %s) "
			    "previously loaded (file ignored)", file_name,
			    prev_arch);
		}
		goto pass1_fat_return;
	    }
	    arch_addr = file_addr + best_fat_arch->offset;
	    arch_size = best_fat_arch->size;
	    if(arch_size >= SARMAG &&
	       strncmp(arch_addr, ARMAG, SARMAG) == 0){
		if(dylib_only == TRUE){
		    if(arch_flag.name != NULL){
			error("fat file: %s (for architecture %s) is not a "
			      "dynamic shared library", file_name,
			      arch_flag.name);
		    }
		    else{
			prev_arch = get_arch_name_from_types(arch_flag.cputype,
							 arch_flag.cpusubtype);
			error("fat file: %s (for architecture %s) is not a "
			      "dynamic shared library", file_name, prev_arch);
		    }
		    goto pass1_fat_return;
		}
		pass1_archive(file_name, arch_addr, arch_size,
			      base_name, TRUE);
	    }
	    else{
		pass1_object(file_name, arch_addr, arch_size, base_name, TRUE,
			     dylib_only);
	    }
	    goto pass1_fat_return;
	}

	/*
	 * If the output file's cputype has not been set so if this fat file
	 * has exactly one type in it then load that type.
	 */
	if(fat_header->nfat_arch == 1){
	    arch_addr = file_addr + fat_archs[0].offset;
	    arch_size = fat_archs[0].size;
	    if(arch_size >= SARMAG &&
	       strncmp(arch_addr, ARMAG, SARMAG) == 0){
		if(dylib_only == TRUE){
		    error("fat file: %s (for architecture %s) is not a dynamic "
			  "shared library", file_name, get_arch_name_from_types(
			   fat_archs[0].cputype, fat_archs[0].cpusubtype));
		    goto pass1_fat_return;
		}
		pass1_archive(file_name, arch_addr, arch_size, base_name, TRUE);
	    }
	    else{
		pass1_object(file_name, arch_addr, arch_size, base_name, TRUE,
			     dylib_only);
	    }
	    goto pass1_fat_return;
	}

	/*
	 * The output file's cputype has not been set and if this fat file has
	 * a best arch for the host's specific architecture type then load that
	 * type and set the output file's cputype to that.
	 */
	if(get_arch_from_host(NULL, &host_arch_flag) == 0)
	    fatal("can't determine the host architecture (specify an "
		  "-arch flag or fix get_arch_from_host() )");
	best_fat_arch = cpusubtype_findbestarch(
			    host_arch_flag.cputype, host_arch_flag.cpusubtype,
			    fat_archs, fat_header->nfat_arch);
	if(best_fat_arch != NULL){
	    arch_addr = file_addr + best_fat_arch->offset;
	    arch_size = best_fat_arch->size;
	    if(arch_size >= SARMAG &&
	       strncmp(arch_addr, ARMAG, SARMAG) == 0){
		if(dylib_only == TRUE){
		    error("fat file: %s (for architecture %s) is not a dynamic "
			  "shared library", file_name, get_arch_name_from_types(
			   best_fat_arch->cputype, best_fat_arch->cpusubtype));
		    goto pass1_fat_return;
		}
		pass1_archive(file_name, arch_addr, arch_size,
			      base_name, TRUE);
	    }
	    else{
		pass1_object(file_name, arch_addr, arch_size, base_name, TRUE,
			     dylib_only);
	    }
	    goto pass1_fat_return;
	}

	/*
	 * The output file's cputype has not been set and this fat file does not
	 * have only one architecture or has the host's family architecture so
	 * we are stuck not knowing what to load if anything from it.
	 */
	fatal("-arch flag must be specified (fat file: %s does not contain the "
	      "host architecture or just one architecture)", file_name);
	
pass1_fat_return:
	errors += previous_errors;
#ifdef __LITTLE_ENDIAN__
	free(fat_archs);
#endif /* __LITTLE_ENDIAN__ */
	return;
}

__private_extern__
void
check_fat(
char *file_name,
unsigned long file_size,
struct fat_header *fat_header,
struct fat_arch *fat_archs,
char *ar_name,
unsigned long ar_name_size)
{
    unsigned long i, j;

	if(fat_header->nfat_arch == 0){
	    if(ar_name != NULL)
		error("fat file: %s(%.*s) malformed (contains zero "
		      "architecture types)", file_name, (int)ar_name_size,
		      ar_name);
	    else
		error("fat file: %s malformed (contains zero architecture "
		      "types)", file_name);
	    return;
	}
	for(i = 0; i < fat_header->nfat_arch; i++){
	    if(fat_archs[i].offset + fat_archs[i].size > file_size){
		if(ar_name != NULL)
		    error("fat file: %s(%.*s) truncated or malformed (offset "
			  "plus size of cputype (%d) cpusubtype (%d) extends "
			  "past the end of the file)", file_name,
			  (int)ar_name_size, ar_name, fat_archs[i].cputype,
			  fat_archs[i].cpusubtype);
		else
		    error("fat file: %s truncated or malformed (offset plus "
			  "size of cputype (%d) cpusubtype (%d) extends past "
			  "the end of the file)", file_name,
			  fat_archs[i].cputype, fat_archs[i].cpusubtype);
		return;
	    }
	    if(fat_archs[i].cputype == 0){
		if(ar_name != NULL)
		    error("fat file: %s(%.*s) fat_archs %lu cputype is zero (a "
			  "reserved value)", file_name, (int)ar_name_size,
			  ar_name, i);
		else
		    error("fat file: %s fat_archs %lu cputype is zero (a "
			  "reserved value)", file_name, i);
		return;
	    }
	    if(fat_archs[i].align > MAXSECTALIGN){
		if(ar_name != NULL)
		    error("fat file: %s(%.*s) align (2^%lu) too large for "
			  "cputype (%d) cpusubtype (%d) (maximum 2^%d)",
			  file_name, (int)ar_name_size, ar_name,
			  fat_archs[i].align, fat_archs[i].cputype,
			  fat_archs[i].cpusubtype, MAXSECTALIGN);
		else
		    error("fat file: %s align (2^%lu) too large for cputype "
			  "(%d) cpusubtype (%d) (maximum 2^%d)", file_name,
			  fat_archs[i].align, fat_archs[i].cputype,
			  fat_archs[i].cpusubtype, MAXSECTALIGN);
		return;
	    }
	    if(fat_archs[i].offset %
	       (1 << fat_archs[i].align) != 0){
		if(ar_name != NULL)
		    error("fat file: %s(%.*s) offset: %lu for cputype (%d) "
			  "cpusubtype (%d)) not aligned on it's alignment "
			  "(2^%lu)", file_name, (int)ar_name_size, ar_name,
			  fat_archs[i].offset, fat_archs[i].cputype,
			  fat_archs[i].cpusubtype, fat_archs[i].align);
		else
		    error("fat file: %s offset: %lu for cputype (%d) "
			  "cpusubtype (%d)) not aligned on it's alignment "
			  "(2^%lu)", file_name, fat_archs[i].offset,
			  fat_archs[i].cputype, fat_archs[i].cpusubtype,
			  fat_archs[i].align);
		return;
	    }
	}
	for(i = 0; i < fat_header->nfat_arch; i++){
	    for(j = i + 1; j < fat_header->nfat_arch; j++){
		if(fat_archs[i].cputype == fat_archs[j].cputype &&
		   fat_archs[i].cpusubtype == fat_archs[j].cpusubtype){
		    if(ar_name != NULL)
			error("fat file: %s(%.*s) contains two of the same "
			      "architecture (cputype (%d) cpusubtype (%d))",
			      file_name, (int)ar_name_size, ar_name,
			      fat_archs[i].cputype, fat_archs[i].cpusubtype);
		    else
			error("fat file: %s contains two of the same "
			      "architecture (cputype (%d) cpusubtype (%d))",
			      file_name, fat_archs[i].cputype,
			      fat_archs[i].cpusubtype);
		    return;
		}
	    }
	}
	return;
}

/*
 * This is an archive so conditionally load those objects that defined
 * currently undefined symbols and process archives with respect to the
 * -ObjC and -load_all flags if set.
 */
static
void
pass1_archive(
char *file_name,
char *file_addr,
unsigned long file_size,
enum bool base_name,
enum bool from_fat_file)
{
    unsigned long i, j, offset;
#ifndef RLD
    unsigned long *loaded_offsets, nloaded_offsets;
    enum bool loaded_offset;
    struct ar_hdr *ar_hdr;
    unsigned long length;
    struct dynamic_library *p;
#endif !defined(RLD)
    struct ar_hdr *symdef_ar_hdr;
    char *symdef_ar_name, *ar_name;
    unsigned long symdef_length, nranlibs, string_size, ar_name_size;
    struct ranlib *ranlibs, *ranlib;
    struct undefined_list *undefined;
    struct merged_symbol *merged_symbol;
    enum bool member_loaded;
    enum byte_sex toc_byte_sex;
    enum bool rc_trace_archive_printed;

#ifndef RLD
    unsigned long ar_size;
    struct fat_header *fat_header;
#ifdef __LITTLE_ENDIAN__
    struct fat_header struct_fat_header;
#endif /* __LITTLE_ENDIAN__ */
    struct fat_arch *fat_archs, *best_fat_arch;
    char *arch_addr;
    unsigned long arch_size;
    struct arch_flag host_arch_flag;
    const char *prev_arch;

	arch_addr = NULL;
	arch_size = 0;
#endif /* !defined(RLD) */
#ifdef __MWERKS__
	{
	    enum bool dummy;
		dummy = base_name;
		dummy = from_fat_file;
	}
#endif

	rc_trace_archive_printed = FALSE;
	if(check_archive_arch(file_name, file_addr, file_size) == FALSE)
	    return;

	offset = SARMAG;
#ifndef RLD
	if(base_name)
	    fatal("base file of incremental link (argument of -A): %s should't "
		  "be an archive", file_name);

	/*
	 * If the flag to specifiy that all the archive members are to be
	 * loaded then load them all.
	 */
	if(archive_all == TRUE){
	    if(offset + sizeof(struct ar_hdr) > file_size){
		error("truncated or malformed archive: %s (archive header of "
		      "first member extends past the end of the file, can't "
		      "load from it)", file_name);
		return;
	    }
	    symdef_ar_hdr = (struct ar_hdr *)(file_addr + offset);
	    if(strncmp(symdef_ar_hdr->ar_name, AR_EFMT1,
		       sizeof(AR_EFMT1)-1) == 0)
		ar_name = file_addr + offset + sizeof(struct ar_hdr);
	    else
		ar_name = symdef_ar_hdr->ar_name;
	    if(strncmp(ar_name, SYMDEF, sizeof(SYMDEF)-1) == 0){
		offset += sizeof(struct ar_hdr);
		symdef_length = strtol(symdef_ar_hdr->ar_size, NULL, 10);
		if(offset + symdef_length > file_size){
		    error("truncated or malformed archive: %s (table of "
			  "contents extends past the end of the file, can't "
			  "load from it)", file_name);
		    return;
		}
		offset += round(symdef_length, sizeof(short));
	    }
	    if(rc_trace_archives == TRUE && rc_trace_archive_printed == FALSE){
		print("[Logging for Build & Integration] Used static archive: "
		      "%s\n", file_name);
		rc_trace_archive_printed = TRUE;
	    }
	    while(offset < file_size){
		if(offset + sizeof(struct ar_hdr) > file_size){
		    error("truncated or malformed archive: %s at offset %lu "
			  "(archive header of next member extends past the end "
			  "of the file, can't load from it)", file_name,offset);
		    return;
		}
		ar_hdr = (struct ar_hdr *)(file_addr + offset);
		if(strncmp(ar_hdr->ar_name, AR_EFMT1, sizeof(AR_EFMT1)-1) == 0){
		    ar_name = file_addr + offset + sizeof(struct ar_hdr);
		    ar_name_size = strtoul(ar_hdr->ar_name +
					   sizeof(AR_EFMT1) - 1, NULL, 10);
		    i = ar_name_size;
		}
		else{
		    ar_name = ar_hdr->ar_name;
		    ar_name_size = 0;
		    i = size_ar_name(ar_hdr);
		}
		offset += sizeof(struct ar_hdr) + ar_name_size;
		ar_size = strtol(ar_hdr->ar_size, NULL, 10) - ar_name_size;
		if(offset + ar_size > file_size){
		    error("truncated or malformed archive: %s (member %.*s "
			  "extends past the end of the file, can't load from "
			  "it)", file_name, (int)i, ar_name);
		    return;
		}
		/*
		 * For -all_load we allow fat files as archive members, so if
		 * this is a fat file load the right architecture.
		 */
		fat_header = (struct fat_header *)(file_addr + offset);
		if(ar_size >= sizeof(struct fat_header) &&
#ifdef __BIG_ENDIAN__
		   fat_header->magic == FAT_MAGIC)
#endif
#ifdef __LITTLE_ENDIAN__
		   fat_header->magic == SWAP_LONG(FAT_MAGIC))
#endif
		{
#ifdef __LITTLE_ENDIAN__
		    struct_fat_header = *fat_header;
		    swap_fat_header(&struct_fat_header, host_byte_sex);
		    fat_header = &struct_fat_header;
#endif
		    if(sizeof(struct fat_header) + fat_header->nfat_arch *
		       sizeof(struct fat_arch) > ar_size){
			error("fat file: %s(%.*s) truncated or malformed "
			    "(fat_arch structs would extend past the end of "
			    "the file)", file_name, (int)i, ar_name);
			return;
		    }
#ifdef __BIG_ENDIAN__
		    fat_archs = (struct fat_arch *)(file_addr + offset +
				sizeof(struct fat_header));
#endif
#ifdef __LITTLE_ENDIAN__
		    fat_archs = allocate(fat_header->nfat_arch *
					 sizeof(struct fat_arch));
		    memcpy(fat_archs, file_addr + offset +
				      sizeof(struct fat_header),
			   fat_header->nfat_arch * sizeof(struct fat_arch));
		    swap_fat_arch(fat_archs, fat_header->nfat_arch,
				  host_byte_sex);
#endif
		    /* check the fat file */
		    check_fat(file_name, ar_size, fat_header, fat_archs,
			      ar_name, i);
		    if(errors)
			return;

		    /* Now select an architecture out of the fat file to use */

		    /*
		     * If the output file's cputype has been set then load the
		     * best fat_arch from it else it's an error.
		     */
		    if(arch_flag.cputype != 0){
			best_fat_arch = cpusubtype_findbestarch(
					arch_flag.cputype, arch_flag.cpusubtype,
					fat_archs, fat_header->nfat_arch);
			if(best_fat_arch == NULL){
			    if(no_arch_warnings == TRUE)
				return;
			    if(arch_flag.name != NULL){
				if(arch_errors_fatal == TRUE){
				    error("fat file: %s(%.*s) does not contain "
					"an architecture that matches the "
					"specified -arch flag: %s", file_name,
					(int)i, ar_name, arch_flag.name);
#ifndef RLD
				    if(told_ProjectBuilder == FALSE){
					tell_ProjectBuilder("Architecture "
					    "mismatch");
					told_ProjectBuilder = TRUE;
				    }
#endif
				}
				else
				    warning("fat file: %s(%.*s) does not "
					"contain an architecture that matches "
					"the specified -arch flag: %s (file "
					"ignored)", file_name, (int)i,
					ar_name, arch_flag.name);
			    }
			    else{
				prev_arch = get_arch_name_from_types(
				    arch_flag.cputype, arch_flag.cpusubtype);
				if(arch_errors_fatal == TRUE){
				    error("fat file: %s(%.*s) does not contain "
					"an architecture that matches the "
					"objects files (architecture %s) "
					"previously loaded", file_name,
					(int)i, ar_name, prev_arch);
#ifndef RLD
				    if(told_ProjectBuilder == FALSE){
					tell_ProjectBuilder("Architecture "
					    "mismatch");
					told_ProjectBuilder = TRUE;
				    }
#endif
				}
				else
				    warning("fat file: %s(%.*s) does not "
					"contain an architecture that matches "
					"the objects files (architecture %s) "
					"previously loaded (file ignored)",
					file_name, (int)i, ar_name, prev_arch);
			    }
			    return;
			}
			arch_addr = file_addr + offset + best_fat_arch->offset;
			arch_size = best_fat_arch->size;
		    }
		    /*
		     * The output file's cputype has not been set so if this
		     * fat file has exactly one type in it then load that type.
		     */
		    else if(fat_header->nfat_arch == 1){
			arch_addr = file_addr + offset + fat_archs[0].offset;
			arch_size = fat_archs[0].size;
		    }
		    /*
		     * The output file's cputype has not been set and if this
		     * fat file has a best arch for the host's specific
		     * architecture type then load that type and set the output
		     * file's cputype to that.
		     */
		    else{
			if(get_arch_from_host(NULL, &host_arch_flag) == 0)
			    fatal("can't determine the host architecture "
				"(specify an -arch flag or fix "
				"get_arch_from_host() )");
			best_fat_arch = cpusubtype_findbestarch(
			    host_arch_flag.cputype, host_arch_flag.cpusubtype,
			    fat_archs, fat_header->nfat_arch);
			if(best_fat_arch != NULL){
			    arch_addr = file_addr + offset +
					best_fat_arch->offset;
			    arch_size = best_fat_arch->size;
			}
			/*
			 * The output file's cputype has not been set and this
			 * fat file does not have only one architecture or has
			 * the host's family architecture so we are stuck not
			 * knowing what to load if anything from it.
			 */
			else
			    fatal("-arch flag must be specified (fat file: "
				"%s(%.*s) does not contain the host "
				"architecture or just one architecture)",
				file_name, (int)i, ar_name);
		    }
		    cur_obj = new_object_file();
		    cur_obj->file_name = file_name;
		    cur_obj->obj_addr = arch_addr;
		    cur_obj->obj_size = arch_size;
		    cur_obj->ar_hdr = ar_hdr;
		    cur_obj->ar_name = ar_name;
		    cur_obj->ar_name_size = i;
#ifdef __LITTLE_ENDIAN__
		    free(fat_archs);
#endif
		    goto down;
		}
		cur_obj = new_object_file();
		cur_obj->file_name = file_name;
		cur_obj->obj_addr = file_addr + offset;
		cur_obj->ar_hdr = ar_hdr;
		cur_obj->ar_name = ar_name;
		cur_obj->ar_name_size = i;
		cur_obj->obj_size = ar_size;
down:
		if(whyload){
		    print_obj_name(cur_obj);
		    print("loaded because of -all_load flag\n");
		}
		merge(FALSE);
		length = round(ar_size + ar_name_size, sizeof(short));
		offset = (offset - ar_name_size) + length;
	    }
	    return;
	}
#endif !defined(RLD)

	/*
	 * If there are no undefined symbols then the archive doesn't have
	 * to be searched because archive members are only loaded to resolve
	 * undefined references unless the -ObjC flag is set.
	 */
	if(undefined_list.next == &undefined_list && archive_ObjC == FALSE)
	    return;

#ifdef RLD
	if(from_fat_file == FALSE)
	    new_archive_or_fat(file_name, file_addr, file_size);
#endif RLD
	/*
	 * The file is an archive so get the symdef file
	 */
	if(offset == file_size){
	    warning("empty archive: %s (can't load from it)", file_name);
	    return;
	}
	if(offset + sizeof(struct ar_hdr) > file_size){
	    error("truncated or malformed archive: %s (archive header of first "
		  "member extends past the end of the file, can't load from "
		  " it)", file_name);
	    return;
	}
	symdef_ar_hdr = (struct ar_hdr *)(file_addr + offset);
	if(strncmp(symdef_ar_hdr->ar_name, AR_EFMT1, sizeof(AR_EFMT1)-1) == 0){
	    symdef_ar_name = file_addr + offset + sizeof(struct ar_hdr);
	    ar_name_size = strtoul(symdef_ar_hdr->ar_name +
				   sizeof(AR_EFMT1) - 1, NULL, 10);
	}
	else{
	    symdef_ar_name = symdef_ar_hdr->ar_name;
	    ar_name_size = 0;
	}
	offset += sizeof(struct ar_hdr) + ar_name_size;
	if(strncmp(symdef_ar_name, SYMDEF, sizeof(SYMDEF) - 1) != 0){
	    error("archive: %s has no table of contents, add one with "
		  "ranlib(1) (can't load from it)", file_name);
	    return;
	}
	if(stat_buf.st_mtime > strtol(symdef_ar_hdr->ar_date, NULL, 10)){
	    error("table of contents for archive: %s is out of date; rerun "
		  "ranlib(1) (can't load from it)", file_name);
	    return;
	}
	symdef_length = strtol(symdef_ar_hdr->ar_size, NULL, 10) - ar_name_size;
	/*
	 * The contents of a __.SYMDEF file is begins with a word giving the
	 * size in bytes of ranlib structures which immediately follow, and
	 * then continues with a string table consisting of a word giving the
	 * number of bytes of strings which follow and then the strings
	 * themselves.  So the smallest valid size is two words long.
	 */
	if(symdef_length < 2 * sizeof(long)){
	    error("size of table of contents for archive: %s too small to be "
		  "a valid table of contents (can't load from it)", file_name);
	    return;
	}
	if(offset + symdef_length > file_size){
	    error("truncated or malformed archive: %s (table of contents "
		  "extends past the end of the file, can't load from it)",
		  file_name);
	    return;
	}
	toc_byte_sex = get_toc_byte_sex(file_addr, file_size);
	nranlibs = *((long *)(file_addr + offset));
	if(toc_byte_sex != host_byte_sex)
	    nranlibs = SWAP_LONG(nranlibs);
	nranlibs = nranlibs / sizeof(struct ranlib);
	offset += sizeof(long);
	ranlibs = (struct ranlib *)(file_addr + offset);
	offset += sizeof(struct ranlib) * nranlibs;
	if(nranlibs == 0){
	    warning("empty table of contents: %s (can't load from it)",
		    file_name);
	    return;
	}
	if(offset - (2 * sizeof(long) + ar_name_size + sizeof(struct ar_hdr) +
		     SARMAG) > symdef_length){
	    error("truncated or malformed archive: %s (ranlib structures in "
		  "table of contents extends past the end of the table of "
		  "contents, can't load from it)", file_name);
	    return;
	}
	string_size = *((long *)(file_addr + offset));
	if(toc_byte_sex != host_byte_sex)
	    string_size = SWAP_LONG(string_size);
	offset += sizeof(long);
	strings = file_addr + offset;
	offset += string_size;
	if(offset - (2 * sizeof(long) + ar_name_size + sizeof(struct ar_hdr) +
		     SARMAG) > symdef_length){
	    error("truncated or malformed archive: %s (ranlib strings in "
		  "table of contents extends past the end of the table of "
		  "contents, can't load from it)", file_name);
	    return;
	}
	if(symdef_length == 2 * sizeof(long)){
	    warning("empty table of contents for archive: %s (can't load from "
		    "it)", file_name);
	    return;
	}

	/*
	 * Check the string offset and the member offsets of the ranlib structs.
	 */
	if(toc_byte_sex != host_byte_sex)
	    swap_ranlib(ranlibs, nranlibs, host_byte_sex);
	for(i = 0; i < nranlibs; i++){
	    if(ranlibs[i].ran_un.ran_strx >= (long)string_size){
		error("malformed table of contents in: %s (ranlib struct %lu "
		      "has bad string index, can't load from it)", file_name,i);
		return;
	    }
	    if(ranlibs[i].ran_off + sizeof(struct ar_hdr) >= file_size){
		error("malformed table of contents in: %s (ranlib struct %lu "
		      "has bad library member offset, can't load from it)",
		      file_name, i);
		return;
	    }
	    /*
	     * These should be on 4 byte boundaries because the maximum
	     * alignment of the header structures and relocation are 4 bytes.
	     * But this is has to be 2 bytes because that's the way ar(1) has
	     * worked historicly in the past.  Fortunately this works on the
	     * 68k machines but will have to change when this is on a real
	     * machine.
	     */
#if defined(mc68000) || defined(__i386__)
	    if(ranlibs[i].ran_off % sizeof(short) != 0){
		error("malformed table of contents in: %s (ranlib struct %lu "
		      "library member offset not a multiple of %lu bytes, can't"
		      " load from it)", file_name, i, sizeof(short));
		return;
	    }
#else
	    if(ranlibs[i].ran_off % sizeof(long) != 0){
		error("malformed table of contents in: %s (ranlib struct %lu "
		      "library member offset not a multiple of %lu bytes, can't"
		      " load from it)", file_name, i, sizeof(long));
		return;
	    }
#endif
	}

#ifndef RLD
	/*
	 * If the objective-C flag is set then load every thing in this archive
	 * that defines a symbol that starts with ".objc_class_name" or
	 * ".objc_category_name".
 	 */
	if(archive_ObjC == TRUE){
	    loaded_offsets = allocate(nranlibs * sizeof(unsigned long));
	    nloaded_offsets = 0;
	    for(i = 0; i < nranlibs; i++){
		/* See if this symbol is an objective-C symbol */
		if(strncmp(strings + ranlibs[i].ran_un.ran_strx,
		           ".objc_class_name",
			   sizeof(".objc_class_name") - 1) != 0 &&
		   strncmp(strings + ranlibs[i].ran_un.ran_strx,
		           ".objc_category_name",
			   sizeof(".objc_category_name") - 1) != 0)
		    continue;

		/* See if the object at this offset has already been loaded */
		loaded_offset = FALSE;
		for(j = 0; j < nloaded_offsets; j++){
		    if(loaded_offsets[j] == ranlibs[i].ran_off){
			loaded_offset = TRUE;
			break;
		    }
		}
		if(loaded_offset == TRUE)
		    continue;
		loaded_offsets[nloaded_offsets++] = ranlibs[i].ran_off;

		if(rc_trace_archives == TRUE &&
		   rc_trace_archive_printed == FALSE){
		    print("[Logging for Build & Integration] Used static "
			  "archive: %s\n", file_name);
		    rc_trace_archive_printed = TRUE;
		}
		/*
		 * This is an objective-C symbol and the object file at this
		 * offset has not been loaded so load it.
		 */
		cur_obj = new_object_file();
		cur_obj->file_name = file_name;
		cur_obj->ar_hdr = (struct ar_hdr *)(file_addr +
					    ranlibs[i].ran_off);
		if(strncmp(cur_obj->ar_hdr->ar_name, AR_EFMT1,
			   sizeof(AR_EFMT1) - 1) == 0){
		    ar_name = file_addr + ranlibs[i].ran_off +
			      sizeof(struct ar_hdr);
		    ar_name_size = strtoul(cur_obj->ar_hdr->ar_name +
					   sizeof(AR_EFMT1) - 1, NULL, 10);
		    j = ar_name_size;
		}
		else{
		    ar_name = cur_obj->ar_hdr->ar_name;
		    ar_name_size = 0;
		    j = size_ar_name(cur_obj->ar_hdr);
		}
		cur_obj->ar_name = ar_name;
		cur_obj->ar_name_size = j;
		cur_obj->obj_addr = file_addr + ranlibs[i].ran_off +
				    sizeof(struct ar_hdr) + ar_name_size;
		cur_obj->obj_size = strtol(cur_obj->ar_hdr->ar_size,
					   NULL, 10) - ar_name_size;
		if(ranlibs[i].ran_off + sizeof(struct ar_hdr) + ar_name_size +
				    cur_obj->obj_size > file_size){
		    error("malformed library: %s (member %.*s "
			  "extends past the end of the file, can't "
			  "load from it)", file_name, (int)j, ar_name);
		    return;
		}
		if(whyload){
		    print_obj_name(cur_obj);
		    print("loaded because of -ObjC flag to get symbol: %s\n", 
			  strings + ranlibs[i].ran_un.ran_strx);
		}
		merge(FALSE);
	    }
	    free(loaded_offsets);
	}

	/*
	 * If a dynamic library has been referenced then this archive library
	 * is put on the dynamic library search list and it will be loaded
	 * from with dynamic library search semantics.
	 */
	if(dynamic_libs != NULL){
	    p = add_dynamic_lib(SORTED_ARCHIVE, NULL, NULL);
	    if(strncmp(symdef_ar_name, SYMDEF_SORTED,
		       sizeof(SYMDEF_SORTED) - 1) == 0){
		p->type = SORTED_ARCHIVE;
/*
 * With the 4.1mach patch and 4.2mach release, we are putting the libgcc
 * functions into a static archive (libcc_dynamic.a) which we will link into
 * every image.  So this obscure warning message would then be seen on nearly
 * every link.  So the decision is to just remove the warning message.
 */
#ifdef notdef
		warning("archive library: %s appears after reference to "
			"dynamic shared library and will be searched as a "
			"dynamic shared library which only the first member "
			"that defines a symbol will ever be loaded", file_name);
#endif /* notdef */
	    }
	    else{
		p->type = UNSORTED_ARCHIVE;
#ifdef notdef
		warning("table of contents of library: %s not sorted slower "
			"link editing will result (use the ranlib(1) -s "
			"option), also library appears after reference to "
			"dynamic shared library and will be searched as a "
			"dynamic shared library which only the first member "
			"that defines a symbol will ever be loaded", file_name);
#endif /* notdef */
	    }
	    p->file_name = file_name;
	    p->file_addr = file_addr;
	    p->file_size = file_size;
	    p->nranlibs = nranlibs;
	    p->ranlibs = ranlibs;
	    p->ranlib_strings = strings;
	    return;
	}
#endif !defined(RLD)

	/*
	 * Two possible algorithms are used to determine which members from the
	 * archive are to be loaded.  The first is faster and requires the 
	 * ranlib structures to be in sorted order (as produced by the ranlib(1)
	 * -s option).  The only case this can't be done is when more than one
	 * library member in the same archive defines the same symbol.  In this
	 * case ranlib(1) will never sort the ranlib structures but will leave
	 * them in the order of the archive so that the proper member that
	 * defines a symbol that is defined in more that one object is loaded.
	 */
	if(strncmp(symdef_ar_name, SYMDEF_SORTED,
		   sizeof(SYMDEF_SORTED) - 1) == 0){
	    /*
	     * Now go through the undefined symbol list and look up each symbol
	     * in the sorted ranlib structures looking to see it their is a
	     * library member that satisfies this undefined symbol.  If so that
	     * member is loaded and merge() is called.
	     */
	    for(undefined = undefined_list.next;
		undefined != &undefined_list;
		/* no increment expression */){
		/* If this symbol is no longer undefined delete it and move on*/
		if(undefined->merged_symbol->nlist.n_type != (N_UNDF | N_EXT) ||
		   undefined->merged_symbol->nlist.n_value != 0){
		    undefined = undefined->next;
		    delete_from_undefined_list(undefined->prev);
		    continue;
		}
		ranlib = bsearch(undefined->merged_symbol->nlist.n_un.n_name, 
			   ranlibs, nranlibs, sizeof(struct ranlib),
			   (int (*)(const void *, const void *))ranlib_bsearch);
		if(ranlib != NULL){

		    if(rc_trace_archives == TRUE &&
		       rc_trace_archive_printed == FALSE){
			print("[Logging for Build & Integration] Used static "
			      "archive: %s\n", file_name);
			rc_trace_archive_printed = TRUE;
		    }

		    /* there is a member that defineds this symbol so load it */
		    cur_obj = new_object_file();
#ifdef RLD
		    cur_obj->file_name = allocate(strlen(file_name) + 1);
		    strcpy(cur_obj->file_name, file_name);
		    cur_obj->from_fat_file = from_fat_file;
#else
		    cur_obj->file_name = file_name;
#endif RLD
		    cur_obj->ar_hdr = (struct ar_hdr *)(file_addr +
							ranlib->ran_off);
		    if(strncmp(cur_obj->ar_hdr->ar_name, AR_EFMT1,
			       sizeof(AR_EFMT1)-1) == 0){
			ar_name = file_addr + ranlib->ran_off +
				  sizeof(struct ar_hdr);
			ar_name_size = strtoul(cur_obj->ar_hdr->ar_name +
					       sizeof(AR_EFMT1) - 1, NULL, 10);
			j = ar_name_size;
		    }
		    else{
			ar_name = cur_obj->ar_hdr->ar_name;
			ar_name_size = 0;
			j = size_ar_name(cur_obj->ar_hdr);
		    }
		    cur_obj->ar_name = ar_name;
		    cur_obj->ar_name_size = j;
		    cur_obj->obj_addr = file_addr + ranlib->ran_off +
					sizeof(struct ar_hdr) + ar_name_size;
		    cur_obj->obj_size = strtol(cur_obj->ar_hdr->ar_size, NULL,
					       10) - ar_name_size;
		    if(ranlib->ran_off + sizeof(struct ar_hdr) + ar_name_size +
						cur_obj->obj_size > file_size){
			error("malformed library: %s (member %.*s extends past "
			      "the end of the file, can't load from it)",
			      file_name, (int)j, ar_name);
			return;
		    }
		    if(whyload){
			print_obj_name(cur_obj);
			print("loaded to resolve symbol: %s\n", 
			       undefined->merged_symbol->nlist.n_un.n_name);
		    }

		    merge(FALSE);

		    /* make sure this symbol got defined */
		    if(errors == 0 && 
		       undefined->merged_symbol->nlist.n_type == (N_UNDF|N_EXT)
		       && undefined->merged_symbol->nlist.n_value == 0){
			error("malformed table of contents in library: %s "
			      "(member %.*s did not defined symbol %s)",
			      file_name, (int)j, ar_name,
			      undefined->merged_symbol->nlist.n_un.n_name);
		    }
		    undefined = undefined->next;
		    delete_from_undefined_list(undefined->prev);
		    continue;
		}
		undefined = undefined->next;
	    }
	}
	else{
	    /*
	     * The slower algorithm.  Lookup each symbol in the table of
	     * contents to see if is undefined.  If so that member is loaded
	     * and merge() is called.  A complete pass over the table of
	     * contents without loading a member terminates searching
	     * the library.  This could be made faster if this wrote on the
	     * ran_off to indicate the member at that offset was loaded and
	     * then it's symbol would be not be looked up on later passes.
	     * But this is not done because it would dirty the table of contents
	     * and cause the possibility of more swapping and if fast linking is
	     * wanted then the table of contents can be sorted.
	     */
	    warning("table of contents of library: %s not sorted slower link "
		    "editing will result (use the ranlib(1) -s option)",
		    file_name);
	    member_loaded = TRUE;
	    while(member_loaded == TRUE && errors == 0){
		member_loaded = FALSE;
		for(i = 0; i < nranlibs; i++){
		    merged_symbol = *(lookup_symbol(strings +
						   ranlibs[i].ran_un.ran_strx));
		    if(merged_symbol != NULL){
			if(merged_symbol->nlist.n_type == (N_UNDF | N_EXT) &&
			   merged_symbol->nlist.n_value == 0){

			    if(rc_trace_archives == TRUE &&
			       rc_trace_archive_printed == FALSE){
				print("[Logging for Build & Integration] Used "
				      "static archive: %s\n", file_name);
				rc_trace_archive_printed = TRUE;
			    }

			    /*
			     * This symbol is defined in this member so load it.
			     */
			    cur_obj = new_object_file();
#ifdef RLD
			    cur_obj->file_name = allocate(strlen(file_name) +1);
			    strcpy(cur_obj->file_name, file_name);
			    cur_obj->from_fat_file = from_fat_file;
#else
			    cur_obj->file_name = file_name;
#endif RLD
			    cur_obj->ar_hdr = (struct ar_hdr *)(file_addr +
							ranlibs[i].ran_off);
			    if(strncmp(cur_obj->ar_hdr->ar_name, AR_EFMT1,
				       sizeof(AR_EFMT1)-1) == 0){
				ar_name = file_addr + ranlibs[i].ran_off +
					  sizeof(struct ar_hdr);
				ar_name_size =
					strtoul(cur_obj->ar_hdr->ar_name +
					        sizeof(AR_EFMT1) - 1, NULL, 10);
				j = ar_name_size;
			    }
			    else{
				ar_name = cur_obj->ar_hdr->ar_name;
				ar_name_size = 0;
				j = size_ar_name(cur_obj->ar_hdr);
			    }
			    cur_obj->ar_name = ar_name;
			    cur_obj->ar_name_size = j;
			    cur_obj->obj_addr = file_addr + ranlibs[i].ran_off +
					sizeof(struct ar_hdr) + ar_name_size;
			    cur_obj->obj_size = strtol(cur_obj->ar_hdr->ar_size,
						       NULL, 10) - ar_name_size;
			    if(ranlibs[i].ran_off + sizeof(struct ar_hdr) +
			       ar_name_size + cur_obj->obj_size > file_size){
				error("malformed library: %s (member %.*s "
				      "extends past the end of the file, can't "
				      "load from it)", file_name, (int)j,
				      ar_name);
				return;
			    }
			    if(whyload){
				print_obj_name(cur_obj);
				print("loaded to resolve symbol: %s\n", 
				       merged_symbol->nlist.n_un.n_name);
			    }

			    merge(FALSE);

			    /* make sure this symbol got defined */
			    if(errors == 0 &&
			       merged_symbol->nlist.n_type == (N_UNDF | N_EXT)
			       && merged_symbol->nlist.n_value == 0){
				error("malformed table of contents in library: "
				      "%s (member %.*s did not defined "
				      "symbol %s)", file_name, (int)j, ar_name,
				      merged_symbol->nlist.n_un.n_name);
			    }
			    /*
			     * Skip any other symbols that are defined in this
			     * member since it has just been loaded.
			     */
			    for(j = i; j + 1 < nranlibs; j++){
				if(ranlibs[i].ran_off != ranlibs[j + 1].ran_off)
				    break;
			    }
			    i = j;
			    member_loaded = TRUE;
			}
		    }
		}
	    }
	}
}

/*
 * check_archive_arch() check the archive specified to see if it's architecture
 * does not match that of whats being loaded and if so returns FALSE.  Else it
 * returns TRUE and the archive should be attemped to be loaded from.  This is
 * done so that the obvious case of an archive that is the wrong architecture
 * is not reported an object file at a time but rather one error message is
 * printed.
 */
static
enum bool
check_archive_arch(
char *file_name,
char *file_addr,
unsigned long file_size)
{
    unsigned long offset, obj_size, length;
    struct ar_hdr *symdef_ar_hdr, *ar_hdr;
    unsigned long symdef_length, ar_name_size;
    char *obj_addr, *ar_name;
    struct mach_header mh;
    cpu_type_t cputype;
    cpu_subtype_t cpusubtype;
    enum bool mixed_types;
    const char *new_arch, *prev_arch;

	cputype = 0;
	cpusubtype = 0;
	mixed_types = FALSE;

	offset = SARMAG;
	if(offset + sizeof(struct ar_hdr) > file_size){
	    error("truncated or malformed archive: %s (archive header of "
		  "first member extends past the end of the file, can't "
		  "load from it)", file_name);
	    return(FALSE);
	}
	symdef_ar_hdr = (struct ar_hdr *)(file_addr + offset);
	if(strncmp(symdef_ar_hdr->ar_name, AR_EFMT1, sizeof(AR_EFMT1)-1) == 0){
	    if(check_extend_format_1(file_name, symdef_ar_hdr,
				    file_size - (offset +sizeof(struct ar_hdr)),
				    &ar_name_size) == FALSE)
		return(FALSE);
	    ar_name = file_addr + offset + sizeof(struct ar_hdr);
	}
	else{
	    ar_name = symdef_ar_hdr->ar_name;
	    ar_name_size = 0;
	}
	if(strncmp(ar_name, SYMDEF, sizeof(SYMDEF) - 1) == 0){
	    offset += sizeof(struct ar_hdr);
	    symdef_length = strtol(symdef_ar_hdr->ar_size, NULL, 10);
	    if(offset + symdef_length > file_size){
		error("truncated or malformed archive: %s (table of "
		      "contents extends past the end of the file, can't "
		      "load from it)", file_name);
		return(FALSE);
	    }
	    offset += round(symdef_length, sizeof(short));
	}
	while(offset < file_size){
	    if(offset + sizeof(struct ar_hdr) > file_size){
		error("truncated or malformed archive: %s at offset %lu "
		      "(archive header of next member extends past the end "
		      "of the file, can't load from it)", file_name, offset);
		return(FALSE);
	    }
	    ar_hdr = (struct ar_hdr *)(file_addr + offset);
	    offset += sizeof(struct ar_hdr);
	    if(strncmp(ar_hdr->ar_name, AR_EFMT1, sizeof(AR_EFMT1) - 1) == 0){
		if(check_extend_format_1(file_name, ar_hdr, file_size - offset,
					&ar_name_size) == FALSE)
		    return(FALSE);
		ar_name = file_addr + offset;
	    }
	    else{
		ar_name = ar_hdr->ar_name;
		ar_name_size = 0;
	    }
	    offset += ar_name_size;
	    obj_addr = file_addr + offset;
	    obj_size = strtol(ar_hdr->ar_size, NULL, 10);
	    obj_size -= ar_name_size;
	    if(offset + obj_size > file_size){
		error("truncated or malformed archive: %s at offset %lu "
		      "(member extends past the end of the file, can't load "
		      "from it)", file_name, offset);
		return(FALSE);
	    }
	    if(obj_size >= sizeof(struct mach_header)){
		memcpy(&mh, obj_addr, sizeof(struct mach_header));
		if(mh.magic == SWAP_LONG(MH_MAGIC))
		    swap_mach_header(&mh, host_byte_sex);
		if(mh.magic == MH_MAGIC){
		    if(cputype == 0){
			cputype = mh.cputype;
			cpusubtype = mh.cpusubtype;
		    }
		    else if(cputype != mh.cputype){
			mixed_types = TRUE;
		    }
		}
	    }
	    length = round(obj_size, sizeof(short));
	    offset += length;
	}
	if(arch_flag.cputype != 0 && mixed_types == FALSE &&
	   arch_flag.cputype != cputype && cputype != 0){
	    new_arch = get_arch_name_from_types(cputype, cpusubtype);
	    prev_arch = get_arch_name_from_types(arch_flag.cputype,
						 arch_flag.cpusubtype);
	    if(arch_flag.name != NULL)
		warning("%s archive's cputype (%d, architecture %s) does "
		    "not match cputype (%d) for specified -arch flag: "
		    "%s (can't load from it)", file_name, cputype, new_arch,
		    arch_flag.cputype, arch_flag.name);
	    else
		warning("%s archive's cputype (%d, architecture %s) does "
		    "not match cputype (%d architecture %s) of objects "
		    "files previously loaded (can't load from it)", file_name,
		    cputype, new_arch, arch_flag.cputype, prev_arch);
	    return(FALSE);
	}
	return(TRUE);
}

/*
 * check_extend_format_1() checks the archive header for extended format #1.
 */
static
enum bool
check_extend_format_1(
char *name,
struct ar_hdr *ar_hdr,
unsigned long size_left,
unsigned long *member_name_size)
{
    char *p, *endp, buf[sizeof(ar_hdr->ar_name)+1];
    unsigned long ar_name_size;

	*member_name_size = 0;

	buf[sizeof(ar_hdr->ar_name)] = '\0';
	memcpy(buf, ar_hdr->ar_name, sizeof(ar_hdr->ar_name));
	p = buf + sizeof(AR_EFMT1) - 1;
	if(isdigit(*p) == 0){
	    error("truncated or malformed archive: %s (ar_name: %.*s for "
		  "archive extend format #1 starts with non-digit)", name,
		  (int)sizeof(ar_hdr->ar_name), ar_hdr->ar_name);
	    return(FALSE);
	}
	ar_name_size = strtoul(p, &endp, 10);
	if(ar_name_size == ULONG_MAX && errno == ERANGE){
	    error("truncated or malformed archive: %s (size in ar_name: %.*s "
		  "for archive extend format #1 overflows unsigned long)",
		  name, (int)sizeof(ar_hdr->ar_name), ar_hdr->ar_name);
	    return(FALSE);
	}
	while(*endp == ' ' && *endp != '\0')
	    endp++;
	if(*endp != '\0'){
	    error("truncated or malformed archive: %s (size in ar_name: %.*s "
		  "for archive extend format #1 contains non-digit and "
		  "non-space characters)", name, (int)sizeof(ar_hdr->ar_name),
		  ar_hdr->ar_name);
	    return(FALSE);
	}
	if(ar_name_size > size_left){
	    error("truncated or malformed archive: %s (archive name of member "
		  "extends past the end of the file)", name);
	    return(FALSE);
	}
	*member_name_size = ar_name_size;
	return(TRUE);
}


/* This is an object file so it is unconditionally loaded */
static
void
pass1_object(
char *file_name,
char *file_addr,
unsigned long file_size,
enum bool base_name,
enum bool from_fat_file,
enum bool dylib_only)
{
#ifdef __MWERKS__
    enum bool dummy;
        dummy = base_name;
        dummy = from_fat_file;
#endif
	cur_obj = new_object_file();
#ifdef RLD
	cur_obj->file_name = allocate(strlen(file_name) + 1);
	strcpy(cur_obj->file_name, file_name);
	cur_obj->from_fat_file = from_fat_file;
#else
	cur_obj->file_name = file_name;
#endif RLD
	cur_obj->obj_addr = file_addr;
	cur_obj->obj_size = file_size;
#ifndef RLD
	/*
	 * If this is the base file of an incremental link then set the
	 * pointer to the object file.
	 */
	if(base_name == TRUE)
	    base_obj = cur_obj;
#endif !defined(RLD)

	merge(dylib_only);

#ifndef RLD
	/*
	 * If this is the base file of an incremental link then collect it's
	 * segments for overlap checking.
	 */
	if(base_name == TRUE)
	    collect_base_obj_segments();
#endif !defined(RLD)
	return;
}

#ifndef RLD
/*
 * search_dynamic_libs() searches the libraries on the list of dynamic libraries
 * to resolve undefined symbols.  This is mostly done for static checking of
 * undefined symbols.  If an archive library appears after a dynamic library
 * on the static link line then it will be on the list of dynamic libraries
 * to search and be searched with dynamic library search semantics.  Dynamic
 * library search semantics here mimic what happens in the dynamic link editor.
 * For each undefined symbol a module that defines this symbol is searched for
 * throught the list of libraries to be searched.  That is for each symbol the
 * search starts at the begining of the list of libraries to be searched.  This
 * is unlike archive library search semantic when each library is search once
 * when encountered.
 */
__private_extern__
void
search_dynamic_libs(
void)
{
    struct dynamic_library *p, *prev;
    unsigned long i, j, nmodules, size, ar_name_size;
    enum bool removed;
    char *ar_name;

    struct mach_header *mh;
    struct load_command *lc;
    struct dylib_command *dl;
    struct segment_command *sg;

    struct undefined_list *undefined;
    enum bool found;
    struct dylib_table_of_contents *toc;
    struct ranlib *ranlib;

    enum bool bind_at_load_warning;
    struct merged_symbol_list **m, *merged_symbol_list;
    struct merged_symbol *merged_symbol;

	/*
	 * For dynamic libraries on the dynamic library search list that are
	 * from LC_LOAD_DYLIB references convert them into using a dylib file
	 * so it can be searched.  Or remove them from the search list if it
	 * can't be converted.  Then add all the dependent libraries for that
	 * library to the search list.
	 */
	prev = NULL;
	for(p = dynamic_libs; p != NULL; p = p->next){
	    removed = FALSE;
	    /*
	     * If this element on the dynamic library list comes from a
	     * LC_LOAD_DYLIB reference try to convert them into using a dylib
	     * file so it can be searched.  If not take it off the list.
	     */
	    if(p->type == DYLIB && p->dl->cmd == LC_LOAD_DYLIB){
		if(open_dylib(p) == FALSE){
		    if(prebinding == TRUE){
			warning("prebinding disabled because dependent "
			    "library: %s can't be searched", p->dylib_file !=
			    NULL ? p->file_name : p->dylib_name);
			prebinding = FALSE;
		    }
		    /* remove this dynamic library from the search list */
		    if(prev == NULL)
			dynamic_libs = p->next;
		    else
			prev->next = p->next;
		    removed = TRUE;
		}
	    }
	    /*
	     * If this element on the dynamic library list is a dylib file
	     * add all of it's dependent libraries to the list.
	     */
	    if(p->type == DYLIB && p->dl->cmd == LC_ID_DYLIB){
		mh = (struct mach_header *)p->definition_obj->obj_addr;
		if(prebinding == TRUE &&
		   (mh->flags & MH_PREBOUND) != MH_PREBOUND){
		    warning("prebinding disabled because dependent library: %s "
			"is not prebound", p->dylib_file != NULL ?
			p->file_name : p->dylib_name);
			prebinding = FALSE;
		}
		lc = (struct load_command *)
			((char *)p->definition_obj->obj_addr +
			 sizeof(struct mach_header));
		for(i = 0; i < mh->ncmds; i++){
		    if(lc->cmd == LC_LOAD_DYLIB){
			dl = (struct dylib_command *)lc;
			(void)add_dynamic_lib(DYLIB, dl,
							p->definition_obj);
		    }
		    lc = (struct load_command *)((char *)lc + lc->cmdsize);
		}
	    }
	    if(removed == FALSE)
		prev = p;
	}

	/*
	 * Go through the specified dynamic libraries setting up their table of
	 * contents data.
	 */
	for(p = dynamic_libs; p != NULL; p = p->next){
	    if(p->type == DYLIB){
		if(p->dl->cmd == LC_ID_DYLIB){
		    p->tocs = (struct dylib_table_of_contents *)(
			       p->definition_obj->obj_addr +
			       p->definition_obj->dysymtab->tocoff);
		    p->strings = p->definition_obj->obj_addr +
				 p->definition_obj->symtab->stroff;
		    p->symbols = (struct nlist *)(
				  p->definition_obj->obj_addr +
				  p->definition_obj->symtab->symoff);
		    if(p->definition_obj->swapped)
	    		swap_nlist(p->symbols,
				   p->definition_obj->symtab->nsyms,
				   host_byte_sex);
		    p->mods = (struct dylib_module *)(
			       p->definition_obj->obj_addr +
			       p->definition_obj->dysymtab->modtaboff);
		    /*
		     * If prebinding an executable create a LC_PREBOUND_DYLIB
		     * load command for each dynamic library.  To allow the
		     * prebinding to be redone when the library has more
		     * modules the bit vector for the linked modules is padded
		     * to 125% of the number of modules with a minimum of 64
		     * modules.
		     */
		    if(prebinding == TRUE && filetype == MH_EXECUTE){
			nmodules = p->definition_obj->dysymtab->nmodtab +
				   (p->definition_obj->dysymtab->nmodtab >> 2);
			if(nmodules < 64)
			    nmodules = 64;
			size = sizeof(struct prebound_dylib_command) +
			       round(strlen(p->dylib_name) + 1, sizeof(long)) +
			       round(nmodules / 8, sizeof(long));
			p->pbdylib = allocate(size);
			memset(p->pbdylib, '\0', size);
			p->pbdylib->cmd = LC_PREBOUND_DYLIB;
			p->pbdylib->cmdsize = size;
			p->pbdylib->name.offset =
				sizeof(struct prebound_dylib_command);
			strcpy(((char *)p->pbdylib) +
				sizeof(struct prebound_dylib_command),
				p->dylib_name);
			p->pbdylib->nmodules =
				p->definition_obj->dysymtab->nmodtab;
			p->pbdylib->linked_modules.offset =
				sizeof(struct prebound_dylib_command) +
				round(strlen(p->dylib_name) + 1, sizeof(long));
			p->linked_modules = ((char *)p->pbdylib) +
				sizeof(struct prebound_dylib_command) +
				round(strlen(p->dylib_name) + 1, sizeof(long));
		    }
		}
	    }
	}

	/*
	 * If we are going to attempt to prebind we save away the segments of
	 * the dylibs so they can be checked for overlap after layout.
	 */
	if(prebinding == TRUE){
	    for(p = dynamic_libs; p != NULL; p = p->next){
		if(p->type == DYLIB && p->dl->cmd == LC_ID_DYLIB){
		    mh = (struct mach_header *)p->definition_obj->obj_addr;
		    lc = (struct load_command *)
			    ((char *)p->definition_obj->obj_addr +
			     sizeof(struct mach_header));
		    for(i = 0; i < mh->ncmds; i++){
			if(lc->cmd == LC_SEGMENT){
		    	    sg = (struct segment_command *)lc;
			    add_dylib_segment(sg, p->dylib_name);
			}
			lc = (struct load_command *)((char *)lc + lc->cmdsize);
		    }
		}
	    }
	}

#ifdef DEBUG
	if(debug & (1 << 22)){
	    print("dynamic library search list:\n");
	    for(p = dynamic_libs; p != NULL; p = p->next){
		if(p->type == DYLIB){
		    if(p->dylib_file != NULL)
			printf("\t%s (using file %s)\n", p->dylib_name,
			       p->file_name);
		    else
			printf("\t%s\n", p->dylib_name);
		}
		else
		    printf("\t%s (archive)\n", p->file_name);
	    }
	}
#endif
	if(rc_trace_dylibs == TRUE){
	    for(p = dynamic_libs; p != NULL; p = p->next){
		if(p->type == DYLIB){
		    if(p->dylib_file != NULL)
			print("[Logging for Build & Integration] Used dynamic "
			      "library: %s\n", p->file_name);
		    else
			print("[Logging for Build & Integration] Used dynamic "
			      "library: %s\n", p->dylib_name);
		}
	    }
	}

	/*
	 * Now go through the undefined symbol list and look up each symbol
	 * in the table of contents of each library on the dynamic library
	 * search list.
	 */
	for(undefined = undefined_list.next;
	    undefined != &undefined_list;
	    /* no increment expression */){
	    /* If this symbol is no longer undefined delete it and move on*/
	    if(undefined->merged_symbol->nlist.n_type != (N_UNDF | N_EXT) ||
	       undefined->merged_symbol->nlist.n_value != 0){
		undefined = undefined->next;
		delete_from_undefined_list(undefined->prev);
		continue;
	    }
	    found = FALSE;
	    for(p = dynamic_libs; p != NULL && found == FALSE; p = p->next){
		switch(p->type){

		case DYLIB:
		    strings = p->strings;
		    symbols = p->symbols;
		    toc = bsearch(undefined->merged_symbol->nlist.n_un.n_name,
				  p->tocs, p->definition_obj->dysymtab->ntoc,
				  sizeof(struct dylib_table_of_contents),
				  (int (*)(const void *, const void *))
				    dylib_bsearch);
		    if(toc != NULL){
			/*
			 * There is a module that defineds this symbol so
			 * load it.
			 */
			cur_obj = new_object_file();
			*cur_obj = *(p->definition_obj);
			cur_obj->dylib_module = p->mods + toc->module_index;
			if(p->linked_modules != NULL)
			    p->linked_modules[toc->module_index / 8] |=
				1 << toc->module_index % 8;
			if(whyload){
			    print_obj_name(cur_obj);
			    print("loaded to resolve symbol: %s\n", 
			       undefined->merged_symbol->nlist.n_un.n_name);
			}
			merge_dylib_module_symbols();
			/* make sure this symbol got defined */
			if(errors == 0 && 
			   undefined->merged_symbol->nlist.n_type ==
			    (N_UNDF|N_EXT)
			   && undefined->merged_symbol->nlist.n_value == 0){
			    error("malformed table of contents in library: "
			       "%s (module %s did not defined symbol %s)",
			       cur_obj->file_name,
			       strings + cur_obj->dylib_module->module_name,
			       undefined->merged_symbol->nlist.n_un.n_name);
			}
			undefined = undefined->next;
			delete_from_undefined_list(undefined->prev);
			found = TRUE;

			/*
			 * Since something from this dynamic library is being
			 * used, if there is a library initialization routine
			 * make sure that the module that defines it is loaded.
			 */
			if(p->definition_obj->rc != NULL &&
			   p->definition_obj->init_module_loaded == FALSE){
			    if(is_dylib_module_loaded(p->mods +
				  p->definition_obj->rc->init_module) == FALSE){
				cur_obj = new_object_file();
				*cur_obj = *(p->definition_obj);
				cur_obj->dylib_module = p->mods +
				    p->definition_obj->rc->init_module;
				if(p->linked_modules != NULL)
				    p->linked_modules[p->definition_obj->rc->
						  init_module / 8] |= 1 <<
					p->definition_obj->rc->init_module % 8;
				if(whyload){
				    print_obj_name(cur_obj);
				    print("loaded for library initialization "
					  "routine\n");
				}
				merge_dylib_module_symbols();
			    }
			    p->definition_obj->init_module_loaded = TRUE;
			}
		    }
		    break;

		case SORTED_ARCHIVE:
		    strings = p->ranlib_strings;
		    ranlib = bsearch(undefined->merged_symbol->
				     nlist.n_un.n_name, p->ranlibs, p->nranlibs,
				     sizeof(struct ranlib),
				     (int (*)(const void *, const void *))
					ranlib_bsearch);
		    if(ranlib != NULL){
			if(rc_trace_archives == TRUE &&
			   p->rc_trace_archive_printed == FALSE){
			    print("[Logging for Build & Integration] Used "
				  "static archive: %s\n", p->file_name);
			    p->rc_trace_archive_printed = TRUE;
			}
			/*
			 * There is a member that defineds this symbol so
			 * load it.
			 */
			cur_obj = new_object_file();
			cur_obj->file_name = p->file_name;
			cur_obj->ar_hdr = (struct ar_hdr *)(p->file_addr +
							    ranlib->ran_off);
			if(strncmp(cur_obj->ar_hdr->ar_name, AR_EFMT1,
				   sizeof(AR_EFMT1)-1) == 0){
			    ar_name = p->file_addr + ranlib->ran_off +
				      sizeof(struct ar_hdr);
			    ar_name_size =
				    strtoul(cur_obj->ar_hdr->ar_name +
					    sizeof(AR_EFMT1) - 1, NULL, 10);
			    j = ar_name_size;
			}
			else{
			    ar_name = cur_obj->ar_hdr->ar_name;
			    ar_name_size = 0;
			    j = size_ar_name(cur_obj->ar_hdr);
			}
			cur_obj->ar_name = ar_name;
			cur_obj->ar_name_size = j;
			cur_obj->obj_addr = p->file_addr +
					    ranlib->ran_off +
					    sizeof(struct ar_hdr) +
					    ar_name_size;
			cur_obj->obj_size = strtol(cur_obj->ar_hdr->ar_size,
						   NULL, 10) - ar_name_size;
			if(ranlib->ran_off + sizeof(struct ar_hdr) +
			   ar_name_size + cur_obj->obj_size > p->file_size){
			    error("malformed library: %s (member %.*s extends "
				  "past the end of the file, can't load from "
				  "it)",p->file_name, (int)j, ar_name);
			    return;
			}
			if(whyload){
			    print_obj_name(cur_obj);
			    print("loaded to resolve symbol: %s\n", 
				   undefined->merged_symbol->nlist.n_un.n_name);
			}

			merge(FALSE);

			/* make sure this symbol got defined */
			if(errors == 0 && 
			   undefined->merged_symbol->nlist.n_type ==
			   (N_UNDF|N_EXT)
			   && undefined->merged_symbol->nlist.n_value == 0){
			    error("malformed table of contents in library: %s "
				  "(member %.*s did not defined symbol %s)",
				  p->file_name, (int)j, ar_name,
				  undefined->merged_symbol->nlist.n_un.n_name);
			}
			undefined = undefined->next;
			delete_from_undefined_list(undefined->prev);
			found = TRUE;
		    }
		    break;

		case UNSORTED_ARCHIVE:
		    for(i = 0; found == FALSE && i < p->nranlibs; i++){
			if(strcmp(undefined->merged_symbol->nlist.n_un.n_name,
				  p->ranlib_strings +
				  p->ranlibs[i].ran_un.ran_strx) == 0){
			    if(rc_trace_archives == TRUE &&
			       p->rc_trace_archive_printed == FALSE){
				print("[Logging for Build & Integration] Used "
				      "static archive: %s\n", p->file_name);
				p->rc_trace_archive_printed = TRUE;
			    }
			    /*
			     * There is a member that defineds this symbol so
			     * load it.
			     */
			    cur_obj = new_object_file();
			    cur_obj->file_name = p->file_name;
			    cur_obj->ar_hdr = (struct ar_hdr *)(p->file_addr +
							p->ranlibs[i].ran_off);
			    if(strncmp(cur_obj->ar_hdr->ar_name, AR_EFMT1,
				       sizeof(AR_EFMT1)-1) == 0){
				ar_name = p->file_addr + p->ranlibs[i].ran_off +
					  sizeof(struct ar_hdr);
				ar_name_size =
					strtoul(cur_obj->ar_hdr->ar_name +
					        sizeof(AR_EFMT1) - 1, NULL, 10);
				j = ar_name_size;
			    }
			    else{
				ar_name = cur_obj->ar_hdr->ar_name;
				ar_name_size = 0;
				j = size_ar_name(cur_obj->ar_hdr);
			    }
			    cur_obj->ar_name = ar_name;
			    cur_obj->ar_name_size = j;
			    cur_obj->obj_addr = p->file_addr +
						p->ranlibs[i].ran_off +
						sizeof(struct ar_hdr) +
						ar_name_size;
			    cur_obj->obj_size = strtol(cur_obj->ar_hdr->ar_size,
						       NULL, 10) - ar_name_size;
			    if(p->ranlibs[i].ran_off + sizeof(struct ar_hdr) +
			       ar_name_size + cur_obj->obj_size > p->file_size){
				error("malformed library: %s (member %.*s "
				      "extends past the end of the file, can't "
				      "load from it)", p->file_name, (int)j,
				      ar_name);
				return;
			    }
			    if(whyload){
				print_obj_name(cur_obj);
				print("loaded to resolve symbol: %s\n", 
				   undefined->merged_symbol->nlist.n_un.n_name);
			    }

			    merge(FALSE);

			    /* make sure this symbol got defined */
			    if(errors == 0 && 
			       undefined->merged_symbol->nlist.n_type ==
			       (N_UNDF|N_EXT)
			       && undefined->merged_symbol->nlist.n_value == 0){
				error("malformed table of contents in library: "
				   "%s (member %.*s did not defined symbol %s)",
				   p->file_name, (int)j, ar_name,
				   undefined->merged_symbol->nlist.n_un.n_name);
			    }
			    undefined = undefined->next;
			    delete_from_undefined_list(undefined->prev);
			    found = TRUE;
			}
		    }
		    break;
		}
	    }
	    if(found == FALSE)
		undefined = undefined->next;
	}

	/*
	 * Check to see all merged symbols coming from dynamic libraries
	 * came from the first one defining the symbol.  If not issue a warning
	 * suggesting -bind_at_launch be used.
	 */
	if(filetype == MH_EXECUTE && bind_at_load == FALSE){
	    bind_at_load_warning = FALSE;
	    for(m = &merged_symbol_lists; *m; m = &(merged_symbol_list->next)){
		merged_symbol_list = *m;
		for(i = 0; i < merged_symbol_list->used; i++){
		    merged_symbol = &(merged_symbol_list->merged_symbols[i]);
		    if(merged_symbol->defined_in_dylib != TRUE)
			continue;
		    for(p = dynamic_libs; p != NULL; p = p->next){
			if(p->type == DYLIB){
			    strings = p->strings;
			    symbols = p->symbols;
			    toc = bsearch(merged_symbol->nlist.n_un.n_name,
				      p->tocs,p->definition_obj->dysymtab->ntoc,
				      sizeof(struct dylib_table_of_contents),
				      (int (*)(const void *, const void *))
					dylib_bsearch);
			    if(toc != NULL){
				if(merged_symbol->definition_object->obj_addr
				   == p->definition_obj->obj_addr)
				    break;
				if(merged_symbol->definition_object->obj_addr
				   != p->definition_obj->obj_addr){
				    if(bind_at_load_warning == FALSE){
					warning("suggest use of -bind_at_load, "
						"as lazy binding may result in "
						"errors or different symbols "
						"being used");
					bind_at_load_warning = TRUE;
				    }
				    printf("symbol %s used from dynamic "
					"library %s(%s) not from earlier "
					"dynamic library %s(%s)\n",
					merged_symbol->nlist.n_un.n_name,
					merged_symbol->definition_object->
					    file_name,
					(merged_symbol->definition_object->
					    obj_addr + merged_symbol->
					    definition_object->symtab->stroff) +
					merged_symbol->definition_object->
					    dylib_module->module_name,
					p->dylib_name,
					strings + (p->mods +
					    toc->module_index)->module_name);
				}
			    }
			}
		    }
		}
	    }
	}
}

/*
 * prebinding_check_for_dylib_override_symbols() checks to make sure that no
 * symbols are being overridden in a dependent library if prebinding is to
 * be done.  If a symbol is overridden prebinding is disabled and a warning
 * is printed.
 */
__private_extern__
void
prebinding_check_for_dylib_override_symbols(
void)
{
    unsigned long i;
    struct merged_symbol_list **p, *merged_symbol_list;
    struct merged_symbol *merged_symbol;

	if(prebinding == TRUE){
	    for(p = &merged_symbol_lists; *p; p = &(merged_symbol_list->next)){
		merged_symbol_list = *p;
		for(i = 0; i < merged_symbol_list->used; i++){
		    merged_symbol = &(merged_symbol_list->merged_symbols[i]);
		    if(merged_symbol->defined_in_dylib == TRUE)
			continue;
		    if((merged_symbol->nlist.n_type & N_PEXT) == N_PEXT)
			continue;
		    check_dylibs_for_definition(merged_symbol);
		}
	    }
	}
}

/*
 * check_dylibs_for_definition() checks to see if the merged symbol is defined
 * in any of the dependent dynamic shared libraries.  If it is a warning is
 * printed, prebinding is disabled and the symbols are traced.
 */
static
void
check_dylibs_for_definition(
struct merged_symbol *merged_symbol)
{
    struct dynamic_library *p;
    struct dylib_table_of_contents *toc;
    static enum bool printed_override = FALSE;

	for(p = dynamic_libs; p != NULL; p = p->next){
	    if(p->type == DYLIB){
		strings = p->strings;
		symbols = p->symbols;
		toc = bsearch(merged_symbol->nlist.n_un.n_name,
			      p->tocs, p->definition_obj->dysymtab->ntoc,
			      sizeof(struct dylib_table_of_contents),
			      (int (*)(const void *, const void *))
				dylib_bsearch);
		if(toc != NULL){
		    /*
		     * There is a module that defineds this symbol.  If this
		     * symbol is also referenced by the libraries then we
		     * can't prebind.
		     */
		    if(check_dylibs_for_reference(merged_symbol) == TRUE){
			if(printed_override == FALSE){
			    warning("prebinding disabled because of symbols "
			       "overridden in dependent dynamic shared "
			       "libraries:");
			    printed_override = TRUE;
			}
			trace_merged_symbol(merged_symbol);
			printf("%s(%s) definition of %s\n",
			       p->definition_obj->file_name,
			       p->strings +
				    p->mods[toc->module_index].module_name,
			       merged_symbol->nlist.n_un.n_name);
			prebinding = FALSE;
		    }
		}
	    }
	}
}

/*
 * check_dylibs_for_reference() checks the dependent dynamic shared libraries
 * to see if the specified merged symbol is referenced.  If it is TRUE is
 * returned else FALSE is returned.
 */
static
enum bool
check_dylibs_for_reference(
struct merged_symbol *merged_symbol)
{
    struct dynamic_library *p;
    struct dylib_table_of_contents *toc;
    struct nlist *symbol;
    struct dylib_reference *dylib_references;
    unsigned long i, symbol_index;

	for(p = dynamic_libs; p != NULL; p = p->next){
	    if(p->type == DYLIB){
		/*
		 * See if this symbol appears at all (defined or undefined)
		 * in this library.
		 */
		strings = p->strings;
		symbols = p->symbols;
		toc = bsearch(merged_symbol->nlist.n_un.n_name,
			      p->tocs, p->definition_obj->dysymtab->ntoc,
			      sizeof(struct dylib_table_of_contents),
			      (int (*)(const void *, const void *))
				dylib_bsearch);
		if(toc != NULL){
		    symbol_index = toc->symbol_index;
		}
		else{
		    symbol = bsearch(merged_symbol->nlist.n_un.n_name,
			     symbols + p->definition_obj->dysymtab->iundefsym,
			     p->definition_obj->dysymtab->nundefsym,
			     sizeof(struct nlist),
			     (int (*)(const void *,const void *))nlist_bsearch);
		    if(symbol == NULL)
			continue;
		    symbol_index = symbol - symbols;
		}
		/*
		 * The symbol appears in this library.  Now see if it is
		 * referenced by a module in the library.
		 */
		dylib_references = (struct dylib_reference *)
		    (p->definition_obj->obj_addr +
		     p->definition_obj->dysymtab->extrefsymoff);
		for(i = 0; i < p->definition_obj->dysymtab->nextrefsyms; i++){
		    if(dylib_references[i].isym == symbol_index &&
		       (dylib_references[i].flags ==
			    REFERENCE_FLAG_UNDEFINED_NON_LAZY ||
		        dylib_references[i].flags ==
			    REFERENCE_FLAG_UNDEFINED_LAZY))
		    return(TRUE);
		}
	    }
	}
	return(FALSE);
}

/*
 * open_dylib() attempts to open the dynamic library specified by the pointer
 * to the dynamic_library structure.  This is only called for dependent
 * libraries found in the object loaded or in other dynamic libraries.  Since
 * this is only used for undefined checking and prebinding it is not fatal if
 * the library can't be opened.  But if it can't be opened and undefined
 * checking or prebinding is to be done a warning is issued.
 */
static
enum bool
open_dylib(
struct dynamic_library *p)
{
    unsigned long i, file_size;
    char *colon, *file_name, *dylib_name, *file_addr;
    int fd;
    struct stat stat_buf;
    kern_return_t r;
    struct fat_header *fat_header;
    struct mach_header *mh;
    struct load_command *lc;
    struct dylib_command *dl;

	/*
	 * First see if there is a -dylib_file option for this dylib and if so
	 * use that as the file name to open for the dylib.
	 */
	for(i = 0; i < ndylib_files; i++){
	    colon = strchr(dylib_files[i], ':');
	    *colon = '\0';
	    if(strcmp(p->dylib_name, dylib_files[i]) == 0){
		p->dylib_file = dylib_files[i];
		p->file_name = colon + 1;
		break;
	    }
	    *colon = ':';
	}
#if 0
	/*
	 * It has been determined that this warning is distracting.  Even though
	 * the user may want to be alerted that this library is being included
	 * indirectly.  It has been determined this rarely a problem.  So when
	 * it is a problem we will just hope the user is smart enought to figure
	 * it out without any clue.
	 */
	if(p->dylib_file == NULL &&
	   (undefined_flag != UNDEFINED_SUPPRESS ||
	    prebinding == TRUE)){
	    if(p->definition_obj->ar_hdr != NULL)
		warning("using file: %s for reference to dynamic shared library"
			" from: %s(%.*s) because no -dylib_file specified",
			p->dylib_name, p->definition_obj->file_name, 
			(int)p->definition_obj->ar_name_size, 
			p->definition_obj->ar_name);

	    else
		warning("using file: %s for reference to dynamic shared library"
			" from: %s because no -dylib_file specified",
			p->dylib_name, p->definition_obj->file_name);
	}
#endif

	/*
	 * Try to open the dynamic library.  If it can't be opened it is only
	 * a warning if undefined checking or prebinging is to be done.  Once
	 * the file is opened sucessfully then any futher problems are treated
	 * as errors.
	 */
	if(p->dylib_file != NULL)
	    file_name = p->file_name;
	else
	    file_name = p->dylib_name;
	if((fd = open(file_name, O_RDONLY, 0)) == -1){
	    if(undefined_flag != UNDEFINED_SUPPRESS){
		system_warning("can't open dynamic library: %s (checking for "
		    "undefined symbols may be affected)", file_name);
	    }
	    return(FALSE);
	}

	/*
	 * Now that the file_name has been determined and opened get it into
	 * memory by mapping it.
	 */
	if(fstat(fd, &stat_buf) == -1){
	    system_error("can't stat dynamic library file: %s", file_name);
	    close(fd);
	    return(FALSE);
	}
	file_size = stat_buf.st_size;
	/*
	 * For some reason mapping files with zero size fails so it has to
	 * be handled specially.
	 */
	if(file_size == 0){
	    error("file: %s is empty (not a dynamic library)", file_name);
	    close(fd);
	    return(FALSE);
	}
	if((r = map_fd((int)fd, (vm_offset_t)0, (vm_offset_t *)&file_addr,
	    (boolean_t)TRUE, (vm_size_t)file_size)) != KERN_SUCCESS){
	    close(fd);
	    mach_fatal(r, "can't map dynamic library file: %s", file_name);
	}
	close(fd);

	/*
	 * This file must be a dynamic library (it can be fat too).
	 */
	cur_obj = NULL;
	if(sizeof(struct fat_header) > file_size){
	    error("truncated or malformed dynamic library file: %s (file size "
		  "too small to be a dynamic library)", file_name);
	    return(FALSE);
	}
	fat_header = (struct fat_header *)file_addr;
#ifdef __BIG_ENDIAN__
	if(fat_header->magic == FAT_MAGIC)
#endif /* __BIG_ENDIAN__ */
#ifdef __LITTLE_ENDIAN__
	if(fat_header->magic == SWAP_LONG(FAT_MAGIC))
#endif /* __LITTLE_ENDIAN__ */
	{
	    pass1_fat(file_name, file_addr, file_size, FALSE, TRUE);
	}
	else{
	    pass1_object(file_name, file_addr, file_size, FALSE, FALSE, TRUE);
	}
	if(cur_obj == NULL || cur_obj->dylib == FALSE)
	    return(FALSE);

	mh = (struct mach_header *)cur_obj->obj_addr;
	lc = (struct load_command *)((char *)cur_obj->obj_addr +
				     sizeof(struct mach_header));
	for(i = 0; i < mh->ncmds; i++){
	    if(lc->cmd == LC_ID_DYLIB){
		dl = (struct dylib_command *)lc;
		dylib_name = (char *)dl + dl->dylib.name.offset;
#ifdef notdef
		if(strcmp(p->dylib_name, dylib_name) != 0){
		    error("wrong dynamic library: %s (the name in the "
			  "LC_ID_DYLIB command (%s) is not %s)", file_name,
			  dylib_name, p->dylib_name);
		}
#endif
		p->dl = dl;
		p->definition_obj = cur_obj;
		break;
	    }
	    lc = (struct load_command *)((char *)lc + lc->cmdsize);
	}
	return(TRUE);
}

/*
 * add_dynamic_lib() adds a library to the list of specified
 * libraries.  A specified library is a library that is referenced from the
 * object files loaded.  It does not include libraries referenced from dynamic
 * libraries.  This returns a pointer to the dynamic_library struct for the
 * dylib_name specified in the dylib_command (or a new dynamic_library struct
 * for archive types).
 */ 
__private_extern__
struct dynamic_library *
add_dynamic_lib(
enum library_type type,
struct dylib_command *dl,
struct object_file *definition_obj)
{
    struct dynamic_library *p, *q;
    char *dylib_name;

	dylib_name = NULL;
	/*
	 * If this is a dynamic shared library check to see if it is all ready
	 * on the list.
	 */
	if(type == DYLIB){
	    dylib_name = (char *)dl + dl->dylib.name.offset;
	    /*
	     * See if this library is already on the list of specified libraries
	     * and if so merge the two.  If only one is an LC_ID_DYLIB then use
	     * that one.
	     */
	    for(p = dynamic_libs; p != NULL; p = p->next){
		if(p->type == DYLIB &&
		   strcmp(p->dylib_name, dylib_name) == 0){
		    if(p->dl->cmd == LC_ID_DYLIB){
			/*
			 * If the new one is also a LC_ID_DYLIB use the one
			 * with the highest compatiblity number.  Else if the
			 * new one is just an LC_LOAD_DYLIB ignore it and use
			 * the one that is on the list which is a LC_ID_DYLIB.
			 */
			if(dl->cmd == LC_ID_DYLIB){
			   if(dl->dylib.compatibility_version >
			      p->dl->dylib.compatibility_version){
				p->dylib_name = dylib_name;
				p->dl = dl;
				p->definition_obj = definition_obj;
			    }
			}
		    }
		    else{
			if(dl->cmd == LC_ID_DYLIB){
			    p->dylib_name = dylib_name;
			    p->dl = dl;
			    p->definition_obj = definition_obj;
			}
		    }
		    return(p);
		}
	    }
	}
	/*
	 * If this library is not the lists of libraries or is an archive
	 * library.  Create a new dynamic_library struct for it.  Add it to the 
	 * end of the list of specified libraries.  Then return a pointer new
	 * dynamic_library struct.
	 */
	p = allocate(sizeof(struct dynamic_library));
	memset(p, '\0', sizeof(struct dynamic_library));
	if(dynamic_libs == NULL)
	    dynamic_libs = p;
	else{
	    for(q = dynamic_libs; q->next != NULL; q = q->next)
		;
	    q->next = p;
	}

	if(type == DYLIB){
	    p->type = DYLIB;
	    p->dylib_name = dylib_name;
	    p->dl = dl;
	    p->definition_obj = definition_obj;
	    /*
	     * If the environment variable NEXT_ROOT is set then the file_name
	     * for this library prepended with NEXT_ROOT.  Basicly faking out
	     * as a -dylib_file agrument was seen.
	     */
	    if(next_root != NULL && *dylib_name == '/'){
		p->file_name = allocate(strlen(next_root) +
				      strlen(dylib_name) + 1);
		strcpy(p->file_name, next_root);
		strcat(p->file_name, dylib_name);
		p->dylib_file = p->dylib_name;
	    }
	}
	return(p);
}

/*
 * Function for bsearch() for finding a symbol name in a dylib table of
 * contents.
 */
static
int
dylib_bsearch(
const char *symbol_name,
const struct dylib_table_of_contents *toc)
{
	return(strcmp(symbol_name,
		      strings + symbols[toc->symbol_index].n_un.n_strx));
}

/*
 * Function for bsearch() for finding a symbol name in the sorted list of
 * undefined symbols.
 */
static
int
nlist_bsearch(
const char *symbol_name,
const struct nlist *symbol)
{
	return(strcmp(symbol_name, strings + symbol->n_un.n_strx));
}
#endif /* !defined(RLD) */

/*
 * Function for bsearch() for finding a symbol name in a ranlib table of
 * contents.
 */
static
int
ranlib_bsearch(
const char *symbol_name,
const struct ranlib *ran)
{
	return(strcmp(symbol_name, strings + ran->ran_un.ran_strx));
}
#endif /* !defined(SA_RLD) */

/*
 * merge() merges all the global information from the cur_obj into the merged
 * data structures for the output object file to be built from.
 */
__private_extern__
void
merge(
enum bool dylib_only)
{
    unsigned long previous_errors;

	/*
	 * save the previous errors and only return out of here if something
	 * we do in here gets an error.
	 */
	previous_errors = errors;
	errors = 0;

	/* print the object file name if tracing */
	if(trace){
	    print_obj_name(cur_obj);
	    print("\n");
	}

	/* check the header and load commands of the object file */
	check_cur_obj(dylib_only);
	if(errors)
	    goto merge_return;

	/* if this was called to via an open_dylib() we are done */
	if(dylib_only == TRUE)
	    goto merge_return;

	/* if this object has any fixed VM shared library stuff merge it */
	if(cur_obj->fvmlib_stuff){
#ifndef RLD
	    merge_fvmlibs();
	    if(errors)
		goto merge_return;
#else defined(RLD)
	    if(cur_obj != base_obj){
		error_with_cur_obj("can't dynamicly load fixed VM shared "
				   "library");
		goto merge_return;
	    }
#endif defined(RLD)
	}

	/* if this object has any dynamic shared library stuff merge it */
	if(cur_obj->dylib_stuff){
#ifndef RLD
	    merge_dylibs();
	    if(errors)
		goto merge_return;
	    if(cur_obj->dylib)
		goto merge_return;
	    if(cur_obj->dylinker)
		goto merge_return;
#else defined(RLD)
	    if(cur_obj != base_obj){
		error_with_cur_obj("can't used dynamic libraries or dynamic "
		    "linker with rld based interfaces");
		goto merge_return;
	    }
#endif defined(RLD)
	}

	/* merged it's sections */
	merge_sections();
	if(errors)
	    goto merge_return;

	/* merged it's symbols */
	merge_symbols();
	if(errors)
	    goto merge_return;

merge_return:
	errors += previous_errors;
}

/*
 * check_cur_obj() checks to see if the cur_obj object file is really an object
 * file and that all the offset and sizes in the headers are within the memory
 * the object file is mapped in.  This allows the rest of the code in the link
 * editor to use the offsets and sizes in the headers without bounds checking.
 * 
 * Since this is making a pass through the headers a number of things are filled
 * in in the object structrure for this object file including: the symtab field,
 * the dysymtab field, the section_maps and nsection_maps fields (this routine 
 * allocates the section_map structures and fills them in too), the fvmlib_
 * stuff field is set if any SG_FVMLIB segments or LC_LOADFVMLIB commands are
 * seen and the dylib_stuff field is set if the file is a MH_DYLIB type and
 * has a LC_ID_DYLIB command or a LC_LOAD_DYLIB command is seen.
 */
static
void
check_cur_obj(
enum bool dylib_only)
{
    unsigned long i, j, section_type;
    struct mach_header *mh;
    struct load_command l, *lc, *load_commands;
    struct segment_command *sg;
    struct section *s;
    struct symtab_command *st;
    struct dysymtab_command *dyst;
    struct routines_command *rc;
    struct symseg_command *ss;
    struct fvmlib_command *fl;
    struct dylib_command *dl, *dlid;
    struct dylinker_command *dyld, *dyldid;
    char *fvmlib_name, *dylib_name, *dylinker_name;
    cpu_subtype_t new_cpusubtype;
    const char *new_arch, *prev_arch;
    const struct arch_flag *family_arch_flag;
    unsigned long *indirect_symtab;
    struct dylib_table_of_contents *tocs;
    struct dylib_module *mods;
    struct dylib_reference *refs;

    static const struct symtab_command empty_symtab = { 0 };
    static const struct dysymtab_command empty_dysymtab = { 0 };

	/* check to see the mach_header is valid */
	if(sizeof(struct mach_header) > cur_obj->obj_size){
	    error_with_cur_obj("truncated or malformed object (mach header "
			       "extends past the end of the file)");
	    return;
	}
	mh = (struct mach_header *)cur_obj->obj_addr;
	cur_obj->swapped = FALSE;
	if(mh->magic == SWAP_LONG(MH_MAGIC)){
	    cur_obj->swapped = TRUE;
	    swap_mach_header(mh, host_byte_sex);
	}
	if(mh->magic != MH_MAGIC){
	    error_with_cur_obj("bad magic number (not a Mach-O file)");
	    return;
	}
	if(mh->cputype != 0){
	    if(target_byte_sex == UNKNOWN_BYTE_SEX){
		if(cur_obj->swapped == TRUE)
		    target_byte_sex = host_byte_sex == BIG_ENDIAN_BYTE_SEX ?
    			LITTLE_ENDIAN_BYTE_SEX : BIG_ENDIAN_BYTE_SEX;
		else
		    target_byte_sex = host_byte_sex;
	    }
	    /*
	     * If we have previous loaded something or an -arch flag was
	     * specified something so make sure the cputype of this object
	     * matches (the case the architecture has been previous selected).
	     */
	    if(arch_flag.cputype){
		if(arch_flag.cputype != mh->cputype){
		    new_arch = get_arch_name_from_types(mh->cputype,
						        mh->cpusubtype);
		    prev_arch = get_arch_name_from_types(arch_flag.cputype,
						         arch_flag.cpusubtype);
		    if(no_arch_warnings == TRUE)
			return;
		    if(arch_flag.name != NULL){
			if(arch_errors_fatal == TRUE){
			    error_with_cur_obj("cputype (%d, architecture %s) "
				"does not match cputype (%d) for specified "
				"-arch flag: %s", mh->cputype, new_arch,
				arch_flag.cputype, arch_flag.name);
#ifndef RLD
				if(told_ProjectBuilder == FALSE){
				    tell_ProjectBuilder("Architecture "
					"mismatch");
				    told_ProjectBuilder = TRUE;
				}
#endif
			}
			else
			    warning_with_cur_obj("cputype (%d, architecture %s)"
				" does not match cputype (%d) for specified "
				"-arch flag: %s (file not loaded)", mh->cputype,
				 new_arch, arch_flag.cputype, arch_flag.name);
		    }
		    else{
			if(arch_errors_fatal == TRUE){
			    error_with_cur_obj("cputype (%d, architecture %s) "
				"does not match cputype (%d architecture %s) "
				"of objects files previously loaded",
				mh->cputype, new_arch, arch_flag.cputype,
				prev_arch);
#ifndef RLD
				if(told_ProjectBuilder == FALSE){
				    tell_ProjectBuilder("Architecture "
					"mismatch");
				    told_ProjectBuilder = TRUE;
				}
#endif
			}
			else
			    warning_with_cur_obj("cputype (%d, architecture %s)"
				" does not match cputype (%d architecture %s) "
				"of objects files previously loaded (file not "
				"loaded)", mh->cputype, new_arch,
				arch_flag.cputype,prev_arch);
		    }
		    return;
		}
		/* deal with combining this cpusubtype and what is current */
		if(force_cpusubtype_ALL == FALSE){
		    new_cpusubtype = cpusubtype_combine(arch_flag.cputype,
					  arch_flag.cpusubtype, mh->cpusubtype);
		    if(new_cpusubtype == -1){
			new_arch = get_arch_name_from_types(mh->cputype,
							    mh->cpusubtype);
			prev_arch = get_arch_name_from_types(arch_flag.cputype,
							 arch_flag.cpusubtype);
			if(no_arch_warnings == TRUE)
			    return;
			if(arch_flag.name != NULL){
			    if(arch_errors_fatal == TRUE){
				error_with_cur_obj("cpusubtype (%d, "
				    "architecture %s) does not combine with "
				    "cpusubtype (%d) for specified -arch flag: "
				    "%s and -force_cpusubtype_ALL not "
				    "specified", mh->cpusubtype, new_arch,
				    arch_flag.cpusubtype, arch_flag.name);
#ifndef RLD
				    if(told_ProjectBuilder == FALSE){
					tell_ProjectBuilder("Architecture "
					    "mismatch");
					told_ProjectBuilder = TRUE;
				    }
#endif
			    }
			    else
				warning_with_cur_obj("cpusubtype (%d, "
				    "architecture %s) does not combine with "
				    "cpusubtype (%d) for specified -arch flag: "
				    "%s and -force_cpusubtype_ALL not specified"
				    " (file not loaded)", mh->cpusubtype,
				    new_arch, arch_flag.cpusubtype,
				    arch_flag.name);
			}
			else{
			    if(arch_errors_fatal == TRUE){
				error_with_cur_obj("cpusubtype (%d, "
				    "architecture %s) does not combine with "
				    "cpusubtype (%d, architecture %s) of "
				    "objects files previously loaded and "
				    "-force_cpusubtype_ALL not specified",
				    mh->cpusubtype, new_arch,
				    arch_flag.cpusubtype, prev_arch);
#ifndef RLD
				    if(told_ProjectBuilder == FALSE){
					tell_ProjectBuilder("Architecture "
					    "mismatch");
					told_ProjectBuilder = TRUE;
				    }
#endif
			    }
			    else
				warning_with_cur_obj("cpusubtype (%d, "
				    "architecture %s) does not combine with "
				    "cpusubtype (%d, architecture %s) of "
				    "objects files previously loaded and "
				    "-force_cpusubtype_ALL not specified (file "
				    "not loaded)", mh->cpusubtype, new_arch,
				    arch_flag.cpusubtype, prev_arch);
			}
			return;
		    }
		    else{
			/*
			 * No -force_cpusubtype_ALL is specified if an -arch
			 * flag for a specific implementation of an architecture
			 * was specified then the resulting cpusubtype will be
			 * for that specific implementation of that architecture
			 * and all cpusubtypes must combine with the cpusubtype
			 * for the -arch flag to the cpusubtype for the -arch
			 * flag else an error must be flaged.
			 */
			if(specific_arch_flag == TRUE){
			    if(arch_flag.cpusubtype != new_cpusubtype){
			      new_arch = get_arch_name_from_types(mh->cputype,
								mh->cpusubtype);
			      warning_with_cur_obj("cpusubtype (%d, "
				"architecture %s) does not combine with "
				"cpusubtype (%d) for specified -arch flag: %s "
				"and -force_cpusubtype_ALL not specified (file "
				"not loaded)", mh->cpusubtype, new_arch,
				arch_flag.cpusubtype, arch_flag.name);
			    }
			}
			else
			    arch_flag.cpusubtype = new_cpusubtype;
		    }
		}
		else{ /* force_cpusubtype_ALL == TRUE */
		    family_arch_flag =get_arch_family_from_cputype(mh->cputype);
		    if(family_arch_flag != NULL)
			arch_flag.cpusubtype = family_arch_flag->cpusubtype;
		    else{
			warning_with_cur_obj("cputype (%d) unknown (file not "
			    "loaded)", mh->cputype);
			return;
		    }
		}
	    }
	    /*
	     * Nothing has been loaded yet and no -arch flag has been specified
	     * so use this object to set what is to be loaded (the case the
	     * architecture has not been selected).
	     */
	    else{
		family_arch_flag = get_arch_family_from_cputype(mh->cputype);
		if(family_arch_flag == NULL){
		    error_with_cur_obj("cputype (%d) unknown (file not loaded)",
			 mh->cputype);
		    return;
		}
		arch_flag.cputype = mh->cputype;
		if(force_cpusubtype_ALL == TRUE)
		    arch_flag.cpusubtype = family_arch_flag->cpusubtype;
		else
		    arch_flag.cpusubtype = mh->cpusubtype;
		if(target_byte_sex != get_byte_sex_from_flag(family_arch_flag))
		    error_with_cur_obj("wrong bytesex for cputype (%d) for "
			"-arch %s (bad object or this program out of sync with "
			"get_arch_family_from_cputype() and get_byte_sex_"
			"from_flag())", arch_flag.cputype,
			family_arch_flag->name);
	    }
	}
	if(mh->sizeofcmds + sizeof(struct mach_header) > cur_obj->obj_size){
	    error_with_cur_obj("truncated or malformed object (load commands "
			       "extend past the end of the file)");
	    return;
	}
	if((mh->flags & MH_INCRLINK) != 0){
	    error_with_cur_obj("was the output of an incremental link, can't "
			       "be link edited again");
	    return;
	}
	if((mh->flags & MH_DYLDLINK) != 0 && 
	   (mh->filetype != MH_DYLIB && mh->filetype != MH_DYLINKER)){
	    error_with_cur_obj("is input for the dynamic link editor, is not "
			       "relocatable by the static link editor again");
	    return;
	}
	/*
	 * If this is a MH_DYLIB file then a single LC_ID_DYLIB command must be
	 * seen to identify the library.
	 */
	cur_obj->dylib = (enum bool)(mh->filetype == MH_DYLIB);
	dlid = NULL;
	if(cur_obj->dylib == TRUE && dynamic == FALSE){
	    error_with_cur_obj("incompatible, file is a dynamic shared library "
		  "(must specify \"-dynamic\" to be used)");
	    return;
	}
	if(dylib_only == TRUE && cur_obj->dylib == FALSE){
	    error_with_cur_obj("file is not a dynamic shared library");
	    return;
	}
	/*
	 * If this is a MH_DYLINKER file then a single LC_ID_DYLINKER command
	 * must be seen to identify the dynamic linker.
	 */
	cur_obj->dylinker = (enum bool)(mh->filetype == MH_DYLINKER);
	dyldid = NULL;
	if(cur_obj->dylinker == TRUE && dynamic == FALSE){
	    error_with_cur_obj("incompatible, file is a dynamic link editor "
		  "(must specify \"-dynamic\" to be used)");
	    return;
	}

	/* check to see that the load commands are valid */
	load_commands = (struct load_command *)((char *)cur_obj->obj_addr +
			    sizeof(struct mach_header));
	st = NULL;
	dyst = NULL;
	rc = NULL;
	lc = load_commands;
	for(i = 0; i < mh->ncmds; i++){
	    l = *lc;
	    if(cur_obj->swapped)
		swap_load_command(&l, host_byte_sex);
	    if(l.cmdsize % sizeof(long) != 0){
		error_with_cur_obj("load command %lu size not a multiple of "
				   "sizeof(long)", i);
		return;
	    }
	    if(l.cmdsize <= 0){
		error_with_cur_obj("load command %lu size is less than or equal"
				   " to zero", i);
		return;
	    }
	    if((char *)lc + l.cmdsize >
	       (char *)load_commands + mh->sizeofcmds){
		error_with_cur_obj("load command %lu extends past end of all "
				   "load commands", i);
		return;
	    }
	    switch(l.cmd){
	    case LC_SEGMENT:
		sg = (struct segment_command *)lc;
		if(cur_obj->swapped)
		    swap_segment_command(sg, host_byte_sex);
		if(sg->cmdsize != sizeof(struct segment_command) +
				     sg->nsects * sizeof(struct section)){
		    error_with_cur_obj("cmdsize field of load command %lu is "
				       "inconsistant for a segment command "
				       "with the number of sections it has", i);
		    return;
		}
		if(sg->flags == SG_FVMLIB){
		    if(sg->nsects != 0){
			error_with_cur_obj("SG_FVMLIB segment %.16s contains "
					   "sections and shouldn't",
					   sg->segname);
			return;
		    }
		    cur_obj->fvmlib_stuff = TRUE;
		    break;
		}
		check_size_offset(sg->filesize, sg->fileoff, sizeof(long),
				  "filesize", "fileoff", i);
		if(errors)
		    return;
		/*
		 * Segments without sections are an error to see on input except
		 * for the segments created by the link-editor (which are
		 * recreated).
		 */
		if(sg->nsects == 0){
		    if(strcmp(sg->segname, SEG_PAGEZERO) != 0 &&
		       strcmp(sg->segname, SEG_LINKEDIT) != 0){
			error_with_cur_obj("segment %.16s contains no "
					   "sections and can't be link-edited",
					   sg->segname);
			return;
		    }
		}
		else{
		    /*
		     * Doing a reallocate here is not bad beacuse in the
		     * normal case this is an MH_OBJECT file type and has only
		     * one segment.  So this only gets done once per object.
		     */
		    cur_obj->section_maps = reallocate(cur_obj->section_maps,
					(cur_obj->nsection_maps + sg->nsects) *
					sizeof(struct section_map));
		    memset(cur_obj->section_maps + cur_obj->nsection_maps, '\0',
			   sg->nsects * sizeof(struct section_map));
		}
		s = (struct section *)
		    ((char *)sg + sizeof(struct segment_command));
		if(cur_obj->swapped)
		    swap_section(s, sg->nsects, host_byte_sex);
		for(j = 0 ; j < sg->nsects ; j++){
		    cur_obj->section_maps[cur_obj->nsection_maps++].s = s;
		    /* check to see that segment name in the section structure
		       matches the one in the segment command if this is not in
		       an MH_OBJECT filetype */
		    if(mh->filetype != MH_OBJECT &&
		       strcmp(sg->segname, s->segname) != 0){
			error_with_cur_obj("segment name %.16s of section %lu "
				"(%.16s,%.16s) in load command %lu does not "
				"match segment name %.16s", s->segname, j,
				s->segname, s->sectname, i, sg->segname);
			return;
		    }
		    /* check to see that flags (type) of this section is some
		       thing the link-editor understands */
		    section_type = s->flags & SECTION_TYPE;
		    if(section_type != S_REGULAR &&
		       section_type != S_ZEROFILL &&
		       section_type != S_CSTRING_LITERALS &&
		       section_type != S_4BYTE_LITERALS &&
		       section_type != S_8BYTE_LITERALS &&
		       section_type != S_LITERAL_POINTERS &&
		       section_type != S_NON_LAZY_SYMBOL_POINTERS &&
		       section_type != S_LAZY_SYMBOL_POINTERS &&
		       section_type != S_SYMBOL_STUBS &&
#define ENABLE_COALESCED
#ifdef ENABLE_COALESCED
		       section_type != S_COALESCED &&
#endif
		       section_type != S_MOD_INIT_FUNC_POINTERS &&
		       section_type != S_MOD_TERM_FUNC_POINTERS){
			error_with_cur_obj("unknown flags (type) of section %lu"
					   " (%.16s,%.16s) in load command %lu",
					   j, s->segname, s->sectname, i);
			return;
		    }
		    if(dynamic == FALSE){
			if(section_type == S_NON_LAZY_SYMBOL_POINTERS ||
			   section_type == S_LAZY_SYMBOL_POINTERS ||
			   section_type == S_SYMBOL_STUBS ||
			   section_type == S_MOD_INIT_FUNC_POINTERS ||
			   section_type == S_MOD_TERM_FUNC_POINTERS){
			    error_with_cur_obj("incompatible, file contains "
				"unsupported type of section %lu (%.16s,%.16s) "
				"in load command %lu (must specify "
				"\"-dynamic\" to be used)", j, s->segname,
				s->sectname, i);
			    return;
			}
		    }
#ifdef SA_RLD
		    if(section_type == S_NON_LAZY_SYMBOL_POINTERS ||
		       section_type == S_LAZY_SYMBOL_POINTERS ||
		       section_type == S_SYMBOL_STUBS ||
		       section_type == S_MOD_INIT_FUNC_POINTERS ||
		       section_type == S_MOD_TERM_FUNC_POINTERS){
			error_with_cur_obj("unsupported type of section %lu "
			   "(%.16s,%.16s) for sarld in load command %lu", j,
			   s->segname, s->sectname, i);
			return;
		    }
#endif /* SA_RLD */
		    /* check to make sure the alignment is reasonable */
		    if(s->align > MAXSECTALIGN){
			error_with_cur_obj("align (%lu) of section %lu "
			    "(%.16s,%.16s) in load command %lu greater "
			    "than maximum section alignment (%d)", s->align,
			     j, s->segname, s->sectname, i, MAXSECTALIGN);
			return;
		    }
		    /* check the size and offset of the contents if it has any*/
		    if(section_type != S_ZEROFILL){
			check_size_offset_sect(s->size, s->offset, sizeof(char),
			    "size", "offset", i, j, s->segname, s->sectname);
			if(errors)
			    return;
		    }
		    /* check the relocation entries if it can have them */
		    if(section_type == S_ZEROFILL ||
		       section_type == S_CSTRING_LITERALS ||
		       section_type == S_4BYTE_LITERALS ||
		       section_type == S_8BYTE_LITERALS ||
		       section_type == S_NON_LAZY_SYMBOL_POINTERS){
			if(s->nreloc != 0){
			    error_with_cur_obj("section %lu (%.16s,%.16s) in "
				"load command %lu has relocation entries which "
				"it shouldn't for its type (flags)", j,
				 s->segname, s->sectname, i);
			    return;
			}
		    }
		    else{
			if(s->nreloc != 0){
			    if(mh->cputype == 0 && mh->cpusubtype == 0){
				error_with_cur_obj("section %lu (%.16s,%.16s)"
				    "in load command %lu has relocation entries"
				    " but the cputype and cpusubtype for the "
				    "object are not set", j, s->segname,
				    s->sectname, i);
				return;
			    }
			}
			else{
			    check_size_offset_sect(s->nreloc * sizeof(struct
				 relocation_info), s->reloff, sizeof(long),
				 "nreloc * sizeof(struct relocation_info)",
				 "reloff", i, j, s->segname, s->sectname);
			    if(errors)
				return;
			}
		    }
		    if(section_type == S_SYMBOL_STUBS && s->reserved2 == 0){
			error_with_cur_obj("symbol stub section %lu "
			    "(%.16s,%.16s) in load command %lu, sizeof stub in "
			    "reserved2 field is zero", j, s->segname,
			    s->sectname, i);
			return;
		    }
		    s++;
		}
		break;

	    case LC_SYMTAB:
		if(st != NULL){
		    error_with_cur_obj("contains more than one LC_SYMTAB load "
				       "command");
		    return;
		}
		st = (struct symtab_command *)lc;
		if(cur_obj->swapped)
		    swap_symtab_command(st, host_byte_sex);
		if(st->cmdsize != sizeof(struct symtab_command)){
		    error_with_cur_obj("cmdsize of load command %lu incorrect "
				       "for LC_SYMTAB", i);
		    return;
		}
		check_size_offset(st->nsyms * sizeof(struct nlist), st->symoff,
				  sizeof(long), "nsyms * sizeof(struct nlist)",
				  "symoff", i);
		if(errors)
		    return;
		check_size_offset(st->strsize, st->stroff, sizeof(long),
				  "strsize", "stroff", i);
		if(errors)
		    return;
		cur_obj->symtab = st;
		break;

	    case LC_DYSYMTAB:
		if(dyst != NULL){
		    error_with_cur_obj("contains more than one LC_DYSYMTAB "
				       "load command");
		    return;
		}
		dyst = (struct dysymtab_command *)lc;
		if(cur_obj->swapped)
		    swap_dysymtab_command(dyst, host_byte_sex);
		if(dyst->cmdsize != sizeof(struct dysymtab_command)){
		    error_with_cur_obj("cmdsize of load command %lu incorrect "
				       "for LC_DYSYMTAB", i);
		    return;
		}
		check_size_offset(dyst->ntoc *
				  sizeof(struct dylib_table_of_contents),
				  dyst->tocoff, sizeof(long),
				"ntoc * sizeof(struct dylib_table_of_contents)",
				"tocoff", i);
		if(errors)
		    return;
		check_size_offset(dyst->nmodtab * sizeof(struct dylib_module),
				  dyst->modtaboff, sizeof(long),
				  "nmodtab * sizeof(struct dylib_module)",
				  "modtaboff", i);
		if(errors)
		    return;
		check_size_offset(dyst->nextrefsyms *
				  sizeof(struct dylib_reference),
				  dyst->extrefsymoff, sizeof(long),
				 "nextrefsyms * sizeof(struct dylib_reference)",
				 "extrefsymoff", i);
		if(errors)
		    return;
		check_size_offset(dyst->nindirectsyms * sizeof(long),
				  dyst->indirectsymoff, sizeof(long),
				  "nindirectsyms * sizeof(long)",
				  "indirectsymoff", i);
		if(errors)
		    return;
		check_size_offset(dyst->nextrel *sizeof(struct relocation_info),
				  dyst->extreloff, sizeof(long),
				  "nextrel * sizeof(struct relocation_info)",
				  "extreloff", i);
		if(errors)
		    return;
		check_size_offset(dyst->nlocrel *sizeof(struct relocation_info),
				  dyst->locreloff, sizeof(long),
				  "nlocrel * sizeof(struct relocation_info)",
				  "locreloff", i);
		if(errors)
		    return;
		cur_obj->dysymtab = dyst;
		break;

	    case LC_ROUTINES:
		if(rc != NULL){
		    error_with_cur_obj("contains more than one LC_ROUTINES "
				       "load command");
		    return;
		}
		rc = (struct routines_command *)lc;
		if(cur_obj->swapped)
		    swap_routines_command(rc, host_byte_sex);
		if(rc->cmdsize != sizeof(struct routines_command)){
		    error_with_cur_obj("cmdsize of load command %lu incorrect "
				       "for LC_ROUTINES", i);
		    return;
		}
		if(errors)
		    return;
		cur_obj->rc = rc;
		break;

	    case LC_SYMSEG:
		ss = (struct symseg_command *)lc;
		if(cur_obj->swapped)
		    swap_symseg_command(ss, host_byte_sex);
		if(ss->size != 0){
		    warning_with_cur_obj("contains obsolete LC_SYMSEG load "
			"command with non-zero size (produced with a pre-1.0 "
			"version of the compiler, please recompile)");
		}
		break;

	    case LC_IDFVMLIB:
		if(filetype != MH_FVMLIB){
		    error_with_cur_obj("LC_IDFVMLIB load command in object "
			"file (should not be in an input file to the link "
			"editor for output filetypes other than MH_FVMLIB)");
		    return;
		}
		cur_obj->fvmlib_stuff = TRUE;
		fl = (struct fvmlib_command *)lc;
		if(cur_obj->swapped)
		    swap_fvmlib_command(fl, host_byte_sex);
		break;

	    case LC_LOADFVMLIB:
		if(filetype == MH_FVMLIB ||
		   filetype == MH_DYLIB ||
		   filetype == MH_DYLINKER){
		    error_with_cur_obj("LC_LOADFVMLIB load command in object "
			"file (should not be in an input file to the link "
			"editor for the output file type %s)",
			filetype == MH_FVMLIB ? "MH_FVMLIB" :
			(filetype == MH_DYLIB ? "MH_DYLIB" : "MH_DYLINKER"));
		    return;
		}
		fl = (struct fvmlib_command *)lc;
		if(cur_obj->swapped)
		    swap_fvmlib_command(fl, host_byte_sex);
		if(fl->cmdsize < sizeof(struct fvmlib_command)){
		    error_with_cur_obj("cmdsize of load command %lu incorrect "
				       "for LC_LOADFVMLIB", i);
		    return;
		}
		if(fl->fvmlib.name.offset >= fl->cmdsize){
		    error_with_cur_obj("name.offset of load command %lu extends"
				       " past the end of the load command", i); 
		    return;
		}
		fvmlib_name = (char *)fl + fl->fvmlib.name.offset;
		for(j = 0 ; j < fl->cmdsize - fl->fvmlib.name.offset; j++){
		    if(fvmlib_name[j] == '\0')
			break;
		}
		if(j >= fl->cmdsize - fl->fvmlib.name.offset){
		    error_with_cur_obj("library name of load command %lu "
				       "not null terminated", i);
		    return;
		}
		cur_obj->fvmlib_stuff = TRUE;
		break;

	    case LC_ID_DYLIB:
		if(filetype == MH_FVMLIB ||
		   filetype == MH_DYLINKER){
		    error_with_cur_obj("LC_ID_DYLIB load command in object "
			"file (should not be in an input file to the link "
			"editor for the output file type %s)",
			filetype == MH_FVMLIB ? "MH_FVMLIB" : "MH_DYLINKER");
		    return;
		}
		if(mh->filetype != MH_DYLIB){
		    error_with_cur_obj("LC_ID_DYLIB load command in non-"
			"MH_DYLIB filetype");
		    return;
		}
		if(dlid != NULL){
		    error_with_cur_obj("malformed object (more than one "
			"LC_ID_DYLIB load command in MH_DYLIB file)");
		    return;
		}
		dl = (struct dylib_command *)lc;
		dlid = dl;
		if(cur_obj->swapped)
		    swap_dylib_command(dl, host_byte_sex);
		if(dl->cmdsize < sizeof(struct dylib_command)){
		    error_with_cur_obj("cmdsize of load command %lu incorrect "
				       "for LC_ID_DYLIB", i);
		    return;
		}
		if(dl->dylib.name.offset >= dl->cmdsize){
		    error_with_cur_obj("name.offset of load command %lu extends"
				       " past the end of the load command", i); 
		    return;
		}
		dylib_name = (char *)dl + dl->dylib.name.offset;
		for(j = 0 ; j < dl->cmdsize - dl->dylib.name.offset; j++){
		    if(dylib_name[j] == '\0')
			break;
		}
		if(j >= dl->cmdsize - dl->dylib.name.offset){
		    error_with_cur_obj("library name of load command %lu "
				       "not null terminated", i);
		    return;
		}
		cur_obj->dylib_stuff = TRUE;
		break;

	    case LC_LOAD_DYLIB:
		if(filetype == MH_FVMLIB ||
		   filetype == MH_DYLINKER){
		    error_with_cur_obj("LC_LOAD_DYLIB load command in object "
			"file (should not be in an input file to the link "
			"editor for the output file type %s)",
			filetype == MH_FVMLIB ? "MH_FVMLIB" : "MH_DYLINKER");
		    return;
		}
		dl = (struct dylib_command *)lc;
		if(cur_obj->swapped)
		    swap_dylib_command(dl, host_byte_sex);
		if(dl->cmdsize < sizeof(struct dylib_command)){
		    error_with_cur_obj("cmdsize of load command %lu incorrect "
				       "for LC_LOAD_DYLIB", i);
		    return;
		}
		if(dl->dylib.name.offset >= dl->cmdsize){
		    error_with_cur_obj("name.offset of load command %lu extends"
				       " past the end of the load command", i); 
		    return;
		}
		dylib_name = (char *)dl + dl->dylib.name.offset;
		for(j = 0 ; j < dl->cmdsize - dl->dylib.name.offset; j++){
		    if(dylib_name[j] == '\0')
			break;
		}
		if(j >= dl->cmdsize - dl->dylib.name.offset){
		    error_with_cur_obj("library name of load command %lu "
				       "not null terminated", i);
		    return;
		}
		cur_obj->dylib_stuff = TRUE;
		break;

	    case LC_ID_DYLINKER:
		if(filetype == MH_FVMLIB ||
		   filetype == MH_DYLIB ||
		   filetype == MH_DYLINKER){
		    error_with_cur_obj("LC_ID_DYLINKER load command in object "
			"file (should not be in an input file to the link "
			"editor for the output file type %s)",
			filetype == MH_FVMLIB ? "MH_FVMLIB" :
			(filetype == MH_DYLIB ? "MH_DYLIB" : "MH_DYLINKER"));
		    return;
		}
		if(mh->filetype != MH_DYLINKER){
		    error_with_cur_obj("LC_ID_DYLINKER load command in non-"
			"MH_DYLINKER filetype");
		    return;
		}
		if(dyldid != NULL){
		    error_with_cur_obj("malformed object (more than one "
			"LC_ID_DYLINKER load command in MH_DYLINKER file)");
		    return;
		}
		dyld = (struct dylinker_command *)lc;
		dyldid = dyld;
		if(cur_obj->swapped)
		    swap_dylinker_command(dyld, host_byte_sex);
		if(dyld->cmdsize < sizeof(struct dylinker_command)){
		    error_with_cur_obj("cmdsize of load command %lu incorrect "
				       "for LC_ID_DYLINKER", i);
		    return;
		}
		if(dyld->name.offset >= dyld->cmdsize){
		    error_with_cur_obj("name.offset of load command %lu extends"
				       " past the end of the load command", i); 
		    return;
		}
		dylinker_name = (char *)dyld + dyld->name.offset;
		for(j = 0 ; j < dyld->cmdsize - dyld->name.offset; j++){
		    if(dylinker_name[j] == '\0')
			break;
		}
		if(j >= dyld->cmdsize - dyld->name.offset){
		    error_with_cur_obj("dynamic linker name of load command "
			"%lu not null terminated", i);
		    return;
		}
		cur_obj->dylib_stuff = TRUE;
		break;

	    case LC_LOAD_DYLINKER:
		if(filetype == MH_FVMLIB ||
		   filetype == MH_DYLIB ||
		   filetype == MH_DYLINKER){
		    error_with_cur_obj("LC_LOAD_DYLINKER load command in object"
			" file (should not be in an input file to the link "
			"editor for the output file type %s)",
			filetype == MH_FVMLIB ? "MH_FVMLIB" :
			(filetype == MH_DYLIB ? "MH_DYLIB" : "MH_DYLINKER"));
		    return;
		}
		dyld = (struct dylinker_command *)lc;
		if(cur_obj->swapped)
		    swap_dylinker_command(dyld, host_byte_sex);
		if(dyld->cmdsize < sizeof(struct dylinker_command)){
		    error_with_cur_obj("cmdsize of load command %lu incorrect "
				       "for LC_LOAD_DYLINKER", i);
		    return;
		}
		if(dyld->name.offset >= dyld->cmdsize){
		    error_with_cur_obj("name.offset of load command %lu extends"
				       " past the end of the load command", i); 
		    return;
		}
		dylinker_name = (char *)dyld + dyld->name.offset;
		for(j = 0 ; j < dyld->cmdsize - dyld->name.offset; j++){
		    if(dylinker_name[j] == '\0')
			break;
		}
		if(j >= dyld->cmdsize - dyld->name.offset){
		    error_with_cur_obj("dynamic linker name of load command "
			"%lu not null terminated", i);
		    return;
		}
		cur_obj->dylib_stuff = TRUE;
		break;

	    /* all of these are not looked at so they are also not swapped */
	    case LC_UNIXTHREAD:
	    case LC_THREAD:
	    case LC_IDENT:
	    case LC_FVMFILE:
	    case LC_PREPAGE:
		break;

	    default:
		error_with_cur_obj("load command %lu unknown cmd field", i);
		return;
	    }
	    lc = (struct load_command *)((char *)lc + l.cmdsize);
	}
	/*
	 * If this object does not have a symbol table command then set it's
	 * symtab pointer to the empty symtab.  This makes symbol number range
	 * checks in relocation cleaner.
	 */
	if(cur_obj->symtab == NULL){
	    if(cur_obj->dysymtab != NULL)
		error_with_cur_obj("contains LC_DYSYMTAB load command without "
				   "a LC_SYMTAB load command");
	    cur_obj->symtab = (struct symtab_command *)&empty_symtab;
	    cur_obj->dysymtab = (struct dysymtab_command *)&empty_dysymtab;
	}
	else{
	    if(cur_obj->dysymtab == NULL){
		cur_obj->dysymtab = (struct dysymtab_command *)&empty_dysymtab;
		if(cur_obj->rc != NULL)
		    error_with_cur_obj("contains LC_ROUTINES load command "
				       "without a LC_DYSYMTAB load command");
	    }
	    else{
		if(dyst->nlocalsym != 0 &&
		   dyst->ilocalsym > st->nsyms)
		    error_with_cur_obj("ilocalsym in LC_DYSYMTAB load "
			"command extends past the end of the symbol table");
		if(dyst->nlocalsym != 0 &&
		   dyst->ilocalsym + dyst->nlocalsym > st->nsyms)
		    error_with_cur_obj("ilocalsym plus nlocalsym in "
			"LC_DYSYMTAB load command extends past the "
			"end of the symbol table");

		if(dyst->nextdefsym != 0 &&
		   dyst->iextdefsym > st->nsyms)
		    error_with_cur_obj("iextdefsym in LC_DYSYMTAB load "
			"command extends past the end of the symbol table");
		if(dyst->nextdefsym != 0 &&
		   dyst->iextdefsym + dyst->nextdefsym > st->nsyms)
		    error_with_cur_obj("iextdefsym plus nextdefsym in "
			"LC_DYSYMTAB load command extends past the "
			"end of the symbol table");

		if(dyst->nundefsym != 0 &&
		   dyst->iundefsym > st->nsyms)
		    error_with_cur_obj("iundefsym in LC_DYSYMTAB load "
			"command extends past the end of the symbol table");
		if(dyst->nundefsym != 0 &&
		   dyst->iundefsym + dyst->nundefsym > st->nsyms)
		    error_with_cur_obj("iundefsym plus nundefsym in "
			"LC_DYSYMTAB load command extends past the "
			"end of the symbol table");

		if(dyst->ntoc != 0){
		    tocs =(struct dylib_table_of_contents *)(cur_obj->obj_addr +
							     dyst->tocoff);
		    if(cur_obj->swapped)
			swap_dylib_table_of_contents(tocs, dyst->ntoc,
						     host_byte_sex);
		    for(i = 0; i < dyst->ntoc; i++){
			if(tocs[i].symbol_index > st->nsyms)
			    error_with_cur_obj("symbol_index field of table of "
				"contents entry %lu past the end of the symbol "
				"table", i);
			if(tocs[i].module_index > dyst->nmodtab)
			    error_with_cur_obj("module_index field of table of "
				"contents entry %lu past the end of the module "
				"table", i);
		    }
		}

		if(dyst->nmodtab != 0){
		    mods = (struct dylib_module *)(cur_obj->obj_addr +
						   dyst->modtaboff);
		    if(cur_obj->swapped)
			swap_dylib_module(mods, dyst->nmodtab, host_byte_sex);
		    for(i = 0; i < dyst->nmodtab; i++){
			if(mods[i].module_name > st->strsize)
			    error_with_cur_obj("module_name field of module "
				"table entry %lu past the end of the string "
				"table", i);
			if(mods[i].iextdefsym > st->nsyms)
			    error_with_cur_obj("iextdefsym field of module "
				"table entry %lu past the end of the symbol "
				"table", i);
			if(mods[i].iextdefsym +
			   mods[i].nextdefsym > st->nsyms)
			    error_with_cur_obj("iextdefsym field plus "
				"nextdefsym field of module table entry %lu "
				"past the end of the symbol table", i);
			if(mods[i].irefsym > dyst->nextrefsyms)
			    error_with_cur_obj("irefsym field of module table "
				"entry %lu past the end of the reference table",
				i);
			if(mods[i].irefsym +
			   mods[i].nrefsym > dyst->nextrefsyms)
			    error_with_cur_obj("irefsym field plus "
				"nrefsym field of module table entry %lu past "
				"the end of the reference table", i);
			if(mods[i].ilocalsym > st->nsyms)
			    error_with_cur_obj("ilocalsym field of module "
				"table entry %lu past the end of the symbol "
				"table", i);
			if(mods[i].ilocalsym +
			   mods[i].nlocalsym > st->nsyms)
			    error_with_cur_obj("ilocalsym field plus "
				"nlocalsym field of module table entry %lu "
				"past the end of the symbol table", i);
		    }
		}

		if(dyst->nextrefsyms != 0){
		    refs = (struct dylib_reference *)(cur_obj->obj_addr +
						      dyst->extrefsymoff);
		    if(cur_obj->swapped)
			swap_dylib_reference(refs, dyst->nextrefsyms,
					     host_byte_sex);
		    for(i = 0; i < dyst->nextrefsyms; i++){
			if(refs[i].isym > st->nsyms)
			    error_with_cur_obj("isym field of reference table "
				"entry %lu past the end of the symbol table",i);
		    }
		}

		if(dyst->nindirectsyms != 0){
		    indirect_symtab = (unsigned long *)(cur_obj->obj_addr +
					    dyst->indirectsymoff);
		    if(cur_obj->swapped)
			swap_indirect_symbols(indirect_symtab,
			    dyst->nindirectsyms, host_byte_sex);
		    for(i = 0; i < dyst->nindirectsyms; i++){
			if(indirect_symtab[i] != INDIRECT_SYMBOL_LOCAL &&
			   indirect_symtab[i] != INDIRECT_SYMBOL_ABS){
			    if(indirect_symtab[i] > st->nsyms)
				error_with_cur_obj("indirect symbol table entry"
				    " %lu past the end of the symbol table", i);
			}
		    }
		}

		if(rc != NULL && rc->init_module > dyst->nmodtab)
		    error_with_cur_obj("init_module field of LC_ROUTINES load "
			"command past the end of the module table");
	    }
	}
	/*
	 * If this is a MH_DYLIB file then a single LC_ID_DYLIB command must be
	 * seen to identify the library.
	 */
	if(mh->filetype == MH_DYLIB && dlid == NULL){
	    error_with_cur_obj("malformed object (no LC_ID_DYLIB load command "
			       "in MH_DYLIB file)");
	    return;
	}
	/*
	 * If this is a MH_DYLINKER file then a single LC_ID_DYLINKER command
	 * must be seen to identify the library.
	 */
	if(mh->filetype == MH_DYLINKER && dyldid == NULL){
	    error_with_cur_obj("malformed object (no LC_ID_DYLINKER load "
		"command in MH_DYLINKER file)");
	    return;
	}
}

#ifdef RLD
/*
 * merge_base_program() does the pass1 functions for the base program that
 * called rld_load() using it's SEG_LINKEDIT.  It does the same things as the
 * routines pass1(),check_obj() and merge() except that the offset are assumed
 * to be correct in most cases (if they weren't the program would not be
 * executing).  If seg_linkedit is NULL it then the symbol and string table
 * passed in is used instead of seg_linkedit.
 * 
 * A hand crafted object structure is created so to work with the rest of the
 * code.  Like check_obj() a number of things are filled in in the object
 * structrure including: the symtab field, the section_maps and nsection_maps
 * fields (this routine allocates the section_map structures and fills them in
 * too), all fvmlib stuff is ignored since the output is loaded as well as
 * linked.  The file offsets in symtab are faked since this is not a file mapped
 * into memory but rather a running process.  This involves setting where the
 * object starts to the address of the _mh_execute_header and calcating the
 * file offset of the symbol and string table as the differences of the
 * addresses from the _mh_execute_header.  This makes using the rest of the
 * code easy.
 */
__private_extern__
void
merge_base_program(
char *basefile_name,
struct mach_header *basefile_addr,
struct segment_command *seg_linkedit,
struct nlist *symtab,
unsigned long nsyms,
char *strtab,
unsigned long strsize)
{
    unsigned long i, j, section_type;
    struct mach_header *mh;
    struct load_command *lc, *load_commands;
    struct segment_command *sg;
    struct section *s;
    struct symtab_command *st;
    struct dysymtab_command *dyst;

    static const struct symtab_command empty_symtab = { 0 };
    static struct symtab_command base_program_symtab = { 0 };

	/*
	 * Hand craft the object structure as in pass1().  Note the obj_size
	 * feild should never be tested against since this is not a file mapped
	 * into memory but rather a running program.
	 */
	cur_obj = new_object_file();
	cur_obj->file_name = basefile_name;
	cur_obj->obj_addr = (char *)basefile_addr;
	cur_obj->obj_size = 0;
	cur_obj->from_fat_file = FALSE;
	base_obj = cur_obj;
	/* Set the output cpu types from the base program's header */
	mh = basefile_addr;
	arch_flag.cputype = mh->cputype;
	arch_flag.cpusubtype = mh->cpusubtype;

	/*
	 * Go through the load commands and do what would be done in check_obj()
	 * but not checking for offsets.
	 */
	load_commands = (struct load_command *)((char *)cur_obj->obj_addr +
			    sizeof(struct mach_header));
	st = NULL;
	dyst = NULL;
	lc = load_commands;
	for(i = 0; i < mh->ncmds; i++){
	    if(lc->cmdsize % sizeof(long) != 0){
		error_with_cur_obj("load command %lu size not a multiple of "
				   "sizeof(long)", i);
		return;
	    }
	    if(lc->cmdsize <= 0){
		error_with_cur_obj("load command %lu size is less than or equal"
				   " to zero", i);
		return;
	    }
	    if((char *)lc + lc->cmdsize >
	       (char *)load_commands + mh->sizeofcmds){
		error_with_cur_obj("load command %lu extends past end of all "
				   "load commands", i);
		return;
	    }
	    switch(lc->cmd){
	    case LC_SEGMENT:
		sg = (struct segment_command *)lc;
		if(sg->cmdsize != sizeof(struct segment_command) +
				     sg->nsects * sizeof(struct section)){
		    error_with_cur_obj("cmdsize field of load command %lu is "
				       "inconsistant for a segment command "
				       "with the number of sections it has", i);
		    return;
		}
		if(sg->flags == SG_FVMLIB){
		    if(sg->nsects != 0){
			error_with_cur_obj("SG_FVMLIB segment %.16s contains "
					   "sections and shouldn't",
					   sg->segname);
			return;
		    }
		    break;
		}
		/*
		 * Segments without sections are an error to see on input except
		 * for the segments created by the link-editor (which are
		 * recreated).
		 */
		if(sg->nsects == 0){
		    if(strcmp(sg->segname, SEG_PAGEZERO) != 0 &&
		       strcmp(sg->segname, SEG_LINKEDIT) != 0){
			error_with_cur_obj("segment %.16s contains no "
					   "sections and can't be link-edited",
					   sg->segname);
			return;
		    }
		}
		else{
		    /*
		     * Doing a reallocate here is not bad beacuse in the
		     * normal case this is an MH_OBJECT file type and has only
		     * one section.  So this only gets done once per object.
		     */
		    cur_obj->section_maps = reallocate(cur_obj->section_maps,
					(cur_obj->nsection_maps + sg->nsects) *
					sizeof(struct section_map));
		    memset(cur_obj->section_maps + cur_obj->nsection_maps, '\0',
			   sg->nsects * sizeof(struct section_map));
		}
		s = (struct section *)
		    ((char *)sg + sizeof(struct segment_command));
		for(j = 0 ; j < sg->nsects ; j++){
		    cur_obj->section_maps[cur_obj->nsection_maps++].s = s;
		    /* check to see that segment name in the section structure
		       matches the one in the segment command if this is not in
		       an MH_OBJECT filetype */
		    if(mh->filetype != MH_OBJECT &&
		       strcmp(sg->segname, s->segname) != 0){
			error_with_cur_obj("segment name %.16s of section %lu "
				"(%.16s,%.16s) in load command %lu does not "
				"match segment name %.16s", s->segname, j,
				s->segname, s->sectname, i, sg->segname);
			return;
		    }
		    /* check to see that flags (type) of this section is some
		       thing the link-editor understands */
		    section_type = s->flags & SECTION_TYPE;
		    if(section_type != S_REGULAR &&
		       section_type != S_ZEROFILL &&
		       section_type != S_CSTRING_LITERALS &&
		       section_type != S_4BYTE_LITERALS &&
		       section_type != S_8BYTE_LITERALS &&
		       section_type != S_LITERAL_POINTERS &&
		       section_type != S_NON_LAZY_SYMBOL_POINTERS &&
		       section_type != S_LAZY_SYMBOL_POINTERS &&
		       section_type != S_SYMBOL_STUBS &&
#ifdef ENABLE_COALESCED
		       section_type != S_COALESCED &&
#endif
		       section_type != S_MOD_INIT_FUNC_POINTERS &&
		       section_type != S_MOD_TERM_FUNC_POINTERS){
			error_with_cur_obj("unknown flags (type) of section %lu"
					   " (%.16s,%.16s) in load command %lu",
					   j, s->segname, s->sectname, i);
			return;
		    }
		    /* check to make sure the alignment is reasonable */
		    if(s->align > MAXSECTALIGN){
			error_with_cur_obj("align (%lu) of section %lu "
			    "(%.16s,%.16s) in load command %lu greater "
			    "than maximum section alignment (%d)", s->align,
			     j, s->segname, s->sectname, i, MAXSECTALIGN);
			return;
		    }
		    s++;
		}
		break;

	    case LC_SYMTAB:
		if(st != NULL){
		    error_with_cur_obj("contains more than one LC_SYMTAB load "
				       "command");
		    return;
		}
		st = (struct symtab_command *)lc;
		if(st->cmdsize != sizeof(struct symtab_command)){
		    error_with_cur_obj("cmdsize of load command %lu incorrect "
				       "for LC_SYMTAB", i);
		    return;
		}
		break;

	    case LC_DYSYMTAB:
		if(dyst != NULL){
		    error_with_cur_obj("contains more than one LC_DYSYMTAB "
				       "load command");
		    return;
		}
		dyst = (struct dysymtab_command *)lc;
		if(dyst->cmdsize != sizeof(struct dysymtab_command)){
		    error_with_cur_obj("cmdsize of load command %lu incorrect "
				       "for LC_DYSYMTAB", i);
		    return;
		}
		break;

	    case LC_SYMSEG:
	    case LC_IDFVMLIB:
	    case LC_LOADFVMLIB:
	    case LC_ID_DYLIB:
	    case LC_LOAD_DYLIB:
	    case LC_ID_DYLINKER:
	    case LC_LOAD_DYLINKER:
	    case LC_UNIXTHREAD:
	    case LC_THREAD:
	    case LC_IDENT:
	    case LC_FVMFILE:
	    case LC_PREPAGE:
		break;

	    default:
		error_with_cur_obj("load command %lu unknown cmd", i);
		return;
	    }
	    lc = (struct load_command *)((char *)lc + lc->cmdsize);
	}
	/*
	 * Now the slightly tricky part of faking up a symtab command that
	 * appears to have offsets to the symbol and string table when added
	 * to the cur_obj->obj_addr get the correct addresses.
	 */
	if(seg_linkedit == NULL){
	    base_program_symtab.cmd = LC_SYMTAB;
	    base_program_symtab.cmdsize = sizeof(struct symtab_command);
	    base_program_symtab.symoff = (long)symtab - (long)cur_obj->obj_addr;
	    base_program_symtab.nsyms = nsyms;
	    base_program_symtab.stroff = (long)strtab - (long)cur_obj->obj_addr;
	    base_program_symtab.strsize = strsize;
	    cur_obj->symtab = &base_program_symtab;
	}
	else if(st != NULL && st->nsyms != 0){
	    if(st->symoff < seg_linkedit->fileoff ||
	       st->symoff + st->nsyms * sizeof(struct nlist) >
				seg_linkedit->fileoff + seg_linkedit->filesize){
		error_with_cur_obj("symbol table is not contained in "
				   SEG_LINKEDIT " segment");
		return;
	    }
	    if(st->stroff < seg_linkedit->fileoff ||
	       st->stroff + st->strsize >
				seg_linkedit->fileoff + seg_linkedit->filesize){
		error_with_cur_obj("string table is not contained in "
				   SEG_LINKEDIT " segment");
		return;
	    }
	    base_program_symtab = *st;
	    base_program_symtab.symoff = (seg_linkedit->vmaddr + (st->symoff -
					  seg_linkedit->fileoff)) -
					  (long)cur_obj->obj_addr;
	    base_program_symtab.stroff = (seg_linkedit->vmaddr + (st->stroff -
					  seg_linkedit->fileoff)) -
					  (long)cur_obj->obj_addr;
	    cur_obj->symtab = &base_program_symtab;
	}
	/*
	 * If this object does not have a symbol table command then set it's
	 * symtab pointer to the empty symtab.  This makes symbol number range
	 * checks in relocation cleaner.
	 */
	else{
	    cur_obj->symtab = (struct symtab_command *)&empty_symtab;
	}

	/*
	 * Now finish with the base program by doing what would be done in
	 * merge() by merging the base program's sections and symbols.
	 */
	/* merged the base program's sections */
	merge_sections();

	/* merged the base program's symbols */
	merge_symbols();
}
#endif RLD

/*
 * check_size_offset() is used by check_cur_obj() to check a pair of sizes,
 * and offsets from the object file to see it they are aligned correctly and
 * containded with in the file.
 */
static
void
check_size_offset(
unsigned long size,
unsigned long offset,
unsigned long align,
char *size_str,
char *offset_str,
unsigned long cmd)
{
	if(size != 0){
	    if(offset % align != 0){
#ifdef mc68000
		/*
		 * For the mc68000 the alignment is only a warning because it
		 * can deal with all accesses on bad alignment.
		 */
		warning_with_cur_obj("%s in load command %lu not aligned on %lu"
				     " byte boundary", offset_str, cmd, align);
#else !defined(mc68000)
		error_with_cur_obj("%s in load command %lu not aligned on %lu "
				   "byte boundary", offset_str, cmd, align);
#endif mc68000
		return;
	    }
	    if(offset > cur_obj->obj_size){
		error_with_cur_obj("%s in load command %lu extends past the "
				   "end of the file", offset_str, cmd);
		return;
	    }
	    if(offset + size > cur_obj->obj_size){
		error_with_cur_obj("%s plus %s in load command %lu extends past"
				   " the end of the file", offset_str, size_str,
				   cmd);
		return;
	    }
	}
}

/*
 * check_size_offset_sect() is used by check_cur_obj() to check a pair of sizes,
 * and offsets from a section in the object file to see it they are aligned
 * correctly and containded with in the file.
 */
static
void
check_size_offset_sect(
unsigned long size,
unsigned long offset,
unsigned long align,
char *size_str,
char *offset_str,
unsigned long cmd,
unsigned long sect,
char *segname,
char *sectname)
{
	if(size != 0){
	    if(offset % align != 0){
#ifdef mc68000
		/*
		 * For the mc68000 the alignment is only a warning because it
		 * can deal with all accesses on bad alignment.
		 */
		warning_with_cur_obj("%s of section %lu (%.16s,%.16s) in load "
		    "command %lu not aligned on %lu byte boundary", offset_str,
		    sect, segname, sectname, cmd, align);
#else !defined(mc68000)
		error_with_cur_obj("%s of section %lu (%.16s,%.16s) in load "
		    "command %lu not aligned on %lu byte boundary", offset_str,
		    sect, segname, sectname, cmd, align);
#endif mc68000
		return;
	    }
	    if(offset > cur_obj->obj_size){
		error_with_cur_obj("%s of section %lu (%.16s,%.16s) in load "
		    "command %lu extends past the end of the file", offset_str,
		    sect, segname, sectname, cmd);
		return;
	    }
	    if(offset + size > cur_obj->obj_size){
		error_with_cur_obj("%s plus %s of section %lu (%.16s,%.16s) "
		    "in load command %lu extends past the end of the file",
		    offset_str, size_str, sect, segname, sectname, cmd);
		return;
	    }
	}
}

#ifndef RLD
/*
 * collect_base_obj_segments() collects the segments from the base file on a
 * merged segment list used for overlap checking in
 * check_for_overlapping_segments().
 */
static
void
collect_base_obj_segments(void)
{
    unsigned long i;
    struct mach_header *mh;
    struct load_command *lc, *load_commands;
    struct segment_command *sg;

	mh = (struct mach_header *)base_obj->obj_addr;
	load_commands = (struct load_command *)((char *)base_obj->obj_addr +
			    sizeof(struct mach_header));
	lc = load_commands;
	for(i = 0; i < mh->ncmds; i++){
	    switch(lc->cmd){
	    case LC_SEGMENT:
		sg = (struct segment_command *)lc;
		add_base_obj_segment(sg, base_obj->file_name);
		break;

	    default:
		break;
	    }
	    lc = (struct load_command *)((char *)lc + lc->cmdsize);
	}
}

/*
 * add_base_obj_segment() adds the specified segment to the list of
 * base_obj_segments as comming from the specified base filename.
 */
static
void
add_base_obj_segment(
struct segment_command *sg,
char *filename)
{
    struct merged_segment **p, *msg;

	p = &base_obj_segments;
	while(*p){
	    msg = *p;
	    p = &(msg->next);
	}
	*p = allocate(sizeof(struct merged_segment));
	msg = *p;
	memset(msg, '\0', sizeof(struct merged_segment));
	msg->sg = *sg;
	msg->filename = filename;
}

/*
 * Mkstr() creates a string that is the concatenation of a variable number of
 * strings.  It is pass a variable number of pointers to strings and the last
 * pointer is NULL.  It returns the pointer to the string it created.  The
 * storage for the string is malloc()'ed can be free()'ed when nolonger needed.
 */
static
char *
mkstr(
const char *args,
...)
{
    va_list ap;
    char *s, *p;
    unsigned long size;

	size = 0;
	if(args != NULL){
	    size += strlen(args);
	    va_start(ap, args);
	    p = (char *)va_arg(ap, char *);
	    while(p != NULL){
		size += strlen(p);
		p = (char *)va_arg(ap, char *);
	    }
	}
	s = allocate(size + 1);
	*s = '\0';

	if(args != NULL){
	    (void)strcat(s, args);
	    va_start(ap, args);
	    p = (char *)va_arg(ap, char *);
	    while(p != NULL){
		(void)strcat(s, p);
		p = (char *)va_arg(ap, char *);
	    }
	    va_end(ap);
	}
	return(s);
}
#endif !defined(RLD)
