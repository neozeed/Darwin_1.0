/*
 * dpkg-deb - construction and deconstruction of *.deb archives
 * main.c - main program
 *
 * Copyright (C) 1994,1995 Ian Jackson <iwj10@cus.cam.ac.uk>
 * Copyright (C) 1997, 1998 Klee Dienes <klee@mit.edu>
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2,
 * or (at your option) any later version.
 *
 * This is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with dpkg; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <unistd.h>
#include <dirent.h>
#include <limits.h>
#include <ctype.h>
#include <assert.h>

#include <dpkg.h>
#include <dpkg-var.h>
#include <myopt.h>

#include "dpkg-deb.h"
#include "config.h"
#include "version.h"

static void setaction (const struct cmdinfo *cip, const char *value);
static void helponly (const struct cmdinfo *cip, const char *value);
static void versiononly (const struct cmdinfo *cip, const char *value);

static dofunction *const dofunctions[]= {
  do_build,
  do_contents,
  do_control,
  do_info,
  do_field,
  do_extract,
  do_vextract,
  do_fsystarfile
};
static int ndofunctions = sizeof (dofunctions) / sizeof (dofunction *);

/* NB: the entries using setaction must appear first and be in the
 * same order as dofunctions: */

static const struct cmdinfo cmdinfos[]= {
  { "build",        'b',  0,  0, 0,               setaction        },
  { "contents",     'c',  0,  0, 0,               setaction        },
  { "control",      'e',  0,  0, 0,               setaction        },
  { "info",         'I',  0,  0, 0,               setaction        },
  { "field",        'f',  0,  0, 0,               setaction        },
  { "extract",      'x',  0,  0, 0,               setaction        },
  { "vextract",     'X',  0,  0, 0,               setaction        },
  { "fsys-tarfile",  0,   0,  0, 0,               setaction        },
  { "new",           0,   0,  &oldformatflag, 0,  0,            0  },
  { "old",           0,   0,  &oldformatflag, 0,  0,            1  },
  { "debug",        'D',  0,  &debugflag,     0,  0,            1  },
  { "nocheck",       0,   0,  &nocheckflag,   0,  0,            1  },
  { "help",         'h',  0,  0, 0,               helponly         },
  { "version",       0,   0,  0, 0,               versiononly      },
  { "licence",       0,   0,  0, 0,               showcopyright    }, /* UK spelling */
  { "license",       0,   0,  0, 0,               showcopyright    }, /* US spelling */
  {  0,              0                                             }
};

const char *thisname = NULL;
const char *printforhelp =
N_("Type dpkg-deb --help for help about manipulating *.deb files;\n"
   "Type dpkg --help for help about installing and deinstalling packages.");

int debugflag = 0;
int nocheckflag = 0;
int oldformatflag = BUILDOLDPKGFORMAT;
const struct cmdinfo *cipaction = 0;
dofunction *action = 0;

static void printversion (void)
{
  int ret = 
    fprintf
    (stdout,
     _("Debian GNU/Linux `%s' package archive backend version %s.\n"
       "Copyright (C) 1994-1996 Ian Jackson.  This is free software; see the\n"
       "GNU General Public Licence version 2 or later for copying conditions.\n"
       "There is NO warranty.  See `%s --licence' for details.\n"),
     BACKEND, DPKG_VERSION_ARCH, BACKEND);

  if (ret < 0) { 
    werr ("stdout");
  }
}

static void usage (void)
{
  int ret = fprintf
    (stdout, 
     _("Usage: %s -b|--build <directory> [<deb>]    Build an archive.\n"
       "       %s -c|--contents <deb>               List contents.\n"
       "       %s -I|--info <deb> [<cfile>...]      Show info to stdout.\n"
       "       %s -f|--field <deb> [<cfield>...]    Show field(s) to stdout.\n"
       "       %s -e|--control <deb> [<directory>]  Extract control info.\n"
       "       %s -x|--extract <deb> <directory>    Extract files.\n"
       "       %s -X|--vextract <deb> <directory>   Extract & list files.\n"
       "       %s --fsys-tarfile <deb>              Output filesystem tarfile.\n"
       "       %s -h|--help                         Display this message.\n"
       "       %s --version | --licence             Show version/licence.\n"
       "\n"
       "<deb> is the filename of a Debian format archive.\n"
       "<cfile> is the name of an administrative file component.\n"
       "<cfield> is the name of a field in the main `control' file.\n"
       "Options:  -D for debugging output; --old or --new controls archive format;\n"
       "--no-check to suppress control file check (build bad package).\n"
       "\n"
       "Use `dpkg' to install and remove packages from your system, or\n"
       "`dselect' for user-friendly package management.  Packages unpacked\n"
       "using `%s --extract' will be incorrectly installed!\n"),
     BACKEND, BACKEND, BACKEND, BACKEND, BACKEND, BACKEND,
     BACKEND, BACKEND, BACKEND, BACKEND, BACKEND);

  if (ret < 0) {
    werr ("stdout");
  }
}

static void setaction (const struct cmdinfo *cip, const char *value) 
{
  if (cipaction) {
    badusage (_("conflicting actions --%s and --%s"),
	      cip->olong,
	      cipaction->olong);
  }
  cipaction = cip;
  assert ((cip - cmdinfos) < ndofunctions);
  action = dofunctions[cip - cmdinfos];
}

static void helponly (const struct cmdinfo *cip, const char *value)
{
  usage (); 
  exit (EXIT_SUCCESS);
}

static void versiononly (const struct cmdinfo *cip, const char *value) 
{
  printversion ();
  exit (EXIT_SUCCESS);
}

static void print_error_fatal (const char *emsg, const char *contextstring) 
{
  fprintf (stderr, "%s: %s\n", thisname, emsg);
}

int main (int argc, const char *const *argv) 
{
  jmp_buf ejbuf;

  setlocale (LC_ALL, "");
  setlocale (LC_NUMERIC, "POSIX");
  bindtextdomain ("dpkgdeb", LOCALEDIR);
  textdomain ("dpkgdeb");

  thisname = BACKEND;

  /* expect warning about possible clobbering of argv */
  if (setjmp (ejbuf)) {
    error_unwind (ehflag_bombout);
    exit (2);
  }
  push_error_handler (&ejbuf, print_error_fatal, 0);

  myopt (&argv, cmdinfos);

  if (! cipaction) {
    badusage (_("need an action option"));
  }

  unsetenv ("GZIP");
  action (argv);
  set_error_display (0,0);
  error_unwind (ehflag_normaltidy);
  exit (EXIT_SUCCESS);
}
