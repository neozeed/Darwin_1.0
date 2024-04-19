/*
 * dpkg-split - splitting and joining of multipart *.deb archives
 * main.c - main program
 *
 * Copyright (C) 1995 Ian Jackson <iwj10@cus.cam.ac.uk>
 * Copyright (C) 1997 Klee Dienes <klee@debian.org>
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

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <assert.h>

#include <dpkg.h>
#include <dpkg-db.h>
#include <dpkg-var.h>
#include <myopt.h>

#include "config.h"
#include "version.h"

#include "dpkg-split.h"

static void printversion (void)
{
  if (fprintf 
      (stdout,
       _("Debian Linux `%s' package split/join tool; version %s.\n"
	 "Copyright (C) 1994-1996 Ian Jackson.  This is free software; see the\n"
	 "GNU General Public Licence version 2 or later for copying conditions.\n"
	 "There is NO warranty.  See `%s --licence' for details.\n"),
       SPLITTER, DPKG_VERSION_ARCH, SPLITTER) < 0) {
    werr ("stdout");
  }
}

static void usage (void) 
{
  if (fprintf
      (stdout, 
       _("Usage: %s -s|--split <file> [<prefix>]     Split an archive.\n"
	 "       %s -j|--join <part> <part> ...      Join parts together.\n"
	 "       %s -I|--info <part> ...             Display info about a part.\n"
	 "       %s -h|--help|--version|--licence    Show help/version/licence.\n"
	 "\n"
	 "       %s -a|--auto -o <complete> <part>   Auto-accumulate parts.\n"
	 "       %s -l|--listq                       List unmatched pieces.\n"
	 "       %s -d|--discard [<filename> ...]    Discard unmatched pieces.\n"
	 "\n"
	 "Options:  --depotdir <directory>  (default is %s/%s)\n"
	 "          -S|--partsize <size>    (in Kb, for -s, default is 450)\n"
	 "          -o|--output <file>      (for -j, default is <package>-<version>.deb)\n"
	 "          -Q|--npquiet            (be quiet when -a is not a part)\n"
	 "          --msdos                 (generate 8.3 filenames)\n"
	 "\n"
	 "Exit status: 0 = OK; 1 = -a is not a part; 2 = trouble!\n"),
       SPLITTER, SPLITTER, SPLITTER, SPLITTER,
       SPLITTER, SPLITTER, SPLITTER, 
       ADMINDIR, PARTSDIR) < 0) {
    werr ("stdout");
  }
}

const char *thisname = NULL;
const char *printforhelp = N_("Type dpkg-split --help for help.");

dofunction *action=0;
const struct cmdinfo *cipaction=0;
long maxpartsize= SPLITPARTDEFMAX;
const char *depotdir = 0;
const char *outputfile= 0;
struct partqueue *queue= 0;
int npquiet= 0, msdos= 0;

void rerr(const char *fn) {
  ohshite(_("error reading %s"),fn);
}

void rerreof(FILE *f, const char *fn) {
  if (ferror(f)) ohshite(_("error reading %.250s"),fn);
  ohshit(_("unexpected end of file in %.250s"),fn);
}

static void helponly(const struct cmdinfo *cip, const char *value) {
  usage(); exit(0);
}
static void versiononly(const struct cmdinfo *cip, const char *value) {
  printversion(); exit(0);
}

static void setaction(const struct cmdinfo *cip, const char *value);

static void setpartsize(const struct cmdinfo *cip, const char *value) {
  long newpartsize;
  char *endp;

  newpartsize= strtol(value,&endp,10);
  if (newpartsize <= 0 || newpartsize > (INT_MAX >> 10))
    badusage(_("part size is far too large or is not positive"));

  maxpartsize= newpartsize << 10;
  if (maxpartsize <= HEADERALLOWANCE)
    badusage(_("part size must be at least %dk (to allow for header)"),
             (HEADERALLOWANCE >> 10) + 1);
}

static dofunction *const dofunctions[]= {
  do_split,
  do_join,
  do_info,
  do_auto,
  do_queue,
  do_discard,
};

/* NB: the entries using setaction must appear first and be in the
 * same order as dofunctions:
 */
static const struct cmdinfo cmdinfos[]= {
  { "split",        's',  0,  0, 0,             setaction           },
  { "join",         'j',  0,  0, 0,             setaction           },
  { "info",         'I',  0,  0, 0,             setaction           },
  { "auto",         'a',  0,  0, 0,             setaction           },
  { "listq",        'l',  0,  0, 0,             setaction           },
  { "discard",      'd',  0,  0, 0,             setaction           },
  { "help",         'h',  0,  0, 0,             helponly            },
  { "version",       0,   0,  0, 0,             versiononly         },
  { "licence",       0,   0,  0, 0,             showcopyright       }, /* UK spelling */
  { "license",       0,   0,  0, 0,             showcopyright       }, /* US spelling */
  { "depotdir",      0,   1,  0, &depotdir,     0                   },
  { "partsize",     'S',  1,  0, 0,             setpartsize         },
  { "output",       'o',  1,  0, &outputfile,   0                   },
  { "npquiet",      'Q',  0,  &npquiet, 0,      0,              1   },
  { "msdos",         0,   0,  &msdos, 0,        0,              1   },
  {  0,              0                                              }
};

static void setaction(const struct cmdinfo *cip, const char *value) {
  if (cipaction)
    badusage(_("conflicting actions --%s and --%s"),cip->olong,cipaction->olong);
  cipaction= cip;
  assert(cip-cmdinfos < sizeof(dofunctions)*sizeof(dofunction*));
  action= dofunctions[cip-cmdinfos];
}

static void print_error_fatal (const char *emsg, const char *contextstring) 
{
  fprintf (stderr, "%s: %s\n", thisname, emsg);
}

int main(int argc, const char *const *argv) {
  jmp_buf ejbuf;
  int l;
  char *p;

  setlocale (LC_ALL, "");
  bindtextdomain ("dpkgutil", LOCALEDIR);
  textdomain (PACKAGE);

  thisname = SPLITTER;
 
  if (setjmp(ejbuf)) { /* expect warning about possible clobbering of argv */
    error_unwind(ehflag_bombout); exit(2);
  }
  push_error_handler(&ejbuf,print_error_fatal,0);

  depotdir = (char *) m_malloc (strlen (ADMINDIR) + strlen (PARTSDIR) + 2);
  sprintf (depotdir, "%s/%s", ADMINDIR, PARTSDIR);
  
  myopt(&argv,cmdinfos);
  if (!cipaction) badusage(_("need an action option"));

  l= strlen(depotdir);
  if (l && depotdir[l-1] != '/') {
    p= nfmalloc(l+2);
    strcpy(p,depotdir);
    strcpy(p+l,"/");
    depotdir= p;
  }

  setvbuf(stdout,0,_IONBF,0);
  action(argv);

  if (ferror(stderr)) werr("stderr");
  
  set_error_display(0,0);
  error_unwind(ehflag_normaltidy);
  exit(0);
}
