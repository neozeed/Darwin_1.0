/* C Compatible Compiler Preprocessor (CCCP)
   Copyright (C) 1986, 87, 89, 92, 93, 1994 Free Software Foundation, Inc.
   Written by Paul Rubin, June 1986
   Adapted to ANSI C, Richard Stallman, Jan 1987

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2, or (at your option) any
later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.

 In other words, you are welcome to use, share and improve this program.
 You are forbidden to forbid anyone else to use, share and improve
 what you give them.   Help stamp out software-hoarding!  */

#if defined (_WIN32) && defined (NeXT_PDO)
#include "cccp-winnt.c"
#else

typedef unsigned char U_CHAR;

#ifdef EMACS
#define NO_SHORTNAMES
#include "../src/config.h"
#ifdef open
#undef open
#undef read
#undef write
#endif /* open */
#endif /* EMACS */

/* The macro EMACS is defined when cpp is distributed as part of Emacs,
   for the sake of machines with limited C compilers.  */
#ifndef EMACS
#include "config.h"
#endif /* not EMACS */

#if defined (__APPLE__) || defined (NeXT) || defined (NEXT_PDO)
#define MAX_FILENAME_LEN 256
#define NEXT_FRAMEWORKS
#ifndef _WIN32
#define NEXT_CPP_SERVER
#endif
#define NEXT_SEMANTICS
#define MALLOC_INPUT_BUFFERS
#endif

#ifndef STANDARD_INCLUDE_DIR
#define STANDARD_INCLUDE_DIR "/usr/include"
#endif

#ifndef LOCAL_INCLUDE_DIR
#define LOCAL_INCLUDE_DIR "/usr/local/include"
#endif

#if 0 /* We can't get ptrdiff_t, so I arranged not to need PTR_INT_TYPE.  */
#ifdef __STDC__
#define PTR_INT_TYPE ptrdiff_t
#else
#define PTR_INT_TYPE long
#endif
#endif /* 0 */

#include "pcp.h"

#ifndef STDC_VALUE
#define STDC_VALUE 1
#endif

/* By default, colon separates directories in a path.  */
#ifndef PATH_SEPARATOR
#define PATH_SEPARATOR ':'
#endif

/* In case config.h defines these.  */
#undef bcopy
#undef bzero
#undef bcmp

#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <stdio.h>
#include <signal.h>

#ifndef MALLOC_INPUT_BUFFERS
#include <mach/mach.h>
#endif

#if defined (NEXT_PDO) && defined (hpux)
#include <sys/times.h>
#endif

#ifdef REPORT_EVENT
#include <stdarg.h>
#include "next/make-support.h"
#endif

#ifndef VMS
#if !defined (USG) || (defined (NEXT_PDO) && defined (SVR4))
#include <sys/time.h>		/* for __DATE__ and __TIME__ */
#include <sys/resource.h>
#else
#include <time.h>
#include <fcntl.h>
#endif /* USG */
#endif /* not VMS */

/* This defines "errno" properly for VMS, and gives us EACCES. */
#include <errno.h>

/* VMS-specific definitions */
#ifdef VMS
#include <time.h>
#include <descrip.h>
#ifndef NEXT_CPP_SERVER
#define O_RDONLY	0	/* Open arg for Read/Only  */
#define O_WRONLY	1	/* Open arg for Write/Only */
#endif
#define read(fd,buf,size)	VMS_read (fd,buf,size)
#define write(fd,buf,size)	VMS_write (fd,buf,size)
#define open(fname,mode,prot)	VMS_open (fname,mode,prot)
#define fopen(fname,mode)	VMS_fopen (fname,mode)
#define freopen(fname,mode,ofile) VMS_freopen (fname,mode,ofile)
#define strncat(dst,src,cnt) VMS_strncat (dst,src,cnt)
static char * VMS_strncat ();
static int VMS_read ();
static int VMS_write ();
static int VMS_open ();
static FILE * VMS_fopen ();
static FILE * VMS_freopen ();
static void hack_vms_include_specification ();
typedef struct { unsigned :16, :16, :16; } vms_ino_t;
#define ino_t vms_ino_t
#define INCLUDE_LEN_FUDGE 10	/* leave room for VMS syntax conversion */
#ifdef __GNUC__
#define BSTRING			/* VMS/GCC supplies the bstring routines */
#endif /* __GNUC__ */
#endif /* VMS */
  
extern char *index ();
extern char *rindex ();

#ifndef NEXT_CPP_SERVER
#ifndef O_RDONLY
#define O_RDONLY 0
#endif
#endif

#undef MIN
#undef MAX
#if !defined (hpux) || !defined (NEXT_PDO)
#define MIN(X,Y) ((X) < (Y) ? (X) : (Y))
#define MAX(X,Y) ((X) > (Y) ? (X) : (Y))
#endif

/* Find the largest host integer type and set its size and type.  */

#ifndef HOST_BITS_PER_WIDE_INT

#if HOST_BITS_PER_LONG > HOST_BITS_PER_INT
#define HOST_BITS_PER_WIDE_INT HOST_BITS_PER_LONG
#define HOST_WIDE_INT long
#else
#define HOST_BITS_PER_WIDE_INT HOST_BITS_PER_INT
#define HOST_WIDE_INT int
#endif

#endif

#ifndef S_ISREG
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#endif

#ifndef S_ISDIR
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#endif

/* Define a generic NULL if one hasn't already been defined.  */

#ifndef NULL
#define NULL 0
#endif

#ifndef GENERIC_PTR
#if defined (USE_PROTOTYPES) ? USE_PROTOTYPES : defined (__STDC__)
#define GENERIC_PTR void *
#else
#define GENERIC_PTR char *
#endif
#endif

#ifndef NULL_PTR
#define NULL_PTR ((GENERIC_PTR)0)
#endif

#ifndef INCLUDE_LEN_FUDGE
#define INCLUDE_LEN_FUDGE 0
#endif

#if defined (_WIN32) && defined (NEXT_PDO)
#define fstat _fstat
#define fdopen _fdopen
#define fileno _fileno
#import <stdlib.h>
#import <windows.h>
#endif

/* Forward declarations.  */

char *xmalloc ();
void error ();
void warning ();

/* External declarations.  */

extern char *getenv ();
extern FILE *fdopen ();
extern char *version_string;
extern struct tm *localtime ();
#ifndef VMS
#ifndef HAVE_STRERROR
#if !defined (_WIN32) || !defined (NEXT_PDO)
extern int sys_nerr;
#endif
#if defined(bsd4_4) || defined(__NetBSD__)
extern const char *const sys_errlist[];
#else
#if !defined (_WIN32) || !defined (NEXT_PDO)
extern char *sys_errlist[];
#endif
#endif
#else	/* HAVE_STERRROR */
char *strerror ();
#endif
#else	/* VMS */
char *strerror (int,...);
#endif
extern int parse_escape ();
extern HOST_WIDE_INT parse_c_expression ();

#ifndef errno
extern int errno;
#endif

/* Forward declarations.  */

struct directive;
struct file_buf;
struct arglist;
struct argdata;

#if defined(USG) || defined(VMS)
#ifndef BSTRING
void bcopy ();
void bzero ();
int bcmp ();
#endif
#endif

/* These functions are declared to return int instead of void since they
   are going to be placed in a table and some old compilers have trouble with
   pointers to functions returning void.  */

static int do_define ();
static int do_line ();
static int do_include ();
static int do_undef ();
static int do_error ();
static int do_pragma ();
static int do_ident ();
static int do_if ();
static int do_xifdef ();
static int do_else ();
static int do_elif ();
static int do_endif ();
static int do_sccs ();
static int do_once ();
static int do_assert ();
static int do_unassert ();
static int do_warning ();

#ifdef NEXT_CPP_SERVER
static struct import_file *add_import ();
#else
static void add_import ();
#endif
static void append_include_chain ();
#ifdef NEXT_FRAMEWORKS
static void append_framework_chain ();
#endif
static void deps_output ();
static void make_undef ();
static void make_definition ();
static void make_assertion ();
static void path_include ();
static void initialize_builtins ();
static void initialize_char_syntax ();
static void dump_arg_n ();
static void dump_defn_1 ();
static void delete_macro ();
static void trigraph_pcp ();
static void rescan ();
static void finclude ();
static void validate_else ();
static int comp_def_part ();
static void error_from_errno ();
static void error_with_line ();
void pedwarn ();
void pedwarn_with_line ();
static void pedwarn_with_file_and_line ();
static void fatal ();
void fancy_abort ();
static void pfatal_with_name ();
static void perror_with_name ();
static void pipe_closed ();
static void print_containing_files ();
static int lookup_import ();
static int redundant_include_p ();
static is_system_include ();
static struct file_name_map *read_name_map ();
static char *read_filename_string ();
static int open_include_file ();
static int check_preconditions ();
static void pcfinclude ();
static void pcstring_used ();
static void write_output ();
#ifdef NEXT_CPP_SERVER
static int write_header_output ();
static time_t file_modtime ();
static void handle_file_modification();
#endif
static int check_macro_name ();
static int compare_defs ();
static int compare_token_lists ();
static HOST_WIDE_INT eval_if_expression ();
static int discard_comments ();
static int change_newlines ();
static int line_for_error ();
static int hashf ();
static int file_size_and_mode ();

static struct arglist *read_token_list ();
static void free_token_list ();

static struct hashnode *install ();
struct hashnode *lookup ();

static struct assertion_hashnode *assertion_install ();
static struct assertion_hashnode *assertion_lookup ();

static char *xrealloc ();
static char *xcalloc ();
static char *savestring ();

static void delete_assertion ();
static void macroexpand ();
static void dump_all_macros ();
static void conditional_skip ();
static void skip_if_group ();
static void output_line_command ();

#ifdef NEXT_SEMANTICS
/* convert rtf */
static void buf_convert_rtf ();
static const char * rtf_to_ascii ();
#endif

/* Last arg to output_line_command.  */
#ifdef NEXT_CPP_SERVER
enum file_change_code {same_file, enter_file, leave_file,
                       enter_expansion, leave_expansion};
#else
enum file_change_code {same_file, enter_file, leave_file};
#endif

static int grow_outbuf ();
static int handle_directive ();
static void memory_full ();

static U_CHAR *macarg1 ();
static char *macarg ();

static U_CHAR *skip_to_end_of_comment ();
static U_CHAR *skip_quoted_string ();
static U_CHAR *skip_paren_group ();
static char *quote_string ();

static char *check_precompiled ();
/* static struct macrodef create_definition ();	[moved below] */
static void dump_single_macro ();
static void output_dots ();

#ifndef FAILURE_EXIT_CODE
#define FAILURE_EXIT_CODE 33	/* gnu cc command understands this */
#endif

#ifndef SUCCESS_EXIT_CODE
#define SUCCESS_EXIT_CODE 0	/* 0 means success on Unix.  */
#endif

/* Name under which this program was invoked.  */

static char *progname;

/* Nonzero means use extra default include directories for C++.  */

static int cplusplus;

/* Nonzero means handle cplusplus style comments */

static int cplusplus_comments;

/* Nonzero means handle #import, for objective C.  */

static int objc;

/* Nonzero means this is an assembly file, and allow
   unknown directives, which could be comments.  */

static int lang_asm;

/* Current maximum length of directory names in the search path
   for include files.  (Altered as we get more of them.)  */

static int max_include_len;

/* Nonzero means turn NOTREACHED into #pragma NOTREACHED etc */

static int for_lint = 0;

/* Nonzero means copy comments into the output file.  */

static int put_out_comments = 0;

/* Nonzero means don't process the ANSI trigraph sequences.  */

static int no_trigraphs = 0;

/* Nonzero means print the names of included files rather than
   the preprocessed output.  1 means just the #include "...",
   2 means #include <...> as well.  */

static int print_deps = 0;

/* Nonzero if missing .h files in -M output are assumed to be generated
   files and not errors.  */

static int print_deps_missing_files = 0;

/* Nonzero means print names of header files (-H).  */

static int print_include_names = 0;

/* Nonzero means don't output line number information.  */

static int no_line_commands;

/* Nonzero means output the text in failing conditionals,
   inside #failed ... #endfailed.  */

static int output_conditionals;

/* dump_only means inhibit output of the preprocessed text
             and instead output the definitions of all user-defined
             macros in a form suitable for use as input to cccp.
   dump_names means pass #define and the macro name through to output.
   dump_definitions means pass the whole definition (plus #define) through
*/

static enum {dump_none, dump_only, dump_names, dump_definitions}
     dump_macros = dump_none;

/* Nonzero means pass all #define and #undef directives which we actually
   process through to the output stream.  This feature is used primarily
   to allow cc1 to record the #defines and #undefs for the sake of
   debuggers which understand about preprocessor macros, but it may
   also be useful with -E to figure out how symbols are defined, and
   where they are defined.  */
static int debug_output = 0;

/* Nonzero indicates special processing used by the pcp program.  The
   special effects of this mode are: 
     
     Inhibit all macro expansion, except those inside #if directives.

     Process #define directives normally, and output their contents 
     to the output file.

     Output preconditions to pcp_outfile indicating all the relevant
     preconditions for use of this file in a later cpp run.
*/
static FILE *pcp_outfile;

/* Nonzero means we are inside an IF during a -pcp run.  In this mode
   macro expansion is done, and preconditions are output for all macro
   uses requiring them. */
static int pcp_inside_if;

/* Nonzero means never to include precompiled files.
   This is 1 since there's no way now to make precompiled files,
   so it's not worth testing for them.  */
static int no_precomp = 1;

/* Nonzero means give all the error messages the ANSI standard requires.  */

int pedantic;

/* Nonzero means try to make failure to fit ANSI C an error.  */

static int pedantic_errors;

/* Nonzero means don't print warning messages.  -w.  */

static int inhibit_warnings = 0;

/* Nonzero means warn if slash-star appears in a comment.  */

static int warn_comments;

/* Nonzero means warn if a macro argument is (or would be)
   stringified with -traditional.  */

static int warn_stringify;

/* Nonzero means warn if there are any trigraphs.  */

static int warn_trigraphs;

/* Nonzero means warn if #import is used.  */

#ifdef NEXT_OBJC_RUNTIME
static int warn_import = 0;
#else
static int warn_import = 1;
#endif

/* Nonzero means turn warnings into errors.  */

static int warnings_are_errors;

/* Nonzero means try to imitate old fashioned non-ANSI preprocessor.  */

int traditional;

/* Nonzero causes output not to be done,
   but directives such as #define that have side effects
   are still obeyed.  */

static int no_output;

#ifdef NEXT_SEMANTICS
/* Nonzero causes input not to have rtf codes stripped before 
   further processing. */

static int no_rtf = 0;
#endif

/* Nonzero means this file was included with a -imacros or -include
   command line and should not be recorded as an include file.  */

static int no_record_file;

/* Nonzero means that we have finished processing the command line options.
   This flag is used to decide whether or not to issue certain errors
   and/or warnings.  */

static int done_initializing = 0;

/* Line where a newline was first seen in a string constant.  */

static int multiline_string_line = 0;

/* I/O buffer structure.
   The `fname' field is nonzero for source files and #include files
   and for the dummy text used for -D and -U.
   It is zero for rescanning results of macro expansion
   and for expanding macro arguments.  */
#define INPUT_STACK_MAX 400
static struct file_buf {
  char *fname;
  /* Filename specified with #line command.  */
  char *nominal_fname;
  /* Record where in the search path this file was found.
     For #include_next.  */
  struct file_name_list *dir;
#ifdef NEXT_CPP_SERVER
  struct file_name_list *file; /* used by segregate_output */
  U_CHAR *beg_of_line;	       /* used by mark_expansions */
  U_CHAR *output_written_to;
#endif
  int lineno;
  int length;
  U_CHAR *buf;
  U_CHAR *bufp;
  /* Macro that this level is the expansion of.
     Included so that we can reenable the macro
     at the end of this level.  */
  struct hashnode *macro;
  /* Value of if_stack at start of this file.
     Used to prohibit unmatched #endif (etc) in an include file.  */
  struct if_stack *if_stack;
  /* Object to be freed at end of input at this level.  */
  U_CHAR *free_ptr;
  /* True if this is a header file included using <FILENAME>.  */
  char system_header_p;
} instack[INPUT_STACK_MAX];

static int last_error_tick;	   /* Incremented each time we print it.  */
static int input_file_stack_tick;  /* Incremented when the status changes.  */

/* Current nesting level of input sources.
   `instack[indepth]' is the level currently being read.  */
static int indepth = -1;
#define CHECK_DEPTH(code) \
  if (indepth >= (INPUT_STACK_MAX - 1))					\
    {									\
      error_with_line (line_for_error (instack[indepth].lineno),	\
		       "macro or `#include' recursion too deep");	\
      code;								\
    }

/* Current depth in #include directives that use <...>.  */
static int system_include_depth = 0;

typedef struct file_buf FILE_BUF;

/* The output buffer.  Its LENGTH field is the amount of room allocated
   for the buffer, not the number of chars actually present.  To get
   that, subtract outbuf.buf from outbuf.bufp. */

#define OUTBUF_SIZE 10	/* initial size of output buffer */
static FILE_BUF outbuf;

#ifdef NEXT_CPP_SERVER

#if !defined (NEXT_PDO) || defined (_WIN32)
#include <sys/dir.h>
#endif
#include <string.h>
#include <sys/fcntl.h>

/* Non-0 means separate the preprocessed module from the headers it imports.
   This is a "poor man's" precompiled headers */
static int segregate_output = 0;

/* Non-0 means output line/position markers before/after each expansion.
   Plain old line commands were extended (see output_line_command) */
static int mark_expansions = 0;

/* Non-zero means we are reading all further commands from stdin until a 
   "terminate" command is received */
static int serverized = 0;

/* Non-zero specifies that the time required to cpp is output.
   Note: clients that turn this on should be prepared to handle the output,
   if it is being parsed. */ 
static int timings = 0;

/* Non-zero means we are serverized and have already output the stat
   information for the module we are processing */
static int outbuf_stat_info_recorded = 0;

static FILE_BUF sys_header_outbuf; /* shared, segregated buf for headers */
static FILE_BUF user_header_outbuf;

static struct hashnode *free_undefined_macros = 0;

/* List of all input and output files (now that cpp can take a list of files)
   This is built as files are processed */
static struct file_name_list *all_input_files = 0;
static struct file_name_list *all_output_files = 0;

static void add_to_negative_cache ();
static int found_in_negative_cache ();
static void add_version_of_header ();
static int duplicate_version_of_header ();
static int aggressive_skip_if_group ();
static void dump_memory_statistics ();
static void clear_import ();
static void clear_all_macros();
static void enable_macros_from_header();

static void init_output_file_buf();

/* for debugging */
static void start();
static void stop();
static void display(FILE * strm, int addNewLine);

/* Hash table of files already included with #include or #import.  */
#define IMPORT_HASH_SIZE 1403
static struct import_file *import_hash_table[IMPORT_HASH_SIZE];
static int import_hash ();
#endif

/* Grow output buffer OBUF points at
   so it can hold at least NEEDED more chars.  */

#define check_expand(OBUF, NEEDED)  \
  (((OBUF)->length - ((OBUF)->bufp - (OBUF)->buf) <= (NEEDED))   \
   ? grow_outbuf ((OBUF), (NEEDED)) : 0)

struct file_name_list
  {
    struct file_name_list *next;
    char *fname;
    /* If the following is nonzero, it is a macro name.
       Don't include the file again if that macro is defined.  */
    U_CHAR *control_macro;
    /* If the following is nonzero, it is a C-language system include
       directory.  */
    int c_system_include_path;
    /* Mapping of file names for this directory.  */
    struct file_name_map *name_map;
    /* Non-zero if name_map is valid.  */
    int got_name_map;
#ifdef NEXT_CPP_SERVER
    /* If we are processing a list of modules, cache headers 
       Normally, this buffer is freed in finclude  */
    int length;
    U_CHAR *buf;
    /* If we are segregating headers, mark if already output */
    int header_output;
    int has_explicit_context_dependency;
    int ever_included; 		/* 0 means always #imported */
    /* Keep a list of macros that are a part of this header */
    int macro_count;
    int macro_index;
    struct hashnode **macro_list;
    /* Keep various markers into the input buffer (to enable interesting
       optimizations in skip_if_group when preprocessing several files). */
    U_CHAR *position_after_last_include; 
    U_CHAR *position_of_first_ifndef; /* if control_macro != 0 */
    U_CHAR *position_of_last_endif;   /* if control_macro != 0 */
    /* info to exclude duplicates in the output buffer (for perverse cases
       like stdtypes.h...very rare). Offsets into the output buffer are
       stored relative, since the buffer is often being realloced. */
    unsigned int relative_position_of_file_start; /* into a header_outbuf */
    unsigned int file_length; /* in bytes */
    struct file_name_list *versions;  /* diff. mutations of the same file */
#endif
  };

/* #include "file" looks in source file dir, then stack. */
/* #include <file> just looks in the stack. */
#if defined (NEXT_SEMANTICS) || defined (NEXT_PDO)
/* cplusplus = 0 -> C.  cplusplus = 1 -> C++. cplusplus = 2 -> nothing */
#endif
/* -I directories are added to the end, then the defaults are added. */
/* The */
static struct default_include {
  char *fname;			/* The name of the directory.  */
  int cplusplus;		/* Only look here if we're compiling C++.  */
  int cxx_aware;		/* Includes in this directory don't need to
				   be wrapped in extern "C" when compiling
				   C++.  */
} include_defaults_array[]
#ifdef INCLUDE_DEFAULTS
  = INCLUDE_DEFAULTS;
#else
  = {
    /* Pick up GNU C++ specific include files.  */
    { GPLUSPLUS_INCLUDE_DIR, 1, 1 },
#ifdef CROSS_COMPILE
    /* This is the dir for fixincludes.  Put it just before
       the files that we fix.  */
    { GCC_INCLUDE_DIR, 0, 0 },
    /* For cross-compilation, this dir name is generated
       automatically in Makefile.in.  */
    { CROSS_INCLUDE_DIR, 0, 0 },
    /* This is another place that the target system's headers might be.  */
    { TOOL_INCLUDE_DIR, 0, 0 },
#else /* not CROSS_COMPILE */
    /* This should be /usr/local/include and should come before
       the fixincludes-fixed header files.  */
    { LOCAL_INCLUDE_DIR, 0, 1 },
    /* This is here ahead of GCC_INCLUDE_DIR because assert.h goes here.
       Likewise, behind LOCAL_INCLUDE_DIR, where glibc puts its assert.h.  */
    { TOOL_INCLUDE_DIR, 0, 0 },
    /* This is the dir for fixincludes.  Put it just before
       the files that we fix.  */
    { GCC_INCLUDE_DIR, 0, 0 },
    /* Some systems have an extra dir of include files.  */
#ifdef SYSTEM_INCLUDE_DIR
    { SYSTEM_INCLUDE_DIR, 0, 0 },
#endif
    { STANDARD_INCLUDE_DIR, 0, 0 },
#endif /* not CROSS_COMPILE */
    { 0, 0, 0 }
    };
#endif /* no INCLUDE_DEFAULTS */
#ifdef NEXT_FRAMEWORKS
static struct default_include framework_defaults_array [] = {
#ifdef OPENSTEP
    {"/LocalLibrary/Frameworks", 0, 0},
    {"/NextLibrary/Frameworks", 0, 0},
#else
    {"/Local/Library/Frameworks", 0, 0},
#ifdef NEXT_PDO
    {"/Library/Frameworks", 0},
#else
    {"/Network/Library/Frameworks", 0, 0},
    {"/System/Library/Frameworks", 0, 0},
#endif
#endif
    {NULL, 0, 0}
 };

static struct default_include *framework_defaults=framework_defaults_array;
static struct file_name_list *framework = 0;

struct framework_header {char * dirName; int dirNameLen; };
static struct framework_header framework_header_dirs[] = {
   { "PrivateHeaders", 14 },
   { "Headers", 7 },
   { NULL, 0 }
};
#endif

/* The code looks at the defaults through this pointer, rather than through
   the constant structure above.  This pointer gets changed if an environment
   variable specifies other defaults.  */
static struct default_include *include_defaults = include_defaults_array;

static struct file_name_list *include = 0;	/* First dir to search */
	/* First dir to search for <file> */
/* This is the first element to use for #include <...>.
   If it is 0, use the entire chain for such includes.  */
static struct file_name_list *first_bracket_include = 0;
#ifdef NEXT_FRAMEWORKS
static struct file_name_list *first_bracket_framework = 0;
#endif
/* This is the first element in the chain that corresponds to
   a directory of system header files.  */
static struct file_name_list *first_system_include = 0;
static struct file_name_list *last_include = 0;	/* Last in chain */
#ifdef NEXT_FRAMEWORKS
static struct file_name_list *last_framework = 0; /* Last in chain */
#endif

/* Chain of include directories to put at the end of the other chain.  */
static struct file_name_list *after_include = 0;
static struct file_name_list *last_after_include = 0;	/* Last in chain */

/* Chain to put at the start of the system include files.  */
static struct file_name_list *before_system = 0;
static struct file_name_list *last_before_system = 0;	/* Last in chain */

/* List of included files that contained #pragma once.  */
static struct file_name_list *dont_repeat_files = 0;

/* List of other included files.
   If ->control_macro if nonzero, the file had a #ifndef
   around the entire contents, and ->control_macro gives the macro name.  */
static struct file_name_list *all_include_files = 0;

/* Directory prefix that should replace `/usr' in the standard
   include file directories.  */
static char *include_prefix;

/* Global list of strings read in from precompiled files.  This list
   is kept in the order the strings are read in, with new strings being
   added at the end through stringlist_tailp.  We use this list to output
   the strings at the end of the run. 
*/
static STRINGDEF *stringlist;
static STRINGDEF **stringlist_tailp = &stringlist;


/* Structure returned by create_definition */
typedef struct macrodef MACRODEF;
struct macrodef
{
  struct definition *defn;
  U_CHAR *symnam;
  int symlen;
};

static struct macrodef create_definition ();


/* Structure allocated for every #define.  For a simple replacement
   such as
   	#define foo bar ,
   nargs = -1, the `pattern' list is null, and the expansion is just
   the replacement text.  Nargs = 0 means a functionlike macro with no args,
   e.g.,
       #define getchar() getc (stdin) .
   When there are args, the expansion is the replacement text with the
   args squashed out, and the reflist is a list describing how to
   build the output from the input: e.g., "3 chars, then the 1st arg,
   then 9 chars, then the 3rd arg, then 0 chars, then the 2nd arg".
   The chars here come from the expansion.  Whatever is left of the
   expansion after the last arg-occurrence is copied after that arg.
   Note that the reflist can be arbitrarily long---
   its length depends on the number of times the arguments appear in
   the replacement text, not how many args there are.  Example:
   #define f(x) x+x+x+x+x+x+x would have replacement text "++++++" and
   pattern list
     { (0, 1), (1, 1), (1, 1), ..., (1, 1), NULL }
   where (x, y) means (nchars, argno). */

typedef struct definition DEFINITION;
struct definition {
  int nargs;
  int length;			/* length of expansion string */
  int predefined;		/* True if the macro was builtin or */
				/* came from the command line */
  U_CHAR *expansion;
  int line;			/* Line number of definition */
  char *file;			/* File of definition */
  char rest_args;		/* Nonzero if last arg. absorbs the rest */
  struct reflist {
    struct reflist *next;
    char stringify;		/* nonzero if this arg was preceded by a
				   # operator. */
    char raw_before;		/* Nonzero if a ## operator before arg. */
    char raw_after;		/* Nonzero if a ## operator after arg. */
    char rest_args;		/* Nonzero if this arg. absorbs the rest */
    int nchars;			/* Number of literal chars to copy before
				   this arg occurrence.  */
    int argno;			/* Number of arg to substitute (origin-0) */
  } *pattern;
  union {
    /* Names of macro args, concatenated in reverse order
       with comma-space between them.
       The only use of this is that we warn on redefinition
       if this differs between the old and new definitions.  */
    U_CHAR *argnames;
  } args;
};

/* different kinds of things that can appear in the value field
   of a hash node.  Actually, this may be useless now. */
union hashval {
  int ival;
  char *cpval;
  DEFINITION *defn;
  KEYDEF *keydef;
};

/*
 * special extension string that can be added to the last macro argument to 
 * allow it to absorb the "rest" of the arguments when expanded.  Ex:
 * 		#define wow(a, b...)		process (b, a, b)
 *		{ wow (1, 2, 3); }	->	{ process (2, 3, 1, 2, 3); }
 *		{ wow (one, two); }	->	{ process (two, one, two); }
 * if this "rest_arg" is used with the concat token '##' and if it is not
 * supplied then the token attached to with ## will not be outputted.  Ex:
 * 		#define wow (a, b...)		process (b ## , a, ## b)
 *		{ wow (1, 2); }		->	{ process (2, 1, 2); }
 *		{ wow (one); }		->	{ process (one); {
 */
static char rest_extension[] = "...";
#define REST_EXTENSION_LENGTH	(sizeof (rest_extension) - 1)

/* The structure of a node in the hash table.  The hash table
   has entries for all tokens defined by #define commands (type T_MACRO),
   plus some special tokens like __LINE__ (these each have their own
   type, and the appropriate code is run when that type of node is seen.
   It does not contain control words like "#define", which are recognized
   by a separate piece of code. */

/* different flavors of hash nodes --- also used in keyword table */
enum node_type {
 T_DEFINE = 1,	/* the `#define' keyword */
 T_INCLUDE,	/* the `#include' keyword */
 T_INCLUDE_NEXT, /* the `#include_next' keyword */
 T_IMPORT,      /* the `#import' keyword */
 T_IFDEF,	/* the `#ifdef' keyword */
 T_IFNDEF,	/* the `#ifndef' keyword */
 T_IF,		/* the `#if' keyword */
 T_ELSE,	/* `#else' */
 T_PRAGMA,	/* `#pragma' */
 T_ELIF,	/* `#elif' */
 T_UNDEF,	/* `#undef' */
 T_LINE,	/* `#line' */
 T_ERROR,	/* `#error' */
 T_WARNING,	/* `#warning' */
 T_ENDIF,	/* `#endif' */
 T_SCCS,	/* `#sccs', used on system V.  */
 T_IDENT,	/* `#ident', used on system V.  */
 T_ASSERT,	/* `#assert', taken from system V.  */
 T_UNASSERT,	/* `#unassert', taken from system V.  */
 T_SPECLINE,	/* special symbol `__LINE__' */
 T_DATE,	/* `__DATE__' */
 T_FILE,	/* `__FILE__' */
 T_BASE_FILE,	/* `__BASE_FILE__' */
 T_INCLUDE_LEVEL, /* `__INCLUDE_LEVEL__' */
 T_VERSION,	/* `__VERSION__' */
 T_SIZE_TYPE,   /* `__SIZE_TYPE__' */
 T_PTRDIFF_TYPE,   /* `__PTRDIFF_TYPE__' */
 T_WCHAR_TYPE,   /* `__WCHAR_TYPE__' */
 T_USER_LABEL_PREFIX_TYPE, /* `__USER_LABEL_PREFIX__' */
 T_REGISTER_PREFIX_TYPE,   /* `__REGISTER_PREFIX__' */
 T_TIME,	/* `__TIME__' */
 T_CONST,	/* Constant value, used by `__STDC__' */
 T_MACRO,	/* macro defined by `#define' */
 T_DISABLED,	/* macro temporarily turned off for rescan */
 T_SPEC_DEFINED, /* special `defined' macro for use in #if statements */
 T_PCSTRING,	/* precompiled string (hashval is KEYDEF *) */
 T_UNUSED	/* Used for something not defined.  */
 };

struct hashnode {
  struct hashnode *next;	/* double links for easy deletion */
  struct hashnode *prev;
  struct hashnode **bucket_hdr;	/* also, a back pointer to this node's hash
				   chain is kept, in case the node is the head
				   of the chain and gets deleted. */
  enum node_type type;		/* type of special token */
  int length;			/* length of token, for quick comparison */
  U_CHAR *name;			/* the actual name */
  union hashval value;		/* pointer to expansion, or whatever */
#ifdef NEXT_CPP_SERVER
  int defined_within_module;	/* is it currently on? (maybe should
                                   just take a bit from the type field) */
  struct file_name_list *owner; /* could have added this to "DEFINITION *",
                                   however decided this was lower risk and
                                   provided better locality (wrt the previous
                                   field. Since hashnodes are primarily used
                                   for macros, the space overhead is not
                                   an issue */
#endif
};

typedef struct hashnode HASHNODE;

/* Some definitions for the hash table.  The hash function MUST be
   computed as shown in hashf () below.  That is because the rescan
   loop computes the hash value `on the fly' for most tokens,
   in order to avoid the overhead of a lot of procedure calls to
   the hashf () function.  Hashf () only exists for the sake of
   politeness, for use when speed isn't so important. */

#define HASHSIZE 1403
static HASHNODE *hashtab[HASHSIZE];
#define HASHSTEP(old, c) ((old << 2) + c)
#define MAKE_POS(v) (v & 0x7fffffff) /* make number positive */

/* Symbols to predefine.  */

#ifdef CPP_PREDEFINES
static char *predefs = CPP_PREDEFINES;
#else
static char *predefs = "";
#endif

/* We let tm.h override the types used here, to handle trivial differences
   such as the choice of unsigned int or long unsigned int for size_t.
   When machines start needing nontrivial differences in the size type,
   it would be best to do something here to figure out automatically
   from other information what type to use.  */

/* The string value for __SIZE_TYPE__.  */

#ifndef SIZE_TYPE
#define SIZE_TYPE "long unsigned int"
#endif

/* The string value for __PTRDIFF_TYPE__.  */

#ifndef PTRDIFF_TYPE
#define PTRDIFF_TYPE "long int"
#endif

/* The string value for __WCHAR_TYPE__.  */

#ifndef WCHAR_TYPE
#define WCHAR_TYPE "int"
#endif
char * wchar_type = WCHAR_TYPE;
#undef WCHAR_TYPE

/* The string value for __USER_LABEL_PREFIX__ */

#ifndef USER_LABEL_PREFIX
#define USER_LABEL_PREFIX ""
#endif

/* The string value for __REGISTER_PREFIX__ */

#ifndef REGISTER_PREFIX
#define REGISTER_PREFIX ""
#endif

/* In the definition of a #assert name, this structure forms
   a list of the individual values asserted.
   Each value is itself a list of "tokens".
   These are strings that are compared by name.  */

struct tokenlist_list {
  struct tokenlist_list *next;
  struct arglist *tokens;
};

struct assertion_hashnode {
  struct assertion_hashnode *next;	/* double links for easy deletion */
  struct assertion_hashnode *prev;
  /* also, a back pointer to this node's hash
     chain is kept, in case the node is the head
     of the chain and gets deleted. */
  struct assertion_hashnode **bucket_hdr;
  int length;			/* length of token, for quick comparison */
  U_CHAR *name;			/* the actual name */
  /* List of token-sequences.  */
  struct tokenlist_list *value;
};

typedef struct assertion_hashnode ASSERTION_HASHNODE;

/* Some definitions for the hash table.  The hash function MUST be
   computed as shown in hashf below.  That is because the rescan
   loop computes the hash value `on the fly' for most tokens,
   in order to avoid the overhead of a lot of procedure calls to
   the hashf function.  hashf only exists for the sake of
   politeness, for use when speed isn't so important. */

#define ASSERTION_HASHSIZE 37
static ASSERTION_HASHNODE *assertion_hashtab[ASSERTION_HASHSIZE];

/* Nonzero means inhibit macroexpansion of what seem to be
   assertion tests, in rescan.  For #if.  */
static int assertions_flag;

/* `struct directive' defines one #-directive, including how to handle it.  */

struct directive {
  int length;			/* Length of name */
  int (*func)();		/* Function to handle directive */
  char *name;			/* Name of directive */
  enum node_type type;		/* Code which describes which directive. */
  char angle_brackets;		/* Nonzero => <...> is special.  */
  char traditional_comments;	/* Nonzero: keep comments if -traditional.  */
  char pass_thru;		/* Copy preprocessed directive to output file.  */
};

/* Here is the actual list of #-directives, most-often-used first.  */

static struct directive directive_table[] = {
  {  6, do_define, "define", T_DEFINE, 0, 1},
  {  2, do_if, "if", T_IF},
  {  5, do_xifdef, "ifdef", T_IFDEF},
  {  6, do_xifdef, "ifndef", T_IFNDEF},
  {  5, do_endif, "endif", T_ENDIF},
  {  4, do_else, "else", T_ELSE},
  {  4, do_elif, "elif", T_ELIF},
  {  4, do_line, "line", T_LINE},
  {  7, do_include, "include", T_INCLUDE, 1},
  { 12, do_include, "include_next", T_INCLUDE_NEXT, 1},
  {  6, do_include, "import", T_IMPORT, 1},
  {  5, do_undef, "undef", T_UNDEF},
  {  5, do_error, "error", T_ERROR},
  {  7, do_warning, "warning", T_WARNING},
#ifdef SCCS_DIRECTIVE
  {  4, do_sccs, "sccs", T_SCCS},
#endif
  {  6, do_pragma, "pragma", T_PRAGMA, 0, 0, 1},
  {  5, do_ident, "ident", T_IDENT},
  {  6, do_assert, "assert", T_ASSERT},
  {  8, do_unassert, "unassert", T_UNASSERT},
  {  -1, 0, "", T_UNUSED},
};

/* When a directive handler is called,
   this points to the # that started the directive.  */
U_CHAR *directive_start;

/* table to tell if char can be part of a C identifier. */
U_CHAR is_idchar[256];
/* table to tell if char can be first char of a c identifier. */
U_CHAR is_idstart[256];
/* table to tell if c is horizontal space.  */
U_CHAR is_hor_space[256];
/* table to tell if c is horizontal or vertical space.  */
static U_CHAR is_space[256];

#define SKIP_WHITE_SPACE(p) do { while (is_hor_space[*p]) p++; } while (0)
#define SKIP_ALL_WHITE_SPACE(p) do { while (is_space[*p]) p++; } while (0)
  
static int errors = 0;			/* Error counter for exit code */

/* Name of output file, for error messages.  */
static char *out_fname;

/* Zero means dollar signs are punctuation.
   -$ stores 0; -traditional may store 1.  Default is 1 for VMS, 0 otherwise.
   This must be 0 for correct processing of this ANSI C program:
	#define foo(a) #a
	#define lose(b) foo (b)
	#define test$
	lose (test)	*/
static int dollars_in_ident;
#ifndef DOLLARS_IN_IDENTIFIERS
#define DOLLARS_IN_IDENTIFIERS 1
#endif

static FILE_BUF expand_to_temp_buffer ();

static DEFINITION *collect_expansion ();

/* Stack of conditionals currently in progress
   (including both successful and failing conditionals).  */

struct if_stack {
  struct if_stack *next;	/* for chaining to the next stack frame */
  char *fname;		/* copied from input when frame is made */
  int lineno;			/* similarly */
  int if_succeeded;		/* true if a leg of this if-group
				    has been passed through rescan */
  U_CHAR *control_macro;	/* For #ifndef at start of file,
				   this is the macro name tested.  */
  enum node_type type;		/* type of last directive seen in this group */
};
typedef struct if_stack IF_STACK_FRAME;
static IF_STACK_FRAME *if_stack = NULL;

/* Buffer of -M output.  */
static char *deps_buffer;

/* Number of bytes allocated in above.  */
static int deps_allocated_size;

/* Number of bytes used.  */
static int deps_size;

/* Number of bytes since the last newline.  */
static int deps_column;

/* Nonzero means -I- has been seen,
   so don't look for #include "foo" the source-file directory.  */
static int ignore_srcdir;

#ifdef NEXT_SEMANTICS
#include <string.h> /* Should contain the prototype for memchr ().  */
#endif

/* Read LEN bytes at PTR from descriptor DESC, for file FILENAME,
   retrying if necessary.  Return a negative value if an error occurs,
   otherwise return the actual number of bytes read,
   which must be LEN unless end-of-file was reached.  */

static int
safe_read (desc, ptr, len)
     int desc;
     char *ptr;
     int len;
{
  int left = len;
  while (left > 0) {
    int nchars = read (desc, ptr, left);
    if (nchars < 0)
      {
#ifdef EINTR
	if (errno == EINTR)
	  continue;
#endif
	return nchars;
      }
    if (nchars == 0)
      break;
#ifdef NEXT_SEMANTICS
    {
      int chars_left; /* Characters still needing to be looked at.  */
      char *p, *q;
      /* Convert Mac- and Windows-style line termination to line feed.  */
      for (chars_left = nchars, p = ptr;
	   chars_left && (q = memchr (p, '\r', chars_left));
	   p = q)
	{
	  *q++ = '\n';
	  chars_left -= q - p;
	  if (chars_left && *q == '\n')
	    break;
	}
      if (chars_left && q)
	{
	  /* Windows-style line termination (CR-LF) was found.
	     Make p point to line feed character of the CR-LF pair just found,
	     and adjust nchars and chars_left accordingly.  */
	  nchars--;
	  for (p = q++; --chars_left; )
	    {
	      if (*q == '\r')
		if (--chars_left)
		  if (*++q == '\n')
		    /* Adjust the number of characters actually read.  */
		    nchars--;
		  else
		    *p++ = '\n';
		else
		  {
		    /* The last character in the file is CR.  */
		    *p++ = '\n';
		    break;
		  }
	      *p++ = *q++;
	    }
	}
    }
#endif
    ptr += nchars;
    left -= nchars;
  }
  return len - left;
}

/* Write LEN bytes at PTR to descriptor DESC,
   retrying if necessary, and treating any real error as fatal.  */

static void
safe_write (desc, ptr, len)
     int desc;
     char *ptr;
     int len;
{
  while (len > 0) {
    int written = write (desc, ptr, len);
    if (written < 0)
      {
#ifdef EINTR
	if (errno == EINTR)
	  continue;
#endif
	pfatal_with_name (out_fname);
      }
    ptr += written;
    len -= written;
  }
}

int
main (argc, argv)
     int argc;
     char **argv;
{
  int st_mode;
  long st_size;
  char *in_fname;
  char *p;
  int f, i;
  FILE_BUF *fp;
  char **pend_files = (char **) xmalloc (argc * sizeof (char *));
  char **pend_defs = (char **) xmalloc (argc * sizeof (char *));
  char **pend_undefs = (char **) xmalloc (argc * sizeof (char *));
  char **pend_assertions = (char **) xmalloc (argc * sizeof (char *));
  char **pend_includes = (char **) xmalloc (argc * sizeof (char *));
#ifdef NEXT_CPP_SERVER
  #define MAX_INPUT_ARGS 128  
  #define MAX_INPUT_CHARS 4096
  char command[MAX_INPUT_CHARS+1];
  struct file_name_list *include_ptr; 
  int max_argc = argc;  /* keep track of largest argc seen */
  char * header_fname;
  char * requested_cwd;
  char actual_cwd[1024];
#endif

  /* Record the option used with each element of pend_assertions.
     This is preparation for supporting more than one option for making
     an assertion.  */
  char **pend_assertion_options = (char **) xmalloc (argc * sizeof (char *));
  int inhibit_predefs = 0;
  int no_standard_includes = 0;
  int no_standard_cplusplus_includes = 0;
  int missing_newline = 0;

  /* Non-0 means don't output the preprocessed program.  */
  int inhibit_output = 0;
  /* Non-0 means -v, so print the full set of include dirs.  */
  int verbose = 0;

  /* File name which deps are being written to.
     This is 0 if deps are being written to stdout.  */
  char *deps_file = 0;
  /* Fopen file mode to open deps_file with.  */
  char *deps_mode = "a";
  /* Stream on which to print the dependency information.  */
  FILE *deps_stream = 0;
  /* Target-name to write with the dependency information.  */
  char *deps_target = 0;

#if !defined (NeXT) && !defined (__APPLE__)
#ifdef RLIMIT_STACK
  /* Get rid of any avoidable limit on stack size.  */
  {
    struct rlimit rlim;

    /* Set the stack limit huge so that alloca (particularly stringtab
     * in dbxread.c) does not fail. */
    getrlimit (RLIMIT_STACK, &rlim);
    rlim.rlim_cur = rlim.rlim_max;
    setrlimit (RLIMIT_STACK, &rlim);
  }
#endif /* RLIMIT_STACK defined */
#endif

#ifdef SIGPIPE
  signal (SIGPIPE, pipe_closed);
#endif

  p = argv[0] + strlen (argv[0]);
  while (p != argv[0] && p[-1] != '/'
#ifdef DIR_SEPARATOR
	 && p[-1] != DIR_SEPARATOR
#endif
	 )
    --p;
  progname = p;

#ifdef VMS
  {
    /* Remove directories from PROGNAME.  */
    char *s;

    progname = savestring (argv[0]);

    if (!(s = rindex (progname, ']')))
      s = rindex (progname, ':');
    if (s)
      strcpy (progname, s+1);
    if (s = rindex (progname, '.'))
      *s = '\0';
  }
#endif

  in_fname = NULL;
  out_fname = NULL;

  /* Initialize is_idchar to allow $.  */
  dollars_in_ident = 1;
  initialize_char_syntax ();
  dollars_in_ident = DOLLARS_IN_IDENTIFIERS > 0;

  no_line_commands = 0;
  no_trigraphs = 1;
  dump_macros = dump_none;
  no_output = 0;
  cplusplus = 0;
#if defined (NEXT_SEMANTICS) || defined (NEXT_PDO)
  cplusplus_comments = 1;
#else
  cplusplus_comments = 0;
#endif

#ifdef NEXT_CPP_SERVER
#define FAILURE                      \
        if (serverized)              \
	   goto processCommand;      \
	else                         \
	   return FAILURE_EXIT_CODE

#define BAILOUT(name) {              \
        if (serverized) {            \
           perror_with_name (name);  \
           goto finishRequest;       \
        } else {                     \
           pfatal_with_name (name);  \
        }                            \
}
           
  /* initialize output buffers */
  init_output_file_buf(&outbuf);
  init_output_file_buf(&sys_header_outbuf);
  init_output_file_buf(&user_header_outbuf);

  if (argc > 1 && !strcmp(argv[1],"-serverize")) {
     /* set up for serverized cpp semantics and looping on input commands */
     serverized = 1;
     segregate_output = 1;
     dump_macros = dump_definitions;
     /* mark_expansions = 1;   only adds about 4-5% to output text size */
#if defined (NEXT_PDO) && !defined (_WIN32)
     if (!getcwd(actual_cwd, 1024))
#else
     if (!getwd(actual_cwd))
#endif
        pfatal_with_name(actual_cwd);
     argv = (char **) xmalloc(sizeof(char *) * MAX_INPUT_ARGS);
     fprintf(stderr,"Starting serverized cpp process.  Enter commands:\n");
  }

  processCommand:
  while (serverized) {
     int preprocess = 0;
     char *tok;

     /* read a command off of stdin and point argv at it */
     if (!fgets(command, MAX_INPUT_CHARS, stdin)) {
        fprintf(stderr,"End of server commands.  Exiting...\n");
#ifdef PROFILE
        chdir(getenv("HOME"));
#endif        
        exit(FAILURE_EXIT_CODE);
     }
     argc = 0;
     for (tok = strtok(command," \t\n");
          tok && (argc < MAX_INPUT_ARGS);
          tok = strtok(NULL," \t\n")) 
        argv[argc++] = tok;
     if (!argc)
        goto processCommand;
     else if (argc > max_argc) {
        max_argc = argc;
        pend_files = (char **) xrealloc (pend_files, argc * sizeof (char *));
        pend_defs = (char **) xrealloc (pend_defs, argc * sizeof (char *));
        pend_undefs = (char **) xrealloc (pend_undefs, argc * sizeof (char *));
        pend_assertions = (char **) xrealloc (pend_assertions, 
                                                       argc * sizeof (char *));
        pend_includes = (char **) xrealloc (pend_includes, 
                                                       argc * sizeof (char *));
        pend_assertion_options = (char **) xrealloc (pend_assertion_options,
                                                       argc * sizeof (char *));       
     }

     switch (argv[0][0]) {
      case 'c':
        if (!strcmp(argv[0],"cpp")) 
           preprocess = 1; /* fall on through into body of main */
        break;
        
      case 'm':
        if (!strcmp(argv[0],"modtime"))
           if (argc > 2) {
              time_t lastModTime = file_modtime (argv[2]);
              fprintf(stderr, "modtime is %d\n");
           } else {
              fprintf(stderr,"bad modtime argument: <file name>\n");
           }
        break;
           
      case 'n':
        if (!strcmp(argv[0],"notice_file_modification"))
           if (argc > 1)
              handle_file_modification(argv[1]);
           else
              fprintf(stderr,
                      "bad notice_file_modification argument: <file name>\n");           
        break;
        
      case 's':
        if (!strcmp(argv[0],"stats"))
           dump_memory_statistics();
        break;

      case 't':
        if (!strcmp(argv[0],"terminate"))
           exit(0);
        else if (!strcmp(argv[0],"timings"))
           if ((argc > 1) && !strcmp(argv[1],"on"))
              timings = 1;
        break;

      case 'w':
        if (!strcmp(argv[0],"write_sys_headers")) {
           if (argc > 2) {
              int write_all = (strcmp(argv[1], "all") == 0) ? 1 : 0;
              out_fname = argv[2];
              write_header_output (out_fname, &sys_header_outbuf, write_all, 1);
           } else {
              fprintf(stderr,
                  "bad write_sys_headers arguments: <all|new> <file name>\n");
           }
        } else if (!strcmp(argv[0],"write_user_headers")) {
           if (argc > 1) {
              int write_all = (strcmp(argv[1], "all") == 0) ? 1 : 0;
              out_fname = argv[2];
              write_header_output (out_fname, &user_header_outbuf, write_all,1);
           } else {
              fprintf(stderr,
                "bad write_user_headers arguments: <all|new> <file name>\n");
           }
        }
        break;
           
     } /* switch(argv[0][0]) */
     if (preprocess)
        break;  /* go do regular "main" logic on this preprocess request */
  }
  errors = 0;  /* in case last iteration caused errors */
  in_fname = out_fname = header_fname = requested_cwd = NULL;
  outbuf_stat_info_recorded = 0;
#endif
     
  bzero ((char *) pend_files, argc * sizeof (char *));
  bzero ((char *) pend_defs, argc * sizeof (char *));
  bzero ((char *) pend_undefs, argc * sizeof (char *));
  bzero ((char *) pend_assertions, argc * sizeof (char *));
  bzero ((char *) pend_includes, argc * sizeof (char *));

  /* Process switches and find input file name.  */

  for (i = 1; i < argc; i++) {
    if (argv[i][0] != '-') {
      if (out_fname != NULL)
	fatal ("Usage: %s [switches] input output", argv[0]);
      else if (in_fname != NULL)
	out_fname = argv[i];
      else
	in_fname = argv[i];
    } else {
      switch (argv[i][1]) {

#ifdef NEXT_SEMANTICS
      case 'a':
	if (!strcmp (argv[i], "-arch"))	/* ignore -arch <archname> */
	  i++;
	break;

      case 'O':
	if (! strcmp (argv[i], "-ObjC++"))
	  ; /* no-op */
	break;
        
      case 'c':
	if (! strcmp (argv[i], "-c++-comments"))
	  cplusplus_comments = 1;
	break;
#endif

      case 'i':
	if (!strcmp (argv[i], "-include")) {
	  if (i + 1 == argc)
	    fatal ("Filename missing after `-include' option");
	  else
	    pend_includes[i] = argv[i+1], i++;
	}
#if defined (NEXT_SEMANTICS) || defined (NEXT_PDO)
	else
#endif
	if (!strcmp (argv[i], "-imacros")) {
	  if (i + 1 == argc)
	    fatal ("Filename missing after `-imacros' option");
	  else
	    pend_files[i] = argv[i+1], i++;
	}
#if defined (NEXT_SEMANTICS) || defined (NEXT_PDO)
	else
#endif
	if (!strcmp (argv[i], "-iprefix")) {
	  if (i + 1 == argc)
	    fatal ("Filename missing after `-iprefix' option");
	  else
	    include_prefix = argv[++i];
	}
#if defined (NEXT_SEMANTICS) || defined (NEXT_PDO)
	else
#endif
	if (!strcmp (argv[i], "-ifoutput")) {
	  output_conditionals = 1;
	}
#if defined (NEXT_SEMANTICS) || defined (NEXT_PDO)
	else
#endif
	if (!strcmp (argv[i], "-isystem")) {
	  struct file_name_list *dirtmp;

	  if (i + 1 == argc)
	    fatal ("Filename missing after `-isystem' option");

	  dirtmp = (struct file_name_list *)
	    xmalloc (sizeof (struct file_name_list));
	  dirtmp->next = 0;
	  dirtmp->control_macro = 0;
	  dirtmp->c_system_include_path = 1;
	  dirtmp->fname = (char *) xmalloc (strlen (argv[i+1]) + 1);
	  strcpy (dirtmp->fname, argv[++i]);
	  dirtmp->got_name_map = 0;

	  if (before_system == 0)
	    before_system = dirtmp;
	  else
	    last_before_system->next = dirtmp;
	  last_before_system = dirtmp; /* Tail follows the last one */
	}
	/* Add directory to end of path for includes,
	   with the default prefix at the front of its name.  */
#if defined (NEXT_SEMANTICS) || defined (NEXT_PDO)
	else
#endif
	if (!strcmp (argv[i], "-iwithprefix")) {
	  struct file_name_list *dirtmp;
	  char *prefix;

	  if (include_prefix != 0)
	    prefix = include_prefix;
	  else {
	    prefix = savestring (GCC_INCLUDE_DIR);
	    /* Remove the `include' from /usr/local/lib/gcc.../include.  */
	    if (!strcmp (prefix + strlen (prefix) - 8, "/include"))
	      prefix[strlen (prefix) - 7] = 0;
	  }

	  dirtmp = (struct file_name_list *)
	    xmalloc (sizeof (struct file_name_list));
	  dirtmp->next = 0;	/* New one goes on the end */
	  dirtmp->control_macro = 0;
	  dirtmp->c_system_include_path = 0;
	  if (i + 1 == argc)
	    fatal ("Directory name missing after `-iwithprefix' option");

	  dirtmp->fname = (char *) xmalloc (strlen (argv[i+1])
					    + strlen (prefix) + 1);
	  strcpy (dirtmp->fname, prefix);
	  strcat (dirtmp->fname, argv[++i]);
	  dirtmp->got_name_map = 0;

	  if (after_include == 0)
	    after_include = dirtmp;
	  else
	    last_after_include->next = dirtmp;
	  last_after_include = dirtmp; /* Tail follows the last one */
	}
	/* Add directory to main path for includes,
	   with the default prefix at the front of its name.  */
#if defined (NEXT_SEMANTICS) || defined (NEXT_PDO)
	else
#endif
	if (!strcmp (argv[i], "-iwithprefixbefore")) {
	  struct file_name_list *dirtmp;
	  char *prefix;

	  if (include_prefix != 0)
	    prefix = include_prefix;
	  else {
	    prefix = savestring (GCC_INCLUDE_DIR);
	    /* Remove the `include' from /usr/local/lib/gcc.../include.  */
	    if (!strcmp (prefix + strlen (prefix) - 8, "/include"))
	      prefix[strlen (prefix) - 7] = 0;
	  }

	  dirtmp = (struct file_name_list *)
	    xmalloc (sizeof (struct file_name_list));
	  dirtmp->next = 0;	/* New one goes on the end */
	  dirtmp->control_macro = 0;
	  dirtmp->c_system_include_path = 0;
	  if (i + 1 == argc)
	    fatal ("Directory name missing after `-iwithprefixbefore' option");

	  dirtmp->fname = (char *) xmalloc (strlen (argv[i+1])
					    + strlen (prefix) + 1);
	  strcpy (dirtmp->fname, prefix);
	  strcat (dirtmp->fname, argv[++i]);
	  dirtmp->got_name_map = 0;

	  append_include_chain (dirtmp, dirtmp);
	}
	/* Add directory to end of path for includes.  */
#if defined (NEXT_SEMANTICS) || defined (NEXT_PDO)
	else
#endif
	if (!strcmp (argv[i], "-idirafter")) {
	  struct file_name_list *dirtmp;

	  dirtmp = (struct file_name_list *)
	    xmalloc (sizeof (struct file_name_list));
	  dirtmp->next = 0;	/* New one goes on the end */
	  dirtmp->control_macro = 0;
	  dirtmp->c_system_include_path = 0;
	  if (i + 1 == argc)
	    fatal ("Directory name missing after `-idirafter' option");
	  else
	    dirtmp->fname = argv[++i];
	  dirtmp->got_name_map = 0;

	  if (after_include == 0)
	    after_include = dirtmp;
	  else
	    last_after_include->next = dirtmp;
	  last_after_include = dirtmp; /* Tail follows the last one */
	}
#ifdef NEXT_CPP_SERVER
        /* To simulate a make process which cd's into sub-directories
         * in order to use makefiles local to that directory, we need to
         * support the notion of requesting a particular working directory
         * for a preprocessing command.
         */
        else if (!strcmp (argv[i], "-in_directory")) {
           if (i + 1 == argc)
              fatal ("Filename missing after -output_headers option");
           requested_cwd = argv[++i];
        }
#endif        
#ifdef NEXT_OBJC_RUNTIME
	else if (argv[i][2] != 0)
	  pend_files[i] = argv[i] + 2;
	else if (i + 1 == argc)
	  fatal ("Filename missing after -i option");
	else
	  pend_files[i] = argv[i+1], i++;
#endif
	break;

      case 'o':
#ifdef NEXT_CPP_SERVER
        if (!strcmp (argv[i], "-output_headers")) {
           if (i + 1 == argc)
              fatal ("Filename missing after -output_headers option");
           header_fname = argv[++i];
        } else {
#endif
	if (out_fname != NULL)
	  fatal ("Output filename specified twice");
	if (i + 1 == argc)
	  fatal ("Filename missing after -o option");
	out_fname = argv[++i];
	if (!strcmp (out_fname, "-"))
	  out_fname = "";
#ifdef NEXT_CPP_SERVER
        }
#endif
	break;

      case 'p':
	if (!strcmp (argv[i], "-pedantic"))
	  pedantic = 1;
	else if (!strcmp (argv[i], "-pedantic-errors")) {
	  pedantic = 1;
	  pedantic_errors = 1;
#ifdef NEXT_PDO
        } else if (!strcmp (argv[i], "-precomp")) {
          fatal ("-precomp option not supported");
	  /* Failure to precompile a header is very nearly harmless.  */
	  exit (SUCCESS_EXIT_CODE);
#endif
	} else if (!strcmp (argv[i], "-pcp")) {
	  char *pcp_fname;
	  if (i + 1 == argc)
	    fatal ("Filename missing after -pcp option");
	  pcp_fname = argv[++i];
	  pcp_outfile = 
	    ((pcp_fname[0] != '-' || pcp_fname[1] != '\0')
	     ? fopen (pcp_fname, "w")
	     : fdopen (dup (fileno (stdout)), "w"));
	  if (pcp_outfile == 0)
#ifdef NEXT_CPP_SERVER
	    BAILOUT (pcp_fname);
#else             
	    pfatal_with_name (pcp_fname);
#endif
	  no_precomp = 1;
	}
	break;

#ifdef NEXT_SEMANTICS
       case 's':
#ifdef NEXT_PDO
 	 // ignore potential -smart option in case cpp-precomp is not installed
#else
	if (!strcmp (argv[i], "-smart"))
	  ;
        else if (!strcmp (argv[i], "-segregate-output"))
          segregate_output = 1;
#endif
	break;

      case 'm':
	if (!strcmp (argv[i], "-mark-expansions"))
	  mark_expansions = 1;
	break;
#endif

      case 't':
	if (!strcmp (argv[i], "-traditional")) {
	  traditional = 1;
	  if (dollars_in_ident > 0)
	    dollars_in_ident = 1;
	} else if (!strcmp (argv[i], "-trigraphs")) {
	  no_trigraphs = 0;
	}
	break;

      case 'l':
        if (! strcmp (argv[i], "-lang-c"))
#if defined (NEXT_SEMANTICS) || defined (NEXT_PDO)
	  cplusplus = 0, cplusplus_comments = 1, objc = 0;
#else
	  cplusplus = 0, cplusplus_comments = 0, objc = 0;
#endif
	if (! strcmp (argv[i], "-lang-c++"))
	  cplusplus = 1, cplusplus_comments = 1, objc = 0;
	if (! strcmp (argv[i], "-lang-c-c++-comments"))
	  cplusplus = 0, cplusplus_comments = 1, objc = 0;
	if (! strcmp (argv[i], "-lang-objc"))
	  objc = 1, cplusplus = 0, cplusplus_comments = 1;
	if (! strcmp (argv[i], "-lang-objc++"))
	  objc = 1, cplusplus = 1, cplusplus_comments = 1;
 	if (! strcmp (argv[i], "-lang-asm"))
 	  lang_asm = 1;
 	if (! strcmp (argv[i], "-lint"))
 	  for_lint = 1;
	break;

      case '+':
	cplusplus = 1, cplusplus_comments = 1;
	break;

      case 'w':
	inhibit_warnings = 1;
	break;

      case 'W':
	if (!strcmp (argv[i], "-Wtrigraphs"))
	  warn_trigraphs = 1;
	else if (!strcmp (argv[i], "-Wno-trigraphs"))
	  warn_trigraphs = 0;
	else if (!strcmp (argv[i], "-Wcomment"))
	  warn_comments = 1;
	else if (!strcmp (argv[i], "-Wno-comment"))
	  warn_comments = 0;
	else if (!strcmp (argv[i], "-Wcomments"))
	  warn_comments = 1;
	else if (!strcmp (argv[i], "-Wno-comments"))
	  warn_comments = 0;
	else if (!strcmp (argv[i], "-Wtraditional"))
	  warn_stringify = 1;
	else if (!strcmp (argv[i], "-Wno-traditional"))
	  warn_stringify = 0;
	else if (!strcmp (argv[i], "-Wimport"))
	  warn_import = 1;
	else if (!strcmp (argv[i], "-Wno-import"))
	  warn_import = 0;
	else if (!strcmp (argv[i], "-Werror"))
	  warnings_are_errors = 1;
	else if (!strcmp (argv[i], "-Wno-error"))
	  warnings_are_errors = 0;
#if defined (NEXT_SEMANTICS) || defined (NEXT_PDO)
	else if (!strcmp (argv[i], "-Wall") || !strcmp (argv[i], "-Wmost"))
#else
	else if (!strcmp (argv[i], "-Wall"))
#endif
	  {
	    warn_trigraphs = 1;
#if defined (NEXT_PDO) && !defined (_WIN32)
	    if (!strcmp (argv[i], "-Wmost"))
	    warn_comments = 0;
#else
	    warn_comments = 1;
#endif
	  }
	break;

      case 'M':
	/* The style of the choices here is a bit mixed.
	   The chosen scheme is a hybrid of keeping all options in one string
	   and specifying each option in a separate argument:
	   -M|-MM|-MD file|-MMD file [-MG].  An alternative is:
	   -M|-MM|-MD file|-MMD file|-MG|-MMG; or more concisely:
	   -M[M][G][D file].  This is awkward to handle in specs, and is not
	   as extensible.  */
	/* ??? -MG must be specified in addition to one of -M or -MM.
	   This can be relaxed in the future without breaking anything.
	   The converse isn't true.  */

	/* -MG isn't valid with -MD or -MMD.  This is checked for later.  */
	if (!strcmp (argv[i], "-MG"))
	  {
	    print_deps_missing_files = 1;
	    break;
	  }
	if (!strcmp (argv[i], "-M"))
	  print_deps = 2;
	else if (!strcmp (argv[i], "-MM"))
	  print_deps = 1;
	else if (!strcmp (argv[i], "-MD"))
	  print_deps = 2;
	else if (!strcmp (argv[i], "-MMD"))
	  print_deps = 1;
	/* For -MD and -MMD options, write deps on file named by next arg.  */
	if (!strcmp (argv[i], "-MD")
	    || !strcmp (argv[i], "-MMD")) {
	  if (i + 1 == argc)
	    fatal ("Filename missing after %s option", argv[i]);
	  i++;
	  deps_file = argv[i];
	  deps_mode = "w";
	} else {
	  /* For -M and -MM, write deps on standard output
	     and suppress the usual output.  */
	  deps_stream = stdout;
	  inhibit_output = 1;
	}	  
	break;

      case 'd':
	{
	  char *p = argv[i] + 2;
	  char c;
	  while (c = *p++) {
	    /* Arg to -d specifies what parts of macros to dump */
	    switch (c) {
	    case 'M':
	      dump_macros = dump_only;
	      no_output = 1;
	      break;
	    case 'N':
	      dump_macros = dump_names;
	      break;
	    case 'D':
	      dump_macros = dump_definitions;
	      break;
	    }
	  }
	}
	break;

      case 'g':
	if (argv[i][2] == '3')
	  debug_output = 1;
	break;

      case 'v':
	fprintf (stderr, "GNU CPP version %s", "2.6.3");
#ifdef TARGET_VERSION
	TARGET_VERSION;
#endif
	fprintf (stderr, "\n");
	verbose = 1;
	break;

      case 'H':
	print_include_names = 1;
	break;

      case 'D':
	if (argv[i][2] != 0)
	  pend_defs[i] = argv[i] + 2;
	else if (i + 1 == argc)
	  fatal ("Macro name missing after -D option");
	else
	  i++, pend_defs[i] = argv[i];
	break;

      case 'A':
	{
	  char *p;

	  if (argv[i][2] != 0)
	    p = argv[i] + 2;
	  else if (i + 1 == argc)
	    fatal ("Assertion missing after -A option");
	  else
	    p = argv[++i];

	  if (!strcmp (p, "-")) {
	    /* -A- eliminates all predefined macros and assertions.
	       Let's include also any that were specified earlier
	       on the command line.  That way we can get rid of any
	       that were passed automatically in from GCC.  */
	    int j;
	    inhibit_predefs = 1;
	    for (j = 0; j < i; j++)
	      pend_defs[j] = pend_assertions[j] = 0;
	  } else {
	    pend_assertions[i] = p;
	    pend_assertion_options[i] = "-A";
	  }
	}
	break;

      case 'U':		/* JF #undef something */
	if (argv[i][2] != 0)
	  pend_undefs[i] = argv[i] + 2;
	else if (i + 1 == argc)
	  fatal ("Macro name missing after -U option");
	else
	  pend_undefs[i] = argv[i+1], i++;
	break;

      case 'C':
	put_out_comments = 1;
	break;

      case 'E':			/* -E comes from cc -E; ignore it.  */
	break;

      case 'P':
	no_line_commands = 1;
	break;

      case '$':			/* Don't include $ in identifiers.  */
	dollars_in_ident = 0;
	break;

      case 'I':			/* Add directory to path for includes.  */
	{
	  struct file_name_list *dirtmp;

	  if (! ignore_srcdir && !strcmp (argv[i] + 2, "-")) {
	    ignore_srcdir = 1;
	    /* Don't use any preceding -I directories for #include <...>.  */
	    first_bracket_include = 0;
	  }
	  else {
	    dirtmp = (struct file_name_list *)
	      xmalloc (sizeof (struct file_name_list));
	    dirtmp->next = 0;		/* New one goes on the end */
	    dirtmp->control_macro = 0;
	    dirtmp->c_system_include_path = 0;
	    if (argv[i][2] != 0)
	      dirtmp->fname = argv[i] + 2;
	    else if (i + 1 == argc)
	      fatal ("Directory name missing after -I option");
	    else
	      dirtmp->fname = argv[++i];
	    dirtmp->got_name_map = 0;
	    append_include_chain (dirtmp, dirtmp);
	  }
	}
	break;

#ifdef NEXT_FRAMEWORKS
  case 'F':
  {
      struct file_name_list *dirtmp;
      if (! ignore_srcdir && !strcmp (argv[i] + 2, "-")) {
          ignore_srcdir = 1;
          /* Don't use any preceding -F directories for #include <...>.  */
          first_bracket_framework = 0;
      }
      else {
          dirtmp = (struct file_name_list *)
              xmalloc (sizeof (struct file_name_list));
          dirtmp->next = 0;		/* New one goes on the end */
          dirtmp->control_macro = 0;
          dirtmp->c_system_include_path = 0;
          if (argv[i][2] != 0)
              dirtmp->fname = argv[i] + 2;
          else if (i + 1 == argc)
              fatal ("Directory name missing after -I option");
          else
              dirtmp->fname = argv[++i];
          dirtmp->got_name_map = 0;
          append_framework_chain (dirtmp, dirtmp);
      }
  }
    break;
#endif

      case 'n':
	if (!strcmp (argv[i], "-nostdinc"))
	  /* -nostdinc causes no default include directories.
	     You must specify all include-file directories with -I.  */
	  no_standard_includes = 1;
	else if (!strcmp (argv[i], "-nostdinc++"))
	  /* -nostdinc++ causes no default C++-specific include directories. */
	  no_standard_cplusplus_includes = 1;
	else if (!strcmp (argv[i], "-noprecomp"))
	  no_precomp = 1;
#if defined (NEXT_SEMANTICS) || defined (NEXT_PDO)
	if (!strcmp (argv[i], "-no-c++-comments"))
	  cplusplus_comments = 0;
#endif
	break;

      case 'u':
	/* Sun compiler passes undocumented switch "-undef".
	   Let's assume it means to inhibit the predefined symbols.  */
	inhibit_predefs = 1;
	break;

      case '\0': /* JF handle '-' as file name meaning stdin or stdout */
	if (in_fname == NULL) {
	  in_fname = "";
	  break;
	} else if (out_fname == NULL) {
	  out_fname = "";
	  break;
	}	/* else fall through into error */

      default:
	fatal ("Invalid option `%s'", argv[i]);
      }
    }
  }

#ifdef NEXT_CPP_SERVER
  if (serverized && requested_cwd && strcmp(actual_cwd, requested_cwd) != 0) {
     if (chdir(requested_cwd) == 0)
        strcpy(actual_cwd, requested_cwd);
  }
#endif

  /* Add dirs from CPATH after dirs from -I.  */
  /* There seems to be confusion about what CPATH should do,
     so for the moment it is not documented.  */
  /* Some people say that CPATH should replace the standard include dirs,
     but that seems pointless: it comes before them, so it overrides them
     anyway.  */
#if defined (WINNT) && !defined (NEXT_PDO)
  p = (char *) getenv ("Include");
#else
  p = (char *) getenv ("CPATH");
#endif
  if (p != 0 && ! no_standard_includes)
    path_include (p);

  /* Now that dollars_in_ident is known, initialize is_idchar.  */
  initialize_char_syntax ();

#ifdef NEXT_CPP_SERVER
  if (timings)
     start();
#else
  /* Initialize output buffer */

  outbuf.buf = (U_CHAR *) xmalloc (OUTBUF_SIZE);
  outbuf.bufp = outbuf.buf;
  outbuf.length = OUTBUF_SIZE;
#endif

  /* Do partial setup of input buffer for the sake of generating
     early #line directives (when -g is in effect).  */

  fp = &instack[++indepth];
  if (in_fname == NULL)
    in_fname = "";
  fp->nominal_fname = fp->fname = in_fname;
  fp->lineno = 0;

  /* In C++, wchar_t is a distinct basic type, and we can expect
     __wchar_t to be defined by cc1plus.  */
  if (cplusplus)
    wchar_type = "__wchar_t";

  /* Install __LINE__, etc.  Must follow initialize_char_syntax
     and option processing.  */
  initialize_builtins (fp, &outbuf);
#if defined (NEXT_SEMANTICS) || defined (NEXT_PDO)
  /* FIXME: does this malloc every time? */
#endif

  /* Do standard #defines and assertions
     that identify system and machine type.  */

#if defined (NEXT_SEMANTICS) || defined (NEXT_PDO)
  /* Normally, the compiler driver passes -undef, which causes this 
     clause to be skipped (i.e. inhibit_predefs == 1). If being run
     standalone, this clause is responsible for installing the predefined
     macros. At present, this cpp has this info compiled into it, rather
     than reading a specs file for each architecture (which is what the NeXT 
     compiler driver does) */
#endif

  if (!inhibit_predefs) {
    char *p = (char *) alloca (strlen (predefs) + 1);
    strcpy (p, predefs);
    while (*p) {
      char *q;
      while (*p == ' ' || *p == '\t')
	p++;
      /* Handle -D options.  */ 
      if (p[0] == '-' && p[1] == 'D') {
	q = &p[2];
	while (*p && *p != ' ' && *p != '\t')
	  p++;
	if (*p != 0)
	  *p++= 0;
	if (debug_output)
	  output_line_command (fp, &outbuf, 0, same_file);
	make_definition (q, &outbuf);
	while (*p == ' ' || *p == '\t')
	  p++;
      } else if (p[0] == '-' && p[1] == 'A') {
	/* Handle -A options (assertions).  */ 
	char *assertion;
	char *past_name;
	char *value;
	char *past_value;
	char *termination;
	int save_char;

	assertion = &p[2];
	past_name = assertion;
	/* Locate end of name.  */
	while (*past_name && *past_name != ' '
	       && *past_name != '\t' && *past_name != '(')
	  past_name++;
	/* Locate `(' at start of value.  */
	value = past_name;
	while (*value && (*value == ' ' || *value == '\t'))
	  value++;
	if (*value++ != '(')
	  abort ();
	while (*value && (*value == ' ' || *value == '\t'))
	  value++;
	past_value = value;
	/* Locate end of value.  */
	while (*past_value && *past_value != ' '
	       && *past_value != '\t' && *past_value != ')')
	  past_value++;
	termination = past_value;
	while (*termination && (*termination == ' ' || *termination == '\t'))
	  termination++;
	if (*termination++ != ')')
	  abort ();
	if (*termination && *termination != ' ' && *termination != '\t')
	  abort ();
	/* Temporarily null-terminate the value.  */
	save_char = *termination;
	*termination = '\0';
	/* Install the assertion.  */
	make_assertion ("-A", assertion);
	*termination = (char) save_char;
	p = termination;
	while (*p == ' ' || *p == '\t')
	  p++;
      } else {
	abort ();
      }
    }
  }

  /* Now handle the command line options.  */

  /* Do -U's, -D's and -A's in the order they were seen.  */
  for (i = 1; i < argc; i++) {
    if (pend_undefs[i]) {
      if (debug_output)
        output_line_command (fp, &outbuf, 0, same_file);
      make_undef (pend_undefs[i], &outbuf);
    }
    if (pend_defs[i]) {
      if (debug_output)
        output_line_command (fp, &outbuf, 0, same_file);
      make_definition (pend_defs[i], &outbuf);
    }
    if (pend_assertions[i])
      make_assertion (pend_assertion_options[i], pend_assertions[i]);
  }

  done_initializing = 1;

  { /* read the appropriate environment variable and if it exists
       replace include_defaults with the listed path. */
    char *epath = 0;
    switch ((objc << 1) + cplusplus)
      {
      case 0:
	epath = getenv ("C_INCLUDE_PATH");
	break;
      case 1:
	epath = getenv ("CPLUS_INCLUDE_PATH");
	break;
      case 2:
	epath = getenv ("OBJC_INCLUDE_PATH");
	break;
      case 3:
	epath = getenv ("OBJCPLUS_INCLUDE_PATH");
	break;
      }
    /* If the environment var for this language is set,
       add to the default list of include directories.  */
    if (epath) {
      char *nstore = (char *) alloca (strlen (epath) + 2);
      int num_dirs;
      char *startp, *endp;

      for (num_dirs = 1, startp = epath; *startp; startp++)
	if (*startp == PATH_SEPARATOR)
	  num_dirs++;
      include_defaults
	= (struct default_include *) xmalloc ((num_dirs
					       * sizeof (struct default_include))
					      + sizeof (include_defaults_array));
      startp = endp = epath;
      num_dirs = 0;
      while (1) {
        /* Handle cases like c:/usr/lib:d:/gcc/lib */
        if ((*endp == PATH_SEPARATOR
#if 0 /* Obsolete, now that we use semicolons as the path separator.  */
#ifdef __MSDOS__
	     && (endp-startp != 1 || !isalpha (*startp))
#endif
#endif
	     )
            || *endp == 0) {
	  strncpy (nstore, startp, endp-startp);
	  if (endp == startp)
	    strcpy (nstore, ".");
	  else
	    nstore[endp-startp] = '\0';

	  include_defaults[num_dirs].fname = savestring (nstore);
	  include_defaults[num_dirs].cplusplus = cplusplus;
	  include_defaults[num_dirs].cxx_aware = 1;
	  num_dirs++;
	  if (*endp == '\0')
	    break;
	  endp = startp = endp + 1;
	} else
	  endp++;
      }
      /* Put the usual defaults back in at the end.  */
      bcopy ((char *) include_defaults_array,
	     (char *) &include_defaults[num_dirs],
	     sizeof (include_defaults_array));
    }
  }

#if defined (NEXT_SEMANTICS) || defined (NEXT_PDO)
  {
    struct default_include *fix_includes = include_defaults;
    char* next_root = getenv ("NEXT_ROOT");
    int i = 0;
    if (next_root && next_root[strlen (next_root) - 1] == '/')
      next_root[strlen (next_root) - 1] = '\0';
    if (next_root && *next_root)
      {
	while (fix_includes[i].fname)
	  {
	    char* new_fname =
	      (char *) xmalloc (strlen (next_root)
				+ strlen (fix_includes[i].fname) + 1);
	    sprintf (new_fname, "%s%s", next_root, fix_includes[i].fname);
	    if (new_fname && *new_fname) 
	      fix_includes[i].fname = new_fname;
	    i++;
	  }
      }
  }
#endif /* NEXT_SEMANTICS || NEXT_PDO */

  append_include_chain (before_system, last_before_system);
  first_system_include = before_system;

  /* Unless -fnostdinc,
     tack on the standard include file dirs to the specified list */
  if (!no_standard_includes) {
#ifdef NEXT_FRAMEWORKS
    int hdr;
#endif
    struct default_include *p = include_defaults;
    char *specd_prefix = include_prefix;
    char *default_prefix = savestring (GCC_INCLUDE_DIR);
    int default_len = 0;
    /* Remove the `include' from /usr/local/lib/gcc.../include.  */
    if (!strcmp (default_prefix + strlen (default_prefix) - 8, "/include")) {
      default_len = strlen (default_prefix) - 7;
      default_prefix[default_len] = 0;
    }
    /* Search "translated" versions of GNU directories.
       These have /usr/local/lib/gcc... replaced by specd_prefix.  */
    if (specd_prefix != 0 && default_len != 0)
      for (p = include_defaults; p->fname; p++) {
	/* Some standard dirs are only for C++.  */
	if (!p->cplusplus || (cplusplus && !no_standard_cplusplus_includes)) {
	  /* Does this dir start with the prefix?  */
	  if (!strncmp (p->fname, default_prefix, default_len)) {
	    /* Yes; change prefix and add to search list.  */
	    struct file_name_list *new
	      = (struct file_name_list *) xmalloc (sizeof (struct file_name_list));
	    int this_len = strlen (specd_prefix) + strlen (p->fname) - default_len;
	    char *str = (char *) xmalloc (this_len + 1);
	    strcpy (str, specd_prefix);
	    strcat (str, p->fname + default_len);
	    new->fname = str;
	    new->control_macro = 0;
	    new->c_system_include_path = !p->cxx_aware;
	    new->got_name_map = 0;
	    append_include_chain (new, new);
	    if (first_system_include == 0)
	      first_system_include = new;
	  }
	}
      }
    /* Search ordinary names for GNU include directories.  */
    for (p = include_defaults; p->fname; p++) {
      /* Some standard dirs are only for C++.  */
      if (!p->cplusplus || (cplusplus && !no_standard_cplusplus_includes)) {
	struct file_name_list *new
	  = (struct file_name_list *) xmalloc (sizeof (struct file_name_list));
	new->control_macro = 0;
	new->c_system_include_path = !p->cxx_aware;
	new->fname = p->fname;
	new->got_name_map = 0;
	append_include_chain (new, new);
	if (first_system_include == 0)
	  first_system_include = new;
      }
    }
#ifdef NEXT_FRAMEWORKS
    for (hdr=0; framework_defaults_array[hdr].fname; hdr++) {
      struct stat dummy;
      char *next_root = getenv ("NEXT_ROOT");
      if (next_root)
	{
	  char *new_name =
	    (char *)xmalloc (strlen (next_root)
			     + strlen (framework_defaults_array[hdr].fname)+1);
	  if (next_root[strlen (next_root)-1] == '/')
	    next_root[strlen(next_root)-1] = '\0';
	  sprintf (new_name, "%s%s", next_root,
		   framework_defaults_array[hdr].fname);
	  framework_defaults_array[hdr].fname = new_name;
	}
       if (stat(framework_defaults_array[hdr].fname, &dummy) == 0) {
          /* Framework directory actually exists  */
          struct file_name_list *new
             = (struct file_name_list *) xmalloc (sizeof (struct file_name_list));
          new->control_macro = 0;
          new->c_system_include_path = !framework_defaults_array[hdr].cxx_aware;
          new->fname = framework_defaults_array[hdr].fname;
          new->got_name_map = 0;
          append_framework_chain (new, new);
       }
    }
    free (default_prefix);
#endif
  }

  /* Tack the after_include chain at the end of the include chain.  */
  append_include_chain (after_include, last_after_include);
  if (first_system_include == 0)
    first_system_include = after_include;

  /* With -v, print the list of dirs to search.  */
  if (verbose) {
    struct file_name_list *p;
    fprintf (stderr, "#include \"...\" search starts here:\n");
    for (p = include; p; p = p->next) {
      if (p == first_bracket_include)
	fprintf (stderr, "#include <...> search starts here:\n");
      fprintf (stderr, " %s\n", p->fname);
    }
    fprintf (stderr, "End of search list.\n");
  }

  /* Scan the -imacros files before the main input.
     Much like #including them, but with no_output set
     so that only their macro definitions matter.  */

  no_output++; no_record_file++;
  for (i = 1; i < argc; i++)
    if (pend_files[i]) {
      int fd = open (pend_files[i], O_RDONLY, 0666);
      if (fd < 0) {
	perror_with_name (pend_files[i]);
#ifdef NEXT_CPP_SERVER
        FAILURE;
#else
	return FAILURE_EXIT_CODE;
#endif	
      }
#ifdef NEXT_CPP_SERVER
      finclude (fd, pend_files[i], &outbuf, 0, 0, 0, 0);
#else
      finclude (fd, pend_files[i], &outbuf, 0, NULL_PTR);
#endif
    }
  no_output--; no_record_file--;

  /* Copy the entire contents of the main input file into
     the stacked input buffer previously allocated for it.  */

  /* JF check for stdin */
  if (in_fname == NULL || *in_fname == 0) {
    in_fname = "";
    f = 0;
  } else if ((f = open (in_fname, O_RDONLY, 0666)) < 0)
#ifdef NEXT_CPP_SERVER
    BAILOUT (in_fname);
#else
    goto perror;
#endif

  /* -MG doesn't select the form of output and must be specified with one of
     -M or -MM.  -MG doesn't make sense with -MD or -MMD since they don't
     inhibit compilation.  */
  if (print_deps_missing_files && (print_deps == 0 || !inhibit_output))
    fatal ("-MG must be specified with one of -M or -MM");

  /* Either of two environment variables can specify output of deps.
     Its value is either "OUTPUT_FILE" or "OUTPUT_FILE DEPS_TARGET",
     where OUTPUT_FILE is the file to write deps info to
     and DEPS_TARGET is the target to mention in the deps.  */

  if (print_deps == 0
      && (getenv ("SUNPRO_DEPENDENCIES") != 0
	  || getenv ("DEPENDENCIES_OUTPUT") != 0)) {
    char *spec = getenv ("DEPENDENCIES_OUTPUT");
    char *s;
    char *output_file;

    if (spec == 0) {
      spec = getenv ("SUNPRO_DEPENDENCIES");
      print_deps = 2;
    }
    else
      print_deps = 1;

    s = spec;
    /* Find the space before the DEPS_TARGET, if there is one.  */
    /* This should use index.  (mrs) */
    while (*s != 0 && *s != ' ') s++;
    if (*s != 0) {
      deps_target = s + 1;
      output_file = (char *) xmalloc (s - spec + 1);
      bcopy (spec, output_file, s - spec);
      output_file[s - spec] = 0;
    }
    else {
      deps_target = 0;
      output_file = spec;
    }
      
    deps_file = output_file;
    deps_mode = "a";
  }

  /* For -M, print the expected object file name
     as the target of this Make-rule.  */
  if (print_deps) {
    deps_allocated_size = 200;
    deps_buffer = (char *) xmalloc (deps_allocated_size);
    deps_buffer[0] = 0;
    deps_size = 0;
    deps_column = 0;

    if (deps_target) {
      deps_output (deps_target, ':');
    } else if (*in_fname == 0) {
      deps_output ("-", ':');
    } else {
      char *p, *q;
      int len;

      /* Discard all directory prefixes from filename.  */
      if ((q = rindex (in_fname, '/')) != NULL
#ifdef DIR_SEPARATOR
	  && (q = rindex (in_fname, DIR_SEPARATOR)) != NULL
#endif
	  )
	++q;
      else
	q = in_fname;

      /* Copy remainder to mungable area.  */
      p = (char *) alloca (strlen(q) + 8);
      strcpy (p, q);

      /* Output P, but remove known suffixes.  */
      len = strlen (p);
      q = p + len;
      if (len >= 2
	  && p[len - 2] == '.'
	  && index("cCsSmM", p[len - 1]))
	q = p + (len - 2);
      else if (len >= 3
	       && p[len - 3] == '.'
	       && p[len - 2] == 'c'
	       && p[len - 1] == 'c')
	q = p + (len - 3);
      else if (len >= 4
	       && p[len - 4] == '.'
	       && p[len - 3] == 'c'
	       && p[len - 2] == 'x'
	       && p[len - 1] == 'x')
	q = p + (len - 4);
      else if (len >= 4
	       && p[len - 4] == '.'
	       && p[len - 3] == 'c'
	       && p[len - 2] == 'p'
	       && p[len - 1] == 'p')
	q = p + (len - 4);
#if defined (NEXT_SEMANTICS) || defined (NEXT_PDO)
      else if (len >= 3
	       && p[len - 3] == '.'
	       && p[len - 2] == 'm'
	       && p[len - 1] == 'm')
	q = p + (len - 3);
#endif

      /* Supply our own suffix.  */
#ifndef VMS
      strcpy (q, ".o");
#else
      strcpy (q, ".obj");
#endif

      deps_output (p, ':');
      deps_output (in_fname, ' ');
    }
  }

  file_size_and_mode (f, &st_mode, &st_size);
  fp->nominal_fname = fp->fname = in_fname;
  fp->lineno = 1;
  fp->system_header_p = 0;
  /* JF all this is mine about reading pipes and ttys */
  if (! S_ISREG (st_mode)) {
    /* Read input from a file that is not a normal disk file.
       We cannot preallocate a buffer with the correct size,
       so we must read in the file a piece at the time and make it bigger.  */
    int size;
    int bsize;
    int cnt;

    bsize = 2000;
    size = 0;
    fp->buf = (U_CHAR *) xmalloc (bsize + 2);
    for (;;) {
      cnt = safe_read (f, fp->buf + size, bsize - size);
#ifdef NEXT_CPP_SERVER
      if (cnt < 0) BAILOUT (in_fname);
      if (cnt == 0) break;	/* End of file */
#else
      if (cnt < 0) goto perror;	/* error! */
#endif
      size += cnt;
      if (size != bsize) break;	/* End of file */
      bsize *= 2;
      fp->buf = (U_CHAR *) xrealloc (fp->buf, bsize + 2);
    }
    fp->length = size;
  } else {
    /* Read a file whose size we can determine in advance.
       For the sake of VMS, st_size is just an upper bound.  */
    fp->buf = (U_CHAR *) xmalloc (st_size + 2);
    fp->length = safe_read (f, fp->buf, st_size);
    if (fp->length < 0) 
#ifdef NEXT_CPP_SERVER
        BAILOUT (in_fname);
#else
      goto perror;
#endif
  }
  fp->bufp = fp->buf;
  fp->if_stack = if_stack;

  /* Make sure data ends with a newline.  And put a null after it.  */

  if ((fp->length > 0 && fp->buf[fp->length - 1] != '\n')
      /* Backslash-newline at end is not good enough.  */
      || (fp->length > 1 && fp->buf[fp->length - 2] == '\\')) {
    fp->buf[fp->length++] = '\n';
    missing_newline = 1;
  }
  fp->buf[fp->length] = '\0';

  /* Unless inhibited, convert trigraphs in the input.  */

  if (!no_trigraphs)
    trigraph_pcp (fp);

#ifdef NEXT_SEMANTICS
  if (!no_rtf)
    buf_convert_rtf (fp);
#endif

  /* Now that we know the input file is valid, open the output.  */

#ifdef NEXT_CPP_SERVER
  if (!out_fname || !strcmp (out_fname, "") || !strcmp (out_fname, "stdout"))
    if (serverized)
       fatal("Can't output to stdout when serverized.");
    else
#else
  if (!out_fname || !strcmp (out_fname, ""))
#endif
    out_fname = "stdout";
#ifndef NEXT_CPP_SERVER
  else if (! freopen (out_fname, "w", stdout))
    pfatal_with_name (out_fname);
#endif

  output_line_command (fp, &outbuf, 0, same_file);

  /* Scan the -include files before the main input.  */

  no_record_file++;
  for (i = 1; i < argc; i++)
    if (pend_includes[i]) {
      int fd = open (pend_includes[i], O_RDONLY, 0666);
      if (fd < 0) {
	perror_with_name (pend_includes[i]);
#ifdef NEXT_CPP_SERVER
        FAILURE;
#else
	return FAILURE_EXIT_CODE;
#endif	
      }
#ifdef NEXT_CPP_SERVER
      finclude (fd, pend_includes[i], &outbuf, 0, 0, 0, 0);
#else
      finclude (fd, pend_includes[i], &outbuf, 0, NULL_PTR);
#endif
    }
  no_record_file--;

  /* Scan the input, processing macros and directives.  */

  rescan (&outbuf, 0);

  if (missing_newline)
    fp->lineno--;

  if (pedantic && missing_newline)
    pedwarn ("file does not end in newline");

  /* Now we have processed the entire input
     Write whichever kind of output has been requested.  */

  if (dump_macros == dump_only)
    dump_all_macros ();
  else if (! inhibit_output) {
#ifdef NEXT_CPP_SERVER
    write_output (&outbuf, out_fname, 1);
#else
    write_output ();
#endif
  }

  if (print_deps) {
    /* Don't actually write the deps file if compilation has failed.  */
    if (errors == 0) {
      if (deps_file && ! (deps_stream = fopen (deps_file, deps_mode)))
#ifdef NEXT_CPP_SERVER
	BAILOUT (deps_file);
#else
	pfatal_with_name (deps_file);
#endif
      fputs (deps_buffer, deps_stream);
      putc ('\n', deps_stream);
      if (deps_file) {
	if (ferror (deps_stream) || fclose (deps_stream) != 0)
	  fatal ("I/O error on output");
      }
    }
  }

#ifdef NEXT_CPP_SERVER
finishRequest:
  if (serverized) {
     int wrote_headers = 0;
     if (header_fname)
        wrote_headers =
                   write_header_output (header_fname, &sys_header_outbuf, 0, 0);       
    if (timings) {
       stop();
       fprintf(stderr, "Preprocessed %s%s into %s\n  in ",
               wrote_headers ? "(loading new headers) " : "",
               in_fname, out_fname);
       display(stderr, 1);
    } else {
       fprintf(stderr, "Preprocessed %s%s into %s\n",
               wrote_headers ? "(loading new headers) " : "",
               in_fname, out_fname);
    }  
    fflush(stderr);

    /* Reinitialize include/import stack and macros */
    indepth = -1;
    clear_all_macros();
    clear_import();

    /* Reinitialize output buffer */
    outbuf.bufp = outbuf.buf;
    bzero (outbuf.bufp, outbuf.length);
    outbuf.lineno = 0;

    close (f);
    
    /* Reinitialize input buffer */
    free (fp->buf);
    fp->buf = fp->bufp = NULL;

    free_undefined_macros = 0;

    /* Clear up old -I paths, which may be different with each cpp command */
    include_ptr = include;
    while (include_ptr) {
       struct file_name_list * next_include = include_ptr->next;
       free (include_ptr);
       include_ptr = next_include;  
    }
    first_bracket_include = 0;
    first_system_include = 0;
    include = 0;	
    last_include = 0;
    after_include = 0;
    last_after_include = 0;

#ifdef NEXT_FRAMEWORKS
    /* Clear up old -F paths, which also could vary by cpp request */
    include_ptr = framework;
    while (include_ptr) {
       struct file_name_list * next_include = include_ptr->next;
       free (include_ptr);
       include_ptr = next_include;
    }
    first_bracket_framework = 0;
    framework = 0;	
    last_framework = 0;
#endif
    
    done_initializing = 0;  
    goto processCommand;
  }
#endif

  if (pcp_outfile && pcp_outfile != stdout
      && (ferror (pcp_outfile) || fclose (pcp_outfile) != 0))
    fatal ("I/O error on `-pcp' output");

  if (ferror (stdout) || fclose (stdout) != 0)
    fatal ("I/O error on output");

  if (errors)
    exit (FAILURE_EXIT_CODE);
  exit (SUCCESS_EXIT_CODE);

#ifndef NEXT_CPP_SERVER /* This insures there are no random goto's out of the main server loop */
 perror:
#endif
  pfatal_with_name (in_fname);
  return 0;
}

/* Given a colon-separated list of file names PATH,
   add all the names to the search path for include files.  */

static void
path_include (path)
     char *path;
{
  char *p;

  p = path;

  if (*p)
    while (1) {
      char *q = p;
      char *name;
      struct file_name_list *dirtmp;

      /* Find the end of this name.  */
      while (*q != 0 && *q != PATH_SEPARATOR) q++;
      if (p == q) {
	/* An empty name in the path stands for the current directory.  */
	name = (char *) xmalloc (2);
	name[0] = '.';
	name[1] = 0;
      } else {
	/* Otherwise use the directory that is named.  */
	name = (char *) xmalloc (q - p + 1);
	bcopy (p, name, q - p);
	name[q - p] = 0;
      }

      dirtmp = (struct file_name_list *)
	xmalloc (sizeof (struct file_name_list));
      dirtmp->next = 0;		/* New one goes on the end */
      dirtmp->control_macro = 0;
      dirtmp->c_system_include_path = 0;
      dirtmp->fname = name;
      dirtmp->got_name_map = 0;
      append_include_chain (dirtmp, dirtmp);

      /* Advance past this name.  */
      p = q;
      if (*p == 0)
	break;
      /* Skip the colon.  */
      p++;
    }
}

/* Return the address of the first character in S that equals C.
   S is an array of length N, possibly containing '\0's, and followed by '\0'.
   Return 0 if there is no such character.  Assume that C itself is not '\0'.
   If we knew we could use memchr, we could just invoke memchr (S, C, N),
   but unfortunately memchr isn't autoconfigured yet.  */

static U_CHAR *
index0 (s, c, n)
     U_CHAR *s;
     int c;
     int n;
{
  for (;;) {
    char *q = index (s, c);
    if (q)
      return (U_CHAR *) q;
    else {
      int l = strlen (s);
      if (l == n)
	return 0;
      l++;
      s += l;
      n -= l;
    }
  }
}

/* Pre-C-Preprocessor to translate ANSI trigraph idiocy in BUF
   before main CCCP processing.  Name `pcp' is also in honor of the
   drugs the trigraph designers must have been on.

   Using an extra pass through the buffer takes a little extra time,
   but is infinitely less hairy than trying to handle trigraphs inside
   strings, etc. everywhere, and also makes sure that trigraphs are
   only translated in the top level of processing. */

static void
trigraph_pcp (buf)
     FILE_BUF *buf;
{
  register U_CHAR c, *fptr, *bptr, *sptr, *lptr;
  int len;

  fptr = bptr = sptr = buf->buf;
  lptr = fptr + buf->length;
  while ((sptr = (U_CHAR *) index0 (sptr, '?', lptr - sptr)) != NULL) {
    if (*++sptr != '?')
      continue;
    switch (*++sptr) {
      case '=':
      c = '#';
      break;
    case '(':
      c = '[';
      break;
    case '/':
      c = '\\';
      break;
    case ')':
      c = ']';
      break;
    case '\'':
      c = '^';
      break;
    case '<':
      c = '{';
      break;
    case '!':
      c = '|';
      break;
    case '>':
      c = '}';
      break;
    case '-':
      c  = '~';
      break;
    case '?':
      sptr--;
      continue;
    default:
      continue;
    }
    len = sptr - fptr - 2;

    /* BSD doc says bcopy () works right for overlapping strings.  In ANSI
       C, this will be memmove (). */
    if (bptr != fptr && len > 0)
      bcopy ((char *) fptr, (char *) bptr, len);

    bptr += len;
    *bptr++ = c;
    fptr = ++sptr;
  }
  len = buf->length - (fptr - buf->buf);
  if (bptr != fptr && len > 0)
    bcopy ((char *) fptr, (char *) bptr, len);
  buf->length -= fptr - bptr;
  buf->buf[buf->length] = '\0';
  if (warn_trigraphs && fptr != bptr)
    warning ("%d trigraph(s) encountered", (fptr - bptr) / 2);
}

/* Move all backslash-newline pairs out of embarrassing places.
   Exchange all such pairs following BP
   with any potentially-embarrassing characters that follow them.
   Potentially-embarrassing characters are / and *
   (because a backslash-newline inside a comment delimiter
   would cause it not to be recognized).  */

static void
newline_fix (bp)
     U_CHAR *bp;
{
  register U_CHAR *p = bp;

  /* First count the backslash-newline pairs here.  */

  while (p[0] == '\\' && p[1] == '\n')
    p += 2;

  /* What follows the backslash-newlines is not embarrassing.  */

  if (*p != '/' && *p != '*')
    return;

  /* Copy all potentially embarrassing characters
     that follow the backslash-newline pairs
     down to where the pairs originally started.  */

  while (*p == '*' || *p == '/')
    *bp++ = *p++;

  /* Now write the same number of pairs after the embarrassing chars.  */
  while (bp < p) {
    *bp++ = '\\';
    *bp++ = '\n';
  }
}

/* Like newline_fix but for use within a directive-name.
   Move any backslash-newlines up past any following symbol constituents.  */

static void
name_newline_fix (bp)
     U_CHAR *bp;
{
  register U_CHAR *p = bp;

  /* First count the backslash-newline pairs here.  */
  while (p[0] == '\\' && p[1] == '\n')
    p += 2;

  /* What follows the backslash-newlines is not embarrassing.  */

  if (!is_idchar[*p])
    return;

  /* Copy all potentially embarrassing characters
     that follow the backslash-newline pairs
     down to where the pairs originally started.  */

  while (is_idchar[*p])
    *bp++ = *p++;

  /* Now write the same number of pairs after the embarrassing chars.  */
  while (bp < p) {
    *bp++ = '\\';
    *bp++ = '\n';
  }
}

/* Look for lint commands in comments.

   When we come in here, ibp points into a comment.  Limit is as one expects.
   scan within the comment -- it should start, after lwsp, with a lint command.
   If so that command is returned as a (constant) string.

   Upon return, any arg will be pointed to with argstart and will be
   arglen long.  Note that we don't parse that arg since it will just
   be printed out again.
*/

static char *
get_lintcmd (ibp, limit, argstart, arglen, cmdlen)
     register U_CHAR *ibp;
     register U_CHAR *limit;
     U_CHAR **argstart;		/* point to command arg */
     int *arglen, *cmdlen;	/* how long they are */
{
  long linsize;
  register U_CHAR *numptr;	/* temp for arg parsing */

  *arglen = 0;

  SKIP_WHITE_SPACE (ibp);

  if (ibp >= limit) return NULL;

  linsize = limit - ibp;
  
  /* Oh, I wish C had lexical functions... hell, I'll just open-code the set */
  if ((linsize >= 10) && !strncmp (ibp, "NOTREACHED", 10)) {
    *cmdlen = 10;
    return "NOTREACHED";
  }
  if ((linsize >= 8) && !strncmp (ibp, "ARGSUSED", 8)) {
    *cmdlen = 8;
    return "ARGSUSED";
  }
  if ((linsize >= 11) && !strncmp (ibp, "LINTLIBRARY", 11)) {
    *cmdlen = 11;
    return "LINTLIBRARY";
  }
  if ((linsize >= 7) && !strncmp (ibp, "VARARGS", 7)) {
    *cmdlen = 7;
    ibp += 7; linsize -= 7;
    if ((linsize == 0) || ! isdigit (*ibp)) return "VARARGS";

    /* OK, read a number */
    for (numptr = *argstart = ibp; (numptr < limit) && isdigit (*numptr);
	 numptr++);
    *arglen = numptr - *argstart;
    return "VARARGS";
  }
  return NULL;
}

/*
 * The main loop of the program.
 *
 * Read characters from the input stack, transferring them to the
 * output buffer OP.
 *
 * Macros are expanded and push levels on the input stack.
 * At the end of such a level it is popped off and we keep reading.
 * At the end of any other kind of level, we return.
 * #-directives are handled, except within macros.
 *
 * If OUTPUT_MARKS is nonzero, keep Newline markers found in the input
 * and insert them when appropriate.  This is set while scanning macro
 * arguments before substitution.  It is zero when scanning for final output.
 *   There are three types of Newline markers:
 *   * Newline -  follows a macro name that was not expanded
 *     because it appeared inside an expansion of the same macro.
 *     This marker prevents future expansion of that identifier.
 *     When the input is rescanned into the final output, these are deleted.
 *     These are also deleted by ## concatenation.
 *   * Newline Space (or Newline and any other whitespace character)
 *     stands for a place that tokens must be separated or whitespace
 *     is otherwise desirable, but where the ANSI standard specifies there
 *     is no whitespace.  This marker turns into a Space (or whichever other
 *     whitespace char appears in the marker) in the final output,
 *     but it turns into nothing in an argument that is stringified with #.
 *     Such stringified arguments are the only place where the ANSI standard
 *     specifies with precision that whitespace may not appear.
 *
 * During this function, IP->bufp is kept cached in IBP for speed of access.
 * Likewise, OP->bufp is kept in OBP.  Before calling a subroutine
 * IBP, IP and OBP must be copied back to memory.  IP and IBP are
 * copied back with the RECACHE macro.  OBP must be copied back from OP->bufp
 * explicitly, and before RECACHE, since RECACHE uses OBP.
 */

static void
rescan (op, output_marks)
     FILE_BUF *op;
     int output_marks;
{
  /* Character being scanned in main loop.  */
  register U_CHAR c;

  /* Length of pending accumulated identifier.  */
  register int ident_length = 0;

  /* Hash code of pending accumulated identifier.  */
  register int hash = 0;

  /* Current input level (&instack[indepth]).  */
  FILE_BUF *ip;

  /* Pointer for scanning input.  */
  register U_CHAR *ibp;

  /* Pointer to end of input.  End of scan is controlled by LIMIT.  */
  register U_CHAR *limit;

  /* Pointer for storing output.  */
  register U_CHAR *obp;

  /* REDO_CHAR is nonzero if we are processing an identifier
     after backing up over the terminating character.
     Sometimes we process an identifier without backing up over
     the terminating character, if the terminating character
     is not special.  Backing up is done so that the terminating character
     will be dispatched on again once the identifier is dealt with.  */
  int redo_char = 0;

  /* 1 if within an identifier inside of which a concatenation
     marker (Newline -) has been seen.  */
  int concatenated = 0;

  /* While scanning a comment or a string constant,
     this records the line it started on, for error messages.  */
  int start_line;

  /* Record position of last `real' newline.  */
  U_CHAR *beg_of_line;

/* Pop the innermost input stack level, assuming it is a macro expansion.  */

#define POPMACRO \
do { ip->macro->type = T_MACRO;		\
     if (ip->free_ptr) free (ip->free_ptr);	\
     --indepth; } while (0)

/* Reload `rescan's local variables that describe the current
   level of the input stack.  */

#ifndef NEXT_CPP_SERVER
#define RECACHE  \
do { ip = &instack[indepth];		\
     ibp = ip->bufp;			\
     limit = ip->buf + ip->length;	\
     op->bufp = obp;			\
     check_expand (op, limit - ibp);	\
     beg_of_line = 0;			\
     obp = op->bufp; } while (0)
#else
#define RECACHE  \
do { ip = &instack[indepth];		\
     ibp = ip->bufp;			\
     limit = ip->buf + ip->length;	\
     op->bufp = obp;			\
     check_expand (op, limit - ibp);	\
     beg_of_line = ip->beg_of_line; /* was: beg_of_line = 0; */	\
     obp = op->bufp; } while (0)
#endif

  if (no_output && instack[indepth].fname != 0)
#ifndef NEXT_CPP_SERVER
    skip_if_group (&instack[indepth], 1, NULL);
#else
    skip_if_group (&instack[indepth], 1, op);
  else if (segregate_output && 
   (instack[indepth].file && instack[indepth].file->header_output))
    skip_if_group (&instack[indepth], 1, op);
#endif

  obp = op->bufp;
  RECACHE;

  beg_of_line = ibp;

  /* Our caller must always put a null after the end of
     the input at each input stack level.  */
  if (*limit != 0)
    abort ();

  while (1) {
    c = *ibp++;
    *obp++ = c;

    switch (c) {
    case '\\':
      if (*ibp == '\n' && !ip->macro) {
	/* At the top level, always merge lines ending with backslash-newline,
	   even in middle of identifier.  But do not merge lines in a macro,
	   since backslash might be followed by a newline-space marker.  */
	++ibp;
	++ip->lineno;
	--obp;		/* remove backslash from obuf */
	break;
      }
      /* If ANSI, backslash is just another character outside a string.  */
      if (!traditional)
	goto randomchar;
      /* Otherwise, backslash suppresses specialness of following char,
	 so copy it here to prevent the switch from seeing it.
	 But first get any pending identifier processed.  */
      if (ident_length > 0)
	goto specialchar;
      if (ibp < limit)
	*obp++ = *ibp++;
      break;

    case '#':
      if (assertions_flag) {
	/* Copy #foo (bar lose) without macro expansion.  */
	SKIP_WHITE_SPACE (ibp);
	while (is_idchar[*ibp])
	  *obp++ = *ibp++;
	SKIP_WHITE_SPACE (ibp);
	if (*ibp == '(') {
	  ip->bufp = ibp;
#ifdef NEXT_CPP_SERVER
	  skip_paren_group (ip, op);
#else
	  skip_paren_group (ip);
#endif
	  bcopy ((char *) ibp, (char *) obp, ip->bufp - ibp);
	  obp += ip->bufp - ibp;
	  ibp = ip->bufp;
	}
      }

      /* If this is expanding a macro definition, don't recognize
	 preprocessor directives.  */
      if (ip->macro != 0)
	goto randomchar;
      /* If this is expand_into_temp_buffer, recognize them
	 only after an actual newline at this level,
	 not at the beginning of the input level.  */
      if (ip->fname == 0 && beg_of_line == ip->buf)
	goto randomchar;
      if (ident_length)
	goto specialchar;

      
      /* # keyword: a # must be first nonblank char on the line */
      if (beg_of_line == 0)
	goto randomchar;
      {
	U_CHAR *bp;

	/* Scan from start of line, skipping whitespace, comments
	   and backslash-newlines, and see if we reach this #.
	   If not, this # is not special.  */
	bp = beg_of_line;
	/* If -traditional, require # to be at beginning of line.  */
	if (!traditional)
	  while (1) {
	    if (is_hor_space[*bp])
	      bp++;
	    else if (*bp == '\\' && bp[1] == '\n')
	      bp += 2;
	    else if (*bp == '/' && bp[1] == '*') {
	      bp += 2;
	      while (!(*bp == '*' && bp[1] == '/'))
		bp++;
	      bp += 2;
	    }
	    /* There is no point in trying to deal with C++ // comments here,
	       because if there is one, then this # must be part of the
	       comment and we would never reach here.  */
	    else break;
	  }
	if (bp + 1 != ibp)
	  goto randomchar;
      }

      /* This # can start a directive.  */

      --obp;		/* Don't copy the '#' */

      ip->bufp = ibp;
      op->bufp = obp;
      if (! handle_directive (ip, op)) {
#ifdef USE_C_ALLOCA
	alloca (0);
#endif
	/* Not a known directive: treat it as ordinary text.
	   IP, OP, IBP, etc. have not been changed.  */
	if (no_output && instack[indepth].fname) {
	  /* If not generating expanded output,
	     what we do with ordinary text is skip it.
	     Discard everything until next # directive.  */
#ifdef NEXT_CPP_SERVER
	  skip_if_group (&instack[indepth], 1, op);
#else
	  skip_if_group (&instack[indepth], 1, 0);
#endif
	  RECACHE;
	  beg_of_line = ibp;
	  break;
	}
#ifdef NEXT_CPP_SERVER
        else if (segregate_output && 
                 (instack[indepth].file &&
                  instack[indepth].file->header_output)) {
	  /* If not generating expanded output,
	     what we do with ordinary text is skip it.
	     Discard everything until next # directive.  */
	  skip_if_group (&instack[indepth], 1, op);
	  RECACHE;
	  beg_of_line = ibp;
	  break;
        }
#endif
	++obp;		/* Copy the '#' after all */
	/* Don't expand an identifier that could be a macro directive.
	   (Section 3.8.3 of the ANSI C standard)			*/
	SKIP_WHITE_SPACE (ibp);
	if (is_idstart[*ibp])
	  {
	    *obp++ = *ibp++;
	    while (is_idchar[*ibp])
	      *obp++ = *ibp++;
	  }
	goto randomchar;
      }
#ifdef USE_C_ALLOCA
      alloca (0);
#endif
      /* A # directive has been successfully processed.  */
      /* If not generating expanded output, ignore everything until
	 next # directive.  */
      if (no_output && instack[indepth].fname)
#ifndef NEXT_CPP_SERVER
	skip_if_group (&instack[indepth], 1, 0);
#else
	skip_if_group (&instack[indepth], 1, op);
      else if (segregate_output && (instack[indepth].file && 
                          instack[indepth].file->header_output))
	skip_if_group (&instack[indepth], 1, op);
#endif
      obp = op->bufp;
      RECACHE;
      beg_of_line = ibp;
      break;

    case '\"':			/* skip quoted string */
    case '\'':
      /* A single quoted string is treated like a double -- some
	 programs (e.g., troff) are perverse this way */

      if (ident_length)
	goto specialchar;

      start_line = ip->lineno;

      /* Skip ahead to a matching quote.  */

      while (1) {
	if (ibp >= limit) {
	  if (ip->macro != 0) {
	    /* try harder: this string crosses a macro expansion boundary.
	       This can happen naturally if -traditional.
	       Otherwise, only -D can make a macro with an unmatched quote.  */
	    POPMACRO;
	    RECACHE;
	    continue;
	  }
	  if (!traditional) {
	    error_with_line (line_for_error (start_line),
			     "unterminated string or character constant");
	    error_with_line (multiline_string_line,
			     "possible real start of unterminated constant");
	    multiline_string_line = 0;
	  }
	  break;
	}
	*obp++ = *ibp;
	switch (*ibp++) {
	case '\n':
	  ++ip->lineno;
	  ++op->lineno;
	  /* Traditionally, end of line ends a string constant with no error.
	     So exit the loop and record the new line.  */
	  if (traditional) {
	    beg_of_line = ibp;
	    goto while2end;
	  }
	  if (c == '\'') {
	    error_with_line (line_for_error (start_line),
			     "unterminated character constant");
	    goto while2end;
	  }
	  if (pedantic && multiline_string_line == 0) {
	    pedwarn_with_line (line_for_error (start_line),
			       "string constant runs past end of line");
	  }
	  if (multiline_string_line == 0)
	    multiline_string_line = ip->lineno - 1;
	  break;

	case '\\':
	  if (ibp >= limit)
	    break;
	  if (*ibp == '\n') {
	    /* Backslash newline is replaced by nothing at all,
	       but keep the line counts correct.  */
	    --obp;
	    ++ibp;
	    ++ip->lineno;
	  } else {
	    /* ANSI stupidly requires that in \\ the second \
	       is *not* prevented from combining with a newline.  */
	    while (*ibp == '\\' && ibp[1] == '\n') {
	      ibp += 2;
	      ++ip->lineno;
	    }
	    *obp++ = *ibp++;
	  }
	  break;

	case '\"':
	case '\'':
	  if (ibp[-1] == c)
	    goto while2end;
	  break;
	}
      }
    while2end:
      break;

    case '/':
      if (*ibp == '\\' && ibp[1] == '\n')
	newline_fix (ibp);

      if (*ibp != '*'
	  && !(cplusplus_comments && *ibp == '/'))
	goto randomchar;
      if (ip->macro != 0)
	goto randomchar;
      if (ident_length)
	goto specialchar;

      if (*ibp == '/') {
	/* C++ style comment... */
	start_line = ip->lineno;

	--ibp;			/* Back over the slash */
	--obp;

	/* Comments are equivalent to spaces. */
	if (! put_out_comments)
	  *obp++ = ' ';
	else {
	  /* must fake up a comment here */
	  *obp++ = '/';
	  *obp++ = '/';
	}
	{
	  U_CHAR *before_bp = ibp+2;

	  while (ibp < limit) {
#if defined (NEXT_PDO) && !defined (_WIN32)
	    if (beg_of_line != ibp && ibp[-1] != '\\' && *ibp == '\n') {
#else
	    if (ibp[-1] != '\\' && *ibp == '\n') {
#endif
	      if (put_out_comments) {
		bcopy ((char *) before_bp, (char *) obp, ibp - before_bp);
		obp += ibp - before_bp;
	      }
	      break;
	    } else {
	      if (*ibp == '\n') {
		++ip->lineno;
		/* Copy the newline into the output buffer, in order to
		   avoid the pain of a #line every time a multiline comment
		   is seen.  */
		if (!put_out_comments)
		  *obp++ = '\n';
		++op->lineno;
	      }
	      ibp++;
	    }
	  }
	  break;
	}
      }

      /* Ordinary C comment.  Skip it, optionally copying it to output.  */

      start_line = ip->lineno;

      ++ibp;			/* Skip the star. */

      /* If this cpp is for lint, we peek inside the comments: */
      if (for_lint) {
	U_CHAR *argbp;
	int cmdlen, arglen;
	char *lintcmd = get_lintcmd (ibp, limit, &argbp, &arglen, &cmdlen);

	if (lintcmd != NULL) {
	  op->bufp = obp;
	  check_expand (op, cmdlen + arglen + 14);
	  obp = op->bufp;
	  /* I believe it is always safe to emit this newline: */
	  obp[-1] = '\n';
	  bcopy ("#pragma lint ", (char *) obp, 13);
	  obp += 13;
	  bcopy (lintcmd, (char *) obp, cmdlen);
	  obp += cmdlen;

	  if (arglen != 0) {
	    *(obp++) = ' ';
	    bcopy (argbp, (char *) obp, arglen);
	    obp += arglen;
	  }

	  /* OK, now bring us back to the state we were in before we entered
	     this branch.  We need #line because the #pragma's newline always
	     messes up the line count.  */
	  op->bufp = obp;
	  output_line_command (ip, op, 0, same_file);
	  check_expand (op, limit - ibp + 2);
	  obp = op->bufp;
	  *(obp++) = '/';
	}
      }

      /* Comments are equivalent to spaces.
	 Note that we already output the slash; we might not want it.
	 For -traditional, a comment is equivalent to nothing.  */
      if (! put_out_comments) {
	if (traditional)
	  obp--;
	else
	  obp[-1] = ' ';
      }
      else
	*obp++ = '*';

      {
	U_CHAR *before_bp = ibp;

	while (ibp < limit) {
	  switch (*ibp++) {
	  case '/':
	    if (warn_comments && ibp < limit && *ibp == '*')
	      warning ("`/*' within comment");
	    break;
	  case '*':
	    if (*ibp == '\\' && ibp[1] == '\n')
	      newline_fix (ibp);
	    if (ibp >= limit || *ibp == '/')
	      goto comment_end;
	    break;
	  case '\n':
	    ++ip->lineno;
	    /* Copy the newline into the output buffer, in order to
	       avoid the pain of a #line every time a multiline comment
	       is seen.  */
	    if (!put_out_comments)
	      *obp++ = '\n';
	    ++op->lineno;
	  }
	}
      comment_end:

	if (ibp >= limit)
	  error_with_line (line_for_error (start_line),
			   "unterminated comment");
	else {
	  ibp++;
	  if (put_out_comments) {
	    bcopy ((char *) before_bp, (char *) obp, ibp - before_bp);
	    obp += ibp - before_bp;
	  }
	}
      }
      break;

    case '$':
      if (!dollars_in_ident)
	goto randomchar;
      goto letter;

    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
      /* If digit is not part of identifier, it starts a number,
	 which means that following letters are not an identifier.
	 "0x5" does not refer to an identifier "x5".
	 So copy all alphanumerics that follow without accumulating
	 as an identifier.  Periods also, for sake of "3.e7".  */

      if (ident_length == 0) {
	while (ibp < limit) {
	  while (ibp < limit && ibp[0] == '\\' && ibp[1] == '\n') {
	    ++ip->lineno;
	    ibp += 2;
	  }
	  c = *ibp++;
	  if (!is_idchar[c] && c != '.') {
	    --ibp;
	    break;
	  }
	  *obp++ = c;
	  /* A sign can be part of a preprocessing number
	     if it follows an e.  */
	  if (c == 'e' || c == 'E') {
	    while (ibp < limit && ibp[0] == '\\' && ibp[1] == '\n') {
	      ++ip->lineno;
	      ibp += 2;
	    }
	    if (ibp < limit && (*ibp == '+' || *ibp == '-')) {
	      *obp++ = *ibp++;
	      /* But traditional C does not let the token go past the sign.  */
	      if (traditional)
		break;
	    }
	  }
	}
	break;
      }
      /* fall through */

    case '_':
    case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
    case 'g': case 'h': case 'i': case 'j': case 'k': case 'l':
    case 'm': case 'n': case 'o': case 'p': case 'q': case 'r':
    case 's': case 't': case 'u': case 'v': case 'w': case 'x':
    case 'y': case 'z':
    case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
    case 'G': case 'H': case 'I': case 'J': case 'K': case 'L':
    case 'M': case 'N': case 'O': case 'P': case 'Q': case 'R':
    case 'S': case 'T': case 'U': case 'V': case 'W': case 'X':
    case 'Y': case 'Z':
    letter:
      ident_length++;
      /* Compute step of hash function, to avoid a proc call on every token */
      hash = HASHSTEP (hash, c);
      break;

    case '\n':
      if (ip->fname == 0 && *ibp == '-') {
	/* Newline - inhibits expansion of preceding token.
	   If expanding a macro arg, we keep the newline -.
	   In final output, it is deleted.
	   We recognize Newline - in macro bodies and macro args.  */
	if (! concatenated) {
	  ident_length = 0;
	  hash = 0;
	}
	ibp++;
	if (!output_marks) {
	  obp--;
	} else {
	  /* If expanding a macro arg, keep the newline -.  */
	  *obp++ = '-';
	}
	break;
      }

      /* If reprocessing a macro expansion, newline is a special marker.  */
      else if (ip->macro != 0) {
	/* Newline White is a "funny space" to separate tokens that are
	   supposed to be separate but without space between.
	   Here White means any whitespace character.
	   Newline - marks a recursive macro use that is not
	   supposed to be expandable.  */

	if (is_space[*ibp]) {
	  /* Newline Space does not prevent expansion of preceding token
	     so expand the preceding token and then come back.  */
	  if (ident_length > 0)
	    goto specialchar;

	  /* If generating final output, newline space makes a space.  */
	  if (!output_marks) {
	    obp[-1] = *ibp++;
	    /* And Newline Newline makes a newline, so count it.  */
	    if (obp[-1] == '\n')
	      op->lineno++;
	  } else {
	    /* If expanding a macro arg, keep the newline space.
	       If the arg gets stringified, newline space makes nothing.  */
	    *obp++ = *ibp++;
	  }
	} else abort ();	/* Newline followed by something random?  */
	break;
      }

      /* If there is a pending identifier, handle it and come back here.  */
      if (ident_length > 0)
	goto specialchar;

      beg_of_line = ibp;

      /* Update the line counts and output a #line if necessary.  */
      ++ip->lineno;
      ++op->lineno;
      if (ip->lineno != op->lineno) {
	op->bufp = obp;
	output_line_command (ip, op, 1, same_file);
	check_expand (op, limit - ibp);
	obp = op->bufp;
      }
      break;

      /* Come here either after (1) a null character that is part of the input
	 or (2) at the end of the input, because there is a null there.  */
    case 0:
      if (ibp <= limit)
	/* Our input really contains a null character.  */
	goto randomchar;

      /* At end of a macro-expansion level, pop it and read next level.  */
      if (ip->macro != 0) {
	obp--;
	ibp--;
	/* If traditional, and we have an identifier that ends here,
	   process it now, so we get the right error for recursion.  */
	if (traditional && ident_length
	    && ! is_idchar[*instack[indepth - 1].bufp]) {
	  redo_char = 1;
	  goto randomchar;
	}
	POPMACRO;
	RECACHE;
#ifdef NEXT_CPP_SERVER
        if (mark_expansions && instack[indepth].beg_of_line) {	
          output_line_command (&instack[indepth], op, 0, leave_expansion); 
          obp = op->bufp;
        }
#endif
	break;
      }

      /* If we don't have a pending identifier,
	 return at end of input.  */
      if (ident_length == 0) {
	obp--;
	ibp--;
	op->bufp = obp;
	ip->bufp = ibp;
	goto ending;
      }

      /* If we do have a pending identifier, just consider this null
	 a special character and arrange to dispatch on it again.
	 The second time, IDENT_LENGTH will be zero so we will return.  */

      /* Fall through */

specialchar:

      /* Handle the case of a character such as /, ', " or null
	 seen following an identifier.  Back over it so that
	 after the identifier is processed the special char
	 will be dispatched on again.  */

      ibp--;
      obp--;
      redo_char = 1;

    default:

randomchar:

      if (ident_length > 0) {
	register HASHNODE *hp;

	/* We have just seen an identifier end.  If it's a macro, expand it.

	   IDENT_LENGTH is the length of the identifier
	   and HASH is its hash code.

	   The identifier has already been copied to the output,
	   so if it is a macro we must remove it.

	   If REDO_CHAR is 0, the char that terminated the identifier
	   has been skipped in the output and the input.
	   OBP-IDENT_LENGTH-1 points to the identifier.
	   If the identifier is a macro, we must back over the terminator.

	   If REDO_CHAR is 1, the terminating char has already been
	   backed over.  OBP-IDENT_LENGTH points to the identifier.  */

	if (!pcp_outfile || pcp_inside_if) {
startagain:
	  for (hp = hashtab[MAKE_POS (hash) % HASHSIZE]; hp != NULL;
	       hp = hp->next) {
	    
	    if (hp->length == ident_length) {
	      int obufp_before_macroname;
	      int op_lineno_before_macroname;
	      register int i = ident_length;
	      register U_CHAR *p = hp->name;
	      register U_CHAR *q = obp - i;
	      int disabled;
	      
	      if (! redo_char)
		q--;
	      
	      do {		/* All this to avoid a strncmp () */
		if (*p++ != *q++)
		  goto hashcollision;
	      } while (--i);
	      
	      /* We found a use of a macro name.
		 see if the context shows it is a macro call.  */
#ifdef NEXT_CPP_SERVER	      
              if (hp->type == T_MACRO && !hp->defined_within_module)
                break;
#endif

	      /* Back up over terminating character if not already done.  */
	      if (! redo_char) {
		ibp--;
		obp--;
	      }
	      
	      /* Save this as a displacement from the beginning of the output
		 buffer.  We can not save this as a position in the output
		 buffer, because it may get realloc'ed by RECACHE.  */
	      obufp_before_macroname = (obp - op->buf) - ident_length;
	      op_lineno_before_macroname = op->lineno;
	      
	      if (hp->type == T_PCSTRING) {
		pcstring_used (hp); /* Mark the definition of this key
				       as needed, ensuring that it
				       will be output.  */
		break;		/* Exit loop, since the key cannot have a
				   definition any longer.  */
	      }

	      /* Record whether the macro is disabled.  */
	      disabled = hp->type == T_DISABLED;
	      
	      /* This looks like a macro ref, but if the macro was disabled,
		 just copy its name and put in a marker if requested.  */
	      
	      if (disabled) {
#if 0
		/* This error check caught useful cases such as
		   #define foo(x,y) bar (x (y,0), y)
		   foo (foo, baz)  */
		if (traditional)
		  error ("recursive use of macro `%s'", hp->name);
#endif
		
		if (output_marks) {
		  check_expand (op, limit - ibp + 2);
		  *obp++ = '\n';
		  *obp++ = '-';
		}
		break;
	      }
	      
	      /* If macro wants an arglist, verify that a '(' follows.
		 first skip all whitespace, copying it to the output
		 after the macro name.  Then, if there is no '(',
		 decide this is not a macro call and leave things that way.  */
	      if ((hp->type == T_MACRO || hp->type == T_DISABLED)
		  && hp->value.defn->nargs >= 0)
		{
		  U_CHAR *old_ibp = ibp;
		  U_CHAR *old_obp = obp;
		  int old_iln = ip->lineno;
		  int old_oln = op->lineno;
		  
		  while (1) {
		    /* Scan forward over whitespace, copying it to the output.  */
		    if (ibp == limit && ip->macro != 0) {
		      POPMACRO;
		      RECACHE;
		      old_ibp = ibp;
		      old_obp = obp;
		      old_iln = ip->lineno;
		      old_oln = op->lineno;
		    }
		    /* A comment: copy it unchanged or discard it.  */
		    else if (*ibp == '/' && ibp+1 != limit && ibp[1] == '*') {
		      if (put_out_comments) {
			*obp++ = '/';
			*obp++ = '*';
		      } else if (! traditional) {
			*obp++ = ' ';
		      }
		      ibp += 2;
		      while (ibp + 1 != limit
			     && !(ibp[0] == '*' && ibp[1] == '/')) {
			/* We need not worry about newline-marks,
			   since they are never found in comments.  */
			if (*ibp == '\n') {
			  /* Newline in a file.  Count it.  */
			  ++ip->lineno;
			  ++op->lineno;
			}
			if (put_out_comments)
			  *obp++ = *ibp++;
			else
			  ibp++;
		      }
		      ibp += 2;
		      if (put_out_comments) {
			*obp++ = '*';
			*obp++ = '/';
		      }
		    }
		    else if (is_space[*ibp]) {
		      *obp++ = *ibp++;
		      if (ibp[-1] == '\n') {
			if (ip->macro == 0) {
			  /* Newline in a file.  Count it.  */
			  ++ip->lineno;
			  ++op->lineno;
			} else if (!output_marks) {
			  /* A newline mark, and we don't want marks
			     in the output.  If it is newline-hyphen,
			     discard it entirely.  Otherwise, it is
			     newline-whitechar, so keep the whitechar.  */
			  obp--;
			  if (*ibp == '-')
			    ibp++;
			  else {
			    if (*ibp == '\n')
			      ++op->lineno;
			    *obp++ = *ibp++;
			  }
			} else {
			  /* A newline mark; copy both chars to the output.  */
			  *obp++ = *ibp++;
			}
		      }
		    }
		    else break;
		  }
		  if (*ibp != '(') {
		    /* It isn't a macro call.
		       Put back the space that we just skipped.  */
		    ibp = old_ibp;
		    obp = old_obp;
		    ip->lineno = old_iln;
		    op->lineno = old_oln;
		    /* Exit the for loop.  */
		    break;
		  }
		}
	      
	      /* This is now known to be a macro call.
		 Discard the macro name from the output,
		 along with any following whitespace just copied,
		 but preserve newlines if not outputting marks since this
		 is more likely to do the right thing with line numbers.  */
	      obp = op->buf + obufp_before_macroname;
	      if (output_marks)
		op->lineno = op_lineno_before_macroname;
	      else {
		int newlines = op->lineno - op_lineno_before_macroname;
		while (0 < newlines--)
		  *obp++ = '\n';
	      }

	      /* Prevent accidental token-pasting with a character
		 before the macro call.  */
	      if (!traditional && obp != op->buf
		  && (obp[-1] == '-' || obp[1] == '+' || obp[1] == '&'
		      || obp[-1] == '|' || obp[1] == '<' || obp[1] == '>')) {
		/* If we are expanding a macro arg, make a newline marker
		   to separate the tokens.  If we are making real output,
		   a plain space will do.  */
		if (output_marks)
		  *obp++ = '\n';
		*obp++ = ' ';
	      }

	      /* Expand the macro, reading arguments as needed,
		 and push the expansion on the input stack.  */
	      ip->bufp = ibp;
	      op->bufp = obp;
#ifdef NEXT_CPP_SERVER
              {
              U_CHAR *start_exp = ip->bufp - ident_length;
#endif
	      macroexpand (hp, op);
#ifdef NEXT_CPP_SERVER
              if (mark_expansions &&
                  (hp->type == T_DISABLED || hp->type == T_MACRO) && 
                  (indepth == 1 || /* processing the main module */
                  (instack[indepth-1].fname && instack[indepth-1].file))) {
                /* overload nominal_fname (for purposes of the call to
                   output_line_command). Even though this seems gross,
                   it acutally models the markers that are output */
                char *save_name = instack[indepth-1].nominal_fname;
                char save_char = *ip->bufp;
                instack[indepth-1].beg_of_line = beg_of_line;
                instack[indepth-1].nominal_fname = start_exp;
                *ip->bufp = '\0';
                output_line_command (&instack[indepth-1], op, 0,
                                     enter_expansion);
                instack[indepth-1].nominal_fname = save_name;
                *ip->bufp = save_char;
              }
              }
#endif
	      
	      /* Reexamine input stack, since macroexpand has pushed
		 a new level on it.  */
	      obp = op->bufp;
	      RECACHE;
	      break;
	    }
hashcollision:
	    ;
	  }			/* End hash-table-search loop */
	}
	ident_length = hash = 0; /* Stop collecting identifier */
	redo_char = 0;
	concatenated = 0;
      }				/* End if (ident_length > 0) */
    }				/* End switch */
  }				/* End per-char loop */

  /* Come here to return -- but first give an error message
     if there was an unterminated successful conditional.  */
 ending:
  if (if_stack != ip->if_stack)
    {
      char *str = "unknown";

      switch (if_stack->type)
	{
	case T_IF:
	  str = "if";
	  break;
	case T_IFDEF:
	  str = "ifdef";
	  break;
	case T_IFNDEF:
	  str = "ifndef";
	  break;
	case T_ELSE:
	  str = "else";
	  break;
	case T_ELIF:
	  str = "elif";
	  break;
	}

      error_with_line (line_for_error (if_stack->lineno),
		       "unterminated `#%s' conditional", str);
  }
  if_stack = ip->if_stack;
}

/*
 * Rescan a string into a temporary buffer and return the result
 * as a FILE_BUF.  Note this function returns a struct, not a pointer.
 *
 * OUTPUT_MARKS nonzero means keep Newline markers found in the input
 * and insert such markers when appropriate.  See `rescan' for details.
 * OUTPUT_MARKS is 1 for macroexpanding a macro argument separately
 * before substitution; it is 0 for other uses.
 */
static FILE_BUF
expand_to_temp_buffer (buf, limit, output_marks, assertions)
     U_CHAR *buf, *limit;
     int output_marks, assertions;
{
  register FILE_BUF *ip;
  FILE_BUF obuf;
  int length = limit - buf;
  U_CHAR *buf1;
  int odepth = indepth;
  int save_assertions_flag = assertions_flag;

  assertions_flag = assertions;

  if (length < 0)
    abort ();

  /* Set up the input on the input stack.  */

  buf1 = (U_CHAR *) alloca (length + 1);
  {
    register U_CHAR *p1 = buf;
    register U_CHAR *p2 = buf1;

    while (p1 != limit)
      *p2++ = *p1++;
  }
  buf1[length] = 0;

  /* Set up to receive the output.  */

  obuf.length = length * 2 + 100; /* Usually enough.  Why be stingy?  */
  obuf.bufp = obuf.buf = (U_CHAR *) xmalloc (obuf.length);
  obuf.fname = 0;
  obuf.macro = 0;
  obuf.free_ptr = 0;

  CHECK_DEPTH ({return obuf;});

  ++indepth;

  ip = &instack[indepth];
  ip->fname = 0;
  ip->nominal_fname = 0;
  ip->system_header_p = 0;
  ip->macro = 0;
  ip->free_ptr = 0;
  ip->length = length;
  ip->buf = ip->bufp = buf1;
  ip->if_stack = if_stack;
#ifdef NEXT_CPP_SERVER
  ip->dir = ip->file = 0;
  ip->beg_of_line = 0;
#endif

  ip->lineno = obuf.lineno = 1;

  /* Scan the input, create the output.  */
  rescan (&obuf, output_marks);

  /* Pop input stack to original state.  */
  --indepth;

  if (indepth != odepth)
    abort ();

  /* Record the output.  */
  obuf.length = obuf.bufp - obuf.buf;

  assertions_flag = save_assertions_flag;
  return obuf;
}

/*
 * Process a # directive.  Expects IP->bufp to point after the '#', as in
 * `#define foo bar'.  Passes to the command handler
 * (do_define, do_include, etc.): the addresses of the 1st and
 * last chars of the command (starting immediately after the #
 * keyword), plus op and the keyword table pointer.  If the command
 * contains comments it is copied into a temporary buffer sans comments
 * and the temporary buffer is passed to the command handler instead.
 * Likewise for backslash-newlines.
 *
 * Returns nonzero if this was a known # directive.
 * Otherwise, returns zero, without advancing the input pointer.
 */

static int
handle_directive (ip, op)
     FILE_BUF *ip, *op;
{
  register U_CHAR *bp, *cp;
  register struct directive *kt;
  register int ident_length;
  U_CHAR *resume_p;

  /* Nonzero means we must copy the entire command
     to get rid of comments or backslash-newlines.  */
  int copy_command = 0;

  U_CHAR *ident, *after_ident;

  bp = ip->bufp;

  /* Record where the directive started.  do_xifdef needs this.  */
  directive_start = bp - 1;

  /* Skip whitespace and \-newline.  */
  while (1) {
    if (is_hor_space[*bp]) {
      if ((*bp == '\f' || *bp == '\v') && pedantic)
	pedwarn ("%s in preprocessing directive",
		 *bp == '\f' ? "formfeed" : "vertical tab");
      bp++;
    } else if (*bp == '/' && (bp[1] == '*'
			      || (cplusplus_comments && bp[1] == '/'))) {
      ip->bufp = bp + 2;
#ifdef NEXT_CPP_SERVER
      skip_to_end_of_comment (ip, op, &ip->lineno, 0);
#else
      skip_to_end_of_comment (ip, &ip->lineno, 0);
#endif
      bp = ip->bufp;
    } else if (*bp == '\\' && bp[1] == '\n') {
      bp += 2; ip->lineno++;
    } else break;
  }

  /* Now find end of directive name.
     If we encounter a backslash-newline, exchange it with any following
     symbol-constituents so that we end up with a contiguous name.  */

  cp = bp;
  while (1) {
    if (is_idchar[*cp])
      cp++;
    else {
      if (*cp == '\\' && cp[1] == '\n')
	name_newline_fix (cp);
      if (is_idchar[*cp])
	cp++;
      else break;
    }
  }
  ident_length = cp - bp;
  ident = bp;
  after_ident = cp;

  /* A line of just `#' becomes blank.  */

  if (ident_length == 0 && *after_ident == '\n') {
    ip->bufp = after_ident;
    return 1;
  }

  if (ident_length == 0 || !is_idstart[*ident]) {
    U_CHAR *p = ident;
    while (is_idchar[*p]) {
      if (*p < '0' || *p > '9')
	break;
      p++;
    }
    /* Handle # followed by a line number.  */
    if (p != ident && !is_idchar[*p]) {
      static struct directive line_directive_table[] = {
	{  4, do_line, "line", T_LINE},
      };
      if (pedantic)
	pedwarn ("`#' followed by integer");
      after_ident = ident;
      kt = line_directive_table;
      goto old_linenum;
    }

    /* Avoid error for `###' and similar cases unless -pedantic.  */
    if (p == ident) {
      while (*p == '#' || is_hor_space[*p]) p++;
      if (*p == '\n') {
	if (pedantic && !lang_asm)
	  warning ("invalid preprocessor directive");
	return 0;
      }
    }

    if (!lang_asm)
      error ("invalid preprocessor directive name");

    return 0;
  }

  /*
   * Decode the keyword and call the appropriate expansion
   * routine, after moving the input pointer up to the next line.
   */
  for (kt = directive_table; kt->length > 0; kt++) {
    if (kt->length == ident_length && !strncmp (kt->name, ident, ident_length)) {
      register U_CHAR *buf;
      register U_CHAR *limit;
      int unterminated;
      int junk;
      int *already_output;

      /* Nonzero means do not delete comments within the directive.
	 #define needs this when -traditional.  */
      int keep_comments;

    old_linenum:

      limit = ip->buf + ip->length;
      unterminated = 0;
      already_output = 0;
      keep_comments = traditional && kt->traditional_comments;
#if !defined (NEXT_SEMANTICS) && !defined (NEXT_PDO)
      /* #import is defined only in Objective C,
	 or when on the NeXT (or Apple).  */
      if (kt->type == T_IMPORT && !(objc || lookup ("__NeXT__", -1, -1)))
	break;
#endif

      /* Find the end of this command (first newline not backslashed
	 and not in a string or comment).
	 Set COPY_COMMAND if the command must be copied
	 (it contains a backslash-newline or a comment).  */

      buf = bp = after_ident;
      while (bp < limit) {
	register U_CHAR c = *bp++;
	switch (c) {
	case '\\':
	  if (bp < limit) {
	    if (*bp == '\n') {
	      ip->lineno++;
	      copy_command = 1;
	      bp++;
	    } else if (traditional)
	      bp++;
	  }
	  break;

	case '\'':
	case '\"':
	  bp = skip_quoted_string (bp - 1, limit, ip->lineno, &ip->lineno, &copy_command, &unterminated);
	  /* Don't bother calling the directive if we already got an error
	     message due to unterminated string.  Skip everything and pretend
	     we called the directive.  */
	  if (unterminated) {
	    if (traditional) {
	      /* Traditional preprocessing permits unterminated strings.  */
	      ip->bufp = bp;
	      goto endloop1;
	    }
	    ip->bufp = bp;
	    return 1;
	  }
	  break;

	  /* <...> is special for #include.  */
	case '<':
	  if (!kt->angle_brackets)
	    break;
	  while (bp < limit && *bp != '>' && *bp != '\n') {
	    if (*bp == '\\' && bp[1] == '\n') {
	      ip->lineno++;
	      copy_command = 1;
	      bp++;
	    }
	    bp++;
	  }
	  break;

	case '/':
	  if (*bp == '\\' && bp[1] == '\n')
	    newline_fix (bp);
	  if (*bp == '*'
	      || (cplusplus_comments && *bp == '/')) {
	    U_CHAR *obp = bp - 1;
	    ip->bufp = bp + 1;
#ifdef NEXT_CPP_SERVER
	    skip_to_end_of_comment (ip, op, &ip->lineno, 0);
#else
	    skip_to_end_of_comment (ip, &ip->lineno, 0);
#endif
	    bp = ip->bufp;
	    /* No need to copy the command because of a comment at the end;
	       just don't include the comment in the directive.  */
	    if (bp == limit || *bp == '\n') {
	      bp = obp;
	      goto endloop1;
	    }
	    /* Don't remove the comments if -traditional.  */
	    if (! keep_comments)
	      copy_command++;
	  }
	  break;

	case '\f':
	case '\v':
	  if (pedantic)
	    pedwarn ("%s in preprocessing directive",
		     c == '\f' ? "formfeed" : "vertical tab");
	  break;

	case '\n':
	  --bp;		/* Point to the newline */
	  ip->bufp = bp;
	  goto endloop1;
	}
      }
      ip->bufp = bp;

    endloop1:
      resume_p = ip->bufp;
      /* BP is the end of the directive.
	 RESUME_P is the next interesting data after the directive.
	 A comment may come between.  */

      /* If a directive should be copied through, and -E was given,
	 pass it through before removing comments.  */
      if (!no_output && kt->pass_thru && put_out_comments) {
        int len;

	/* Output directive name.  */
        check_expand (op, kt->length + 2);
	/* Make sure # is at the start of a line */
	if (op->bufp > op->buf && op->bufp[-1] != '\n') {
	  op->lineno++;
	  *op->bufp++ = '\n';
	}
        *op->bufp++ = '#';
        bcopy (kt->name, op->bufp, kt->length);
        op->bufp += kt->length;

	/* Output arguments.  */
	len = (bp - buf);
	check_expand (op, len);
	bcopy (buf, (char *) op->bufp, len);
	op->bufp += len;
	/* Take account of any (escaped) newlines just output.  */
	while (--len >= 0)
	  if (buf[len] == '\n')
	    op->lineno++;

	already_output = &junk;
      }				/* Don't we need a newline or #line? */

      if (copy_command) {
	register U_CHAR *xp = buf;
	/* Need to copy entire command into temp buffer before dispatching */

	cp = (U_CHAR *) alloca (bp - buf + 5); /* room for cmd plus
						  some slop */
	buf = cp;

	/* Copy to the new buffer, deleting comments
	   and backslash-newlines (and whitespace surrounding the latter).  */

	while (xp < bp) {
	  register U_CHAR c = *xp++;
	  *cp++ = c;

	  switch (c) {
	  case '\n':
	    abort ();  /* A bare newline should never part of the line.  */
	    break;

	    /* <...> is special for #include.  */
	  case '<':
	    if (!kt->angle_brackets)
	      break;
	    while (xp < bp && c != '>') {
	      c = *xp++;
	      if (c == '\\' && xp < bp && *xp == '\n')
		xp++;
	      else
		*cp++ = c;
	    }
	    break;

	  case '\\':
	    if (*xp == '\n') {
	      xp++;
	      cp--;
	      if (cp != buf && is_space[cp[-1]]) {
		while (cp != buf && is_space[cp[-1]]) cp--;
		cp++;
		SKIP_WHITE_SPACE (xp);
	      } else if (is_space[*xp]) {
		*cp++ = *xp++;
		SKIP_WHITE_SPACE (xp);
	      }
	    } else {
	      *cp++ = *xp++;
	    }
	    break;

	  case '\'':
	  case '\"':
	    {
	      register U_CHAR *bp1
		= skip_quoted_string (xp - 1, bp, ip->lineno,
				      NULL_PTR, NULL_PTR, NULL_PTR);
	      while (xp != bp1)
		if (*xp == '\\') {
		  if (*++xp != '\n')
		    *cp++ = '\\';
		  else
		    xp++;
		} else
		  *cp++ = *xp++;
	    }
	    break;

	  case '/':
	    if (*xp == '*'
		|| (cplusplus_comments && *xp == '/')) {
	      ip->bufp = xp + 1;
	      /* If we already copied the command through,
		 already_output != 0 prevents outputting comment now.  */
#ifdef NEXT_CPP_SERVER
	      skip_to_end_of_comment (ip, op, already_output, 0);
#else
	      skip_to_end_of_comment (ip, already_output, 0);
#endif
	      if (keep_comments)
		while (xp != ip->bufp)
		  *cp++ = *xp++;
	      /* Delete or replace the slash.  */
	      else if (traditional)
		cp--;
	      else
		cp[-1] = ' ';
	      xp = ip->bufp;
	    }
	  }
	}

	/* Null-terminate the copy.  */

	*cp = 0;
      } else
	cp = bp;

      ip->bufp = resume_p;

      /* Some directives should be written out for cc1 to process,
	 just as if they were not defined.  And sometimes we're copying
	 definitions through.  */

      if (!no_output && already_output == 0
	  && (kt->pass_thru
	      || (kt->type == T_DEFINE
		  && (dump_macros == dump_names
		      || dump_macros == dump_definitions)
#ifdef NEXT_CPP_SERVER
		  && (!ip->file || !ip->file->header_output)
#endif		      
		      ))) {
        int len;

	/* Output directive name.  */
        check_expand (op, kt->length + 1);
        *op->bufp++ = '#';
        bcopy (kt->name, (char *) op->bufp, kt->length);
        op->bufp += kt->length;

	if (kt->pass_thru || dump_macros == dump_definitions) {
	  /* Output arguments.  */
	  len = (cp - buf);
	  check_expand (op, len);
	  bcopy (buf, (char *) op->bufp, len);
	  op->bufp += len;
	} else if (kt->type == T_DEFINE && dump_macros == dump_names) {
	  U_CHAR *xp = buf;
	  U_CHAR *yp;
	  SKIP_WHITE_SPACE (xp);
	  yp = xp;
	  while (is_idchar[*xp]) xp++;
	  len = (xp - yp);
	  check_expand (op, len + 1);
	  *op->bufp++ = ' ';
	  bcopy (yp, op->bufp, len);
	  op->bufp += len;
	}
      }				/* Don't we need a newline or #line? */

      /* Call the appropriate command handler.  buf now points to
	 either the appropriate place in the input buffer, or to
	 the temp buffer if it was necessary to make one.  cp
	 points to the first char after the contents of the (possibly
	 copied) command, in either case. */
      (*kt->func) (buf, cp, op, kt);
#ifdef NEXT_CPP_SERVER
      if (segregate_output
          && (ip->file && !ip->file->header_output)) {
         if (kt->func == do_include)
           ip->file->position_after_last_include = ip->bufp;
         else if (kt->func == do_if || kt->func == do_xifdef) {
	   ip->file->has_explicit_context_dependency = 1;
	   if (kt->type == T_IFNDEF && !ip->file->position_of_first_ifndef)
             ip->file->position_of_first_ifndef = directive_start;
         }
      }
#endif
      check_expand (op, ip->length - (ip->bufp - ip->buf));

      return 1;
    }
  }

  /* It is deliberate that we don't warn about undefined directives.
     That is the responsibility of cc1.  */
  return 0;
}

static struct tm *
timestamp ()
{
  static struct tm *timebuf;
  if (!timebuf) {
    time_t t = time ((time_t *)0);
    timebuf = localtime (&t);
  }
  return timebuf;
}

static char *monthnames[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
			     "Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
			    };

/*
 * expand things like __FILE__.  Place the expansion into the output
 * buffer *without* rescanning.
 */

static void
special_symbol (hp, op)
     HASHNODE *hp;
     FILE_BUF *op;
{
  char *buf;
  int i, len;
  int true_indepth;
  FILE_BUF *ip = NULL;
  struct tm *timebuf;

  int paren = 0;		/* For special `defined' keyword */

  if (pcp_outfile && pcp_inside_if
      && hp->type != T_SPEC_DEFINED && hp->type != T_CONST)
    error ("Predefined macro `%s' used inside `#if' during precompilation",
	   hp->name);
    
  for (i = indepth; i >= 0; i--)
    if (instack[i].fname != NULL) {
      ip = &instack[i];
      break;
    }
  if (ip == NULL) {
    error ("cccp error: not in any file?!");
    return;			/* the show must go on */
  }

  switch (hp->type) {
  case T_FILE:
  case T_BASE_FILE:
    {
      char *string;
      if (hp->type == T_FILE)
	string = ip->nominal_fname;
      else
	string = instack[0].nominal_fname;

      if (string)
	{
	  buf = (char *) alloca (3 + 4 * strlen (string));
	  quote_string (buf, string);
	}
      else
	buf = "\"\"";

      break;
    }

  case T_INCLUDE_LEVEL:
    true_indepth = 0;
    for (i = indepth; i >= 0; i--)
      if (instack[i].fname != NULL)
        true_indepth++;

    buf = (char *) alloca (8);	/* Eight bytes ought to be more than enough */
    sprintf (buf, "%d", true_indepth - 1);
    break;

  case T_VERSION:
    buf = (char *) alloca (3 + strlen (version_string));
    sprintf (buf, "\"%s\"", version_string);
    break;

#ifndef NO_BUILTIN_SIZE_TYPE
  case T_SIZE_TYPE:
    buf = SIZE_TYPE;
    break;
#endif

#ifndef NO_BUILTIN_PTRDIFF_TYPE
  case T_PTRDIFF_TYPE:
    buf = PTRDIFF_TYPE;
    break;
#endif

  case T_WCHAR_TYPE:
    buf = wchar_type;
    break;

  case T_USER_LABEL_PREFIX_TYPE:
    buf = USER_LABEL_PREFIX;
    break;

  case T_REGISTER_PREFIX_TYPE:
    buf = REGISTER_PREFIX;
    break;

  case T_CONST:
    buf = (char *) alloca (4 * sizeof (int));
    sprintf (buf, "%d", hp->value.ival);
    if (pcp_inside_if && pcp_outfile)
      /* Output a precondition for this macro use */
      fprintf (pcp_outfile, "#define %s %d\n", hp->name, hp->value.ival);
    break;

  case T_SPECLINE:
    buf = (char *) alloca (10);
    sprintf (buf, "%d", ip->lineno);
    break;

  case T_DATE:
  case T_TIME:
    buf = (char *) alloca (20);
    timebuf = timestamp ();
    if (hp->type == T_DATE)
      sprintf (buf, "\"%s %2d %4d\"", monthnames[timebuf->tm_mon],
	      timebuf->tm_mday, timebuf->tm_year + 1900);
    else
      sprintf (buf, "\"%02d:%02d:%02d\"", timebuf->tm_hour, timebuf->tm_min,
	      timebuf->tm_sec);
    break;

  case T_SPEC_DEFINED:
    buf = " 0 ";		/* Assume symbol is not defined */
    ip = &instack[indepth];
    SKIP_WHITE_SPACE (ip->bufp);
    if (*ip->bufp == '(') {
      paren++;
      ip->bufp++;			/* Skip over the paren */
      SKIP_WHITE_SPACE (ip->bufp);
    }

    if (!is_idstart[*ip->bufp])
      goto oops;
    if (hp = lookup (ip->bufp, -1, -1)) {
      if (pcp_outfile && pcp_inside_if
	  && (hp->type == T_CONST
	      || (hp->type == T_MACRO && hp->value.defn->predefined)))
	/* Output a precondition for this macro use. */
	fprintf (pcp_outfile, "#define %s\n", hp->name);
      buf = " 1 ";
    }
    else
      if (pcp_outfile && pcp_inside_if)	{
	/* Output a precondition for this macro use */
	U_CHAR *cp = ip->bufp;
	fprintf (pcp_outfile, "#undef ");
	while (is_idchar[*cp]) /* Ick! */
	  fputc (*cp++, pcp_outfile);
	putc ('\n', pcp_outfile);
      }
    while (is_idchar[*ip->bufp])
      ++ip->bufp;
    SKIP_WHITE_SPACE (ip->bufp);
    if (paren) {
      if (*ip->bufp != ')')
	goto oops;
      ++ip->bufp;
    }
    break;

oops:

    error ("`defined' without an identifier");
    break;

  default:
    error ("cccp error: invalid special hash type"); /* time for gdb */
    abort ();
  }
  len = strlen (buf);
  check_expand (op, len);
  bcopy (buf, (char *) op->bufp, len);
  op->bufp += len;

  return;
}

struct import_file {
#ifdef NEXT_CPP_SERVER
  struct file_name_list *name;
  int visited_within_module;	/* to support multiple modules */
  time_t modtime;
#else
  char *name;
#endif
  ino_t inode;
  dev_t dev;
#if defined (_WIN32) && defined (NEXT_PDO)
  _off_t size;
  time_t mtime;
  time_t ctime;
#endif
  struct import_file *next;
};

#if defined (NEXT_SEMANTICS) || defined (NEXT_PDO)
typedef enum {
   NON_EXISTENT_HEADER,
   GOOD_HEADER,
   REDUNDANT_HEADER
} header_type;

static header_type try_include_file(path, importing, pf, pimpfile, pstatbuf)
const char * path;
int importing;
int * pf;
struct import_file ** pimpfile;
struct stat * pstatbuf;
{
   /* lookup in negative cache...opens/stats are painfully slow */
   if (found_in_negative_cache(path)) {
      return NON_EXISTENT_HEADER;
   } else {
      struct import_file * impfile = 0;
      int f = -1;
      if (importing) {
         /* Returns -2 if impfile set  */
         f = lookup_import (path, pstatbuf, &impfile);
      } else {
         redundant_include_p (path, &impfile);
         if (impfile)
            f = -2;
         else
            f = open (path, O_RDONLY, 0666);
      }
      if (f == -2) {
         if ((importing && impfile->visited_within_module)
            || (!importing 
                && ((!segregate_output && impfile->name->control_macro) 
                   || (segregate_output && impfile->name->control_macro 
                                        && impfile->visited_within_module))))
               return REDUNDANT_HEADER;
      }
      else if (f == -1) {
         add_to_negative_cache(savestring(path));
      }
      else if (!importing) { /* handle the "once idiom" */
         if (impfile && impfile->name->control_macro &&
             lookup (impfile->name->control_macro, -1, -1))
            return REDUNDANT_HEADER;
      }
      if (impfile) {
         *pf = f;
         *pimpfile = impfile;
         return GOOD_HEADER;
      }
      if (f >= 0) {
         if (!importing)
            fstat (f, pstatbuf);
         *pf = f;
         *pimpfile = impfile;
         return GOOD_HEADER;
      }
      return NON_EXISTENT_HEADER;
   }
}
#endif


/* Routines to handle #directives */

/* Handle #include and #import.
   This function expects to see "fname" or <fname> on the input.  */

static int
do_include (buf, limit, op, keyword)
     U_CHAR *buf, *limit;
     FILE_BUF *op;
     struct directive *keyword;
{
  int importing = (keyword->type == T_IMPORT);
  int skip_dirs = (keyword->type == T_INCLUDE_NEXT);
#ifdef NEXT_OBJC_RUNTIME
  static int import_warning = 1;
#else
  static int import_warning = 0;
#endif
  char *fname;		/* Dynamically allocated fname buffer */
  char *pcftry;
  char *pcfname;
  U_CHAR *fbeg, *fend;		/* Beginning and end of fname */
#ifdef NEXT_FRAMEWORKS
  char extendedPath[MAX_FILENAME_LEN];
  U_CHAR *bufptr;
  int hasSlash;    
#endif

  struct file_name_list *search_start = include; /* Chain of dirs to search */
  struct file_name_list dsp[1];	/* First in chain, if #include "..." */
  struct file_name_list *searchptr = 0;
  int flen;

  int f;			/* file number */

  int retried = 0;		/* Have already tried macro
				   expanding the include line*/
  FILE_BUF trybuf;		/* It got expanded into here */
  int angle_brackets = 0;	/* 0 for "...", 1 for <...> */
  int pcf = -1;
  char *pcfbuf;
  int pcfbuflimit;
  int pcfnum;
#ifdef NEXT_OBJC_RUNTIME
  U_CHAR *partial = NULL;
  int plen = 0;
#endif
#ifdef NEXT_CPP_SERVER
  header_type hdr_result; 
  struct stat shared_stat_f;
  struct import_file *impfile = 0;
#endif
  f= -1;			/* JF we iz paranoid! */

  if (importing && warn_import && !inhibit_warnings
      && !instack[indepth].system_header_p && !import_warning) {
    import_warning = 1;
    warning ("using `#import' is not recommended");
    fprintf (stderr, "The fact that a certain header file need not be processed more than once\n");
    fprintf (stderr, "should be indicated in the header file, not where it is used.\n");
    fprintf (stderr, "The best way to do this is with a conditional of this form:\n\n");
    fprintf (stderr, "  #ifndef _FOO_H_INCLUDED\n");
    fprintf (stderr, "  #define _FOO_H_INCLUDED\n");
    fprintf (stderr, "  ... <real contents of file> ...\n");
    fprintf (stderr, "  #endif /* Not _FOO_H_INCLUDED */\n\n");
    fprintf (stderr, "Then users can use `#include' any number of times.\n");
    fprintf (stderr, "GNU C automatically avoids processing the file more than once\n");
    fprintf (stderr, "when it is equipped with such a conditional.\n");
  }

get_filename:

  fbeg = buf;
  SKIP_WHITE_SPACE (fbeg);
  /* Discard trailing whitespace so we can easily see
     if we have parsed all the significant chars we were given.  */
  while (limit != fbeg && is_hor_space[limit[-1]]) limit--;

  switch (*fbeg++) {
  case '\"':
    {
      FILE_BUF *fp;
      /* Copy the operand text, concatenating the strings.  */
      {
	U_CHAR *fin = fbeg;
	fbeg = (U_CHAR *) alloca (limit - fbeg + 1);
	fend = fbeg;
	while (fin != limit) {
	  while (fin != limit && *fin != '\"')
	    *fend++ = *fin++;
	  fin++;
	  if (fin == limit)
	    break;
	  /* If not at the end, there had better be another string.  */
	  /* Skip just horiz space, and don't go past limit.  */
	  while (fin != limit && is_hor_space[*fin]) fin++;
	  if (fin != limit && *fin == '\"')
	    fin++;
	  else
	    goto fail;
	}
      }
      *fend = 0;

      /* We have "filename".  Figure out directory this source
	 file is coming from and put it on the front of the list. */

      /* If -I- was specified, don't search current dir, only spec'd ones. */
      if (ignore_srcdir) break;

      for (fp = &instack[indepth]; fp >= instack; fp--)
	{
	  int n;
	  char *ep,*nam;

	  if ((nam = fp->nominal_fname) != NULL) {
	    /* Found a named file.  Figure out dir of the file,
	       and put it in front of the search list.  */
	    dsp[0].next = search_start;
	    search_start = dsp;
#ifndef VMS
	    ep = rindex (nam, '/');
#else				/* VMS */
	    ep = rindex (nam, ']');
	    if (ep == NULL) ep = rindex (nam, '>');
	    if (ep == NULL) ep = rindex (nam, ':');
	    if (ep != NULL) ep++;
#endif				/* VMS */
	    if (ep != NULL) {
	      n = ep - nam;
	      dsp[0].fname = (char *) alloca (n + 1);
	      strncpy (dsp[0].fname, nam, n);
	      dsp[0].fname[n] = '\0';
	      if (n + INCLUDE_LEN_FUDGE > max_include_len)
		max_include_len = n + INCLUDE_LEN_FUDGE;
	    } else {
	      dsp[0].fname = 0; /* Current directory */
	    }
	    dsp[0].got_name_map = 0;
	    break;
	  }
	}
      break;
    }

  case '<':
    fend = fbeg;
    while (fend != limit && *fend != '>') fend++;
    if (*fend == '>' && fend + 1 == limit) {
      angle_brackets = 1;
#ifdef NEXT_FRAMEWORKS
      flen = fend -fbeg;
      hasSlash = 0;
      for (bufptr = fbeg; bufptr != fend; bufptr++)
          if (*bufptr == '/') {
             hasSlash++;
           break;
        }
       if (hasSlash) {
         extendedPath[0] = '\0';
         for (searchptr = framework;
              f < 0 && searchptr != 0;
              searchptr = searchptr->next) {
            U_CHAR *end, *headerNameTo, *headerNameFrom;
            int index;
            
            /* path to framework */
            strcpy(extendedPath, searchptr->fname);
            end = &extendedPath[strlen(extendedPath)];
            *end++ = '/';

            /* add framework directory */
            for (bufptr = fbeg;
                 *bufptr != '/' && bufptr != fend;
                 bufptr++)
               *end++ = *bufptr;
            strncpy(end, ".framework/", 11);   end += 11;
            
            for (index = 0; framework_header_dirs[index].dirName; index++) {
               /* add path to headers inside framework */
               strncpy(end,
                       framework_header_dirs[index].dirName,
                       framework_header_dirs[index].dirNameLen);
               /* add name of header */
               headerNameTo = end + framework_header_dirs[index].dirNameLen;
               for (headerNameFrom = bufptr; headerNameFrom != fend; headerNameFrom++)
                  *headerNameTo++ = *headerNameFrom;
               *headerNameTo = '\0';
               /* Try looking for the file */
#ifdef NEXT_CPP_SERVER
               hdr_result = try_include_file(extendedPath, importing, &f, &impfile, &shared_stat_f);
               if (hdr_result == REDUNDANT_HEADER)
                  return 0;
               else if (hdr_result == GOOD_HEADER)
                  break;
#if 0
               /* lookup in negative cache...opens/stats are painfully slow */
               if (!found_in_negative_cache(extendedPath)) {
                  impfile = 0;
                  if (importing)
                     f = lookup_import (extendedPath, &shared_stat_f, &impfile);
                  else {
                     redundant_include_p (extendedPath, &impfile);
                     if (!impfile)
                        f = open (extendedPath, O_RDONLY, 0666);
                     else
                        f = -2;
                  }
                  if (f == -2) {
                     if (segregate_output &&
                         impfile->visited_within_module &&
                         impfile->name &&
                         !impfile->name->has_explicit_context_dependency) {
                        return 0;	     /* Already included this file */
                     }
                  }
                  else if (f == -1) {
                     add_to_negative_cache(savestring(extendedPath));
                  }
                  else if (!importing) { /* handle the "once idiom" */
                     if (impfile && impfile->name->control_macro &&
                         lookup (impfile->name->control_macro, -1, -1))
                        return 0;
                  }
                  if (f >= 0 || impfile) {
                     if (!importing)
                        fstat (f, &shared_stat_f);
                     break;
                  }
               } /* else, found in negative cache...don't bother */
#endif 0
#else
               if (importing)
                  f = lookup_import (extendedPath, searchptr);
               else
                  f = open_include_file (extendedPath, searchptr);
               if (f == -2)
                  return 0;			/* Already included this file */
#ifdef EACCES
                 else if (f == -1 && errno == EACCES)
                     warning ("Header file %s exists, but is not readable",
                              extendedPath);
#endif
                 if (redundant_include_p (extendedPath)) {
                     close (f);
                     return 0;
                 }
                 if (f >= 0)
                     break;
#endif
                 
             }
          }
      }
#endif
      /* If -I-, start with the first -I dir after the -I-.  */
      if (first_bracket_include)
	search_start = first_bracket_include;
      break;
    }
    goto fail;

  default:
#ifdef VMS
    /*
     * Support '#include xyz' like VAX-C to allow for easy use of all the
     * decwindow include files. It defaults to '#include <xyz.h>' (so the
     * code from case '<' is repeated here) and generates a warning.
     */
    if (isalpha(*(--fbeg))) {
      fend = fbeg;
      while (fend != limit && (!isspace(*fend))) fend++;
      warning ("VAX-C-style include specification found, use '#include <filename.h>' !");
      if (fend  == limit) {
	angle_brackets = 1;
	/* If -I-, start with the first -I dir after the -I-.  */
	if (first_bracket_include)
	  search_start = first_bracket_include;
	break;
      }
    }
#endif

  fail:
    if (retried) {
      error ("`#%s' expects \"FILENAME\" or <FILENAME>", keyword->name);
      return 0;
    } else {
      trybuf = expand_to_temp_buffer (buf, limit, 0, 0);
      buf = (U_CHAR *) alloca (trybuf.bufp - trybuf.buf + 1);
      bcopy ((char *) trybuf.buf, (char *) buf, trybuf.bufp - trybuf.buf);
      limit = buf + (trybuf.bufp - trybuf.buf);
      free (trybuf.buf);
      retried++;
      goto get_filename;
    }
  }

  /* For #include_next, skip in the search path
     past the dir in which the containing file was found.  */
  if (skip_dirs) {
    FILE_BUF *fp;
    for (fp = &instack[indepth]; fp >= instack; fp--)
      if (fp->fname != NULL) {
	/* fp->dir is null if the containing file was specified
	   with an absolute file name.  In that case, don't skip anything.  */
	if (fp->dir)
	  search_start = fp->dir->next;
	break;
      }
  }

  flen = fend - fbeg;

  if (flen == 0)
    {
      error ("empty file name in `#%s'", keyword->name);
      return 0;
    }

#ifdef NEXT_FRAMEWORKS
  if (f > 0 || impfile) {
     fname = savestring(extendedPath);
     goto found_in_framework;
  }
#endif

  /* Allocate this permanently, because it gets stored in the definitions
     of macros.  */
  fname = (char *) xmalloc (max_include_len + flen + 4);
  /* + 2 above for slash and terminating null.  */
  /* + 2 added for '.h' on VMS (to support '#include filename') */

  /* If specified file name is absolute, just open it.  */

#if defined (_WIN32) && defined (NEXT_PDO)
  if ((fbeg[1] == ':') || (*fbeg == '/')) {
#else 
  if (*fbeg == '/') {
#endif
    strncpy (fname, fbeg, flen);
    fname[flen] = 0;
#ifndef NEXT_CPP_SERVER
    if (redundant_include_p (fname)) {
#else
    if (redundant_include_p (fname, &impfile)) {
      free(fname);
#endif
      return 0;
    }
    if (importing)
#ifndef NEXT_CPP_SERVER
      f = lookup_import (fname, NULL_PTR);
#else
      f = lookup_import (fname, &shared_stat_f, &impfile);
#endif
    else {
#ifndef NEXT_CPP_SERVER
      f = open_include_file (fname, NULL_PTR);
#else
      f = open (fname, O_RDONLY, 0666);
      fstat (f, &shared_stat_f);
#endif
    }
    if (f == -2) {
#ifdef NEXT_CPP_SERVER
      free(fname);
#endif
      return 0;		/* Already included this file */
    }
  } else {
    /* Search directory path, trying to open the file.
       Copy each filename tried into FNAME.  */

    for (searchptr = search_start; searchptr; searchptr = searchptr->next) {
      if (searchptr->fname) {
	/* The empty string in a search path is ignored.
	   This makes it possible to turn off entirely
	   a standard piece of the list.  */
	if (searchptr->fname[0] == 0)
	  continue;
	strcpy (fname, searchptr->fname);
	strcat (fname, "/");
	fname[strlen (fname) + flen] = 0;
      } else {
	fname[0] = 0;
      }
      strncat (fname, fbeg, flen);
#ifdef VMS
      /* Change this 1/2 Unix 1/2 VMS file specification into a
         full VMS file specification */
      if (searchptr->fname && (searchptr->fname[0] != 0)) {
	/* Fix up the filename */
	hack_vms_include_specification (fname);
      } else {
      	/* This is a normal VMS filespec, so use it unchanged.  */
	strncpy (fname, fbeg, flen);
	fname[flen] = 0;
	/* if it's '#include filename', add the missing .h */
	if (index(fname,'.')==NULL) {
	  strcat (fname, ".h");
	}
      }
#endif /* VMS */
#ifdef NEXT_CPP_SERVER
      hdr_result = try_include_file(fname, importing, &f, &impfile, &shared_stat_f);
      if (hdr_result == REDUNDANT_HEADER) {
         free(fname);
         return 0;
      }
      else if (hdr_result == GOOD_HEADER)
         break;
#if 0
      /* lookup in negative cache...opens/stats are painfully slow */
      if (!found_in_negative_cache(fname)) {
        impfile = 0;
        if (importing)
	  f = lookup_import (fname, &shared_stat_f, &impfile);
        else {
          redundant_include_p (fname, &impfile);
          if (!impfile)
	    f = open (fname, O_RDONLY, 0666);
          else
             f = -2;
        }
        if (f == -2) {
          if (segregate_output &&
              impfile->visited_within_module &&
              impfile->name && !impfile->name->has_explicit_context_dependency) {
	    free(fname);
	    return 0;			/* Already included this file */
	  }
        }
        else if (f == -1) {
          add_to_negative_cache(savestring(fname));
        }
        else if (!importing) { /* handle the "once idiom" */
          if (impfile && impfile->name->control_macro && 
              lookup (impfile->name->control_macro, -1, -1)) {
	    free(fname);
	    return 0;
	  }
        }
        if (f >= 0 || impfile) {
          if (!importing)
            fstat (f, &shared_stat_f);
	  break;
        }
      } /* else, found in negative cache...don't bother */
#endif 0
#else
      if (importing)
	f = lookup_import (fname, searchptr);
      else
	f = open_include_file (fname, searchptr);
      if (f == -2)
	return 0;			/* Already included this file */
#ifdef EACCES
      else if (f == -1 && errno == EACCES)
	warning ("Header file %s exists, but is not readable", fname);
#endif
      if (redundant_include_p (fname)) {
	close (f);
	return 0;
      }
      if (f >= 0)
	break;
#endif
    }
  }

#ifdef NEXT_FRAMEWORKS
found_in_framework:
#endif
#ifdef NEXT_CPP_SERVER
  if (f < 0 && !impfile) {
#else
  if (f < 0) {
#endif
    /* A file that was not found.  */

    strncpy (fname, fbeg, flen);
    fname[flen] = 0;
    /* If generating dependencies and -MG was specified, we assume missing
       files are leaf files, living in the same directory as the source file
       or other similar place; these missing files may be generated from
       other files and may not exist yet (eg: y.tab.h).  */
    if (print_deps_missing_files
	&& print_deps > (angle_brackets || (system_include_depth > 0)))
      {
	/* If it was requested as a system header file,
	   then assume it belongs in the first place to look for such.  */
	if (angle_brackets)
	  {
	    for (searchptr = search_start; searchptr; searchptr = searchptr->next)
	      {
		if (searchptr->fname)
		  {
		    char *p;

		    if (searchptr->fname[0] == 0)
		      continue;
		    p = xmalloc (strlen (searchptr->fname)
				 + strlen (fname) + 2);
		    strcpy (p, searchptr->fname);
		    strcat (p, "/");
		    strcat (p, fname);
		    deps_output (p, ' ');
		    break;
		  }
	      }
	  }
	else
	  {
	    /* Otherwise, omit the directory, as if the file existed
	       in the directory with the source.  */
	    deps_output (fname, ' ');
	  }
      }
    /* If -M was specified, and this header file won't be added to the
       dependency list, then don't count this as an error, because we can
       still produce correct output.  Otherwise, we can't produce correct
       output, because there may be dependencies we need inside the missing
       file, and we don't know what directory this missing file exists in.  */
    else if (print_deps
	&& (print_deps <= (angle_brackets || (system_include_depth > 0))))
      warning ("No include path in which to find %s", fname);
    else if (search_start)
      error_from_errno (fname);
    else
      error ("No include path in which to find %s", fname);
  } else {
    struct stat stat_f;

    /* Check to see if this include file is a once-only include file.
       If so, give up.  */

    struct file_name_list* ptr;

    for (ptr = dont_repeat_files; ptr; ptr = ptr->next) {
      if (!strcmp (ptr->fname, fname)) {
	close (f);
        return 0;				/* This file was once'd. */
      }
    }

#ifdef NEXT_CPP_SERVER
    if (impfile) {
      ptr = impfile->name;
    }
#else /* this is *very* slow operation when n gets large */
    for (ptr = all_include_files; ptr; ptr = ptr->next) {
      if (!strcmp (ptr->fname, fname))
        break;				/* This file was included before. */
    }
#endif

    if (ptr == 0) {
      /* This is the first time for this file.  */
      /* Add it to list of files included.  */

      ptr = (struct file_name_list *) xmalloc (sizeof (struct file_name_list));
#ifdef NEXT_CPP_SERVER
      bzero (ptr, sizeof (struct file_name_list));
#else
      ptr->control_macro = 0;
      ptr->c_system_include_path = 0;
#endif
      ptr->next = all_include_files;
      all_include_files = ptr;
      ptr->fname = savestring (fname);
      ptr->got_name_map = 0;

      /* For -M, add this file to the dependencies.  */
      if (print_deps > (angle_brackets || (system_include_depth > 0)))
	deps_output (fname, ' ');
    }   

    /* Handle -H option.  */
    if (print_include_names) {
      output_dots (stderr, indepth);
      fprintf (stderr, "%s\n", fname);
    }

    if (angle_brackets)
      system_include_depth++;

#ifdef NEXT_CPP_SERVER
    if (impfile) {
      /* this allows the hash table to be reused across several runs of cpp */
      impfile->visited_within_module++;
    } else
      /* Actually process the file.  */
      impfile = add_import (f, ptr, &shared_stat_f);

    if (!importing)
      ptr->ever_included = 1;
#else
    /* Actually process the file.  */
    add_import (f, fname);	/* Record file on "seen" list for #import. */
#endif

    pcftry = (char *) alloca (strlen (fname) + 30);
    pcfbuf = 0;
    pcfnum = 0;

#ifdef NEXT_CPP_SERVER
    if (!no_precomp) {
      fstat (f, &stat_f);
#else
    fstat (f, &stat_f);

    if (!no_precomp)
#endif
      do {
	sprintf (pcftry, "%s%d", fname, pcfnum++);
	
	pcf = open (pcftry, O_RDONLY, 0666);
	if (pcf != -1)
	  {
	    struct stat s;

	    fstat (pcf, &s);
	    if (bcmp ((char *) &stat_f.st_ino, (char *) &s.st_ino,
		      sizeof (s.st_ino))
		|| stat_f.st_dev != s.st_dev)
	      {
		pcfbuf = check_precompiled (pcf, fname, &pcfbuflimit);
		/* Don't need it any more.  */
		close (pcf);
	      }
	    else
	      {
		/* Don't need it at all.  */
		close (pcf);
		break;
	      }
	  }
      } while (pcf != -1 && !pcfbuf);
#ifdef NEXT_CPP_SERVER
    }
#endif
    
    /* Actually process the file */
    if (pcfbuf) {
      pcfname = xmalloc (strlen (pcftry) + 1);
      strcpy (pcfname, pcftry);
#ifndef NEXT_CPP_SERVER
      pcfinclude (pcfbuf, pcfbuflimit, fname, op);
#else
      pcfinclude (pcfbuf, pcfbuflimit, ptr->fname, op);
#endif
    }
    else
#ifndef NEXT_CPP_SERVER
      finclude (f, fname, op, is_system_include (fname), searchptr);
#else
      finclude (f, ptr->fname, op, is_system_include (ptr->fname), searchptr, 
                ptr, &shared_stat_f);
#endif

    if (angle_brackets)
      system_include_depth--;
  }
#ifdef NEXT_CPP_SERVER
  free (fname); 
#endif
  return 0;
}

/* Return nonzero if there is no need to include file NAME
   because it has already been included and it contains a conditional
   to make a repeated include do nothing.  */

static int
#ifndef NEXT_CPP_SERVER
redundant_include_p (name)
#else
redundant_include_p (name, impfile)
     struct import_file **impfile; /* output parm if found */
#endif
     char *name;
{
#ifndef NEXT_CPP_SERVER
  struct file_name_list *l = all_include_files;
  for (; l; l = l->next)
    if (! strcmp (name, l->fname)
	&& l->control_macro
	&& lookup (l->control_macro, -1, -1))
      return 1;
#else
  struct import_file *i;
  int hashval = import_hash (name);

  /* Attempt to find file in list of already included files */
  for (i = import_hash_table[hashval]; i; i = i->next)
    if (!strcmp (name, i->name->fname)) {
      *impfile = i;
      if (i->name->control_macro && lookup (i->name->control_macro, -1, -1))
        return 1;
      else
        return 0;
    }
#endif
  return 0;
}

/* Return nonzero if the given FILENAME is an absolute pathname which
   designates a file within one of the known "system" include file
   directories.  We assume here that if the given FILENAME looks like
   it is the name of a file which resides either directly in a "system"
   include file directory, or within any subdirectory thereof, then the
   given file must be a "system" include file.  This function tells us
   if we should suppress pedantic errors/warnings for the given FILENAME.

   The value is 2 if the file is a C-language system header file
   for which C++ should (on most systems) assume `extern "C"'.  */

static int
is_system_include (filename)
    register char *filename;
{
  struct file_name_list *searchptr;

  for (searchptr = first_system_include; searchptr;
       searchptr = searchptr->next)
    if (searchptr->fname) {
      register char *sys_dir = searchptr->fname;
      register unsigned length = strlen (sys_dir);

      if (! strncmp (sys_dir, filename, length) && filename[length] == '/')
	{
	  if (searchptr->c_system_include_path)
	    return 2;
	  else
	    return 1;
	}
    }
  return 0;
}

/* The file_name_map structure holds a mapping of file names for a
   particular directory.  This mapping is read from the file named
   FILE_NAME_MAP_FILE in that directory.  Such a file can be used to
   map filenames on a file system with severe filename restrictions,
   such as DOS.  The format of the file name map file is just a series
   of lines with two tokens on each line.  The first token is the name
   to map, and the second token is the actual name to use.  */

struct file_name_map
{
  struct file_name_map *map_next;
  char *map_from;
  char *map_to;
};

#define FILE_NAME_MAP_FILE "header.gcc"

/* Read a space delimited string of unlimited length from a stdio
   file.  */

static char *
read_filename_string (ch, f)
     int ch;
     FILE *f;
{
  char *alloc, *set;
  int len;

  len = 20;
  set = alloc = xmalloc (len + 1);
  if (! is_space[ch])
    {
      *set++ = ch;
      while ((ch = getc (f)) != EOF && ! is_space[ch])
	{
	  if (set - alloc == len)
	    {
	      len *= 2;
	      alloc = xrealloc (alloc, len + 1);
	      set = alloc + len / 2;
	    }
	  *set++ = ch;
	}
    }
  *set = '\0';
  ungetc (ch, f);
  return alloc;
}

/* Read the file name map file for DIRNAME.  */

static struct file_name_map *
read_name_map (dirname)
     char *dirname;
{
  /* This structure holds a linked list of file name maps, one per
     directory.  */
  struct file_name_map_list
    {
      struct file_name_map_list *map_list_next;
      char *map_list_name;
      struct file_name_map *map_list_map;
    };
  static struct file_name_map_list *map_list;
  register struct file_name_map_list *map_list_ptr;
  char *name;
  FILE *f;

  for (map_list_ptr = map_list; map_list_ptr;
       map_list_ptr = map_list_ptr->map_list_next)
    if (! strcmp (map_list_ptr->map_list_name, dirname))
      return map_list_ptr->map_list_map;

  map_list_ptr = ((struct file_name_map_list *)
		  xmalloc (sizeof (struct file_name_map_list)));
  map_list_ptr->map_list_name = savestring (dirname);
  map_list_ptr->map_list_map = NULL;

  name = (char *) alloca (strlen (dirname) + strlen (FILE_NAME_MAP_FILE) + 2);
  strcpy (name, dirname);
  if (*dirname)
    strcat (name, "/");
  strcat (name, FILE_NAME_MAP_FILE);
  f = fopen (name, "r");
  if (!f)
    map_list_ptr->map_list_map = NULL;
  else
    {
      int ch;
      int dirlen = strlen (dirname);

      while ((ch = getc (f)) != EOF)
	{
	  char *from, *to;
	  struct file_name_map *ptr;

	  if (is_space[ch])
	    continue;
	  from = read_filename_string (ch, f);
	  while ((ch = getc (f)) != EOF && is_hor_space[ch])
	    ;
	  to = read_filename_string (ch, f);

	  ptr = ((struct file_name_map *)
		 xmalloc (sizeof (struct file_name_map)));
	  ptr->map_from = from;

	  /* Make the real filename absolute.  */
	  if (*to == '/')
	    ptr->map_to = to;
	  else
	    {
	      ptr->map_to = xmalloc (dirlen + strlen (to) + 2);
	      strcpy (ptr->map_to, dirname);
	      ptr->map_to[dirlen] = '/';
	      strcpy (ptr->map_to + dirlen + 1, to);
	      free (to);
	    }	      

	  ptr->map_next = map_list_ptr->map_list_map;
	  map_list_ptr->map_list_map = ptr;

	  while ((ch = getc (f)) != '\n')
	    if (ch == EOF)
	      break;
	}
      fclose (f);
    }
  
  map_list_ptr->map_list_next = map_list;
  map_list = map_list_ptr;

  return map_list_ptr->map_list_map;
}  

/* Try to open include file FILENAME.  SEARCHPTR is the directory
   being tried from the include file search path.  This function maps
   filenames on file systems based on information read by
   read_name_map.  */

static int
open_include_file (filename, searchptr)
     char *filename;
     struct file_name_list *searchptr;
{
  register struct file_name_map *map;
  register char *from;
  char *p, *dir;

  if (searchptr && ! searchptr->got_name_map)
    {
      searchptr->name_map = read_name_map (searchptr->fname
					   ? searchptr->fname : ".");
      searchptr->got_name_map = 1;
    }

  /* First check the mapping for the directory we are using.  */
  if (searchptr && searchptr->name_map)
    {
      from = filename;
      if (searchptr->fname)
	from += strlen (searchptr->fname) + 1;
      for (map = searchptr->name_map; map; map = map->map_next)
	{
	  if (! strcmp (map->map_from, from))
	    {
	      /* Found a match.  */
	      return open (map->map_to, O_RDONLY, 0666);
	    }
	}
    }

  /* Try to find a mapping file for the particular directory we are
     looking in.  Thus #include <sys/types.h> will look up sys/types.h
     in /usr/include/header.gcc and look up types.h in
     /usr/include/sys/header.gcc.  */
  p = rindex (filename, '/');
  if (! p)
    p = filename;
  if (searchptr
      && searchptr->fname
      && strlen (searchptr->fname) == p - filename
      && ! strncmp (searchptr->fname, filename, p - filename))
    {
      /* FILENAME is in SEARCHPTR, which we've already checked.  */
      return open (filename, O_RDONLY, 0666);
    }

  if (p == filename)
    {
      dir = ".";
      from = filename;
    }
  else
    {
      dir = (char *) alloca (p - filename + 1);
      bcopy (filename, dir, p - filename);
      dir[p - filename] = '\0';
      from = p + 1;
    }
  for (map = read_name_map (dir); map; map = map->map_next)
    if (! strcmp (map->map_from, from))
      return open (map->map_to, O_RDONLY, 0666);

  return open (filename, O_RDONLY, 0666);
}

/* Process the contents of include file FNAME, already open on descriptor F,
   with output to OP.
   SYSTEM_HEADER_P is 1 if this file resides in any one of the known
   "system" include directories (as decided by the `is_system_include'
   function above).
   DIRPTR is the link in the dir path through which this file was found,
   or 0 if the file name was absolute.  */

static void
#ifndef NEXT_CPP_SERVER
finclude (f, fname, op, system_header_p, dirptr)
#else
finclude (f, fname, op, system_header_p, dirptr, fileptr, sb)
     struct file_name_list *fileptr;
     struct stat *sb;
#endif
     int f;
     char *fname;
     FILE_BUF *op;
     int system_header_p;
     struct file_name_list *dirptr;
{
  int st_mode;
  long st_size;
  long i;
  FILE_BUF *fp;			/* For input stack frame */
  int missing_newline = 0;
#ifdef NEXT_CPP_SERVER
  int check_for_duplicate = 0;
  U_CHAR *position_of_file_start;
  unsigned int file_length;
#ifdef SEPARATE_USER_AND_SYS_HEADERS
  FILE_BUF * header_buf = system_header_p ? &sys_header_outbuf
                                          : &user_header_outbuf;
#else
  FILE_BUF * header_buf = &sys_header_outbuf;
#endif
#endif

  CHECK_DEPTH (return;);

#ifdef NEXT_CPP_SERVER
  if (sb == 0) {
    if (file_size_and_mode (f, &st_mode, &st_size) < 0)
      goto nope;		/* Impossible? */
  } else { /* don't redo any stat stuff */
    st_mode = sb->st_mode;
    st_size = sb->st_size;
  }
#else
  if (file_size_and_mode (f, &st_mode, &st_size) < 0)
    {
      perror_with_name (fname);
      close (f);
      return;
    }
#endif

  fp = &instack[indepth + 1];
  bzero ((char *) fp, sizeof (FILE_BUF));
  fp->nominal_fname = fp->fname = fname;
  fp->length = 0;
  fp->lineno = 1;
  fp->if_stack = if_stack;
  fp->system_header_p = system_header_p;
  fp->dir = dirptr;
#ifdef NEXT_CPP_SERVER
  fp->file = fileptr;
  if (fileptr && fileptr->buf && fileptr->length) {
    /*fprintf(stderr, "reusing buffer for file %s\n", fileptr->fname);*/
    fp->buf = fileptr->buf;
    fp->length = fileptr->length;
    if (fileptr->control_macro && fileptr->position_of_first_ifndef)
      /* simple optimization to avoid processing superfluous comments
         at the top of headers (60k savings for appkit.h) */
      fp->bufp = fileptr->position_of_first_ifndef;
    else
      fp->bufp = fileptr->buf;
    goto dorescan;
  }
#endif

  if (S_ISREG (st_mode)) {
#ifdef MALLOC_INPUT_BUFFERS
    fp->buf = (U_CHAR *) xmalloc (st_size + 2);
    fp->bufp = fp->buf;

    /* Read the file contents, knowing that st_size is an upper bound
       on the number of bytes we can read.  */
    fp->length = safe_read (f, fp->buf, st_size);
#else
    if (map_fd(f, 0, &fp->buf, TRUE, st_size+2) != KERN_SUCCESS)
       goto nope;
    fp->bufp = fp->buf;
    fp->length = st_size;    
#endif    
    if (fp->length < 0) goto nope;
  }
  else if (S_ISDIR (st_mode)) {
    error ("directory `%s' specified in #include", fname);
    close (f);
    return;
  } else {
    /* Cannot count its file size before reading.
       First read the entire file into heap and
       copy them into buffer on stack. */

    int bsize = 2000;

    st_size = 0;
    fp->buf = (U_CHAR *) xmalloc (bsize + 2);

    for (;;) {
      i = safe_read (f, fp->buf + st_size, bsize - st_size);
      if (i < 0)
	goto nope;      /* error! */
      st_size += i;
      if (st_size != bsize)
	break;	/* End of file */
      bsize *= 2;
      fp->buf = (U_CHAR *) xrealloc (fp->buf, bsize + 2);
    }
    fp->bufp = fp->buf;
    fp->length = st_size;
  }

  if ((fp->length > 0 && fp->buf[fp->length - 1] != '\n')
      /* Backslash-newline at end is not good enough.  */
      || (fp->length > 1 && fp->buf[fp->length - 2] == '\\')) {
    fp->buf[fp->length++] = '\n';
    missing_newline = 1;
  }
  fp->buf[fp->length] = '\0';

  /* Close descriptor now, so nesting does not use lots of descriptors.  */
  close (f);

  /* Must do this before calling trigraph_pcp, so that the correct file name
     will be printed in warning messages.  */

#ifdef NEXT_CPP_SERVER
dorescan:
  if (fileptr) {
     fileptr->buf = fp->buf;
     fileptr->length = fp->length;
  }
#endif

  indepth++;
  input_file_stack_tick++;

  if (!no_trigraphs)
    trigraph_pcp (fp);

#ifdef NEXT_SEMANTICS
  if (!no_rtf)
    buf_convert_rtf (fp);
#endif

#ifdef NEXT_CPP_SERVER
  if (segregate_output) {
    output_line_command (fp, &outbuf, 0, enter_file); /* serve as a "note" */

    /* the following is a conservative heuristic to prevent us from
       over zealous removal of headers that need to be output more than once */
    if (fileptr->header_output &&
        fileptr->has_explicit_context_dependency &&
        (fileptr->control_macro == 0 && fileptr->ever_included)) {
      check_for_duplicate = 1;
      fileptr->header_output = 0; /* has the effect of including it again */
    }
    position_of_file_start = header_buf->bufp;
    output_line_command (fp, header_buf, 0, enter_file);
    rescan (header_buf, 0);
  } else
#endif
  {
  output_line_command (fp, op, 0, enter_file);
  rescan (op, 0);
  }

  if (missing_newline)
    fp->lineno--;

  if (pedantic && missing_newline)
    pedwarn ("file does not end in newline");

  indepth--;
  input_file_stack_tick++;
#ifdef NEXT_CPP_SERVER
  if (segregate_output) {
    /* It is important to compute the length of what we just output now, since
       the length should NOT include the next end marker, which includes the
       file we are returning to */
    file_length = header_buf->bufp - position_of_file_start;

    if (indepth)
       output_line_command (&instack[indepth], header_buf, 0, leave_file);
    else
       output_line_command (&instack[indepth], &outbuf, 0, enter_file);

    if (check_for_duplicate) { 
      /* file was included twice, if it is identical remove it */
      if (duplicate_version_of_header(header_buf,
             fileptr, position_of_file_start, file_length)) {
        header_buf->bufp = position_of_file_start; /* remove it */
      } else {
        add_version_of_header(fileptr, 
                        position_of_file_start - header_buf->buf,
                        file_length);
      }
      fileptr->header_output = 1; /* put back the original value */
    } else if (!fileptr->header_output) {
      /* first time this header has been output */
      fileptr->relative_position_of_file_start = 
          position_of_file_start - header_buf->buf;
      fileptr->file_length = file_length;
      fileptr->header_output = 1; 
    }
  } else
#endif
  output_line_command (&instack[indepth], op, 0, leave_file);
#ifndef NEXT_CPP_SERVER
  free (fp->buf);
#endif
  return;

 nope:

  perror_with_name (fname);
  close (f);
  free (fp->buf);
}

/* Record that inclusion of the file named FILE
   should be controlled by the macro named MACRO_NAME.
   This means that trying to include the file again
   will do something if that macro is defined.  */

static void
record_control_macro (file, macro_name)
     char *file;
     U_CHAR *macro_name;
{
#ifndef NEXT_CPP_SERVER
  struct file_name_list *new;

  for (new = all_include_files; new; new = new->next) {
    if (!strcmp (new->fname, file)) {
      new->control_macro = macro_name;
      return;
    }
  }
#else
  struct import_file *impfile;

  redundant_include_p(file, &impfile);
  if (impfile) {
     if (impfile->name->control_macro)
        free(impfile->name->control_macro);
    impfile->name->control_macro = macro_name;
    impfile->name->position_of_last_endif = directive_start;
    return;
  }
#endif

  /* If the file is not in all_include_files, something's wrong.  */
  abort ();
}

/* Maintain and search list of included files, for #import.  */

#ifndef NEXT_CPP_SERVER
#define IMPORT_HASH_SIZE 31

/* Hash table of files already included with #include or #import.  */

static struct import_file *import_hash_table[IMPORT_HASH_SIZE];
#endif

/* Hash a file name for import_hash_table.  */

static int 
import_hash (f)
     char *f;
{
  int val = 0;

  while (*f) val += *f++;
  return (val%IMPORT_HASH_SIZE);
}

#if defined (_WIN32) && defined (NEXT_PDO)
/* Check to see if two filenames really refer to the same file.
   Returs 1 if they are, otherwise 0 if they aren't. */

static int
check_import_name (char* name1, char* name2)
{
	char fullpath1[_MAX_PATH];
	char fullpath2[_MAX_PATH];
	char *file1;
	char *file2;

	if (GetFullPathName (name1, _MAX_PATH, fullpath1, &file1))
	{
		if (GetFullPathName (name2, _MAX_PATH, fullpath2, &file2))
		{
			if ( stricmp (fullpath1, fullpath2) == 0)
			{
				return 1;
			}
		}
	}

	return 0;
}
#endif /* _WIN32 */

/* Search for file FILENAME in import_hash_table.
   Return -2 if found, either a matching name or a matching inode.
   Otherwise, open the file and return a file descriptor if successful
   or -1 if unsuccessful.  */

static int
#ifndef NEXT_CPP_SERVER
lookup_import (filename, searchptr)
     char *filename;
     struct file_name_list *searchptr;
#else
lookup_import (filename, sb, impfile)
     char *filename;
     struct stat *sb;
     struct import_file **impfile; /* output parm if found */
#endif
{
  struct import_file *i;
  int h;
  int hashval;
#ifndef NEXT_CPP_SERVER
  struct stat sb;
#endif
  int fd;

  hashval = import_hash (filename);

  /* Attempt to find file in list of already included files */
  i = import_hash_table[hashval];

  while (i) {
#ifndef NEXT_CPP_SERVER
    if (!strcmp (filename, i->name)) {
#else
    if (!strcmp (filename, i->name->fname)) {
      *impfile = i;
#endif
      return -2;		/* return found */
    }
    i = i->next;
  }
  /* Open it and try a match on inode/dev */
#ifndef NEXT_CPP_SERVER
  fd = open_include_file (filename, searchptr);
#else
  fd = open (filename, O_RDONLY, 0666);
#endif
  if (fd < 0)
    return fd;
#ifndef NEXT_CPP_SERVER
  fstat (fd, &sb);
#else
  fstat (fd, sb);
#endif
  for (h = 0; h < IMPORT_HASH_SIZE; h++) {
    i = import_hash_table[h];
    while (i) {
      /* Compare the inode and the device.
	 Supposedly on some systems the inode is not a scalar.  */
#ifdef NEXT_CPP_SERVER
      if (i->inode == sb->st_ino && i->dev == sb->st_dev) {
	*impfile = i;
#elif defined (_WIN32) && defined (NEXT_PDO)
	/* Windows doesn't have inodes or devices.  The structure that fstat
	   fills in will have zeros in those fields so this call will
	   always fail.  Because of this, the only thing we can really 
	   compare is the size, creation time, modification time, and 
	   filename.  */
      if ( (i->size == sb.st_size) 
				&& (i->ctime == sb.st_ctime)
				&& (i->mtime == sb.st_mtime)
				&& (check_import_name (i->name, filename))) {
#else
      if (!bcmp ((char *) &i->inode, (char *) &sb.st_ino, sizeof (sb.st_ino))
	  && i->dev == sb.st_dev) {
#endif
        close (fd);
        return -2;		/* return found */
      }
      i = i->next;
    }
  }
  return fd;			/* Not found, return open file */
}

/* Add the file FNAME, open on descriptor FD, to import_hash_table.  */

#ifndef NEXT_CPP_SERVER
static void
add_import (fd, fname)
     int fd;
     char *fname;
#else
static struct import_file *
add_import (fd, fnamelist, sb)
     int fd;
     struct file_name_list *fnamelist;
     struct stat *sb;
#endif
{
  struct import_file *i;
  int hashval;
#ifndef NEXT_CPP_SERVER
  struct stat sb;

  hashval = import_hash (fname);
  fstat (fd, &sb);
  i = (struct import_file *)xmalloc (sizeof (struct import_file));
  i->name = (char *)xmalloc (strlen (fname)+1);
  strcpy (i->name, fname);
  bcopy ((char *) &sb.st_ino, (char *) &i->inode, sizeof (sb.st_ino));
  i->dev = sb.st_dev;
#else
  hashval = import_hash (fnamelist->fname);
  i = (struct import_file *)xmalloc (sizeof (struct import_file));
  i->name = fnamelist;
  bcopy (&sb->st_ino, &i->inode, sizeof (sb->st_ino));
  i->modtime = sb->st_mtime;
  i->dev = sb->st_dev;
  i->visited_within_module = 1;
#endif
  i->next = import_hash_table[hashval];
#if defined (_WIN32) && defined (NEXT_PDO)
  i->size = sb.st_size;
  i->mtime = sb.st_mtime;
  i->ctime = sb.st_ctime;
#endif
  import_hash_table[hashval] = i;
#ifdef NEXT_CPP_SERVER
  return i;
#endif
}

/* Load the specified precompiled header into core, and verify its
   preconditions.  PCF indicates the file descriptor to read, which must
   be a regular file.  FNAME indicates the file name of the original 
   header.  *LIMIT will be set to an address one past the end of the file.
   If the preconditions of the file are not satisfied, the buffer is 
   freed and we return 0.  If the preconditions are satisfied, return
   the address of the buffer following the preconditions.  The buffer, in
   this case, should never be freed because various pieces of it will
   be referred to until all precompiled strings are output at the end of
   the run.
*/
static char *
check_precompiled (pcf, fname, limit)
     int pcf;
     char *fname;
     char **limit;
{
  int st_mode;
  long st_size;
  int length = 0;
  char *buf;
  char *cp;

  if (pcp_outfile)
    return 0;
  
  if (file_size_and_mode (pcf, &st_mode, &st_size) < 0)
    return 0;

  if (S_ISREG (st_mode))
    {
      buf = xmalloc (st_size + 2);
      length = safe_read (pcf, buf, st_size);
      if (length < 0)
	goto nope;
    }
  else
    abort ();
    
  if (length > 0 && buf[length-1] != '\n')
    buf[length++] = '\n';
  buf[length] = '\0';
  
  *limit = buf + length;

  /* File is in core.  Check the preconditions. */
  if (!check_preconditions (buf))
    goto nope;
  for (cp = buf; *cp; cp++)
    ;
#ifdef DEBUG_PCP
  fprintf (stderr, "Using preinclude %s\n", fname);
#endif
  return cp + 1;

 nope:
#ifdef DEBUG_PCP
  fprintf (stderr, "Cannot use preinclude %s\n", fname);
#endif
  free (buf);
  return 0;
}

/* PREC (null terminated) points to the preconditions of a
   precompiled header.  These are a series of #define and #undef
   lines which must match the current contents of the hash
   table.  */
static int 
check_preconditions (prec)
     char *prec;
{
  MACRODEF mdef;
  char *lineend;
  
  while (*prec) {
    lineend = (char *) index (prec, '\n');
    
    if (*prec++ != '#') {
      error ("Bad format encountered while reading precompiled file");
      return 0;
    }
    if (!strncmp (prec, "define", 6)) {
      HASHNODE *hp;
#ifdef NEXT_CPP_SERVER
      int hashcode;
#endif
      
      prec += 6;
#ifdef NEXT_CPP_SERVER
      mdef = create_definition (prec, lineend, NULL_PTR, &hashcode, 0);
#else
      mdef = create_definition (prec, lineend, NULL_PTR);
#endif

      if (mdef.defn == 0)
	abort ();
      
#ifdef NEXT_CPP_SERVER
      if ((hp = lookup (mdef.symnam, mdef.symlen, hashcode)) == NULL
#else
      if ((hp = lookup (mdef.symnam, mdef.symlen, -1)) == NULL
#endif
	  || (hp->type != T_MACRO && hp->type != T_CONST)
	  || (hp->type == T_MACRO
	      && !compare_defs (mdef.defn, hp->value.defn)
	      && (mdef.defn->length != 2
		  || mdef.defn->expansion[0] != '\n'
		  || mdef.defn->expansion[1] != ' ')))
	return 0;
    } else if (!strncmp (prec, "undef", 5)) {
      char *name;
      int len;
      
      prec += 5;
      while (is_hor_space[(U_CHAR) *prec])
	prec++;
      name = prec;
      while (is_idchar[(U_CHAR) *prec])
	prec++;
      len = prec - name;
      
      if (lookup (name, len, -1))
	return 0;
    } else {
      error ("Bad format encountered while reading precompiled file");
      return 0;
    }
    prec = lineend + 1;
  }
  /* They all passed successfully */
  return 1;
}

/* Process the main body of a precompiled file.  BUF points to the
   string section of the file, following the preconditions.  LIMIT is one
   character past the end.  NAME is the name of the file being read
   in.  OP is the main output buffer */
static void
pcfinclude (buf, limit, name, op)
     U_CHAR *buf, *limit, *name;
     FILE_BUF *op;
{
  FILE_BUF tmpbuf;
  int nstrings;
  U_CHAR *cp = buf;

  /* First in the file comes 4 bytes indicating the number of strings, */
  /* in network byte order. (MSB first).  */
  nstrings = *cp++;
  nstrings = (nstrings << 8) | *cp++;
  nstrings = (nstrings << 8) | *cp++;
  nstrings = (nstrings << 8) | *cp++;
  
  /* Looping over each string... */
  while (nstrings--) {
    U_CHAR *string_start;
    U_CHAR *endofthiskey;
    STRINGDEF *str;
    int nkeys;
    
    /* Each string starts with a STRINGDEF structure (str), followed */
    /* by the text of the string (string_start) */

    /* First skip to a longword boundary */
    /* ??? Why a 4-byte boundary?  On all machines? */
    /* NOTE: This works correctly even if HOST_WIDE_INT
       is narrower than a pointer.
       Do not try risky measures here to get another type to use!
       Do not include stddef.h--it will fail!  */
    if ((HOST_WIDE_INT) cp & 3)
      cp += 4 - ((HOST_WIDE_INT) cp & 3);
    
    /* Now get the string. */
    str = (STRINGDEF *) cp;
    string_start = cp += sizeof (STRINGDEF);
    
    for (; *cp; cp++)		/* skip the string */
      ;
    
    /* We need to macro expand the string here to ensure that the
       proper definition environment is in place.  If it were only
       expanded when we find out it is needed, macros necessary for
       its proper expansion might have had their definitions changed. */
    tmpbuf = expand_to_temp_buffer (string_start, cp++, 0, 0);
    /* Lineno is already set in the precompiled file */
    str->contents = tmpbuf.buf;
    str->len = tmpbuf.length;
    str->writeflag = 0;
    str->filename = name;
#ifdef NEXT_CPP_SERVER
    str->output_mark = op->bufp - op->buf;
#else
    str->output_mark = outbuf.bufp - outbuf.buf;
#endif
    
    str->chain = 0;
    *stringlist_tailp = str;
    stringlist_tailp = &str->chain;
    
    /* Next comes a fourbyte number indicating the number of keys */
    /* for this string. */
    nkeys = *cp++;
    nkeys = (nkeys << 8) | *cp++;
    nkeys = (nkeys << 8) | *cp++;
    nkeys = (nkeys << 8) | *cp++;

    /* If this number is -1, then the string is mandatory. */
    if (nkeys == -1)
      str->writeflag = 1;
    else
      /* Otherwise, for each key, */
      for (; nkeys--; free (tmpbuf.buf), cp = endofthiskey + 1) {
	KEYDEF *kp = (KEYDEF *) cp;
	HASHNODE *hp;
	
	/* It starts with a KEYDEF structure */
	cp += sizeof (KEYDEF);
	
	/* Find the end of the key.  At the end of this for loop we
	   advance CP to the start of the next key using this variable. */
	endofthiskey = cp + strlen (cp);
	kp->str = str;
	
	/* Expand the key, and enter it into the hash table. */
	tmpbuf = expand_to_temp_buffer (cp, endofthiskey, 0, 0);
	tmpbuf.bufp = tmpbuf.buf;
	
	while (is_hor_space[*tmpbuf.bufp])
	  tmpbuf.bufp++;
	if (!is_idstart[*tmpbuf.bufp]
	    || tmpbuf.bufp == tmpbuf.buf + tmpbuf.length) {
	  str->writeflag = 1;
	  continue;
	}
	    
	hp = lookup (tmpbuf.bufp, -1, -1);
	if (hp == NULL) {
	  kp->chain = 0;
	  install (tmpbuf.bufp, -1, T_PCSTRING, 0, (char *) kp, -1);
	}
	else if (hp->type == T_PCSTRING) {
	  kp->chain = hp->value.keydef;
	  hp->value.keydef = kp;
	}
	else
	  str->writeflag = 1;
      }
  }
  /* This output_line_command serves to switch us back to the current
     input file in case some of these strings get output (which will 
     result in line commands for the header file being output). */
  output_line_command (&instack[indepth], op, 0, enter_file);
}

/* Called from rescan when it hits a key for strings.  Mark them all */
 /* used and clean up. */
static void
pcstring_used (hp)
     HASHNODE *hp;
{
  KEYDEF *kp;
  
  for (kp = hp->value.keydef; kp; kp = kp->chain)
    kp->str->writeflag = 1;
  delete_macro (hp);
}

/* Write the output, interspersing precompiled strings in their */
 /* appropriate places. */
static void
#ifdef NEXT_CPP_SERVER
write_output (op, path, write_all)
     FILE_BUF *op;
     const char * path;
     int write_all;
#else
write_output ()
#endif
{
  STRINGDEF *next_string;
  U_CHAR *cur_buf_loc;
  int line_command_len = 80;
  char *line_command = xmalloc (line_command_len);
  int len;

  /* In each run through the loop, either cur_buf_loc == */
  /* next_string_loc, in which case we print a series of strings, or */
  /* it is less than next_string_loc, in which case we write some of */
  /* the buffer. */
#ifdef NEXT_CPP_SERVER
  int output_fd;
  FILE * outs;
  if (!strcmp(path,"stdout"))
     output_fd = fileno(stdout);
  else
     output_fd = open (path, O_CREAT | O_WRONLY | O_TRUNC, 0666);
  if (output_fd < 0) {
     fatal("Could not write file %s", path);
     return;
  }
  cur_buf_loc = write_all ? op->buf : op->output_written_to; 
  next_string = stringlist;
  
  while (cur_buf_loc < op->bufp || next_string) {
    if (next_string
	&& cur_buf_loc - op->buf == next_string->output_mark) {
#else
  cur_buf_loc = outbuf.buf; 
  next_string = stringlist;
  
  while (cur_buf_loc < outbuf.bufp || next_string) {
    if (next_string
	&& cur_buf_loc - outbuf.buf == next_string->output_mark) {
#endif
      if (next_string->writeflag) {
	len = 4 * strlen (next_string->filename) + 32;
	while (len > line_command_len)
	  line_command = xrealloc (line_command, 
				   line_command_len *= 2);
	sprintf (line_command, "\n# %d ", next_string->lineno);
	strcpy (quote_string (line_command + strlen (line_command),
		              next_string->filename),
		"\n");
#ifdef NEXT_CPP_SERVER		
	safe_write (output_fd, line_command, strlen (line_command));
	safe_write (output_fd, next_string->contents, next_string->len);
#else	
	safe_write (fileno (stdout), line_command, strlen (line_command));
	safe_write (fileno (stdout), next_string->contents, next_string->len);
#endif	
      }	      
      next_string = next_string->chain;
    }
    else {
      len = (next_string
	     ? (next_string->output_mark 
#ifdef NEXT_CPP_SERVER
		- (cur_buf_loc - op->buf))
	     : op->bufp - cur_buf_loc);

      safe_write (output_fd, cur_buf_loc, len);
#else
		- (cur_buf_loc - outbuf.buf))
	     : outbuf.bufp - cur_buf_loc);
      
      safe_write (fileno (stdout), cur_buf_loc, len);
#endif
      cur_buf_loc += len;
    }
  }
  free (line_command);
#ifdef NEXT_CPP_SERVER
#if 0
  while (fsync(output_fd)) {
     fprintf(stderr, "error calling fsync(%d) in ", output_fd);
     perror_with_name(path);
     break;  /* FIXME */
  }
#endif
   if (strcmp (path, "stdout") != 0 && close(output_fd)) {
     fprintf(stderr, "error calling close(%d) in ", output_fd);
     perror_with_name(path);
  }
  op->output_written_to = cur_buf_loc;
#endif    
}

#ifdef NEXT_CPP_SERVER
static int
write_header_output (const char * output_fname,
                     FILE_BUF * op,
                     int write_all,
                     int report)
{
   int result;
   if (write_all || (op->output_written_to < op->bufp)) {
      /* more output required */
      write_output (op, output_fname, write_all);
      if (report)
         fprintf(stderr,"wrote headers to %s\n", output_fname);
      result = 1;
   } else {
      /* keep output consistent at 2 lines */
      if (report)
         fprintf(stderr,"did not write headers to %s\n", output_fname);
      result = 0;
   }
   fflush(stderr);
   return result;
}
#endif NEXT_CPP_SERVER

/* Pass a directive through to the output file.
   BUF points to the contents of the directive, as a contiguous string.
   LIMIT points to the first character past the end of the directive.
   KEYWORD is the keyword-table entry for the directive.  */

static void
pass_thru_directive (buf, limit, op, keyword)
     U_CHAR *buf, *limit;
     FILE_BUF *op;
     struct directive *keyword;
{
  register unsigned keyword_length = keyword->length;

  check_expand (op, 1 + keyword_length + (limit - buf));
  *op->bufp++ = '#';
  bcopy (keyword->name, (char *) op->bufp, keyword_length);
  op->bufp += keyword_length;
  if (limit != buf && buf[0] != ' ')
    *op->bufp++ = ' ';
  bcopy ((char *) buf, (char *) op->bufp, limit - buf);
  op->bufp += (limit - buf);
#if 0
  *op->bufp++ = '\n';
  /* Count the line we have just made in the output,
     to get in sync properly.  */
  op->lineno++;
#endif
}

/* The arglist structure is built by do_define to tell
   collect_definition where the argument names begin.  That
   is, for a define like "#define f(x,y,z) foo+x-bar*y", the arglist
   would contain pointers to the strings x, y, and z.
   Collect_definition would then build a DEFINITION node,
   with reflist nodes pointing to the places x, y, and z had
   appeared.  So the arglist is just convenience data passed
   between these two routines.  It is not kept around after
   the current #define has been processed and entered into the
   hash table. */

struct arglist {
  struct arglist *next;
  U_CHAR *name;
  int length;
  int argno;
  char rest_args;
};

#ifdef DEBUG_REDEFINED_MACRO_LEAKS
void foo(char * file, int line) {
   fprintf(stderr,"### About to create macro NL0 for %s:%d ###\n", file, line);
}
#endif

/* Create a DEFINITION node from a #define directive.  Arguments are 
   as for do_define. */
static MACRODEF
#ifndef NEXT_CPP_SERVER
create_definition (buf, limit, op)
#else
create_definition (buf, limit, op, hashcode, hashp)
     int *hashcode;
     HASHNODE **hashp;
#endif
     U_CHAR *buf, *limit;
     FILE_BUF *op;
{
  U_CHAR *bp;			/* temp ptr into input buffer */
  U_CHAR *symname;		/* remember where symbol name starts */
  int sym_length;		/* and how long it is */
  int line = instack[indepth].lineno;
  char *file = instack[indepth].nominal_fname;
  int rest_args = 0;

  DEFINITION *defn;
  int arglengths = 0;		/* Accumulate lengths of arg names
				   plus number of args.  */
  MACRODEF mdef;

  bp = buf;

  while (is_hor_space[*bp])
    bp++;

  symname = bp;			/* remember where it starts */
  sym_length = check_macro_name (bp, "macro");
  bp += sym_length;

#ifdef NEXT_CPP_SERVER
  if (hashp) {
    HASHNODE *hp;
    *hashcode = hashf (symname, sym_length, HASHSIZE);
    if ((hp = lookup (symname, sym_length, *hashcode)) != NULL) {
      *hashp = hp;
      if (hp->type == T_MACRO) {
        /* now that we can process many files, the names should be unique */
        if (strcmp(file, hp->value.defn->file) == 0 &&
            line == hp->value.defn->line) {
          /* reuse macro definition...no need to advance buffer or anything!*/
          defn = hp->value.defn;
          goto reusedefn;
        } 
      }
    }
  }
#endif

#ifdef DEBUG_REDEFINED_MACRO_LEAKS
  /* REMOVE ME!! */
  if (!strncmp(symname, "NL0", sym_length))
     foo(file, line);
#endif
  
  /* Lossage will occur if identifiers or control keywords are broken
     across lines using backslash.  This is not the right place to take
     care of that. */

  if (*bp == '(') {
    struct arglist *arg_ptrs = NULL;
    int argno = 0;

    bp++;			/* skip '(' */
    SKIP_WHITE_SPACE (bp);

    /* Loop over macro argument names.  */
    while (*bp != ')') {
      struct arglist *temp;

      temp = (struct arglist *) alloca (sizeof (struct arglist));
      temp->name = bp;
      temp->next = arg_ptrs;
      temp->argno = argno++;
      temp->rest_args = 0;
      arg_ptrs = temp;

      if (rest_args)
	pedwarn ("another parameter follows `%s'",
		 rest_extension);

      if (!is_idstart[*bp])
	pedwarn ("invalid character in macro parameter name");
      
      /* Find the end of the arg name.  */
      while (is_idchar[*bp]) {
	bp++;
	/* do we have a "special" rest-args extension here? */
	if (limit - bp > REST_EXTENSION_LENGTH &&
	    strncmp (rest_extension, bp, REST_EXTENSION_LENGTH) == 0) {
	  rest_args = 1;
	  temp->rest_args = 1;
	  break;
	}
      }
      temp->length = bp - temp->name;
      if (rest_args == 1)
	bp += REST_EXTENSION_LENGTH;
      arglengths += temp->length + 2;
      SKIP_WHITE_SPACE (bp);
      if (temp->length == 0 || (*bp != ',' && *bp != ')')) {
	error ("badly punctuated parameter list in `#define'");
	goto nope;
      }
      if (*bp == ',') {
	bp++;
	SKIP_WHITE_SPACE (bp);
      }
      if (bp >= limit) {
	error ("unterminated parameter list in `#define'");
	goto nope;
      }
      {
	struct arglist *otemp;

	for (otemp = temp->next; otemp != NULL; otemp = otemp->next)
	  if (temp->length == otemp->length &&
	    strncmp (temp->name, otemp->name, temp->length) == 0) {
	      U_CHAR *name;

	      name = (U_CHAR *) alloca (temp->length + 1);
	      (void) strncpy (name, temp->name, temp->length);
	      name[temp->length] = '\0';
	      error ("duplicate argument name `%s' in `#define'", name);
	      goto nope;
	  }
      }
    }

    ++bp;			/* skip paren */
    /* Skip spaces and tabs if any.  */
    while (bp < limit && (*bp == ' ' || *bp == '\t'))
      ++bp;
    /* now everything from bp before limit is the definition. */
    defn = collect_expansion (bp, limit, argno, arg_ptrs);
    defn->rest_args = rest_args;

    /* Now set defn->args.argnames to the result of concatenating
       the argument names in reverse order
       with comma-space between them.  */
    defn->args.argnames = (U_CHAR *) xmalloc (arglengths + 1);
    {
      struct arglist *temp;
      int i = 0;
      for (temp = arg_ptrs; temp; temp = temp->next) {
	bcopy (temp->name, &defn->args.argnames[i], temp->length);
	i += temp->length;
	if (temp->next != 0) {
	  defn->args.argnames[i++] = ',';
	  defn->args.argnames[i++] = ' ';
	}
      }
      defn->args.argnames[i] = 0;
    }
  } else {
    /* Simple expansion or empty definition.  */

    if (bp < limit)
      {
	switch (*bp)
	  {
	    case '\t': case ' ': case '\r':
	      /* Skip spaces and tabs.  */
	      while (++bp < limit && (*bp == ' ' || *bp == '\t' || *bp == '\r'))
		continue;
	      break;

	    case '!':  case '"':  case '#':  case '%':  case '&':  case '\'':
	    case ')':  case '*':  case '+':  case ',':  case '-':  case '.':
	    case '/':  case ':':  case ';':  case '<':  case '=':  case '>':
	    case '?':  case '[':  case '\\': case ']':  case '^':  case '{':
	    case '|':  case '}':  case '~':
	      warning ("missing white space after `#define %.*s'",
		       sym_length, symname);
	      break;

	    default:
	      pedwarn ("missing white space after `#define %.*s'",
		       sym_length, symname);
	      break;
	  }
      }
    /* Now everything from bp before limit is the definition. */
    defn = collect_expansion (bp, limit, -1, NULL_PTR);
    defn->args.argnames = (U_CHAR *) "";
  }

  defn->line = line;
  defn->file = file;

  /* OP is null if this is a predefinition */
  defn->predefined = !op;
#ifdef NEXT_CPP_SERVER
reusedefn:
#endif
  mdef.defn = defn;
  mdef.symnam = symname;
  mdef.symlen = sym_length;

  return mdef;

 nope:
  mdef.defn = 0;
  return mdef;
}
 
/* Process a #define command.
BUF points to the contents of the #define command, as a contiguous string.
LIMIT points to the first character past the end of the definition.
KEYWORD is the keyword-table entry for #define.  */

static int
do_define (buf, limit, op, keyword)
     U_CHAR *buf, *limit;
     FILE_BUF *op;
     struct directive *keyword;
{
  int hashcode;
  MACRODEF mdef;
#ifdef NEXT_CPP_SERVER
  HASHNODE *hp = 0;
#endif

  /* If this is a precompiler run (with -pcp) pass thru #define commands.  */
  if (pcp_outfile && op)
    pass_thru_directive (buf, limit, op, keyword);

#ifdef NEXT_CPP_SERVER
  mdef = create_definition (buf, limit, op, &hashcode, &hp);
#else
  mdef = create_definition (buf, limit, op);
#endif
  if (mdef.defn == 0)
    goto nope;

#ifdef NEXT_CPP_SERVER
  if (hp) {
    if (hp->value.defn == mdef.defn) {
      hp->defined_within_module = 1;
    } else {
#else
  hashcode = hashf (mdef.symnam, mdef.symlen, HASHSIZE);

  {
    HASHNODE *hp;
    if ((hp = lookup (mdef.symnam, mdef.symlen, hashcode)) != NULL) {
#endif
      int ok = 0;
      /* Redefining a precompiled key is ok.  */
      if (hp->type == T_PCSTRING)
	ok = 1;
      /* Redefining a macro is ok if the definitions are the same.  */
      else if (hp->type == T_MACRO) {
	ok = ! compare_defs (mdef.defn, hp->value.defn);
#ifdef NEXT_CPP_SERVER
        if (!ok && !hp->defined_within_module) {
          hp->value.defn = mdef.defn;
          ok = 1; /* calm down, everything is ok now */
        }
        hp->defined_within_module = 1;
        /*fprintf(stderr, "reusing macro %.10s\n", mdef.symnam); */
#endif
      }
      /* Redefining a constant is ok with -D.  */
      else if (hp->type == T_CONST)
        ok = ! done_initializing;
      /* Print the warning if it's not ok.  */
      if (!ok) {
	U_CHAR *msg;		/* what pain... */

        /* If we are passing through #define and #undef directives, do
	   that for this re-definition now.  */
        if (debug_output && op)
	  pass_thru_directive (buf, limit, op, keyword);

	msg = (U_CHAR *) alloca (mdef.symlen + 22);
	*msg = '`';
	bcopy ((char *) mdef.symnam, (char *) (msg + 1), mdef.symlen);
	strcpy ((char *) (msg + mdef.symlen + 1), "' redefined");
	pedwarn (msg);
	if (hp->type == T_MACRO)
	  pedwarn_with_file_and_line (hp->value.defn->file, hp->value.defn->line,
				      "this is the location of the previous definition");
      }
      /* Replace the old definition.  */
      hp->type = T_MACRO;
      hp->value.defn = mdef.defn;
#ifdef NEXT_CPP_SERVER
      }
#endif
    } else {
      /* If we are passing through #define and #undef directives, do
	 that for this new definition now.  */
      if (debug_output && op)
	pass_thru_directive (buf, limit, op, keyword);
#ifdef NEXT_CPP_SERVER
      hp = install (mdef.symnam, mdef.symlen, T_MACRO, 0,
                    (char *) mdef.defn, hashcode);
      if (segregate_output && 
          instack[indepth].file && !instack[indepth].file->macro_list) {
        hp->owner = instack[indepth].file;
        hp->owner->macro_count++;
      }
#else
      install (mdef.symnam, mdef.symlen, T_MACRO, 0,
	       (char *) mdef.defn, hashcode);
    }
#endif
  }

  return 0;

nope:

  return 1;
}

/* Check a purported macro name SYMNAME, and yield its length.
   USAGE is the kind of name this is intended for.  */

static int
check_macro_name (symname, usage)
     U_CHAR *symname;
     char *usage;
{
  U_CHAR *p;
  int sym_length;

  for (p = symname; is_idchar[*p]; p++)
    ;
  sym_length = p - symname;
  if (sym_length == 0)
    error ("invalid %s name", usage);
  else if (!is_idstart[*symname]) {
    U_CHAR *msg;			/* what pain... */
    msg = (U_CHAR *) alloca (sym_length + 1);
    bcopy ((char *) symname, (char *) msg, sym_length);
    msg[sym_length] = 0;
    error ("invalid %s name `%s'", usage, msg);
  } else {
    if (! strncmp (symname, "defined", 7) && sym_length == 7)
      error ("invalid %s name `defined'", usage);
  }
  return sym_length;
}

/*
 * return zero if two DEFINITIONs are isomorphic
 */
static int
compare_defs (d1, d2)
     DEFINITION *d1, *d2;
{
  register struct reflist *a1, *a2;
  register U_CHAR *p1 = d1->expansion;
  register U_CHAR *p2 = d2->expansion;
  int first = 1;

  if (d1->nargs != d2->nargs)
    return 1;
  if (strcmp ((char *)d1->args.argnames, (char *)d2->args.argnames))
    return 1;
  for (a1 = d1->pattern, a2 = d2->pattern; a1 && a2;
       a1 = a1->next, a2 = a2->next) {
    if (!((a1->nchars == a2->nchars && ! strncmp (p1, p2, a1->nchars))
	  || ! comp_def_part (first, p1, a1->nchars, p2, a2->nchars, 0))
	|| a1->argno != a2->argno
	|| a1->stringify != a2->stringify
	|| a1->raw_before != a2->raw_before
	|| a1->raw_after != a2->raw_after)
      return 1;
    first = 0;
    p1 += a1->nchars;
    p2 += a2->nchars;
  }
  if (a1 != a2)
    return 1;
  if (comp_def_part (first, p1, d1->length - (p1 - d1->expansion),
		     p2, d2->length - (p2 - d2->expansion), 1))
    return 1;
  return 0;
}

/* Return 1 if two parts of two macro definitions are effectively different.
   One of the parts starts at BEG1 and has LEN1 chars;
   the other has LEN2 chars at BEG2.
   Any sequence of whitespace matches any other sequence of whitespace.
   FIRST means these parts are the first of a macro definition;
    so ignore leading whitespace entirely.
   LAST means these parts are the last of a macro definition;
    so ignore trailing whitespace entirely.  */

static int
comp_def_part (first, beg1, len1, beg2, len2, last)
     int first;
     U_CHAR *beg1, *beg2;
     int len1, len2;
     int last;
{
  register U_CHAR *end1 = beg1 + len1;
  register U_CHAR *end2 = beg2 + len2;
  if (first) {
    while (beg1 != end1 && is_space[*beg1]) beg1++;
    while (beg2 != end2 && is_space[*beg2]) beg2++;
  }
  if (last) {
    while (beg1 != end1 && is_space[end1[-1]]) end1--;
    while (beg2 != end2 && is_space[end2[-1]]) end2--;
  }
  while (beg1 != end1 && beg2 != end2) {
    if (is_space[*beg1] && is_space[*beg2]) {
      while (beg1 != end1 && is_space[*beg1]) beg1++;
      while (beg2 != end2 && is_space[*beg2]) beg2++;
    } else if (*beg1 == *beg2) {
      beg1++; beg2++;
    } else break;
  }
  return (beg1 != end1) || (beg2 != end2);
}

/* Read a replacement list for a macro with parameters.
   Build the DEFINITION structure.
   Reads characters of text starting at BUF until END.
   ARGLIST specifies the formal parameters to look for
   in the text of the definition; NARGS is the number of args
   in that list, or -1 for a macro name that wants no argument list.
   MACRONAME is the macro name itself (so we can avoid recursive expansion)
   and NAMELEN is its length in characters.
   
Note that comments and backslash-newlines have already been deleted
from the argument.  */

/* Leading and trailing Space, Tab, etc. are converted to markers
   Newline Space, Newline Tab, etc.
   Newline Space makes a space in the final output
   but is discarded if stringified.  (Newline Tab is similar but
   makes a Tab instead.)

   If there is no trailing whitespace, a Newline Space is added at the end
   to prevent concatenation that would be contrary to the standard.  */

static DEFINITION *
collect_expansion (buf, end, nargs, arglist)
     U_CHAR *buf, *end;
     int nargs;
     struct arglist *arglist;
{
  DEFINITION *defn;
  register U_CHAR *p, *limit, *lastp, *exp_p;
  struct reflist *endpat = NULL;
  /* Pointer to first nonspace after last ## seen.  */
  U_CHAR *concat = 0;
  /* Pointer to first nonspace after last single-# seen.  */
  U_CHAR *stringify = 0;
  int maxsize;
  int expected_delimiter = '\0';

  /* Scan thru the replacement list, ignoring comments and quoted
     strings, picking up on the macro calls.  It does a linear search
     thru the arg list on every potential symbol.  Profiling might say
     that something smarter should happen. */

  if (end < buf)
    abort ();

  /* Find the beginning of the trailing whitespace.  */
  /* Find end of leading whitespace.  */
  limit = end;
  p = buf;
  while (p < limit && is_space[limit[-1]]) limit--;
  while (p < limit && is_space[*p]) p++;

  /* Allocate space for the text in the macro definition.
     Leading and trailing whitespace chars need 2 bytes each.
     Each other input char may or may not need 1 byte,
     so this is an upper bound.
     The extra 2 are for invented trailing newline-marker and final null.  */
  maxsize = (sizeof (DEFINITION)
	     + 2 * (end - limit) + 2 * (p - buf)
	     + (limit - p) + 3);
  defn = (DEFINITION *) xcalloc (1, maxsize);

  defn->nargs = nargs;
  exp_p = defn->expansion = (U_CHAR *) defn + sizeof (DEFINITION);
  lastp = exp_p;

  p = buf;

  /* Convert leading whitespace to Newline-markers.  */
  while (p < limit && is_space[*p]) {
    *exp_p++ = '\n';
    *exp_p++ = *p++;
  }

  if (limit - p >= 2 && p[0] == '#' && p[1] == '#') {
    error ("`##' at start of macro definition");
    p += 2;
  }

  /* Process the main body of the definition.  */
  while (p < limit) {
    int skipped_arg = 0;
    register U_CHAR c = *p++;

    *exp_p++ = c;

    if (!traditional) {
      switch (c) {
      case '\'':
      case '\"':
        if (expected_delimiter != '\0') {
          if (c == expected_delimiter)
            expected_delimiter = '\0';
        } else
          expected_delimiter = c;
	break;

      case '\\':
	if (p < limit && expected_delimiter) {
	  /* In a string, backslash goes through
	     and makes next char ordinary.  */
	  *exp_p++ = *p++;
	}
	break;

      case '#':
	/* # is ordinary inside a string.  */
	if (expected_delimiter)
	  break;
	if (p < limit && *p == '#') {
	  /* ##: concatenate preceding and following tokens.  */
	  /* Take out the first #, discard preceding whitespace.  */
	  exp_p--;
	  while (exp_p > lastp && is_hor_space[exp_p[-1]])
	    --exp_p;
	  /* Skip the second #.  */
	  p++;
	  /* Discard following whitespace.  */
	  SKIP_WHITE_SPACE (p);
	  concat = p;
	  if (p == limit)
	    error ("`##' at end of macro definition");
	} else if (nargs >= 0) {
	  /* Single #: stringify following argument ref.
	     Don't leave the # in the expansion.  */
	  exp_p--;
	  SKIP_WHITE_SPACE (p);
	  if (p == limit || ! is_idstart[*p])
	    error ("`#' operator is not followed by a macro argument name");
	  else
	    stringify = p;
	}
	break;
      }
    } else {
      /* In -traditional mode, recognize arguments inside strings and
	 and character constants, and ignore special properties of #.
	 Arguments inside strings are considered "stringified", but no
	 extra quote marks are supplied.  */
      switch (c) {
      case '\'':
      case '\"':
	if (expected_delimiter != '\0') {
	  if (c == expected_delimiter)
	    expected_delimiter = '\0';
	} else
	  expected_delimiter = c;
	break;

      case '\\':
	/* Backslash quotes delimiters and itself, but not macro args.  */
	if (expected_delimiter != 0 && p < limit
	    && (*p == expected_delimiter || *p == '\\')) {
	  *exp_p++ = *p++;
	  continue;
	}
	break;

      case '/':
	if (expected_delimiter != '\0') /* No comments inside strings.  */
	  break;
	if (*p == '*') {
	  /* If we find a comment that wasn't removed by handle_directive,
	     this must be -traditional.  So replace the comment with
	     nothing at all.  */
	  exp_p--;
	  p += 1;
	  while (p < limit && !(p[-2] == '*' && p[-1] == '/'))
	    p++;
#if 0
	  /* Mark this as a concatenation-point, as if it had been ##.  */
	  concat = p;
#endif
	}
	break;
      }
    }

    /* Handle the start of a symbol.  */
    if (is_idchar[c] && nargs > 0) {
      U_CHAR *id_beg = p - 1;
      int id_len;

      --exp_p;
      while (p != limit && is_idchar[*p]) p++;
      id_len = p - id_beg;

      if (is_idstart[c]) {
	register struct arglist *arg;

	for (arg = arglist; arg != NULL; arg = arg->next) {
	  struct reflist *tpat;

	  if (arg->name[0] == c
	      && arg->length == id_len
	      && strncmp (arg->name, id_beg, id_len) == 0) {
	    if (expected_delimiter && warn_stringify) {
	      if (traditional) {
		warning ("macro argument `%.*s' is stringified.",
			 id_len, arg->name);
	      } else {
		warning ("macro arg `%.*s' would be stringified with -traditional.",
			 id_len, arg->name);
	      }
	    }
	    /* If ANSI, don't actually substitute inside a string.  */
	    if (!traditional && expected_delimiter)
	      break;
	    /* make a pat node for this arg and append it to the end of
	       the pat list */
	    tpat = (struct reflist *) xmalloc (sizeof (struct reflist));
	    tpat->next = NULL;
	    tpat->raw_before = concat == id_beg;
	    tpat->raw_after = 0;
	    tpat->rest_args = arg->rest_args;
	    tpat->stringify = (traditional ? expected_delimiter != '\0'
			       : stringify == id_beg);

	    if (endpat == NULL)
	      defn->pattern = tpat;
	    else
	      endpat->next = tpat;
	    endpat = tpat;

	    tpat->argno = arg->argno;
	    tpat->nchars = exp_p - lastp;
	    {
	      register U_CHAR *p1 = p;
	      SKIP_WHITE_SPACE (p1);
	      if (p1 + 2 <= limit && p1[0] == '#' && p1[1] == '#')
		tpat->raw_after = 1;
	    }
	    lastp = exp_p;	/* place to start copying from next time */
	    skipped_arg = 1;
	    break;
	  }
	}
      }

      /* If this was not a macro arg, copy it into the expansion.  */
      if (! skipped_arg) {
	register U_CHAR *lim1 = p;
	p = id_beg;
	while (p != lim1)
	  *exp_p++ = *p++;
	if (stringify == id_beg)
	  error ("`#' operator should be followed by a macro argument name");
      }
    }
  }

  if (!traditional && expected_delimiter == 0) {
    /* There is no trailing whitespace, so invent some in ANSI mode.
       But not if "inside a string" (which in ANSI mode
       happens only for -D option).  */
    *exp_p++ = '\n';
    *exp_p++ = ' ';
  }

  *exp_p = '\0';

  defn->length = exp_p - defn->expansion;

  /* Crash now if we overrun the allocated size.  */
  if (defn->length + 1 > maxsize)
    abort ();

#if 0
/* This isn't worth the time it takes.  */
  /* give back excess storage */
  defn->expansion = (U_CHAR *) xrealloc (defn->expansion, defn->length + 1);
#endif

  return defn;
}

static int
do_assert (buf, limit, op, keyword)
     U_CHAR *buf, *limit;
     FILE_BUF *op;
     struct directive *keyword;
{
  U_CHAR *bp;			/* temp ptr into input buffer */
  U_CHAR *symname;		/* remember where symbol name starts */
  int sym_length;		/* and how long it is */
  struct arglist *tokens = NULL;

  if (pedantic && done_initializing && !instack[indepth].system_header_p)
    pedwarn ("ANSI C does not allow `#assert'");

  bp = buf;

  while (is_hor_space[*bp])
    bp++;

  symname = bp;			/* remember where it starts */
  sym_length = check_macro_name (bp, "assertion");
  bp += sym_length;
  /* #define doesn't do this, but we should.  */
  SKIP_WHITE_SPACE (bp);

  /* Lossage will occur if identifiers or control tokens are broken
     across lines using backslash.  This is not the right place to take
     care of that. */

  if (*bp != '(') {
    error ("missing token-sequence in `#assert'");
    return 1;
  }

  {
    int error_flag = 0;

    bp++;			/* skip '(' */
    SKIP_WHITE_SPACE (bp);

    tokens = read_token_list (&bp, limit, &error_flag);
    if (error_flag)
      return 1;
    if (tokens == 0) {
      error ("empty token-sequence in `#assert'");
      return 1;
    }

    ++bp;			/* skip paren */
    SKIP_WHITE_SPACE (bp);
  }

  /* If this name isn't already an assertion name, make it one.
     Error if it was already in use in some other way.  */

  {
    ASSERTION_HASHNODE *hp;
    int hashcode = hashf (symname, sym_length, ASSERTION_HASHSIZE);
    struct tokenlist_list *value
      = (struct tokenlist_list *) xmalloc (sizeof (struct tokenlist_list));

    hp = assertion_lookup (symname, sym_length, hashcode);
    if (hp == NULL) {
      if (sym_length == 7 && ! strncmp (symname, "defined", sym_length))
	error ("`defined' redefined as assertion");
      hp = assertion_install (symname, sym_length, hashcode);
    }

    /* Add the spec'd token-sequence to the list of such.  */
    value->tokens = tokens;
    value->next = hp->value;
    hp->value = value;
  }

  return 0;
}

static int
do_unassert (buf, limit, op, keyword)
     U_CHAR *buf, *limit;
     FILE_BUF *op;
     struct directive *keyword;
{
  U_CHAR *bp;			/* temp ptr into input buffer */
  U_CHAR *symname;		/* remember where symbol name starts */
  int sym_length;		/* and how long it is */

  struct arglist *tokens = NULL;
  int tokens_specified = 0;

  if (pedantic && done_initializing && !instack[indepth].system_header_p)
    pedwarn ("ANSI C does not allow `#unassert'");

  bp = buf;

  while (is_hor_space[*bp])
    bp++;

  symname = bp;			/* remember where it starts */
  sym_length = check_macro_name (bp, "assertion");
  bp += sym_length;
  /* #define doesn't do this, but we should.  */
  SKIP_WHITE_SPACE (bp);

  /* Lossage will occur if identifiers or control tokens are broken
     across lines using backslash.  This is not the right place to take
     care of that. */

  if (*bp == '(') {
    int error_flag = 0;

    bp++;			/* skip '(' */
    SKIP_WHITE_SPACE (bp);

    tokens = read_token_list (&bp, limit, &error_flag);
    if (error_flag)
      return 1;
    if (tokens == 0) {
      error ("empty token list in `#unassert'");
      return 1;
    }

    tokens_specified = 1;

    ++bp;			/* skip paren */
    SKIP_WHITE_SPACE (bp);
  }

  {
    ASSERTION_HASHNODE *hp;
    int hashcode = hashf (symname, sym_length, ASSERTION_HASHSIZE);
    struct tokenlist_list *tail, *prev;

    hp = assertion_lookup (symname, sym_length, hashcode);
    if (hp == NULL)
      return 1;

    /* If no token list was specified, then eliminate this assertion
       entirely.  */
    if (! tokens_specified) {
      struct tokenlist_list *next;
      for (tail = hp->value; tail; tail = next) {
	next = tail->next;
	free_token_list (tail->tokens);
	free (tail);
      }
      delete_assertion (hp);
    } else {
      /* If a list of tokens was given, then delete any matching list.  */

      tail = hp->value;
      prev = 0;
      while (tail) {
	struct tokenlist_list *next = tail->next;
	if (compare_token_lists (tail->tokens, tokens)) {
	  if (prev)
	    prev->next = next;
	  else
	    hp->value = tail->next;
	  free_token_list (tail->tokens);
	  free (tail);
	} else {
	  prev = tail;
	}
	tail = next;
      }
    }
  }

  return 0;
}

/* Test whether there is an assertion named NAME
   and optionally whether it has an asserted token list TOKENS.
   NAME is not null terminated; its length is SYM_LENGTH.
   If TOKENS_SPECIFIED is 0, then don't check for any token list.  */

int
check_assertion (name, sym_length, tokens_specified, tokens)
     U_CHAR *name;
     int sym_length;
     int tokens_specified;
     struct arglist *tokens;
{
  ASSERTION_HASHNODE *hp;
  int hashcode = hashf (name, sym_length, ASSERTION_HASHSIZE);

  if (pedantic && !instack[indepth].system_header_p)
    pedwarn ("ANSI C does not allow testing assertions");

  hp = assertion_lookup (name, sym_length, hashcode);
  if (hp == NULL)
    /* It is not an assertion; just return false.  */
    return 0;

  /* If no token list was specified, then value is 1.  */
  if (! tokens_specified)
    return 1;

  {
    struct tokenlist_list *tail;

    tail = hp->value;

    /* If a list of tokens was given,
       then succeed if the assertion records a matching list.  */

    while (tail) {
      if (compare_token_lists (tail->tokens, tokens))
	return 1;
      tail = tail->next;
    }

    /* Fail if the assertion has no matching list.  */
    return 0;
  }
}

/* Compare two lists of tokens for equality including order of tokens.  */

static int
compare_token_lists (l1, l2)
     struct arglist *l1, *l2;
{
  while (l1 && l2) {
    if (l1->length != l2->length)
      return 0;
    if (strncmp (l1->name, l2->name, l1->length))
      return 0;
    l1 = l1->next;
    l2 = l2->next;
  }

  /* Succeed if both lists end at the same time.  */
  return l1 == l2;
}

/* Read a space-separated list of tokens ending in a close parenthesis.
   Return a list of strings, in the order they were written.
   (In case of error, return 0 and store -1 in *ERROR_FLAG.)
   Parse the text starting at *BPP, and update *BPP.
   Don't parse beyond LIMIT.  */

static struct arglist *
read_token_list (bpp, limit, error_flag)
     U_CHAR **bpp;
     U_CHAR *limit;
     int *error_flag;
{
  struct arglist *token_ptrs = 0;
  U_CHAR *bp = *bpp;
  int depth = 1;

  *error_flag = 0;

  /* Loop over the assertion value tokens.  */
  while (depth > 0) {
    struct arglist *temp;
    int eofp = 0;
    U_CHAR *beg = bp;

    /* Find the end of the token.  */
    if (*bp == '(') {
      bp++;
      depth++;
    } else if (*bp == ')') {
      depth--;
      if (depth == 0)
	break;
      bp++;
    } else if (*bp == '"' || *bp == '\'')
      bp = skip_quoted_string (bp, limit, 0, NULL_PTR, NULL_PTR, &eofp);
    else
      while (! is_hor_space[*bp] && *bp != '(' && *bp != ')'
	     && *bp != '"' && *bp != '\'' && bp != limit)
	bp++;

    temp = (struct arglist *) xmalloc (sizeof (struct arglist));
    temp->name = (U_CHAR *) xmalloc (bp - beg + 1);
    bcopy ((char *) beg, (char *) temp->name, bp - beg);
    temp->name[bp - beg] = 0;
    temp->next = token_ptrs;
    token_ptrs = temp;
    temp->length = bp - beg;

    SKIP_WHITE_SPACE (bp);

    if (bp >= limit) {
      error ("unterminated token sequence in `#assert' or `#unassert'");
      *error_flag = -1;
      return 0;
    }
  }
  *bpp = bp;

  /* We accumulated the names in reverse order.
     Now reverse them to get the proper order.  */
  {
    register struct arglist *prev = 0, *this, *next;
    for (this = token_ptrs; this; this = next) {
      next = this->next;
      this->next = prev;
      prev = this;
    }
    return prev;
  }
}

static void
free_token_list (tokens)
     struct arglist *tokens;
{
  while (tokens) {
    struct arglist *next = tokens->next;
    free (tokens->name);
    free (tokens);
    tokens = next;
  }
}

/*
 * Install a name in the assertion hash table.
 *
 * If LEN is >= 0, it is the length of the name.
 * Otherwise, compute the length by scanning the entire name.
 *
 * If HASH is >= 0, it is the precomputed hash code.
 * Otherwise, compute the hash code.
 */
static ASSERTION_HASHNODE *
assertion_install (name, len, hash)
     U_CHAR *name;
     int len;
     int hash;
{
  register ASSERTION_HASHNODE *hp;
  register int i, bucket;
  register U_CHAR *p, *q;

  i = sizeof (ASSERTION_HASHNODE) + len + 1;
  hp = (ASSERTION_HASHNODE *) xmalloc (i);
  bucket = hash;
  hp->bucket_hdr = &assertion_hashtab[bucket];
  hp->next = assertion_hashtab[bucket];
  assertion_hashtab[bucket] = hp;
  hp->prev = NULL;
  if (hp->next != NULL)
    hp->next->prev = hp;
  hp->length = len;
  hp->value = 0;
  hp->name = ((U_CHAR *) hp) + sizeof (ASSERTION_HASHNODE);
  p = hp->name;
  q = name;
  for (i = 0; i < len; i++)
    *p++ = *q++;
  hp->name[len] = 0;
  return hp;
}

/*
 * find the most recent hash node for name name (ending with first
 * non-identifier char) installed by install
 *
 * If LEN is >= 0, it is the length of the name.
 * Otherwise, compute the length by scanning the entire name.
 *
 * If HASH is >= 0, it is the precomputed hash code.
 * Otherwise, compute the hash code.
 */
static ASSERTION_HASHNODE *
assertion_lookup (name, len, hash)
     U_CHAR *name;
     int len;
     int hash;
{
  register ASSERTION_HASHNODE *bucket;

  bucket = assertion_hashtab[hash];
  while (bucket) {
    if (bucket->length == len && strncmp (bucket->name, name, len) == 0)
      return bucket;
    bucket = bucket->next;
  }
  return NULL;
}

static void
delete_assertion (hp)
     ASSERTION_HASHNODE *hp;
{

  if (hp->prev != NULL)
    hp->prev->next = hp->next;
  if (hp->next != NULL)
    hp->next->prev = hp->prev;

  /* make sure that the bucket chain header that
     the deleted guy was on points to the right thing afterwards. */
  if (hp == *hp->bucket_hdr)
    *hp->bucket_hdr = hp->next;

  free (hp);
}

/*
 * interpret #line command.  Remembers previously seen fnames
 * in its very own hash table.
 */
#define FNAME_HASHSIZE 37

static int
do_line (buf, limit, op, keyword)
     U_CHAR *buf, *limit;
     FILE_BUF *op;
     struct directive *keyword;
{
  register U_CHAR *bp;
  FILE_BUF *ip = &instack[indepth];
  FILE_BUF tem;
  int new_lineno;
  enum file_change_code file_change = same_file;

  /* Expand any macros.  */
  tem = expand_to_temp_buffer (buf, limit, 0, 0);

  /* Point to macroexpanded line, which is null-terminated now.  */
  bp = tem.buf;
  SKIP_WHITE_SPACE (bp);

  if (!isdigit (*bp)) {
    error ("invalid format `#line' command");
    return 0;
  }

  /* The Newline at the end of this line remains to be processed.
     To put the next line at the specified line number,
     we must store a line number now that is one less.  */
  new_lineno = atoi (bp) - 1;

  /* NEW_LINENO is one less than the actual line number here.  */
  if (pedantic && new_lineno < 0)
    pedwarn ("line number out of range in `#line' command");

  /* skip over the line number.  */
  while (isdigit (*bp))
    bp++;

#if 0 /* #line 10"foo.c" is supposed to be allowed.  */
  if (*bp && !is_space[*bp]) {
    error ("invalid format `#line' command");
    return;
  }
#endif

  SKIP_WHITE_SPACE (bp);

  if (*bp == '\"') {
    static HASHNODE *fname_table[FNAME_HASHSIZE];
    HASHNODE *hp, **hash_bucket;
    U_CHAR *fname, *p;
    int fname_length;

    fname = ++bp;

    /* Turn the file name, which is a character string literal,
       into a null-terminated string.  Do this in place.  */
    p = bp;
    for (;;)
      switch ((*p++ = *bp++)) {
      case '\0':
	error ("invalid format `#line' command");
	return 0;

      case '\\':
	{
	  char *bpc = (char *) bp;
	  int c = parse_escape (&bpc);
	  bp = (U_CHAR *) bpc;
	  if (c < 0)
	    p--;
	  else
	    p[-1] = c;
	}
	break;

      case '\"':
	p[-1] = 0;
	goto fname_done;
      }
  fname_done:
    fname_length = p - fname;

    SKIP_WHITE_SPACE (bp);
    if (*bp) {
      if (pedantic)
	pedwarn ("garbage at end of `#line' command");
      if (*bp == '1')
	file_change = enter_file;
      else if (*bp == '2')
	file_change = leave_file;
      else if (*bp == '3')
	ip->system_header_p = 1;
      else if (*bp == '4')
	ip->system_header_p = 2;
      else {
	error ("invalid format `#line' command");
	return 0;
      }

      bp++;
      SKIP_WHITE_SPACE (bp);
      if (*bp == '3') {
	ip->system_header_p = 1;
	bp++;
	SKIP_WHITE_SPACE (bp);
      }
      if (*bp == '4') {
	ip->system_header_p = 2;
	bp++;
	SKIP_WHITE_SPACE (bp);
      }
      if (*bp) {
	error ("invalid format `#line' command");
	return 0;
      }
    }

    hash_bucket =
      &fname_table[hashf (fname, fname_length, FNAME_HASHSIZE)];
    for (hp = *hash_bucket; hp != NULL; hp = hp->next)
      if (hp->length == fname_length &&
	  strncmp (hp->value.cpval, fname, fname_length) == 0) {
	ip->nominal_fname = hp->value.cpval;
	break;
      }
    if (hp == 0) {
      /* Didn't find it; cons up a new one.  */
      hp = (HASHNODE *) xcalloc (1, sizeof (HASHNODE) + fname_length + 1);
      hp->next = *hash_bucket;
      *hash_bucket = hp;

      hp->length = fname_length;
      ip->nominal_fname = hp->value.cpval = ((char *) hp) + sizeof (HASHNODE);
      bcopy (fname, hp->value.cpval, fname_length);
    }
  } else if (*bp) {
    error ("invalid format `#line' command");
    return 0;
  }

  ip->lineno = new_lineno;
  output_line_command (ip, op, 0, file_change);
  check_expand (op, ip->length - (ip->bufp - ip->buf));
  return 0;
}

/*
 * remove the definition of a symbol from the symbol table.
 * according to un*x /lib/cpp, it is not an error to undef
 * something that has no definitions, so it isn't one here either.
 */

static int
do_undef (buf, limit, op, keyword)
     U_CHAR *buf, *limit;
     FILE_BUF *op;
     struct directive *keyword;
{
  int sym_length;
  HASHNODE *hp;
  U_CHAR *orig_buf = buf;

  /* If this is a precompiler run (with -pcp) pass thru #undef commands.  */
  if (pcp_outfile && op)
    pass_thru_directive (buf, limit, op, keyword);

  SKIP_WHITE_SPACE (buf);
  sym_length = check_macro_name (buf, "macro");

  while ((hp = lookup (buf, sym_length, -1)) != NULL) {
    /* If we are generating additional info for debugging (with -g) we
       need to pass through all effective #undef commands.  */
    if (debug_output && op)
      pass_thru_directive (orig_buf, limit, op, keyword);
    if (hp->type != T_MACRO)
      warning ("undefining `%s'", hp->name);
#ifdef NEXT_CPP_SERVER
    if (segregate_output && hp->owner && !hp->owner->macro_list) 
      hp->owner->macro_count--;
#endif
    delete_macro (hp);
  }

  if (pedantic) {
    buf += sym_length;
    SKIP_WHITE_SPACE (buf);
    if (buf != limit)
      pedwarn ("garbage after `#undef' directive");
  }
  return 0;
}

/*
 * Report an error detected by the program we are processing.
 * Use the text of the line in the error message.
 * (We use error because it prints the filename & line#.)
 */

static int
do_error (buf, limit, op, keyword)
     U_CHAR *buf, *limit;
     FILE_BUF *op;
     struct directive *keyword;
{
  int length = limit - buf;
  U_CHAR *copy = (U_CHAR *) xmalloc (length + 1);
  bcopy ((char *) buf, (char *) copy, length);
  copy[length] = 0;
  SKIP_WHITE_SPACE (copy);
  error ("#error %s", copy);
  return 0;
}

/*
 * Report a warning detected by the program we are processing.
 * Use the text of the line in the warning message, then continue.
 * (We use error because it prints the filename & line#.)
 */

static int
do_warning (buf, limit, op, keyword)
     U_CHAR *buf, *limit;
     FILE_BUF *op;
     struct directive *keyword;
{
  int length = limit - buf;
  U_CHAR *copy = (U_CHAR *) xmalloc (length + 1);
  bcopy ((char *) buf, (char *) copy, length);
  copy[length] = 0;
  SKIP_WHITE_SPACE (copy);
  warning ("#warning %s", copy);
  return 0;
}

/* Remember the name of the current file being read from so that we can
   avoid ever including it again.  */

static int
do_once ()
{
  int i;
  FILE_BUF *ip = NULL;

  for (i = indepth; i >= 0; i--)
    if (instack[i].fname != NULL) {
      ip = &instack[i];
      break;
    }

  if (ip != NULL) {
    struct file_name_list *new;
    
    new = (struct file_name_list *) xmalloc (sizeof (struct file_name_list));
    new->next = dont_repeat_files;
    dont_repeat_files = new;
    new->fname = savestring (ip->fname);
    new->control_macro = 0;
    new->got_name_map = 0;
    new->c_system_include_path = 0;
  }
  return 0;
}

/* #ident has already been copied to the output file, so just ignore it.  */

static int
do_ident (buf, limit)
     U_CHAR *buf, *limit;
{
  FILE_BUF trybuf;
  int len;
  FILE_BUF *op = &outbuf;

  /* Allow #ident in system headers, since that's not user's fault.  */
  if (pedantic && !instack[indepth].system_header_p)
    pedwarn ("ANSI C does not allow `#ident'");

  trybuf = expand_to_temp_buffer (buf, limit, 0, 0);
  buf = (U_CHAR *) alloca (trybuf.bufp - trybuf.buf + 1);
  bcopy ((char *) trybuf.buf, (char *) buf, trybuf.bufp - trybuf.buf);
  limit = buf + (trybuf.bufp - trybuf.buf);
  len = (limit - buf);
  free (trybuf.buf);

  /* Output directive name.  */
  check_expand (op, 7);
  bcopy ("#ident ", (char *) op->bufp, 7);
  op->bufp += 7;

  /* Output the expanded argument line.  */
  check_expand (op, len);
  bcopy ((char *) buf, (char *) op->bufp, len);
  op->bufp += len;

  return 0;
}

/* #pragma and its argument line have already been copied to the output file.
   Just check for some recognized pragmas that need validation here.  */

static int
do_pragma (buf, limit)
     U_CHAR *buf, *limit;
{
  while (*buf == ' ' || *buf == '\t')
    buf++;
  if (!strncmp (buf, "once", 4)) {
#if !defined (NeXT) && !defined (__APPLE__) || defined (OPENSTEP)
    /* Allow #pragma once in system headers, since that's not the user's
       fault.  */
    if (!instack[indepth].system_header_p)
      warning ("`#pragma once' is obsolete");
#endif
    do_once ();
  }

#if defined (NEXT_SEMANTICS) || defined (NEXT_PDO)
  if (!strncmp (buf, "cplusplus", 9))
    {
      if (instack[indepth].system_header_p == 2 && cplusplus)
	{
	  instack[indepth].system_header_p = 1;
	  output_line_command (&instack[indepth], &outbuf, 0, same_file);
	}
    }
#endif

  if (!strncmp (buf, "implementation", 14)) {
    /* Be quiet about `#pragma implementation' for a file only if it hasn't
       been included yet.  */
    struct file_name_list *ptr;
    U_CHAR *p = buf + 14, *fname, *inc_fname;
    SKIP_WHITE_SPACE (p);
    if (*p == '\n' || *p != '\"')
      return 0;

    fname = p + 1;
    if (p = (U_CHAR *) index (fname, '\"'))
      *p = '\0';
    
    for (ptr = all_include_files; ptr; ptr = ptr->next) {
      inc_fname = (U_CHAR *) rindex (ptr->fname, '/');
      inc_fname = inc_fname ? inc_fname + 1 : (U_CHAR *) ptr->fname;
      if (inc_fname && !strcmp (inc_fname, fname))
	warning ("`#pragma implementation' for `%s' appears after file is included",
		 fname);
    }
  }

  return 0;
}

#if 0
/* This was a fun hack, but #pragma seems to start to be useful.
   By failing to recognize it, we pass it through unchanged to cc1.  */

/*
 * the behavior of the #pragma directive is implementation defined.
 * this implementation defines it as follows.
 */

static int
do_pragma ()
{
  close (0);
  if (open ("/dev/tty", O_RDONLY, 0666) != 0)
    goto nope;
  close (1);
  if (open ("/dev/tty", O_WRONLY, 0666) != 1)
    goto nope;
  execl ("/usr/games/hack", "#pragma", 0);
  execl ("/usr/games/rogue", "#pragma", 0);
  execl ("/usr/new/emacs", "-f", "hanoi", "9", "-kill", 0);
  execl ("/usr/local/emacs", "-f", "hanoi", "9", "-kill", 0);
nope:
  fatal ("You are in a maze of twisty compiler features, all different");
}
#endif

/* Just ignore #sccs, on systems where we define it at all.  */

static int
do_sccs ()
{
  if (pedantic)
    pedwarn ("ANSI C does not allow `#sccs'");
  return 0;
}

/*
 * handle #if command by
 *   1) inserting special `defined' keyword into the hash table
 *	that gets turned into 0 or 1 by special_symbol (thus,
 *	if the luser has a symbol called `defined' already, it won't
 *      work inside the #if command)
 *   2) rescan the input into a temporary output buffer
 *   3) pass the output buffer to the yacc parser and collect a value
 *   4) clean up the mess left from steps 1 and 2.
 *   5) call conditional_skip to skip til the next #endif (etc.),
 *      or not, depending on the value from step 3.
 */

static int
do_if (buf, limit, op, keyword)
     U_CHAR *buf, *limit;
     FILE_BUF *op;
     struct directive *keyword;
{
  HOST_WIDE_INT value;
  FILE_BUF *ip = &instack[indepth];

  value = eval_if_expression (buf, limit - buf);
  conditional_skip (ip, value == 0, T_IF, NULL_PTR, op);
  return 0;
}

/*
 * handle a #elif directive by not changing  if_stack  either.
 * see the comment above do_else.
 */

static int
do_elif (buf, limit, op, keyword)
     U_CHAR *buf, *limit;
     FILE_BUF *op;
     struct directive *keyword;
{
  HOST_WIDE_INT value;
  FILE_BUF *ip = &instack[indepth];

  if (if_stack == instack[indepth].if_stack) {
    error ("`#elif' not within a conditional");
    return 0;
  } else {
    if (if_stack->type != T_IF && if_stack->type != T_ELIF) {
      error ("`#elif' after `#else'");
#ifdef REPORT_EVENT
      REPORT_EVENT (0, NULL, if_stack->fname, if_stack->lineno,
		    "matching conditional", 0, 0, 0);
#endif
      fprintf (stderr, " (matches line %d", if_stack->lineno);
      if (if_stack->fname != NULL && ip->fname != NULL &&
	  strcmp (if_stack->fname, ip->nominal_fname) != 0)
	fprintf (stderr, ", file %s", if_stack->fname);
      fprintf (stderr, ")\n");
    }
    if_stack->type = T_ELIF;
  }

  if (if_stack->if_succeeded)
    skip_if_group (ip, 0, op);
  else {
    value = eval_if_expression (buf, limit - buf);
    if (value == 0)
      skip_if_group (ip, 0, op);
    else {
      ++if_stack->if_succeeded;	/* continue processing input */
      output_line_command (ip, op, 1, same_file);
    }
  }
  return 0;
}

/*
 * evaluate a #if expression in BUF, of length LENGTH,
 * then parse the result as a C expression and return the value as an int.
 */
static HOST_WIDE_INT
eval_if_expression (buf, length)
     U_CHAR *buf;
     int length;
{
  FILE_BUF temp_obuf;
  HASHNODE *save_defined;
  HOST_WIDE_INT value;

  save_defined = install ("defined", -1, T_SPEC_DEFINED, 0, NULL_PTR, -1);
  pcp_inside_if = 1;
  temp_obuf = expand_to_temp_buffer (buf, buf + length, 0, 1);
  pcp_inside_if = 0;
  delete_macro (save_defined);	/* clean up special symbol */

  value = parse_c_expression (temp_obuf.buf);

  free (temp_obuf.buf);

  return value;
}

/*
 * routine to handle ifdef/ifndef.  Try to look up the symbol,
 * then do or don't skip to the #endif/#else/#elif depending
 * on what directive is actually being processed.
 */

static int
do_xifdef (buf, limit, op, keyword)
     U_CHAR *buf, *limit;
     FILE_BUF *op;
     struct directive *keyword;
{
  int skip;
  FILE_BUF *ip = &instack[indepth];
  U_CHAR *end; 
  int start_of_file = 0;
  U_CHAR *control_macro = 0;

  /* Detect a #ifndef at start of file (not counting comments).  */
  if (ip->fname != 0 && keyword->type == T_IFNDEF) {
    U_CHAR *p = ip->buf;
    while (p != directive_start) {
      U_CHAR c = *p++;
      if (is_space[c])
	;
      else if (c == '/' && p != ip->bufp && *p == '*') {
	/* Skip this comment.  */
	int junk = 0;
	U_CHAR *save_bufp = ip->bufp;
	ip->bufp = p + 1;
#ifdef NEXT_CPP_SERVER
	p = skip_to_end_of_comment (ip, op, &junk, 1);
#else
	p = skip_to_end_of_comment (ip, &junk, 1);
#endif
	ip->bufp = save_bufp;
      } else {
	goto fail;
      }
    }
    /* If we get here, this conditional is the beginning of the file.  */
    start_of_file = 1;
  fail: ;
  }

  /* Discard leading and trailing whitespace.  */
  SKIP_WHITE_SPACE (buf);
  while (limit != buf && is_hor_space[limit[-1]]) limit--;

  /* Find the end of the identifier at the beginning.  */
  for (end = buf; is_idchar[*end]; end++);

  if (end == buf) {
    skip = (keyword->type == T_IFDEF);
    if (! traditional)
      pedwarn (end == limit ? "`#%s' with no argument"
	       : "`#%s' argument starts with punctuation",
	       keyword->name);
  } else {
    HASHNODE *hp;

    if (pedantic && buf[0] >= '0' && buf[0] <= '9')
      pedwarn ("`#%s' argument starts with a digit", keyword->name);
    else if (end != limit && !traditional)
      pedwarn ("garbage at end of `#%s' argument", keyword->name);

    hp = lookup (buf, end-buf, -1);

    if (pcp_outfile) {
      /* Output a precondition for this macro.  */
      if (hp &&
	  (hp->type == T_CONST
	   || (hp->type == T_MACRO && hp->value.defn->predefined)))
	fprintf (pcp_outfile, "#define %s\n", hp->name);
      else {
	U_CHAR *cp = buf;
	fprintf (pcp_outfile, "#undef ");
	while (is_idchar[*cp]) /* Ick! */
	  fputc (*cp++, pcp_outfile);
	putc ('\n', pcp_outfile);
      }
    }

    skip = (hp == NULL) ^ (keyword->type == T_IFNDEF);
    if (start_of_file && !skip) {
      control_macro = (U_CHAR *) xmalloc (end - buf + 1);
      bcopy ((char *) buf, (char *) control_macro, end - buf);
      control_macro[end - buf] = 0;
    }
  }
  
  conditional_skip (ip, skip, T_IF, control_macro, op);
  return 0;
}

/* Push TYPE on stack; then, if SKIP is nonzero, skip ahead.
   If this is a #ifndef starting at the beginning of a file,
   CONTROL_MACRO is the macro name tested by the #ifndef.
   Otherwise, CONTROL_MACRO is 0.  */

static void
conditional_skip (ip, skip, type, control_macro, op)
     FILE_BUF *ip;
     int skip;
     enum node_type type;
     U_CHAR *control_macro;
     FILE_BUF *op;
{
  IF_STACK_FRAME *temp;

  temp = (IF_STACK_FRAME *) xcalloc (1, sizeof (IF_STACK_FRAME));
  temp->fname = ip->nominal_fname;
  temp->lineno = ip->lineno;
  temp->next = if_stack;
  temp->control_macro = control_macro;
  if_stack = temp;

  if_stack->type = type;

  if (skip != 0) {
    skip_if_group (ip, 0, op);
    return;
  } else {
    ++if_stack->if_succeeded;
#ifdef NEXT_CPP_SERVER
    output_line_command (ip, op, 1, same_file);
#else
    output_line_command (ip, &outbuf, 1, same_file);
#endif
  }
}

/*
 * skip to #endif, #else, or #elif.  adjust line numbers, etc.
 * leaves input ptr at the sharp sign found.
 * If ANY is nonzero, return at next directive of any sort.
 */
static void
skip_if_group (ip, any, op)
     FILE_BUF *ip;
     int any;
     FILE_BUF *op;
{
  register U_CHAR *bp = ip->bufp, *cp;
  register U_CHAR *endb = ip->buf + ip->length;
  struct directive *kt;
  IF_STACK_FRAME *save_if_stack = if_stack; /* don't pop past here */
  U_CHAR *beg_of_line = bp;
  register int ident_length;
  U_CHAR *ident, *after_ident;
  /* Save info about where the group starts.  */
  U_CHAR *beg_of_group = bp;
  int beg_lineno = ip->lineno;

#ifdef NEXT_CPP_SERVER
  if (any && segregate_output && aggressive_skip_if_group(ip))
    return;
#endif

  if (output_conditionals && op != 0) {
    char *ptr = "#failed\n";
    int len = strlen (ptr);

    if (op->bufp > op->buf && op->bufp[-1] != '\n')
      {
	*op->bufp++ = '\n';
	op->lineno++;
      }
    check_expand (op, len);
    bcopy (ptr, (char *) op->bufp, len);
    op->bufp += len;
    op->lineno++;
    output_line_command (ip, op, 1, 0);
  }

  while (bp < endb) {
    switch (*bp++) {
    case '/':			/* possible comment */
      if (*bp == '\\' && bp[1] == '\n')
	newline_fix (bp);
      if (*bp == '*'
	  || (cplusplus_comments && *bp == '/')) {
	ip->bufp = ++bp;
#ifdef NEXT_CPP_SERVER
	bp = skip_to_end_of_comment (ip, op, &ip->lineno, 0);
#else
	bp = skip_to_end_of_comment (ip, &ip->lineno, 0);
#endif
      }
      break;
    case '\"':
    case '\'':
      bp = skip_quoted_string (bp - 1, endb, ip->lineno, &ip->lineno,
			       NULL_PTR, NULL_PTR);
      break;
    case '\\':
      /* Char after backslash loses its special meaning.  */
      if (bp < endb) {
	if (*bp == '\n')
	  ++ip->lineno;		/* But do update the line-count.  */
	bp++;
      }
      break;
    case '\n':
      ++ip->lineno;
      beg_of_line = bp;
      break;
    case '#':
      ip->bufp = bp - 1;

      /* # keyword: a # must be first nonblank char on the line */
      if (beg_of_line == 0)
	break;
      /* Scan from start of line, skipping whitespace, comments
	 and backslash-newlines, and see if we reach this #.
	 If not, this # is not special.  */
      bp = beg_of_line;
      /* If -traditional, require # to be at beginning of line.  */
      if (!traditional)
	while (1) {
	  if (is_hor_space[*bp])
	    bp++;
	  else if (*bp == '\\' && bp[1] == '\n')
	    bp += 2;
	  else if (*bp == '/' && bp[1] == '*') {
	    bp += 2;
	    while (!(*bp == '*' && bp[1] == '/'))
	      bp++;
	    bp += 2;
	  }
	  /* There is no point in trying to deal with C++ // comments here,
	     because if there is one, then this # must be part of the
	     comment and we would never reach here.  */
	  else break;
	}
      if (bp != ip->bufp) {
	bp = ip->bufp + 1;	/* Reset bp to after the #.  */
	break;
      }

      bp = ip->bufp + 1;	/* Point after the '#' */

      /* Skip whitespace and \-newline.  */
      while (1) {
	if (is_hor_space[*bp])
	  bp++;
	else if (*bp == '\\' && bp[1] == '\n')
	  bp += 2;
	else if (*bp == '/' && bp[1] == '*') {
	  bp += 2;
	  while (!(*bp == '*' && bp[1] == '/')) {
	    if (*bp == '\n')
	      ip->lineno++;
	    bp++;
	  }
	  bp += 2;
	} else if (cplusplus_comments && *bp == '/' && bp[1] == '/') {
	  bp += 2;
	  while (bp[-1] == '\\' || *bp != '\n') {
	    if (*bp == '\n')
	      ip->lineno++;
	    bp++;
	  }
        }
	else break;
      }

      cp = bp;

      /* Now find end of directive name.
	 If we encounter a backslash-newline, exchange it with any following
	 symbol-constituents so that we end up with a contiguous name.  */

      while (1) {
	if (is_idchar[*bp])
	  bp++;
	else {
	  if (*bp == '\\' && bp[1] == '\n')
	    name_newline_fix (bp);
	  if (is_idchar[*bp])
	    bp++;
	  else break;
	}
      }
      ident_length = bp - cp;
      ident = cp;
      after_ident = bp;

      /* A line of just `#' becomes blank.  */

      if (ident_length == 0 && *after_ident == '\n') {
	continue;
      }

      if (ident_length == 0 || !is_idstart[*ident]) {
	U_CHAR *p = ident;
	while (is_idchar[*p]) {
	  if (*p < '0' || *p > '9')
	    break;
	  p++;
	}
	/* Handle # followed by a line number.  */
	if (p != ident && !is_idchar[*p]) {
	  if (pedantic)
	    pedwarn ("`#' followed by integer");
	  continue;
	}

	/* Avoid error for `###' and similar cases unless -pedantic.  */
	if (p == ident) {
	  while (*p == '#' || is_hor_space[*p]) p++;
	  if (*p == '\n') {
	    if (pedantic && !lang_asm)
	      pedwarn ("invalid preprocessor directive");
	    continue;
	  }
	}

	if (!lang_asm && pedantic)
	  pedwarn ("invalid preprocessor directive name");
	continue;
      }

      for (kt = directive_table; kt->length >= 0; kt++) {
	IF_STACK_FRAME *temp;
	if (ident_length == kt->length
	    && strncmp (cp, kt->name, kt->length) == 0) {
	  /* If we are asked to return on next directive, do so now.  */
	  if (any)
	    goto done;

	  switch (kt->type) {
	  case T_IF:
	  case T_IFDEF:
	  case T_IFNDEF:
	    temp = (IF_STACK_FRAME *) xcalloc (1, sizeof (IF_STACK_FRAME));
	    temp->next = if_stack;
	    if_stack = temp;
	    temp->lineno = ip->lineno;
	    temp->fname = ip->nominal_fname;
	    temp->type = kt->type;
	    break;
	  case T_ELSE:
	  case T_ENDIF:
	    if (pedantic && if_stack != save_if_stack)
	      validate_else (bp);
	  case T_ELIF:
	    if (if_stack == instack[indepth].if_stack) {
	      error ("`#%s' not within a conditional", kt->name);
	      break;
	    }
	    else if (if_stack == save_if_stack)
	      goto done;		/* found what we came for */

	    if (kt->type != T_ENDIF) {
	      if (if_stack->type == T_ELSE)
		error ("`#else' or `#elif' after `#else'");
	      if_stack->type = kt->type;
	      break;
	    }

	    temp = if_stack;
	    if_stack = if_stack->next;
	    free (temp);
	    break;
	  }
	  break;
	}
      }
      /* Don't let erroneous code go by.  */
      if (kt->length < 0 && !lang_asm && pedantic)
	pedwarn ("invalid preprocessor directive name");
    }
  }

  ip->bufp = bp;
  /* after this returns, rescan will exit because ip->bufp
     now points to the end of the buffer.
     rescan is responsible for the error message also.  */

 done:
  if (output_conditionals && op != 0) {
    char *ptr = "#endfailed\n";
    int len = strlen (ptr);

    if (op->bufp > op->buf && op->bufp[-1] != '\n')
      {
	*op->bufp++ = '\n';
	op->lineno++;
      }
    check_expand (op, beg_of_line - beg_of_group);
    bcopy ((char *) beg_of_group, (char *) op->bufp,
	   beg_of_line - beg_of_group);
    op->bufp += beg_of_line - beg_of_group;
    op->lineno += ip->lineno - beg_lineno;
    check_expand (op, len);
    bcopy (ptr, (char *) op->bufp, len);
    op->bufp += len;
    op->lineno++;
  }
}

/*
 * handle a #else directive.  Do this by just continuing processing
 * without changing  if_stack ;  this is so that the error message
 * for missing #endif's etc. will point to the original #if.  It
 * is possible that something different would be better.
 */

static int
do_else (buf, limit, op, keyword)
     U_CHAR *buf, *limit;
     FILE_BUF *op;
     struct directive *keyword;
{
  FILE_BUF *ip = &instack[indepth];

  if (pedantic) {
    SKIP_WHITE_SPACE (buf);
    if (buf != limit)
      pedwarn ("text following `#else' violates ANSI standard");
  }

  if (if_stack == instack[indepth].if_stack) {
    error ("`#else' not within a conditional");
    return 0;
  } else {
    /* #ifndef can't have its special treatment for containing the whole file
       if it has a #else clause.  */
    if_stack->control_macro = 0;

    if (if_stack->type != T_IF && if_stack->type != T_ELIF) {
      error ("`#else' after `#else'");
#ifdef REPORT_EVENT
      REPORT_EVENT (0, NULL, if_stack->fname, if_stack->lineno,
		    "matching conditional", 0, 0, 0);
#endif
      fprintf (stderr, " (matches line %d", if_stack->lineno);
      if (strcmp (if_stack->fname, ip->nominal_fname) != 0)
	fprintf (stderr, ", file %s", if_stack->fname);
      fprintf (stderr, ")\n");
    }
    if_stack->type = T_ELSE;
  }

  if (if_stack->if_succeeded)
    skip_if_group (ip, 0, op);
  else {
    ++if_stack->if_succeeded;	/* continue processing input */
    output_line_command (ip, op, 1, same_file);
  }
  return 0;
}

/*
 * unstack after #endif command
 */

static int
do_endif (buf, limit, op, keyword)
     U_CHAR *buf, *limit;
     FILE_BUF *op;
     struct directive *keyword;
{
  if (pedantic) {
    SKIP_WHITE_SPACE (buf);
    if (buf != limit)
      pedwarn ("text following `#endif' violates ANSI standard");
  }

  if (if_stack == instack[indepth].if_stack)
    error ("unbalanced `#endif'");
  else {
    IF_STACK_FRAME *temp = if_stack;
    if_stack = if_stack->next;
    if (temp->control_macro != 0) {
      /* This #endif matched a #ifndef at the start of the file.
	 See if it is at the end of the file.  */
      FILE_BUF *ip = &instack[indepth];
      U_CHAR *p = ip->bufp;
      U_CHAR *ep = ip->buf + ip->length;

      while (p != ep) {
	U_CHAR c = *p++;
	switch (c) {
	case ' ':
	case '\t':
	case '\n':
	  break;
	case '/':
	  if (p != ep && *p == '*') {
	    /* Skip this comment.  */
	    int junk = 0;
	    U_CHAR *save_bufp = ip->bufp;
	    ip->bufp = p + 1;
#ifdef NEXT_CPP_SERVER
	    p = skip_to_end_of_comment (ip, op, &junk, 1);
#else
	    p = skip_to_end_of_comment (ip, &junk, 1);
#endif
	    ip->bufp = save_bufp;
	  }
	  break;
	default:
	  goto fail;
	}
      }
      /* If we get here, this #endif ends a #ifndef
	 that contains all of the file (aside from whitespace).
	 Arrange not to include the file again
	 if the macro that was tested is defined.

	 Do not do this for the top-level file in a -include or any
	 file in a -imacros.  */
      if (indepth != 0
	  && ! (indepth == 1 && no_record_file)
	  && ! (no_record_file && no_output))
	record_control_macro (ip->fname, temp->control_macro);
    fail: ;
    }
    free (temp);
    output_line_command (&instack[indepth], op, 1, same_file);
  }
  return 0;
}

/* When an #else or #endif is found while skipping failed conditional,
   if -pedantic was specified, this is called to warn about text after
   the command name.  P points to the first char after the command name.  */

static void
validate_else (p)
     register U_CHAR *p;
{
  /* Advance P over whitespace and comments.  */
  while (1) {
    if (*p == '\\' && p[1] == '\n')
      p += 2;
    if (is_hor_space[*p])
      p++;
    else if (*p == '/') {
      if (p[1] == '\\' && p[2] == '\n')
	newline_fix (p + 1);
      if (p[1] == '*') {
	p += 2;
	/* Don't bother warning about unterminated comments
	   since that will happen later.  Just be sure to exit.  */
	while (*p) {
	  if (p[1] == '\\' && p[2] == '\n')
	    newline_fix (p + 1);
	  if (*p == '*' && p[1] == '/') {
	    p += 2;
	    break;
	  }
	  p++;
	}
      }
      else if (cplusplus_comments && p[1] == '/') {
	p += 2;
	while (*p && (*p != '\n' || p[-1] == '\\'))
	  p++;
      }
    } else break;
  }
  if (*p && *p != '\n')
    pedwarn ("text following `#else' or `#endif' violates ANSI standard");
}

/* Skip a comment, assuming the input ptr immediately follows the
   initial slash-star.  Bump *LINE_COUNTER for each newline.
   (The canonical line counter is &ip->lineno.)
   Don't use this routine (or the next one) if bumping the line
   counter is not sufficient to deal with newlines in the string.

   If NOWARN is nonzero, don't warn about slash-star inside a comment.
   This feature is useful when processing a comment that is going to be
   processed or was processed at another point in the preprocessor,
   to avoid a duplicate warning.  Likewise for unterminated comment errors.  */

static U_CHAR *
#ifndef NEXT_CPP_SERVER
skip_to_end_of_comment (ip, line_counter, nowarn)
#else
skip_to_end_of_comment (ip, op, line_counter, nowarn)
     register FILE_BUF *op;
#endif
     register FILE_BUF *ip;
     int *line_counter;		/* place to remember newlines, or NULL */
     int nowarn;
{
  register U_CHAR *limit = ip->buf + ip->length;
  register U_CHAR *bp = ip->bufp;
#ifndef NEXT_CPP_SERVER
  FILE_BUF *op = &outbuf;	/* JF */
#endif
  int output = put_out_comments && !line_counter;
  int start_line = line_counter ? *line_counter : 0;

	/* JF this line_counter stuff is a crock to make sure the
	   comment is only put out once, no matter how many times
	   the comment is skipped.  It almost works */
  if (output) {
    *op->bufp++ = '/';
    *op->bufp++ = '*';
  }
  if (cplusplus_comments && bp[-1] == '/') {
    if (output) {
      while (bp < limit) {
	*op->bufp++ = *bp;
	if (*bp == '\n' && bp[-1] != '\\')
	  break;
	if (*bp == '\n') {
	  ++*line_counter;
	  ++op->lineno;
	}
	bp++;
      }
      op->bufp[-1] = '*';
      *op->bufp++ = '/';
      *op->bufp++ = '\n';
    } else {
      while (bp < limit) {
	if (bp[-1] != '\\' && *bp == '\n') {
	  break;
	} else {
	  if (*bp == '\n' && line_counter)
	    ++*line_counter;
	  bp++;
	}
      }
    }
    ip->bufp = bp;
    return bp;
  }
  while (bp < limit) {
    if (output)
      *op->bufp++ = *bp;
    switch (*bp++) {
    case '/':
      if (warn_comments && !nowarn && bp < limit && *bp == '*')
	warning ("`/*' within comment");
      break;
    case '\n':
      /* If this is the end of the file, we have an unterminated comment.
	 Don't swallow the newline.  We are guaranteed that there will be a
	 trailing newline and various pieces assume it's there.  */
      if (bp == limit)
	{
	  --bp;
	  --limit;
	  break;
	}
      if (line_counter != NULL)
	++*line_counter;
      if (output)
	++op->lineno;
      break;
    case '*':
      if (*bp == '\\' && bp[1] == '\n')
	newline_fix (bp);
      if (*bp == '/') {
        if (output)
	  *op->bufp++ = '/';
	ip->bufp = ++bp;
	return bp;
      }
      break;
    }
  }

  if (!nowarn)
    error_with_line (line_for_error (start_line), "unterminated comment");
  ip->bufp = bp;
  return bp;
}

/*
 * Skip over a quoted string.  BP points to the opening quote.
 * Returns a pointer after the closing quote.  Don't go past LIMIT.
 * START_LINE is the line number of the starting point (but it need
 * not be valid if the starting point is inside a macro expansion).
 *
 * The input stack state is not changed.
 *
 * If COUNT_NEWLINES is nonzero, it points to an int to increment
 * for each newline passed.
 *
 * If BACKSLASH_NEWLINES_P is nonzero, store 1 thru it
 * if we pass a backslash-newline.
 *
 * If EOFP is nonzero, set *EOFP to 1 if the string is unterminated.
 */
static U_CHAR *
skip_quoted_string (bp, limit, start_line, count_newlines, backslash_newlines_p, eofp)
     register U_CHAR *bp;
     register U_CHAR *limit;
     int start_line;
     int *count_newlines;
     int *backslash_newlines_p;
     int *eofp;
{
  register U_CHAR c, match;

  match = *bp++;
  while (1) {
    if (bp >= limit) {
      error_with_line (line_for_error (start_line),
		       "unterminated string or character constant");
      error_with_line (multiline_string_line,
		       "possible real start of unterminated constant");
      multiline_string_line = 0;
      if (eofp)
	*eofp = 1;
      break;
    }
    c = *bp++;
    if (c == '\\') {
      while (*bp == '\\' && bp[1] == '\n') {
	if (backslash_newlines_p)
	  *backslash_newlines_p = 1;
	if (count_newlines)
	  ++*count_newlines;
	bp += 2;
      }
      if (*bp == '\n' && count_newlines) {
	if (backslash_newlines_p)
	  *backslash_newlines_p = 1;
	++*count_newlines;
      }
      bp++;
    } else if (c == '\n') {
      if (traditional) {
 	/* Unterminated strings and character constants are 'legal'.  */
 	bp--;	/* Don't consume the newline. */
 	if (eofp)
 	  *eofp = 1;
 	break;
      }
      if (pedantic || match == '\'') {
	error_with_line (line_for_error (start_line),
			 "unterminated string or character constant");
	bp--;
	if (eofp)
	  *eofp = 1;
	break;
      }
      /* If not traditional, then allow newlines inside strings.  */
      if (count_newlines)
	++*count_newlines;
      if (multiline_string_line == 0)
	multiline_string_line = start_line;
    } else if (c == match)
      break;
  }
  return bp;
}

/* Place into DST a quoted string representing the string SRC.
   Return the address of DST's terminating null.  */
static char *
quote_string (dst, src)
     char *dst, *src;
{
  U_CHAR c;

  *dst++ = '\"';
  for (;;)
    switch ((c = *src++))
      {
      default:
        if (isprint (c))
	  *dst++ = c;
	else
	  {
	    sprintf (dst, "\\%03o", c);
	    dst += 4;
	  }
	break;

      case '\"':
      case '\\':
	*dst++ = '\\';
	*dst++ = c;
	break;
      
      case '\0':
	*dst++ = '\"';
	*dst = '\0';
	return dst;
      }
}

/* Skip across a group of balanced parens, starting from IP->bufp.
   IP->bufp is updated.  Use this with IP->bufp pointing at an open-paren.

   This does not handle newlines, because it's used for the arg of #if,
   where there aren't any newlines.  Also, backslash-newline can't appear.  */

static U_CHAR *
#ifndef NEXT_CPP_SERVER
skip_paren_group (ip)
#else
skip_paren_group (ip, op)
     register FILE_BUF *op;
#endif
     register FILE_BUF *ip;
{
  U_CHAR *limit = ip->buf + ip->length;
  U_CHAR *p = ip->bufp;
  int depth = 0;
  int lines_dummy = 0;

  while (p != limit) {
    int c = *p++;
    switch (c) {
    case '(':
      depth++;
      break;

    case ')':
      depth--;
      if (depth == 0)
	return ip->bufp = p;
      break;

    case '/':
      if (*p == '*') {
	ip->bufp = p;
#ifdef NEXT_CPP_SERVER
	p = skip_to_end_of_comment (ip, op, &lines_dummy, 0);
#else
	p = skip_to_end_of_comment (ip, &lines_dummy, 0);
#endif
	p = ip->bufp;
      }

    case '"':
    case '\'':
      {
	int eofp = 0;
	p = skip_quoted_string (p - 1, limit, 0, NULL_PTR, NULL_PTR, &eofp);
	if (eofp)
	  return ip->bufp = p;
      }
      break;
    }
  }

  ip->bufp = p;
  return p;
}

/*
 * write out a #line command, for instance, after an #include file.
 * If CONDITIONAL is nonzero, we can omit the #line if it would
 * appear to be a no-op, and we can output a few newlines instead
 * if we want to increase the line number by a small amount.
 * FILE_CHANGE says whether we are entering a file, leaving, or neither.
 */

static void
output_line_command (ip, op, conditional, file_change)
     FILE_BUF *ip, *op;
     int conditional;
     enum file_change_code file_change;
{
  int len;
  char *line_cmd_buf, *line_end;

  if (no_line_commands
      || ip->fname == NULL
      || no_output
#ifdef NEXT_CPP_SERVER
      /* allow all "notes" to main outbuf to pass through */
      || (ip->file && ip->file->header_output 
          && ((op == &sys_header_outbuf) || (op == &user_header_outbuf)))
#endif
	) {
    op->lineno = ip->lineno;
    return;
  }

  if (conditional) {
    if (ip->lineno == op->lineno)
      return;

    /* If the inherited line number is a little too small,
       output some newlines instead of a #line command.  */
    if (ip->lineno > op->lineno && ip->lineno < op->lineno + 8) {
      check_expand (op, 10);
      while (ip->lineno > op->lineno) {
	*op->bufp++ = '\n';
	op->lineno++;
      }
      return;
    }
  }

  /* Don't output a line number of 0 if we can help it.  */
  if (ip->lineno == 0 && ip->bufp - ip->buf < ip->length
      && *ip->bufp == '\n') {
    ip->lineno++;
    ip->bufp++;
  }

  line_cmd_buf = (char *) alloca (4 * strlen (ip->nominal_fname) + 100);
#ifdef OUTPUT_LINE_COMMANDS
  sprintf (line_cmd_buf, "#line %d ", ip->lineno);
#else
  sprintf (line_cmd_buf, "# %d ", ip->lineno);
#endif
  line_end = quote_string (line_cmd_buf + strlen (line_cmd_buf),
			   ip->nominal_fname);
  if (file_change != same_file) {
    *line_end++ = ' ';
#if defined (NEXT_SEMANTICS) || defined (NEXT_PDO)
    if (file_change == enter_file) *line_end++ = '1';
    else if (file_change == leave_file) *line_end++ = '2';
    else if (file_change == enter_expansion) *line_end++ = '5';
    else if (file_change == leave_expansion) *line_end++ = '6';
#else
    *line_end++ = file_change == enter_file ? '1' : '2';
#endif
  }
#if defined (NEXT_SEMANTICS) || defined (NEXT_PDO)
  if (file_change == enter_expansion) { 
    /* note the position of macro reference */
    sprintf (line_end, " %d", (ip->bufp - strlen(ip->nominal_fname))
                              - ip->beg_of_line);
    line_end = &line_cmd_buf[strlen(line_cmd_buf)];
  }
  else if (file_change == leave_expansion) {
    /* note the original character position in the input stream */
    sprintf (line_end, " %d", ip->bufp - ip->beg_of_line);
    line_end = &line_cmd_buf[strlen(line_cmd_buf)];
  }
  else
#endif
  /* Tell cc1 if following text comes from a system header file.  */
  if (ip->system_header_p) {
    *line_end++ = ' ';
    *line_end++ = '3';
  }
#ifndef NO_IMPLICIT_EXTERN_C
  /* Tell cc1plus if following text should be treated as C.  */
  if (ip->system_header_p == 2 && cplusplus) {
    *line_end++ = ' ';
    *line_end++ = '4';
  }
#endif
#ifdef NEXT_CPP_SERVER
   if (serverized) {
      int record_stat_info = 0;
      if (file_change == enter_file) {
         /* header in need of being recorded */
         record_stat_info = 1;
      } else if ((file_change == same_file)
                 && (op == &outbuf)
                 && !outbuf_stat_info_recorded) {
         record_stat_info = 1;
         outbuf_stat_info_recorded = 1;
      }
      if (record_stat_info) {
         int hashval = import_hash (ip->fname);
         struct import_file *import = import_hash_table[hashval];
         /* Is the following loop slowing things down?  Needs investigation. */
         if (import) {
            while (import) {
               if (!strcmp (ip->fname, import->name->fname)) {
                  char tmp[64];
				  int len;
                  sprintf(tmp, " modtime=%d size=%d inode=%d dev=%d",
                          import->modtime, ip->length,
                          import->inode, import->dev);
                  len = strlen(tmp);
			      strncpy(line_end, tmp, len);
                  line_end += len;
                  break;
               }
               import = import->next;
            }
         } 
      }
  }
#endif
  *line_end++ = '\n';
  len = line_end - line_cmd_buf;
  check_expand (op, len + 1);
  if (op->bufp > op->buf && op->bufp[-1] != '\n')
    *op->bufp++ = '\n';
  bcopy ((char *) line_cmd_buf, (char *) op->bufp, len);
  op->bufp += len;
  op->lineno = ip->lineno;
}

/* This structure represents one parsed argument in a macro call.
   `raw' points to the argument text as written (`raw_length' is its length).
   `expanded' points to the argument's macro-expansion
   (its length is `expand_length').
   `stringified_length' is the length the argument would have
   if stringified.
   `use_count' is the number of times this macro arg is substituted
   into the macro.  If the actual use count exceeds 10, 
   the value stored is 10.
   `free1' and `free2', if nonzero, point to blocks to be freed
   when the macro argument data is no longer needed.  */

struct argdata {
  U_CHAR *raw, *expanded;
  int raw_length, expand_length;
  int stringified_length;
  U_CHAR *free1, *free2;
  char newlines;
  char comments;
  char use_count;
};

/* Expand a macro call.
   HP points to the symbol that is the macro being called.
   Put the result of expansion onto the input stack
   so that subsequent input by our caller will use it.

   If macro wants arguments, caller has already verified that
   an argument list follows; arguments come from the input stack.  */

static void
macroexpand (hp, op)
     HASHNODE *hp;
     FILE_BUF *op;
{
  int nargs;
  DEFINITION *defn = hp->value.defn;
  register U_CHAR *xbuf;
  int xbuf_len;
  int start_line = instack[indepth].lineno;
  int rest_args, rest_zero;

  CHECK_DEPTH (return;);

  /* it might not actually be a macro.  */
  if (hp->type != T_MACRO) {
    special_symbol (hp, op);
    return;
  }

  /* This macro is being used inside a #if, which means it must be */
  /* recorded as a precondition.  */
  if (pcp_inside_if && pcp_outfile && defn->predefined)
    dump_single_macro (hp, pcp_outfile);
  
  nargs = defn->nargs;

  if (nargs >= 0) {
    register int i;
    struct argdata *args;
    char *parse_error = 0;

    args = (struct argdata *) alloca ((nargs + 1) * sizeof (struct argdata));

    for (i = 0; i < nargs; i++) {
      args[i].raw = (U_CHAR *) "";
      args[i].expanded = 0;
      args[i].raw_length = args[i].expand_length
	= args[i].stringified_length = 0;
      args[i].free1 = args[i].free2 = 0;
      args[i].use_count = 0;
    }

    /* Parse all the macro args that are supplied.  I counts them.
       The first NARGS args are stored in ARGS.
       The rest are discarded.
       If rest_args is set then we assume macarg absorbed the rest of the args.
       */
    i = 0;
    rest_args = 0;
    do {
      /* Discard the open-parenthesis or comma before the next arg.  */
      ++instack[indepth].bufp;
      if (rest_args)
	continue;
      if (i < nargs || (nargs == 0 && i == 0)) {
	/* if we are working on last arg which absorbs rest of args... */
	if (i == nargs - 1 && defn->rest_args)
	  rest_args = 1;
	parse_error = macarg (&args[i], rest_args);
      }
      else
	parse_error = macarg (NULL_PTR, 0);
      if (parse_error) {
	error_with_line (line_for_error (start_line), parse_error);
	break;
      }
      i++;
    } while (*instack[indepth].bufp != ')');

    /* If we got one arg but it was just whitespace, call that 0 args.  */
    if (i == 1) {
      register U_CHAR *bp = args[0].raw;
      register U_CHAR *lim = bp + args[0].raw_length;
      /* cpp.texi says for foo ( ) we provide one argument.
	 However, if foo wants just 0 arguments, treat this as 0.  */
      if (nargs == 0)
	while (bp != lim && is_space[*bp]) bp++;
      if (bp == lim)
	i = 0;
    }

    /* Don't output an error message if we have already output one for
       a parse error above.  */
    rest_zero = 0;
    if (nargs == 0 && i > 0) {
      if (! parse_error)
	error ("arguments given to macro `%s'", hp->name);
    } else if (i < nargs) {
      /* traditional C allows foo() if foo wants one argument.  */
      if (nargs == 1 && i == 0 && traditional)
	;
      /* the rest args token is allowed to absorb 0 tokens */
      else if (i == nargs - 1 && defn->rest_args)
	rest_zero = 1;
      else if (parse_error)
	;
      else if (i == 0)
	error ("macro `%s' used without args", hp->name);
      else if (i == 1)
	error ("macro `%s' used with just one arg", hp->name);
      else
	error ("macro `%s' used with only %d args", hp->name, i);
    } else if (i > nargs) {
      if (! parse_error)
	error ("macro `%s' used with too many (%d) args", hp->name, i);
    }

    /* Swallow the closeparen.  */
    ++instack[indepth].bufp;

    /* If macro wants zero args, we parsed the arglist for checking only.
       Read directly from the macro definition.  */
    if (nargs == 0) {
      xbuf = defn->expansion;
      xbuf_len = defn->length;
    } else {
      register U_CHAR *exp = defn->expansion;
      register int offset;	/* offset in expansion,
				   copied a piece at a time */
      register int totlen;	/* total amount of exp buffer filled so far */

      register struct reflist *ap, *last_ap;

      /* Macro really takes args.  Compute the expansion of this call.  */

      /* Compute length in characters of the macro's expansion.
	 Also count number of times each arg is used.  */
      xbuf_len = defn->length;
      for (ap = defn->pattern; ap != NULL; ap = ap->next) {
	if (ap->stringify)
	  xbuf_len += args[ap->argno].stringified_length;
	else if (ap->raw_before || ap->raw_after || traditional)
	  /* Add 4 for two newline-space markers to prevent
	     token concatenation.  */
	  xbuf_len += args[ap->argno].raw_length + 4;
	else {
	  /* We have an ordinary (expanded) occurrence of the arg.
	     So compute its expansion, if we have not already.  */
	  if (args[ap->argno].expanded == 0) {
	    FILE_BUF obuf;
	    obuf = expand_to_temp_buffer (args[ap->argno].raw,
					  args[ap->argno].raw + args[ap->argno].raw_length,
					  1, 0);

	    args[ap->argno].expanded = obuf.buf;
	    args[ap->argno].expand_length = obuf.length;
	    args[ap->argno].free2 = obuf.buf;
	  }

	  /* Add 4 for two newline-space markers to prevent
	     token concatenation.  */
	  xbuf_len += args[ap->argno].expand_length + 4;
	}
	if (args[ap->argno].use_count < 10)
	  args[ap->argno].use_count++;
      }

      xbuf = (U_CHAR *) xmalloc (xbuf_len + 1);

      /* Generate in XBUF the complete expansion
	 with arguments substituted in.
	 TOTLEN is the total size generated so far.
	 OFFSET is the index in the definition
	 of where we are copying from.  */
      offset = totlen = 0;
      for (last_ap = NULL, ap = defn->pattern; ap != NULL;
	   last_ap = ap, ap = ap->next) {
	register struct argdata *arg = &args[ap->argno];
	int count_before = totlen;

	/* Add chars to XBUF.  */
	for (i = 0; i < ap->nchars; i++, offset++)
	  xbuf[totlen++] = exp[offset];

	/* If followed by an empty rest arg with concatenation,
	   delete the last run of nonwhite chars.  */
	if (rest_zero && totlen > count_before
	    && ((ap->rest_args && ap->raw_before)
		|| (last_ap != NULL && last_ap->rest_args
		    && last_ap->raw_after))) {
	  /* Delete final whitespace.  */
	  while (totlen > count_before && is_space[xbuf[totlen - 1]]) {
	    totlen--;
	  }

	  /* Delete the nonwhites before them.  */
	  while (totlen > count_before && ! is_space[xbuf[totlen - 1]]) {
	    totlen--;
	  }
	}

	if (ap->stringify != 0) {
	  int arglen = arg->raw_length;
	  int escaped = 0;
	  int in_string = 0;
	  int c;
	  i = 0;
	  while (i < arglen
		 && (c = arg->raw[i], is_space[c]))
	    i++;
	  while (i < arglen
		 && (c = arg->raw[arglen - 1], is_space[c]))
	    arglen--;
	  if (!traditional)
	    xbuf[totlen++] = '\"'; /* insert beginning quote */
	  for (; i < arglen; i++) {
	    c = arg->raw[i];

	    /* Special markers Newline Space
	       generate nothing for a stringified argument.  */
	    if (c == '\n' && arg->raw[i+1] != '\n') {
	      i++;
	      continue;
	    }

	    /* Internal sequences of whitespace are replaced by one space
	       except within an string or char token.  */
	    if (! in_string
		&& (c == '\n' ? arg->raw[i+1] == '\n' : is_space[c])) {
	      while (1) {
		/* Note that Newline Space does occur within whitespace
		   sequences; consider it part of the sequence.  */
		if (c == '\n' && is_space[arg->raw[i+1]])
		  i += 2;
		else if (c != '\n' && is_space[c])
		  i++;
		else break;
		c = arg->raw[i];
	      }
	      i--;
	      c = ' ';
	    }

	    if (escaped)
	      escaped = 0;
	    else {
	      if (c == '\\')
		escaped = 1;
	      if (in_string) {
		if (c == in_string)
		  in_string = 0;
	      } else if (c == '\"' || c == '\'')
		in_string = c;
	    }

	    /* Escape these chars */
	    if (c == '\"' || (in_string && c == '\\'))
	      xbuf[totlen++] = '\\';
	    if (isprint (c))
	      xbuf[totlen++] = c;
	    else {
	      sprintf ((char *) &xbuf[totlen], "\\%03o", (unsigned int) c);
	      totlen += 4;
	    }
	  }
	  if (!traditional)
	    xbuf[totlen++] = '\"'; /* insert ending quote */
	} else if (ap->raw_before || ap->raw_after || traditional) {
	  U_CHAR *p1 = arg->raw;
	  U_CHAR *l1 = p1 + arg->raw_length;
	  if (ap->raw_before) {
	    while (p1 != l1 && is_space[*p1]) p1++;
	    while (p1 != l1 && is_idchar[*p1])
	      xbuf[totlen++] = *p1++;
	    /* Delete any no-reexpansion marker that follows
	       an identifier at the beginning of the argument
	       if the argument is concatenated with what precedes it.  */
	    if (p1[0] == '\n' && p1[1] == '-')
	      p1 += 2;
	  } else if (!traditional) {
	  /* Ordinary expanded use of the argument.
	     Put in newline-space markers to prevent token pasting.  */
	    xbuf[totlen++] = '\n';
	    xbuf[totlen++] = ' ';
	  }
	  if (ap->raw_after) {
	    /* Arg is concatenated after: delete trailing whitespace,
	       whitespace markers, and no-reexpansion markers.  */
	    while (p1 != l1) {
	      if (is_space[l1[-1]]) l1--;
	      else if (l1[-1] == '-') {
		U_CHAR *p2 = l1 - 1;
		/* If a `-' is preceded by an odd number of newlines then it
		   and the last newline are a no-reexpansion marker.  */
		while (p2 != p1 && p2[-1] == '\n') p2--;
		if ((l1 - 1 - p2) & 1) {
		  l1 -= 2;
		}
		else break;
	      }
	      else break;
	    }
	  }

	  bcopy ((char *) p1, (char *) (xbuf + totlen), l1 - p1);
	  totlen += l1 - p1;
	  if (!traditional && !ap->raw_after) {
	    /* Ordinary expanded use of the argument.
	       Put in newline-space markers to prevent token pasting.  */
	    xbuf[totlen++] = '\n';
	    xbuf[totlen++] = ' ';
	  }
	} else {
	  /* Ordinary expanded use of the argument.
	     Put in newline-space markers to prevent token pasting.  */
	  if (!traditional) {
	    xbuf[totlen++] = '\n';
	    xbuf[totlen++] = ' ';
	  }
	  bcopy ((char *) arg->expanded, (char *) (xbuf + totlen),
		 arg->expand_length);
	  totlen += arg->expand_length;
	  if (!traditional) {
	    xbuf[totlen++] = '\n';
	    xbuf[totlen++] = ' ';
	  }
	  /* If a macro argument with newlines is used multiple times,
	     then only expand the newlines once.  This avoids creating output
	     lines which don't correspond to any input line, which confuses
	     gdb and gcov.  */
	  if (arg->use_count > 1 && arg->newlines > 0) {
	    /* Don't bother doing change_newlines for subsequent
	       uses of arg.  */
	    arg->use_count = 1;
	    arg->expand_length
	      = change_newlines (arg->expanded, arg->expand_length);
	  }
	}

	if (totlen > xbuf_len)
	  abort ();
      }

      /* if there is anything left of the definition
	 after handling the arg list, copy that in too. */

      for (i = offset; i < defn->length; i++) {
	/* if we've reached the end of the macro */
	if (exp[i] == ')')
	  rest_zero = 0;
	if (! (rest_zero && last_ap != NULL && last_ap->rest_args
	       && last_ap->raw_after))
	  xbuf[totlen++] = exp[i];
      }

      xbuf[totlen] = 0;
      xbuf_len = totlen;

      for (i = 0; i < nargs; i++) {
	if (args[i].free1 != 0)
	  free (args[i].free1);
	if (args[i].free2 != 0)
	  free (args[i].free2);
      }
    }
  } else {
    xbuf = defn->expansion;
    xbuf_len = defn->length;
  }

  /* Now put the expansion on the input stack
     so our caller will commence reading from it.  */
  {
    register FILE_BUF *ip2;

    ip2 = &instack[++indepth];

    ip2->fname = 0;
    ip2->nominal_fname = 0;
    /* This may not be exactly correct, but will give much better error
       messages for nested macro calls than using a line number of zero.  */
    ip2->lineno = start_line;
    ip2->buf = xbuf;
    ip2->length = xbuf_len;
    ip2->bufp = xbuf;
    ip2->free_ptr = (nargs > 0) ? xbuf : 0;
    ip2->macro = hp;
    ip2->if_stack = if_stack;
    ip2->system_header_p = 0;

    /* Recursive macro use sometimes works traditionally.
       #define foo(x,y) bar (x (y,0), y)
       foo (foo, baz)  */

    if (!traditional)
      hp->type = T_DISABLED;
  }
}

/*
 * Parse a macro argument and store the info on it into *ARGPTR.
 * REST_ARGS is passed to macarg1 to make it absorb the rest of the args.
 * Return nonzero to indicate a syntax error.
 */

static char *
macarg (argptr, rest_args)
     register struct argdata *argptr;
     int rest_args;
{
  FILE_BUF *ip = &instack[indepth];
  int paren = 0;
  int newlines = 0;
  int comments = 0;

  /* Try to parse as much of the argument as exists at this
     input stack level.  */
  U_CHAR *bp = macarg1 (ip->bufp, ip->buf + ip->length,
			&paren, &newlines, &comments, rest_args);

  /* If we find the end of the argument at this level,
     set up *ARGPTR to point at it in the input stack.  */
  if (!(ip->fname != 0 && (newlines != 0 || comments != 0))
      && bp != ip->buf + ip->length) {
    if (argptr != 0) {
      argptr->raw = ip->bufp;
      argptr->raw_length = bp - ip->bufp;
      argptr->newlines = newlines;
    }
    ip->bufp = bp;
  } else {
    /* This input stack level ends before the macro argument does.
       We must pop levels and keep parsing.
       Therefore, we must allocate a temporary buffer and copy
       the macro argument into it.  */
    int bufsize = bp - ip->bufp;
    int extra = newlines;
    U_CHAR *buffer = (U_CHAR *) xmalloc (bufsize + extra + 1);
    int final_start = 0;

    bcopy ((char *) ip->bufp, (char *) buffer, bufsize);
    ip->bufp = bp;
    ip->lineno += newlines;

    while (bp == ip->buf + ip->length) {
      if (instack[indepth].macro == 0) {
	free (buffer);
	return "unterminated macro call";
      }
      ip->macro->type = T_MACRO;
      if (ip->free_ptr)
	free (ip->free_ptr);
      ip = &instack[--indepth];
      newlines = 0;
      comments = 0;
      bp = macarg1 (ip->bufp, ip->buf + ip->length, &paren,
		    &newlines, &comments, rest_args);
      final_start = bufsize;
      bufsize += bp - ip->bufp;
      extra += newlines;
      buffer = (U_CHAR *) xrealloc (buffer, bufsize + extra + 1);
      bcopy ((char *) ip->bufp, (char *) (buffer + bufsize - (bp - ip->bufp)),
	     bp - ip->bufp);
      ip->bufp = bp;
      ip->lineno += newlines;
    }

    /* Now, if arg is actually wanted, record its raw form,
       discarding comments and duplicating newlines in whatever
       part of it did not come from a macro expansion.
       EXTRA space has been preallocated for duplicating the newlines.
       FINAL_START is the index of the start of that part.  */
    if (argptr != 0) {
      argptr->raw = buffer;
      argptr->raw_length = bufsize;
      argptr->free1 = buffer;
      argptr->newlines = newlines;
      argptr->comments = comments;
      if ((newlines || comments) && ip->fname != 0)
	argptr->raw_length
	  = final_start +
	    discard_comments (argptr->raw + final_start,
			      argptr->raw_length - final_start,
			      newlines);
      argptr->raw[argptr->raw_length] = 0;
      if (argptr->raw_length > bufsize + extra)
	abort ();
    }
  }

  /* If we are not discarding this argument,
     macroexpand it and compute its length as stringified.
     All this info goes into *ARGPTR.  */

  if (argptr != 0) {
    register U_CHAR *buf, *lim;
    register int totlen;

    buf = argptr->raw;
    lim = buf + argptr->raw_length;

    while (buf != lim && is_space[*buf])
      buf++;
    while (buf != lim && is_space[lim[-1]])
      lim--;
    totlen = traditional ? 0 : 2;	/* Count opening and closing quote.  */
    while (buf != lim) {
      register U_CHAR c = *buf++;
      totlen++;
      /* Internal sequences of whitespace are replaced by one space
	 in most cases, but not always.  So count all the whitespace
	 in case we need to keep it all.  */
#if 0
      if (is_space[c])
	SKIP_ALL_WHITE_SPACE (buf);
      else
#endif
      if (c == '\"' || c == '\\') /* escape these chars */
	totlen++;
      else if (!isprint (c))
	totlen += 3;
    }
    argptr->stringified_length = totlen;
  }
  return 0;
}

/* Scan text from START (inclusive) up to LIMIT (exclusive),
   counting parens in *DEPTHPTR,
   and return if reach LIMIT
   or before a `)' that would make *DEPTHPTR negative
   or before a comma when *DEPTHPTR is zero.
   Single and double quotes are matched and termination
   is inhibited within them.  Comments also inhibit it.
   Value returned is pointer to stopping place.

   Increment *NEWLINES each time a newline is passed.
   REST_ARGS notifies macarg1 that it should absorb the rest of the args.
   Set *COMMENTS to 1 if a comment is seen.  */

static U_CHAR *
macarg1 (start, limit, depthptr, newlines, comments, rest_args)
     U_CHAR *start;
     register U_CHAR *limit;
     int *depthptr, *newlines, *comments;
     int rest_args;
{
  register U_CHAR *bp = start;

  while (bp < limit) {
    switch (*bp) {
    case '(':
      (*depthptr)++;
      break;
    case ')':
      if (--(*depthptr) < 0)
	return bp;
      break;
    case '\\':
      /* Traditionally, backslash makes following char not special.  */
      if (bp + 1 < limit && traditional)
	{
	  bp++;
	  /* But count source lines anyway.  */
	  if (*bp == '\n')
	    ++*newlines;
	}
      break;
    case '\n':
      ++*newlines;
      break;
    case '/':
      if (bp[1] == '\\' && bp[2] == '\n')
	newline_fix (bp + 1);
      if (cplusplus_comments && bp[1] == '/') {
	*comments = 1;
	bp += 2;
	while (bp < limit && (*bp != '\n' || bp[-1] == '\\')) {
	  if (*bp == '\n') ++*newlines;
	  bp++;
	}
	break;
      }
      if (bp[1] != '*' || bp + 1 >= limit)
	break;
      *comments = 1;
      bp += 2;
      while (bp + 1 < limit) {
	if (bp[0] == '*'
	    && bp[1] == '\\' && bp[2] == '\n')
	  newline_fix (bp + 1);
	if (bp[0] == '*' && bp[1] == '/')
	  break;
	if (*bp == '\n') ++*newlines;
	bp++;
      }
      break;
    case '\'':
    case '\"':
      {
	int quotec;
	for (quotec = *bp++; bp + 1 < limit && *bp != quotec; bp++) {
	  if (*bp == '\\') {
	    bp++;
	    if (*bp == '\n')
	      ++*newlines;
	    while (*bp == '\\' && bp[1] == '\n') {
	      bp += 2;
	    }
	  } else if (*bp == '\n') {
	    ++*newlines;
	    if (quotec == '\'')
	      break;
	  }
	}
      }
      break;
    case ',':
      /* if we've returned to lowest level and we aren't absorbing all args */
      if ((*depthptr) == 0 && rest_args == 0)
	return bp;
      break;
    }
    bp++;
  }

  return bp;
}

/* Discard comments and duplicate newlines
   in the string of length LENGTH at START,
   except inside of string constants.
   The string is copied into itself with its beginning staying fixed.  

   NEWLINES is the number of newlines that must be duplicated.
   We assume that that much extra space is available past the end
   of the string.  */

static int
discard_comments (start, length, newlines)
     U_CHAR *start;
     int length;
     int newlines;
{
  register U_CHAR *ibp;
  register U_CHAR *obp;
  register U_CHAR *limit;
  register int c;

  /* If we have newlines to duplicate, copy everything
     that many characters up.  Then, in the second part,
     we will have room to insert the newlines
     while copying down.
     NEWLINES may actually be too large, because it counts
     newlines in string constants, and we don't duplicate those.
     But that does no harm.  */
  if (newlines > 0) {
    ibp = start + length;
    obp = ibp + newlines;
    limit = start;
    while (limit != ibp)
      *--obp = *--ibp;
  }

  ibp = start + newlines;
  limit = start + length + newlines;
  obp = start;

  while (ibp < limit) {
    *obp++ = c = *ibp++;
    switch (c) {
    case '\n':
      /* Duplicate the newline.  */
      *obp++ = '\n';
      break;

    case '\\':
      if (*ibp == '\n') {
	obp--;
	ibp++;
      }
      break;

    case '/':
      if (*ibp == '\\' && ibp[1] == '\n')
	newline_fix (ibp);
      /* Delete any comment.  */
      if (cplusplus_comments && ibp[0] == '/') {
	/* Comments are equivalent to spaces.  */
	obp[-1] = ' ';
	ibp++;
	while (ibp < limit && (*ibp != '\n' || ibp[-1] == '\\'))
	  ibp++;
	break;
      }
      if (ibp[0] != '*' || ibp + 1 >= limit)
	break;
      /* Comments are equivalent to spaces.  */
      obp[-1] = ' ';
      ibp++;
      while (ibp + 1 < limit) {
	if (ibp[0] == '*'
	    && ibp[1] == '\\' && ibp[2] == '\n')
	  newline_fix (ibp + 1);
	if (ibp[0] == '*' && ibp[1] == '/')
	  break;
	ibp++;
      }
      ibp += 2;
      break;

    case '\'':
    case '\"':
      /* Notice and skip strings, so that we don't
	 think that comments start inside them,
	 and so we don't duplicate newlines in them.  */
      {
	int quotec = c;
	while (ibp < limit) {
	  *obp++ = c = *ibp++;
	  if (c == quotec)
	    break;
	  if (c == '\n' && quotec == '\'')
	    break;
	  if (c == '\\' && ibp < limit) {
	    while (*ibp == '\\' && ibp[1] == '\n')
	      ibp += 2;
	    *obp++ = *ibp++;
	  }
	}
      }
      break;
    }
  }

  return obp - start;
}

/* Turn newlines to spaces in the string of length LENGTH at START,
   except inside of string constants.
   The string is copied into itself with its beginning staying fixed.  */

static int
change_newlines (start, length)
     U_CHAR *start;
     int length;
{
  register U_CHAR *ibp;
  register U_CHAR *obp;
  register U_CHAR *limit;
  register int c;

  ibp = start;
  limit = start + length;
  obp = start;

  while (ibp < limit) {
    *obp++ = c = *ibp++;
    switch (c) {
    case '\n':
      /* If this is a NEWLINE NEWLINE, then this is a real newline in the
	 string.  Skip past the newline and its duplicate.
	 Put a space in the output.  */
      if (*ibp == '\n')
	{
	  ibp++;
	  obp--;
	  *obp++ = ' ';
	}
      break;

    case '\'':
    case '\"':
      /* Notice and skip strings, so that we don't delete newlines in them.  */
      {
	int quotec = c;
	while (ibp < limit) {
	  *obp++ = c = *ibp++;
	  if (c == quotec)
	    break;
	  if (c == '\n' && quotec == '\'')
	    break;
	}
      }
      break;
    }
  }

  return obp - start;
}

/*
 * my_strerror - return the descriptive text associated with an `errno' code.
 */

char *
my_strerror (errnum)
     int errnum;
{
  char *result;

#ifndef VMS
#ifndef HAVE_STRERROR
  result = (char *) ((errnum < sys_nerr) ? sys_errlist[errnum] : 0);
#else
  result = strerror (errnum);
#endif
#else	/* VMS */
  /* VAXCRTL's strerror() takes an optional second argument, which only
     matters when the first argument is EVMSERR.  However, it's simplest
     just to pass it unconditionally.  `vaxc$errno' is declared in
     <errno.h>, and maintained by the library in parallel with `errno'.
     We assume that caller's `errnum' either matches the last setting of
     `errno' by the library or else does not have the value `EVMSERR'.  */

  result = strerror (errnum, vaxc$errno);
#endif

  if (!result)
    result = "undocumented I/O error";

  return result;
}

/*
 * error - print error message and increment count of errors.
 */

void
error (msg, arg1, arg2, arg3)
     char *msg;
     char *arg1, *arg2, *arg3;
{
  int i;
  FILE_BUF *ip = NULL;

  print_containing_files ();

  for (i = indepth; i >= 0; i--)
    if (instack[i].fname != NULL) {
      ip = &instack[i];
      break;
    }

#ifdef REPORT_EVENT
  REPORT_EVENT (0, NULL,
		ip ? ip->nominal_fname : NULL,
		ip ? ip->lineno : 0,
		msg, arg1, arg2, arg3);
#endif

  if (ip != NULL)
#if defined (_WIN32) && defined (NEXT_PDO)
    fprintf (stderr, "%s(%d): ", ip->nominal_fname, ip->lineno);
#else
    fprintf (stderr, "%s:%d: ", ip->nominal_fname, ip->lineno);
#endif
  fprintf (stderr, msg, arg1, arg2, arg3);
  fprintf (stderr, "\n");
  errors++;
}

/* Error including a message from `errno'.  */

static void
error_from_errno (name)
     char *name;
{
  int i;
  FILE_BUF *ip = NULL;

  print_containing_files ();

  for (i = indepth; i >= 0; i--)
    if (instack[i].fname != NULL) {
      ip = &instack[i];
      break;
    }

#ifdef REPORT_EVENT
  REPORT_EVENT (0, NULL,
		ip ? ip->nominal_fname : NULL,
		ip ? ip->lineno : 0,
		"%s: %s", name,
		(errno < sys_nerr)
		? sys_errlist[errno]
		: "undocumented I/O error",
		0);
#endif

  if (ip != NULL)
#if defined (_WIN32) && defined (NEXT_PDO)
    fprintf (stderr, "%s(%d): ", ip->nominal_fname, ip->lineno);
#else
    fprintf (stderr, "%s:%d: ", ip->nominal_fname, ip->lineno);
#endif

  fprintf (stderr, "%s: %s\n", name, my_strerror (errno));

  errors++;
}

/* Print error message but don't count it.  */

void
warning (msg, arg1, arg2, arg3)
     char *msg;
     char *arg1, *arg2, *arg3;
{
  int i;
  FILE_BUF *ip = NULL;

  if (inhibit_warnings)
    return;

  if (warnings_are_errors)
    errors++;

  print_containing_files ();

  for (i = indepth; i >= 0; i--)
    if (instack[i].fname != NULL) {
      ip = &instack[i];
      break;
    }

#ifdef REPORT_EVENT
  REPORT_EVENT (1, NULL,
		ip ? ip->nominal_fname : NULL,
		ip ? ip->lineno : 0,
		msg, arg1, arg2, arg3);
#endif

  if (ip != NULL)
#if defined (_WIN32) && defined (NEXT_PDO)
    fprintf (stderr, "%s(%d): ", ip->nominal_fname, ip->lineno);
#else
    fprintf (stderr, "%s:%d: ", ip->nominal_fname, ip->lineno);
#endif
  fprintf (stderr, "warning: ");
  fprintf (stderr, msg, arg1, arg2, arg3);
  fprintf (stderr, "\n");
}

static void
error_with_line (line, msg, arg1, arg2, arg3)
     int line;
     char *msg;
     char *arg1, *arg2, *arg3;
{
  int i;
  FILE_BUF *ip = NULL;

  print_containing_files ();

  for (i = indepth; i >= 0; i--)
    if (instack[i].fname != NULL) {
      ip = &instack[i];
      break;
    }

#ifdef REPORT_EVENT
  REPORT_EVENT (0, NULL,
		ip ? ip->nominal_fname : NULL,
		line, msg, arg1, arg2, arg3);
#endif

  if (ip != NULL)
#if defined (_WIN32) && defined (NEXT_PDO)
    fprintf (stderr, "%s(%d): ", ip->nominal_fname, line);
#else
    fprintf (stderr, "%s:%d: ", ip->nominal_fname, line);
#endif
  fprintf (stderr, msg, arg1, arg2, arg3);
  fprintf (stderr, "\n");
  errors++;
}

static void
warning_with_line (line, msg, arg1, arg2, arg3)
     int line;
     char *msg;
     char *arg1, *arg2, *arg3;
{
  int i;
  FILE_BUF *ip = NULL;

  if (inhibit_warnings)
    return;

  if (warnings_are_errors)
    errors++;

  print_containing_files ();

  for (i = indepth; i >= 0; i--)
    if (instack[i].fname != NULL) {
      ip = &instack[i];
      break;
    }

#ifdef REPORT_EVENT
  REPORT_EVENT (0, NULL,
		ip ? ip->nominal_fname : NULL,
		line, msg, arg1, arg2, arg3);
#endif

  if (ip != NULL)
#if defined (_WIN32) && defined (NEXT_PDO)
    fprintf (stderr, "%s(%d): ", ip->nominal_fname, line);
#else
    fprintf (stderr, "%s:%d: ", ip->nominal_fname, line);
#endif
  fprintf (stderr, "warning: ");
  fprintf (stderr, msg, arg1, arg2, arg3);
  fprintf (stderr, "\n");
}

/* print an error message and maybe count it.  */

void
pedwarn (msg, arg1, arg2, arg3)
     char *msg;
     char *arg1, *arg2, *arg3;
{
  if (pedantic_errors)
    error (msg, arg1, arg2, arg3);
  else
    warning (msg, arg1, arg2, arg3);
}

void
pedwarn_with_line (line, msg, arg1, arg2, arg3)
     int line;
     char *msg;
     char *arg1, *arg2, *arg3;
{
  if (pedantic_errors)
    error_with_line (line, msg, arg1, arg2, arg3);
  else
    warning_with_line (line, msg, arg1, arg2, arg3);
}

/* Report a warning (or an error if pedantic_errors)
   giving specified file name and line number, not current.  */

static void
pedwarn_with_file_and_line (file, line, msg, arg1, arg2, arg3)
     char *file;
     int line;
     char *msg;
     char *arg1, *arg2, *arg3;
{
#ifdef REPORT_EVENT
  REPORT_EVENT (pedantic_errors ? 0 : 1,
		NULL, file, line, msg, arg1, arg2, arg3);
#endif
  if (!pedantic_errors && inhibit_warnings)
    return;
  if (file != NULL)
#if defined (_WIN32) && defined (NEXT_PDO)
    fprintf (stderr, "%s(%d): ", file, line);
#else
    fprintf (stderr, "%s:%d: ", file, line);
#endif
  if (pedantic_errors)
    errors++;
  if (!pedantic_errors)
    fprintf (stderr, "warning: ");
  fprintf (stderr, msg, arg1, arg2, arg3);
  fprintf (stderr, "\n");
}

/* Print the file names and line numbers of the #include
   commands which led to the current file.  */

static void
print_containing_files ()
{
  FILE_BUF *ip = NULL;
  int i;
  int first = 1;

  /* If stack of files hasn't changed since we last printed
     this info, don't repeat it.  */
  if (last_error_tick == input_file_stack_tick)
    return;

  for (i = indepth; i >= 0; i--)
    if (instack[i].fname != NULL) {
      ip = &instack[i];
      break;
    }

  /* Give up if we don't find a source file.  */
  if (ip == NULL)
    return;

  /* Find the other, outer source files.  */
  for (i--; i >= 0; i--)
    if (instack[i].fname != NULL) {
      ip = &instack[i];
      if (first) {
	first = 0;
	fprintf (stderr, "In file included");
      } else {
	fprintf (stderr, ",\n                ");
      }

      fprintf (stderr, " from %s:%d", ip->nominal_fname, ip->lineno);
    }
  if (! first)
    fprintf (stderr, ":\n");

  /* Record we have printed the status as of this time.  */
  last_error_tick = input_file_stack_tick;
}

/* Return the line at which an error occurred.
   The error is not necessarily associated with the current spot
   in the input stack, so LINE says where.  LINE will have been
   copied from ip->lineno for the current input level.
   If the current level is for a file, we return LINE.
   But if the current level is not for a file, LINE is meaningless.
   In that case, we return the lineno of the innermost file.  */

static int
line_for_error (line)
     int line;
{
  int i;
  int line1 = line;

  for (i = indepth; i >= 0; ) {
    if (instack[i].fname != 0)
      return line1;
    i--;
    if (i < 0)
      return 0;
    line1 = instack[i].lineno;
  }
  abort ();
  /*NOTREACHED*/
  return 0;
}

/*
 * If OBUF doesn't have NEEDED bytes after OPTR, make it bigger.
 *
 * As things stand, nothing is ever placed in the output buffer to be
 * removed again except when it's KNOWN to be part of an identifier,
 * so flushing and moving down everything left, instead of expanding,
 * should work ok.
 */

/* You might think void was cleaner for the return type,
   but that would get type mismatch in check_expand in strict ANSI.  */
static int
grow_outbuf (obuf, needed)
     register FILE_BUF *obuf;
     register int needed;
{
  register U_CHAR *p;
  int minsize;

  if (obuf->length - (obuf->bufp - obuf->buf) > needed)
    return 0;

  /* Make it at least twice as big as it is now.  */
  obuf->length *= 2;
  /* Make it have at least 150% of the free space we will need.  */
  minsize = (3 * needed) / 2 + (obuf->bufp - obuf->buf);
  if (minsize > obuf->length)
    obuf->length = minsize;

  if ((p = (U_CHAR *) xrealloc (obuf->buf, obuf->length)) == NULL)
    memory_full ();

  obuf->bufp = p + (obuf->bufp - obuf->buf);
#ifdef NEXT_CPP_SERVER
  obuf->output_written_to = p + (obuf->output_written_to - obuf->buf);
#endif
  obuf->buf = p;

  return 0;
}

/* Symbol table for macro names and special symbols */

/*
 * install a name in the main hash table, even if it is already there.
 *   name stops with first non alphanumeric, except leading '#'.
 * caller must check against redefinition if that is desired.
 * delete_macro () removes things installed by install () in fifo order.
 * this is important because of the `defined' special symbol used
 * in #if, and also if pushdef/popdef directives are ever implemented.
 *
 * If LEN is >= 0, it is the length of the name.
 * Otherwise, compute the length by scanning the entire name.
 *
 * If HASH is >= 0, it is the precomputed hash code.
 * Otherwise, compute the hash code.
 */
static HASHNODE *
install (name, len, type, ivalue, value, hash)
     U_CHAR *name;
     int len;
     enum node_type type;
     int ivalue;
     char *value;
     int hash;
{
  register HASHNODE *hp;
  register int i, bucket;
  register U_CHAR *p, *q;

  if (len < 0) {
    p = name;
    while (is_idchar[*p])
      p++;
    len = p - name;
  }

  if (hash < 0)
    hash = hashf (name, len, HASHSIZE);

  i = sizeof (HASHNODE) + len + 1;
  hp = (HASHNODE *) xmalloc (i);
  bucket = hash;
  hp->bucket_hdr = &hashtab[bucket];
  hp->next = hashtab[bucket];
  hashtab[bucket] = hp;
  hp->prev = NULL;
  if (hp->next != NULL)
    hp->next->prev = hp;
  hp->type = type;
  hp->length = len;
  if (hp->type == T_CONST)
    hp->value.ival = ivalue;
  else
    hp->value.cpval = value;
  hp->name = ((U_CHAR *) hp) + sizeof (HASHNODE);
#ifdef NEXT_CPP_SERVER
  hp->defined_within_module = 1;
  hp->owner = 0;
#endif
  p = hp->name;
  q = name;
  for (i = 0; i < len; i++)
    *p++ = *q++;
  hp->name[len] = 0;
  return hp;
}

/*
 * find the most recent hash node for name name (ending with first
 * non-identifier char) installed by install
 *
 * If LEN is >= 0, it is the length of the name.
 * Otherwise, compute the length by scanning the entire name.
 *
 * If HASH is >= 0, it is the precomputed hash code.
 * Otherwise, compute the hash code.
 */
HASHNODE *
lookup (name, len, hash)
     U_CHAR *name;
     int len;
     int hash;
{
  register U_CHAR *bp;
  register HASHNODE *bucket;

  if (len < 0) {
    for (bp = name; is_idchar[*bp]; bp++) ;
    len = bp - name;
  }

  if (hash < 0)
#ifdef NEXT_CPP_SERVER
    {
#endif
    hash = hashf (name, len, HASHSIZE);

  bucket = hashtab[hash];
  while (bucket) {
    if (bucket->length == len && strncmp (bucket->name, name, len) == 0)
#ifdef NEXT_CPP_SERVER
        return bucket->defined_within_module ? bucket : NULL;
      bucket = bucket->next;
    }
  } else { /* snaroff: this mod should be improved */
    bucket = hashtab[hash];
    while (bucket) {
      if (bucket->length == len && strncmp (bucket->name, name, len) == 0)
#endif
      return bucket;
    bucket = bucket->next;
#ifdef NEXT_CPP_SERVER
    }
#endif
  }
  return NULL;
}

/*
 * Delete a hash node.  Some weirdness to free junk from macros.
 * More such weirdness will have to be added if you define more hash
 * types that need it.
 */

/* Note that the DEFINITION of a macro is removed from the hash table
   but its storage is not freed.  This would be a storage leak
   except that it is not reasonable to keep undefining and redefining
   large numbers of macros many times.
   In any case, this is necessary, because a macro can be #undef'd
   in the middle of reading the arguments to a call to it.
   If #undef freed the DEFINITION, that would crash.  */

static void
delete_macro (hp)
     HASHNODE *hp;
{

  if (hp->prev != NULL)
    hp->prev->next = hp->next;
  if (hp->next != NULL)
    hp->next->prev = hp->prev;

  /* make sure that the bucket chain header that
     the deleted guy was on points to the right thing afterwards. */
  if (hp == *hp->bucket_hdr)
    *hp->bucket_hdr = hp->next;

#if 0
  if (hp->type == T_MACRO) {
    DEFINITION *d = hp->value.defn;
    struct reflist *ap, *nextap;

    for (ap = d->pattern; ap != NULL; ap = nextap) {
      nextap = ap->next;
      free (ap);
    }
    free (d);
  }
#endif
#ifdef NEXT_CPP_SERVER
  /* a server cannot afford to leak data not in the shared header
     cache, so queue macros to be freed in clear_all_macros() */
  if (!hp->owner) {
     if (hp->type == T_MACRO) {
        hp->next = free_undefined_macros;
        free_undefined_macros = hp;
     } else {
#endif
  free (hp);
#ifdef NEXT_CPP_SERVER
     }
  }
#endif
}

/*
 * return hash function on name.  must be compatible with the one
 * computed a step at a time, elsewhere
 */
static int
hashf (name, len, hashsize)
     register U_CHAR *name;
     register int len;
     int hashsize;
{
  register int r = 0;

  while (len--)
    r = HASHSTEP (r, *name++);

  return MAKE_POS (r) % hashsize;
}


/* Dump the definition of a single macro HP to OF.  */
static void
dump_single_macro (hp, of)
     register HASHNODE *hp;
     FILE *of;
{
  register DEFINITION *defn = hp->value.defn;
  struct reflist *ap;
  int offset;
  int concat;


  /* Print the definition of the macro HP.  */

  fprintf (of, "#define %s", hp->name);

  if (defn->nargs >= 0) {
    int i;

    fprintf (of, "(");
    for (i = 0; i < defn->nargs; i++) {
      dump_arg_n (defn, i, of);
      if (i + 1 < defn->nargs)
	fprintf (of, ", ");
    }
    fprintf (of, ")");
  }

  fprintf (of, " ");

  offset = 0;
  concat = 0;
  for (ap = defn->pattern; ap != NULL; ap = ap->next) {
    dump_defn_1 (defn->expansion, offset, ap->nchars, of);
    offset += ap->nchars;
    if (!traditional) {
      if (ap->nchars != 0)
	concat = 0;
      if (ap->stringify)
	fprintf (of, " #");
      if (ap->raw_before && !concat)
	fprintf (of, " ## ");
      concat = 0;
    }
    dump_arg_n (defn, ap->argno, of);
    if (!traditional && ap->raw_after) {
      fprintf (of, " ## ");
      concat = 1;
    }
  }
  dump_defn_1 (defn->expansion, offset, defn->length - offset, of);
  fprintf (of, "\n");
}

/* Dump all macro definitions as #defines to stdout.  */

static void
dump_all_macros ()
{
  int bucket;

  for (bucket = 0; bucket < HASHSIZE; bucket++) {
    register HASHNODE *hp;

    for (hp = hashtab[bucket]; hp; hp= hp->next) {
      if (hp->type == T_MACRO)
	dump_single_macro (hp, stdout);
    }
  }
}

/* Output to OF a substring of a macro definition.
   BASE is the beginning of the definition.
   Output characters START thru LENGTH.
   Unless traditional, discard newlines outside of strings, thus
   converting funny-space markers to ordinary spaces.  */

static void
dump_defn_1 (base, start, length, of)
     U_CHAR *base;
     int start;
     int length;
     FILE *of;
{
  U_CHAR *p = base + start;
  U_CHAR *limit = base + start + length;

  if (traditional)
    fwrite (p, sizeof (*p), length, of);
  else {
    while (p < limit) {
      if (*p == '\"' || *p =='\'') {
	U_CHAR *p1 = skip_quoted_string (p, limit, 0, NULL_PTR,
					 NULL_PTR, NULL_PTR);
	fwrite (p, sizeof (*p), p1 - p, of);
	p = p1;
      } else {
	if (*p != '\n')
	  putc (*p, of);
	p++;
      }
    }
  }
}

/* Print the name of argument number ARGNUM of macro definition DEFN
   to OF.
   Recall that DEFN->args.argnames contains all the arg names
   concatenated in reverse order with comma-space in between.  */

static void
dump_arg_n (defn, argnum, of)
     DEFINITION *defn;
     int argnum;
     FILE *of;
{
  register U_CHAR *p = defn->args.argnames;
  while (argnum + 1 < defn->nargs) {
    p = (U_CHAR *) index (p, ' ') + 1;
    argnum++;
  }

  while (*p && *p != ',') {
    putc (*p, of);
    p++;
  }
}

/* Initialize syntactic classifications of characters.  */

static void
initialize_char_syntax ()
{
  register int i;

  /*
   * Set up is_idchar and is_idstart tables.  These should be
   * faster than saying (is_alpha (c) || c == '_'), etc.
   * Set up these things before calling any routines tthat
   * refer to them.
   */
  for (i = 'a'; i <= 'z'; i++) {
    is_idchar[i - 'a' + 'A'] = 1;
    is_idchar[i] = 1;
    is_idstart[i - 'a' + 'A'] = 1;
    is_idstart[i] = 1;
  }
  for (i = '0'; i <= '9'; i++)
    is_idchar[i] = 1;
  is_idchar['_'] = 1;
  is_idstart['_'] = 1;
  is_idchar['$'] = dollars_in_ident;
  is_idstart['$'] = dollars_in_ident;

  /* horizontal space table */
  is_hor_space[' '] = 1;
  is_hor_space['\t'] = 1;
  is_hor_space['\v'] = 1;
  is_hor_space['\f'] = 1;
  is_hor_space['\r'] = 1;

  is_space[' '] = 1;
  is_space['\t'] = 1;
  is_space['\v'] = 1;
  is_space['\f'] = 1;
  is_space['\n'] = 1;
  is_space['\r'] = 1;
}

/* Initialize the built-in macros.  */

static void
initialize_builtins (inp, outp)
     FILE_BUF *inp;
     FILE_BUF *outp;
{
  install ("__LINE__", -1, T_SPECLINE, 0, NULL_PTR, -1);
  install ("__DATE__", -1, T_DATE, 0, NULL_PTR, -1);
  install ("__FILE__", -1, T_FILE, 0, NULL_PTR, -1);
  install ("__BASE_FILE__", -1, T_BASE_FILE, 0, NULL_PTR, -1);
  install ("__INCLUDE_LEVEL__", -1, T_INCLUDE_LEVEL, 0, NULL_PTR, -1);
  install ("__VERSION__", -1, T_VERSION, 0, NULL_PTR, -1);
#ifndef NO_BUILTIN_SIZE_TYPE
  install ("__SIZE_TYPE__", -1, T_SIZE_TYPE, 0, NULL_PTR, -1);
#endif
#ifndef NO_BUILTIN_PTRDIFF_TYPE
  install ("__PTRDIFF_TYPE__ ", -1, T_PTRDIFF_TYPE, 0, NULL_PTR, -1);
#endif
  install ("__WCHAR_TYPE__", -1, T_WCHAR_TYPE, 0, NULL_PTR, -1);
  install ("__USER_LABEL_PREFIX__",-1,T_USER_LABEL_PREFIX_TYPE,0,NULL_PTR, -1);
  install ("__REGISTER_PREFIX__", -1, T_REGISTER_PREFIX_TYPE, 0, NULL_PTR, -1);
  install ("__TIME__", -1, T_TIME, 0, NULL_PTR, -1);
  if (!traditional)
    install ("__STDC__", -1, T_CONST, STDC_VALUE, NULL_PTR, -1);
  if (objc)
    install ("__OBJC__", -1, T_CONST, 1, NULL_PTR, -1);
#ifdef NEXT_CPP_SERVER
  install ("_NEXT_SOURCE", -1, T_CONST, 1, NULL_PTR, -1);
  install ("__GNUC__", -1, T_CONST, 2, NULL_PTR, -1);
#endif
/*  This is supplied using a -D by the compiler driver
    so that it is present only when truly compiling with GNU C.  */
/*  install ("__GNUC__", -1, T_CONST, 2, NULL_PTR, -1);  */

  if (debug_output)
    {
      char directive[2048];
      register struct directive *dp = &directive_table[0];
      struct tm *timebuf = timestamp ();

      sprintf (directive, " __BASE_FILE__ \"%s\"\n",
	       instack[0].nominal_fname);
      output_line_command (inp, outp, 0, same_file);
      pass_thru_directive (directive, &directive[strlen (directive)], outp, dp);

      sprintf (directive, " __VERSION__ \"%s\"\n", version_string);
      output_line_command (inp, outp, 0, same_file);
      pass_thru_directive (directive, &directive[strlen (directive)], outp, dp);

#ifndef NO_BUILTIN_SIZE_TYPE
      sprintf (directive, " __SIZE_TYPE__ %s\n", SIZE_TYPE);
      output_line_command (inp, outp, 0, same_file);
      pass_thru_directive (directive, &directive[strlen (directive)], outp, dp);
#endif

#ifndef NO_BUILTIN_PTRDIFF_TYPE
      sprintf (directive, " __PTRDIFF_TYPE__ %s\n", PTRDIFF_TYPE);
      output_line_command (inp, outp, 0, same_file);
      pass_thru_directive (directive, &directive[strlen (directive)], outp, dp);
#endif

      sprintf (directive, " __WCHAR_TYPE__ %s\n", wchar_type);
      output_line_command (inp, outp, 0, same_file);
      pass_thru_directive (directive, &directive[strlen (directive)], outp, dp);

      sprintf (directive, " __DATE__ \"%s %2d %4d\"\n",
	       monthnames[timebuf->tm_mon],
	       timebuf->tm_mday, timebuf->tm_year + 1900);
      output_line_command (inp, outp, 0, same_file);
      pass_thru_directive (directive, &directive[strlen (directive)], outp, dp);

      sprintf (directive, " __TIME__ \"%02d:%02d:%02d\"\n",
	       timebuf->tm_hour, timebuf->tm_min, timebuf->tm_sec);
      output_line_command (inp, outp, 0, same_file);
      pass_thru_directive (directive, &directive[strlen (directive)], outp, dp);

      if (!traditional)
	{
          sprintf (directive, " __STDC__ 1");
          output_line_command (inp, outp, 0, same_file);
          pass_thru_directive (directive, &directive[strlen (directive)],
			       outp, dp);
	}
      if (objc)
	{
          sprintf (directive, " __OBJC__ 1");
          output_line_command (inp, outp, 0, same_file);
          pass_thru_directive (directive, &directive[strlen (directive)],
			       outp, dp);
	}
    }
}

/*
 * process a given definition string, for initialization
 * If STR is just an identifier, define it with value 1.
 * If STR has anything after the identifier, then it should
 * be identifier=definition.
 */

static void
make_definition (str, op)
     U_CHAR *str;
     FILE_BUF *op;
{
  FILE_BUF *ip;
  struct directive *kt;
  U_CHAR *buf, *p;

  buf = str;
  p = str;
  if (!is_idstart[*p]) {
    error ("malformed option `-D %s'", str);
    return;
  }
  while (is_idchar[*++p])
    ;
  if (*p == '(') {
    while (is_idchar[*++p] || *p == ',' || is_hor_space[*p])
      ;
    if (*p++ != ')')
      p = str;			/* Error */
  }
  if (*p == 0) {
    buf = (U_CHAR *) alloca (p - buf + 4);
    strcpy ((char *)buf, str);
    strcat ((char *)buf, " 1");
  } else if (*p != '=') {
    error ("malformed option `-D %s'", str);
    return;
  } else {
    U_CHAR *q;
    /* Copy the entire option so we can modify it.  */
    buf = (U_CHAR *) alloca (2 * strlen (str) + 1);
    strncpy (buf, str, p - str);
    /* Change the = to a space.  */
    buf[p - str] = ' ';
    /* Scan for any backslash-newline and remove it.  */
    p++;
    q = &buf[p - str];
    while (*p) {
      if (*p == '\"' || *p == '\'') {
	int unterminated = 0;
	U_CHAR *p1 = skip_quoted_string (p, p + strlen (p), 0,
					 NULL_PTR, NULL_PTR, &unterminated);
	if (unterminated)
	  return;
	while (p != p1)
	  if (*p == '\\' && p[1] == '\n')
	    p += 2;
	  else
	    *q++ = *p++;
      } else if (*p == '\\' && p[1] == '\n')
	p += 2;
      /* Change newline chars into newline-markers.  */
      else if (*p == '\n')
	{
	  *q++ = '\n';
	  *q++ = '\n';
	  p++;
	}
      else
	*q++ = *p++;
    }
    *q = 0;
  }
  
  ip = &instack[++indepth];
  ip->nominal_fname = ip->fname = "*Initialization*";

  ip->buf = ip->bufp = buf;
  ip->length = strlen (buf);
  ip->lineno = 1;
  ip->macro = 0;
  ip->free_ptr = 0;
  ip->if_stack = if_stack;
  ip->system_header_p = 0;
#ifdef NEXT_CPP_SERVER
  ip->dir = ip->file = 0;
  ip->beg_of_line = 0;
#endif

  for (kt = directive_table; kt->type != T_DEFINE; kt++)
    ;

  /* Pass NULL instead of OP, since this is a "predefined" macro.  */
  do_define (buf, buf + strlen (buf), NULL_PTR, kt);
  --indepth;
}

/* JF, this does the work for the -U option */

static void
make_undef (str, op)
     U_CHAR *str;
     FILE_BUF *op;
{
  FILE_BUF *ip;
  struct directive *kt;

  ip = &instack[++indepth];
  ip->nominal_fname = ip->fname = "*undef*";

  ip->buf = ip->bufp = str;
  ip->length = strlen (str);
  ip->lineno = 1;
  ip->macro = 0;
  ip->free_ptr = 0;
  ip->if_stack = if_stack;
  ip->system_header_p = 0;

  for (kt = directive_table; kt->type != T_UNDEF; kt++)
    ;

  do_undef (str, str + strlen (str), op, kt);
  --indepth;
}

/* Process the string STR as if it appeared as the body of a #assert.
   OPTION is the option name for which STR was the argument.  */

static void
make_assertion (option, str)
     char *option;
     U_CHAR *str;
{
  FILE_BUF *ip;
  struct directive *kt;
  U_CHAR *buf, *p, *q;

  /* Copy the entire option so we can modify it.  */
  buf = (U_CHAR *) alloca (strlen (str) + 1);
  strcpy ((char *) buf, str);
  /* Scan for any backslash-newline and remove it.  */
  p = q = buf;
  while (*p) {
    if (*p == '\\' && p[1] == '\n')
      p += 2;
    else
      *q++ = *p++;
  }
  *q = 0;

  p = buf;
  if (!is_idstart[*p]) {
    error ("malformed option `%s %s'", option, str);
    return;
  }
  while (is_idchar[*++p])
    ;
  while (*p == ' ' || *p == '\t') p++;
  if (! (*p == 0 || *p == '(')) {
    error ("malformed option `%s %s'", option, str);
    return;
  }
  
  ip = &instack[++indepth];
  ip->nominal_fname = ip->fname = "*Initialization*";

  ip->buf = ip->bufp = buf;
  ip->length = strlen (buf);
  ip->lineno = 1;
  ip->macro = 0;
  ip->free_ptr = 0;
  ip->if_stack = if_stack;
  ip->system_header_p = 0;
#ifdef NEXT_CPP_SERVER
  ip->dir = ip->file = 0;
  ip->beg_of_line = 0;
#endif

  for (kt = directive_table; kt->type != T_ASSERT; kt++)
    ;

  /* pass NULL as output ptr to do_define since we KNOW it never
     does any output.... */
  do_assert (buf, buf + strlen (buf) , NULL_PTR, kt);
  --indepth;
}

/* Append a chain of `struct file_name_list's
   to the end of the main include chain.
   FIRST is the beginning of the chain to append, and LAST is the end.  */

static void
append_include_chain (first, last)
     struct file_name_list *first, *last;
{
  struct file_name_list *dir;

  if (!first || !last)
    return;

  if (include == 0)
    include = first;
  else
    last_include->next = first;

  if (first_bracket_include == 0)
    first_bracket_include = first;

  for (dir = first; ; dir = dir->next) {
    int len = strlen (dir->fname) + INCLUDE_LEN_FUDGE;
    if (len > max_include_len)
      max_include_len = len;
    if (dir == last)
      break;
  }

  last->next = NULL;
  last_include = last;
}

#ifdef NEXT_FRAMEWORKS
/* Append a chain of `struct file_name_list's
   to the end of the main Frameworks chain.
   FIRST is the beginning of the chain to append, and LAST is the end.  */

static void
append_framework_chain (first, last)
     struct file_name_list *first, *last;
{
  struct file_name_list *dir;

  if (!first || !last)
    return;

  if (framework == 0)
    framework = first;
  else
    last_framework->next = first;

  if (first_bracket_framework == 0)
    first_bracket_framework = first;

  for (dir = first; ; dir = dir->next) {
    int len = strlen (dir->fname) + INCLUDE_LEN_FUDGE;
    if (len > max_include_len)
      max_include_len = len;
    if (dir == last)
      break;
  }

  last->next = NULL;
  last_framework = last;
}
#endif

/* Add output to `deps_buffer' for the -M switch.
   STRING points to the text to be output.
   SPACER is ':' for targets, ' ' for dependencies, zero for text
   to be inserted literally.  */

static void
deps_output (string, spacer)
     char *string;
     int spacer;
{
  int size = strlen (string);

  if (size == 0)
    return;

#ifndef MAX_OUTPUT_COLUMNS
#define MAX_OUTPUT_COLUMNS 72
#endif
  if (spacer
      && deps_column > 0
      && (deps_column + size) > MAX_OUTPUT_COLUMNS)
  {
    deps_output (" \\\n  ", 0);
    deps_column = 0;
  }

  if (deps_size + size + 8 > deps_allocated_size) {
    deps_allocated_size = (deps_size + size + 50) * 2;
    deps_buffer = (char *) xrealloc (deps_buffer, deps_allocated_size);
  }
  if (spacer == ' ' && deps_column > 0)
    deps_buffer[deps_size++] = ' ';
  bcopy (string, &deps_buffer[deps_size], size);
  deps_size += size;
  deps_column += size;
  if (spacer == ':')
    deps_buffer[deps_size++] = ':';
  deps_buffer[deps_size] = 0;
}

#if defined(USG) || defined(VMS)
#ifndef BSTRING

void
bzero (b, length)
     register char *b;
     register unsigned length;
{
  while (length-- > 0)
    *b++ = 0;
}

void
bcopy (b1, b2, length)
     register char *b1;
     register char *b2;
     register unsigned length;
{
  while (length-- > 0)
    *b2++ = *b1++;
}

int
bcmp (b1, b2, length)	/* This could be a macro! */
     register char *b1;
     register char *b2;
     register unsigned length;
{
   while (length-- > 0)
     if (*b1++ != *b2++)
       return 1;

   return 0;
}
#endif /* not BSTRING */
#endif /* USG or VMS */


static void
fatal (str, arg)
     char *str, *arg;
{
#ifdef REPORT_EVENT
  REPORT_EVENT (0, progname, NULL, 0, str, arg, 0, 0);
#endif
  fprintf (stderr, "%s: ", progname);
  fprintf (stderr, str, arg);
  fprintf (stderr, "\n");
#ifdef NEXT_CPP_SERVER
  if (serverized)
     return;
#endif	
  exit (FAILURE_EXIT_CODE);
}

/* More 'friendly' abort that prints the line and file.
   config.h can #define abort fancy_abort if you like that sort of thing.  */

void
fancy_abort ()
{
  fatal ("Internal gcc abort.");
}

static void
perror_with_name (name)
     char *name;
{
#ifdef REPORT_EVENT
  REPORT_EVENT (0, progname, NULL, 0,
		"%s: %s", name,
		(errno < sys_nerr)
		? sys_errlist[errno]
		: "undocumented I/O error",
		0);
#endif
  fprintf (stderr, "%s: ", progname);
  fprintf (stderr, "%s: %s\n", name, my_strerror (errno));
  errors++;
}

static void
pfatal_with_name (name)
     char *name;
{
#ifdef REPORT_EVENT
  REPORT_EVENT (0, progname, NULL, 0,
		"%s: %s", name,
		(errno < sys_nerr)
		? sys_errlist[errno]
		: "undocumented I/O error",
		0);
#endif
  perror_with_name (name);
#ifdef VMS
  exit (vaxc$errno);
#else
  exit (FAILURE_EXIT_CODE);
#endif
}

/* Handler for SIGPIPE.  */

static void
pipe_closed (signo)
     /* If this is missing, some compilers complain.  */
     int signo;
{
  fatal ("output pipe has been closed");
}

static void
memory_full ()
{
  fatal ("Memory exhausted.");
}


char *
xmalloc (size)
     unsigned size;
{
  register char *ptr = (char *) malloc (size);
  if (ptr != 0) return (ptr);
  memory_full ();
  /*NOTREACHED*/
  return 0;
}

static char *
xrealloc (old, size)
     char *old;
     unsigned size;
{
  register char *ptr = (char *) realloc (old, size);
  if (ptr != 0) return (ptr);
  memory_full ();
  /*NOTREACHED*/
  return 0;
}

static char *
xcalloc (number, size)
     unsigned number, size;
{
  register unsigned total = number * size;
  register char *ptr = (char *) malloc (total);
  if (ptr != 0) {
    if (total > 100)
      bzero (ptr, total);
    else {
      /* It's not too long, so loop, zeroing by longs.
	 It must be safe because malloc values are always well aligned.  */
      register long *zp = (long *) ptr;
      register long *zl = (long *) (ptr + total - 4);
      register int i = total - 4;
      while (zp < zl)
	*zp++ = 0;
      if (i < 0)
	i = 0;
      while (i < total)
	ptr[i++] = 0;
    }
    return ptr;
  }
  memory_full ();
  /*NOTREACHED*/
  return 0;
}

static char *
savestring (input)
     char *input;
{
  unsigned size = strlen (input);
  char *output = xmalloc (size + 1);
#ifdef NEXT_FRAMEWORKS
  strncpy (output, input, size + 1);
#else  
  strcpy (output, input);
#endif
  return output;
}

/* Get the file-mode and data size of the file open on FD
   and store them in *MODE_POINTER and *SIZE_POINTER.  */

static int
file_size_and_mode (fd, mode_pointer, size_pointer)
     int fd;
     int *mode_pointer;
     long int *size_pointer;
{
  struct stat sbuf;

  if (fstat (fd, &sbuf) < 0) return (-1);
  if (mode_pointer) *mode_pointer = sbuf.st_mode;
  if (size_pointer) *size_pointer = sbuf.st_size;
  return 0;
}

static void
output_dots (fd, depth)
     FILE* fd;
     int depth;
{
  while (depth > 0) {
    putc ('.', fd);
    depth--;
  }
}
  

#ifdef VMS

/* Under VMS we need to fix up the "include" specification
   filename so that everything following the 1st slash is
   changed into its correct VMS file specification. */

static void
hack_vms_include_specification (fname)
     char *fname;
{
  register char *cp, *cp1, *cp2;
  int f, check_filename_before_returning, no_prefix_seen;
  char Local[512];

  check_filename_before_returning = 0;
  no_prefix_seen = 0;

  /* Ignore leading "./"s */
  while (fname[0] == '.' && fname[1] == '/') {
    strcpy (fname, fname+2);
    no_prefix_seen = 1;		/* mark this for later */
  }
  /* Look for the boundary between the VMS and UNIX filespecs */
  cp = rindex (fname, ']');	/* Look for end of dirspec. */
  if (cp == 0) cp = rindex (fname, '>'); /* ... Ditto		    */
  if (cp == 0) cp = rindex (fname, ':'); /* Look for end of devspec. */
  if (cp) {
    cp++;
  } else {
    cp = index (fname, '/');	/* Look for the "/" */
  }

  /*
   * Check if we have a vax-c style '#include filename'
   * and add the missing .h
   */
  if (cp == 0) {
    if (index(fname,'.') == 0)
      strcat(fname, ".h");
  } else {
    if (index(cp,'.') == 0)
      strcat(cp, ".h");
  }

  cp2 = Local;			/* initialize */

  /* We are trying to do a number of things here.  First of all, we are
     trying to hammer the filenames into a standard format, such that later
     processing can handle them.
     
     If the file name contains something like [dir.], then it recognizes this
     as a root, and strips the ".]".  Later processing will add whatever is
     needed to get things working properly.
     
     If no device is specified, then the first directory name is taken to be
     a device name (or a rooted logical). */

  /* See if we found that 1st slash */
  if (cp == 0) return;		/* Nothing to do!!! */
  if (*cp != '/') return;	/* Nothing to do!!! */
  /* Point to the UNIX filename part (which needs to be fixed!) */
  cp1 = cp+1;
  /* If the directory spec is not rooted, we can just copy
     the UNIX filename part and we are done */
  if (((cp - fname) > 1) && ((cp[-1] == ']') || (cp[-1] == '>'))) {
    if (cp[-2] != '.') {
      /*
       * The VMS part ends in a `]', and the preceding character is not a `.'.
       * We strip the `]', and then splice the two parts of the name in the
       * usual way.  Given the default locations for include files in cccp.c,
       * we will only use this code if the user specifies alternate locations
       * with the /include (-I) switch on the command line.  */
      cp -= 1;			/* Strip "]" */
      cp1--;			/* backspace */
    } else {
      /*
       * The VMS part has a ".]" at the end, and this will not do.  Later
       * processing will add a second directory spec, and this would be a syntax
       * error.  Thus we strip the ".]", and thus merge the directory specs.
       * We also backspace cp1, so that it points to a '/'.  This inhibits the
       * generation of the 000000 root directory spec (which does not belong here
       * in this case).
       */
      cp -= 2;			/* Strip ".]" */
      cp1--; };			/* backspace */
  } else {

    /* We drop in here if there is no VMS style directory specification yet.
     * If there is no device specification either, we make the first dir a
     * device and try that.  If we do not do this, then we will be essentially
     * searching the users default directory (as if they did a #include "asdf.h").
     *
     * Then all we need to do is to push a '[' into the output string. Later
     * processing will fill this in, and close the bracket.
     */
    if (cp[-1] != ':') *cp2++ = ':'; /* dev not in spec.  take first dir */
    *cp2++ = '[';		/* Open the directory specification */
  }

  /* at this point we assume that we have the device spec, and (at least
     the opening "[" for a directory specification.  We may have directories
     specified already */

  /* If there are no other slashes then the filename will be
     in the "root" directory.  Otherwise, we need to add
     directory specifications. */
  if (index (cp1, '/') == 0) {
    /* Just add "000000]" as the directory string */
    strcpy (cp2, "000000]");
    cp2 += strlen (cp2);
    check_filename_before_returning = 1; /* we might need to fool with this later */
  } else {
    /* As long as there are still subdirectories to add, do them. */
    while (index (cp1, '/') != 0) {
      /* If this token is "." we can ignore it */
      if ((cp1[0] == '.') && (cp1[1] == '/')) {
	cp1 += 2;
	continue;
      }
      /* Add a subdirectory spec. Do not duplicate "." */
      if (cp2[-1] != '.' && cp2[-1] != '[' && cp2[-1] != '<')
	*cp2++ = '.';
      /* If this is ".." then the spec becomes "-" */
      if ((cp1[0] == '.') && (cp1[1] == '.') && (cp[2] == '/')) {
	/* Add "-" and skip the ".." */
	*cp2++ = '-';
	cp1 += 3;
	continue;
      }
      /* Copy the subdirectory */
      while (*cp1 != '/') *cp2++= *cp1++;
      cp1++;			/* Skip the "/" */
    }
    /* Close the directory specification */
    if (cp2[-1] == '.')		/* no trailing periods */
      cp2--;
    *cp2++ = ']';
  }
  /* Now add the filename */
  while (*cp1) *cp2++ = *cp1++;
  *cp2 = 0;
  /* Now append it to the original VMS spec. */
  strcpy (cp, Local);

  /* If we put a [000000] in the filename, try to open it first. If this fails,
     remove the [000000], and return that name.  This provides flexibility
     to the user in that they can use both rooted and non-rooted logical names
     to point to the location of the file.  */

  if (check_filename_before_returning && no_prefix_seen) {
    f = open (fname, O_RDONLY, 0666);
    if (f >= 0) {
      /* The file name is OK as it is, so return it as is.  */
      close (f);
      return;
    }
    /* The filename did not work.  Try to remove the [000000] from the name,
       and return it.  */
    cp = index (fname, '[');
    cp2 = index (fname, ']') + 1;
    strcpy (cp, cp2);		/* this gets rid of it */
  }
  return;
}
#endif	/* VMS */

#ifdef	VMS

/* These are the read/write replacement routines for
   VAX-11 "C".  They make read/write behave enough
   like their UNIX counterparts that CCCP will work */

static int
read (fd, buf, size)
     int fd;
     char *buf;
     int size;
{
#undef	read	/* Get back the REAL read routine */
  register int i;
  register int total = 0;

  /* Read until the buffer is exhausted */
  while (size > 0) {
    /* Limit each read to 32KB */
    i = (size > (32*1024)) ? (32*1024) : size;
    i = read (fd, buf, i);
    if (i <= 0) {
      if (i == 0) return (total);
      return (i);
    }
    /* Account for this read */
    total += i;
    buf += i;
    size -= i;
  }
  return (total);
}

static int
write (fd, buf, size)
     int fd;
     char *buf;
     int size;
{
#undef	write	/* Get back the REAL write routine */
  int i;
  int j;

  /* Limit individual writes to 32Kb */
  i = size;
  while (i > 0) {
    j = (i > (32*1024)) ? (32*1024) : i;
    if (write (fd, buf, j) < 0) return (-1);
    /* Account for the data written */
    buf += j;
    i -= j;
  }
  return (size);
}

/* The following wrapper functions supply additional arguments to the VMS
   I/O routines to optimize performance with file handling.  The arguments
   are:
     "mbc=16" - Set multi-block count to 16 (use a 8192 byte buffer).
     "deq=64" - When extending the file, extend it in chunks of 32Kbytes.
     "fop=tef"- Truncate unused portions of file when closing file.
     "shr=nil"- Disallow file sharing while file is open.
 */

static FILE *
freopen (fname, type, oldfile)
     char *fname;
     char *type;
     FILE *oldfile;
{
#undef	freopen	/* Get back the REAL fopen routine */
  if (strcmp (type, "w") == 0)
    return freopen (fname, type, oldfile, "mbc=16", "deq=64", "fop=tef", "shr=nil");
  return freopen (fname, type, oldfile, "mbc=16");
}

static FILE *
fopen (fname, type)
     char *fname;
     char *type;
{
#undef fopen	/* Get back the REAL fopen routine */
  if (strcmp (type, "w") == 0)
    return fopen (fname, type, "mbc=16", "deq=64", "fop=tef", "shr=nil");
  return fopen (fname, type, "mbc=16");
}

static int 
open (fname, flags, prot)
     char *fname;
     int flags;
     int prot;
{
#undef open	/* Get back the REAL open routine */
  return open (fname, flags, prot, "mbc=16", "deq=64", "fop=tef");
}

/* Avoid run-time library bug, where copying M out of N+M characters with
   N >= 65535 results in VAXCRTL's strncat falling into an infinite loop.
   gcc-cpp exercises this particular bug.  */

static char *
strncat (dst, src, cnt)
     char *dst;
     const char *src;
     unsigned cnt;
{
  register char *d = dst, *s = (char *) src;
  register int n = cnt;	/* convert to _signed_ type */

  while (*d) d++;	/* advance to end */
  while (--n >= 0)
    if (!(*d++ = *s++)) break;
  if (n < 0) *d = '\0';
  return dst;
}
#endif /* VMS */

#ifdef NEXT_SEMANTICS

struct negative_file {
  char *name;
  struct negative_file *next;
};
static struct negative_file *negative_hash_table[IMPORT_HASH_SIZE];

static void
add_to_negative_cache (fname)
     char *fname;
{
  struct negative_file *i;
  int hashval;

  hashval = import_hash (fname);
  i = (struct negative_file *)xmalloc (sizeof (struct negative_file));
  i->name = fname;
  i->next = negative_hash_table[hashval];
  negative_hash_table[hashval] = i;
}

static int
found_in_negative_cache (filename)
     char *filename;
{
  struct negative_file *i;
  int hashval;

  hashval = import_hash (filename);

  /* Attempt to find file in list of already included files */
  i = negative_hash_table[hashval];

  while (i) {
    if (!strcmp (filename, i->name))
      return 1;		/* return found */
    i = i->next;
  }
  return 0;
}

#if 0
void get_input_file_names(in_fname, in_fname_count, in_fname_list)
  char *in_fname;
  int *in_fname_count; /* output */
  struct file_name_list **in_fname_list; /* optional output */
{
  char *in_fnamep = in_fname;
  char c;

  *in_fname_count = 1;
  while (c = *in_fnamep++)
    if (c == ' ') (*in_fname_count)++; /* need the parens */

  if (*in_fname_count > 1) {
    int nelem = 0;
    *in_fname_list = (struct file_name_list *) xmalloc (*in_fname_count * 
					     sizeof (struct file_name_list));
    in_fnamep = in_fname;
    if ((*in_fname_list)[nelem++].fname = strtok(in_fnamep," "))
      while ((*in_fname_list)[nelem++].fname = strtok(NULL," "));
  } else {
    *in_fname_list = 0;
  }
}
#endif

static void
add_version_of_header(fileptr, start, length)
     struct file_name_list *fileptr;
     unsigned int start;
     unsigned int length;
{
   struct file_name_list *ptr;

   ptr = (struct file_name_list *) xmalloc (sizeof (struct file_name_list));
   bzero (ptr, sizeof (struct file_name_list));

   ptr->fname = fileptr->fname;
   ptr->versions = fileptr->versions;
   fileptr->versions = ptr;
   ptr->relative_position_of_file_start = start;
   ptr->file_length = length;
}

static int
duplicate_version_of_header(op, fileptr, position_of_file_start, 
                                     file_length)
     FILE_BUF *op;
     struct file_name_list *fileptr;
     U_CHAR *position_of_file_start;
     unsigned int file_length;
{
  int found_a_version = 0;
  do {
    if ((file_length == fileptr->file_length) &&
        (!bcmp (position_of_file_start, 
                op->buf+fileptr->relative_position_of_file_start,
                file_length))) {
      found_a_version = 1;
    } 
    fileptr = fileptr->versions;
  } while (fileptr && !found_a_version);
  return found_a_version;
}

/* This saves processing 497k for "appkit.h" (with the "include" heuristic)
   This results in a significant savings in user time (~40% I believe!) */

static int
aggressive_skip_if_group (ip)
     FILE_BUF *ip;
{
    if (ip->bufp == ip->file->position_after_last_include) {

      /* #if/#ifdef/#ifndef's are all identified by "T_IF" (see calls to
         conditional_skip in do_if/do_xifdef) */
      if (if_stack && if_stack->type == T_IF && if_stack->control_macro) {
        /* just because we are in the context of "some" #ifndef, check
           whether the input we are currently processing had one. If it did,
           make sure we skip to its matching #endif, else go to end of buf */
        if (ip->file->control_macro)
          ip->bufp = ip->file->position_of_last_endif;
        else
          ip->bufp = ip->buf + ip->length;
        enable_macros_from_header(ip->file);
        /* fprintf(stderr, "SKIP for %s: %.20s (saved %d)\n", 
         *        ip->fname, ip->bufp, total_saved += ip->bufp-sav);
         */
        return 1;
      } else {
        /* this means the last include is in the context of an arbitrary
           conditional (i.e. not the "ifndef idiom"). for now, we will just
           exclude it from this optimization. It is possible to include it, 
           but this will require more complexity */
        /* fprintf(stderr, "CAN'T SKIP for %s: %.20s\n", ip->fname, ip->bufp); */
        return 0;
      }
    } else if (!ip->file->position_after_last_include) { 
      /* no #includes, currently processing a "leaf" header */
      ip->bufp = ip->buf + ip->length;
      enable_macros_from_header(ip->file);
      /* fprintf(stderr, "NO INCLUDES for %s: %.20s (saved %d)\n", 
       *        ip->fname, ip->bufp, total_saved += ip->bufp-sav);
       */
      return 1;
    } else { /* this case is not currently optimized (obviously) */
      return 0;
    }
}

static void
dump_memory_statistics ()
{
  struct file_name_list *new;
  struct import_file *imp;
  struct negative_file *neg;
  int n_elements = 0, n_other = 0, size_bufs = 0, size_mlists = 0;
  int n_expansion_strings = 0, n_string_count = 0, control_names = 0;
  int bucket;

  fprintf(stderr, "all_input_files (chain of struct file_name_list's):\n");
  for (new = all_input_files; new; new = new->next) {
    n_elements++;
    n_string_count += (strlen(new->fname)+1);
    size_bufs += new->length;
  }
  fprintf(stderr, "%10d bytes for %d elements (%d bytes each)\n",
                   n_elements * sizeof(struct file_name_list),
                   n_elements, sizeof(struct file_name_list));
  fprintf(stderr, "%10d bytes for input file names\n", n_string_count);
  fprintf(stderr, "%10d bytes for input buffers\n", size_bufs);

  n_string_count = n_elements = size_bufs = 0;

  fprintf(stderr, "all_output_files (chain of struct file_name_list's):\n");
  for (new = all_output_files; new; new = new->next) {
    n_elements++;
    if (new->fname)
      n_string_count += (strlen(new->fname)+1);
    size_bufs += new->length;
  }
  fprintf(stderr, "%10d bytes for %d elements (%d bytes each)\n",
                   n_elements * sizeof(struct file_name_list),
                   n_elements, sizeof(struct file_name_list));
  fprintf(stderr, "%10d bytes for output file names\n", n_string_count);
  fprintf(stderr, "%10d bytes for output buffers\n", size_bufs);

  n_string_count = n_elements = size_bufs = 0;
  fprintf(stderr, "all_include_files (chain of struct file_name_list's):\n");
  for (new = all_include_files; new; new = new->next) {
    n_elements++;
    n_string_count += (strlen(new->fname)+1);
    if (new->control_macro)
      control_names += (strlen(new->control_macro)+1);
    size_bufs += new->length;
    size_mlists += new->macro_count * sizeof ( HASHNODE * );
    fprintf(stderr, "%s\n", new->fname);
  }
  fprintf(stderr, "%10d bytes for %d elements (%d bytes each)\n",
                   n_elements * sizeof(struct file_name_list),
                   n_elements, sizeof(struct file_name_list));
  fprintf(stderr, "%10d bytes for file names\n", n_string_count);
  fprintf(stderr, "%10d bytes for control macro names\n", control_names);
  fprintf(stderr, "%10d bytes for input buffers\n", size_bufs);
  fprintf(stderr, "%10d bytes for macro lists\n", size_mlists);

  n_string_count = n_elements = 0;
  fprintf(stderr, "include directories (chain of struct file_name_list's):\n");
  for (new = include; new; new = new->next) {
    n_elements++;
    n_string_count += (strlen(new->fname)+1);
  }
  fprintf(stderr, "%10d bytes for %d elements (%d bytes each)\n",
                   n_elements * sizeof(struct file_name_list),
                   n_elements, sizeof(struct file_name_list));
  fprintf(stderr, "%10d bytes for directory names\n", n_string_count);

  n_elements = 0;
  fprintf(stderr, "import_hash_table (struct import_file's):\n");
  for (bucket = 0; bucket < IMPORT_HASH_SIZE; bucket++) {
    imp = import_hash_table[bucket];
    while (imp) {
      n_elements++;
      imp = imp->next;
    }
  }
  fprintf(stderr, "%10d bytes for import hash table buckets (statically allocated)\n",
		   sizeof(import_hash_table));
  fprintf(stderr, "%10d bytes for %d elements (%d bytes each)\n",
                   n_elements * sizeof(struct import_file),
                   n_elements, sizeof(struct import_file));

  n_elements = 0;
  n_string_count = 0;
  fprintf(stderr, "negative_hash_table (struct negative_file's):\n");
  for (bucket = 0; bucket < IMPORT_HASH_SIZE; bucket++) {
    neg = negative_hash_table[bucket];
    while (neg) {
      n_elements++;
      n_string_count += (strlen(neg->name)+1);
      neg = neg->next;
    }
  }
  fprintf(stderr, "%10d bytes for negative hash table buckets (statically allocated)\n",
		   sizeof(negative_hash_table));
  fprintf(stderr, "%10d bytes for %d elements (%d bytes each)\n",
                   n_elements * sizeof(struct negative_file),
                   n_elements, sizeof(struct negative_file));
  fprintf(stderr, "%10d bytes for file names\n", n_string_count);

  fprintf(stderr, "hashtab for macro's (struct hashnode's/definition's):\n");
  n_string_count = 0;
  n_elements = 0;
  n_other = 0;
  for (bucket = 0; bucket < HASHSIZE; bucket++) {
    register HASHNODE *hp;

    for (hp = hashtab[bucket]; hp; hp= hp->next) {
      if (hp->type == T_MACRO) {
        n_elements++;
        if (hp->value.defn->length) {
          n_string_count++;
          n_expansion_strings += hp->value.defn->length;
        }
      } else {
        n_other++;
      }
    }
  }
  fprintf(stderr, "%10d bytes for macro hash table buckets (statically allocated)\n",
		   sizeof(hashtab));
  fprintf(stderr, "%10d bytes for %d macro hashnode's (%d bytes each)\n",
                   n_elements * sizeof(struct hashnode),
                   n_elements, sizeof(struct hashnode));
  fprintf(stderr, "%10d bytes for %d macro definitions's (%d bytes each)\n",
                   n_elements * sizeof(struct definition),
                   n_elements, sizeof(struct definition));
  fprintf(stderr, "%10d bytes for %d macro expansion strings\n",
                   n_expansion_strings, n_string_count);
  fprintf(stderr, "%10d bytes for %d other hashnode's (%d bytes each)\n",
                   n_other * sizeof(struct hashnode),
                   n_other, sizeof(struct hashnode));
  return;
}

static void
enable_macros_from_header(entry)
   struct file_name_list *entry;
{
   int i;

   if (entry->macro_list) {
      /* list may not yet be filled in */
      for (i = 0; i < entry->macro_count; i++) {
         entry->macro_list[i]->defined_within_module = 1;
      }
   }
}

static void
clear_import()
{
  struct import_file *i;
  int h;

  for (h = 0; h < IMPORT_HASH_SIZE; h++) {
    i = import_hash_table[h];
    while (i) {
      i->visited_within_module = 0;
      /* dump_file_name_list(i->name); */
      i = i->next;
    }
  }
}

static void
really_delete_macro(hp)
HASHNODE * hp;
{
   DEFINITION *d = hp->value.defn;
   struct reflist *ap, *nextap;

   for (ap = d->pattern; ap != NULL; ap = nextap) {
      nextap = ap->next;
      free (ap);
   }
   if ((d->nargs > 0) && d->args.argnames)
      free (d->args.argnames);

   free (d);
   free (hp);
}

static void
clear_all_macros ()
{
  int bucket;
  register HASHNODE *hp, *hp_next;

  for (bucket = 0; bucket < HASHSIZE; bucket++) {

    for (hp = hashtab[bucket]; hp; hp= hp->next) {
       if (hp->type == T_MACRO) {
        /* possible bug...are we disabling command line stuff? Probably not. */
	hp->defined_within_module = 0;
        if (hp->owner && hp->owner->macro_count) {
          if (hp->owner->macro_index < hp->owner->macro_count) { 
            /* not done filling in the list */
            if (!hp->owner->macro_list) {
              int nbytes = hp->owner->macro_count * sizeof ( HASHNODE * );
              hp->owner->macro_list = (HASHNODE **) xmalloc ( nbytes );
              bzero(hp->owner->macro_list, nbytes);
            } 
            /* stash in the next empty slot */
            hp->owner->macro_list[hp->owner->macro_index++] = hp;
          } 
          /* else, header has been filled (or it has no macros) */
        } else { 
          /* defined on the command line (not a header) */
        }
      }
    }
  }

  /* free macros from delete_macro (usually due to #undef) */
  for (hp = free_undefined_macros; hp; hp = hp_next) {
     hp_next = hp->next;
     really_delete_macro (hp);
  }
#if 0
  /* free macros from delete_macro (usually due to #undef) */
  for (hp = installed_builtins; hp; hp = hp_next) {
     hp_next = hp->next;
     really_delete_macro (hp);
  }
#endif  
}

/* Pre-C-Preprocessor to translate RTF (Rich Text Format) Level 0
   to plain ASCII before normal preprocessing.  This removes all font, color,
   and help links from the text, but leaves the actual contents unchanged.

   Using an extra pass through the buffer takes a little extra time,
   but Rich Source Code is rare and a separate, shared routine
   can be used.   */

#define RTF_HEADER     "{\\rtf0\\ansi"
#define RTF_HEADER_LEN 11

static void
buf_convert_rtf (buf)
     FILE_BUF *buf;
{
  int asciiTextLen;

  if (buf->buf 
      && buf->length > RTF_HEADER_LEN 
      && !strncmp (buf->buf, RTF_HEADER, RTF_HEADER_LEN)) 
    {
      const char * asciiText = rtf_to_ascii(buf->buf, buf->length, &asciiTextLen);
      if (asciiText) 
	{
	  /* FIXME: deallocate buf->buf */
	  buf->buf = buf->bufp = (char *) asciiText;
	  buf->length = asciiTextLen;
	}
    }
}


/* Process rtf text. RTFTEXTLEN bytes are processed from
   RTFTEXTPTR.  The output is ASCIITEXTLEN bytes in the same buffer. */

#define ASCII_TERMINATOR '\0' 

static const char *
rtf_to_ascii (const char *rtfTextPtr, int rtfTextLen, int * asciiTextLen) 
{
  char * asciiTextBuf = (char *) xmalloc(rtfTextLen);
  char * asciiTextPtr = asciiTextBuf;
  const char *endRtfPtr = rtfTextPtr + rtfTextLen;

  if (!asciiTextBuf)
    return NULL;
  
  while ( rtfTextPtr != endRtfPtr ) {
    switch( *rtfTextPtr ) {
    case '\n':
      rtfTextPtr++;
      break;
    case '}':
      if ( *++rtfTextPtr == '\n' ) {
	rtfTextPtr++;
	if (*rtfTextPtr == '�') rtfTextPtr++;
      }	     
      break;
    case '{':
      if ( !strncmp( rtfTextPtr, "{\\stylesheet", 12 ) 
	  || !strncmp( rtfTextPtr, "{\\fonttbl", 9 ) 
	  || !strncmp( rtfTextPtr, "{\\colortbl", 10 ) 
	  || !strncmp( rtfTextPtr, "{\\NeXTHelp", 10 )
	  || !strncmp( rtfTextPtr, "{{\\NeXTHelp", 11 ) ) {
	int depth = 1;
	while ( depth > 0 ) {
	  switch ( *++rtfTextPtr ) {
	  case '{': depth++; break;
	  case '}': depth--; break;
	  }
	}
      }
      else
	rtfTextPtr++;
      break;
    case '\\':
      rtfTextPtr++;
      if (*rtfTextPtr == '{'  
	  || *rtfTextPtr == '}'  
	  ||  *rtfTextPtr == '\\'  
	  ||  *rtfTextPtr == '\n' ) {
	*asciiTextPtr++ = *rtfTextPtr++;
      } else if ( *rtfTextPtr == '\'' ) {
	int v1, v2, value;
	rtfTextPtr++;
	v1 = isdigit( *rtfTextPtr ) ? (*rtfTextPtr - '0') 
	  : ((*rtfTextPtr - 'a') + 10);
	rtfTextPtr++;
	v2 = isdigit( *rtfTextPtr ) ? (*rtfTextPtr - '0') 
	  : ((*rtfTextPtr - 'a') + 10);
	value = (v1 << 4) | (v2);
	*asciiTextPtr++ = value;
	rtfTextPtr++;
      } else {
	while ( isalpha( *rtfTextPtr ) ) rtfTextPtr++;
	if (*rtfTextPtr == '-') rtfTextPtr++;
	while ( isdigit( *rtfTextPtr ) ) rtfTextPtr++;
	if ( *rtfTextPtr == ' ' ) rtfTextPtr++;
	else while (*rtfTextPtr == ' ') rtfTextPtr++;
      }
      break;
    default:
      *asciiTextPtr++ = *rtfTextPtr++;
    }       
  }
  *asciiTextPtr = ASCII_TERMINATOR;
  *asciiTextLen = (int)asciiTextPtr - (int)asciiTextBuf;
  return asciiTextBuf;
}


/* Returns last known modifiy time for FILENAME */

time_t file_modtime(const char * filename) {
  int hashval = import_hash (filename);

  /* Attempt to find file in list of already included files */
  struct import_file *i = import_hash_table[hashval];

  while (i) {
     if (!strcmp (filename, i->name->fname)) {
        return i->modtime;
     }
     i = i->next;
  }
  return 0;
}

static void handle_file_modification(const char * filename) {
  struct file_name_list *ptr, *next_ptr, *prev_ptr, *version, *next_version;
  int hashval = import_hash (filename);
  int multiple_versions = 0, removed_from_hash = 0, removed_from_cache = 0;
  struct import_file *prev = NULL;
  struct import_file *i = import_hash_table[hashval];
  while (i) {
     if (!strcmp (filename, i->name->fname)) {
        if (prev)
           prev->next = i->next;
        else
           import_hash_table[hashval] = i->next;
        removed_from_hash = 1;
     }
     prev = i;
     i = i->next;
  }
  if (!removed_from_hash) {
     const char * basefilename = strrchr(filename,'/');
     if (basefilename) {
        /* Look for other files by the same name  */
        for (hashval = 0; hashval < IMPORT_HASH_SIZE; hashval++) {
	   prev = NULL;
           i = import_hash_table[hashval];
           while (i) {
              const char * baseiname = strrchr(i->name->fname,'/');
              if (!strcmp (basefilename, baseiname)) {
                 if (prev)
                    prev->next = i->next;
                 else
                    import_hash_table[hashval] = i->next;
                 fprintf(stderr,"...punted %s\n",i->name->fname);
              }
              prev = i;
              i = i->next;
           }
        }
     } else {
        fprintf(stderr,"Cpp Internal Error: could not remove file '%s' from import hash table.  Looks like a bad file name!!\n", filename);
     }
  }


  /* This looks through the entire header cache for copies of this header.
     It's probably more efficient to terminate the search after finding the
     header being modified, but the conservative approach is more likely
     to be correct. */
  
  prev_ptr = NULL;
  for (ptr = next_ptr = all_include_files; ptr; ptr = next_ptr) {
     next_ptr = ptr->next;
     if (ptr->fname && !strcmp (ptr->fname, filename)) {
        if (ptr->versions) {
           multiple_versions = 1;
           for (version = next_version = ptr->versions;
                version;
                version = next_version) {
              next_version = version->next;
              free(version);
           }
        }
        if (prev_ptr)
           prev_ptr->next = next_ptr;
        else
           all_include_files = next_ptr;
        free(ptr->fname);
        free(ptr);
        removed_from_cache = 1;
     }
     prev_ptr = ptr;
  }

#ifdef DEBUG
  if (!removed_from_cache)
     fprintf(stderr,"Warning: could not remove file '%s' from import cache.\n", filename);
#endif
  
  if (multiple_versions)
     fprintf(stderr,"Removed all versions of %s from header cache\n", filename);
  else
     fprintf(stderr,"Removed %s from header cache\n", filename);
  fflush(stderr);
}

static void init_output_file_buf(FILE_BUF * outbuf)
{
  outbuf->buf = (U_CHAR *) xmalloc (OUTBUF_SIZE);
  outbuf->length = OUTBUF_SIZE;
  outbuf->bufp = outbuf->output_written_to = outbuf->buf;
  bzero (outbuf->bufp, outbuf->length);
  outbuf->lineno = 0;
}


static int sysUsr_startTime;
static int sysUsr_finishTime;
  
static struct timeval real_startTime;
static struct timeval real_finishTime;
  
  /* Return time used so far, in microseconds.  */
static int gettime ()
{
#if defined (NEXT_PDO) && defined (hpux)
   struct tms tms;
   times (&tms);
   return tms.tms_utime / CLK_TCK;
#else
   struct rusage rusage;

   getrusage (0, &rusage);
   return (rusage.ru_utime.tv_sec * 1000000 + rusage.ru_utime.tv_usec
	    + rusage.ru_stime.tv_sec * 1000000 + rusage.ru_stime.tv_usec);
#endif
}

static void start() 
{
    sysUsr_startTime = gettime();
    gettimeofday(&real_startTime, 0);
}

static void stop() 
{
    sysUsr_finishTime = gettime();
    gettimeofday(&real_finishTime, 0);
}

static void display(FILE * strm, int addNewLine)
{
    real_finishTime.tv_sec -= real_startTime.tv_sec;
    real_finishTime.tv_usec -= real_startTime.tv_usec;
    if (real_finishTime.tv_usec < 0)
      real_finishTime.tv_sec--, real_finishTime.tv_usec += 1000000;
      
    fprintf(strm, "%3d.%03d (sys+usr) %3d.%01d (real)%s",
	     (sysUsr_finishTime-sysUsr_startTime) / 1000000, 
	     (sysUsr_finishTime-sysUsr_startTime) % 1000000, 
	     real_finishTime.tv_sec, 
	     real_finishTime.tv_usec/100000,
	     addNewLine ? "\n" : "");
}
#endif

#endif /* !_WIN32 || !NeXT_PDO */
