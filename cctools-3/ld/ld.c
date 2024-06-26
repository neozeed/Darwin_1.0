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
 * The NeXT, Inc. Mach-O (Mach object file format) link-editor.  This file
 * contains the main() routine and the global error handling routines and other
 * miscellaneous small global routines.  It also defines the global varaibles
 * that are set or changed by command line arguments.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <signal.h>
#include <errno.h>
#include <libc.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ar.h>
#include <mach/mach.h>
#include <mach/mach_error.h>
#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include "stuff/arch.h"
#include "stuff/version_number.h"

#include "make.h"
#include <mach/mach_init.h>
#include <servers/netname.h>

#include "ld.h"
#ifndef RLD
static char *mkstr(
	const char *args,
	...);
#endif /* !defined(RLD) */
#include "specs.h"
#include "pass1.h"
#include "objects.h"
#include "fvmlibs.h"
#include "sections.h"
#include "symbols.h"
#include "layout.h"
#include "pass2.h"

/* name of this program as executed (argv[0]) */
__private_extern__ char *progname = NULL;
/* indication of an error set in error(), for processing a number of errors
   and then exiting */
__private_extern__ unsigned long errors = 0;
/* the pagesize of the machine this program is running on, getpagesize() value*/
__private_extern__ unsigned long host_pagesize = 0;
/* the byte sex of the machine this program is running on */
__private_extern__ enum byte_sex host_byte_sex = UNKNOWN_BYTE_SEX;

/* name of output file */
__private_extern__ char *outputfile = NULL;
/* type of output file */
__private_extern__ unsigned long filetype = MH_EXECUTE;
#ifndef RLD
static enum bool filetype_specified = FALSE;
/* if the -A flag is specified use to set the object file type */
static enum bool Aflag_specified = FALSE;
#endif !defined(RLD)
/*
 * The architecture of the output file as specified by -arch and the cputype
 * and cpusubtype of the object files being loaded which will be the output
 * cputype and cpusubtype.  specific_arch_flag is true if an -arch flag is
 * specified and the flag for a specific implementation of an architecture.
 */
__private_extern__ struct arch_flag arch_flag = { 0 };
__private_extern__ enum bool specific_arch_flag = FALSE;

/*
 * The -force_cpusubtype_ALL flag.
 */
__private_extern__ enum bool force_cpusubtype_ALL = FALSE;

/* the byte sex of the output file */
__private_extern__ enum byte_sex target_byte_sex = UNKNOWN_BYTE_SEX;
static enum bool arch_multiple = FALSE;	/* print one arch message before error*/

__private_extern__
enum bool trace = FALSE;		/* print stages of link-editing */
__private_extern__
enum bool save_reloc = FALSE;		/* save relocation information */
__private_extern__
enum bool output_for_dyld = FALSE;	/* produce output for use with dyld */
__private_extern__
enum bool bind_at_load = FALSE;		/* mark the output for dyld to be bound
					   when loaded */
__private_extern__
enum bool load_map = FALSE;		/* print a load map */
__private_extern__
enum bool define_comldsyms = TRUE;	/* define common and link-editor defined
					   symbol reguardless of file type */
#ifndef RLD
static enum bool
    dflag_specified = FALSE;		/* the -d flag has been specified */
#endif !defined(RLD)
__private_extern__
enum bool seglinkedit = FALSE;		/* create the link edit segment */
#ifndef RLD
static enum bool
    seglinkedit_specified = FALSE;	/* if either -seglinkedit or */
					/*  -noseglinkedit was specified */
#endif !defined(RLD)
__private_extern__
enum bool whyload = FALSE;		/* print why archive members are 
					   loaded */
#ifndef RLD
static enum bool whatsloaded = FALSE;	/* print which object files are loaded*/
#endif !defined(RLD)
__private_extern__
enum bool flush = TRUE;			/* Use the output_flush routine to flush
					   output file by pages */
__private_extern__
enum bool sectorder_detail = FALSE;	/* print sectorder warnings in detail */
__private_extern__
enum bool nowarnings = FALSE;		/* suppress warnings */
__private_extern__
enum bool no_arch_warnings = FALSE;	/* suppress wrong arch warnings */
__private_extern__
enum bool arch_errors_fatal = FALSE;	/* cause wrong arch errors to be fatal*/
__private_extern__
enum bool archive_ObjC = FALSE;		/* objective-C archive semantics */
__private_extern__
enum bool archive_all = FALSE;		/* always load everything in archives */
__private_extern__
enum bool keep_private_externs = FALSE;	/* don't turn private externs into
					   non-external symbols */
/* TRUE if -dynamic is specified, FALSE if -static is specified */
__private_extern__
enum bool dynamic = TRUE;
#ifndef RLD
static enum bool dynamic_specified = FALSE;
static enum bool static_specified = FALSE;
#endif

/* The level of symbol table stripping */
__private_extern__ enum strip_levels strip_level = STRIP_DUP_INCLS;
/* Strip the base file symbols (the -A argument's symbols) */
__private_extern__ enum bool strip_base_symbols = FALSE;

/* The list of symbols to be traced */
__private_extern__ char **trace_syms = NULL;
__private_extern__ unsigned long ntrace_syms = 0;

/* The number of references of undefined symbols to print */
__private_extern__ unsigned long Yflag = 0;

/* The list of allowed undefined symbols */
__private_extern__ char **undef_syms = NULL;
__private_extern__ unsigned long nundef_syms = 0;

/* The list of -dylib_file arguments */
__private_extern__ char **dylib_files = NULL;
__private_extern__ unsigned long ndylib_files = 0;

/* The checking for undefined symbols */
__private_extern__ enum undefined_check_level undefined_flag = UNDEFINED_ERROR;
#ifndef RLD
static enum bool undefined_flag_specified = FALSE;
#endif

/* The checking for read only relocs */
__private_extern__ enum read_only_reloc_check_level
    read_only_reloc_flag = READ_ONLY_RELOC_ERROR;

/* The prebinding optimization */
#ifndef RLD
static enum bool prebinding_flag_specified = FALSE;
#endif
__private_extern__ enum bool prebinding = FALSE;
#ifndef RLD
static enum bool read_only_reloc_flag_specified = FALSE;
#endif

/* True if -m is specified to allow multiply symbols, as a warning */
__private_extern__ enum bool allow_multiply_defined_symbols = FALSE;

/* The segment alignment and pagezero_size, note the segalign is reset in
 * layout() by get_segalign_from_flag() based on the target architecture.
 */
__private_extern__ unsigned long segalign = 0x2000;
#ifndef RLD
__private_extern__ enum bool segalign_specified = FALSE;
#endif !defined(RLD)
__private_extern__ unsigned long pagezero_size = 0;

/* The default section alignment */
__private_extern__ unsigned long defaultsectalign = DEFAULTSECTALIGN;

/* The first segment address */
__private_extern__ unsigned long seg1addr = 0;
__private_extern__ enum bool seg1addr_specified = FALSE;

/* read-only and read-write segment addresses */
__private_extern__ unsigned long segs_read_only_addr = 0;
__private_extern__ enum bool segs_read_only_addr_specified = FALSE;
__private_extern__ unsigned long segs_read_write_addr = 0;
__private_extern__ enum bool segs_read_write_addr_specified = FALSE;

/* The stack address and size */
__private_extern__ unsigned long stack_addr = 0;
__private_extern__ enum bool stack_addr_specified = FALSE;
__private_extern__ unsigned long stack_size = 0;
__private_extern__ enum bool stack_size_specified = FALSE;

#ifndef RLD
/* A -segaddr option was specified */
static enum bool segaddr_specified = FALSE;
#endif !defined(RLD)

/*
 * The header pad, the default is set to the size of a section strcuture so
 * that if /bin/objcunique is run on the result and up to two sections can be
 * added.
 */
__private_extern__ unsigned long headerpad = sizeof(struct section) * 2;
#ifndef RLD
static enum bool headerpad_specified = FALSE;
#endif !defined(RLD)

/* The name of the specified entry point */
__private_extern__ char *entry_point_name = NULL;

/* The name of the specified library initialization routine */
__private_extern__ char *init_name = NULL;

/* The dylib information */
__private_extern__ char *dylib_install_name = NULL;
__private_extern__ unsigned long dylib_current_version = 0;
__private_extern__ unsigned long dylib_compatibility_version = 0;

/* The dylinker information */
__private_extern__ char *dylinker_install_name = NULL;

/* The value of the environment variable NEXT_ROOT */
__private_extern__ char *next_root = NULL;

/* TRUE if the environment variable RC_TRACE_ARCHIVES is set */
__private_extern__ enum bool rc_trace_archives = FALSE;

/* TRUE if the environment variable RC_TRACE_DYLIBS is set */
__private_extern__ enum bool rc_trace_dylibs = FALSE;

#ifdef DEBUG
__private_extern__ unsigned long debug = 0;	/* link-editor debugging */
#endif DEBUG

#ifdef RLD
/* the cleanup routine for fatal errors to remove the output file */
__private_extern__ void cleanup(void);
#else !defined(RLD)
static void cleanup(void);
static void ld_exit(int exit_value);

/*
 * This is for the ProjectBuilder (formally MakeApp) interface.
 */
static int talking_to_ProjectBuilder = 0;
static mach_port_t ProjectBuilder_port;
static void check_for_ProjectBuilder(void);

/* The signal hander routine for SIGINT and SIGTERM */
static void handler(int sig);

/* Static routines to help parse arguments */
static enum bool ispoweroftwo(unsigned long x);
static vm_prot_t getprot(char *prot, char **endp);
static enum bool check_max_init_prot(vm_prot_t maxprot, vm_prot_t initprot);

/*
 * main() parses the command line arguments and drives the link-edit process.
 */
int
main(
int argc,
char *argv[],
char *envp[])
{
    unsigned long i, j, symbols_created, objects_specified, sections_created;
    int fd;
    char *p, *symbol_name, *indr_symbol_name, *endp, *file_name;
    char *filelist, *dirname, *addr;
    struct segment_spec *seg_spec;
    struct section_spec *sect_spec;
    unsigned long align, tmp;
    struct stat stat_buf;
    kern_return_t r;
    const struct arch_flag *family_arch_flag;
    enum undefined_check_level new_undefined_flag;
    enum read_only_reloc_check_level new_read_only_reloc_flag;
#ifdef __MWERKS__
    char **dummy;
        dummy = envp;
#endif

	progname = argv[0];
#ifndef BINARY_COMPARE
	host_pagesize = 0x2000;
#else
	host_pagesize = getpagesize();
#endif
	host_byte_sex = get_host_byte_sex();

	if(argc == 1)
	    fatal("Usage: %s [options] file [...]", progname);

	/*
	 * If interrupt and termination signal are not being ignored catch
	 * them so things can be cleaned up.
	 */
	if(signal(SIGINT, SIG_IGN) != SIG_IGN)
	    signal(SIGINT, handler);
	if(signal(SIGTERM, SIG_IGN) != SIG_IGN)
	    signal(SIGTERM, handler);

	/* If ProjectBuilder is around set up for it */
	check_for_ProjectBuilder();

	/*
	 * Parse the command line options in this pass and skip the object files
	 * and symbol creation flags in this pass.  This will make sure optionsd
	 * like -Ldir are not position dependent relative to -lx options (the
	 * same for -ysymbol relative to object files, etc).
	 */
	for(i = 1 ; i < argc ; i++){
	    if(*argv[i] != '-'){
		/* object file argv[i] processed in the next pass of
		   parsing arguments */
		continue;
	    }
	    else{
		p = &(argv[i][1]);
		switch(*p){
		case 'l':
		    /* path searched abbrevated file name, processed in the
		       next pass of parsing arguments */
		    break;

		/* Flags effecting search path of -lx arguments */
		case 'L':
		    if(p[1] == '\0')
			fatal("-L: directory name missing");
		    /* add a pathname to the list of search paths */
		    search_dirs = reallocate(search_dirs,
					   (nsearch_dirs + 1) * sizeof(char *));
		    search_dirs[nsearch_dirs++] = &(p[1]);
		    if(stat(&(p[1]), &stat_buf) == -1)
			warning("-L: directory name (%s) does not exist",
				&(p[1]));
		    break;
		case 'Z':
		    if(p[1] != '\0')
			goto unknown_flag;
		    /* do not use the standard search path */
		    standard_dirs[0] = NULL;
		    standard_framework_dirs[0] = NULL;
		    break;

		/* File format flags */
		case 'M':
		    if(strcmp(p, "Mach") == 0){
			if(filetype_specified == TRUE && filetype != MH_EXECUTE)
			    fatal("more than one output filetype specified");
			filetype_specified = TRUE;
			filetype = MH_EXECUTE;
		    }
		    else if(strcmp(p, "M") == 0){
			/* produce load map */
			load_map = TRUE;
		    }
		    else
			goto unknown_flag;
		    break;
		case 'p':
		    if(strcmp(p, "preload") == 0 || p[1] == '\0'){
			if(filetype_specified == TRUE && filetype != MH_PRELOAD)
			    fatal("more than one output filetype specified");
			filetype_specified = TRUE;
			filetype = MH_PRELOAD;
		    }
		    else if(strcmp(p, "pagezero_size") == 0){
			if(i + 1 >= argc)
			    fatal("-pagezero_size: argument missing");
			if(pagezero_size != 0)
			    fatal("-pagezero_size: multiply specified");
			pagezero_size = strtoul(argv[i+1], &endp, 16);
			if(*endp != '\0')
			    fatal("size for -pagezero_size %s not a proper "
				  "hexadecimal number", argv[i+1]);
			if(pagezero_size == 0)
			    fatal("size for -pagezero_size %s must not be zero",
				  argv[i+1]);
			i += 1;
		    }
		    else if(strcmp(p, "prebind") == 0){
			if(prebinding_flag_specified == TRUE &&
			   prebinding == FALSE)
			    fatal("both -prebind and -noprebind can't "
				  "be specified");
			prebinding_flag_specified = TRUE;
			prebinding = TRUE;
		    }
		    else
			goto unknown_flag;
		    break;
		case 'f':
		    if(p[1] == '\0')
			fatal("use of old flag -f (old version of mkshlib(1) "
			      "will not work with this version of ld(1))");
		    else if(strcmp(p, "fvmlib") == 0){
			if(filetype_specified == TRUE && filetype != MH_FVMLIB)
			    fatal("more than one output filetype specified");
			filetype_specified = TRUE;
			filetype = MH_FVMLIB;
		    }
		    else if(strcmp(p, "force_cpusubtype_ALL") == 0){
			force_cpusubtype_ALL = TRUE;
		    }
		    else if(strcmp(p, "framework") == 0){
			if(i + 1 >= argc)
			    fatal("-framework: argument missing");
			/* path searched abbrevated framework name, processed
			   in the next pass of parsing arguments */
			i += 1;
		    }
		    else if(strcmp(p, "filelist") == 0){
			if(i + 1 >= argc)
			    fatal("-filelist: argument missing");
			/* filelist of object names, processed
			   in the next pass of parsing arguments */
			i += 1;
		    }
		    else
			goto unknown_flag;
		    break;

		case 'F':
		    if(p[1] == '\0')
			fatal("-F: directory name missing");
		    /* add a pathname to the list of framework search paths */
		    framework_dirs = reallocate(framework_dirs,
				       (nframework_dirs + 1) * sizeof(char *));
		    framework_dirs[nframework_dirs++] = &(p[1]);
		    if(stat(&(p[1]), &stat_buf) == -1)
			warning("-F: directory name (%s) does not exist",
				&(p[1]));
		    break;

		case 'r':
		    if(strcmp(p, "read_only_relocs") == 0){
			if(++i >= argc)
			    fatal("-read_only_relocs: argument missing");
			if(strcmp(argv[i], "error") == 0)
			    new_read_only_reloc_flag = READ_ONLY_RELOC_ERROR;
			else if(strcmp(argv[i], "warning") == 0)
			    new_read_only_reloc_flag = READ_ONLY_RELOC_WARNING;
			else if(strcmp(argv[i], "suppress") == 0)
			    new_read_only_reloc_flag = READ_ONLY_RELOC_SUPPRESS;
			else{
			    fatal("-read_only_relocs: unknown argument: %s",
				  argv[i]);
			    new_read_only_reloc_flag = READ_ONLY_RELOC_ERROR;
			}
			if(read_only_reloc_flag_specified == TRUE &&
			   new_read_only_reloc_flag != read_only_reloc_flag)
			    fatal("more than one value specified for "
				  "-read_only_relocs");
			read_only_reloc_flag_specified = TRUE;
			read_only_reloc_flag = new_read_only_reloc_flag;
			break;
		    }
		    if(p[1] != '\0')
			goto unknown_flag;
		    /* save relocation information, and produce a relocatable
		       object */
		    save_reloc = TRUE;
		    if(filetype_specified == FALSE)
			filetype = MH_OBJECT;
		    if(dflag_specified == FALSE)
			define_comldsyms = FALSE;
		    break;
		case 'A':
		    if(p[1] != '\0')
			goto unknown_flag;
		    if(++i >= argc)
			fatal("-A: argument missing");
		    /* object file argv[i] processed in the next pass of
		       parsing arguments */
		    Aflag_specified = TRUE;
		    break;
		case 'd':
		    if(strcmp(p, "d") == 0){
			/* define common symbols and loader defined symbols
			   reguardless of file format */
			dflag_specified = TRUE;
			define_comldsyms = TRUE;
		    }
		    else if(strcmp(p, "dynamic") == 0){
			if(static_specified)
			    fatal("only one of -dynamic or -static can be "
				  "specified");
			
			dynamic = TRUE;
			dynamic_specified = TRUE;
		    }
		    else if(strcmp(p, "dylib") == 0){
			if(filetype_specified == TRUE && filetype != MH_DYLIB)
			    fatal("more than one output filetype specified");
			filetype_specified = TRUE;
			filetype = MH_DYLIB;
			output_for_dyld = TRUE;
		    }
		    else if(strcmp(p, "dylib_install_name") == 0){
			if(i + 1 >= argc)
			    fatal("-dylib_install_name: argument missing");
			if(dylib_install_name != NULL)
			    fatal("-dylib_install_name multiply specified");
			dylib_install_name = argv[i + 1];
			i += 1;
		    }
		    else if(strcmp(p, "dylib_current_version") == 0){
			if(i + 1 >= argc)
			    fatal("-dylib_current_version: argument missing");
			if(dylib_current_version != 0)
			    fatal("-dylib_current_version multiply specified");
			if(get_version_number("-dylib_current_version",
			    argv[i+1], &dylib_current_version) == FALSE)
			    cleanup();
			if(dylib_current_version == 0)
			    fatal("-dylib_current_version must be greater than "
				  "zero");
			i += 1;
		    }
		    else if(strcmp(p, "dylib_compatibility_version") == 0){
			if(i + 1 >= argc)
			    fatal("-dylib_compatibility_version: argument "
				  "missing");
			if(dylib_compatibility_version != 0)
			    fatal("-dylib_compatibility_version multiply "
				  "specified");
			if(get_version_number("-dylib_compatibility_version",
			    argv[i+1], &dylib_compatibility_version) == FALSE)
			    cleanup();
			if(dylib_compatibility_version == 0)
			    fatal("-dylib_compatibility_version must be "
				  "greater than zero");
			i += 1;
		    }
		    else if(strcmp(p, "dylib_file") == 0){
			if(++i >= argc)
			    fatal("-dylib_file: argument missing");
			file_name = strchr(argv[i], ':');
			if(file_name == NULL ||
			   file_name[1] == '\0' || argv[i][0] == ':')
			    fatal("-dylib_file argument: %s must have a ':' "
				  "between it's file names", argv[i]);
			dylib_files = reallocate(dylib_files,
					(ndylib_files + 1) * sizeof(char *));
			dylib_files[ndylib_files++] = argv[i];
		    }
		    else if(strcmp(p, "dylinker") == 0){
			if(filetype_specified == TRUE &&
			   filetype != MH_DYLINKER)
			    fatal("more than one output filetype specified");
			filetype_specified = TRUE;
			filetype = MH_DYLINKER;
			output_for_dyld = TRUE;
		    }
		    else if(strcmp(p, "dylinker_install_name") == 0){
			if(i + 1 >= argc)
			    fatal("-dylinker_install_name: argument missing");
			if(dylinker_install_name != NULL)
			    fatal("-dylinker_install_name multiply specified");
			dylinker_install_name = argv[i + 1];
			i += 1;
		    }
#ifdef DEBUG
		    else if(strcmp(p, "debug") == 0){
			if(++i >= argc)
			    fatal("-debug: argument missing");
			debug |= 1 << strtoul(argv[i], &endp, 10);
			if(*endp != '\0' || strtoul(argv[i], &endp, 10) > 32)
			    fatal("argument for -debug %s not a proper "
				  "decimal number less than 32", argv[i]);
		    }
#endif DEBUG
		    else
			goto unknown_flag;
		    break;

		case 'n':
		    if(strcmp(p, "noflush") == 0){
			flush = FALSE;
		    }
		    else if(strcmp(p, "no_arch_warnings") == 0){
			no_arch_warnings = TRUE;
		    }
		    else if(strcmp(p, "noseglinkedit") == 0){
			if(seglinkedit_specified && seglinkedit == TRUE)
			    fatal("both -seglinkedit and -noseglinkedit can't "
				  "be specified");
			seglinkedit = FALSE;
			seglinkedit_specified = TRUE;
		    }
		    else if(strcmp(p, "noprebind") == 0){
			if(prebinding_flag_specified == TRUE &&
			   prebinding == TRUE)
			    fatal("both -prebind and -noprebind can't "
				  "be specified");
			prebinding_flag_specified = TRUE;
			prebinding = FALSE;
		    }
		    else
			goto unknown_flag;
		    break;

		case 'b':
		    if(strcmp(p, "bundle") == 0){
			if(filetype_specified == TRUE && filetype != MH_BUNDLE)
			    fatal("more than one output filetype specified");
			filetype_specified = TRUE;
			filetype = MH_BUNDLE;
			output_for_dyld = TRUE;
		    }
		    else if(strcmp(p, "bind_at_load") == 0){
			bind_at_load = TRUE;
		    }
		    /* Strip the base file symbols (the -A argument's symbols)*/
		    else if(p[1] == '\0')
			strip_base_symbols = TRUE;
		    else
			goto unknown_flag;
		    break;

		/*
		 * Stripping level flags, in increasing level of stripping.  The
		 * level of stripping is set to the maximum level specified.
		 */
		case 'X':
		    if(p[1] != '\0')
			goto unknown_flag;
		    if(strip_level < STRIP_L_SYMBOLS)
			strip_level = STRIP_L_SYMBOLS;
		    break;
		case 'S':
		    if(strcmp(p, "Sn") == 0){
			strip_level = STRIP_NONE;
		    }
		    else if(strcmp(p, "Si") == 0){
			if(strip_level < STRIP_DUP_INCLS)
			    strip_level = STRIP_DUP_INCLS;
		    }
		    else if(p[1] == '\0'){
			if(strip_level < STRIP_DEBUG)
			    strip_level = STRIP_DEBUG;
		    }
		    else{
			goto unknown_flag;
		    }
		    break;
		case 'x':
		    if(p[1] != '\0')
			goto unknown_flag;
		    if(strip_level < STRIP_NONGLOBALS)
			strip_level = STRIP_NONGLOBALS;
		    break;
		case 's':
		    if(strcmp(p, "s") == 0){
			strip_level = STRIP_ALL;
		    }
		    else if(strcmp(p, "static") == 0){
			if(dynamic_specified)
			    fatal("only one of -static or -dynamic can be "
				  "specified");
			dynamic = FALSE;
			static_specified = TRUE;
		    }
		    /*
		     * Flags for specifing information about sections.
		     */
		    /* create a section from the contents of a file
		       -sectcreate <segname> <sectname> <filename> */
		    else if(strcmp(p, "sectcreate") == 0 ||
		    	    strcmp(p, "segcreate") == 0){ /* the old name */
			if(i + 3 >= argc)
			    fatal("%s: arguments missing", argv[i]);
			seg_spec = create_segment_spec(argv[i+1]);
			sect_spec = create_section_spec(seg_spec, argv[i+2]);
			if(sect_spec->contents_filename != NULL)
			     fatal("section (%s,%s) multiply specified with a "
				   "%s option", argv[i+1], argv[i+2], argv[i]);
			if((fd = open(argv[i+3], O_RDONLY, 0)) == -1)
			    system_fatal("Can't open: %s for %s %s %s",
				    argv[i+3], argv[i], argv[i+1], argv[i+2]);
			if(fstat(fd, &stat_buf) == -1)
			    system_fatal("Can't stat file: %s for %s %s %s",
				    argv[i+3], argv[i], argv[i+1], argv[i+2]);
			/*
			 * For some reason mapping files with zero size fails
			 * so it has to be handled specially.
			 */
			if(stat_buf.st_size != 0){
			    if((r = map_fd((int)fd, (vm_offset_t)0,
				(vm_offset_t *)&(sect_spec->file_addr),
				(boolean_t)TRUE, (vm_size_t)stat_buf.st_size)
				) != KERN_SUCCESS)
				mach_fatal(r, "can't map file: %s for %s %s %s",
				    argv[i+3], argv[i], argv[i+1], argv[i+2]);
			}
			else{
			    sect_spec->file_addr = NULL;
			}
			close(fd);
			sect_spec->file_size = stat_buf.st_size;
			sect_spec->contents_filename = argv[i+3];
			i += 3;
		    }
		    /* specify the alignment of a section as a hexadecimal
		       power of 2
		       -sectalign <segname> <sectname> <number> */
		    else if(strcmp(p, "sectalign") == 0){
			if(i + 3 >= argc)
			    fatal("-sectalign arguments missing");
			seg_spec = create_segment_spec(argv[i+1]);
			sect_spec = create_section_spec(seg_spec, argv[i+2]);
			if(sect_spec->align_specified)
			     fatal("alignment for section (%s,%s) multiply "
				   "specified", argv[i+1], argv[i+2]);
			sect_spec->align_specified = TRUE;
			align = strtoul(argv[i+3], &endp, 16);
			if(*endp != '\0')
			    fatal("argument for -sectalign %s %s: %s not a "
				  "proper hexadecimal number", argv[i+1],
				  argv[i+2], argv[i+3]);
			if(!ispoweroftwo(align))
			    fatal("argument to -sectalign %s %s: %lx (hex) must"
				  " be a power of two", argv[i+1], argv[i+2],
				  align);
			if(align != 0)
			    for(tmp = align; (tmp & 1) == 0; tmp >>= 1)
				sect_spec->align++;
			if(sect_spec->align > MAXSECTALIGN)
			    fatal("argument to -sectalign %s %s: %lx (hex) must"
				  " equal to or less than %x (hex)", argv[i+1],
				  argv[i+2], align,
				  (unsigned int)(1 << MAXSECTALIGN));
			i += 3;
		    }
		    /* specify that section object symbols are to be created
		       for the specified section
		       -sectobjectsymbols <segname> <sectname> */
		    else if(strcmp(p, "sectobjectsymbols") == 0){
			if(i + 2 >= argc)
			    fatal("-sectobjectsymbols arguments missing");
			if(sect_object_symbols.specified &&
			   (strcmp(sect_object_symbols.segname,
				   argv[i+1]) != 0 ||
			    strcmp(sect_object_symbols.sectname,
				   argv[i+2]) != 0) )
			     fatal("-sectobjectsymbols multiply specified (it "
				   "can only be specified for one section)");
			sect_object_symbols.specified = TRUE;
			sect_object_symbols.segname = argv[i+1];
			sect_object_symbols.sectname = argv[i+2];
			i += 2;
		    }
		    /* layout a section in the order the symbols appear in file
		       -sectorder <segname> <sectname> <filename> */
		    else if(strcmp(p, "sectorder") == 0){
			if(i + 3 >= argc)
			    fatal("%s: arguments missing", argv[i]);
			seg_spec = create_segment_spec(argv[i+1]);
			sect_spec = create_section_spec(seg_spec, argv[i+2]);
			if(sect_spec->order_filename != NULL)
			     fatal("section (%s,%s) multiply specified with a "
				   "%s option", argv[i+1], argv[i+2], argv[i]);
			if((fd = open(argv[i+3], O_RDONLY, 0)) == -1)
			    system_fatal("Can't open: %s for %s %s %s",
				    argv[i+3], argv[i], argv[i+1], argv[i+2]);
			if(fstat(fd, &stat_buf) == -1)
			    system_fatal("Can't stat file: %s for %s %s %s",
				    argv[i+3], argv[i], argv[i+1], argv[i+2]);
			/*
			 * For some reason mapping files with zero size fails
			 * so it has to be handled specially.
			 */
			if(stat_buf.st_size != 0){
			    if((r = map_fd((int)fd, (vm_offset_t)0,
				(vm_offset_t *)&(sect_spec->order_addr),
				(boolean_t)TRUE, (vm_size_t)stat_buf.st_size)
				) != KERN_SUCCESS)
				mach_fatal(r, "can't map file: %s for %s %s %s",
				    argv[i+3], argv[i], argv[i+1], argv[i+2]);
			}
			else{
			    sect_spec->order_addr = NULL;
			}
			close(fd);
			sect_spec->order_size = stat_buf.st_size;
			sect_spec->order_filename = argv[i+3];
			i += 3;
		    }
		    else if(strcmp(p, "sectorder_detail") == 0){
			sectorder_detail = TRUE;
		    }
		    /*
		     * Flags for specifing information about segments.
		     */
		    /* specify the address (in hex) of a segment
		       -segaddr <segname> <address> */
		    else if(strcmp(p, "segaddr") == 0){
			if(i + 2 >= argc)
			    fatal("-segaddr: arguments missing");
			seg_spec = create_segment_spec(argv[i+1]);
			if(seg_spec->addr_specified == TRUE)
			    fatal("address of segment %s multiply specified",
				  argv[i+1]);
			segaddr_specified = TRUE;
			seg_spec->addr_specified = TRUE;
			seg_spec->addr = strtoul(argv[i+2], &endp, 16);
			if(*endp != '\0')
			    fatal("address for -segaddr %s %s not a proper "
				  "hexadecimal number", argv[i+1], argv[i+2]);
			i += 2;
		    }
		    /* specify the protection for a segment
		       -segprot <segname> <maxprot> <initprot>
		       where the protections are specified with "rwx" with a 
		       "-" for no protection. */
		    else if(strcmp(p, "segprot") == 0){
			if(i + 3 >= argc)
			    fatal("-segprot: arguments missing");
			seg_spec = create_segment_spec(argv[i+1]);
			if(seg_spec->prot_specified == TRUE)
			    fatal("protection of segment %s multiply "
				  "specified", argv[i]);
			seg_spec->maxprot = getprot(argv[i+2], &endp);
			if(*endp != '\0')
			    fatal("bad character: '%c' in maximum protection: "
				  "%s for segment %s", *endp, argv[i+2],
				  argv[i+1]);
			seg_spec->initprot = getprot(argv[i+3], &endp);
			if(*endp != '\0')
			    fatal("bad character: '%c' in initial protection: "
				  "%s for segment %s", *endp, argv[i+3],
				  argv[i+1]);
			if(check_max_init_prot(seg_spec->maxprot,
					       seg_spec->initprot) == FALSE)
			    fatal("maximum protection: %s for segment: %s "	
				  "doesn't include all initial protections: %s",
				  argv[i+2], argv[i+1], argv[i+3]);
			seg_spec->prot_specified = TRUE;
			i += 3;
		    }
		    /* specify the address (in hex) of the first segment
		       -seg1addr <address> */
		    else if(strcmp(p, "seg1addr") == 0){
			if(i + 1 >= argc)
			    fatal("%s: argument missing", argv[i]);
			if(seg1addr_specified == TRUE)
			    fatal("%s: multiply specified", argv[i]);
			seg1addr = strtoul(argv[i+1], &endp, 16);
			if(*endp != '\0')
			    fatal("address for %s %s not a proper "
				  "hexadecimal number", argv[i], argv[i+1]);
			seg1addr_specified = TRUE;
			i += 1;
		    }
		    /* specify the address (in hex) of the read-only segments
		       -segs_read_only_addr <address> */
		    else if(strcmp(p, "segs_read_only_addr") == 0){
			if(i + 1 >= argc)
			    fatal("%s: argument missing", argv[i]);
			if(segs_read_only_addr_specified == TRUE)
			    fatal("%s: multiply specified", argv[i]);
			segs_read_only_addr = strtoul(argv[i+1], &endp, 16);
			if(*endp != '\0')
			    fatal("address for %s %s not a proper "
				  "hexadecimal number", argv[i], argv[i+1]);
			segs_read_only_addr_specified = TRUE;
			i += 1;
		    }
		    /* specify the address (in hex) of the read-write segments
		       -segs_read_write_addr <address> */
		    else if(strcmp(p, "segs_read_write_addr") == 0){
			if(i + 1 >= argc)
			    fatal("%s: argument missing", argv[i]);
			if(segs_read_write_addr_specified == TRUE)
			    fatal("%s: multiply specified", argv[i]);
			segs_read_write_addr = strtoul(argv[i+1], &endp, 16);
			if(*endp != '\0')
			    fatal("address for %s %s not a proper "
				  "hexadecimal number", argv[i], argv[i+1]);
			segs_read_write_addr_specified = TRUE;
			i += 1;
		    }
		    /* specify the segment alignment as a hexadecimal power of 2
		       -segalign <number> */
		    else if(strcmp(p, "segalign") == 0){
			if(segalign_specified)
			    fatal("-segalign: multiply specified");
			if(++i >= argc)
			    fatal("-segalign: argument missing");
			segalign = strtoul(argv[i], &endp, 16);
			if(*endp != '\0')
			    fatal("argument for -segalign %s not a proper "
				  "hexadecimal number", argv[i]);
			if(!ispoweroftwo(segalign) || segalign == 0)
			    fatal("argument to -segalign: %lx (hex) must be a "
				  "non-zero power of two", segalign);
			if(segalign > MAXSEGALIGN)
			    fatal("argument to -segalign: %lx (hex) must equal "
				  "to or less than %x (hex)", segalign,
				  (unsigned int)MAXSEGALIGN);
			segalign_specified = TRUE;
			if(segalign < (1 << DEFAULTSECTALIGN)){
			    defaultsectalign = 0;
			    align = segalign;
			    while((align & 0x1) != 1){
				defaultsectalign++;
				align >>= 1;
			    }
			}
		    }
		    else if(strcmp(p, "seglinkedit") == 0){
			if(seglinkedit_specified && seglinkedit == FALSE)
			    fatal("both -seglinkedit and -noseglinkedit can't "
				  "be specified");
			seglinkedit = TRUE;
			seglinkedit_specified = TRUE;
		    }
		    /* specify the stack address as a hexadecimal number
		       -stack_addr <address> */
		    else if(strcmp(p, "stack_addr") == 0){
			if(i + 1 >= argc)
			    fatal("%s: argument missing", argv[i]);
			if(stack_addr_specified == TRUE)
			    fatal("%s: multiply specified", argv[i]);
			stack_addr = strtoul(argv[i+1], &endp, 16);
			if(*endp != '\0')
			    fatal("address for %s %s not a proper "
				  "hexadecimal number", argv[i], argv[i+1]);
			stack_addr_specified = TRUE;
			i += 1;
		    }
		    /* specify the stack size as a hexadecimal number
		       -stack_size <address> */
		    else if(strcmp(p, "stack_size") == 0){
			if(i + 1 >= argc)
			    fatal("%s: argument missing", argv[i]);
			if(stack_size_specified == TRUE)
			    fatal("%s: multiply specified", argv[i]);
			stack_size = strtoul(argv[i+1], &endp, 16);
			if(*endp != '\0')
			    fatal("address for %s %s not a proper "
				  "hexadecimal number", argv[i], argv[i+1]);
			stack_size_specified = TRUE;
			i += 1;
		    }
		    else
			goto unknown_flag;
		    break;

		case 't':
		    /* trace flag */
		    if(p[1] != '\0')
			goto unknown_flag;
		    trace = TRUE;
		    break;

		case 'o':
		    if(strcmp(p, "object") == 0){
			if(filetype_specified == TRUE && filetype != MH_OBJECT)
			    fatal("more than one output filetype specified");
			filetype_specified = TRUE;
			filetype = MH_OBJECT;
			break;
		    }
		    /* specify the output file name */
		    if(p[1] != '\0')
			goto unknown_flag;
		    if(outputfile != NULL)
			fatal("-o: multiply specified");
		    if(++i >= argc)
			fatal("-o: argument missing");
		    outputfile = argv[i];
		    break;

		case 'a':
		    if(strcmp(p, "all_load") == 0)
			archive_all = TRUE;
		    else if(strcmp(p, "arch_multiple") == 0)
			arch_multiple = TRUE;
		    else if(strcmp(p, "arch_errors_fatal") == 0)
			arch_errors_fatal = TRUE;
		    else if(strcmp(p, "arch") == 0){
			if(++i >= argc)
			    fatal("-arch: argument missing");
			if(arch_flag.name != NULL &&
			   strcmp(arch_flag.name, argv[i]) != 0)
			    fatal("-arch: multiply specified");
			if(get_arch_from_flag(argv[i], &arch_flag) == 0){
			    error("unknown architecture specification flag: "
				  "-arch %s", argv[i]);
			    fatal("Usage: %s [options] file [...]", progname);
			}
			target_byte_sex = get_byte_sex_from_flag(&arch_flag);
		    }
		    else
			goto unknown_flag;
		    break;

		/* Flags dealing with symbols */
		case 'i':
		    if(strcmp(p, "image_base") == 0){
			if(i + 1 >= argc)
			    fatal("%s: argument missing", argv[i]);
			if(seg1addr_specified == TRUE)
			    fatal("%s: argument missing", argv[i]);
			seg1addr = strtoul(argv[i+1], &endp, 16);
			if(*endp != '\0')
			    fatal("address for %s %s not a proper "
				  "hexadecimal number", argv[i], argv[i+1]);
			seg1addr_specified = TRUE;
			i += 1;
		    }
		    else if(strcmp(p, "init") == 0){
			/* check to see if the pointer is not already set */
			if(init_name != NULL)
			    fatal("-init: multiply specified");
			if(++i >= argc)
			    fatal("-init: argument missing");
			init_name = argv[i];
		    }
		    else{
			/* create an indirect symbol, symbol_name, to be an
			   indirect symbol for indr_symbol_name */
			symbol_name = p + 1;
			indr_symbol_name = strchr(p + 1, ':');
			if(indr_symbol_name == NULL ||
			   indr_symbol_name[1] == '\0' || *symbol_name == ':')
			    fatal("-i argument: %s must have a ':' between "
				  "it's symbol names", p + 1);
			/* the creating of the symbol is done in the next pass
			   of parsing arguments */
		    }
		    break;

		case 'm':
		    /* treat multiply defined symbols as a warning not a
		       hard error */
		    if(p[1] != '\0')
			goto unknown_flag;
		    allow_multiply_defined_symbols = TRUE;
		    break;

		case 'u':
		    if(strcmp(p, "undefined") == 0){
			if(++i >= argc)
			    fatal("-undefined: argument missing");
			if(strcmp(argv[i], "error") == 0)
			    new_undefined_flag = UNDEFINED_ERROR;
			else if(strcmp(argv[i], "warning") == 0)
			    new_undefined_flag = UNDEFINED_WARNING;
			else if(strcmp(argv[i], "suppress") == 0)
			    new_undefined_flag = UNDEFINED_SUPPRESS;
			else{
			    fatal("-undefined: unknown argument: %s", argv[i]);
			    new_undefined_flag = UNDEFINED_ERROR;
			}
			if(undefined_flag_specified == TRUE &&
			   new_undefined_flag != undefined_flag)
			    fatal("more than one value specified for "
				  "-undefined");
			undefined_flag_specified = TRUE;
			undefined_flag = new_undefined_flag;
			break;
		    }
		    if(p[1] != '\0')
			goto unknown_flag;
		    /* cause the specified symbol name to be undefined */
		    if(++i >= argc)
			fatal("-u: argument missing");
		    /* the creating of the symbol is done in the next pass of
		       parsing arguments */
		    break;

		case 'e':
		    if(strcmp(p, "execute") == 0){
			if(filetype_specified == TRUE && filetype != MH_EXECUTE)
			    fatal("more than one output filetype specified");
			filetype_specified = TRUE;
			filetype = MH_EXECUTE;
			break;
		    }
		    /* specify the entry point, the symbol who's value to be
		       used as the program counter in the unix thread */
		    if(p[1] != '\0')
			goto unknown_flag;
		    /* check to see if the pointer is not already set */
		    if(entry_point_name != NULL)
			fatal("-e: multiply specified");
		    if(++i >= argc)
			fatal("-e: argument missing");
		    entry_point_name = argv[i];
		    break;

		case 'U':
		    if(p[1] != '\0')
			goto unknown_flag;
		    /* allow the specified symbol name to be undefined */
		    if(++i >= argc)
			fatal("-U: argument missing");
		    undef_syms = reallocate(undef_syms,
					    (nundef_syms + 1) * sizeof(char *));
		    undef_syms[nundef_syms++] = argv[i];
		    break;

		case 'w':
		    if(strcmp(p, "w") == 0)
			nowarnings = TRUE;
		    else if(strcmp(p, "whyload") == 0)
			whyload = TRUE;
		    else if(strcmp(p, "whatsloaded") == 0)
			whatsloaded = TRUE;
		    else
			goto unknown_flag;
		    break;

		case 'O':
		    if(strcmp(p, "ObjC") == 0)
			archive_ObjC = TRUE;
		    else
			goto unknown_flag;
		    break;

		case 'y':
		    /* symbol tracing */
		    if(p[1] == '\0')
			fatal("-y: symbol name missing");
		    trace_syms = reallocate(trace_syms,
					    (ntrace_syms + 1) * sizeof(char *));
		    trace_syms[ntrace_syms++] = &(p[1]);
		    break;

		case 'Y':
		    /* undefined reference symbol tracing */
		    if(strcmp(p, "Y") == 0){
			if(i + 1 >= argc)
			    fatal("-Y: argument missing");
			if(Yflag != 0)
			    fatal("-Y: multiply specified");
			Yflag = strtoul(argv[i+1], &endp, 10);
			if(*endp != '\0')
			    fatal("reference count for -Y %s not a proper "
				  "decimal number", argv[i+1]);
		    }
		    else
			goto unknown_flag;
		    break;

		case 'h':
		    /* specify the header pad (in hex)
		       -headerpad <value> */
		    if(strcmp(p, "headerpad") == 0){
			if(i + 1 >= argc)
			    fatal("-headerpad: argument missing");
			if(headerpad_specified == TRUE)
			    fatal("-headerpad: multiply specified");
			headerpad = strtoul(argv[i+1], &endp, 16);
			if(*endp != '\0')
			    fatal("address for -headerpad %s not a proper "
				  "hexadecimal number", argv[i+1]);
			headerpad_specified = TRUE;
			i += 1;
		    }
		    else
			goto unknown_flag;
		    break;

		case 'k':
		    if(strcmp(p, "keep_private_externs") == 0)
			keep_private_externs = TRUE;
		    else if(strcmp(p, "k") == 0)
			dynamic = TRUE;
		    else
			goto unknown_flag;
		    break;

		case 'N':
		    if(strcmp(p, "NEXTSTEP-deployment-target") == 0){
			if(i + 1 >= argc)
			    fatal("-NEXTSTEP-deployment-target: argument "
				  "missing");
			if(dynamic_specified == TRUE ||
			   static_specified == TRUE)
			    fatal("-NEXTSTEP-deployment-target, -dynamic or "
				  "-static : multiply specified");
			if(strcmp(argv[i+1], "3.3") == 0){
			    if(static_specified)
				fatal("only one of -NEXTSTEP-deployment-target "
				      "3.3 or -static can be specified");
			    dynamic = TRUE;
			    dynamic_specified = TRUE;
			}
			else if(strcmp(argv[i+1], "3.2") == 0){
			    if(dynamic_specified)
				fatal("only one of -NEXTSTEP-deployment-target "
				      "3.2 or -dynamic can be specified");
			    dynamic = FALSE;
			    static_specified = TRUE;
			}
			else
			    fatal("unknown deployment release flag: "
				"-NEXTSTEP-deployment-target %s", argv[i+1]);
			i += 1;
		    }
		    else
			goto unknown_flag;
		    break;

		default:
unknown_flag:
		    fatal("unknown flag: %s", argv[i]);
		}
	    }
	}

	/*
	 * If the environment variable NEXT_ROOT is set prepend it to the
	 * standard paths for library searches.  This was added to ease
	 * cross build environments.
	 */
	next_root = getenv("NEXT_ROOT");
	if(next_root != NULL){
	    for(i = 0; standard_dirs[i] != NULL; i++){
		p = allocate(strlen(next_root) +
			     strlen(standard_dirs[i]) + 1);
		strcpy(p, next_root);
		strcat(p, standard_dirs[i]);
		standard_dirs[i] = p;
	    }
	    for(i = 0; standard_framework_dirs[i] != NULL; i++){
		p = allocate(strlen(next_root) +
			     strlen(standard_framework_dirs[i]) + 1);
		strcpy(p, next_root);
		strcat(p, standard_framework_dirs[i]);
		standard_framework_dirs[i] = p;
	    }
	}
	/*
         * Test to see if the environment variables RC_TRACE_ARCHIVES or
	 * RC_TRACE_DYLIBS are set.
         */
        if(getenv("RC_TRACE_ARCHIVES") != NULL)
	    rc_trace_archives = TRUE;
        if(getenv("RC_TRACE_DYLIBS") != NULL)
	    rc_trace_dylibs = TRUE;

	/*
	 * The -prebind flag can also be specified with the LD_PREBIND
	 * environment variable.  We quitely ignore this when -r is on or
	 * if this is a fixed shared library output.
	 */
	if(getenv("LD_PREBIND") != NULL &&
	   save_reloc == FALSE &&
	   filetype != MH_FVMLIB){
	    if(prebinding_flag_specified == TRUE &&
	       prebinding == FALSE){
		warning("LD_PREBIND environment variable ignored because "
			"-noprebind specified");
	    }
	    else{
		prebinding_flag_specified = TRUE;
		prebinding = TRUE;
	    }
	}

	/*
	 * Check for flag combinations that would result in a bad output file.
	 */
	if(save_reloc && strip_level == STRIP_ALL)
	    fatal("can't use -s with -r (resulting file would not be "
		  "relocatable)");
	if(save_reloc && strip_base_symbols == TRUE)
	    fatal("can't use -b with -r (resulting file would not be "
		  "relocatable)");
	if(filetype_specified == TRUE && filetype == MH_OBJECT){
	    if(dynamic == TRUE)
		fatal("incompatible to specifiy -object when -dynamic is used "
		      "(use -execute (the default) with -dynamic or -static "
		      "with -object)");
	}
	if(filetype == MH_DYLINKER){
	    if(dynamic == FALSE)
		fatal("incompatible flag -dylinker used (must specify "
		      "\"-dynamic\" to be used)");
	}
	if(filetype == MH_DYLIB){
	    if(dynamic == FALSE)
		fatal("incompatible flag -dylib used (must specify "
		      "\"-dynamic\" to be used)");
	    if(save_reloc)
		fatal("can't use -r and -dylib (file format produced with "
		      "-dylib is not a relocatable format)");
	    if(strip_level == STRIP_ALL)
		fatal("can't use -s with -dylib (file must contain at least "
		      "global symbols, for maximum stripping use -x)");
	    if(Aflag_specified)
		fatal("can't use -A and -dylib");
	    if(keep_private_externs == TRUE)
		fatal("can't use -keep_private_externs and -dylib");
	    if(segaddr_specified)
		fatal("can't use -segaddr options with -dylib (use seg1addr to "
		      "specify the starting address)");
	    if(seg1addr_specified && segs_read_only_addr_specified)
		fatal("can't use both -seg1addr option and "
		      "-segs_read_only_addr option");
	    if(seg1addr_specified && segs_read_write_addr_specified)
		fatal("can't use both -seg1addr option and "
		      "-segs_read_write_addr option");
	    if(segs_read_write_addr_specified &&
	       !segs_read_only_addr_specified)
		fatal("must also specify -segs_read_only_addr when using "
		      "-segs_read_write_addr");
	    if(seglinkedit_specified && seglinkedit == FALSE)
		fatal("can't use -noseglinkedit with -dylib (resulting file "
		      "must have a link edit segment to access symbols)");
	    if(bind_at_load == TRUE){
		warning("-bind_at_load is meanless with -dylib");
		bind_at_load = FALSE;
	    }
	}
	else{
	    if(segs_read_only_addr_specified)
		fatal("-segs_read_only_addr can only be used when -dylib "
		      "is also specified");
	    if(segs_read_write_addr_specified)
		fatal("-segs_read_write_addr can only be used when -dylib "
		      "is also specified");
	}
	if(filetype == MH_BUNDLE){
	    if(dynamic == FALSE)
		fatal("incompatible flag -bundle used (must specify "
		      "\"-dynamic\" to be used)");
	    if(save_reloc)
		fatal("can't use -r and -bundle (flags are mutually "
		      "exclusive, only one or the other can be used)");
	    if(strip_level == STRIP_ALL)
		fatal("can't use -s with -bundle (file must contain "
		      "at least global symbols, for maximum stripping use -x)");
	    if(Aflag_specified)
		fatal("can't use -A and -bundle");
	    if(keep_private_externs == TRUE)
		fatal("can't use -keep_private_externs and -bundle");
	    if(segaddr_specified)
		fatal("can't use -segaddr options with -bundle (use "
		      "seg1addr to specify the starting address)");
	    if(seglinkedit_specified && seglinkedit == FALSE)
		fatal("can't use -noseglinkedit with -bundle "
		      "(resulting file must have a link edit segment to access "
		      "symbols)");
	    if(prebinding == TRUE){
		if(prebinding_flag_specified == TRUE)
		    warning("-prebind has no effect with -bundle");
		prebinding = FALSE;
	    }
	}
	if(filetype != MH_DYLINKER){
	    if(dylinker_install_name != NULL)
		warning("flag: -dylinker_install_name %s ignored (-dylinker "
			"was not specified", dylinker_install_name);
	}
	if(filetype != MH_DYLIB){
	    if(dylib_install_name != NULL)
		warning("flag: -dylib_install_name %s ignored (-dylib "
			"was not specified", dylib_install_name);
	    if(dylib_current_version != 0)
		warning("flag: -dylib_current_version %lu ignored (-dylib "
			"was not specified", dylib_current_version);
	    if(dylib_compatibility_version != 0)
		warning("flag: -dylib_compatibility_version %lu ignored (-dylib"
			" was not specified", dylib_compatibility_version);
	    if(init_name != NULL)
		warning("flag: -init %s ignored (-dylib was not specified",
			init_name);
	}
	if(prebinding == TRUE && undefined_flag == UNDEFINED_SUPPRESS){
	    if(prebinding_flag_specified == TRUE)
		warning("-undefined suppress disables -prebind");
	    prebinding = FALSE;
	}
	if(prebinding == TRUE && save_reloc){
	    if(prebinding_flag_specified == TRUE)
		warning("-r disables -prebind");
	    prebinding = FALSE;
	}
	if(prebinding == TRUE && dynamic == FALSE){
	    prebinding = FALSE;
	}
	/*
	 * If there was a -arch flag two things needed to be done in reguard to
	 * the handling of the cpusubtypes.
	 */
	if(arch_flag.name != NULL){
	    family_arch_flag = get_arch_family_from_cputype(arch_flag.cputype);
	    if(family_arch_flag == NULL)
		fatal("internal error: unknown cputype (%d) for -arch %s (this "
		      "program out of sync with get_arch_family_from_cputype())"
		      ,arch_flag.cputype, arch_flag.name);
	    /*
	     * First, if -force_cpusubtype_ALL is set and an -arch flag was
	     * specified set the cpusubtype to the _ALL type for that cputype 
	     * since the specified flag may not have the _ALL type and the
	     * -force_cpusubtype_ALL has precedence over an -arch flags for a
	     * specific implementation of an architecture.
	     */
	    if(force_cpusubtype_ALL == TRUE){
		arch_flag.cpusubtype = family_arch_flag->cpusubtype;
	    }
	    else{
		/*
		 * Second, if no -force_cpusubtype_ALL is specified and an -arch
		 * flag for a specific implementation of an architecture was
		 * specified then the resulting cpusubtype will be for that
		 * specific implementation of that architecture and all
		 * cpusubtypes must combine with the cpusubtype for the -arch
		 * flag to the cpusubtype for the -arch flag else an error must
		 * be flaged.  This is done check_cur_obj() where cpusubtypes
		 * are combined.  What needs to be done here is to determine if
		 * the -arch flag is for a specific implementation of an
		 * architecture.
		 */
		if(arch_flag.cpusubtype != family_arch_flag->cpusubtype)
		    specific_arch_flag = TRUE;
	    }
	}

	/*
	 * If the output file name as not specified set it to the default name
	 * "a.out".  This needs to be done before any segments are merged
	 * because this is used when merging them (the 'filename' field in a
	 * merged_segment is set to it).
	 */
	if(outputfile == NULL)
	    outputfile = "a.out";
	/*
	 * If the -A flag is specified and the file type has not been specified
	 * then make the output file type MH_OBJECT.
	 */
	if(Aflag_specified == TRUE && filetype_specified == FALSE)
	    filetype = MH_OBJECT;

	/*
	 * If neither the -seglinkedit or -noseglinkedit has been specified then
	 * set creation of this segment if the output file type can have one.
	 * If -seglinkedit has been specified make sure the output file type
	 * can have one.
	 */
	if(seglinkedit_specified == FALSE){
	    if((filetype == MH_EXECUTE || filetype == MH_BUNDLE ||
		filetype == MH_FVMLIB ||
		filetype == MH_DYLIB || filetype == MH_DYLINKER) &&
	        strip_level != STRIP_ALL)
		seglinkedit = TRUE;
	    else
		seglinkedit = FALSE;
	}
	else{
	    if(seglinkedit &&
	       (filetype != MH_EXECUTE && filetype != MH_BUNDLE &&
		filetype != MH_FVMLIB &&
		filetype != MH_DYLIB && filetype != MH_DYLINKER))
		fatal("link edit segment can't be created (wrong output file "
		      "type, file type must be MH_EXECUTE, MH_BUNDLE, "
		      "MH_DYLIB, MH_DYLINKER or MH_FVMLIB)");
	}

	if(trace)
	    print("%s: Pass 1\n", progname);
	/*
	 * This pass of parsing arguments only processes object files
	 * and creation of symbols now that all the options are set.
	 * This are order dependent and must be processed as they appear
	 * on the command line.
	 */
	symbols_created = 0;
	objects_specified = 0;
	sections_created = 0;
	for(i = 1 ; i < argc ; i++){
	    if(*argv[i] != '-'){
		/* just a normal object file name */
		pass1(argv[i], FALSE, FALSE, FALSE);
		objects_specified++;
	    }
	    else{
		p = &(argv[i][1]);
		switch(*p){
		case 'l':
		    /* path searched abbrevated file name */
		    pass1(argv[i], TRUE, FALSE, FALSE);
		    objects_specified++;
		    break;
		case 'A':
		    if(base_obj != NULL)
			fatal("only -A argument can be specified");
		    pass1(argv[++i], FALSE, TRUE, FALSE);
		    objects_specified++;
		    break;
		case 'f':
		    if(strcmp(p, "framework") == 0){
			if(dynamic == FALSE)
			    fatal("incompatible flag -framework used (must "
				  "specify \"-dynamic\" to be used)");
			pass1(argv[++i], FALSE, FALSE, TRUE);
			objects_specified++;
		    }
		    if(strcmp(p, "filelist") == 0){
			filelist = argv[++i];
			dirname = strrchr(filelist, ',');
			if(dirname != NULL){
			    *dirname = '\0';
			    dirname++;
			}
			else
			    dirname = "";
			if((fd = open(filelist, O_RDONLY, 0)) == -1)
			    system_fatal("can't open file list file: %s",
				         filelist);
			if(fstat(fd, &stat_buf) == -1)
			    system_fatal("can't stat file list file: %s",
					 filelist);
			/*
			 * For some reason mapping files with zero size fails
			 * so it has to be handled specially.
			 */
			if(stat_buf.st_size != 0){
			    if((r = map_fd((int)fd, (vm_offset_t)0,
				(vm_offset_t *)&(addr), (boolean_t)TRUE,
				(vm_size_t)stat_buf.st_size)) != KERN_SUCCESS)
				mach_fatal(r, "can't map file list file: %s",
				    filelist);
			}
			else{
			    fatal("file list file: %s is empty", filelist);
			}
			close(fd);
			file_name = addr;
			for(j = 0; j < stat_buf.st_size; j++){
			    if(addr[j] == '\n'){
				addr[j] = '\0';
				if(*dirname != '\0'){
				    file_name = mkstr(dirname, "/",
						      file_name, NULL);
				}
				pass1(file_name, FALSE, FALSE, FALSE);
				objects_specified++;
				file_name = addr + j + 1;
			    }
			}
		    }
		    break;
		case 'u':
		    if(strcmp(p, "undefined") == 0){
			i++;
			break;
		    }
		    /* cause the specified symbol name to be undefined */
		    (void)command_line_symbol(argv[++i]);
		    symbols_created++;
		    break;
		case 'i':
		    if(strcmp(p, "image_base") == 0){
			i++;
			break;
		    }
		    else if(strcmp(p, "init") == 0){
			i++;
			break;
		    }
		    /* create an indirect symbol, symbol_name, to be an indirect
		       symbol for indr_symbol_name */
		    symbol_name = p + 1;
	  	    indr_symbol_name = strchr(p + 1, ':');
		    *indr_symbol_name = '\0';
		    indr_symbol_name++;
		    command_line_indr_symbol(symbol_name, indr_symbol_name);
		    symbols_created++;
		    break;

		/* multi argument flags */
		case 'a':
		    if(strcmp(p, "all_load") == 0 ||
		       strcmp(p, "arch_multiple") == 0 ||
		       strcmp(p, "arch_errors_fatal") == 0)
			break;
		    i++;
		    break;
		case 'p':
		    if(strcmp(p, "pagezero_size") == 0){
			i++;
			break;
		    }
		    break;
		case 's':
		    if(strcmp(p, "sectcreate") == 0 ||
		       strcmp(p, "segcreate") == 0){
			sections_created++;
			i += 3;
		    }
		    else if(strcmp(p, "sectalign") == 0 ||
		            strcmp(p, "segprot") == 0 ||
		            strcmp(p, "sectorder") == 0)
			i += 3;
		    else if(strcmp(p, "segaddr") == 0 ||
		            strcmp(p, "sectobjectsymbols") == 0)
			i += 2;
		    else if(strcmp(p, "seg1addr") == 0 ||
		            strcmp(p, "stack_addr") == 0 ||
		            strcmp(p, "stack_size") == 0 ||
		            strcmp(p, "segalign") == 0 ||
			    strcmp(p, "segs_read_only_addr") == 0 ||
			    strcmp(p, "segs_read_write_addr") == 0)
			i++;
		    break;
		case 'r':
		    if(strcmp(p, "r") == 0)
			break;
		    i++;
		    break;
		case 'o':
		    if(strcmp(p, "object") == 0)
			break;
		    i++;
		    break;
		case 'e':
		    if(strcmp(p, "execute") == 0)
			break;
		    i++;
		    break;
		case 'd':
		    if(strcmp(p, "d") == 0 ||
		       strcmp(p, "dylib") == 0 ||
		       strcmp(p, "dylinker") == 0 ||
		       strcmp(p, "dynamic") == 0)
			break;
		    i++;
		    break;
		case 'h':
		case 'U':
		case 'N':
		case 'Y':
		    i++;
		    break;
		}
	    }
	}

	/*
	 * Now search the libraries on the dynamic shared libraries search list
	 */
	search_dynamic_libs();

	/*
	 * Check to see that the output file will have something in it.
	 */
	if(objects_specified == 0){
	    if(symbols_created != 0 || sections_created != 0){
		warning("no object files specified, only command line created "
			"symbols and/or sections created from files will "
			"appear in the output file");
		if(arch_flag.name == NULL)
		    target_byte_sex = host_byte_sex;
		segalign = host_pagesize;
	    }
	    else
		fatal("no object files specified");
	}
	else if(base_obj != NULL && nobjects == 1){
	    if(symbols_created != 0 || sections_created != 0)
		warning("no object files loaded other than base file, only "
			"additional command line created symbols and/or "
			"sections created from files will appear in the output "
			"file");
	    else
		fatal("no object files loaded other than base file");
	}
	else if(nobjects == 0){
	    if(symbols_created != 0 || sections_created != 0)
		warning("no object files loaded, only command line created "
			"symbols and/or sections created from files will "
			"appear in the output file");
	    else
		fatal("no object files loaded");
	}

#ifdef DEBUG
	if(debug & (1 < 0))
	    print_object_list();
	if(debug & (1 << 1))
	    print_merged_sections("after pass1");
	if(debug & (1 << 2))
	    print_symbol_list("after pass1", TRUE);
	if(debug & (1 << 3))
	    print_undefined_list();
	if(debug & (1 << 4))
	    print_segment_specs();
	if(debug & (1 << 5))
	    print_load_fvmlibs_list();
	if(debug & (1 << 6))
	    print_fvmlib_segments();
	if(debug & (1 << 9)){
	    print("Number of objects loaded = %lu\n", nobjects);
	    print("Number of merged symbols = %lu\n", nmerged_symbols);
	}
#endif DEBUG

	/*
	 * If there were any errors from pass1() then don't continue.
	 */
	if(errors != 0)
	    ld_exit(1);

	/*
	 * Print which files are loaded if requested.
	 */
	if(whatsloaded == TRUE)
	    print_whatsloaded();

	/*
	 * Clean up any data structures not need for layout() or pass2().
	 */
	if(nsearch_dirs != 0){
	    free(search_dirs);
	    nsearch_dirs = 0;
	}

	/*
	 * Layout the output object file.
	 */
	layout();

	/*
	 * If there were any errors from layout() then don't continue.
	 */
	if(errors != 0)
	    ld_exit(1);

	/*
	 * Clean up any data structures not need for pass2().
	 */
	free_pass1_symbol_data();
	if(ntrace_syms != 0){
	    free(trace_syms);
	    ntrace_syms = 0;
	}
	if(nundef_syms != 0){
	    free(undef_syms);
	    nundef_syms = 0;
	}

	/*
	 * Write the output object file doing relocation on the sections.
	 */
	if(trace)
	    print("%s: Pass 2\n", progname);
	pass2();
	/*
	 * If there were any errors from pass2() make sure the output file is
	 * removed and exit non-zero.
	 */
	if(errors != 0)
	    cleanup();

	ld_exit(0);

	/* this is to remove the compiler warning, it never gets here */
	return(0);
}

/*
 * ispoweroftwo() returns TRUE or FALSE depending if x is a power of two.
 */
static
enum
bool
ispoweroftwo(
unsigned long x)
{
	if(x == 0)
	    return(TRUE);
	while((x & 0x1) != 0x1){
	    x >>= 1;
	}
	if((x & ~0x1) != 0)
	    return(FALSE);
	else
	    return(TRUE);
}

/*
 * getprot() returns the vm_prot for the specified string passed to it.  The
 * string may contain any of the following characters: 'r', 'w', 'x' and '-'
 * representing read, write, execute and no protections.  The pointer pointed
 * to by endp is set to the first character that is not one of the above
 * characters.
 */
static
vm_prot_t
getprot(
char *prot,
char **endp)
{
    vm_prot_t vm_prot;

	vm_prot = VM_PROT_NONE;
	while(*prot){
	    switch(*prot){
	    case 'r':
	    case 'R':
		vm_prot |= VM_PROT_READ;
		break;
	    case 'w':
	    case 'W':
		vm_prot |= VM_PROT_WRITE;
		break;
	    case 'x':
	    case 'X':
		vm_prot |= VM_PROT_EXECUTE;
		break;
	    case '-':
		break;
	    default:
		*endp = prot;
		return(vm_prot);
	    }
	    prot++;
	}
	*endp = prot;
	return(vm_prot);
}

/*
 * check_max_init_prot() checks to make sure that all protections in the initial
 * protection are also in the maximum protection.
 */
static
enum bool
check_max_init_prot(
vm_prot_t maxprot,
vm_prot_t initprot)
{
	if(((initprot & VM_PROT_READ)    && !(maxprot & VM_PROT_READ)) ||
	   ((initprot & VM_PROT_WRITE)   && !(maxprot & VM_PROT_WRITE)) ||
	   ((initprot & VM_PROT_EXECUTE) && !(maxprot & VM_PROT_EXECUTE)) )
	return(FALSE);
	return(TRUE);
}

/*
 * check_for_ProjectBuilder() is called once before any error messages are
 * generated and sets up what is needed to send error messages to project
 * builder.
 */
static
void
check_for_ProjectBuilder(void)
{
    char *portName, *hostName;
    
	portName = getenv("MAKEPORT");
	hostName = getenv("MAKEHOST");
	if(portName == NULL)
	    return;
	if(hostName == NULL)
	    hostName = "";
	if(netname_look_up(name_server_port, hostName, portName,
	   (int *)&ProjectBuilder_port) != KERN_SUCCESS)
	    return;
	if(ProjectBuilder_port == MACH_PORT_NULL)
	    return;
	talking_to_ProjectBuilder = 1;
}

/*
 * tell_ProjectBuilder() sends the message to project builder if talking to
 * project builder.
 */
__private_extern__
void
tell_ProjectBuilder(
char *message)
{
	make_alert(ProjectBuilder_port,
	    2, /* eventType */
	    NULL, 0, /* functionName, not used by ProjectBuilder */
	    NULL, 0, /* fileName */
	    NULL, 0, /* directory */
	    0, /* line */
	    message, strlen(message)+1 > 1024 ? 1024 : strlen(message)+1);
}

/*
 * ld_exit() is use for all exit()s from the link editor.
 */
static
void
ld_exit(
int exit_value)
{
	if(exit_value != 0 && talking_to_ProjectBuilder)
	    tell_ProjectBuilder("Link errors");
	exit(exit_value);
}

/*
 * cleanup() is called by all routines handling fatal errors to remove the
 * output file if it had been created by the link editor and exit non-zero.
 */
static
void
cleanup(void)
{
	if(output_addr != NULL)
	    unlink(outputfile);
	ld_exit(1);
}

/*
 * handler() is the routine for catching SIGINT and SIGTERM signals.
 * It cleans up and exit()'s non-zero.
 */
static
void
handler(
int sig)
{
#ifdef __MWERKS__
    int dummy;
        dummy = sig;
#endif
	cleanup();
}

/*
 * allocate() is just a wrapper around malloc that prints and error message and
 * exits if the malloc fails.
 */
__private_extern__
void *
allocate(
unsigned long size)
{
    void *p;

	if(size == 0)
	    return(NULL);
	if((p = malloc(size)) == NULL)
	    system_fatal("virtual memory exhausted (malloc failed)");
	return(p);
}

/*
 * reallocate() is just a wrapper around realloc that prints and error message
 * and exits if the realloc fails.
 */
__private_extern__
void *
reallocate(
void *p,
unsigned long size)
{
	if(p == NULL)
	    return(allocate(size));
	if((p = realloc(p, size)) == NULL)
	    system_fatal("virtual memory exhausted (realloc failed)");
	return(p);
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

/*
 * round() rounds v to a multiple of r.
 */
__private_extern__
unsigned long
round(
unsigned long v,
unsigned long r)
{
	r--;
	v += r;
	v &= ~(long)r;
	return(v);
}

#ifndef RLD
/*
 * All printing of all messages goes through this function.
 */
__private_extern__
void
vprint(
const char *format,
va_list ap)
{
	vprintf(format, ap);
}
#endif !defined(RLD)

/*
 * The print function that just calls the above vprint() function.
 */
__private_extern__
void
print(
const char *format,
...)
{
    va_list ap;

	va_start(ap, format);
	vprint(format, ap);
	va_end(ap);
}

static
void
print_architecture_banner(void)
{
    static enum bool printed = FALSE;

	if(arch_multiple == TRUE && printed == FALSE && arch_flag.name != NULL){
	    print("%s: for architecture %s\n", progname, arch_flag.name);
	    printed = TRUE;
	}
}

/*
 * Print the warning message.  This is non-fatal and does not set 'errors'.
 */
__private_extern__
void
warning(
const char *format,
...)
{
    va_list ap;

	if(nowarnings == TRUE)
	    return;
	if(arch_multiple)
	    print_architecture_banner();
	va_start(ap, format);
        print("%s: warning ", progname);
	vprint(format, ap);
        print("\n");
	va_end(ap);
}

/*
 * Print the error message and set the 'error' indication.
 */
__private_extern__
void
error(
const char *format,
...)
{
    va_list ap;

	if(arch_multiple)
	    print_architecture_banner();
	va_start(ap, format);
        print("%s: ", progname);
	vprint(format, ap);
        print("\n");
	va_end(ap);
	errors = 1;
}

/*
 * Print the fatal error message, and exit non-zero.
 */
__private_extern__
void
fatal(
const char *format,
...)
{
    va_list ap;

	if(arch_multiple)
	    print_architecture_banner();
	va_start(ap, format);
        print("%s: ", progname);
	vprint(format, ap);
        print("\n");
	va_end(ap);
	cleanup();
}

/*
 * Print the current object file name and warning message.
 */
__private_extern__
void
warning_with_cur_obj(
const char *format,
...)
{
    va_list ap;

	if(nowarnings == TRUE)
	    return;
	if(arch_multiple)
	    print_architecture_banner();
	va_start(ap, format);
        print("%s: warning ", progname);
	print_obj_name(cur_obj);
	vprint(format, ap);
        print("\n");
	va_end(ap);
}

/*
 * Print the current object file name and error message, set the non-fatal
 * error indication.
 */
__private_extern__
void
error_with_cur_obj(
const char *format,
...)
{
    va_list ap;

	if(arch_multiple)
	    print_architecture_banner();
	va_start(ap, format);
        print("%s: ", progname);
	print_obj_name(cur_obj);
	vprint(format, ap);
        print("\n");
	va_end(ap);
	errors = 1;
}

#ifndef SA_RLD
/*
 * Print the warning message along with the system error message.
 */
__private_extern__
void
system_warning(
const char *format,
...)
{
    va_list ap;
    int errnum;

	if(arch_multiple)
	    print_architecture_banner();
	errnum = errno;
	va_start(ap, format);
        print("%s: warning ", progname);
	vprint(format, ap);
	print(" (%s, errno = %d)\n", strerror(errnum), errnum);
	va_end(ap);
}

/*
 * Print the error message along with the system error message, set the
 * non-fatal error indication.
 */
__private_extern__
void
system_error(
const char *format,
...)
{
    va_list ap;
    int errnum;

	if(arch_multiple)
	    print_architecture_banner();
	errnum = errno;
	va_start(ap, format);
        print("%s: ", progname);
	vprint(format, ap);
	print(" (%s, errno = %d)\n", strerror(errnum), errnum);
	va_end(ap);
	errors = 1;
}

/*
 * Print the fatal message along with the system error message, and exit
 * non-zero.
 */
__private_extern__
void
system_fatal(
const char *format,
...)
{
    va_list ap;
    int errnum;

	if(arch_multiple)
	    print_architecture_banner();
	errnum = errno;
	va_start(ap, format);
        print("%s: ", progname);
	vprint(format, ap);
	print(" (%s, errno = %d)\n", strerror(errnum), errnum);
	va_end(ap);
	cleanup();
}

/*
 * Print the fatal error message along with the mach error string, and exit
 * non-zero.
 */
__private_extern__
void
mach_fatal(
kern_return_t r,
char *format,
...)
{
    va_list ap;

	if(arch_multiple)
	    print_architecture_banner();
	va_start(ap, format);
        print("%s: ", progname);
	vprint(format, ap);
	print(" (%s)\n", mach_error_string(r));
	va_end(ap);
	cleanup();
}
#endif /* SA_RLD */
