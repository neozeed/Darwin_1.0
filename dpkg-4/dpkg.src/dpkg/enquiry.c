/*
 * dpkg - main program for package management
 * enquiry.c - status enquiry and listing options
 *
 * Copyright (C) 1995,1996 Ian Jackson <ijackson@gnu.ai.mit.edu>
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

/* fixme: per-package audit */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fnmatch.h>
#include <assert.h>
#include <unistd.h>

#include <dpkg.h>
#include <dpkg-db.h>
#include <myopt.h>

#include "varbuf.h"
#include "arch.h"
#include "filesdb.h"
#include "main.h"
#include "dpkg-int.h"
#include "config.h"

int pkglistqsortcmp(const void *a, const void *b) {
  struct pkginfo *pa= *(struct pkginfo**)a;
  struct pkginfo *pb= *(struct pkginfo**)b;
  return strcmp(pa->name,pb->name);
}

static void limiteddescription(struct pkginfo *pkg, int maxl,
                               const char **pdesc_r, int *l_r) {
  const char *pdesc, *p;
  int l;
  
  pdesc= pkg->installed.valid ? pkg->installed.description : 0;
  if (!pdesc) pdesc= _("(no description available)");
  p= strchr(pdesc,'\n');
  if (!p) p= pdesc+strlen(pdesc);
  l= (p - pdesc > maxl) ? maxl : (int)(p - pdesc);
  *pdesc_r=pdesc; *l_r=l;
}

static void list1package(struct pkginfo *pkg, int *head) {
  int l;
  const char *pdesc;
    
  if (!*head) {
    fputs(_("\
Desired=Unknown/Install/Remove/Purge\n\
| Status=Not/Installed/Config-files/Unpacked/Failed-config/Half-installed\n\
|/ Err?=(none)/Hold/Reinst-required/X=both-problems (Status,Err: uppercase=bad)\n\
||/ Name            Version        Description\n\
+++-===============-==============-============================================\n"),
          stdout);
    *head= 1;
  }
  
  if (!pkg->installed.valid) blankpackageperfile(&pkg->installed);
  limiteddescription(pkg,44,&pdesc,&l);
  printf("%c%c%c %-15.15s %-14.14s %.*s\n",
         _("uihrp")[pkg->want],
         _("nUFiHc")[pkg->status],
         _(" R?#")[pkg->eflag],
         pkg->name,
         versiondescribe(&pkg->installed.version,vdew_never),
         l, pdesc);
}         

void listpackages(const char *const *argv) {
  struct pkgiterator *it;
  struct pkginfo *pkg;
  struct pkginfo **pkgl;
  const char *thisarg;
  int np, i, head, found;

  modstatdb_init(admindir,msdbrw_readonly);

  np= countpackages();
  pkgl= m_malloc(sizeof(struct pkginfo*)*np);
  it= iterpkgstart(); i=0;
  while ((pkg= iterpkgnext(it))) {
    assert(i<np);
    pkgl[i++]= pkg;
  }
  iterpkgend(it);
  assert(i==np);

  qsort(pkgl,np,sizeof(struct pkginfo*),pkglistqsortcmp);
  head=0;
  
  if (!*argv) {
    for (i=0; i<np; i++) {
      pkg= pkgl[i];
      if (pkg->status == stat_notinstalled) continue;
      list1package(pkg,&head);
    }
  } else {
    while ((thisarg= *argv++)) {
      found= 0;
      for (i=0; i<np; i++) {
        pkg= pkgl[i];
        if (fnmatch(thisarg,pkg->name,0)) continue;
        list1package(pkg,&head); found++;
      }
      if (!found)
        fprintf(stderr,_("No packages found matching %s.\n"),thisarg);
    }
  }
  if (ferror(stdout)) werr("stdout");
  if (ferror(stderr)) werr("stderr");  
}

struct badstatinfo {
  int (*yesno)(struct pkginfo*, const struct badstatinfo *bsi);
  int val;
  const char *explanation;
};

static int bsyn_reinstreq(struct pkginfo *pkg, const struct badstatinfo *bsi) {
  return pkg->eflag &= eflagf_reinstreq;
}

static int bsyn_status(struct pkginfo *pkg, const struct badstatinfo *bsi) {
  if (pkg->eflag &= eflagf_reinstreq) return 0;
  return pkg->status == bsi->val;
}

static const struct badstatinfo badstatinfos[]= {
  {
    bsyn_reinstreq, 0,
    N_("The following packages are in a mess due to serious problems during\n"
       "installation.  They must be reinstalled for them (and any packages\n"
       "that depend on them) to function properly:\n")
  }, {
    bsyn_status, stat_unpacked,
    N_("The following packages have been unpacked but not yet configured.\n"
       "They must be configured using dpkg --configure or the configure\n"
       "menu option in dselect for them to work:\n")
  }, {
    bsyn_status, stat_halfconfigured,
    N_("The following packages are only half configured, probably due to problems\n"
       "configuring them the first time.  The configuration should be retried using\n"
       "dpkg --configure <package>:\n")
  }, {
    bsyn_status, stat_halfinstalled,
    N_("The following packages are only half installed, due to problems during\n"
       "installation.  The installation can probably be completed by retrying it;\n"
       "the packages can be removed using dselect or dpkg --remove:\n")
  }, {
    0
  }
};

static void describebriefly(struct pkginfo *pkg) {
  int maxl, l;
  const char *pdesc;

  maxl= 57;
  l= strlen(pkg->name);
  if (l>20) maxl -= (l-20);
  limiteddescription(pkg,maxl,&pdesc,&l);
  printf(" %-20s %.*s\n",pkg->name,l,pdesc);
}

void audit(const char *const *argv) {
  struct pkgiterator *it;
  struct pkginfo *pkg;
  const struct badstatinfo *bsi;
  int head;

  if (*argv) badusage(_("--audit does not take any arguments"));

  modstatdb_init(admindir,msdbrw_readonly);

  for (bsi= badstatinfos; bsi->yesno; bsi++) {
    head= 0;
    it= iterpkgstart(); 
    while ((pkg= iterpkgnext(it))) {
      if (!bsi->yesno(pkg,bsi)) continue;
      if (!head) {
        fputs(gettext(bsi->explanation),stdout);
        head= 1;
      }
      describebriefly(pkg);
    }
    iterpkgend(it);
    if (head) putchar('\n');
  }
  if (ferror(stderr)) werr("stderr");  
}

struct sectionentry {
  struct sectionentry *next;
  const char *name;
  int count;
};

static int yettobeunpacked(struct pkginfo *pkg, const char **thissect) {
  if (pkg->want != want_install) return 0;

  switch (pkg->status) {
  case stat_unpacked: case stat_installed: case stat_halfconfigured:
    return 0;
  case stat_notinstalled: case stat_halfinstalled: case stat_configfiles:
    if (thissect)
      *thissect= pkg->section && *pkg->section ? pkg->section : "<unknown>";
    return 1;
  default:
    internerr("unknown status checking for unpackedness");
  }
}

void unpackchk(const char *const *argv) {
  int totalcount, sects;
  struct sectionentry *sectionentries, *se, **sep;
  struct pkgiterator *it;
  struct pkginfo *pkg;
  const char *thissect;
  char buf[20];
  int width;
  
  if (*argv) badusage(_("--yet-to-unpack does not take any arguments"));

  modstatdb_init(admindir,msdbrw_readonly);

  totalcount= 0;
  sectionentries= 0;
  sects= 0;
  it= iterpkgstart(); 
  while ((pkg= iterpkgnext(it))) {
    if (!yettobeunpacked(pkg, &thissect)) continue;
    for (se= sectionentries; se && strcasecmp(thissect,se->name); se= se->next);
    if (!se) {
      se= nfmalloc(sizeof(struct sectionentry));
      for (sep= &sectionentries;
           *sep && strcasecmp(thissect,(*sep)->name) > 0;
           sep= &(*sep)->next);
      se->name= thissect;
      se->count= 0;
      se->next= *sep;
      *sep= se;
      sects++;
    }
    se->count++; totalcount++;
  }
  iterpkgend(it);

  if (totalcount == 0) exit(0);
  
  if (totalcount <= 12) {
    it= iterpkgstart(); 
    while ((pkg= iterpkgnext(it))) {
      if (!yettobeunpacked(pkg,0)) continue;
      describebriefly(pkg);
    }
    iterpkgend(it);
  } else if (sects <= 12) {
    for (se= sectionentries; se; se= se->next) {
      sprintf(buf,"%d",se->count);
      printf(" %d in %s: ",se->count,se->name);
      width= 70-strlen(se->name)-strlen(buf);
      while (width > 59) { putchar(' '); width--; }
      it= iterpkgstart(); 
      while ((pkg= iterpkgnext(it))) {
        if (!yettobeunpacked(pkg,&thissect)) continue;
        if (strcasecmp(thissect,se->name)) continue;
        width -= strlen(pkg->name); width--;
        if (width < 4) { printf(" ..."); break; }
        printf(" %s",pkg->name);
      }
      iterpkgend(it);
      putchar('\n');
    }
  } else {
    printf(_(" %d packages, from the following sections:"),totalcount);
    width= 0;
    for (se= sectionentries; se; se= se->next) {
      sprintf(buf,"%d",se->count);
      width -= (6 + strlen(se->name) + strlen(buf));
      if (width < 0) { putchar('\n'); width= 73 - strlen(se->name) - strlen(buf); }
      printf("   %s (%d)",se->name,se->count);
    }
    putchar('\n');
  }
  fflush(stdout);
  if (ferror(stdout)) werr("stdout");
}

static int searchoutput(struct filenamenode *namenode) {
  int found, i;
  struct filepackages *packageslump;

  if (namenode->divert) {
    for (i=0; i<2; i++) {
      if (namenode->divert->pkg) printf(_("diversion by %s"),namenode->divert->pkg->name);
      else printf(_("local diversion"));
      /* next translation just for correct colon layout */
      printf(_(" %s: %s\n"), i ? _("to") : _("from"),
             i ?
             (namenode->divert->useinstead
              ? namenode->divert->useinstead->name
              : namenode->name)
             :
             (namenode->divert->camefrom
              ? namenode->divert->camefrom->name
              : namenode->name));
    }
  }
  found= 0;
  for (packageslump= namenode->packages;
       packageslump;
       packageslump= packageslump->more) {
    for (i=0; i < PERFILEPACKAGESLUMP && packageslump->pkgs[i]; i++) {
      if (found) fputs(", ",stdout);
      fputs(packageslump->pkgs[i]->name,stdout);
      found++;
    }
  }
  if (found) printf(": %s\n",namenode->name);
  return found + (namenode->divert ? 1 : 0);
}

void searchfiles(const char *const *argv) {
  struct filenamenode *namenode;
  struct fileiterator *it;
  const char *thisarg;
  int found;
  static struct varbuf vb;
  
  if (!*argv)
    badusage(_("--search needs at least one file name pattern argument"));

  modstatdb_init(admindir,msdbrw_readonly);
  ensure_allinstfiles_available_quiet();
  ensure_diversions();

  while ((thisarg= *argv++) != 0) {
    found= 0;
    if (!strchr("*[?/",*thisarg)) {
      varbufreset(&vb);
      varbufaddc(&vb,'*');
      varbufaddstr(&vb,thisarg);
      varbufaddc(&vb,'*');
      varbufaddc(&vb,0);
      thisarg= vb.buf;
    }
    if (strcspn(thisarg,"*[?\\") == strlen(thisarg)) {
      namenode= findnamenode(thisarg);
      found += searchoutput(namenode);
    } else {
      it= iterfilestart();
      while ((namenode= iterfilenext(it)) != 0) {
        if (fnmatch(thisarg,namenode->name,0)) continue;
        found+= searchoutput(namenode);
      }
      iterfileend(it);
    }
    if (!found) {
      fprintf(stderr,_("dpkg: %s not found.\n"),thisarg);
      if (ferror(stderr)) werr("stderr");
    } else {
      if (ferror(stdout)) werr("stdout");
    }
  }
}

void enqperpackage(const char *const *argv) {
  int failures;
  const char *thisarg;
  struct fileinlist *file;
  struct pkginfo *pkg;
  struct filenamenode *namenode;
  
  if (!*argv)
    badusage(_("--%s needs at least one package name argument"), cipaction->olong);

  failures= 0;
  modstatdb_init(admindir,msdbrw_readonly);
  
  while ((thisarg= *argv++) != 0) {
    pkg= findpackage(thisarg);

    switch (cipaction->arg) {
      
    case act_status:
      if (pkg->status == stat_notinstalled &&
          pkg->priority == pri_unknown &&
          !(pkg->section && *pkg->section) &&
          !pkg->files &&
          pkg->want == want_unknown &&
          !informative(pkg,&pkg->installed)) {
        printf(_("Package `%s' is not installed and no info is available.\n"),pkg->name);
        failures++;
      } else {
        writerecord(stdout, "<stdout>", pkg, &pkg->installed);
      }
      break;

    case act_printavail:
      if (!informative(pkg,&pkg->available)) {
        printf(_("Package `%s' is not available.\n"),pkg->name);
        failures++;
      } else {
        writerecord(stdout, "<stdout>", pkg, &pkg->available);
      }
      break;
      
    case act_listfiles:
      switch (pkg->status) {
      case stat_notinstalled: 
        printf(_("Package `%s' is not installed.\n"),pkg->name);
        failures++;
        break;
        
      default:
        ensure_packagefiles_available(pkg);
        ensure_diversions();
        file= pkg->clientdata->files;
        if (!file) {
          printf(_("Package `%s' does not contain any files (!)\n"),pkg->name);
        } else {
          while (file) {
            namenode= file->namenode;
            puts(namenode->name);
            if (namenode->divert && !namenode->divert->camefrom) {
	      /* submsg: enqperpackage */
              if (!namenode->divert->pkg) printf(_("locally diverted"));
	      /* submsg: enqperpackage */
              else if (pkg == namenode->divert->pkg) printf(_("package diverts others"));
	      /* submsg: enqperpackage */
              else printf(_("diverted by %s"),namenode->divert->pkg->name);
	      /* supermsg: enqperpackage <at-start> */
              printf(_(" to: %s\n"),namenode->divert->useinstead->name);
            }
            file= file->next;
          }
        }
        break;
      }
      break;

    default:
      internerr("unknown action");
    }
        
    putchar('\n');
    if (ferror(stdout)) werr("stdout");
  }

  if (failures) {
    puts(_("Use dpkg --info (= dpkg-deb --info) to examine archive files,\n"
         "and dpkg --contents (= dpkg-deb --contents) to list their contents."));
    if (ferror(stdout)) werr("stdout");
  }
}

void assertepoch(const char *const *argv) {
  static struct versionrevision epochversion = {~0UL,0,0};
  struct pkginfo *pkg;

  if (*argv) badusage(_("--assert-working-epoch does not take any arguments"));

  modstatdb_init(admindir,msdbrw_readonly);
  pkg= findpackage("dpkg");
  if (epochversion.epoch == ~0UL) {
    epochversion.epoch= 0;
    epochversion.version= nfstrsave("1.4.0.7");
    epochversion.revision= 0;
  }
  switch (pkg->status) {
  case stat_installed:
    break;
  case stat_unpacked: case stat_halfconfigured: case stat_halfinstalled:
    if (versionsatisfied3(&pkg->configversion,&epochversion,dvr_laterequal))
      break;
    printf(_("Version of dpkg with working epoch support not yet configured.\n"
	     " Please use `dpkg --configure dpkg', and then try again.\n"));
    exit(1);
  default:
    printf(_("dpkg not recorded as installed, cannot check for epoch support !\n"));
    exit(1);
  }
}

void assertpredep(const char *const *argv) {
  static struct versionrevision predepversion = {~0UL,0,0};
  struct pkginfo *pkg;
  
  if (*argv) badusage(_("--assert-support-predepends does not take any arguments"));

  modstatdb_init(admindir,msdbrw_readonly);
  pkg= findpackage("dpkg");
  if (predepversion.epoch == ~0UL) {
    predepversion.epoch= 0;
    predepversion.version= nfstrsave("1.1.0");
    predepversion.revision= 0;
  }
  switch (pkg->status) {
  case stat_installed:
    break;
  case stat_unpacked: case stat_halfconfigured: case stat_halfinstalled:
    if (versionsatisfied3(&pkg->configversion,&predepversion,dvr_laterequal))
      break;
    printf(_("Version of dpkg with Pre-Depends support not yet configured.\n"
           " Please use `dpkg --configure dpkg', and then try again.\n"));
    exit(1);
  default:
    printf(_("dpkg not recorded as installed, cannot check for Pre-Depends support !\n"));
    exit(1);
  }
}

void predeppackage(const char *const *argv) {
  /* Print a single package which:
   *  (a) is the target of one or more relevant predependencies.
   *  (b) has itself no unsatisfied pre-dependencies.
   * If such a package is present output is the Packages file entry,
   *  which can be massaged as appropriate.
   * Exit status:
   *  0 = a package printed, OK
   *  1 = no suitable package available
   *  2 = error
   */
  static struct varbuf vb;
  
  struct pkgiterator *it;
  struct pkginfo *pkg, *startpkg, *trypkg;
  struct dependency *dep;
  struct deppossi *possi, *provider;

  if (*argv) badusage(_("--predep-package does not take any argument"));

  modstatdb_init(admindir,msdbrw_readonly);
  clear_istobes(); /* We use clientdata->istobe to detect loops */

  for (it=iterpkgstart(), dep=0;
       !dep && (pkg=iterpkgnext(it));
       ) {
    if (pkg->want != want_install) continue; /* Ignore packages user doesn't want */
    if (!pkg->files) continue; /* Ignore packages not available */
    pkg->clientdata->istobe= itb_preinstall;
    for (dep= pkg->available.depends; dep; dep= dep->next) {
      if (dep->type != dep_predepends) continue;
      if (depisok(dep,&vb,0,1)) continue;
      break; /* This will leave dep non-NULL, and so exit the loop. */
    }
    pkg->clientdata->istobe= itb_normal;
    /* If dep is NULL we go and get the next package. */
  }
  if (!dep) exit(1); /* Not found */
  assert(pkg);
  startpkg= pkg;
  pkg->clientdata->istobe= itb_preinstall;

  /* OK, we have found an unsatisfied predependency.
   * Now go and find the first thing we need to install, as a first step
   * towards satisfying it.
   */
  do {
    /* We search for a package which would satisfy dep, and put it in pkg */
    for (possi=dep->list, pkg=0;
         !pkg && possi;
         possi=possi->next) {
      trypkg= possi->ed;
      if (!trypkg->available.valid) continue;
      if (trypkg->files && versionsatisfied(&trypkg->available,possi)) {
        if (trypkg->clientdata->istobe == itb_normal) { pkg= trypkg; break; }
      }
      if (possi->verrel != dvr_none) continue;
      for (provider=possi->ed->available.depended;
           !pkg && provider;
           provider=provider->next) {
        if (provider->up->type != dep_provides) continue;
        trypkg= provider->up->up;
        if (!trypkg->available.valid || !trypkg->files) continue;
        if (trypkg->clientdata->istobe == itb_normal) { pkg= trypkg; break; }
      }
    }
    if (!pkg) {
      varbufreset(&vb);
      describedepcon(&vb,dep);
      varbufaddc(&vb,0);
      fprintf(stderr, _("dpkg: cannot see how to satisfy pre-dependency:\n %s\n"),vb.buf);
      ohshit(_("cannot satisfy pre-dependencies for %.250s (wanted due to %.250s)"),
             dep->up->name,startpkg->name);
    }
    pkg->clientdata->istobe= itb_preinstall;
    for (dep= pkg->available.depends; dep; dep= dep->next) {
      if (dep->type != dep_predepends) continue;
      if (depisok(dep,&vb,0,1)) continue;
      break; /* This will leave dep non-NULL, and so exit the loop. */
    }
  } while (dep);

  /* OK, we've found it - pkg has no unsatisfied pre-dependencies ! */
  writerecord(stdout,"<stdout>",pkg,&pkg->available);
  if (fflush(stdout)) werr("stdout");
}

void cmpversions(const char *const *argv) {
  struct relationinfo {
    const char *string;
    /* These values are exit status codes, so 0=true, 1=false */
    int if_lesser, if_equal, if_greater;
    int if_none_a, if_none_both, if_none_b;
  };

  static const struct relationinfo relationinfos[]= {
   /*              < = > !a!2!b  */
    { "le",        0,0,1, 0,0,1  },
    { "lt",        0,1,1, 0,1,1  },
    { "eq",        1,0,1, 1,0,1  },
    { "ne",        0,1,0, 0,1,0  },
    { "ge",        1,0,0, 1,0,0  },
    { "gt",        1,1,0, 1,1,0  },
    { "le-nl",     0,0,1, 1,0,0  }, /* Here none        */
    { "lt-nl",     0,1,1, 1,1,0  }, /* is counted       */
    { "ge-nl",     1,0,0, 0,0,1  }, /* than any version.*/
    { "gt-nl",     1,1,0, 0,1,1  }, /*                  */
    { "<",         0,0,1, 0,0,1  }, /* For compatibility*/
    { "<=",        0,0,1, 0,0,1  }, /* with dpkg        */
    { "<<",        0,1,1, 0,1,1  }, /* control file     */
    { "=",         1,0,1, 1,0,1  }, /* syntax           */
    { ">",         1,0,0, 1,0,0  }, /*                  */
    { ">=",        1,0,0, 1,0,0  },
    { ">>",        1,1,0, 1,1,0  },
    {  0                         }
  };

  const struct relationinfo *rip;
  const char *emsg;
  struct versionrevision a, b;
  int r;
  
  if (!argv[0] || !argv[1] || !argv[2] || argv[3])
    badusage(_("--cmpversions takes three arguments:"
             " <version> <relation> <version>"));

  for (rip=relationinfos; rip->string && strcmp(rip->string,argv[1]); rip++);

  if (!rip->string) badusage(_("--cmpversions bad relation"));

  if (*argv[0] && strcmp(argv[0],"<unknown>")) {
    emsg= parseversion(&a,argv[0]);
    if (emsg) {
      if (printf(_("version a has bad syntax: %s\n"),emsg) == EOF) werr("stdout");
      if (fflush(stdout)) werr("stdout");
      exit(1);
    }
  } else {
    blankversion(&a);
  }
  if (*argv[2] && strcmp(argv[2],"<unknown>")) {
    emsg= parseversion(&b,argv[2]);
    if (emsg) {
      if (printf(_("version b has bad syntax: %s\n"),emsg) == EOF) werr("stdout");
      if (fflush(stdout)) werr("stdout");
      exit(1);
    }
  } else {
    blankversion(&b);
  }
  if (!informativeversion(&a)) {
    exit(informativeversion(&b) ? rip->if_none_a : rip->if_none_both);
  } else if (!informativeversion(&b)) {
    exit(rip->if_none_b);
  }
  r= versioncompare(&a,&b);
  debug(dbg_general,"cmpversions a=`%s' b=`%s' r=%d",
        versiondescribe(&a,vdew_always),
        versiondescribe(&b,vdew_always),
        r);
  if (r>0) exit(rip->if_greater);
  else if (r<0) exit(rip->if_lesser);
  else exit(rip->if_equal);
}

void printarch (const char *const *argv) 
{
  char buf1[DPKG_ARCHSTR_MAX], buf2[DPKG_ARCHSTR_MAX];
  char *arch;
  int ret;
  
  switch (cipaction->arg) {
  case act_printarch:
    if (*argv) { badusage (_("--print-architecture does not take any argument")); }
    ret = dpkg_find_gnu_target_architecture (buf1, DPKG_ARCHSTR_MAX);
    if (ret <= 0) { ohshit (_("unable to determine target architecture")); }
    ret = dpkg_canonicalize_architecture (buf1, buf2, DPKG_ARCHSTR_MAX);
    if (ret <= 0) { ohshit (_("unable to canonicalize target architecture")); }
    ret = dpkg_reduce_archcompat (buf2, buf1, DPKG_ARCHSTR_MAX);
    if (ret <= 0) { ohshit (_("unable to reduce target architecture")); }
    arch = buf1;
    break;
  case act_printinstarch:
    if (*argv) { badusage (_("--print-installation-architecture does not take any argument")); }
    ret = dpkg_find_gnu_installation_architecture (buf1, DPKG_ARCHSTR_MAX);
    if (ret <= 0) { ohshit (_("unable to determine installation architecture")); }
    ret = dpkg_canonicalize_architecture (buf1, buf2, DPKG_ARCHSTR_MAX);
    if (ret <= 0) { ohshit (_("unable to canonicalize installation architecture")); }
    ret = dpkg_reduce_archcompat (buf2, buf1, DPKG_ARCHSTR_MAX);
    if (ret <= 0) { ohshit (_("unable to reduce installation architecture")); }
    arch = buf1;
    break;
  case act_printgnubuildarch:
    if (*argv) { badusage (_("--print-gnu-build-architecture does not take any argument")); }
    ret = dpkg_find_gnu_build_architecture (buf1, DPKG_ARCHSTR_MAX);
    if (ret <= 0) { ohshit (_("unable to determine build architecture")); }
    ret = dpkg_canonicalize_architecture (buf1, buf2, DPKG_ARCHSTR_MAX);
    if (ret <= 0) { ohshit (_("unable to canonicalize build architecture")); }
    arch = buf2;
    break;
  case act_printgnuinstarch:
    if (*argv) { badusage (_("--print-gnu-installation-architecture does not take any argument")); }
    ret = dpkg_find_gnu_installation_architecture (buf1, DPKG_ARCHSTR_MAX);
    if (ret <= 0) { ohshit (_("unable to determine installation architecture")); }
    ret = dpkg_canonicalize_architecture (buf1, buf2, DPKG_ARCHSTR_MAX);
    if (ret <= 0) { ohshit (_("unable to canonicalize installation architecture")); }
    arch = buf2;
    break;
  case act_printgnutargetarch:
    if (*argv) { badusage (_("--print-gnu-target-architecture does not take any argument")); }
    ret = dpkg_find_gnu_target_architecture (buf1, DPKG_ARCHSTR_MAX);
    if (ret <= 0) { ohshit (_("unable to determine target architecture")); }
    ret = dpkg_canonicalize_architecture (buf1, buf2, DPKG_ARCHSTR_MAX);
    if (ret <= 0) { ohshit (_("unable to canonicalize target architecture")); }
    arch = buf2;
    break;
  default:
    ohshit (_("unknown action in printarch()"));
    arch = "unknown";
    break;
  }
  
  if (printf ("%s\n", arch) < 0) { werr ("stdout"); }
  if (fflush (stdout) != 0) { werr ("stdout"); }
}
