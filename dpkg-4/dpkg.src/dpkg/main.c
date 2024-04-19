/*
 * dpkg - main program for package management
 * main.c - main program
 *
 * Copyright (C) 1994, 1995 Ian Jackson <iwj10@cus.cam.ac.uk>
 * Copyright (C) 1997, 1998 Klee Dienes <klee@debian.org>
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

#include <dpkg.h>
#include <dpkg-db.h>
#include <dpkg-var.h>
#include <myopt.h>

#include "main.h"
#include "dpkg-int.h"
#include "config.h"
#include "version.h"

static void printversion (void)
{
  if (fprintf 
      (stdout,
       _("Debian GNU/Linux `%s' package management program version %s.\n"
	 "Copyright 1994-1996 Ian Jackson, Bruce Perens.  This is free software;\n"
	 "see the GNU general Public Licence version 2 or later for copying\n"
	 "conditions.  There is NO warranty.  See `%s --licence' for details.\n"),
       DPKG, DPKG_VERSION_ARCH, DPKG) < 0) {
    werr ("stdout");
  }
}

static void usage (void) 
{
  int ret;

  ret = fprintf 
    (stdout,
     _("Usage: \n"
       "  %s -i|--install      <.deb file name> ... | -R|--recursive <dir> ...\n"
       "  %s --unpack          <.deb file name> ... | -R|--recursive <dir> ...\n"
       "  %s -A|--record-avail <.deb file name> ... | -R|--recursive <dir> ...\n"
       "  %s --configure           <package name> ... | -a|--pending\n"
       "  %s -r|--remove | --purge <package name> ... | -a|--pending\n"
       "  %s --get-selections [<pattern> ...]   get list of selections to stdout\n"
       "  %s --set-selections                   set package selections from stdin\n"
       "  %s --update-avail <Packages-file>     replace available packages info\n"
       "  %s --merge-avail <Packages-file>      merge with info from file\n"
       "  %s --clear-avail                      erase existing available info\n"
       "  %s --forget-old-unavail               forget uninstalled unavailable pkgs\n"
       "  %s -s|--status <package-name> ...     display package status details\n"
       "  %s --print-avail <package-name> ...   display available version details\n"
       "  %s -L|--listfiles <package-name> ...  list files `owned' by package(s)\n"
       "  %s -l|--list [<pattern> ...]          list packages concisely\n"
       "  %s -S|--search <pattern> ...          find package(s) owning file(s)\n"
       "  %s -C|--audit                         check for broken package(s)\n"
       "  %s --print-architecture               print target architecture (uses GCC)\n"
       "  %s --print-installation-architecture  print host architecture (for inst'n)\n"
       "  %s --print-gnu-build-architecture     print GNU version of target arch\n"
       "  %s --print-gnu-installation-architecture    print GNU version of intall arch\n"
       "  %s --print-gnu-target-architecture    print target architecture (for inst'n)\n"
       "  %s --compare-versions <a> <rel> <b>   compare version numbers - see below\n"
       "  %s --help | --version                 show this help / version number\n"
       "  %s --force-help | -Dh|--debug=help    help on forcing resp. debugging\n"
       "  %s --licence                          print copyright licencing terms\n"
       "\n"
       "Use %s -b|--build|-c|--contents|-e|--control|-I|--info|-f|--field|\n"
       " -x|--extract|-X|--vextract|--fsys-tarfile  on archives (type %s --help.)\n"
       "\n"
       "For internal use: %s --assert-support-predepends | --predep-package |\n"
       "  --assert-working-epoch\n"
       "\n"
       "Options:\n"
       "  --admindir=<directory>     Use <directory> instead of %s\n"
       "  --root=<directory>         Install on alternative system rooted elsewhere\n"
       "  --instdir=<directory>      Change inst'n root without changing admin dir\n"
       "  -O|--selected-only         Skip packages not selected for install/upgrade\n"
       "  -E|--skip-same-version     Skip packages whose same version is installed\n"
       "  -G=--refuse-downgrade      Skip packages with earlier version than installed\n"
       "  -B|--auto-deconfigure      Install even if it would break some other package\n"
       "  --largemem | --smallmem    Optimise for large (>4Mb) or small (<4Mb) RAM use\n"
       "  --no-act                   Just say what we would do - don't do it\n"
       "  -D|--debug=<octal>         Enable debugging - see -Dhelp or --debug=help\n"
       "  --ignore-depends=<package>,... Ignore dependencies involving <package>\n"
       "  --force-...                    Override problems - see --force-help\n"
       "  --no-force-...|--refuse-...    Stop when problems encountered\n"
       "\n"
       "Comparison operators for --compare-versions are:\n"
       " lt le eq ne ge gt       (treat no version as earlier than any version);\n"
       " lt-nl le-nl ge-nl gt-nl (treat no version as later than any version);\n"
       " < << <= = >= >> >       (only for compatibility with control file syntax).\n"
       "\n"
       "Use `%s' for user-friendly package management.\n"),
     DPKG, DPKG, DPKG, DPKG, DPKG, DPKG, DPKG, DPKG, DPKG, DPKG, DPKG, DPKG, 
     DPKG, DPKG, DPKG, DPKG, DPKG, DPKG, DPKG, DPKG, DPKG, DPKG, DPKG, DPKG, DPKG, DPKG,
     BACKEND, ADMINDIR, DSELECT);
  if (ret < 0) {
    werr ("stdout");
  }
}

const char *thisname = NULL;

const char *const printforhelp = N_("\
Type dpkg --help for help about installing and deinstalling packages [*];\n\
Use dselect for user-friendly package management;\n\
Type dpkg -Dhelp for a list of dpkg debug flag values;\n\
Type dpkg --force-help for a list of forcing options;\n\
Type dpkg-deb --help for help about manipulating *.deb files;\n\
Type dpkg --licence for copyright licence and lack of warranty (GNU GPL) [*].\n\
\n\
Options marked [*] produce a lot of output - pipe it through `less' or `more' !");

const struct cmdinfo *cipaction= 0;
int f_pending=0, f_recursive=0, f_alsoselect=1, f_skipsame=0, f_noact=0;
int f_autodeconf=0, f_largemem=0;
unsigned long f_debug=0;
int fc_downgrade=1, fc_configureany=0, fc_hold=0, fc_removereinstreq=0, fc_overwrite=0;
int fc_removeessential=0, fc_conflicts=0, fc_depends=0, fc_dependsversion=0;
int fc_autoselect=1, fc_badpath=0, fc_overwritediverted=0, fc_architecture=0;
int fc_nonroot=0, fc_overwritedir=0;

const char *admindir = NULL;
const char *instdir = "";
struct packageinlist *ignoredependss = 0;

static const struct forceinfo {
  const char *name;
  int *opt;
} forceinfos[]= {
  { "downgrade",           &fc_downgrade                },
  { "configure-any",       &fc_configureany             },
  { "hold",                &fc_hold                     },
  { "remove-reinstreq",    &fc_removereinstreq          },
  { "remove-essential",    &fc_removeessential          },
  { "conflicts",           &fc_conflicts                },
  { "depends",             &fc_depends                  },
  { "depends-version",     &fc_dependsversion           },
  { "auto-select",         &fc_autoselect               },
  { "bad-path",            &fc_badpath                  },
  { "not-root",            &fc_nonroot                  },
  { "overwrite",           &fc_overwrite                },
  { "overwrite-diverted",  &fc_overwritediverted        },
  { "overwrite-dir",       &fc_overwritedir             },
  { "architecture",        &fc_architecture             },
  {  0                                                  }
};

static void helponly(const struct cmdinfo *cip, const char *value) {
  usage(); exit(0);
}
static void versiononly(const struct cmdinfo *cip, const char *value) {
  printversion(); exit(0);
}

static void setaction(const struct cmdinfo *cip, const char *value) {
  if (cipaction)
    badusage(_("conflicting actions --%s and --%s"),cip->olong,cipaction->olong);
  cipaction= cip;
}

static void setdebug (const struct cmdinfo *cpi, const char *value)
{
  char *endp;

  if (*value == 'h') {
    if (fprintf 
	(stderr,
"%s debugging option, --debug=<octal> or -D<octal>:\n\n\
 number  ref. in source   description\n\
      1   general           Generally helpful progress information\n\
      2   scripts           Invocation and status of maintainer scripts\n\
     10   eachfile          Output for each file processed\n\
    100   eachfiledetail    Lots of output for each file processed\n\
     20   conff             Output for each configuration file\n\
    200   conffdetail       Lots of output for each configuration file\n\
     40   depcon            Dependencies and conflicts\n\
    400   depcondetail      Lots of dependencies/conflicts output\n\
   1000   veryverbose       Lots of drivel about eg the dpkg/info directory\n\
   2000   stupidlyverbose   Insane amounts of drivel\n\n\
Debugging options are be mixed using bitwise-or.\n\
Note that the meanings and values are subject to change.\n", DPKG) < 0) {
      werr ("stderr");
    }
    exit (0);
  }
  
  f_debug = strtoul (value, &endp, 8);
  if (*endp) {
    badusage ("--debug requires an octal argument");
  }
}

static void setroot (const struct cmdinfo *cip, const char *value) 
{
  char *p;
  instdir = value;
  p = m_malloc (strlen (value) + strlen (ADMINDIR) + 1);
  sprintf (p, "%s%s", value, ADMINDIR);
  admindir = p;
}

static void ignoredepends(const struct cmdinfo *cip, const char *value) {
  char *copy, *p;
  const char *pnerr;
  struct packageinlist *ni;

  copy= m_malloc(strlen(value)+2);
  strcpy(copy,value);
  copy[strlen(value)+1]= 0;
  for (p=copy; *p; p++) {
    if (*p != ',') continue;
    *p++= 0;
    if (!*p || *p==',' || p==copy+1)
      badusage(_("null package name in --ignore-depends comma-separated list `%.250s'"),
               value);
  }
  p= copy;
  while (*p) {
    pnerr= illegal_packagename(value,0);
    if (pnerr) ohshite(_("--ignore-depends requires a legal package name. "
                       "`%.250s' is not; %s"), value, pnerr);
    ni= m_malloc(sizeof(struct packageinlist));
    ni->pkg= findpackage(value);
    ni->next= ignoredependss;
    ignoredependss= ni;
    p+= strlen(p)+1;
  }
}

static void setforce(const struct cmdinfo *cip, const char *value) {
  const char *comma;
  int l;
  const struct forceinfo *fip;

  if (!strcmp(value,"help")) {
    if (fputs (_("dpkg forcing options - control behaviour when problems found:\n\
  warn but continue:  --force-<thing>,<thing>,...\n\
  stop with error:    --refuse-<thing>,<thing>,... | --no-force-<thing>,...\n\
 Forcing things:\n\
  auto-select [*]        (De)select packages to install (remove) them\n\
  dowgrade [*]           Replace a package with a lower version\n\
  configure-any          Configure any package which may help this one\n\
  hold                   Process incidental packages even when on hold\n\
  bad-path               PATH is missing important programs, problems likely\n\
  not-root               Try to (de)install things even when not root\n\
  overwrite [*]          Overwrite a file from one package with another\n\
  overwrite-diverted     Overwrite a diverted file with an undiverted version\n\
  depends-version [!]    Turn dependency version problems into warnings\n\
  depends [!]            Turn all dependency problems into warnings\n\
  conflicts [!]          Allow installation of conflicting packages\n\
  architecture [!]       Process even packages with wrong architecture\n\
  overwrite-dir [!]      Overwrite one package's directory with another's file\n\
  remove-reinstreq [!]   Remove packages which require installation\n\
  remove-essential [!]   Remove an essential package\n\
\n\
WARNING - use of options marked [!] can seriously damage your installation.\n\
Forcing options marked [*] are enabled by default.\n"),
               stdout) < 0) {
      werr("stdout");
    }
    exit(0);
  }

  for (;;) {
    comma= strchr(value,',');
    l= comma ? (int)(comma-value) : strlen(value);
    for (fip=forceinfos; fip->name; fip++)
      if (!strncmp(fip->name,value,l) && strlen(fip->name)==l) break;
    if (!fip->name)
      badusage(_("unknown force/refuse option `%.*s'"), l<250 ? l : 250, value);
    *fip->opt= cip->arg;
    if (!comma) break;
    value= ++comma;
  }
}

static const char *const passlongopts[]= {
  "build", "contents", "control", "info", "field", "extract", "new", "old",
  "vextract", "fsys-tarfile", 0
};

static const char passshortopts[]= "bceIfxX";
static const char okpassshortopts[]= "D";

static const struct cmdinfo cmdinfos[]= {
  /* This table has both the action entries in it and the normal options.
   * The action entries are made with the ACTION macro, as they all
   * have a very similar structure.
   */

#define ACTION(longopt,shortopt,code,function) \
  { longopt, shortopt, 0,0,0, setaction, code, 0, (voidfnp) function }

  ACTION( "install",                            'i', act_install,            archivefiles    ),
  ACTION( "unpack",                              0,  act_unpack,             archivefiles    ),
  ACTION( "record-avail",                       'A', act_avail,              archivefiles    ),
  ACTION( "configure",                           0,  act_configure,          packages        ),
  ACTION( "remove",                             'r', act_remove,             packages        ),
  ACTION( "purge",                               0,  act_purge,              packages        ),
  ACTION( "listfiles",                          'L', act_listfiles,          enqperpackage   ),
  ACTION( "status",                             's', act_status,             enqperpackage   ),
  ACTION( "get-selections",                      0,  act_getselections,      getselections   ),
  ACTION( "set-selections",                      0,  act_setselections,      setselections   ),
  ACTION( "print-avail",                         0,  act_printavail,         enqperpackage   ),
  ACTION( "update-avail",                        0,  act_avreplace,          updateavailable ),
  ACTION( "merge-avail",                         0,  act_avmerge,            updateavailable ),
  ACTION( "clear-avail",                         0,  act_avclear,            updateavailable ),
  ACTION( "forget-old-unavail",                  0,  act_forgetold,          forgetold       ),
  ACTION( "audit",                              'C', act_audit,              audit           ),
  ACTION( "yet-to-unpack",                       0,  act_unpackchk,          unpackchk       ),
  ACTION( "list",                               'l', act_listpackages,       listpackages    ),
  ACTION( "search",                             'S', act_searchfiles,        searchfiles     ),
  ACTION( "assert-support-predepends",           0,  act_assertpredep,       assertpredep    ),
  ACTION( "assert-working-epoch",                0,  act_assertepoch,        assertepoch     ),
  ACTION( "print-architecture",                  0,  act_printarch,          printarch       ),
  ACTION( "print-installation-architecture",     0,  act_printinstarch,      printarch       ),
  ACTION( "print-gnu-build-architecture",        0,  act_printgnubuildarch,  printarch       ),
  ACTION( "print-gnu-installation-architecture", 0,  act_printgnuinstarch,   printarch       ),
  ACTION( "print-gnu-target-architecture",       0,  act_printgnutargetarch, printarch       ),
  ACTION( "predep-package",                      0,  act_predeppackage,      predeppackage   ),
  ACTION( "compare-versions",                    0,  act_cmpversions,        cmpversions     ),
  
  { "pending",           'a',  0,  &f_pending,     0,  0,             1              },
  { "recursive",         'R',  0,  &f_recursive,   0,  0,             1              },
  { "no-act",             0,   0,  &f_noact,       0,  0,             1              },
  {  0,                  'G',  0,  &fc_downgrade,  0,  0, /* alias for --refuse */ 0 },
  { "selected-only",     'O',  0,  &f_alsoselect,  0,  0,             0              },
  { "no-also-select",    'N',  0,  &f_alsoselect,  0,0,0 /* fixme: remove sometime */ },
  { "skip-same-version", 'E',  0,  &f_skipsame,    0,  0,             1              },
  { "auto-deconfigure",  'B',  0,  &f_autodeconf,  0,  0,             1              },
  { "largemem",           0,   0,  &f_largemem,    0,  0,             1              },
  { "smallmem",           0,   0,  &f_largemem,    0,  0,            -1              },
  { "root",               0,   1,  0, 0,               setroot                       },
  { "admindir",           0,   1,  0, &admindir,       0                             },
  { "instdir",            0,   1,  0, &instdir,        0                             },
  { "ignore-depends",     0,   1,  0, 0,               ignoredepends                 },
  { "force",              0,   2,  0, 0,               setforce,      1              },
  { "refuse",             0,   2,  0, 0,               setforce,      0              },
  { "no-force",           0,   2,  0, 0,               setforce,      0              },
  { "debug",             'D',  1,  0, 0,               setdebug                      },
  { "help",              'h',  0,  0, 0,               helponly                      },
  { "version",            0,   0,  0, 0,               versiononly                   },
  { "licence",/* UK spelling */ 0,0,0,0,               showcopyright                 },
  { "license",/* US spelling */ 0,0,0,0,               showcopyright                 },
  {  0,                   0                                                          }
};

static void execbackend(int argc, const char *const *argv) {
  execvp(BACKEND, (char* const*) argv);
  ohshite(_("failed to exec dpkg-deb"));
}

static void print_error_fatal (const char *emsg, const char *contextstring) 
{
  fprintf (stderr, "%s: %s\n", thisname, emsg);
}

int main(int argc, const char *const *argv) {
  jmp_buf ejbuf;
  int c;
  const char *argp, *const *blongopts, *const *argvs;
  static void (*actionfunction)(const char *const *argv);

  setlocale (LC_ALL, "");
  bindtextdomain ("dpkg", LOCALEDIR);
  textdomain ("dpkg");

  admindir = ADMINDIR;
  thisname = DPKG;

  if (setjmp(ejbuf)) { /* expect warning about possible clobbering of argv */
    error_unwind(ehflag_bombout); exit(2);
  }
  push_error_handler(&ejbuf,print_error_fatal,0);

  umask(022); /* Make sure all our status databases are readable. */
  
  for (argvs=argv+1; (argp= *argvs) && *argp++=='-'; argvs++) {
    if (*argp++=='-') {
      if (!strcmp(argp,"-")) break;
      for (blongopts=passlongopts; *blongopts; blongopts++) {
        if (!strcmp(argp,*blongopts)) execbackend(argc,argv);
      }
    } else {
      if (!*--argp) break;
      while ((c= *argp++)) {
        if (strchr(passshortopts,c)) execbackend(argc,argv);
        if (!strchr(okpassshortopts,c)) break;
      }
    }
  }

  myopt(&argv,cmdinfos);
  if (!cipaction) badusage(_("need an action option"));

  setvbuf(stdout,0,_IONBF,0);
  filesdbinit();

  actionfunction= (void (*)(const char* const*))cipaction->farg;

  actionfunction(argv);

  set_error_display(0,0);
  error_unwind(ehflag_normaltidy);

  return reportbroken_retexitstatus();
}
