/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*-
 * Copyright (c) 1997 Doug Rabson
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Original code from:
 *	"kldload.c,v 1.5 1998/07/06 06:58:32 charnier Exp"
 */

#ifndef lint
static const char rcsid[] =
	"$Id: kmodload.c,v 1.2 2000/02/24 22:31:35 lindak Exp $";
#endif /* not lint */

#include <stdlib.h>
#include <err.h>
#include <sys/file.h>
#include <nlist.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <paths.h>

#include <mach/mach.h>
#include <mach/mach_error.h>
#include <mach/mach_host.h>
#include <mach-o/rld.h>

#define KMOD_ERROR_USAGE	1
#define KMOD_ERROR_PERMS	2
#define KMOD_ERROR_LOADING	3		
#define KMOD_ERROR_INTERNAL	4

static NXStream *stream;
static mach_port_t kernel_port;
static mach_port_t kernel_priv_port;

static kmod_info_t *module_dependencies = 0;
static vm_address_t kernel_load_address = 0;
static unsigned long kernel_load_size = 0;
static unsigned long kernel_hdr_size = 0;
static unsigned long faked_kernel_load_address = 0;

static char *progname = "program name?";
static int kmodsyms = 0;
static int verbose = 0;
#define v_printf	if (verbose) printf

static void			machwarn(int error, const char *message);
static void			macherr(int error, const char *message);

static unsigned long		linkedit_address(unsigned long size,
						 unsigned long headers_size);
static void 			cleanup_kernel_memory();
static void 			read_module_info(char *module_path, char **object_addr,
						 long *object_size, kmod_info_t **kinfo);
static void 			link_base(const char *base,
					  const char **dependency_paths,
					  const vm_address_t *dependency_addrs,
					  const char *output);
static struct mach_header	*link_module(const char **modules,
					     const char *output);
static struct mach_header	*link_module_from_memory(const char *filename, 
							char *address, 
							long size, 
							const char *output);
static vm_address_t		patch_module(struct mach_header *mach_header);
static kmod_t			load_module(struct mach_header *mach_header,
					    vm_address_t info);
static void 			set_module_dependencies(kmod_t id);
static void			start_module(kmod_t id);

static void
usage(void)
{
	if (kmodsyms) {
		fprintf(stderr, "usage: kmodsyms [-v] [-k kernelfile] [-d dependencyfile] -o symbolfile modulefile\n");
		fprintf(stderr, "       kmodsyms [-v]  -k kernelfile  [-d dependencyfile@address] -o symbolfile modulefile@address\n");
	} else {
		fprintf(stderr, "usage: kmodload [-v] [-k kernelfile] [-d dependencyfile] [-o symbolfile] modulefile\n");
	}
	exit(KMOD_ERROR_USAGE);
}

int
main(int argc, char** argv)
{
	int c, r, i;
	char * kernel = _PATH_UNIX;
	char * gdbfile = 0;
	char * modules[2];
#define MAX_DEPENDANCIES	128
	char * dependencies[MAX_DEPENDANCIES];
	vm_address_t loaded_addresses[MAX_DEPENDANCIES];
	char *address;
	int dependency_count = 0;
	mach_port_t dummy_port_1, dummy_port_2, dummy_port_3, dummy_port_4;
	struct mach_header *rld_header;
	vm_address_t info;
	kmod_t id;
	int link_addrs_set = 0;
	int kernel_set = 0;

	if ((progname = rindex(argv[0], '/')) == NULL)
		progname = argv[0];
	else
		++progname;

	kmodsyms = !strcmp(progname, "kmodsyms");

	// XXX things to add:
	//  -p data string to send as outofband data on start
	//  -P data file to send as outofband data on start

	while ((c = getopt(argc, argv, "d:o:k:v")) != -1)
		switch (c) {
		case 'd':
			dependencies[dependency_count] = optarg;
			if (kmodsyms) {
				if ((address = rindex(optarg, '@'))) {
					*address++ = 0;
					loaded_addresses[dependency_count] = strtoul(address, NULL, 0);
					link_addrs_set++;
				} else {
					loaded_addresses[dependency_count] = 0;
				}
			}
			if (++dependency_count == MAX_DEPENDANCIES) {
				fprintf(stderr, "%s: internal error, dependency count overflow.\n", progname); 
				exit(KMOD_ERROR_INTERNAL);
			}
			break;
		case 'o':
			gdbfile = optarg;
			break;
		case 'k':
			kernel_set++;
			kernel = optarg;
			break;
		case 'v':
			verbose = 1;
			break;
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (argc != 1) usage();

	if (kmodsyms) {
		if (!gdbfile) usage();

		dependencies[dependency_count] = argv[0];
		if ((address = rindex(argv[0], '@'))) {
			*address++ = 0;
			loaded_addresses[dependency_count] = strtoul(address, NULL, 0);
		} else {
			loaded_addresses[dependency_count] = 0;
		}
		if (++dependency_count == MAX_DEPENDANCIES) {
			fprintf(stderr, "%s: internal error, dependency count overflow.\n", progname); 
			exit(KMOD_ERROR_INTERNAL);
		}

		// if any arg uses @address then they all must be and the kernel must be set
		if (link_addrs_set || loaded_addresses[dependency_count-1]) {
			if (!kernel_set) usage();
			for (i=0; i < dependency_count; i++) {
				if (!loaded_addresses[i]) usage();
			}
		}
	} else {
		modules[0] = argv[0];
		modules[1] = NULL;
	}
	dependencies[dependency_count] = 0;
	loaded_addresses[dependency_count] = 0;

	stream = NXOpenFile(fileno(stdout), NX_WRITEONLY);
    
	r = task_for_pid(mach_task_self(), 0, &kernel_port);
	machwarn(r, "unable to get kernel task port");
	if (r) {
		fprintf(stderr, "%s: Are you running as root?\n", progname);
		exit(KMOD_ERROR_PERMS);
	}

	//XXX it would be nice to be able to verify this is the correct kernel
	//XXX once we get sarld working this may be a possibility :-)

	link_base(kernel, dependencies, loaded_addresses, gdbfile);
	if (kmodsyms) return 0;

	r = bootstrap_ports( mach_task_self(),	/* BIG HACK */
			     &kernel_priv_port,
			     &dummy_port_1, &dummy_port_2,
			     &dummy_port_3, &dummy_port_4);
	macherr(r, "unable to get host priv port");

	rld_header = link_module(modules, gdbfile);
	info = patch_module(rld_header);
	id = load_module(rld_header, info);
	set_module_dependencies(id);
	start_module(id);

	return 0;
}

static void
machwarn(int error, const char *message)
{
	if (error == KERN_SUCCESS) return;
	fprintf(stderr, "%s: %s: %s\n", progname, message, mach_error_string(error));
}

static void
macherr(int error, const char *message)
{
	if (error == KERN_SUCCESS) return;
	fprintf(stderr, "%s: %s: %s\n", progname, message, mach_error_string(error));

	cleanup_kernel_memory();

	exit(KMOD_ERROR_INTERNAL);
}

#if !defined(page_round)
#define page_trunc(p) ((int)(p)&~(vm_page_size-1))
#define page_round(p) page_trunc((int)(p)+vm_page_size-1)
#endif


static unsigned long
linkedit_address(unsigned long size, unsigned long headers_size)
{
	int r;

	kernel_load_size = page_round(size);
	kernel_hdr_size = headers_size;

	if (headers_size & (vm_page_size - 1)) {
		//bogus kernel file ?
		fprintf(stderr, "%s: internal error, the text is not page aligned.\n", progname); 
		exit(KMOD_ERROR_INTERNAL);
	}

	if (faked_kernel_load_address) {
		kernel_load_address = faked_kernel_load_address;
		return kernel_load_address;
	}
	if (kmodsyms) {
		fprintf(stderr, "%s: internal error, tried to alloc kernel memory.\n", progname); 
		exit(KMOD_ERROR_INTERNAL);
	}

	r = vm_allocate(kernel_port, &kernel_load_address, 
			kernel_load_size, TRUE);
	macherr(r, "unable to allocate kernel memory");

	v_printf("%s: allocated %ld bytes in kernel space at 0x%x\n",
		 progname, kernel_load_size, kernel_load_address);

	return kernel_load_address;
}

static void
cleanup_kernel_memory()
{
	int r;

	if (faked_kernel_load_address) return;	

	if (kernel_load_address || kernel_load_size) {	
		v_printf("%s: freeing %ld bytes in kernel space at 0x%x\n",
			 progname, kernel_load_size, kernel_load_address);
		r = vm_deallocate(kernel_port, kernel_load_address, kernel_load_size);
		kernel_load_address = kernel_load_size = 0;
		machwarn(r, "unable to cleanup kernel memory");
	}
}

static void
read_module_info(char *module_path, char **object_addr, long *object_size, kmod_info_t **kinfo)
{
	int fd;
	struct stat stat_buf;
	struct mach_header *mh;
	char *p;

	struct nlist nl[] = {
		{ "_kmod_info" },
		{ "" },
	};

	if((fd = open(module_path, O_RDONLY)) == -1){
	    fprintf(stderr, "%s: Can't open: %s\n", progname, module_path);
	    exit(KMOD_ERROR_USAGE);
	}
	if (nlist(module_path, nl)) {
		fprintf(stderr, "%s: %s is not a valid kernel module.\n", progname, module_path);
		exit(KMOD_ERROR_LOADING);
	}
	if(fstat(fd, &stat_buf) == -1){
	    fprintf(stderr, "%s: Can't stat file: %s", progname, module_path);
	    exit(KMOD_ERROR_PERMS);
	}
	*object_size = stat_buf.st_size;
	if(map_fd(fd, 0, (vm_offset_t *)object_addr, TRUE, *object_size) != KERN_SUCCESS){
	    fprintf(stderr, "%s: Can't map file: %s", progname, module_path);
	    exit(KMOD_ERROR_INTERNAL);
	}
	close(fd);

	mh = (struct mach_header *)*object_addr;
	p = *object_addr + sizeof(struct mach_header) + mh->sizeofcmds + nl->n_value;
	*kinfo = (kmod_info_t *)p;
}

static void
link_base(const char *base,
	  const char **dependency_paths,
	  const vm_address_t *dependency_addrs,
	  const char *output)
{
	struct mach_header *rld_header;
	int ok;

	ok = rld_load_basefile(stream, base);
	NXFlush(stream); fflush(stdout);
	if (!ok) {
		fprintf(stderr, "%s: rld_load_basefile(%s) failed.\n", progname, base); 
		exit(KMOD_ERROR_LOADING);
	}

	if (*dependency_paths) {
		char **dependency = dependency_paths;
		const vm_address_t *load_addr = dependency_addrs;

		kmod_info_t *loaded_modules;
		int loaded_count;
		int r = kmod_get_info(kernel_port, (void *)&loaded_modules, &loaded_count);  // never freed
		macherr(r, "kmod_get_info() failed");

		while (*dependency) {
			char *object_addr;
			long object_size;

			vm_address_t info;
			kmod_info_t *file_kinfo;

			read_module_info(*dependency, &object_addr, &object_size, &file_kinfo);

			if (kmodsyms && *load_addr) {
				faked_kernel_load_address = *load_addr;
			} else {
				// match up file version of kmod_info with kernel version

				kmod_info_t *k = loaded_modules;
				int found_it = 0;
				while (k) {
					if (!strcmp(k->name, file_kinfo->name)) {
						if (strcmp(k->version, file_kinfo->version)) {
							fprintf(stderr, "%s: loaded kernel module '%s' version differs.\n",
								progname, *dependency);
							fprintf(stderr, "%s: loaded version '%s', file version '%s'.\n",
								progname, k->version, file_kinfo->version);
							exit(KMOD_ERROR_LOADING);
						}
						found_it++;
						break;
					}
					k = (k->next) ? (k + 1) : 0;
				}
				if (!found_it) {
					fprintf(stderr, "%s: kernel module '%s' is not loaded.\n", 
						progname, *dependency);
					exit(KMOD_ERROR_USAGE);
				}	
			
				k->next = module_dependencies;
				module_dependencies = k;

				faked_kernel_load_address = k->address;
			}

			// if this the last dep and we are running as
			// kmodsyms then write out the "gdb" file
			if (kmodsyms && !*(dependency + 1)) {
				rld_header = link_module_from_memory(*dependency, object_addr,
								     object_size, output);
			} else {
				rld_header = link_module_from_memory(*dependency, object_addr,
								     object_size, 0);
			}
			info = patch_module(rld_header);

			dependency++; load_addr++;
		}
		/* make sure we clear these so clean up does the right thing. */
		faked_kernel_load_address = 0;
		kernel_load_address = 0;
		kernel_load_size = 0;
		kernel_hdr_size = 0;
	}
}

static struct mach_header *
link_module(const char **modules,
	    const char *output)
{
	struct mach_header *rld_header;
	int ok;

	rld_address_func(linkedit_address);

	ok = rld_load(stream, &rld_header, modules, output);
	NXFlush(stream); fflush(stdout);
	if (!ok) {
		fprintf(stderr, "%s: rld_load() failed.\n", progname);
		cleanup_kernel_memory();
		exit(KMOD_ERROR_LOADING);
	}

	return rld_header;
}

static struct mach_header *
link_module_from_memory(const char *filename, char *address, 
			long size, const char *output)
{
	struct mach_header *rld_header;
	int ok;

	rld_address_func(linkedit_address);

	ok = rld_load_from_memory(stream, &rld_header, filename, address, size, output);
	NXFlush(stream); fflush(stdout);
	if (!ok) {
		fprintf(stderr, "%s: rld_load_from_memory() failed.\n", progname);
		cleanup_kernel_memory();
		exit(KMOD_ERROR_LOADING);
	}

	return rld_header;
}

vm_address_t
patch_module(struct mach_header *mach_header)
{
	char * symbol = "_kmod_info";
	kmod_info_t *info;
	unsigned long value;
	int ok;

	ok = rld_lookup(stream, symbol, &value);
	NXFlush(stream); fflush(stdout);
	if (!ok) {
		fprintf(stderr, "%s: rld_lookup(%s) failed.\n", progname, symbol);
		cleanup_kernel_memory();
		exit(KMOD_ERROR_LOADING);
	}

	ok = rld_forget_symbol(stream, symbol);
	NXFlush(stream); fflush(stdout);
	if (!ok) {
		fprintf(stderr, "%s: rld_forget_symbol(%s) failed.\n", progname, symbol);
		cleanup_kernel_memory();
		exit(KMOD_ERROR_INTERNAL);
	}

	info = (kmod_info_t *)(value - (unsigned long)kernel_load_address + (unsigned long)mach_header);
	v_printf("%s: kmod name: %s\n", progname, info->name);
	v_printf("%s: kmod start @ 0x%x\n", progname, (vm_address_t)info->start);
	v_printf("%s: kmod stop  @ 0x%x\n", progname, (vm_address_t)info->stop);
	info->address = kernel_load_address;
	info->size = kernel_load_size;
	info->hdr_size = kernel_hdr_size;

	if (!info->start) {
		fprintf(stderr, "%s: invalid start address?\n", progname);
		cleanup_kernel_memory();
		exit(KMOD_ERROR_LOADING);
	}
	if (!info->stop) {
		fprintf(stderr, "%s: invalid stop address?\n", progname);
		cleanup_kernel_memory();
		exit(KMOD_ERROR_LOADING);
	}

	return (vm_address_t)value;
}

static kmod_t 
load_module(struct mach_header *mach_header, vm_address_t info)
{
	int r;
	kmod_t id;

	// copy linked executable into kernel address space
	r = vm_write(kernel_port, kernel_load_address,
		     (vm_address_t)mach_header, kernel_load_size);
	macherr(r, "unable to write module into kernel memory");

	// let the kernel know about it
	r = kmod_create(kernel_priv_port, info, &id);
	macherr(r, "unable to register module with kernel");

	v_printf("%s: kmod id %d successfully created at 0x%x size %ld.\n", 
		 progname, id, kernel_load_address, kernel_load_size);

	return id;
}

static void
set_module_dependencies(kmod_t id)
{
	int r;
	void * args = 0;
	int argsCount= 0;
	kmod_info_t *module = module_dependencies;

	while (module) {

		r = kmod_control(kernel_priv_port, KMOD_PACK_IDS(id, module->id), KMOD_CNTL_RETAIN, &args, &argsCount);
		machwarn(r, "kmod_control(retain) failed");
		if (r) {
			kernel_load_address = kernel_load_size = kernel_hdr_size = 0;
			r = kmod_destroy(kernel_priv_port, id);
			macherr(r, "kmod_destroy failed");
			exit(KMOD_ERROR_INTERNAL);
		}

		v_printf("%s: kmod id %d reference count was sucessfully incremented.\n", progname, module->id);

		module = module->next;
	}
}

static void
start_module(kmod_t id)
{
	int r;
	void * args = 0;
	int argsCount= 0;

	r = kmod_control(kernel_priv_port, id, KMOD_CNTL_START, &args, &argsCount);
	machwarn(r, "kmod_control(start) failed");
	if (r) {
		kernel_load_address = kernel_load_size = kernel_hdr_size = 0;
		kmod_destroy(kernel_priv_port, id);
		macherr(r, "kmod_destroy failed");
		exit(KMOD_ERROR_INTERNAL);
	}

	v_printf("%s: kmod id %d successfully started.\n", progname, id);
}
