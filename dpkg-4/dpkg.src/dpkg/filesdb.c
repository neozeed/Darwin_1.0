/*
 * dpkg - main program for package management
 * filesdb.c - management of database of files installed on system
 *
 * Copyright (C) 1995 Ian Jackson <iwj10@cus.cam.ac.uk>
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

#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

#include <dpkg.h>
#include <dpkg-db.h>
#include <dpkg-var.h>

#include "filesdb.h"
#include "main.h"
#include "dpkg-int.h"
#include "config.h"

#ifdef HAVE_SYSINFO
# include <sys/sysinfo.h>
#endif /* HAVE_SYSINFO */

/*** Generic data structures and routines ***/

static int allpackagesdone= 0;
static int nfiles= 0;
static struct diversion *diversions= 0;
static FILE *diversionsfile= 0;

void note_must_reread_files_inpackage(struct pkginfo *pkg) {
  allpackagesdone= 0;
  ensure_package_clientdata(pkg);
  pkg->clientdata->fileslistvalid= 0;
}

static int saidread=0;

void ensure_packagefiles_available(struct pkginfo *pkg) {
  static struct varbuf fnvb;
  static char stdiobuf[8192];
  
  FILE *file;
  const char *filelistfile;
  struct fileinlist **lendp, *newent, *current;
  struct filepackages *packageslump;
  int search, findlast, putat, l;
  char *thefilename;
  char linebuf[1024];

  if (pkg->clientdata && pkg->clientdata->fileslistvalid) return;
  ensure_package_clientdata(pkg);

  /* Throw away any the stale data, if there was any. */
  for (current= pkg->clientdata->files;
       current;
       current= current->next) {
    /* For each file that used to be in the package,
     * go through looking for this package's entry in the list
     * of packages containing this file, and blank it out.
     */
    for (packageslump= current->namenode->packages;
         packageslump;
         packageslump= packageslump->more)
      for (search= 0;
           search < PERFILEPACKAGESLUMP && packageslump->pkgs[search];
           search++)
        if (packageslump->pkgs[search] == pkg) {
          /* Hah!  Found it. */
          for (findlast= search+1;
               findlast < PERFILEPACKAGESLUMP && packageslump->pkgs[findlast];
               findlast++);
          findlast--;
          /* findlast is now the last occupied entry, which may be the same as
           * search.  We blank out the entry for this package.  We also
           * have to copy the last entry into the empty slot, because
           * the list is null-pointer-terminated.
           */
          packageslump->pkgs[search]= packageslump->pkgs[findlast];
          packageslump->pkgs[findlast]= 0;
          /* This may result in an empty link in the list.  This is OK. */
          goto xit_search_to_delete_from_perfilenodelist;
        }
  xit_search_to_delete_from_perfilenodelist:
    ;
    /* The actual filelist links were allocated using nfmalloc, so
     * we shouldn't free them.
     */
  }
  pkg->clientdata->files= 0;

  /* Packages which aren't installed don't have a files list. */
  if (pkg->status == stat_notinstalled) {
    pkg->clientdata->fileslistvalid= 1; return;
  }

  filelistfile= pkgadminfile(pkg,LISTFILE);

  onerr_abort++;
  
  file= fopen(filelistfile,"r");

  if (!file) {
    if (errno != ENOENT)
      ohshite(_("unable to open files list file for package `%.250s'"),pkg->name);
    onerr_abort--;
    if (pkg->status != stat_configfiles) {
      if (saidread == 1) putc('\n',stderr);
      fprintf(stderr,
              _("dpkg: serious warning: files list file for package `%.250s' missing,"
              " assuming package has no files currently installed.\n"), pkg->name);
    }
    pkg->clientdata->files= 0;
    pkg->clientdata->fileslistvalid= 1;
    return;
  }

  push_cleanup(cu_closefile,ehflag_bombout, 0,0, 1,(void*)file);
  
  if (setvbuf(file,stdiobuf,_IOFBF,sizeof(stdiobuf)))
    /* FIXME: is this message's parameter correct ? */
    ohshite(_("unable to set buffering on `%.250s'"),filelistfile);
  
  lendp= &pkg->clientdata->files;
  varbufreset(&fnvb);
  while (fgets(linebuf,sizeof(linebuf),file)) {
    /* This is a very important loop, and it is therefore rather messy.
     * We break the varbuf abstraction even more than usual, and we
     * avoid copying where possible.
     */
    l= strlen(linebuf);
    if (l == 0) ohshit(_("fgets gave an empty null-terminated string from `%.250s'"),
                       filelistfile);
    l--;
    if (linebuf[l] != '\n') {
      varbufaddstr(&fnvb,linebuf);
      continue;
    } else if (!fnvb.used && l>0 && linebuf[l-1] != '/') { /* fast path */
      linebuf[l]= 0;
      thefilename= linebuf;
    } else {
      if (l>0 && linebuf[l-1] == '/') l--; /* strip trailing slashes */
      linebuf[l]= 0;
      varbufaddstr(&fnvb,linebuf);
      varbufaddc(&fnvb,0);
      fnvb.used= 0;
      thefilename= fnvb.buf;
    }
    if (!*thefilename)
      ohshit(_("files list file for package `%.250s' contains empty filename"),pkg->name);
    newent= nfmalloc(sizeof(struct fileinlist));
    newent->namenode= findnamenode(thefilename);
    newent->next= 0;
    *lendp= newent;
    lendp= &newent->next;
  }
  if (ferror(file))
    ohshite(_("error reading files list file for package `%.250s'"),pkg->name);
  pop_cleanup(ehflag_normaltidy); /* file= fopen() */
  if (fclose(file))
    ohshite(_("error closing files list file for package `%.250s'"),pkg->name);
  if (fnvb.used)
    ohshit(_("files list file for package `%.250s' is truncated"),pkg->name);

  onerr_abort--;

  for (newent= pkg->clientdata->files; newent; newent= newent->next) {
    packageslump= newent->namenode->packages;
    if (packageslump) {
      for (putat= 0;
           putat < PERFILEPACKAGESLUMP && packageslump->pkgs[putat];
           putat++);
      if (putat >= PERFILEPACKAGESLUMP) packageslump= 0;
    }
    if (!packageslump) {
      packageslump= nfmalloc(sizeof(struct filepackages));
      packageslump->more= newent->namenode->packages;
      newent->namenode->packages= packageslump;
      putat= 0;
    }
    packageslump->pkgs[putat]= pkg;
    if (++putat < PERFILEPACKAGESLUMP) packageslump->pkgs[putat]= 0;
  }      
  pkg->clientdata->fileslistvalid= 1;
}

void ensure_allinstfiles_available(void) {
  struct pkgiterator *it;
  struct pkginfo *pkg;
    
  if (allpackagesdone) return;
  if (saidread<2) {
    saidread=1;
    printf(f_largemem>0 ? _("(Reading database ... ") : _("(Scanning database ... "));
  }
  it= iterpkgstart();
  while ((pkg= iterpkgnext(it)) != 0) ensure_packagefiles_available(pkg);
  iterpkgend(it);
  allpackagesdone= 1;

  if (saidread==1) {
    printf(_("%d files and directories currently installed.)\n"),nfiles);
    saidread=2;
  }
}

void ensure_allinstfiles_available_quiet(void) {
  saidread=2;
  ensure_allinstfiles_available();
}

void write_filelist_except(struct pkginfo *pkg, struct fileinlist *list, int leaveout) {
  /* If leaveout is nonzero, will not write any file whose filenamenode
   * has the fnnf_elide_other_lists flag set.
   */
  static struct varbuf vb, newvb;
  FILE *file;

  varbufreset(&vb);
  varbufaddstr(&vb,admindir);
  varbufaddstr(&vb,"/");
  varbufaddstr(&vb,INFODIR);
  varbufaddstr(&vb,"/");
  varbufaddstr(&vb,pkg->name);
  varbufaddstr(&vb,".");
  varbufaddstr(&vb,LISTFILE);
  varbufaddc(&vb,0);

  varbufreset(&newvb);
  varbufaddstr(&newvb,vb.buf);
  varbufaddstr(&newvb,NEWDBEXT);
  varbufaddc(&newvb,0);
  
  file= fopen(newvb.buf,"w+");
  if (!file)
    ohshite(_("unable to create updated files list file for package %s"),pkg->name);
  push_cleanup(cu_closefile,ehflag_bombout, 0,0, 1,(void*)file);
  while (list) {
    if (!(leaveout && (list->namenode->flags & fnnf_elide_other_lists))) {
      fputs(list->namenode->name,file);
      putc('\n',file);
    }
    list= list->next;
  }
  if (ferror(file))
    ohshite(_("failed to write to updated files list file for package %s"),pkg->name);
  if (fflush(file))
    ohshite(_("failed to flush updated files list file for package %s"),pkg->name);
  if (fsync(fileno(file)))
    ohshite(_("failed to sync updated files list file for package %s"),pkg->name);
  pop_cleanup(ehflag_normaltidy); /* file= fopen() */
  if (fclose(file))
    ohshite(_("failed to close updated files list file for package %s"),pkg->name);
  if (rename(newvb.buf,vb.buf))
    ohshite(_("failed to install updated files list file for package %s"),pkg->name);

  note_must_reread_files_inpackage(pkg);
}

void reversefilelist_init(struct reversefilelistiter *iterptr,
                          struct fileinlist *files) {
  /* Initialises an iterator that appears to go through the file
   * list `files' in reverse order, returning the namenode from
   * each.  What actually happens is that we walk the list here,
   * building up a reverse list, and then peel it apart one
   * entry at a time.
   */
  struct fileinlist *newent;
  
  iterptr->todo= 0;
  while (files) {
    newent= m_malloc(sizeof(struct fileinlist));
    newent->namenode= files->namenode;
    newent->next= iterptr->todo;
    iterptr->todo= newent;
    files= files->next;
  }
}

struct filenamenode *reversefilelist_next(struct reversefilelistiter *iterptr) {
  struct filenamenode *ret;
  struct fileinlist *todo;

  todo= iterptr->todo;
  if (!todo) return 0;
  ret= todo->namenode;
  iterptr->todo= todo->next;
  free(todo);
  return ret;
}

void reversefilelist_abort(struct reversefilelistiter *iterptr) {
  /* Clients must call this function to clean up the reversefilelistiter
   * if they wish to break out of the iteration before it is all done.
   * Calling this function is not necessary if reversefilelist_next has
   * been called until it returned 0.
   */
  while (reversefilelist_next(iterptr));
}

void ensure_diversions(void) {
  static struct varbuf vb;

  struct stat stab1, stab2;
  char linebuf[MAXDIVERTFILENAME];
  FILE *file;
  struct diversion *ov, *oicontest, *oialtname;
  int l;
  
  varbufreset(&vb);
  varbufaddstr(&vb,admindir);
  varbufaddstr(&vb,"/");
  varbufaddstr(&vb,DIVERSIONSFILE);
  varbufaddc(&vb,0);

  onerr_abort++;
  
  file= fopen(vb.buf,"r");
  if (!file) {
    if (errno != ENOENT) ohshite(_("failed to open diversions file"));
    if (!diversionsfile) { onerr_abort--; return; }
  } else if (diversionsfile) {
    if (fstat(fileno(diversionsfile),&stab1))
      ohshite(_("failed to fstat previous diversions file"));
    if (fstat(fileno(file),&stab2))
      ohshite(_("failed to fstat diversions file"));
    if (stab1.st_dev == stab2.st_dev && stab1.st_ino == stab2.st_ino) {
      fclose(file); onerr_abort--; return;
    }
  }
  if (diversionsfile) fclose(diversionsfile);
  diversionsfile= file;

  for (ov= diversions; ov; ov= ov->next) {
    ov->useinstead->divert->camefrom->divert= 0;
    ov->useinstead->divert= 0;
  }
  diversions= 0;
  if (!file) { onerr_abort--; return; }

  while (fgets(linebuf,sizeof(linebuf),file)) {
    oicontest= nfmalloc(sizeof(struct diversion));
    oialtname= nfmalloc(sizeof(struct diversion));

    l= strlen(linebuf);
    if (l == 0) ohshit(_("fgets gave an empty string from diversions [i]"));
    if (linebuf[--l] != '\n') ohshit(_("diversions file has too-long line or EOF [i]"));
    linebuf[l]= 0;
    oialtname->camefrom= findnamenode(linebuf);
    
    if (!fgets(linebuf,sizeof(linebuf),file))
      if (ferror(file)) ohshite(_("read error in diversions [ii]"));
      else ohshit(_("unexpected EOF in diversions [ii]"));
    l= strlen(linebuf);
    if (l == 0) ohshit(_("fgets gave an empty string from diversions [ii]"));
    if (linebuf[--l] != '\n') ohshit(_("diversions file has too-long line or EOF [ii]"));
    linebuf[l]= 0;
    oicontest->useinstead= findnamenode(linebuf);
    
    if (!fgets(linebuf,sizeof(linebuf),file))
      if (ferror(file)) ohshite(_("read error in diversions [iii]"));
      else ohshit(_("unexpected EOF in diversions [iii]"));
    l= strlen(linebuf);
    if (l == 0) ohshit(_("fgets gave an empty string from diversions [iii]"));
    if (linebuf[--l] != '\n') ohshit(_("diversions file has too-long line or EOF [iii]"));
    linebuf[l]= 0;

    oicontest->pkg= oialtname->pkg=
      strcmp(linebuf,":") ? findpackage(linebuf) : 0;

    if (oialtname->camefrom->divert || oicontest->useinstead->divert)
      ohshit(_("conflicting diversions involving `%.250s' or `%.250s'"),
             oialtname->camefrom->name, oicontest->useinstead->name);

    oialtname->camefrom->divert= oicontest;
    oicontest->useinstead->divert= oialtname;

    oicontest->next= diversions;
    diversions= oicontest;
  }
  if (ferror(file)) ohshite(_("read error in diversions [i]"));

  diversionsfile= file;
  onerr_abort--;
}

/*** Data structures common to both in-core databases ***/

struct fileiterator {
  union {
    struct {
      struct filenamenode *current;
    } low;
    struct {
      struct filenamenode *namenode;
      int nbinn;
    } high;
  } u;
};

/*** Data structures for fast, large memory usage database ***/

#define BINS (1 << 13)
 /* This must always be a power of two.  If you change it
  * consider changing the per-character hashing factor (currently
  * 1785 = 137*13) too.
  */

static struct filenamenode *bins[BINS];

/*** Data structures for low-memory-footprint in-core files database ***/

struct fdirents {
  struct fdirents *more;
  struct { const char *component; struct fdirnode *go; } entries[1];
  /* first one has one entry, then two, then four, &c */
};

struct fdirnode {
  struct filenamenode *here;
  struct fdirents *contents;
};

static struct fdirnode fdirroot;
static struct filenamenode *allfiles;

struct filenamesblock {
  struct filenamesblock *next;
  char *data;
};

static struct filenamesblock *namesarea= 0;
static int namesarealeft= 0;

/*** Code for both.  This is rather hacky, sorry ... ***/

struct fileiterator *iterfilestart(void) {
  struct fileiterator *i;
  i= m_malloc(sizeof(struct fileiterator));
  switch (f_largemem) {
  case 1:
    i->u.high.namenode= 0;
    i->u.high.nbinn= 0;
    break;
  case -1:
    i->u.low.current= allfiles;
    break;
  default:
    internerr("iterfilestart no f_largemem");
  }
  return i;
}

struct filenamenode *iterfilenext(struct fileiterator *i) {
  struct filenamenode *r;
  switch (f_largemem) {
  case 1:
    while (!i->u.high.namenode) {
      if (i->u.high.nbinn >= BINS) return 0;
      i->u.high.namenode= bins[i->u.high.nbinn++];
    }
    r= i->u.high.namenode;
    i->u.high.namenode= r->next;
    break;
  case -1:
    if (!i->u.low.current) return 0;
    r= i->u.low.current;
    i->u.low.current= i->u.low.current->next;
    break;
  default:
    internerr("iterfilenext no f_largemem");
  }
  return r;
}

void iterfileend(struct fileiterator *i) {
  free(i);
}

void filesdbinit(void) {
  struct filenamenode *fnn;
#ifdef HAVE_SYSINFO
  struct sysinfo info;
#endif /* HAVE_SYSINFO */
  int i;

  if (!f_largemem) {
    f_largemem= -1;
#ifdef HAVE_SYSINFO
    if (!sysinfo(&info)) {
      if (info.freeram + (info.sharedram>>2) + (info.bufferram>>2) >= 4096*1024 ||
          info.totalram >= 6144)
        f_largemem= 1;
    }
#else /* HAVE_SYSINFO */
    f_largemem= 1;
#endif /* HAVE_SYSINFO */
  }
  
  switch (f_largemem) {
  case 1:
    for (i=0; i<BINS; i++)
      for (fnn= bins[i]; fnn; fnn= fnn->next) {
        fnn->flags= 0;
        fnn->oldhash= 0;
      }
    break;
  case -1:
    for (fnn= allfiles;
         fnn;
         fnn= fnn->next) {
      fnn->flags= 0;
      fnn->oldhash= 0;
    }
    break;
  default:
    internerr("filesdbinit no f_largemem");
  }    
}

static struct filenamenode *findnamenode_high(const char *name);
static struct filenamenode *findnamenode_low(const char *name);
  
struct filenamenode *findnamenode(const char *name) {
  switch (f_largemem) {
  case 1:
    return findnamenode_high(name);
  case -1:
    return findnamenode_low(name);
  default:
    internerr("findnamenode no f_largemem");
  }
}

/*** Code for low-memory-footprint in-core files database ***/
  
static struct filenamenode *findnamenode_low(const char *name) {
  struct fdirnode *traverse;
  struct fdirents *ents, **addto;
  const char *nameleft, *slash;
  char *p;
  struct filenamesblock *newblock;
  int n, i, nentrieshere, alloc;

  /* We skip initial slashes and ./ pairs, and add our own single leading slash. */
  name= skip_slash_dotslash(name);

  nameleft= name;
  traverse= &fdirroot;
  while (nameleft) {
    slash= strchr(nameleft,'/');
    if (slash) {
      n= (int)(slash-nameleft); slash++;
    } else {
      n= strlen(nameleft);
    }
    ents= traverse->contents; addto= &traverse->contents; i= 0; nentrieshere= 1;
    for (;;) {
      if (!ents) break;
      if (!ents->entries[i].component) break;
      if (!strncmp(ents->entries[i].component,nameleft,n) &&
          !ents->entries[i].component[n]) {
        break;
      }
      i++;
      if (i < nentrieshere) continue;
      addto= &ents->more;
      ents= ents->more;
      nentrieshere += nentrieshere;
      i=0;
    }
    if (!ents) {
      ents= nfmalloc(sizeof(struct fdirents) +
                     sizeof(ents->entries[0])*(nentrieshere-1));
      i=0;
      ents->entries[i].component= 0;
      ents->more= 0;
      *addto= ents;
    }
    if (!ents->entries[i].component) {
      p= nfmalloc(n+1);
      memcpy(p,nameleft,n); p[n]= 0;
      ents->entries[i].component= p;
      ents->entries[i].go= nfmalloc(sizeof(struct fdirnode));
      ents->entries[i].go->here= 0;
      ents->entries[i].go->contents= 0;
      if (i+1 < nentrieshere)
        ents->entries[i+1].component= 0;
    }
    traverse= ents->entries[i].go;
    nameleft= slash;
  }
  if (traverse->here) return traverse->here;

  traverse->here= nfmalloc(sizeof(struct filenamenode));
  traverse->here->packages= 0;
  traverse->here->flags= 0;
  traverse->here->divert= 0;

  n= strlen(name)+2;
  if (namesarealeft < n) {
    newblock= m_malloc(sizeof(struct filenamesblock));
    alloc= 256*1024;
    if (alloc<n) alloc= n;
    newblock->data= m_malloc(alloc);
    newblock->next= namesarea;
    namesarea= newblock;
    namesarealeft= alloc;
  }
  namesarealeft-= n;
  p= namesarea->data+namesarealeft;
  traverse->here->name= p; *p++= '/'; strcpy(p,name);

  traverse->here->next= allfiles;
  allfiles= traverse->here;
  nfiles++;
  return traverse->here;  
}

/*** Code for high memory usage fast database ***/

static int hash(const char *name) {
  int v= 0;
  while (*name) { v *= 1785; v += *name; name++; }
  return v;
}

struct filenamenode *findnamenode_high(const char *name) {
  struct filenamenode **pointerp, *newnode;

  /* We skip initial slashes and ./ pairs, and add our own single leading slash. */
  name= skip_slash_dotslash(name);

  pointerp= bins + (hash(name) & (BINS-1));
  while (*pointerp) {
    assert((*pointerp)->name[0] == '/');
    if (!strcmp((*pointerp)->name+1,name)) break;
    pointerp= &(*pointerp)->next;
  }
  if (*pointerp) return *pointerp;

  newnode= nfmalloc(sizeof(struct filenamenode));
  newnode->packages= 0;
  newnode->name= nfmalloc(strlen(name)+2);
  newnode->name[0]= '/'; strcpy(newnode->name+1,name);
  newnode->flags= 0;
  newnode->next= 0;
  newnode->divert= 0;
  *pointerp= newnode;
  nfiles++;

  return newnode;
}
