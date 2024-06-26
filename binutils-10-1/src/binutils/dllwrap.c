/* dllwrap.c -- wrapper for DLLTOOL and GCC to generate PE style DLLs
   Copyright (C) 1998, 1999, 2000 Free Software Foundation, Inc.
   Contributed by Mumit Khan (khan@xraylith.wisc.edu).

   This file is part of GNU Binutils.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */

/* AIX requires this to be the first thing in the file.  */
#ifndef __GNUC__
# ifdef _AIX
 #pragma alloca
#endif
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "bfd.h"
#include "libiberty.h"
#include "bucomm.h"
#include "getopt.h"
#include "dyn-string.h"

#include <ctype.h>
#include <time.h>
#include <sys/stat.h>

#ifdef ANSI_PROTOTYPES
#include <stdarg.h>
#else
#include <varargs.h>
#endif

#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#else /* ! HAVE_SYS_WAIT_H */
#if ! defined (_WIN32) || defined (__CYGWIN32__)
#ifndef WIFEXITED
#define WIFEXITED(w)	(((w)&0377) == 0)
#endif
#ifndef WIFSIGNALED
#define WIFSIGNALED(w)	(((w)&0377) != 0177 && ((w)&~0377) == 0)
#endif
#ifndef WTERMSIG
#define WTERMSIG(w)	((w) & 0177)
#endif
#ifndef WEXITSTATUS
#define WEXITSTATUS(w)	(((w) >> 8) & 0377)
#endif
#else /* defined (_WIN32) && ! defined (__CYGWIN32__) */
#ifndef WIFEXITED
#define WIFEXITED(w)	(((w) & 0xff) == 0)
#endif
#ifndef WIFSIGNALED
#define WIFSIGNALED(w)	(((w) & 0xff) != 0 && ((w) & 0xff) != 0x7f)
#endif
#ifndef WTERMSIG
#define WTERMSIG(w)	((w) & 0x7f)
#endif
#ifndef WEXITSTATUS
#define WEXITSTATUS(w)	(((w) & 0xff00) >> 8)
#endif
#endif /* defined (_WIN32) && ! defined (__CYGWIN32__) */
#endif /* ! HAVE_SYS_WAIT_H */

static char *program_version = "0.2.4";
static char *driver_name = NULL;
static char *cygwin_driver_flags = 
  "-Wl,--dll -nostartfiles";
static char *mingw32_driver_flags = "-mdll";
static char *generic_driver_flags = "-Wl,--dll";

static char *entry_point;

static char *dlltool_name = NULL;

static char *target = TARGET;

typedef enum {
  UNKNOWN_TARGET, 
  CYGWIN_TARGET, 
  MINGW_TARGET
} 
target_type;

static target_type which_target = UNKNOWN_TARGET;

static int dontdeltemps = 0;
static int dry_run = 0;

static char *program_name;

static int verbose = 0;

static char *dll_file_name;
static char *dll_name;
static char *base_file_name;
static char *exp_file_name;
static char *def_file_name;
static int delete_base_file = 1;
static int delete_exp_file = 1;
static int delete_def_file = 1;

static int run PARAMS ((const char *, char *));
static void usage PARAMS ((FILE *, int));
static void display PARAMS ((const char *, va_list));
static void inform PARAMS ((const char *, ...));
static char *look_for_prog PARAMS ((const char *, const char *, int));
static char *deduce_name PARAMS ((const char *));
static void delete_temp_files PARAMS ((void));
static void cleanup_and_exit PARAMS ((int status));

/**********************************************************************/

/* Please keep the following 4 routines in sync with dlltool.c:
     display ()
     inform ()
     look_for_prog ()
     deduce_name ()
   It's not worth the hassle to break these out since dllwrap will
   (hopefully) soon be retired in favor of `ld --shared.  */

static void
display (message, args)
     const char * message;
     va_list      args;
{
  if (program_name != NULL)
    fprintf (stderr, "%s: ", program_name);

  vfprintf (stderr, message, args);

  if (message [strlen (message) - 1] != '\n')
    fputc ('\n', stderr);
}  


static void
#ifdef __STDC__
inform (const char * message, ...)
#else
inform (message, va_alist)
     const char * message;
     va_dcl
#endif
{
  va_list args;
  
  if (!verbose)
    return;

#ifdef __STDC__
  va_start (args, message);
#else
  va_start (args);
#endif

  display (message, args);
  
  va_end (args);
}

/* Look for the program formed by concatenating PROG_NAME and the
   string running from PREFIX to END_PREFIX.  If the concatenated
   string contains a '/', try appending EXECUTABLE_SUFFIX if it is
   appropriate.  */

static char *
look_for_prog (prog_name, prefix, end_prefix)
     const char *prog_name;
     const char *prefix;
     int end_prefix;
{
  struct stat s;
  char *cmd;

  cmd = xmalloc (strlen (prefix) 
                 + strlen (prog_name) 
#ifdef HAVE_EXECUTABLE_SUFFIX
                 + strlen (EXECUTABLE_SUFFIX) 
#endif
		 + 10);
  strcpy (cmd, prefix);

  sprintf (cmd + end_prefix, "%s", prog_name);

  if (strchr (cmd, '/') != NULL)
    {
      int found;

      found = (stat (cmd, &s) == 0
#ifdef HAVE_EXECUTABLE_SUFFIX
               || stat (strcat (cmd, EXECUTABLE_SUFFIX), &s) == 0
#endif
	       );

      if (! found)
        {
	  /* xgettext:c-format */
	  inform (_("Tried file: %s"), cmd);
	  free (cmd);
	  return NULL;
	}
    }

  /* xgettext:c-format */
  inform (_("Using file: %s"), cmd);

  return cmd;
}

/* Deduce the name of the program we are want to invoke.
   PROG_NAME is the basic name of the program we want to run,
   eg "as" or "ld".  The catch is that we might want actually
   run "i386-pe-as" or "ppc-pe-ld".  

   If argv[0] contains the full path, then try to find the program
   in the same place, with and then without a target-like prefix.

   Given, argv[0] = /usr/local/bin/i586-cygwin32-dlltool,
   deduce_name("as") uses the following search order: 

     /usr/local/bin/i586-cygwin32-as
     /usr/local/bin/as
     as
   
   If there's an EXECUTABLE_SUFFIX, it'll use that as well; for each
   name, it'll try without and then with EXECUTABLE_SUFFIX.

   Given, argv[0] = i586-cygwin32-dlltool, it will not even try "as"
   as the fallback, but rather return i586-cygwin32-as.
     
   Oh, and given, argv[0] = dlltool, it'll return "as".

   Returns a dynamically allocated string.  */

static char *
deduce_name (prog_name)
     const char *prog_name;
{
  char *cmd;
  char *dash, *slash, *cp;

  dash = NULL;
  slash = NULL;
  for (cp = program_name; *cp != '\0'; ++cp)
    {
      if (*cp == '-')
	dash = cp;
      if (
#if defined(__DJGPP__) || defined (__CYGWIN__) || defined(__WIN32__)
	  *cp == ':' || *cp == '\\' ||
#endif
	  *cp == '/')
	{
	  slash = cp;
	  dash = NULL;
	}
    }

  cmd = NULL;

  if (dash != NULL)
    {
      /* First, try looking for a prefixed PROG_NAME in the
         PROGRAM_NAME directory, with the same prefix as PROGRAM_NAME.  */
      cmd = look_for_prog (prog_name, program_name, dash - program_name + 1);
    }

  if (slash != NULL && cmd == NULL)
    {
      /* Next, try looking for a PROG_NAME in the same directory as
         that of this program.  */
      cmd = look_for_prog (prog_name, program_name, slash - program_name + 1);
    }

  if (cmd == NULL)
    {
      /* Just return PROG_NAME as is.  */
      cmd = xstrdup (prog_name);
    }

  return cmd;
}

static void
delete_temp_files ()
{
  if (delete_base_file && base_file_name)
    {
      if (verbose)
	fprintf (stderr, "%s temporary base file %s\n",
		 dontdeltemps ? "Keeping" : "Deleting",
	         base_file_name);
      if (! dontdeltemps)
        {
          unlink (base_file_name);
	  free (base_file_name);
	}
    }
  
  if (delete_exp_file && exp_file_name)
    {
      if (verbose)
	fprintf (stderr, "%s temporary exp file %s\n",
		 dontdeltemps ? "Keeping" : "Deleting",
	         exp_file_name);
      if (! dontdeltemps)
        {
          unlink (exp_file_name);
          free (exp_file_name);
	}
    }
  if (delete_def_file && def_file_name)
    {
      if (verbose)
	fprintf (stderr, "%s temporary def file %s\n",
		 dontdeltemps ? "Keeping" : "Deleting",
	         def_file_name);
      if (! dontdeltemps)
        {
          unlink (def_file_name);
          free (def_file_name);
	}
    }
}

static void 
cleanup_and_exit (int status)
{
  delete_temp_files ();
  exit (status);
}
  
static int
run (what, args)
     const char *what;
     char *args;
{
  char *s;
  int pid, wait_status, retcode;
  int i;
  const char **argv;
  char *errmsg_fmt, *errmsg_arg;
  char *temp_base = choose_temp_base ();
  int in_quote;
  char sep;

  if (verbose || dry_run)
    fprintf (stderr, "%s %s\n", what, args);

  /* Count the args */
  i = 0;
  for (s = args; *s; s++)
    if (*s == ' ')
      i++;
  i++;
  argv = alloca (sizeof (char *) * (i + 3));
  i = 0;
  argv[i++] = what;
  s = args;
  while (1)
    {
      while (*s == ' ' && *s != 0)
	s++;
      if (*s == 0)
	break;
      in_quote = (*s == '\'' || *s == '"');
      sep = (in_quote) ? *s++ : ' ';
      argv[i++] = s;
      while (*s != sep && *s != 0)
	s++;
      if (*s == 0)
	break;
      *s++ = 0;
      if (in_quote)
        s++;
    }
  argv[i++] = NULL;

  if (dry_run)
    return 0;

  pid = pexecute (argv[0], (char * const *) argv, program_name, temp_base,
		  &errmsg_fmt, &errmsg_arg, PEXECUTE_ONE | PEXECUTE_SEARCH);

  if (pid == -1)
    {
      int errno_val = errno;

      fprintf (stderr, "%s: ", program_name);
      fprintf (stderr, errmsg_fmt, errmsg_arg);
      fprintf (stderr, ": %s\n", strerror (errno_val));
      return 1;
    }

  retcode = 0;
  pid = pwait (pid, &wait_status, 0);
  if (pid == -1)
    {
      fprintf (stderr, "%s: wait: %s\n", program_name, strerror (errno));
      retcode = 1;
    }
  else if (WIFSIGNALED (wait_status))
    {
      fprintf (stderr, "%s: subprocess got fatal signal %d\n",
	       program_name, WTERMSIG (wait_status));
      retcode = 1;
    }
  else if (WIFEXITED (wait_status))
    {
      if (WEXITSTATUS (wait_status) != 0)
	{
	  fprintf (stderr, "%s: %s exited with status %d\n",
	  	   program_name, what, WEXITSTATUS (wait_status));
	  retcode = 1;
	}
    }
  else
    retcode = 1;
  
  return retcode;
}

static char *
mybasename (name)
     const char *name;
{
  const char *base = name;

  while (*name)
    {
      if (*name == '/' || *name == '\\')
	{
	  base = name + 1;
	}
      ++name;
    }
  return (char *) base;
}

static int 
strhash (const char *str)
{
  const unsigned char *s;
  unsigned long hash;
  unsigned int c;
  unsigned int len;

  hash = 0;
  len = 0;
  s = (const unsigned char *) str;
  while ((c = *s++) != '\0')
    {
      hash += c + (c << 17);
      hash ^= hash >> 2;
      ++len;
    }
  hash += len + (len << 17);
  hash ^= hash >> 2;

  return hash;
}

/**********************************************************************/

void
print_version (name)
     const char *name;
{
  /* This output is intended to follow the GNU standards document.  */
  /* xgettext:c-format */
  printf ("GNU %s %s\n", name, program_version);
  printf ("Copyright 1998 Free Software Foundation, Inc.\n");
  printf ("\
This program is free software; you may redistribute it under the terms of\n\
the GNU General Public License.  This program has absolutely no warranty.\n");
  exit (0);
}

static void
usage (file, status)
     FILE *file;
     int status;
{
  fprintf (file, "Usage %s <options> <object-files>\n", program_name);
  fprintf (file, "  Generic options:\n");
  fprintf (file, "   --quiet, -q            Work quietly\n");
  fprintf (file, "   --verbose, -v          Verbose\n");
  fprintf (file, "   --version              Print dllwrap version\n");
  fprintf (file, "   --implib <outname>     Synonym for --output-lib\n");
  fprintf (file, "  Options for %s:\n", program_name);
  fprintf (file, "   --driver-name <driver> Defaults to \"gcc\"\n");
  fprintf (file, "   --driver-flags <flags> Override default ld flags\n");
  fprintf (file, "   --dlltool-name <dlltool> Defaults to \"dlltool\"\n");
  fprintf (file, "   --entry <entry>        Specify alternate DLL entry point\n");
  fprintf (file, "   --image-base <base>    Specify image base address\n");
  fprintf (file, "   --target <machine>     i386-cygwin32 or i386-mingw32\n");
  fprintf (file, "   --dry-run              Show what needs to be run\n");
  fprintf (file, "   --mno-cygwin           Create Mingw DLL\n");
  fprintf (file, "  Options passed to DLLTOOL:\n");
  fprintf (file, "   --machine <machine>\n");
  fprintf (file, "   --output-exp <outname> Generate export file.\n");
  fprintf (file, "   --output-lib <outname> Generate input library.\n");
  fprintf (file, "   --add-indirect         Add dll indirects to export file.\n");
  fprintf (file, "   --dllname <name>       Name of input dll to put into output lib.\n");
  fprintf (file, "   --def <deffile>        Name input .def file\n");
  fprintf (file, "   --output-def <deffile> Name output .def file\n");
  fprintf (file, "   --export-all-symbols     Export all symbols to .def\n");
  fprintf (file, "   --no-export-all-symbols  Only export .drectve symbols\n");
  fprintf (file, "   --exclude-symbols <list> Exclude <list> from .def\n");
  fprintf (file, "   --no-default-excludes    Zap default exclude symbols\n");
  fprintf (file, "   --base-file <basefile> Read linker generated base file\n");
  fprintf (file, "   --no-idata4           Don't generate idata$4 section\n");
  fprintf (file, "   --no-idata5           Don't generate idata$5 section\n");
  fprintf (file, "   -U                     Add underscores to .lib\n");
  fprintf (file, "   -k                     Kill @<n> from exported names\n");
  fprintf (file, "   --add-stdcall-alias    Add aliases without @<n>\n");
  fprintf (file, "   --as <name>            Use <name> for assembler\n");
  fprintf (file, "   --nodelete             Keep temp files.\n");
  fprintf (file, "  Rest are passed unmodified to the language driver\n");
  fprintf (file, "\n\n");
  exit (status);
}

#define OPTION_START		149

/* GENERIC options. */
#define OPTION_QUIET		(OPTION_START + 1)
#define OPTION_VERBOSE		(OPTION_QUIET + 1)
#define OPTION_VERSION		(OPTION_VERBOSE + 1)

/* DLLWRAP options. */
#define OPTION_DRY_RUN		(OPTION_VERSION + 1)
#define OPTION_DRIVER_NAME	(OPTION_DRY_RUN + 1)
#define OPTION_DRIVER_FLAGS	(OPTION_DRIVER_NAME + 1)
#define OPTION_DLLTOOL_NAME	(OPTION_DRIVER_FLAGS + 1)
#define OPTION_ENTRY		(OPTION_DLLTOOL_NAME + 1)
#define OPTION_IMAGE_BASE	(OPTION_ENTRY + 1)
#define OPTION_TARGET		(OPTION_IMAGE_BASE + 1)
#define OPTION_MNO_CYGWIN	(OPTION_TARGET + 1)

/* DLLTOOL options. */
#define OPTION_NODELETE		(OPTION_MNO_CYGWIN + 1)
#define OPTION_DLLNAME		(OPTION_NODELETE + 1)
#define OPTION_NO_IDATA4 	(OPTION_DLLNAME + 1)
#define OPTION_NO_IDATA5	(OPTION_NO_IDATA4 + 1)
#define OPTION_OUTPUT_EXP	(OPTION_NO_IDATA5 + 1)
#define OPTION_OUTPUT_DEF	(OPTION_OUTPUT_EXP + 1)
#define OPTION_EXPORT_ALL_SYMS	(OPTION_OUTPUT_DEF + 1)
#define OPTION_NO_EXPORT_ALL_SYMS (OPTION_EXPORT_ALL_SYMS + 1)
#define OPTION_EXCLUDE_SYMS	(OPTION_NO_EXPORT_ALL_SYMS + 1)
#define OPTION_NO_DEFAULT_EXCLUDES (OPTION_EXCLUDE_SYMS + 1)
#define OPTION_OUTPUT_LIB	(OPTION_NO_DEFAULT_EXCLUDES + 1)
#define OPTION_DEF		(OPTION_OUTPUT_LIB + 1)
#define OPTION_ADD_UNDERSCORE	(OPTION_DEF + 1)
#define OPTION_KILLAT		(OPTION_ADD_UNDERSCORE + 1)
#define OPTION_HELP		(OPTION_KILLAT + 1)
#define OPTION_MACHINE		(OPTION_HELP + 1)
#define OPTION_ADD_INDIRECT	(OPTION_MACHINE + 1)
#define OPTION_BASE_FILE	(OPTION_ADD_INDIRECT + 1)
#define OPTION_AS		(OPTION_BASE_FILE + 1)

static const struct option long_options[] =
{
  /* generic options. */
  {"quiet", no_argument, NULL, 'q'},
  {"verbose", no_argument, NULL, 'v'},
  {"version", no_argument, NULL, OPTION_VERSION},
  {"implib", required_argument, NULL, OPTION_OUTPUT_LIB},

  /* dllwrap options. */
  {"dry-run", no_argument, NULL, OPTION_DRY_RUN},
  {"driver-name", required_argument, NULL, OPTION_DRIVER_NAME},
  {"driver-flags", required_argument, NULL, OPTION_DRIVER_FLAGS},
  {"dlltool-name", required_argument, NULL, OPTION_DLLTOOL_NAME},
  {"entry", required_argument, NULL, 'e'},
  {"image-base", required_argument, NULL, OPTION_IMAGE_BASE},
  {"target", required_argument, NULL, OPTION_TARGET},

  /* dlltool options. */
  {"no-delete", no_argument, NULL, 'n'},
  {"dllname", required_argument, NULL, OPTION_DLLNAME},
  {"no-idata4", no_argument, NULL, OPTION_NO_IDATA4},
  {"no-idata5", no_argument, NULL, OPTION_NO_IDATA5},
  {"output-exp", required_argument, NULL, OPTION_OUTPUT_EXP},
  {"output-def", required_argument, NULL, OPTION_OUTPUT_DEF},
  {"export-all-symbols", no_argument, NULL, OPTION_EXPORT_ALL_SYMS},
  {"no-export-all-symbols", no_argument, NULL, OPTION_NO_EXPORT_ALL_SYMS},
  {"exclude-symbols", required_argument, NULL, OPTION_EXCLUDE_SYMS},
  {"no-default-excludes", no_argument, NULL, OPTION_NO_DEFAULT_EXCLUDES},
  {"output-lib", required_argument, NULL, OPTION_OUTPUT_LIB},
  {"def", required_argument, NULL, OPTION_DEF},
  {"add-underscore", no_argument, NULL, 'U'},
  {"killat", no_argument, NULL, 'k'},
  {"add-stdcall-alias", no_argument, NULL, 'A'},
  {"help", no_argument, NULL, 'h'},
  {"machine", required_argument, NULL, OPTION_MACHINE},
  {"add-indirect", no_argument, NULL, OPTION_ADD_INDIRECT},
  {"base-file", required_argument, NULL, OPTION_BASE_FILE},
  {"as", required_argument, NULL, OPTION_AS},
  {0}
};

int
main (argc, argv)
     int argc;
     char **argv;
{
  int c;
  int i;

  char **saved_argv = 0;
  int cmdline_len = 0;

  int export_all = 0;

  int *dlltool_arg_indices;
  int *driver_arg_indices;

  char *driver_flags = 0;
  char *output_lib_file_name = 0;

  dyn_string_t dlltool_cmdline;
  dyn_string_t driver_cmdline;

  int def_file_seen = 0;

  char *image_base_str = 0;

  program_name = argv[0];

  saved_argv = (char **) xmalloc (argc * sizeof (char*));
  dlltool_arg_indices = (int *) xmalloc (argc * sizeof (int));
  driver_arg_indices = (int *) xmalloc (argc * sizeof (int));
  for (i = 0; i < argc; ++i) 
    {
      size_t len = strlen (argv[i]);
      char *arg = (char *) xmalloc (len + 1);
      strcpy (arg, argv[i]);
      cmdline_len += len;
      saved_argv[i] = arg;
      dlltool_arg_indices[i] = 0;
      driver_arg_indices[i] = 1;
    }
  cmdline_len++;

  /* We recognize dllwrap and dlltool options, and everything else is
     passed onto the language driver (eg., to GCC). We collect options
     to dlltool and driver in dlltool_args and driver_args. */
   
  opterr = 0;
  while ((c = getopt_long_only (argc, argv, "nkAqve:Uho:l:L:I:", 
                                long_options, (int *) 0)) != EOF)
    {
      int dlltool_arg;
      int driver_arg;
      int single_word_option_value_pair;

      dlltool_arg = 0;
      driver_arg = 1;
      single_word_option_value_pair = 0;

      if (c != '?')
        {
	  /* We recognize this option, so it has to be either dllwrap or
	     dlltool option. Do not pass to driver unless it's one of the
	     generic options that are passed to all the tools (such as -v)
	     which are dealt with later. */
	  driver_arg = 0;
	}

      /* deal with generic and dllwrap options first. */
      switch (c)
	{
	case 'h':
	  usage (stdout, 0);
	  break;
	case 'q':
	  verbose = 0;
	  break;
	case 'v':
	  verbose = 1;
	  break;
	case OPTION_VERSION:
	  print_version (program_name);
	  break;
	case 'e':
	  entry_point = optarg;
	  break;
	case OPTION_IMAGE_BASE:
	  image_base_str = optarg;
	  break;
	case OPTION_DEF:
	  def_file_name = optarg;
	  def_file_seen = 1;
	  delete_def_file = 0;
	  break;
	case 'n':
	  dontdeltemps = 1;
	  dlltool_arg = 1;
	  break;
	case 'o':
	  dll_file_name = optarg;
	  break;
	case 'I':
	case 'l':
	case 'L':
	  driver_arg = 1;
	  break;
	case OPTION_DLLNAME:
	  dll_name = optarg;
	  break;
	case OPTION_DRY_RUN:
	  dry_run = 1;
	  break;
	case OPTION_DRIVER_NAME:
	  driver_name = optarg;
	  break;
	case OPTION_DRIVER_FLAGS:
	  driver_flags = optarg;
	  break;
	case OPTION_DLLTOOL_NAME:
	  dlltool_name = optarg;
	  break;
	case OPTION_TARGET:
	  target = optarg;
	  break;
	case OPTION_MNO_CYGWIN:
	  target = "i386-mingw32";
	  break;
	case OPTION_BASE_FILE:
	  base_file_name = optarg;
	  delete_base_file = 0;
	  break;
	case OPTION_OUTPUT_EXP:
	  exp_file_name = optarg;
	  delete_exp_file = 0;
	  break;
	case OPTION_EXPORT_ALL_SYMS:
	  export_all = 1;
	  break;
	case OPTION_OUTPUT_LIB:
	  output_lib_file_name = optarg;
	  break;
	case '?':
	  break;
	default:
	  dlltool_arg = 1;
	  break;
	}
      
      /* Handle passing through --option=value case. */
      if (optarg 
          && saved_argv[optind-1][0] == '-' 
          && saved_argv[optind-1][1] == '-' 
	  && strchr (saved_argv[optind-1], '='))
	single_word_option_value_pair = 1;

      if (dlltool_arg)
        {
	  dlltool_arg_indices[optind-1] = 1;
	  if (optarg && ! single_word_option_value_pair)
	    {
	      dlltool_arg_indices[optind-2] = 1;
	    } 
	}

      if (! driver_arg)
        {
	  driver_arg_indices[optind-1] = 0;
	  if (optarg && ! single_word_option_value_pair)
	    {
	      driver_arg_indices[optind-2] = 0;
	    } 
	}
    }

  /* sanity checks. */
  if (! dll_name && ! dll_file_name)
    {
      fprintf (stderr,
               "%s: Must provide at least one of -o or --dllname options\n",
               program_name);
      exit (1);
    }
  else if (! dll_name)
    {
      dll_name = xstrdup (mybasename (dll_file_name));
    }
  else if (! dll_file_name)
    {
      dll_file_name = xstrdup (dll_name);
    }

  /* Deduce driver-name and dlltool-name from our own. */
  if (driver_name == NULL)
    driver_name = deduce_name ("gcc");

  if (dlltool_name == NULL)
    dlltool_name = deduce_name ("dlltool");

  if (! def_file_seen)
    {
      char *fileprefix = choose_temp_base ();
      def_file_name = (char *) xmalloc (strlen (fileprefix) + 5);
      sprintf (def_file_name, "%s.def", 
               (dontdeltemps) ? mybasename (fileprefix) : fileprefix);
      delete_def_file = 1;
      free (fileprefix);
      delete_def_file = 1;
      fprintf (stderr, "Warning: no export definition file provided\n");
      fprintf (stderr, 
               "dllwrap will create one, but may not be what you want\n");
    }
  
  /* set the target platform. */
  if (strstr (target, "cygwin"))
    which_target = CYGWIN_TARGET;
  else if (strstr (target, "mingw"))
    which_target = MINGW_TARGET;
  else 
    which_target = UNKNOWN_TARGET;

  /* re-create the command lines as a string, taking care to quote stuff. */
  dlltool_cmdline = dyn_string_new (cmdline_len);
  if (verbose)
    {
      dyn_string_append (dlltool_cmdline, " -v");
    }
  dyn_string_append (dlltool_cmdline, " --dllname ");
  dyn_string_append (dlltool_cmdline, dll_name);

  for (i = 1; i < argc; ++i)
    {
      if (dlltool_arg_indices[i])
        {
	  char *arg = saved_argv[i];
          int quote = (strchr (arg, ' ') || strchr (arg, '\t'));
	  dyn_string_append (dlltool_cmdline, 
	                     (quote) ? " \"" : " ");
	  dyn_string_append (dlltool_cmdline, arg);
	  dyn_string_append (dlltool_cmdline, 
	                     (quote) ? "\"" : "");
	}
    }

  driver_cmdline = dyn_string_new (cmdline_len);
  if (! driver_flags || strlen (driver_flags) == 0)
    {
      switch (which_target)
        {
	case CYGWIN_TARGET:
          driver_flags = cygwin_driver_flags;
	  break;
	
	case MINGW_TARGET:
          driver_flags = mingw32_driver_flags;
	  break;
	
	default:
          driver_flags = generic_driver_flags;
	  break;
	}
    }
  dyn_string_append (driver_cmdline, driver_flags);
  dyn_string_append (driver_cmdline, " -o ");
  dyn_string_append (driver_cmdline, dll_file_name);

  if (! entry_point || strlen (entry_point) == 0)
    {
      switch (which_target)
        {
	case CYGWIN_TARGET:
	  entry_point = "__cygwin_dll_entry@12";
	  break;
	
	case MINGW_TARGET:
	  entry_point = "_DllMainCRTStartup@12";
	  break;
	
	default:
          entry_point = "_DllMain@12";
	  break;
	}
    }
  dyn_string_append (driver_cmdline, " -Wl,-e,");
  dyn_string_append (driver_cmdline, entry_point);
  dyn_string_append (dlltool_cmdline, " --exclude-symbol=");
  dyn_string_append (dlltool_cmdline, 
                     (entry_point[0] == '_') ? entry_point+1 : entry_point);

  if (! image_base_str || strlen (image_base_str) == 0)
    {
      char *tmpbuf = (char *) xmalloc (sizeof ("0x12345678") + 1);
      unsigned long hash = strhash (dll_file_name);
      sprintf (tmpbuf, "0x%.8lX", 0x60000000|((hash<<16)&0xFFC0000));
      image_base_str = tmpbuf;
    }

  dyn_string_append (driver_cmdline, " -Wl,--image-base,");
  dyn_string_append (driver_cmdline, image_base_str);

  if (verbose)
    {
      dyn_string_append (driver_cmdline, " -v");
    }

  for (i = 1; i < argc; ++i)
    {
      if (driver_arg_indices[i])
        {
	  char *arg = saved_argv[i];
          int quote = (strchr (arg, ' ') || strchr (arg, '\t'));
	  dyn_string_append (driver_cmdline, 
	                     (quote) ? " \"" : " ");
	  dyn_string_append (driver_cmdline, arg);
	  dyn_string_append (driver_cmdline, 
	                     (quote) ? "\"" : "");
	}
    }
  
  /*
   * Step pre-1. If no --def <EXPORT_DEF> is specified, then create it
   * and then pass it on.
   */
  
  if (! def_file_seen) 
    {
      int i;
      dyn_string_t step_pre1;

      step_pre1 = dyn_string_new (1024);

      dyn_string_append (step_pre1, dlltool_cmdline->s);
      if (export_all)
	{
          dyn_string_append (step_pre1, " --export-all --exclude-symbol=");
          dyn_string_append (step_pre1, 
	  "_cygwin_dll_entry@12,DllMainCRTStartup@12,DllMain@12,DllEntryPoint@12");
	}
      dyn_string_append (step_pre1, " --output-def ");
      dyn_string_append (step_pre1, def_file_name);

      for (i = 1; i < argc; ++i)
	{
	  if (driver_arg_indices[i])
	    {
	      char *arg = saved_argv[i];
	      size_t len = strlen (arg);
	      if (len >= 2 && arg[len-2] == '.' 
	          && (arg[len-1] == 'o' || arg[len-1] == 'a'))
		{
		  int quote = (strchr (arg, ' ') || strchr (arg, '\t'));
		  dyn_string_append (step_pre1,
				     (quote) ? " \"" : " ");
		  dyn_string_append (step_pre1, arg);
		  dyn_string_append (step_pre1,
				     (quote) ? "\"" : "");
		}
	    }
	}

      if (run (dlltool_name, step_pre1->s))
	cleanup_and_exit (1);
      
      dyn_string_delete (step_pre1);
    }

  dyn_string_append (dlltool_cmdline, " --def ");
  dyn_string_append (dlltool_cmdline, def_file_name);

  if (verbose)
    {
      fprintf (stderr, "DLLTOOL name    : %s\n", dlltool_name);
      fprintf (stderr, "DLLTOOL options : %s\n", dlltool_cmdline->s);
      fprintf (stderr, "DRIVER name     : %s\n", driver_name);
      fprintf (stderr, "DRIVER options  : %s\n", driver_cmdline->s);
    }
 
  /*
   * Step 1. Call GCC/LD to create base relocation file. If using GCC, the
   * driver command line will look like the following:
   *    
   *    % gcc -Wl,--dll --Wl,--base-file,foo.base [rest of command line]
   *
   * If the user does not specify a base name, create temporary one that
   * is deleted at exit.
   *
   */
  
  if (! base_file_name)
    {
      char *fileprefix = choose_temp_base ();
      base_file_name = (char *) xmalloc (strlen (fileprefix) + 6);
      sprintf (base_file_name, "%s.base", 
               (dontdeltemps) ? mybasename (fileprefix) : fileprefix);
      delete_base_file = 1;
      free (fileprefix);
    }
  
  {
    int quote;

    dyn_string_t step1 = dyn_string_new (driver_cmdline->length 
                                         + strlen (base_file_name)
				         + 20);
    dyn_string_append (step1, "-Wl,--base-file,");
    quote = (strchr (base_file_name, ' ') 
             || strchr (base_file_name, '\t'));
    dyn_string_append (step1, 
	               (quote) ? "\"" : "");
    dyn_string_append (step1, base_file_name);
    dyn_string_append (step1, 
	               (quote) ? "\"" : "");
    if (driver_cmdline->length)
      {
        dyn_string_append (step1, " ");
        dyn_string_append (step1, driver_cmdline->s);
      }

    if (run (driver_name, step1->s))
      cleanup_and_exit (1);
    
    dyn_string_delete (step1);
  }



  /*
   * Step 2. generate the exp file by running dlltool. 
   * dlltool command line will look like the following:
   *    
   *    % dlltool -Wl,--dll --Wl,--base-file,foo.base [rest of command line]
   *
   * If the user does not specify a base name, create temporary one that
   * is deleted at exit.
   *
   */
  
  if (! exp_file_name)
    {
      char *p = strrchr (dll_name, '.');
      size_t prefix_len = (p) ? p - dll_name : strlen (dll_name);
      exp_file_name = (char *) xmalloc (prefix_len + 4 + 1);
      strncpy (exp_file_name, dll_name, prefix_len);
      exp_file_name[prefix_len] = '\0';
      strcat (exp_file_name, ".exp");
      delete_exp_file = 1;
    }
  
  {
    int quote;
    dyn_string_t step2 = dyn_string_new (dlltool_cmdline->length 
                                         + strlen (base_file_name)
                                         + strlen (exp_file_name)
				         + 20);

    dyn_string_append (step2, "--base-file ");
    quote = (strchr (base_file_name, ' ') 
             || strchr (base_file_name, '\t'));
    dyn_string_append (step2, 
	               (quote) ? "\"" : "");
    dyn_string_append (step2, base_file_name);
    dyn_string_append (step2, 
	               (quote) ? "\" " : " ");

    dyn_string_append (step2, "--output-exp ");
    quote = (strchr (exp_file_name, ' ') 
             || strchr (exp_file_name, '\t'));
    dyn_string_append (step2, 
	               (quote) ? "\"" : "");
    dyn_string_append (step2, exp_file_name);
    dyn_string_append (step2, 
	               (quote) ? "\"" : "");

    if (dlltool_cmdline->length)
      {
        dyn_string_append (step2, " ");
        dyn_string_append (step2, dlltool_cmdline->s);
      }

    if (run (dlltool_name, step2->s))
      cleanup_and_exit (1);
    
    dyn_string_delete (step2);
  }

  /*
   * Step 3. Call GCC/LD to again, adding the exp file this time.
   * driver command line will look like the following:
   *    
   *    % gcc -Wl,--dll --Wl,--base-file,foo.base foo.exp [rest ...]
   */

  {
    int quote;

    dyn_string_t step3 = dyn_string_new (driver_cmdline->length 
                                         + strlen (exp_file_name)
                                         + strlen (base_file_name)
				         + 20);
    dyn_string_append (step3, "-Wl,--base-file,");
    quote = (strchr (base_file_name, ' ') 
             || strchr (base_file_name, '\t'));
    dyn_string_append (step3, 
	               (quote) ? "\"" : "");
    dyn_string_append (step3, base_file_name);
    dyn_string_append (step3, 
	               (quote) ? "\" " : " ");

    quote = (strchr (exp_file_name, ' ') 
             || strchr (exp_file_name, '\t'));
    dyn_string_append (step3, 
	               (quote) ? "\"" : "");
    dyn_string_append (step3, exp_file_name);
    dyn_string_append (step3, 
	               (quote) ? "\"" : "");

    if (driver_cmdline->length)
      {
        dyn_string_append (step3, " ");
        dyn_string_append (step3, driver_cmdline->s);
      }

    if (run (driver_name, step3->s))
      cleanup_and_exit (1);
    
    dyn_string_delete (step3);
  }


  /*
   * Step 4. Run DLLTOOL again using the same command line.
   */

  {
    int quote;
    dyn_string_t step4 = dyn_string_new (dlltool_cmdline->length 
                                         + strlen (base_file_name)
                                         + strlen (exp_file_name)
				         + 20);

    dyn_string_append (step4, "--base-file ");
    quote = (strchr (base_file_name, ' ') 
             || strchr (base_file_name, '\t'));
    dyn_string_append (step4, 
	               (quote) ? "\"" : "");
    dyn_string_append (step4, base_file_name);
    dyn_string_append (step4, 
	               (quote) ? "\" " : " ");

    dyn_string_append (step4, "--output-exp ");
    quote = (strchr (exp_file_name, ' ') 
             || strchr (exp_file_name, '\t'));
    dyn_string_append (step4, 
	               (quote) ? "\"" : "");
    dyn_string_append (step4, exp_file_name);
    dyn_string_append (step4, 
	               (quote) ? "\"" : "");

    if (dlltool_cmdline->length)
      {
        dyn_string_append (step4, " ");
        dyn_string_append (step4, dlltool_cmdline->s);
      }

    if (output_lib_file_name)
      {
        dyn_string_append (step4, " --output-lib ");
        dyn_string_append (step4, output_lib_file_name);
      }

    if (run (dlltool_name, step4->s))
      cleanup_and_exit (1);
    
    dyn_string_delete (step4);
  }
  

  /*
   * Step 5. Link it all together and be done with it.
   * driver command line will look like the following:
   *    
   *    % gcc -Wl,--dll foo.exp [rest ...]
   *
   */

  {
    int quote;

    dyn_string_t step5 = dyn_string_new (driver_cmdline->length 
                                         + strlen (exp_file_name)
				         + 20);
    quote = (strchr (exp_file_name, ' ') 
             || strchr (exp_file_name, '\t'));
    dyn_string_append (step5, 
	               (quote) ? "\"" : "");
    dyn_string_append (step5, exp_file_name);
    dyn_string_append (step5, 
	               (quote) ? "\"" : "");

    if (driver_cmdline->length)
      {
        dyn_string_append (step5, " ");
        dyn_string_append (step5, driver_cmdline->s);
      }

    if (run (driver_name, step5->s))
      cleanup_and_exit (1);
    
    dyn_string_delete (step5);
  }

  cleanup_and_exit (0);

  return 0;
}
