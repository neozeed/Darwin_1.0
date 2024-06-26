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
/*
 * The strip(1) and nmedit(l) program.  This understands only Mach-O format
 * files (with the restriction the symbol table is at the end of the file) and
 * fat files with Mach-O files in them.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>
#include <libc.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <mach-o/loader.h>
#include <mach-o/reloc.h>
#include <mach-o/nlist.h>
#include <mach-o/stab.h>
#include <stuff/breakout.h>
#include <stuff/allocate.h>
#include <stuff/errors.h>
#include <stuff/round.h>
#include "stuff/reloc.h"

/* These are set from the command line arguments */
char *progname;		/* name of the program for error messages (argv[0]) */
static char *output_file;/* name of the output file */
static char *sfile;	/* filename of global symbol names to keep */
static char *Rfile;	/* filename of global symbol names to remove */
static long Aflag;	/* save only absolute symbols with non-zero value and
			   .objc_class_name_* symbols */
static long iflag;	/* -i ignore symbols in -s file not in object */
#ifdef NMEDIT
static long pflag;	/* make all defined global symbols private extern */
#else /* !defined(NMEDIT) */
static char *dfile;	/* filename of filenames of debugger symbols to keep */
static long uflag;	/* save undefined symbols */
static long rflag;	/* save symbols referenced dynamically */
static long nflag;	/* save N_SECT global symbols */
static long Sflag;	/* -S strip only debugger symbols N_STAB */
static long xflag;	/* -x strip non-globals */
static long Xflag;	/* -X strip local symbols with 'L' names */
static long strip_all = 1;
/*
 * This is set on an object by object basis if the strip_all flag is still set
 * and the object is an executable that is for use with the dynamic linker.
 * This has the same effect as -r and -u.
 */
static enum bool default_dyld_executable = FALSE;
#endif /* NMEDIT */

/*
 * Data structures to perform selective stripping of symbol table entries.
 * save_symbols is the names of the symbols from the -s <file> argument.
 * remove_symbols is the names of the symbols from the -R <file> argument.
 */
struct symbol_list {
    char *name;		/* name of the global symbol */
    struct nlist *sym;	/* pointer to the nlist structure for this symbol */
    enum bool seen;	/* set if the symbol is seen in the input file */
};
static struct symbol_list *save_symbols = NULL;
static unsigned long nsave_symbols = 0;
static struct symbol_list *remove_symbols = NULL;
static unsigned long nremove_symbols = 0;

#ifndef NMEDIT
/*
 * saves points to an array of longs that is allocated.  This array is a map of
 * old symbol indexes to new symbol indexes.  The new symbol indexes are
 * plus 1 and zero value means that old symbol is not in the new symbol table.
 * ref_saves is used in the same way but for the reference table.
 */
static long *saves = NULL;
static long *ref_saves = NULL;

/*
 * These hold the new symbol and string table created by strip_symtab()
 * and the new counts of local, defined external and undefined symbols.
 */
static struct nlist *new_symbols = NULL;
static long new_nsyms = 0;
static char *new_strings = NULL;
static long new_strsize = 0;
static long new_nlocalsym = 0;
static long new_nextdefsym = 0;
static long new_nundefsym = 0;

/*
 * These hold the new table of contents and reference table for dylibs.
 */
static struct dylib_table_of_contents *new_tocs = NULL;
static unsigned long new_ntoc = 0;
static struct dylib_reference *new_refs = NULL;
static unsigned long new_nextrefsyms = 0;

/*
 * The list of file names to save debugging symbols from.
 */
static char **debug_filenames = NULL;
static long ndebug_filenames = 0;
struct undef_map {
    unsigned long index;
    struct nlist symbol;
};
static struct undef_map *undef_map;
static char *qsort_strings = NULL;
#endif /* !defined(NMEDIT) */

/* Internal routines */
static void usage(
    void);

static void strip_file(
    char *input_file,
    struct arch_flag *arch_flags,
    unsigned long narch_flags,
    enum bool all_archs);

static void strip_arch(
    struct arch *archs,
    unsigned long narchs,
    struct arch_flag *arch_flags,
    unsigned long narch_flags,
    enum bool all_archs);

static void strip_object(
    struct arch *arch,
    struct member *member,
    struct object *object);

#ifndef NMEDIT
static enum bool strip_symtab(
    struct arch *arch,
    struct member *member,
    struct nlist *symbols,
    long nsyms,
    char *strings,
    long strsize,
    struct dylib_table_of_contents *tocs,
    unsigned long ntoc,
    struct dylib_module *mods,
    unsigned long nmodtab,
    struct dylib_reference *refs,
    unsigned long nextrefsyms);

static int cmp_qsort_undef_map(
    const struct undef_map *sym1,
    const struct undef_map *sym2);
#endif /* !defined(NMEDIT) */

#ifdef NMEDIT
static enum bool edit_symtab(
    struct arch *arch,
    struct member *member,
    struct object *object,
    struct nlist *symbols,
    long nsyms,
    char *strings,
    long strsize);
#endif /* NMEDIT */

static void setup_symbol_list(
    char *file,
    struct symbol_list **list,
    unsigned long *size);

static int cmp_qsort_name(
    const struct symbol_list *sym1,
    const struct symbol_list *sym2);

static int cmp_bsearch(
    const char *name,
    const struct symbol_list *sym);

#ifndef NMEDIT
static void setup_debug_filenames(
    char *dfile);

static int cmp_qsort_filename(
    const char **name1,
    const char **name2);

static int cmp_bsearch_filename(
    const char *name1,
    const char **name2);
#endif /* NMEDIT */

#ifdef NMEDIT
/*
 * This variable and routines are used for nmedit(1) only.
 */
static char *global_strings = NULL;

static int cmp_qsort_global(
    const struct nlist **sym1,
    const struct nlist **sym2);

static int cmp_bsearch_global_stab(
    const char *name,
    const struct nlist **sym);

static int cmp_bsearch_global(
    const char *name,
    const struct nlist **sym);
#endif /* NMEDIT */

int
main(
int argc,
char *argv[],
char *envp[])
{
    unsigned long i, j, args_left, files_specified;
    struct arch_flag *arch_flags;
    unsigned long narch_flags;
    enum bool all_archs;
    struct symbol_list *sp;

	progname = argv[0];

	arch_flags = NULL;
	narch_flags = 0;
	all_archs = FALSE;

	files_specified = 0;
	args_left = 1;
	for (i = 1; i < argc; i++){
	    if(argv[i][0] == '-'){
		if(argv[i][1] == '\0'){
		    args_left = 0;
		    break;
		}
		if(strcmp(argv[i], "-o") == 0){
		    if(i + 1 >= argc)
			fatal("-o requires an argument");
		    if(output_file != NULL)
			fatal("only one -o option allowed");
		    output_file = argv[i + 1];
		    i++;
		}
		else if(strcmp(argv[i], "-s") == 0){
		    if(i + 1 >= argc)
			fatal("-s requires an argument");
		    if(sfile != NULL)
			fatal("only one -s option allowed");
		    sfile = argv[i + 1];
		    i++;
		}
		else if(strcmp(argv[i], "-R") == 0){
		    if(i + 1 >= argc)
			fatal("-R requires an argument");
		    if(Rfile != NULL)
			fatal("only one -R option allowed");
		    Rfile = argv[i + 1];
		    i++;
		}
#ifndef NMEDIT
		else if(strcmp(argv[i], "-d") == 0){
		    if(i + 1 >= argc)
			fatal("-d requires an argument");
		    if(dfile != NULL)
			fatal("only one -d option allowed");
		    dfile = argv[i + 1];
		    i++;
		}
#endif /* !defined(NMEDIT) */
		else if(strcmp(argv[i], "-arch") == 0){
		    if(i + 1 == argc){
			error("missing argument(s) to %s option", argv[i]);
			usage();
		    }
		    if(strcmp("all", argv[i+1]) == 0){
			all_archs = TRUE;
		    }
		    else{
			arch_flags = reallocate(arch_flags,
				(narch_flags + 1) * sizeof(struct arch_flag));
			if(get_arch_from_flag(argv[i+1],
					      arch_flags + narch_flags) == 0){
			    error("unknown architecture specification flag: "
				  "%s %s", argv[i], argv[i+1]);
			    arch_usage();
			    usage();
			}
			for(j = 0; j < narch_flags; j++){
			    if(arch_flags[j].cputype ==
				    arch_flags[narch_flags].cputype &&
			       arch_flags[j].cpusubtype ==
				    arch_flags[narch_flags].cpusubtype &&
			       strcmp(arch_flags[j].name,
				    arch_flags[narch_flags].name) == 0)
				break;
			}
			if(j == narch_flags)
			    narch_flags++;
		    }
		    i++;
		}
		else{
		    for(j = 1; argv[i][j] != '\0'; j++){
			switch(argv[i][j]){
#ifdef NMEDIT
			case 'p':
			    pflag = 1;
			    break;
#else /* !defined(NMEDIT) */
			case 'S':
			    Sflag = 1;
			    strip_all = 0;
			    break;
			case 'X':
			    Xflag = 1;
			    strip_all = 0;
			    break;
			case 'x':
			    xflag = 1;
			    strip_all = 0;
			    break;
			case 'i':
			    iflag = 1;
			    break;
			case 'u':
			    uflag = 1;
			    strip_all = 0;
			    break;
			case 'r':
			    rflag = 1;
			    strip_all = 0;
			    break;
			case 'n':
			    nflag = 1;
			    strip_all = 0;
			    break;
#endif /* !defined(NMEDIT) */
			case 'A':
			    Aflag = 1;
#ifndef NMEDIT
			    strip_all = 0;
#endif /* !defined(NMEDIT) */
			    break;
			default:
			    error("unrecognized option: %s", argv[i]);
			    usage();
			}
		    }
		}
	    }
	    else
		files_specified++;
	}
	if(args_left == 0)
	    files_specified += argc - (i + 1);
	
	if(files_specified > 1 && output_file != NULL){
	    error("-o <filename> can only be used when one file is specified");
	    usage();
	}

	if(sfile){
	    setup_symbol_list(sfile, &save_symbols, &nsave_symbols);
	}
#ifdef NMEDIT
	else{
	    if(Rfile == NULL && pflag == 0){
		error("-s <filename>, -R <filename> or -p argument required");
		usage();
	    }
	}
#endif /* NMEDIT */

	if(Rfile){
	    setup_symbol_list(Rfile, &remove_symbols, &nremove_symbols);
	    if(sfile){
		for(i = 0; i < nremove_symbols ; i++){
		    sp = bsearch(remove_symbols[i].name,
				 save_symbols, nsave_symbols,
				 sizeof(struct symbol_list),
				 (int (*)(const void *, const void *))
				    cmp_bsearch);
		    if(sp != NULL){
			error("symbol name: %s is listed in both -s %s and -R "
			      "%s files (can't be both saved and removed)",
			      remove_symbols[i].name, sfile, Rfile);
		    }
		}
		if(errors)
		    exit(EXIT_FAILURE);
	    }
	}

	/* the default when no -arch flags is present is to strip all archs */
	if(narch_flags == 0)
	   all_archs = TRUE;

#ifndef NMEDIT
	if(dfile){
	    setup_debug_filenames(dfile);
	}
#endif /* !defined(NMEDIT) */

	files_specified = 0;
	args_left = 1;
	for (i = 1; i < argc; i++) {
	    if(args_left && argv[i][0] == '-'){
		if(argv[i][1] == '\0')
		    args_left = 0;
		else if(strcmp(argv[i], "-o") == 0 ||
			strcmp(argv[i], "-s") == 0 ||
			strcmp(argv[i], "-R") == 0 ||
#ifndef NMEDIT
			strcmp(argv[i], "-d") == 0 ||
#endif /* !defined(NMEDIT) */
			strcmp(argv[i], "-arch") == 0)
		    i++;
	    }
	    else{
		strip_file(argv[i], arch_flags, narch_flags, all_archs);
		files_specified++;
	    }
	}
	if(files_specified == 0)
	    fatal("no files specified");

	if(errors)
	    return(EXIT_FAILURE);
	else
	    return(EXIT_SUCCESS);
}

static
void
usage(
void)
{
#ifndef NMEDIT
	fprintf(stderr, "Usage: %s [-AnuSXx] [-] [-d filename] [-s filename] "
		"[-R filename] [-o output] file [...] \n", progname);
#else /* defined(NMEDIT) */
	fprintf(stderr, "Usage: %s -s filename [-R filename] [-p] [-A] [-] "
		"[-o output] file [...] \n",
		progname);
#endif /* NMEDIT */
	exit(EXIT_FAILURE);
}

static
void
strip_file(
char *input_file,
struct arch_flag *arch_flags,
unsigned long narch_flags,
enum bool all_archs)
{
    struct arch *archs;
    unsigned long narchs;
    struct stat stat_buf;
    unsigned long previous_errors;

	archs = NULL;
	narchs = 0;
	previous_errors = errors;
	errors = 0;

	/* breakout the file for processing */
	breakout(input_file, &archs, &narchs);
	if(errors)
	    return;

	/* checkout the file for symbol table replacement processing */
	checkout(archs, narchs);

	/* process the symbols in the input file */
	strip_arch(archs, narchs, arch_flags, narch_flags, all_archs);
	if(errors)
	    return;

	/* create the output file */
	if(stat(input_file, &stat_buf) == -1)
	    system_error("can't stat input file: %s", input_file);
	if(output_file != NULL){
	    writeout(archs, narchs, output_file, stat_buf.st_mode & 0777,
		     TRUE, FALSE, FALSE);
	}
	else{
#ifdef NMEDIT
	    output_file = makestr(input_file, ".nmedit", NULL);
#else /* !defined(NMEDIT) */
	    output_file = makestr(input_file, ".strip", NULL);
#endif /* NMEDIT */
	    writeout(archs, narchs, output_file, stat_buf.st_mode & 0777,
		     TRUE, FALSE, FALSE);
	    if(rename(output_file, input_file) == 1)
		system_error("can't move temporary file: %s to input "
			     "file: %s\n", output_file, input_file);
	    free(output_file);
	    output_file = NULL;
	}

	/* clean-up data structures */
	free_archs(archs, narchs);

	errors += previous_errors;
}

static
void
strip_arch(
struct arch *archs,
unsigned long narchs,
struct arch_flag *arch_flags,
unsigned long narch_flags,
enum bool all_archs)
{
    unsigned long i, j, k, offset, size, missing_syms;
    cpu_type_t cputype;
    cpu_subtype_t cpusubtype;
    struct arch_flag host_arch_flag;
    enum bool arch_process, any_processing, *arch_flag_processed, family;
    const struct arch_flag *family_arch_flag;

	/*
	 * Using the specified arch_flags process specified objects for those
	 * architecures.
	 */
	any_processing = FALSE;
	arch_flag_processed = NULL;
	if(narch_flags != 0)
	    arch_flag_processed = allocate(narch_flags * sizeof(enum bool));
	memset(arch_flag_processed, '\0', narch_flags * sizeof(enum bool));
	for(i = 0; i < narchs; i++){
	    /*
	     * Determine the architecture (cputype and cpusubtype) of arch[i]
	     */
	    cputype = 0;
	    cpusubtype = 0;
	    if(archs[i].type == OFILE_ARCHIVE){
		for(j = 0; j < archs[i].nmembers; j++){
		    if(archs[i].members[j].type == OFILE_Mach_O){
			cputype = archs[i].members[j].object->mh->cputype;
			cpusubtype = archs[i].members[j].object->mh->cpusubtype;
			break;
		    }
		}
	    }
	    else if(archs[i].type == OFILE_Mach_O){
		cputype = archs[i].object->mh->cputype;
		cpusubtype = archs[i].object->mh->cpusubtype;
	    }
	    else if(archs[i].fat_arch != NULL){
		cputype = archs[i].fat_arch->cputype;
		cpusubtype = archs[i].fat_arch->cpusubtype;
	    }
	    arch_process = FALSE;
	    if(all_archs == TRUE){
		arch_process = TRUE;
	    }
	    else if(narch_flags != 0){
		family = FALSE;
		if(narch_flags == 1){
		    family_arch_flag =
			get_arch_family_from_cputype(arch_flags[0].cputype);
		    if(family_arch_flag != NULL)
			family = (enum bool)(family_arch_flag->cpusubtype ==
					     arch_flags[0].cpusubtype);
		}
		for(j = 0; j < narch_flags; j++){
		    if(arch_flags[j].cputype == cputype &&
		       (arch_flags[j].cpusubtype == cpusubtype ||
			family == TRUE)){
			arch_process = TRUE;
			arch_flag_processed[j] = TRUE;
			break;
		    }
		}
	    }
	    else{
		(void)get_arch_from_host(&host_arch_flag, NULL);
		if(host_arch_flag.cputype == cputype &&
		   host_arch_flag.cpusubtype == cpusubtype)
		    arch_process = TRUE;
	    }
	    if(narchs != 1 && arch_process == FALSE)
		continue;
	    any_processing = TRUE;

	    /*
	     * Now this arch[i] has been selected to be processed so process it
	     * according to it's type.
	     */
	    if(archs[i].type == OFILE_ARCHIVE){
		for(j = 0; j < archs[i].nmembers; j++){
		    if(archs[i].members[j].type == OFILE_Mach_O){
			strip_object(archs + i, archs[i].members + j,
				     archs[i].members[j].object);
		    }
		}
		missing_syms = 0;
		if(iflag == 0){
		    for(k = 0; k < nsave_symbols; k++){
			if(save_symbols[k].seen == FALSE){
			    if(missing_syms == 0){
				error_arch(archs + i, NULL, "symbols names "
					   "listed in: %s not in: ", sfile);
				missing_syms = 1;
			    }
			    fprintf(stderr, "%s\n", save_symbols[k].name);
			}
		    }
		}
		for(k = 0; k < nsave_symbols; k++){
		    save_symbols[k].seen = FALSE;
		}
		missing_syms = 0;
		if(iflag == 0){
		    for(k = 0; k < nremove_symbols; k++){
			if(remove_symbols[k].seen == FALSE){
			    if(missing_syms == 0){
				error_arch(archs + i, NULL, "symbols names "
					   "listed in: %s not defined in: ",
					   Rfile);
				missing_syms = 1;
			    }
			    fprintf(stderr, "%s\n", remove_symbols[k].name);
			}
		    }
		}
		for(k = 0; k < nremove_symbols; k++){
		    remove_symbols[k].seen = FALSE;
		}
		/*
		 * Reset the library offsets and size.
		 */
		offset = 0;
		for(j = 0; j < archs[i].nmembers; j++){
		    archs[i].members[j].offset = offset;
		    size = 0;
		    if(archs[i].members[j].member_long_name == TRUE){
			size = round(archs[i].members[j].member_name_size,
				     sizeof(long));
			archs[i].toc_long_name = TRUE;
		    }
		    if(archs[i].members[j].object != NULL){
			size += archs[i].members[j].object->object_size
			   - archs[i].members[j].object->input_sym_info_size
			   + archs[i].members[j].object->output_sym_info_size;
			sprintf(archs[i].members[j].ar_hdr->ar_size, "%-*ld",
			       (int)sizeof(archs[i].members[j].ar_hdr->ar_size),
			       (long)(size));
			/*
			 * This has to be done by hand because sprintf puts a
			 * null at the end of the buffer.
			 */
			memcpy(archs[i].members[j].ar_hdr->ar_fmag, ARFMAG,
			      (int)sizeof(archs[i].members[j].ar_hdr->ar_fmag));
		    }
		    else{
			size += archs[i].members[j].unknown_size;
		    }
		    offset += sizeof(struct ar_hdr) + size;
		}
		archs[i].library_size = offset;
	    }
	    else if(archs[i].type == OFILE_Mach_O){
		strip_object(archs + i, NULL, archs[i].object);
	    }
	    else {
		warning_arch(archs + i, NULL, "can't process non-object and "
			   "non-archive file: ");
		return;
	    }
	}
	if(all_archs == FALSE && narch_flags != 0){
	    for(i = 0; i < narch_flags; i++){
		if(arch_flag_processed[i] == FALSE)
		    error("file: %s does not contain architecture: %s",
			  archs[0].file_name, arch_flags[i].name);
	    }
	    free(arch_flag_processed);
	}
	if(any_processing == FALSE)
	    fatal("no processing done on input file: %s (specify a -arch flag)",
		  archs[0].file_name);
}

static
void
strip_object(
struct arch *arch,
struct member *member,
struct object *object)
{
    enum byte_sex host_byte_sex;
    struct nlist *symbols;
    unsigned long nsyms;
    char *strings;
    unsigned long strsize;
#ifndef NMEDIT
    unsigned long offset;
    struct dylib_table_of_contents *tocs;
    unsigned long ntoc;
    struct dylib_module *mods;
    unsigned long nmodtab;
    struct dylib_reference *refs;
    unsigned long nextrefsyms;
    long i, j, k;
    struct load_command *lc;
    struct segment_command *sg;
    struct section *s;
    struct relocation_info *relocs;
    struct scattered_relocation_info *sreloc;
    long missing_reloc_symbols;
    unsigned long stride, section_type, nitems, index;
#endif /* NMEDIT */

	host_byte_sex = get_host_byte_sex();

	if(object->st == NULL || object->st->nsyms == 0){
	    warning_arch(arch, member, "input object file stripped: ");
	    return;
	}

	symbols = (struct nlist *)(object->object_addr + object->st->symoff);
	nsyms = object->st->nsyms;
	if(object->object_byte_sex != host_byte_sex)
	    swap_nlist(symbols, nsyms, host_byte_sex);
	strings = object->object_addr + object->st->stroff;
	strsize = object->st->strsize;

#ifndef NMEDIT
	if(object->mh->filetype == MH_DYLIB){
	    tocs = (struct dylib_table_of_contents *)
		    (object->object_addr + object->dyst->tocoff);
	    ntoc = object->dyst->ntoc;
	    mods = (struct dylib_module *)
		    (object->object_addr + object->dyst->modtaboff);
	    nmodtab = object->dyst->nmodtab;
	    refs = (struct dylib_reference *)
		    (object->object_addr + object->dyst->extrefsymoff);
	    nextrefsyms = object->dyst->nextrefsyms;
	    if(object->object_byte_sex != host_byte_sex){
		swap_dylib_table_of_contents(tocs, ntoc, host_byte_sex);
		swap_dylib_module(mods, nmodtab, host_byte_sex);
		swap_dylib_reference(refs, nextrefsyms, host_byte_sex);
	    }
	}
	else{
	    tocs = NULL;
	    ntoc = 0;
	    mods = NULL;
	    nmodtab = 0;
	    refs = NULL;
	    nextrefsyms = 0;
	}
#endif /* NMEDIT */

#ifdef NMEDIT
	/*
	 * For nmedit, edit_symtab() will change some indirect symbol table
	 * entries to INDIRECT_SYMBOL_LOCAL or INDIRECT_SYMBOL_ABS for symbols
	 * that it turns into statics.
	 */
	if(object->dyst != NULL && object->dyst->nindirectsyms != 0){
	    object->output_indirect_symtab = (unsigned long *)
		(object->object_addr + object->dyst->indirectsymoff);
	}
	if(edit_symtab(arch, member, object, symbols, nsyms, strings,
		       strsize) == FALSE)
	    return;
	if(object->object_byte_sex != host_byte_sex)
	    swap_nlist(symbols, nsyms, object->object_byte_sex);
#else /* !defined(NMEDIT) */

	object->input_sym_info_size =
	    nsyms * sizeof(struct nlist) +
	    strsize;
	if(strip_all &&
	   (object->mh->flags & MH_DYLDLINK) == MH_DYLDLINK &&
	   object->mh->filetype == MH_EXECUTE)
	    default_dyld_executable = TRUE;
	else
	    default_dyld_executable = FALSE;

	if(sfile != NULL || Rfile != NULL || dfile != NULL || Aflag || uflag ||
	   Sflag || xflag || Xflag || nflag || rflag ||default_dyld_executable){
	    if(strip_symtab(arch, member, symbols, nsyms, strings, strsize,
		       tocs, ntoc, mods, nmodtab, refs, nextrefsyms) == FALSE)
		return;
	    object->output_sym_info_size =
		new_nsyms * sizeof(struct nlist) +
		new_strsize;

	    object->st->nsyms = new_nsyms; 
	    object->st->strsize = new_strsize;

	    object->output_symbols = new_symbols;
	    object->output_nsymbols = new_nsyms;
	    object->output_strings = new_strings;
	    object->output_strings_size = new_strsize;

	    if(object->dyst != NULL){
		object->dyst->ilocalsym = 0;
		object->dyst->nlocalsym = new_nlocalsym;
		object->dyst->iextdefsym = new_nlocalsym;
		object->dyst->nextdefsym = new_nextdefsym;
		object->dyst->iundefsym = new_nlocalsym + new_nextdefsym;
		object->dyst->nundefsym = new_nundefsym;

		if(object->mh->filetype == MH_DYLIB){
		    object->output_tocs = new_tocs;
		    object->output_ntoc = new_ntoc;
		    object->output_mods = mods;
		    object->output_nmodtab = nmodtab;
		    object->output_refs = new_refs;
		    object->output_nextrefsyms = new_nextrefsyms;
		    if(object->object_byte_sex != host_byte_sex){
			swap_dylib_table_of_contents(new_tocs, new_ntoc,
			    object->object_byte_sex);
			swap_dylib_module(mods, nmodtab,
			    object->object_byte_sex);
			swap_dylib_reference(new_refs, new_nextrefsyms,
			    object->object_byte_sex);
		    }
		}
		object->input_sym_info_size +=
		    object->dyst->nlocrel * sizeof(struct relocation_info) +
		    object->dyst->nextrel * sizeof(struct relocation_info) +
		    object->dyst->nindirectsyms * sizeof(unsigned long) +
		    object->dyst->ntoc * sizeof(struct dylib_table_of_contents)+
		    object->dyst->nmodtab * sizeof(struct dylib_module) +
		    object->dyst->nextrefsyms * sizeof(struct dylib_reference);
		object->output_sym_info_size +=
		    object->dyst->nlocrel * sizeof(struct relocation_info) +
		    object->dyst->nextrel * sizeof(struct relocation_info) +
		    object->dyst->nindirectsyms * sizeof(unsigned long) +
		    new_ntoc * sizeof(struct dylib_table_of_contents)+
		    object->dyst->nmodtab * sizeof(struct dylib_module) +
		    new_nextrefsyms * sizeof(struct dylib_reference);
		object->dyst->ntoc = new_ntoc;
		object->dyst->nextrefsyms = new_nextrefsyms;

		if(object->seg_linkedit != NULL)
		    offset = object->seg_linkedit->fileoff;
		else{
		    offset = ULONG_MAX;
		    if(object->dyst->nlocrel != 0 &&
		       object->dyst->locreloff < offset)
			offset = object->dyst->locreloff;
		    if(object->st->nsyms != 0 &&
		       object->st->symoff < offset)
			offset = object->st->symoff;
		    if(object->dyst->nextrel != 0 &&
		       object->dyst->extreloff < offset)
			offset = object->dyst->extreloff;
		    if(object->dyst->nindirectsyms != 0 &&
		       object->dyst->indirectsymoff < offset)
			offset = object->dyst->indirectsymoff;
		    if(object->dyst->ntoc != 0 &&
		       object->dyst->tocoff < offset)
			offset = object->dyst->tocoff;
		    if(object->dyst->nmodtab != 0 &&
		       object->dyst->modtaboff < offset)
			offset = object->dyst->modtaboff;
		    if(object->dyst->nextrefsyms != 0 &&
		       object->dyst->extrefsymoff < offset)
			offset = object->dyst->extrefsymoff;
		    if(object->st->strsize != 0 &&
		       object->st->stroff < offset)
			offset = object->st->stroff;
		} 

		if(object->dyst->nlocrel != 0){
		    object->output_loc_relocs = (struct relocation_info *)
			(object->object_addr + object->dyst->locreloff);
		    object->dyst->locreloff = offset;
		    offset += object->dyst->nlocrel *
			      sizeof(struct relocation_info);
		}
		else
		    object->dyst->locreloff = 0;

		if(object->st->nsyms != 0){
		    object->st->symoff = offset;
		    offset += object->st->nsyms * sizeof(struct nlist);
		}
		else
		    object->st->symoff = 0;

		if(object->dyst->nextrel != 0){
		    object->output_ext_relocs = (struct relocation_info *)
			(object->object_addr + object->dyst->extreloff);
		    object->dyst->extreloff = offset;
		    offset += object->dyst->nextrel *
			      sizeof(struct relocation_info);
		}
		else
		    object->dyst->extreloff = 0;

		if(object->dyst->nindirectsyms != 0){
		    object->output_indirect_symtab = (unsigned long *)
			(object->object_addr + object->dyst->indirectsymoff);
		    object->dyst->indirectsymoff = offset;
		    offset += object->dyst->nindirectsyms *
			      sizeof(unsigned long);
		}
		else
		    object->dyst->indirectsymoff = 0;;

		if(object->dyst->ntoc != 0){
		    object->dyst->tocoff = offset;
		    offset += object->dyst->ntoc *
			      sizeof(struct dylib_table_of_contents);
		}
		else
		    object->dyst->tocoff = 0;

		if(object->dyst->nmodtab != 0){
		    object->dyst->modtaboff = offset;
		    offset += object->dyst->nmodtab *
			      sizeof(struct dylib_module);
		}
		else
		    object->dyst->modtaboff = 0;

		if(object->dyst->nextrefsyms != 0){
		    object->dyst->extrefsymoff = offset;
		    offset += object->dyst->nextrefsyms *
			      sizeof(struct dylib_reference);
		}
		else
		    object->dyst->extrefsymoff = 0;

		if(object->st->strsize != 0){
		    object->st->stroff = offset;
		    offset += object->st->strsize;
		}
		else
		    object->st->stroff = 0;
	    }
	    else{
		if(new_strsize != 0)
		    object->st->stroff = object->st->symoff +
				     new_nsyms * sizeof(struct nlist);
		else
		    object->st->stroff = 0;
		if(new_nsyms == 0)
		    object->st->symoff = 0;
	    }
	}
	else{
	    if(saves != NULL)
		free(saves);
	    saves = (long *)allocate(object->st->nsyms * sizeof(long));
	    bzero(saves, object->st->nsyms * sizeof(long));

	    object->output_sym_info_size = 0;
	    object->st->symoff = 0;
	    object->st->nsyms = 0;
	    object->st->stroff = 0;
	    object->st->strsize = 0;
	    if(object->dyst != NULL){
		object->dyst->ilocalsym = 0;
		object->dyst->nlocalsym = 0;
		object->dyst->iextdefsym = 0;
		object->dyst->nextdefsym = 0;
		object->dyst->iundefsym = 0;
		object->dyst->nundefsym = 0;
	    }
	    /*
	     * We set these so that checking can be done below to report the
	     * symbols that can't be stripped because of relocation entries
	     * or indirect symbol table entries.  If these table are non-zero
	     * number of entries it will be an error as we are trying to
	     * strip everything.
	     */
	    if(object->dyst != NULL){
		if(object->dyst->nextrel != 0){
		    object->output_ext_relocs = (struct relocation_info *)
			(object->object_addr + object->dyst->extreloff);
		}
		if(object->dyst->nindirectsyms != 0){
		    object->output_indirect_symtab = (unsigned long *)
			(object->object_addr + object->dyst->indirectsymoff);
		}
		/*
		 * Since this file has a dynamic symbol table and if this file
		 * has local relocation entries on input make sure they are
		 * there on output.  This is a rare case that it will not have
		 * external relocs or indirect symbols but can happen as is the
		 * case with the dynamic linker itself.
		 */
		if(object->dyst->nlocrel != 0){
		    object->output_loc_relocs = (struct relocation_info *)
			(object->object_addr + object->dyst->locreloff);
		    object->output_sym_info_size +=
			object->dyst->nlocrel * sizeof(struct relocation_info);
		}
	    }
	}

	if(object->seg_linkedit != NULL){
	    object->seg_linkedit->filesize += object->output_sym_info_size -
					      object->input_sym_info_size;
	    object->seg_linkedit->vmsize = object->seg_linkedit->filesize;
	}

	/*
	 * Check and update the external relocation entries to make sure
	 * referenced symbols are not stripped and refer to the new symbol
	 * table indexes.
	 * 
	 * The external relocation entries can be located in one of two places,
	 * first off of the sections or second off of the dynamic symtab.
	 */
	missing_reloc_symbols = 0;
	lc = object->load_commands;
	for(i = 0; i < object->mh->ncmds; i++){
	    if(lc->cmd == LC_SEGMENT &&
	       object->seg_linkedit != (struct segment_command *)lc){
		sg = (struct segment_command *)lc;
		s = (struct section *)((char *)sg +
					sizeof(struct segment_command));
		for(j = 0; j < sg->nsects; j++){
		    if(s->nreloc != 0){
			if(s->reloff + s->nreloc *
			   sizeof(struct relocation_info) >
						object->object_size){
			    fatal_arch(arch, member, "truncated or malformed "
				"object (relocation entries for section (%.16s,"
				"%.16s) extends past the end of the file)",
				s->segname, s->sectname);
			}
			relocs = (struct relocation_info *)
					(object->object_addr + s->reloff);
			if(object->object_byte_sex != host_byte_sex)
			    swap_relocation_info(relocs, s->nreloc,
						 host_byte_sex);
			for(k = 0; k < s->nreloc; k++){
			    if((relocs[k].r_address & R_SCATTERED) == 0 &&
			       relocs[k].r_extern == 1){
				if(relocs[k].r_symbolnum > nsyms){
				    fatal_arch(arch, member, "bad r_symbolnum "
					"for relocation entry %ld in section "
					"(%.16s,%.16s) in: ", k, s->segname,
					  s->sectname);
				}
				if(saves[relocs[k].r_symbolnum] == 0){
				    if(missing_reloc_symbols == 0){
					error_arch(arch, member, "symbols "
					    "referenced by relocation entries "
					    "that can't be stripped in: ");
					missing_reloc_symbols = 1;
				    }
				    fprintf(stderr, "%s\n", strings + symbols
					  [relocs[k].r_symbolnum].n_un.n_strx);
				    saves[relocs[k].r_symbolnum] = -1;
				}
				if(saves[relocs[k].r_symbolnum] != -1){
				    relocs[k].r_symbolnum =
					saves[relocs[k].r_symbolnum] - 1;
				}
			    }
			    if((relocs[k].r_address & R_SCATTERED) == 0){
				if(reloc_has_pair(object->mh->cputype,
						  relocs[k].r_type) == TRUE)
				    k++;
			    }
			    else{
				sreloc = (struct scattered_relocation_info *)
					 relocs + k;
				if(reloc_has_pair(object->mh->cputype,
						  sreloc->r_type) == TRUE)
				    k++;
			    }
			}
			if(object->object_byte_sex != host_byte_sex)
			    swap_relocation_info(relocs, s->nreloc,
						 object->object_byte_sex);
		    }
		    s++;
		}
	    }
	    lc = (struct load_command *)((char *)lc + lc->cmdsize);
	}
	if(object->dyst != NULL && object->dyst->nextrel != 0){
	    relocs = object->output_ext_relocs;
	    if(object->object_byte_sex != host_byte_sex)
		swap_relocation_info(relocs, object->dyst->nextrel,
				     host_byte_sex);

	    for(i = 0; i < object->dyst->nextrel; i++){
		if((relocs[i].r_address & R_SCATTERED) == 0 &&
		   relocs[i].r_extern == 1){
		    if(relocs[i].r_symbolnum > nsyms){
			fatal_arch(arch, member, "bad r_symbolnum for external "
			    "relocation entry %ld in: ", i);
		    }
		    if(saves[relocs[i].r_symbolnum] == 0){
			if(missing_reloc_symbols == 0){
			    error_arch(arch, member, "symbols referenced by "
			      "relocation entries that can't be stripped in: ");
			    missing_reloc_symbols = 1;
			}
			fprintf(stderr, "%s\n", strings + symbols
			      [relocs[i].r_symbolnum].n_un.n_strx);
			saves[relocs[i].r_symbolnum] = -1;
		    }
		    if(saves[relocs[i].r_symbolnum] != -1){
			relocs[i].r_symbolnum =
			    saves[relocs[i].r_symbolnum] - 1;
		    }
		}
		else{
		    fatal_arch(arch, member, "bad external relocation entry "
			"%ld (not external) in: ", i);
		}
		if((relocs[i].r_address & R_SCATTERED) == 0){
		    if(reloc_has_pair(object->mh->cputype, relocs[i].r_type))
			i++;
		}
		else{
		    sreloc = (struct scattered_relocation_info *)relocs + i;
		    if(reloc_has_pair(object->mh->cputype, sreloc->r_type))
			i++;
		}
	    }
	    if(object->object_byte_sex != host_byte_sex)
		swap_relocation_info(relocs, object->dyst->nextrel,
				     object->object_byte_sex);
	}

	/*
	 * Check and update the indirect symbol table entries to make sure
	 * referenced symbols are not stripped and refer to the new symbol
	 * table indexes.
	 */
	if(object->dyst != NULL && object->dyst->nindirectsyms != 0){
	    if(object->object_byte_sex != host_byte_sex)
		swap_indirect_symbols(object->output_indirect_symtab,
		    object->dyst->nindirectsyms, host_byte_sex);

	    lc = object->load_commands;
	    for(i = 0; i < object->mh->ncmds; i++){
		if(lc->cmd == LC_SEGMENT &&
		   object->seg_linkedit != (struct segment_command *)lc){
		    sg = (struct segment_command *)lc;
		    s = (struct section *)((char *)sg +
					    sizeof(struct segment_command));
		    for(j = 0; j < sg->nsects; j++){
			section_type = s->flags & SECTION_TYPE;
			if(section_type == S_LAZY_SYMBOL_POINTERS ||
			   section_type == S_NON_LAZY_SYMBOL_POINTERS)
			    stride = 4;
			else if(section_type == S_SYMBOL_STUBS)
			    stride = s->reserved2;
			else{
			    s++;
			    continue;
			}
			nitems = s->size / stride;
			for(k = 0; k < nitems; k++){
			    index = object->output_indirect_symtab[
					s->reserved1 + k];
			    if(index == INDIRECT_SYMBOL_LOCAL ||
			       index == (INDIRECT_SYMBOL_LOCAL |
				         INDIRECT_SYMBOL_ABS))
				continue;
			    if(index > nsyms)
				fatal_arch(arch, member,"indirect symbol table "
				    "entry %ld (past the end of the symbol "
				    "table) in: ", s->reserved1 + k);
			    if(saves[index] == 0){
				/*
				 * Indirect symbol table entries for defined
				 * symbols in a non-lazy pointer section that
				 * are not saved are changed to
				 * INDIRECT_SYMBOL_LOCAL which their values
				 * just have to be slid if the are not absolute
				 * symbols.
				 */
				if((symbols[index].n_type && N_TYPE) !=
				    N_UNDF &&
				   (symbols[index].n_type && N_TYPE) !=
				    N_PBUD &&
				   section_type == S_NON_LAZY_SYMBOL_POINTERS){
				    object->output_indirect_symtab[
					s->reserved1 + k] =
					    INDIRECT_SYMBOL_LOCAL;
				    if((symbols[index].n_type &N_TYPE) == N_ABS)
					object->output_indirect_symtab[
					    s->reserved1 + k] |=
						INDIRECT_SYMBOL_ABS;
				}
				else{
				    if(missing_reloc_symbols == 0){
					error_arch(arch, member, "symbols "
					    "referenced by indirect symbol "
					    "table entries that can't be "
					    "stripped in: ");
					missing_reloc_symbols = 1;
				    }
				    fprintf(stderr, "%s\n", strings +
					    symbols[index].n_un.n_strx);
				}
				saves[index] = -1;
			    }
			    if(saves[index] != -1){
				object->output_indirect_symtab[s->reserved1+k] =
				    saves[index] - 1;
			    }
			}
			s++;
		    }
		}
		lc = (struct load_command *)((char *)lc + lc->cmdsize);
	    }

	    if(object->object_byte_sex != host_byte_sex)
		swap_indirect_symbols(object->output_indirect_symtab,
		    object->dyst->nindirectsyms, object->object_byte_sex);
	}
#endif /* NMEDIT */
}

/*
 * This is called to setup a symbol list from a file.  It reads the file with
 * the strings in it and places them in an array of symbol_list structures and
 * then sorts them by name.
 *
 * The file that contains the symbol names must have symbol names one per line,
 * leading and trailing white space is removed and lines starting with a '#'
 * and lines with only white space are ignored.
 */
static
void
setup_symbol_list(
char *file,
struct symbol_list **list,
unsigned long *size)
{
    int fd, i, j, len, strings_size;
    struct stat stat_buf;
    char *strings, *p, *line;

	if((fd = open(file, O_RDONLY)) < 0){
	    system_error("can't open: %s", file);
	    return;
	}
	if(fstat(fd, &stat_buf) == -1){
	    system_error("can't stat: %s", file);
	    close(fd);
	    return;
	}
	strings_size = stat_buf.st_size;
	strings = (char *)allocate(strings_size + 1);
	strings[strings_size] = '\0';
	if(read(fd, strings, strings_size) != strings_size){
	    system_error("can't read: %s", file);
	    close(fd);
	    return;
	}
	/*
	 * Change the newlines to '\0' and count the number of lines with
	 * symbol names.  Lines starting with '#' are comments and lines
	 * contain all space characters do not contain symbol names.
	 */
	p = strings;
	line = p;
	for(i = 0; i < strings_size; i++){
	    if(*p == '\n'){
		*p = '\0';
		if(*line != '#'){
		    while(*line != '\0' && isspace(*line))
			line++;
		    if(*line != '\0')
			(*size)++;
		}
		p++;
		line = p;
	    }
	    else{
		p++;
	    }
	}
	*list = (struct symbol_list *)
		allocate((*size) * sizeof(struct symbol_list));

	/*
	 * Place the strings in the list trimming leading and trailing spaces
	 * from the lines with symbol names.
	 */
	p = strings;
	line = p;
	for(i = 0; i < (*size); ){
	    p += strlen(p) + 1;
	    if(*line != '#' && *line != '\0'){
		while(*line != '\0' && isspace(*line))
		    line++;
		if(*line != '\0'){
		    (*list)[i].name = line;
		    (*list)[i].seen = FALSE;
		    i++;
		    len = strlen(line);
		    j = len - 1;
		    while(j > 0 && isspace(line[j])){
			j--;
		    }
		    if(j > 0 && j + 1 < len && isspace(line[j+1]))
			line[j+1] = '\0';
		}
	    }
	    line = p;
	}

	qsort(*list, *size, sizeof(struct symbol_list),
	      (int (*)(const void *, const void *))cmp_qsort_name);

#ifdef DEBUG
	printf("symbol list:\n");
	for(i = 0; i < (*size); i++){
	    printf("0x%x name = %s\n", &((*list)[i]),(*list)[i].name);
	}
#endif DEBUG
}

#ifndef NMEDIT
/*
 * This is called if there is a -d option specified.  It reads the file with
 * the strings in it and places them in the array debug_filenames and sorts
 * them by name.  The file that contains the file names must have names one
 * per line with no white space (except the newlines).
 */
static
void
setup_debug_filenames(
char *dfile)
{
    int fd, i, strings_size;
    struct stat stat_buf;
    char *strings, *p;

	if((fd = open(dfile, O_RDONLY)) < 0){
	    system_error("can't open: %s", dfile);
	    return;
	}
	if(fstat(fd, &stat_buf) == -1){
	    system_error("can't stat: %s", dfile);
	    close(fd);
	    return;
	}
	strings_size = stat_buf.st_size;
	strings = (char *)allocate(strings_size + 1);
	strings[strings_size] = '\0';
	if(read(fd, strings, strings_size) != strings_size){
	    system_error("can't read: %s", dfile);
	    close(fd);
	    return;
	}
	p = strings;
	for(i = 0; i < strings_size; i++){
	    if(*p == '\n'){
		*p = '\0';
		ndebug_filenames++;
	    }
	    p++;
	}
	debug_filenames = (char **)allocate(ndebug_filenames * sizeof(char *));
	p = strings;
	for(i = 0; i < ndebug_filenames; i++){
	    debug_filenames[i] = p;
	    p += strlen(p) + 1;
	}
	qsort(debug_filenames, ndebug_filenames, sizeof(char *),
	      (int (*)(const void *, const void *))cmp_qsort_filename);

#ifdef DEBUG
	printf("Debug filenames:\n");
	for(i = 0; i < ndebug_filenames; i++){
	    printf("filename = %s\n", debug_filenames[i]);
	}
#endif DEBUG
}

/*
 * Strip the symbol table to the level specified by the command line arguments.
 * The new symbol table is built and new_symbols is left pointing to it.  The
 * number of new symbols is left in new_nsyms, the new string table is built
 * and new_stings is left pointing to it and new_strsize is left containing it.
 * This routine returns zero if successfull and non-zero otherwise.
 */
static
enum bool
strip_symtab(
struct arch *arch,
struct member *member,
struct nlist *symbols,
long nsyms,
char *strings,
long strsize,
struct dylib_table_of_contents *tocs,
unsigned long ntoc,
struct dylib_module *mods,
unsigned long nmodtab,
struct dylib_reference *refs,
unsigned long nextrefsyms)
{
    long i, j, k, n, inew_syms, save_debug, missing_syms, missing_symbols;
    char *p, *q, **pp, *basename;
    struct symbol_list *sp;
    unsigned long new_ext_strsize, len, *changes, inew_undefsyms;

	save_debug = 0;
	if(saves != NULL)
	    free(saves);
	saves = (long *)allocate(nsyms * sizeof(long));
	bzero(saves, nsyms * sizeof(long));
	changes = NULL;
	for(i = 0; i < nsave_symbols; i++)
	    save_symbols[i].sym = NULL;
	for(i = 0; i < nremove_symbols; i++)
	    remove_symbols[i].sym = NULL;
	if(member == NULL){
	    for(i = 0; i < nsave_symbols; i++)
		save_symbols[i].seen = FALSE;
	    for(i = 0; i < nremove_symbols; i++)
		remove_symbols[i].seen = FALSE;
	}

	new_nsyms = 0;
	new_strsize = sizeof(long);
	new_nlocalsym = 0;
	new_nextdefsym = 0;
	new_nundefsym = 0;
	new_ext_strsize = 0;

	for(i = 0; i < nsyms; i++){
	    if(symbols[i].n_un.n_strx != 0){
		if(symbols[i].n_un.n_strx < 0 ||
		   symbols[i].n_un.n_strx > strsize){
		    error_arch(arch, member, "bad string index for symbol "
			       "table entry %ld in: ", i);
		    return(FALSE);
		}
	    }
	    if((symbols[i].n_type & N_TYPE) == N_INDR){
		if(symbols[i].n_value != 0){
		    if(symbols[i].n_value > strsize){
			error_arch(arch, member, "bad string index for "
				   "indirect symbol table entry %ld in: ", i);
			return(FALSE);
		    }
		}
	    }
	    if((symbols[i].n_type & N_EXT) == 0){ /* local symbol */
		/*
		 * The cases a local symbol might be saved is with -X -S or
		 * with -d filename.
		 */
		if((!strip_all && (Xflag || Sflag)) || dfile){
		    if(symbols[i].n_type & N_STAB){ /* debug symbol */
			if(dfile && symbols[i].n_type == N_SO){
			    if(symbols[i].n_un.n_strx != 0){
				basename = strrchr(strings +
						   symbols[i].n_un.n_strx, '/');
				if(basename != NULL)
				    basename++;
				else
				    basename = strings + symbols[i].n_un.n_strx;
				pp = bsearch(basename, debug_filenames,
					    ndebug_filenames, sizeof(char *),
			 		    (int (*)(const void *, const void *)
					    )cmp_bsearch_filename);
				/*
				 * Save the bracketing N_SO. For each N_SO that
				 * has a filename there is an N_SO that has a
				 * name of "" which ends the stabs for that file
				 */
				if(*basename != '\0'){
				    if(pp != NULL)
					save_debug = 1;
				    else
					save_debug = 0;
				}
				else{
				    /*
				     * This is a bracketing SO so if we are
				     * currently saving debug symbols save this
				     * last one and turn off saving debug syms.
				     */
				    if(save_debug){
					if(symbols[i].n_un.n_strx != 0)
					    new_strsize += strlen(strings +
						  symbols[i].n_un.n_strx) + 1;
					new_nlocalsym++;
					new_nsyms++;
					saves[i] = new_nsyms;
				    }
				    save_debug = 0;
				}
			    }
			    else{
				save_debug = 0;
			    }
			}
			if(saves[i] == 0 && (!Sflag || save_debug)){
			    if(symbols[i].n_un.n_strx != 0)
				new_strsize += strlen(strings +
						  symbols[i].n_un.n_strx) + 1;
			    new_nlocalsym++;
			    new_nsyms++;
			    saves[i] = new_nsyms;
			}
		    }
		    else{ /* non-debug local symbol */
			if(xflag == 0 && (Sflag || Xflag)){
			    if(Xflag == 0 ||
			       (symbols[i].n_un.n_strx != 0 &&
		                strings[symbols[i].n_un.n_strx] != 'L')){
				new_strsize += strlen(strings +
						  symbols[i].n_un.n_strx) + 1;
				new_nlocalsym++;
				new_nsyms++;
				saves[i] = new_nsyms;
			    }
			}
			/*
			 * Treat a local symbol that was a private extern as if
			 * were global.
			 */
			if((symbols[i].n_type & N_PEXT) == N_PEXT){
			    if(saves[i] == 0 && xflag){
				if(symbols[i].n_un.n_strx != 0)
				    new_strsize += strlen(strings +
						  symbols[i].n_un.n_strx) + 1;
				new_nlocalsym++;
				new_nsyms++;
				saves[i] = new_nsyms;
			    }
			}
		    }
		}
		/*
		 * Treat a local symbol that was a private extern as if were
		 * global.
		 */
		else if((symbols[i].n_type & N_PEXT) == N_PEXT){
		    if(saves[i] == 0 && xflag){
			if(symbols[i].n_un.n_strx != 0)
			    new_strsize += strlen(strings +
					      symbols[i].n_un.n_strx) + 1;
			new_nlocalsym++;
			new_nsyms++;
			saves[i] = new_nsyms;
		    }
		    if(saves[i] == 0 && sfile){
			sp = bsearch(strings + symbols[i].n_un.n_strx,
				     save_symbols, nsave_symbols,
				     sizeof(struct symbol_list),
				     (int (*)(const void *, const void *))
					cmp_bsearch);
			if(sp != NULL){
			    if(sp->sym == NULL){
				sp->sym = &(symbols[i]);
				sp->seen = TRUE;
			    }
			    if(symbols[i].n_un.n_strx != 0)
				new_strsize += strlen(strings +
						  symbols[i].n_un.n_strx) + 1;
			    new_nlocalsym++;
			    new_nsyms++;
			    saves[i] = new_nsyms;
			}
		    }
		}
	    }
	    else{ /* global symbol */
		if(Rfile){
		    sp = bsearch(strings + symbols[i].n_un.n_strx,
				 remove_symbols, nremove_symbols,
				 sizeof(struct symbol_list),
				 (int (*)(const void *, const void *))
				    cmp_bsearch);
		    if(sp != NULL){
			if((symbols[i].n_type & N_TYPE) == N_UNDF ||
			   (symbols[i].n_type & N_TYPE) == N_PBUD){
			    error_arch(arch, member, "symbol: %s undefined"
				       " and can't be stripped from: ",
				       sp->name);
			}
			else if(sp->sym != NULL &&
			   (sp->sym->n_type & N_PEXT) != N_PEXT){
			    error_arch(arch, member, "more than one symbol "
				       "for: %s found in: ", sp->name);
			}
			else{
			    sp->sym = &(symbols[i]);
			    sp->seen = TRUE;
			}
			if(symbols[i].n_desc & REFERENCED_DYNAMICALLY){
			    error_arch(arch, member, "symbol: %s is dynamically"
				       " referenced and can't be stripped "
				       "from: ", sp->name);
			}
			/* don't save this symbol */
			continue;
		    }
		}
		if(Aflag && (symbols[i].n_type & N_TYPE) == N_ABS &&
		   (symbols[i].n_value != 0 ||
		   (symbols[i].n_un.n_strx != 0 &&
		    strncmp(strings + symbols[i].n_un.n_strx,
			    ".objc_class_name_",
			    sizeof(".objc_class_name_") - 1) == 0))){
		    len = strlen(strings + symbols[i].n_un.n_strx) + 1;
		    new_strsize += len;
		    new_ext_strsize += len;
		    new_nextdefsym++;
		    new_nsyms++;
		    saves[i] = new_nsyms;
		}
		if(saves[i] == 0 && (uflag || default_dyld_executable) &&
		   ((((symbols[i].n_type & N_TYPE) == N_UNDF) &&
		     symbols[i].n_value == 0) ||
		    (symbols[i].n_type & N_TYPE) == N_PBUD)){
		    if(symbols[i].n_un.n_strx != 0){
			len = strlen(strings + symbols[i].n_un.n_strx) + 1;
			new_strsize += len;
			new_ext_strsize += len;
		    }
		    new_nundefsym++;
		    new_nsyms++;
		    saves[i] = new_nsyms;
		}
		if(saves[i] == 0 && nflag &&
		   (symbols[i].n_type & N_TYPE) == N_SECT){
		    if(symbols[i].n_un.n_strx != 0){
			len = strlen(strings + symbols[i].n_un.n_strx) + 1;
			new_strsize += len;
			new_ext_strsize += len;
		    }
		    new_nextdefsym++;
		    new_nsyms++;
		    saves[i] = new_nsyms;
		}
		if(saves[i] == 0 && sfile){
		    sp = bsearch(strings + symbols[i].n_un.n_strx,
				 save_symbols, nsave_symbols,
				 sizeof(struct symbol_list),
				 (int (*)(const void *, const void *))
				    cmp_bsearch);
		    if(sp != NULL){
			if(sp->sym != NULL &&
			   (sp->sym->n_type & N_PEXT) != N_PEXT){
			    error_arch(arch, member, "more than one symbol "
				       "for: %s found in: ", sp->name);
			}
			else{
			    sp->sym = &(symbols[i]);
			    sp->seen = TRUE;
			    len = strlen(strings + symbols[i].n_un.n_strx) + 1;
			    new_strsize += len;
			    new_ext_strsize += len;
			    if((symbols[i].n_type & N_TYPE) == N_UNDF ||
			       (symbols[i].n_type & N_TYPE) == N_PBUD)
				new_nundefsym++;
			    else
				new_nextdefsym++;
			    new_nsyms++;
			    saves[i] = new_nsyms;
			}
		    }
		}
		if(saves[i] == 0 && ((Xflag || Sflag || xflag) ||
		   ((rflag || default_dyld_executable) &&
		    symbols[i].n_desc & REFERENCED_DYNAMICALLY))){
		    len = strlen(strings + symbols[i].n_un.n_strx) + 1;
		    new_strsize += len;
		    new_ext_strsize += len;
		    if((symbols[i].n_type & N_TYPE) == N_INDR){
			len = strlen(strings + symbols[i].n_value) + 1;
			new_strsize += len;
			new_ext_strsize += len;
		    }
		    if((symbols[i].n_type & N_TYPE) == N_UNDF ||
		       (symbols[i].n_type & N_TYPE) == N_PBUD)
			new_nundefsym++;
		    else
			new_nextdefsym++;
		    new_nsyms++;
		    saves[i] = new_nsyms;
		}
	    }
	}
	/*
	 * The module table's module names are placed with the external strings.
	 * So size them and add this to the external string size.
	 */
	for(i = 0; i < nmodtab; i++){
	    if(mods[i].module_name == 0 ||
	       mods[i].module_name > strsize){
		error_arch(arch, member, "bad string index for module_name "
			   "of module table entry %ld in: ", i);
		return(FALSE);
	    }
	    len = strlen(strings + mods[i].module_name) + 1;
	    new_strsize += len;
	    new_ext_strsize += len;
	}

	/*
	 * Updating the reference table may require a symbol not yet listed as
	 * as saved to be present in the output file.  If a defined external
	 * symbol is removed and there is a undefined reference to it in the
	 * reference table an undefined symbol needs to be created for it in
	 * the output file.  If this happens the number of new symbols and size
	 * of the new strings are adjusted.  And the array changes[] is set to
	 * map the old symbol index to the new symbol index for the symbol that
	 * is changed to an undefined symbol.
	 */
	missing_symbols = 0;
	if(ref_saves != NULL)
	    free(ref_saves);
	ref_saves = (long *)allocate(nextrefsyms * sizeof(long));
	bzero(ref_saves, nextrefsyms * sizeof(long));
	changes = (unsigned long *)allocate(nsyms * sizeof(long));
	bzero(changes, nsyms * sizeof(long));
	new_nextrefsyms = 0;
	for(i = 0; i < nextrefsyms; i++){
	    if(refs[i].isym > nsyms){
		error_arch(arch, member, "bad symbol table index for "
			   "reference table entry %ld in: ", i);
		return(FALSE);
	    }
	    if(saves[refs[i].isym]){
		new_nextrefsyms++;
		ref_saves[i] = new_nextrefsyms;
	    }
	    else{
		if(refs[i].flags == REFERENCE_FLAG_UNDEFINED_NON_LAZY ||
		   refs[i].flags == REFERENCE_FLAG_UNDEFINED_LAZY){
		    if(changes[refs[i].isym] == 0){
			len = strlen(strings +
				     symbols[refs[i].isym].n_un.n_strx) + 1;
			new_strsize += len;
			new_ext_strsize += len;
			new_nundefsym++;
			new_nsyms++;
			changes[refs[i].isym] = new_nsyms;
			new_nextrefsyms++;
			ref_saves[i] = new_nextrefsyms;
		    }
		}
		else{
		    if(refs[i].flags ==
				    REFERENCE_FLAG_PRIVATE_UNDEFINED_NON_LAZY ||
		       refs[i].flags == REFERENCE_FLAG_PRIVATE_UNDEFINED_LAZY){
			if(missing_symbols == 0){
			    error_arch(arch, member, "private extern symbols "
			      "referenced by modules can't be stripped in: ");
			    missing_symbols = 1;
			}
			fprintf(stderr, "%s\n", strings + symbols
			      [refs[i].isym].n_un.n_strx);
			saves[refs[i].isym] = -1;
		    }
		}
	    }
	}
	if(missing_symbols == 1)
	    return(FALSE);

	if(member == NULL){
	    missing_syms = 0;
	    if(iflag == 0){
		for(i = 0; i < nsave_symbols; i++){
		    if(save_symbols[i].sym == NULL){
			if(missing_syms == 0){
			    error_arch(arch, member, "symbols names listed "
				       "in: %s not in: ", sfile);
			    missing_syms = 1;
			}
			fprintf(stderr, "%s\n", save_symbols[i].name);
		    }
		}
	    }
	    missing_syms = 0;
	    if(iflag == 0){
		for(i = 0; i < nremove_symbols; i++){
		    if(remove_symbols[i].sym == NULL){
			if(missing_syms == 0){
			    error_arch(arch, member, "symbols names listed "
				       "in: %s not in: ", Rfile);
			    missing_syms = 1;
			}
			fprintf(stderr, "%s\n", remove_symbols[i].name);
		    }
		}
	    }
	}

	new_symbols =(struct nlist *)allocate(new_nsyms * sizeof(struct nlist));
	new_strsize = round(new_strsize, sizeof(long));
	new_strings = (char *)allocate(new_strsize);
	new_strings[new_strsize - 3] = '\0';
	new_strings[new_strsize - 2] = '\0';
	new_strings[new_strsize - 1] = '\0';

	memset(new_strings, '\0', sizeof(long));
	p = new_strings + sizeof(long);
	q = p + new_ext_strsize;

	/* if all strings were stripped set the size to zero */
	if(new_strsize == sizeof(long))
	    new_strsize = 0;

	/*
	 * Now create a symbol table and string table in this order
	 * symbol table
	 *	local symbols
	 *	external defined symbols
	 *	undefined symbols
	 * string table
	 *	external strings
	 *	local strings
	 */
	inew_syms = 0;
	for(i = 0; i < nsyms; i++){
	    if(saves[i]){
		if((symbols[i].n_type & N_EXT) == 0){
		    new_symbols[inew_syms] = symbols[i];
		    if(symbols[i].n_un.n_strx != 0){
			strcpy(q, strings + symbols[i].n_un.n_strx);
			new_symbols[inew_syms].n_un.n_strx = q - new_strings;
			q += strlen(q) + 1;
		    }
		    inew_syms++;
		    saves[i] = inew_syms;
		}
	    }
	}
	for(i = 0; i < nsyms; i++){
	    if(saves[i]){
		if((symbols[i].n_type & N_EXT) == N_EXT &&
		   ((symbols[i].n_type & N_TYPE) != N_UNDF &&
		    (symbols[i].n_type & N_TYPE) != N_PBUD)){
		    new_symbols[inew_syms] = symbols[i];
		    if(symbols[i].n_un.n_strx != 0){
			strcpy(p, strings + symbols[i].n_un.n_strx);
			new_symbols[inew_syms].n_un.n_strx = p - new_strings;
			p += strlen(p) + 1;
		    }
		    if((symbols[i].n_type & N_TYPE) == N_INDR){
			if(symbols[i].n_value != 0){
			    strcpy(p, strings + symbols[i].n_value);
			    new_symbols[inew_syms].n_value = p - new_strings;
			    p += strlen(p) + 1;
			}
		    }
		    inew_syms++;
		    saves[i] = inew_syms;
		}
	    }
	}
	/*
	 * Build the new undefined symbols into a map and sort it.
	 */
	inew_undefsyms = 0;
	undef_map = (struct undef_map *)allocate(new_nundefsym *
						 sizeof(struct undef_map));
	for(i = 0; i < nsyms; i++){
	    if(saves[i]){
		if((symbols[i].n_type & N_EXT) == N_EXT &&
		   ((symbols[i].n_type & N_TYPE) == N_UNDF ||
		    (symbols[i].n_type & N_TYPE) == N_PBUD)){
		    undef_map[inew_undefsyms].symbol = symbols[i];
		    if(symbols[i].n_un.n_strx != 0){
			strcpy(p, strings + symbols[i].n_un.n_strx);
			undef_map[inew_undefsyms].symbol.n_un.n_strx =
			    p - new_strings;
			p += strlen(p) + 1;
		    }
		    undef_map[inew_undefsyms].index = i;
		    inew_undefsyms++;
		}
	    }
	}
	for(i = 0; i < nsyms; i++){
	    if(changes[i]){
		if(symbols[i].n_un.n_strx != 0){
		    strcpy(p, strings + symbols[i].n_un.n_strx);
		     undef_map[inew_undefsyms].symbol.n_un.n_strx =
			p - new_strings;
		    p += strlen(p) + 1;
		}
		undef_map[inew_undefsyms].symbol.n_type = N_UNDF | N_EXT;
		undef_map[inew_undefsyms].symbol.n_sect = NO_SECT;
		undef_map[inew_undefsyms].symbol.n_desc = 0;
		undef_map[inew_undefsyms].symbol.n_value = 0;
		undef_map[inew_undefsyms].index = i;
		inew_undefsyms++;
	    }
	}
	/* Sort the undefined symbols by name */
	qsort_strings = new_strings;
	qsort(undef_map, new_nundefsym, sizeof(struct undef_map),
	      (int (*)(const void *, const void *))cmp_qsort_undef_map);
	/* Copy the symbols now in sorted order into new_symbols */
	for(i = 0; i < new_nundefsym; i++){
	    new_symbols[inew_syms] = undef_map[i].symbol;
	    inew_syms++;
	    saves[undef_map[i].index] = inew_syms;
	}

	/*
	 * Fixup the module table's module name strings adding them to the
	 * string table.  Also fix the indexes into the symbol table for
	 * external and local symbols.  And fix up the indexes into the
	 * reference table.
	 */
	for(i = 0; i < nmodtab; i++){
	    strcpy(p, strings + mods[i].module_name);
	    mods[i].module_name = p - new_strings;
	    p += strlen(p) + 1;

	    if(mods[i].iextdefsym > nsyms){
		error_arch(arch, member, "bad index into externally defined "
		    "symbols of module table entry %ld in: ", i);
		return(FALSE);
	    }
	    if(mods[i].iextdefsym + mods[i].nextdefsym > nsyms){
		error_arch(arch, member, "bad number of externally defined "
		    "symbols of module table entry %ld in: ", i);
		return(FALSE);
	    }
	    for(j = mods[i].iextdefsym;
		j < mods[i].iextdefsym + mods[i].nextdefsym;
		j++){
		if(saves[j] != 0 && changes[j] == 0)
		    break;
	    }
	    n = 0;
	    for(k = j;
		k < mods[i].iextdefsym + mods[i].nextdefsym;
		k++){
		if(saves[k] != 0 && changes[k] == 0)
		    n++;
	    }
	    if(n == 0){
		mods[i].iextdefsym = 0;
		mods[i].nextdefsym = 0;
	    }
	    else{
		mods[i].iextdefsym = saves[j] - 1;
		mods[i].nextdefsym = n;
	    }

	    if(mods[i].ilocalsym > nsyms){
		error_arch(arch, member, "bad index into symbols for local "
		    "symbols of module table entry %ld in: ", i);
		return(FALSE);
	    }
	    if(mods[i].ilocalsym + mods[i].nlocalsym > nsyms){
		error_arch(arch, member, "bad number of local "
		    "symbols of module table entry %ld in: ", i);
		return(FALSE);
	    }
	    for(j = mods[i].ilocalsym;
		j < mods[i].ilocalsym + mods[i].nlocalsym;
		j++){
		if(saves[j] != 0)
		    break;
	    }
	    n = 0;
	    for(k = j;
		k < mods[i].ilocalsym + mods[i].nlocalsym;
		k++){
		if(saves[k] != 0)
		    n++;
	    }
	    if(n == 0){
		mods[i].ilocalsym = 0;
		mods[i].nlocalsym = 0;
	    }
	    else{
		mods[i].ilocalsym = saves[j] - 1;
		mods[i].nlocalsym = n;
	    }

	    if(mods[i].irefsym > nextrefsyms){
		error_arch(arch, member, "bad index into reference table "
		    "of module table entry %ld in: ", i);
		return(FALSE);
	    }
	    if(mods[i].irefsym + mods[i].nrefsym > nextrefsyms){
		error_arch(arch, member, "bad number of reference table "
		    "entries of module table entry %ld in: ", i);
		return(FALSE);
	    }
	    for(j = mods[i].irefsym;
		j < mods[i].irefsym + mods[i].nrefsym;
		j++){
		if(ref_saves[j] != 0)
		    break;
	    }
	    n = 0;
	    for(k = j;
		k < mods[i].irefsym + mods[i].nrefsym;
		k++){
		if(ref_saves[k] != 0)
		    n++;
	    }
	    if(n == 0){
		mods[i].irefsym = 0;
		mods[i].nrefsym = 0;
	    }
	    else{
		mods[i].irefsym = ref_saves[j] - 1;
		mods[i].nrefsym = n;
	    }
	}

	/*
	 * Create a new reference table.
	 */
	new_refs = allocate(new_nextrefsyms * sizeof(struct dylib_reference));
	j = 0;
	for(i = 0; i < nextrefsyms; i++){
	    if(ref_saves[i]){
		if(saves[refs[i].isym]){
		    new_refs[j].isym = saves[refs[i].isym] - 1;
		    new_refs[j].flags = refs[i].flags;
		}
		else{
		    if(refs[i].flags == REFERENCE_FLAG_UNDEFINED_NON_LAZY ||
		       refs[i].flags == REFERENCE_FLAG_UNDEFINED_LAZY){
			new_refs[j].isym = changes[refs[i].isym] - 1;
			new_refs[j].flags = refs[i].flags;
		    }
		}
		j++;
	    }
	}

	/*
	 * Create a new dylib table of contents.
	 */
	new_ntoc = 0;
	for(i = 0; i < ntoc; i++){
	    if(tocs[i].symbol_index >= nsyms){
		error_arch(arch, member, "bad symbol index for table of "
		    "contents table entry %ld in: ", i);
		return(FALSE);
	    }
	    if(saves[tocs[i].symbol_index] != 0 &&
	       changes[tocs[i].symbol_index] == 0)
		new_ntoc++;
	}
	new_tocs = allocate(new_ntoc * sizeof(struct dylib_table_of_contents));
	j = 0;
	for(i = 0; i < ntoc; i++){
	    if(saves[tocs[i].symbol_index] != 0 &&
	       changes[tocs[i].symbol_index] == 0){
		new_tocs[j].symbol_index = saves[tocs[i].symbol_index] - 1;
		new_tocs[j].module_index = tocs[i].module_index;
		j++;
	    }
	}

	if(changes != NULL)
	    free(changes);

	return(TRUE);
}

/*
 * Function for qsort for comparing undefined map entries.
 */
static
int
cmp_qsort_undef_map(
const struct undef_map *sym1,
const struct undef_map *sym2)
{
	return(strcmp(qsort_strings + sym1->symbol.n_un.n_strx,
		      qsort_strings + sym2->symbol.n_un.n_strx));
}
#endif /* !defined(NMEDIT) */

/*
 * Function for qsort for comparing symbol list names.
 */
static
int
cmp_qsort_name(
const struct symbol_list *sym1,
const struct symbol_list *sym2)
{
	return(strcmp(sym1->name, sym2->name));
}

/*
 * Function for bsearch for finding a symbol.
 */
static
int
cmp_bsearch(
const char *name,
const struct symbol_list *sym)
{
	return(strcmp(name, sym->name));
}

#ifndef NMEDIT
/*
 * Function for qsort for comparing object names.
 */
static
int
cmp_qsort_filename(
const char **name1,
const char **name2)
{
	return(strcmp(*name1, *name2));
}

/*
 * Function for bsearch for finding a object name.
 */
static
int
cmp_bsearch_filename(
const char *name1,
const char **name2)
{
	return(strcmp(name1, *name2));
}
#endif /* !defined(NMEDIT) */

#ifdef NMEDIT
static
enum bool
edit_symtab(
struct arch *arch,
struct member *member,
struct object *object,
struct nlist *symbols,
long nsyms,
char *strings,
long strsize)
{
    unsigned long i, j, k;
    unsigned char data_n_sect, n_sect;
    struct load_command *lc;
    struct segment_command *sg;
    struct section *s;

    unsigned long missing_syms, nchanged_globals;
    struct symbol_list *sp;
    struct nlist **changed_globals, **global_symbol;
    char *global_name, save_char;
    enum byte_sex host_byte_sex;
    long missing_reloc_symbols;
    unsigned long stride, section_type, nitems, index;

	host_byte_sex = get_host_byte_sex();
	missing_reloc_symbols = 0;

	if(object->mh->filetype == MH_DYLIB)
	    fatal_arch(arch, member, "can't operate on MH_DYLIB file: ");

	j = 0;
	n_sect = 1;
	data_n_sect = NO_SECT;
	lc = object->load_commands;
	for(i = 0; data_n_sect == NO_SECT && i < object->mh->ncmds; i++){
	    if(lc->cmd == LC_SEGMENT){
		sg = (struct segment_command *)lc;
		s = (struct section *)((char *)sg +
					sizeof(struct segment_command));
		for(j = 0; j < sg->nsects; j++){
		    if(strcmp(s->segname, SEG_DATA) == 0 &&
		       strcmp(s->sectname, SECT_DATA) == 0){
			data_n_sect = n_sect;
			break;
		    }
		    n_sect++;
		    s++;
		}
	    }
	    lc = (struct load_command *)((char *)lc + lc->cmdsize);
	}

	/*
	 * Zero out the saved symbols so they can be recorded for this file.
	 */
	for(i = 0; i < nsave_symbols; i++)
	    save_symbols[i].sym = NULL;
	for(i = 0; i < nremove_symbols; i++)
	    remove_symbols[i].sym = NULL;
	if(member == NULL){
	    for(i = 0; i < nsave_symbols; i++)
		save_symbols[i].seen = FALSE;
	    for(i = 0; i < nremove_symbols; i++)
		remove_symbols[i].seen = FALSE;
	}

	changed_globals = allocate(nsyms * sizeof(struct nlist *));
	for(i = 0; i < nsyms; i++)
	    changed_globals[i] = NULL;
	nchanged_globals = 0;

	/*
	 * First pass: turn the globals symbols into non-global symbols.
	 */
	for(i = 0; i < nsyms; i++){
	    if(symbols[i].n_un.n_strx != 0){
		if(symbols[i].n_un.n_strx < 0 ||
		   symbols[i].n_un.n_strx > strsize){
		    error_arch(arch, member, "bad string index for symbol "
			       "table entry %lu in: ", i);
		    free(changed_globals);
		    return(FALSE);
		}
	    }
	    if(symbols[i].n_type & N_EXT){
		if((symbols[i].n_type & N_TYPE) != N_UNDF &&
		   (symbols[i].n_type & N_TYPE) != N_PBUD){
		    sp = bsearch(strings + symbols[i].n_un.n_strx,
				 remove_symbols, nremove_symbols,
				 sizeof(struct symbol_list),
				 (int (*)(const void *, const void *))
				    cmp_bsearch);
		    if(sp != NULL){
			if(sp->sym != NULL)
			    error_arch(arch, member, "more than one symbol "
				       "for: %s found in: ", sp->name);
			else{
			    sp->sym = &(symbols[i]);
			    sp->seen = TRUE;
			    goto make_static;
			}
		    }
		    else{
			/*
			 * If there is no list of saved symbols, then all
			 * symbols will be saved unless listed in the remove
			 * list.
			 */
			if(sfile == NULL){
			    /*
			     * There is no save list, so if there is also no
			     * remove list but the -p flag is specified then
			     * make all symbols private extern.
			     */
			    if(pflag && nremove_symbols == 0)
				goto make_static;
			    continue; /* leave this symbol unchanged */
			}
		    }
		    sp = bsearch(strings + symbols[i].n_un.n_strx,
				 save_symbols, nsave_symbols,
				 sizeof(struct symbol_list),
				 (int (*)(const void *, const void *))
				    cmp_bsearch);
		    if(sp != NULL){
			if(sp->sym != NULL)
			    error_arch(arch, member, "more than one symbol "
				       "for: %s found in: ", sp->name);
			else{
			    sp->sym = &(symbols[i]);
			    sp->seen = TRUE;
			}
		    }
		    else{
			if(Aflag && symbols[i].n_type == (N_EXT | N_ABS) &&
		            (symbols[i].n_value != 0 ||
		            (symbols[i].n_un.n_strx != 0 &&
			     strncmp(strings + symbols[i].n_un.n_strx,
				".objc_class_name_",
				sizeof(".objc_class_name_") - 1) == 0))){
			    /* do nothing */
			}
			else{
make_static:
			    if((symbols[i].n_type & N_TYPE) != N_INDR){
				if(pflag)
				    symbols[i].n_type |= N_PEXT;
				else
				    symbols[i].n_type &= ~N_EXT;
				changed_globals[nchanged_globals++] =
				    symbols + i;
			    }
			}
		    }
		}
	    }
	}

	/*
	 * Warn about symbols to be saved that were missing.
	 */
	if(member == NULL){
	    missing_syms = 0;
	    if(iflag == 0){
		for(i = 0; i < nsave_symbols; i++){
		    if(save_symbols[i].sym == NULL){
			if(missing_syms == 0){
			    error_arch(arch, member, "symbols names listed "
				       "in: %s not in: ", sfile);
			    missing_syms = 1;
			}
			fprintf(stderr, "%s\n", save_symbols[i].name);
		    }
		}
		for(i = 0; i < nremove_symbols; i++){
		    if(remove_symbols[i].sym == NULL){
			if(missing_syms == 0){
			    error_arch(arch, member, "symbols names listed "
				       "in: %s not in: ", Rfile);
			    missing_syms = 1;
			}
			fprintf(stderr, "%s\n", remove_symbols[i].name);
		    }
		}
	    }
	}

	/*
	 * Second pass: fix stabs for the globals symbols that got turned into
	 * non-global symbols.  This is a MAJOR guess.  The specific changes
	 * to do here were determined by compiling test cases with and without
	 * the key word 'static' and looking at the difference between the STABS
	 * the compiler generates and trying to match that here.
	 */
	global_strings = strings;
	qsort(changed_globals, nchanged_globals, sizeof(struct nlist *),
	      (int (*)(const void *, const void *))cmp_qsort_global);
	for(i = 0; i < nsyms; i++){
	    if(symbols[i].n_type & N_STAB &&
	       (symbols[i].n_type == N_GSYM || symbols[i].n_type == N_FUN)){
		global_name = strings + symbols[i].n_un.n_strx;
		if((global_name[0] == '+' || global_name[0] == '-') &&
		   global_name[1] == '['){
		    j = 2;
		    while(j + symbols[i].n_un.n_strx < strsize &&
			  global_name[j] != ']')
			j++;
		    if(j + symbols[i].n_un.n_strx < strsize &&
		       global_name[j] == ']')
			j++;
		}
		else
		    j = 0;
		while(j + symbols[i].n_un.n_strx < strsize &&
		      global_name[j] != ':')
		    j++;
		if(j + symbols[i].n_un.n_strx >= strsize){
		    error_arch(arch, member, "bad N_STAB symbol name for entry "
			"%lu (does not contain ':' separating name from type) "
			"in: ", i);
		    free(changed_globals);
		    return(FALSE);
		}
		save_char = global_name[j];
		global_name[j] = '\0';

		global_symbol = bsearch(global_name, changed_globals,
					nchanged_globals,sizeof(struct nlist *),
			     		(int (*)(const void *, const void *))
					cmp_bsearch_global_stab);
		global_name[j] = save_char;
		if(global_symbol != NULL){
		    if(symbols[i].n_type == N_GSYM){
			if((*global_symbol)->n_sect == data_n_sect)
			    symbols[i].n_type = N_STSYM;
			else
			    symbols[i].n_type = N_FUN;
			symbols[i].n_sect = (*global_symbol)->n_sect;
			symbols[i].n_value = (*global_symbol)->n_value;
			symbols[i].n_desc = (*global_symbol)->n_desc;
			if(j + 1 + symbols[i].n_un.n_strx >= strsize ||
			   global_name[j+1] != 'G'){
			    error_arch(arch, member, "bad N_GSYM symbol name "
				"for entry %lu (does not have type 'G' after "
				"':' in name) in: ", i);
			    free(changed_globals);
			    return(FALSE);
			}
		        global_name[j+1] = 'S';
		    }
		    else{ /* symbols[i].n_type == N_FUN */
			if(j + 1 + symbols[i].n_un.n_strx >= strsize ||
			   global_name[j+1] == 'F'){
			    global_name[j+1] = 'f';
			}
		    }
		}
	    }
	}
	global_strings = NULL;

	/*
	 * Check and update the effected indirect symbol table entries to make
	 * sure indirect symbols for lazy pointer and stub sections are not
	 * changed.  Then for indirect symbols for non-lazy pointers that were
	 * changed to statics change the indirect symbol to
	 * INDIRECT_SYMBOL_LOCAL or INDIRECT_SYMBOL_ABS.
	 */
	if(pflag == 0 &&
	   object->dyst != NULL && object->dyst->nindirectsyms != 0){
	    if(object->object_byte_sex != host_byte_sex)
		swap_indirect_symbols(object->output_indirect_symtab,
		    object->dyst->nindirectsyms, host_byte_sex);

	    lc = object->load_commands;
	    for(i = 0; i < object->mh->ncmds; i++){
		if(lc->cmd == LC_SEGMENT &&
		   object->seg_linkedit != (struct segment_command *)lc){
		    sg = (struct segment_command *)lc;
		    s = (struct section *)((char *)sg +
					    sizeof(struct segment_command));
		    for(j = 0; j < sg->nsects; j++){
			section_type = s->flags & SECTION_TYPE;
			if(section_type == S_LAZY_SYMBOL_POINTERS ||
			   section_type == S_NON_LAZY_SYMBOL_POINTERS)
			    stride = 4;
			else if(section_type == S_SYMBOL_STUBS)
			    stride = s->reserved2;
			else{
			    s++;
			    continue;
			}
			nitems = s->size / stride;
			for(k = 0; k < nitems; k++){
			    index = object->output_indirect_symtab[
					s->reserved1 + k];
			    if(index == INDIRECT_SYMBOL_LOCAL ||
			       index == (INDIRECT_SYMBOL_LOCAL |
				         INDIRECT_SYMBOL_ABS))
				continue;
			    if(index > nsyms)
				fatal_arch(arch, member,"indirect symbol table "
				    "entry %ld (past the end of the symbol "
				    "table) in: ", s->reserved1 + k);

			    global_name = strings + symbols[index].n_un.n_strx;
			    global_strings = strings;
			    global_symbol = bsearch(global_name,changed_globals,
					nchanged_globals,sizeof(struct nlist *),
			     		(int (*)(const void *, const void *))
					cmp_bsearch_global);
			    if(global_symbol != NULL){
				/*
				 * Indirect symbol table entries for defined
				 * symbols in a non-lazy pointer section that
				 * were changed from global to static get the
				 * indirect symbol table entry changed to
			 	 * INDIRECT_SYMBOL_LOCAL or INDIRECT_SYMBOL_ABS.
				 */
				if((symbols[index].n_type && N_TYPE) !=
				    N_UNDF &&
				   (symbols[index].n_type && N_TYPE) !=
				    N_PBUD &&
				   section_type == S_NON_LAZY_SYMBOL_POINTERS){
				    object->output_indirect_symtab[
					s->reserved1 + k] =
					    INDIRECT_SYMBOL_LOCAL;
				    if((symbols[index].n_type &N_TYPE) == N_ABS)
					object->output_indirect_symtab[
					    s->reserved1 + k] |=
						INDIRECT_SYMBOL_ABS;
				}
				else{
				    if(missing_reloc_symbols == 0){
					error_arch(arch, member, "symbols "
					    "referenced by indirect symbol "
					    "table entries that can't be "
					    "changed to statics in: ");
					missing_reloc_symbols = 1;
				    }
				    fprintf(stderr, "%s\n", strings +
					    symbols[index].n_un.n_strx);
				}
			    }
			}
			s++;
		    }
		}
		lc = (struct load_command *)((char *)lc + lc->cmdsize);
	    }

	    if(object->object_byte_sex != host_byte_sex)
		swap_indirect_symbols(object->output_indirect_symtab,
		    object->dyst->nindirectsyms, object->object_byte_sex);
	}

	free(changed_globals);
	return(TRUE);
}

/*
 * Function for qsort for comparing global symbol names.
 */
static
int
cmp_qsort_global(
const struct nlist **sym1,
const struct nlist **sym2)
{
	return(strcmp(global_strings + (*sym1)->n_un.n_strx,
		      global_strings + (*sym2)->n_un.n_strx));
}

/*
 * Function for bsearch for finding a global symbol that matches a stab name.
 */
static
int
cmp_bsearch_global_stab(
const char *name,
const struct nlist **sym)
{
	/*
	 * The +1 is for the '_' on the global symbol that is not on the
	 * stab string that is trying to be matched.
	 */
	return(strcmp(name, global_strings + (*sym)->n_un.n_strx + 1));
}

/*
 * Function for bsearch for finding a global symbol name.
 */
static
int
cmp_bsearch_global(
const char *name,
const struct nlist **sym)
{
	return(strcmp(name, global_strings + (*sym)->n_un.n_strx));
}
#endif /* defined(NMEDIT) */
