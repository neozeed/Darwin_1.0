/*
 * dpkg - main program for package management
 * archives.c - actions that process archive files, mainly unpack
 *
 * Copyright (C) 1994,1995 Ian Jackson <iwj10@cus.cam.ac.uk>
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

#undef _POSIX_SOURCE

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <utime.h>
#include <assert.h>
#include <time.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <dpkg.h>
#include <dpkg-db.h>
#include <dpkg-var.h>
#include <tarfn.h>
#include <myopt.h>

#include "filesdb.h"
#include "main.h"
#include "archives.h"
#include "dpkg-int.h"
#include "config.h"

#ifdef NeXT
#define lchown(x,y,z) 0
#endif

#define lchown(x,y,z) 0

int filesavespackage(struct fileinlist *file, struct pkginfo *pkgtobesaved,
                     struct pkginfo *pkgbeinginstalled) {
  struct pkginfo *divpkg, *thirdpkg;
  struct filepackages *packageslump;
  int i;
  
  debug(dbg_eachfiledetail,"filesavespackage file `%s' package %s",
        file->namenode->name,pkgtobesaved->name);
  /* A package can only be saved by a file or directory which is part
   * only of itself - it must be neither part of the new package being
   * installed nor part of any 3rd package (this is important so that
   * shared directories don't stop packages from disappearing).
   */
  /* Is the file in the package being installed ?  If so then it can't save.
   */
  if (file->namenode->flags & fnnf_new_inarchive) {
    debug(dbg_eachfiledetail,"filesavespackage ... in new archive -- no save");
    return 0;
  }
  /* If the file is a contended one and it's overridden by either
   * the package we're considering disappearing or the package
   * we're installing then they're not actually the same file, so
   * we can't disappear the package - it is saved by this file.
   */
  if (file->namenode->divert && file->namenode->divert->useinstead) {
    divpkg= file->namenode->divert->pkg;
    if (divpkg == pkgtobesaved || divpkg == pkgbeinginstalled) {
      debug(dbg_eachfiledetail,"filesavespackage ... diverted -- save!");
      return 1;
    }
  }
  /* Look for a 3rd package which can take over the file (in case
   * it's a directory which is shared by many packages.
   */
  for (packageslump= file->namenode->packages;
       packageslump;
       packageslump= packageslump->more) {
    for (i=0; i < PERFILEPACKAGESLUMP && packageslump->pkgs[i]; i++) {
      thirdpkg= packageslump->pkgs[i];
      debug(dbg_eachfiledetail, "filesavespackage ... also in %s",
            thirdpkg->name);
      /* Is this not the package being installed or the one being
       * checked for disappearance ?
       */
      if (thirdpkg == pkgbeinginstalled || thirdpkg == pkgtobesaved) continue;
      /* If !fileslistvalid then we've already disappeared this one, so
       * we shouldn't try to make it take over this shared directory.
       */
      debug(dbg_eachfiledetail,"filesavespackage ...  is 3rd package");

      if (!thirdpkg->clientdata->fileslistvalid) {
        debug(dbg_eachfiledetail,
              "process_archive ...  already disappeared !");
        continue;
      }
      /* We've found a package that can take this file. */
      debug(dbg_eachfiledetail, "filesavespackage ...  taken -- no save");
      return 0;
    }
  }
  debug(dbg_eachfiledetail, "filesavespackage ... not taken -- save !");
  return 1;
}

void cu_pathname(int argc, void **argv) {
  ensure_pathname_nonexisting((char*)(argv[0]));
} 

void cu_backendpipe(int argc, void **argv) {
  FILE *f= *(FILE**)argv[0];
  if (f) fclose(f);
} 

int tarfileread (void *ud, char *buf, int len) 
{
  struct tarcontext *tc = (struct tarcontext*) ud;
  int r = fread (buf, 1, len, tc->backendpipe);
  if ((r != len) && ferror (tc->backendpipe)) {
    ohshite (_("error reading from dpkg-deb pipe"));
  }
  return r;
}

int fnameidlu;
struct varbuf fnamevb;
struct varbuf fnametmpvb;
struct varbuf fnamenewvb;
struct packageinlist *deconfigure= 0;

static time_t currenttime;

static int does_replace(struct pkginfo *newpigp,
                          struct pkginfoperfile *newpifp,
                          struct pkginfo *oldpigp) {
  struct dependency *dep;
  
  debug(dbg_depcon,"does_replace new=%s old=%s (%s)",newpigp->name,
        oldpigp->name,versiondescribe(&oldpigp->installed.version,
                                      vdew_always));
  for (dep= newpifp->depends; dep; dep= dep->next) {
    if (dep->type != dep_replaces || dep->list->ed != oldpigp) continue;
    debug(dbg_depcondetail,"does_replace ... found old, version %s",
          versiondescribe(&dep->list->version,vdew_always));
    if (!versionsatisfied(&oldpigp->installed,dep->list)) continue;
    debug(dbg_depcon,"does_replace ... yes");
    return 1;
  }
  debug(dbg_depcon,"does_replace ... no");
  return 0;
}

static void newtarobject_utime(const char *path, struct TarInfo *ti) {
  struct utimbuf utb;
  utb.actime= currenttime;
  utb.modtime= ti->ModTime;
  if (utime(path,&utb))
    ohshite(_("error setting timestamps of `%.255s'"),ti->Name);
}

static void newtarobject_allmodes(const char *path, struct TarInfo *ti) {
  if (chown(path,ti->UserID,ti->GroupID))
    ohshite(_("error setting ownership of `%.255s'"),ti->Name);
  if (chmod(path,ti->Mode & ~S_IFMT))
    ohshite(_("error setting permissions of `%.255s'"),ti->Name);
  newtarobject_utime(path,ti);
}

void setupfnamevbs(const char *filename) {
  fnamevb.used= fnameidlu;
  varbufaddstr(&fnamevb,filename);
  varbufaddc(&fnamevb,0);

  fnametmpvb.used= fnameidlu;
  varbufaddstr(&fnametmpvb,filename);
  varbufaddstr(&fnametmpvb,DPKGTEMPEXT);
  varbufaddc(&fnametmpvb,0);

  fnamenewvb.used= fnameidlu;
  varbufaddstr(&fnamenewvb,filename);
  varbufaddstr(&fnamenewvb,DPKGNEWEXT);
  varbufaddc(&fnamenewvb,0);

  debug(dbg_eachfiledetail, "setupvnamevbs main=`%s' tmp=`%s' new=`%s'",
        fnamevb.buf, fnametmpvb.buf, fnamenewvb.buf);
}

int unlinkorrmdir(const char *filename) {
  /* Returns 0 on success or -1 on failure, just like unlink & rmdir */
  int r, e;
  
  if (!rmdir(filename)) {
    debug(dbg_eachfiledetail,"unlinkorrmdir `%s' rmdir OK",filename);
    return 0;
  }
  
  if (errno != ENOTDIR) {
    e= errno;
    debug(dbg_eachfiledetail,"unlinkorrmdir `%s' rmdir %s",filename,strerror(e));
    errno= e; return -1;
  }
  
  r= unlink(filename); e= errno;
  debug(dbg_eachfiledetail,"unlinkorrmdir `%s' unlink %s",
        filename, r ? strerror(e) : "OK");
  errno= e; return r;
}
     
int tarobject(struct TarInfo *ti) {
  static struct varbuf conffderefn, hardlinkfn, symlinkfn;
  const char *usename;
    
  struct tarcontext *tc= (struct tarcontext*)ti->UserData;
  int statr, fd, r, i, existingdirectory;
  struct stat stab, stabd;
  size_t sz, wsz;
  FILE *thefile;
  char databuf[TARBLKSZ];
  struct fileinlist *nifd;
  struct pkginfo *divpkg, *otherpkg;
  struct filepackages *packageslump;

  /* Append to list of files.
   * The trailing / put on the end of names in tarfiles has already
   * been stripped by TarExtractor (lib/tarfn.c).
   */
  nifd= m_malloc(sizeof(struct fileinlist));
  nifd->namenode= findnamenode(ti->Name);
  nifd->next= 0; *tc->newfilesp= nifd; tc->newfilesp= &nifd->next;
  nifd->namenode->flags |= fnnf_new_inarchive;

  debug(dbg_eachfile,
        "tarobject ti->Name=`%s' Mode=%lo owner=%u.%u Type=%d(%c)"
        " ti->LinkName=`%s' namenode=`%s' flags=%o instead=`%s'",
        ti->Name, (long)ti->Mode, (unsigned)ti->UserID, (unsigned)ti->GroupID, ti->Type,
        ti->Type == '\0' ? '_' :
        ti->Type >= '0' && ti->Type <= '6' ? "-hlcbdp"[ti->Type - '0'] : '?',
        ti->LinkName,
        nifd->namenode->name, nifd->namenode->flags,
        nifd->namenode->divert && nifd->namenode->divert->useinstead
        ? nifd->namenode->divert->useinstead->name : "<none>");

  if (nifd->namenode->divert && nifd->namenode->divert->camefrom) {
    divpkg= nifd->namenode->divert->pkg;
    forcibleerr(fc_overwritediverted,
                _("trying to overwrite `%.250s', which is the "
                "diverted version of `%.250s'%.10s%.100s%.10s"),
                nifd->namenode->name,
                nifd->namenode->divert->camefrom->name,
                divpkg ? _(" (package: ") : "",
                divpkg ? divpkg->name : "",
                divpkg ? ")" : "");
  }

  usename= namenodetouse(nifd->namenode,tc->pkg)->name + 1; /* Skip the leading `/' */

  if (nifd->namenode->flags & fnnf_new_conff) {
    /* If it's a conffile we have to extract it next to the installed
     * version (ie, we do the usual link-following).
     */
    if (conffderef(tc->pkg, &conffderefn, usename))
      usename= conffderefn.buf;
    debug(dbg_conff,"tarobject fnnf_new_conff deref=`%s'",usename);
  }
  
  setupfnamevbs(usename);

  statr= lstat(fnamevb.buf,&stab);
  if (statr) {
    /* The lstat failed. */
    if (errno != ENOENT && errno != ENOTDIR)
      ohshite(_("unable to stat `%.255s' (which I was about to install)"),ti->Name);
    /* OK, so it doesn't exist.
     * However, it's possible that we were in the middle of some other
     * backup/restore operation and were rudely interrupted.
     * So, we see if we have .dpkg-tmp, and if so we restore it.
     */
    if (rename(fnametmpvb.buf,fnamevb.buf)) {
      if (errno != ENOENT && errno != ENOTDIR)
        ohshite(_("unable to clean up mess surrounding `%.255s' before "
                "installing another version"),ti->Name);
      debug(dbg_eachfiledetail,"tarobject nonexistent");
    } else {
      debug(dbg_eachfiledetail,"tarobject restored tmp to main");
      statr= lstat(fnamevb.buf,&stab);
      if (statr) ohshite(_("unable to stat restored `%.255s' before installing"
                         " another version"), ti->Name);
    }
  } else {
    debug(dbg_eachfiledetail,"tarobject already exists");
  }

  /* Check to see if it's a directory or link to one and we don't need to
   * do anything.  This has to be done now so that we don't die due to
   * a file overwriting conflict.
   */
  existingdirectory= 0;
  switch (ti->Type) {
  case SymbolicLink:
    /* If it's already an existing directory, do nothing. */
    if (!statr && S_ISDIR(stab.st_mode)) {
      debug(dbg_eachfiledetail,"tarobject SymbolicLink exists as directory");
      existingdirectory= 1;
    }
    break;
  case Directory:
    /* If it's already an existing directory, do nothing. */
    if (!stat(fnamevb.buf,&stabd) && S_ISDIR(stabd.st_mode)) {
      debug(dbg_eachfiledetail,"tarobject Directory exists");
      existingdirectory= 1;
    }
    break;
  case NormalFile0: case NormalFile1:
  case CharacterDevice: case BlockDevice: case FIFO:
  case HardLink:
    break;
  default:
    ohshit(_("archive contained object `%.255s' of unknown type 0x%x"),ti->Name,ti->Type);
  }

  if (!existingdirectory) {
    for (packageslump= nifd->namenode->packages;
         packageslump;
         packageslump= packageslump->more) {
      for (i=0; i < PERFILEPACKAGESLUMP && packageslump->pkgs[i]; i++) {
        otherpkg= packageslump->pkgs[i];
        if (otherpkg == tc->pkg) continue;
        debug(dbg_eachfile, "tarobject ... found in %s",otherpkg->name);
        if (nifd->namenode->divert && nifd->namenode->divert->useinstead) {
          /* Right, so we may be diverting this file.  This makes the conflict
           * OK iff one of us is the diverting package (we don't need to
           * check for both being the diverting package, obviously).
           */
          divpkg= nifd->namenode->divert->pkg;
          debug(dbg_eachfile, "tarobject ... diverted, divpkg=%s",
                divpkg ? divpkg->name : "<none>");
          if (otherpkg == divpkg || tc->pkg == divpkg) continue;
        }
        /* Nope ?  Hmm, file conflict, perhaps.  Check Replaces. */
        if (otherpkg->clientdata->replacingfilesandsaid) continue;
        /* Is the package with the conflicting file in the `config files
         * only' state ?  If so it must be a config file and we can
         * silenty take it over.
         */
        if (otherpkg->status == stat_configfiles) continue;
        /* Perhaps we're removing a conflicting package ? */
        if (otherpkg->clientdata->istobe == itb_remove) continue;
        if (does_replace(tc->pkg,&tc->pkg->available,otherpkg)) {
          printf(_("Replacing files in old package `%s' ...\n"),otherpkg->name);
          otherpkg->clientdata->replacingfilesandsaid= 1;
        } else {
          if (S_ISDIR(stab.st_mode)) {
            forcibleerr(fc_overwritedir, _("trying to overwrite directory `%.250s' "
                        "in package %.250s with nondirectory"),
                        nifd->namenode->name,otherpkg->name);
          } else {
            forcibleerr(fc_overwrite,
                        _("trying to overwrite `%.250s', which is also in package %.250s"),
                        nifd->namenode->name,otherpkg->name);
          }
        }
      }
    }
  }
       
  /* Now, at this stage we want to make sure neither of .dpkg-new and .dpkg-tmp
   * are hanging around.
   */
  ensure_pathname_nonexisting(fnamenewvb.buf);
  ensure_pathname_nonexisting(fnametmpvb.buf);

  if (existingdirectory) return 0;

  /* Now we start to do things that we need to be able to undo
   * if something goes wrong.
   */
  push_cleanup(cu_installnew,~ehflag_normaltidy, 0,0, 1,(void*)nifd);

  /* Extract whatever it is as .dpkg-new ... */
  switch (ti->Type) {
  case NormalFile0: case NormalFile1:
    fd= open(fnamenewvb.buf, O_CREAT|O_EXCL|O_WRONLY,
             ti->Mode & (S_IRUSR|S_IRGRP|S_IROTH));
    if (fd < 0) ohshite(_("unable to create `%.255s'"),ti->Name);
    thefile= fdopen(fd,"w");
    if (!thefile) { close(fd); ohshite(_("unable to fdopen for `%.255s'"),ti->Name); }
    push_cleanup(cu_closefile,ehflag_bombout, 0,0, 1,(void*)thefile);
    debug(dbg_eachfiledetail,"tarobject NormalFile[01] open size=%lu",
          (unsigned long)ti->Size);
    for (sz= ti->Size; sz > 0; sz -= wsz) {
      wsz= sz > TARBLKSZ ? TARBLKSZ : sz;
      r= fread(databuf,1,TARBLKSZ,tc->backendpipe);
      if (r<TARBLKSZ) {
        if (ferror(tc->backendpipe)) {
          ohshite(_("error reading dpkg-deb during `%.255s'"),ti->Name);
        } else {
          errno= 0;
          return -1;
        }
      }
      if (fwrite(databuf,1,wsz,thefile) != wsz)
        ohshite(_("error writing to `%.255s'"),ti->Name);
    }
    if (fchown(fd,ti->UserID,ti->GroupID))
      ohshite(_("error setting ownership of `%.255s'"),ti->Name);
    /* We flush the stream here to avoid any future
     * writes, which will mask any setuid or setgid
     * attemps by fchmod below.
     */
    if (fflush(thefile) == EOF)
      ohshite(_("error flushing `%.255s'"),ti->Name);
    if (fchmod(fd,ti->Mode & ~S_IFMT))
      ohshite(_("error setting permissions of `%.255s'"),ti->Name);
    pop_cleanup(ehflag_normaltidy); /* thefile= fdopen(fd) */
    if (fclose(thefile))
      ohshite(_("error closing/writing `%.255s'"),ti->Name);
    newtarobject_utime(fnamenewvb.buf,ti);
    break;
  case FIFO:
    if (mkfifo(fnamenewvb.buf,ti->Mode & S_IFMT))
      ohshite(_("error creating pipe `%.255s'"),ti->Name);
    debug(dbg_eachfiledetail,"tarobject FIFO");
    newtarobject_allmodes(fnamenewvb.buf,ti);
    break;
  case CharacterDevice: case BlockDevice:
    if (mknod(fnamenewvb.buf,ti->Mode & S_IFMT,ti->Device))
      ohshite(_("error creating device `%.255s'"),ti->Name);
    debug(dbg_eachfiledetail,"tarobject CharacterDevice|BlockDevice");
    newtarobject_allmodes(fnamenewvb.buf,ti);
    break;
  case HardLink:
    varbufreset(&hardlinkfn);
    varbufaddstr(&hardlinkfn,instdir); varbufaddc(&hardlinkfn,'/');
    varbufaddstr(&hardlinkfn,ti->LinkName); varbufaddc(&hardlinkfn,0);
    if (link(hardlinkfn.buf,fnamenewvb.buf))
      ohshite(_("error creating hard link `%.255s'"),ti->Name);
    debug(dbg_eachfiledetail,"tarobject HardLink");
    newtarobject_allmodes(fnamenewvb.buf,ti);
    break;
  case SymbolicLink:
    /* We've already cheched for an existing directory. */
    if (symlink(ti->LinkName,fnamenewvb.buf))
      ohshite(_("error creating symbolic link `%.255s'"),ti->Name);
    debug(dbg_eachfiledetail,"tarobject SymbolicLink creating");
    if (lchown(fnamenewvb.buf,ti->UserID,ti->GroupID))
      ohshite(_("error setting ownership of symlink `%.255s'"),ti->Name);
    break;
  case Directory:
    /* We've already checked for an existing directory. */
    if (mkdir(fnamenewvb.buf,
              ti->Mode & (S_IRUSR|S_IRGRP|S_IROTH | S_IXUSR|S_IXGRP|S_IXOTH)))
      ohshite(_("error creating directory `%.255s'"),ti->Name);
    debug(dbg_eachfiledetail,"tarobject Directory creating");
    newtarobject_allmodes(fnamenewvb.buf,ti);
    break;
  default:
    internerr("bad tar type, but already checked");
  }
  /*
   * Now we have extracted the new object in .dpkg-new (or, if the
   * file already exists as a directory and we were trying to extract
   * a directory or symlink, we returned earlier, so we don't need
   * to worry about that here).
   */

  /* First, check to see if it's a conffile.  If so we don't install
   * it now - we leave it in .dpkg-new for --configure to take care of
   */
  if (nifd->namenode->flags & fnnf_new_conff) {
    debug(dbg_conffdetail,"tarobject conffile extracted");
    nifd->namenode->flags |= fnnf_elide_other_lists;
    return 0;
  }

  /* Now we install it.  If we can do an atomic overwrite we do so.
   * If not we move aside the old file and then install the new.
   * The backup file will be deleted later.
   */
  if (statr) { /* Don't try to back it up if it didn't exist. */
    debug(dbg_eachfiledetail,"tarobject new - no backup");
  } else {
    if (ti->Type == Directory || S_ISDIR(stab.st_mode)) {
      /* One of the two is a directory - can't do atomic install. */
      debug(dbg_eachfiledetail,"tarobject directory, nonatomic");
      nifd->namenode->flags |= fnnf_no_atomic_overwrite;
      if (rename(fnamevb.buf,fnametmpvb.buf))
        ohshite(_("unable to move aside `%.255s' to install new version"),ti->Name);
    } else if (S_ISLNK(stab.st_mode)) {
      /* We can't make a symlink with two hardlinks, so we'll have to copy it.
       * (Pretend that making a copy of a symlink is the same as linking to it.)
       */
      varbufreset(&symlinkfn);
      do {
        varbufextend(&symlinkfn);
        r= readlink(fnamevb.buf,symlinkfn.buf,symlinkfn.size);
        if (r<0) ohshite("unable to read link `%.255s'",ti->Name);
      } while (r == symlinkfn.size);
      symlinkfn.used= r; varbufaddc(&symlinkfn,0);
      if (symlink(symlinkfn.buf,fnametmpvb.buf))
        ohshite(_("unable to make backup symlink for `%.255s'"),ti->Name);
      if (lchown(fnametmpvb.buf,stab.st_uid,stab.st_gid))
        ohshite(_("unable to lchown backup symlink for `%.255s'"),ti->Name);
    } else {
      debug(dbg_eachfiledetail,"tarobject nondirectory, `link' backup");
      if (link(fnamevb.buf,fnametmpvb.buf))
        ohshite(_("unable to make backup link of `%.255s' before installing new version"),
                ti->Name);
    }
  }

  if (rename(fnamenewvb.buf,fnamevb.buf))
    ohshite(_("unable to install new version of `%.255s'"),ti->Name);

  nifd->namenode->flags |= fnnf_elide_other_lists;

  debug(dbg_eachfiledetail,"tarobject done and installed");
  return 0;
}

static int try_remove_can(struct deppossi *pdep,
                          struct pkginfo *fixbyrm,
                          const char *why) {
  struct packageinlist *newdeconf;
  
  if (force_depends(pdep)) {
    fprintf(stderr, _("dpkg: warning - "
            "ignoring dependency problem with removal of %s:\n%s"),
            fixbyrm->name, why);
    return 1;
  } else if (f_autodeconf) {
    if (pdep->up->up->installed.essential) {
      if (fc_removeessential) {
        fprintf(stderr, _("dpkg: warning - considering deconfiguration of essential\n"
                " package %s, to enable removal of %s.\n"),
                pdep->up->up->name,fixbyrm->name);
      } else {
        fprintf(stderr, _("dpkg: no, %s is essential, will not deconfigure\n"
                " it in order to enable removal of %s.\n"),
                pdep->up->up->name,fixbyrm->name);
        return 0;
      }
    }
    pdep->up->up->clientdata->istobe= itb_deconfigure;
    newdeconf= m_malloc(sizeof(struct packageinlist));
    newdeconf->next= deconfigure;
    newdeconf->pkg= pdep->up->up;
    deconfigure= newdeconf;
    return 1;
  } else {
    fprintf(stderr, _("dpkg: no, cannot remove %s (--auto-deconfigure will help):\n%s"),
            fixbyrm->name, why);
    return 0;
  }
}

void check_conflict(struct dependency *dep, struct pkginfo *pkg,
                    const char *pfilename, struct pkginfo **conflictorp) {
  struct pkginfo *fixbyrm;
  struct deppossi *pdep, flagdeppossi;
  struct varbuf conflictwhy, removalwhy;
  struct dependency *providecheck;
  
  varbufinit(&conflictwhy);
  varbufinit(&removalwhy);

  fixbyrm= 0;
  if (depisok(dep, &conflictwhy, *conflictorp ? 0 : &fixbyrm, 0)) {
    varbuffree(&conflictwhy);
    varbuffree(&removalwhy);
    return;
  }
  if (fixbyrm) {
    ensure_package_clientdata(fixbyrm);
    if (fixbyrm->clientdata->istobe == itb_installnew) {
      fixbyrm= dep->up;
      ensure_package_clientdata(fixbyrm);
    }
    if (((pkg->available.essential && fixbyrm->installed.essential) ||
         (((fixbyrm->want != want_install && fixbyrm->want != want_hold) ||
           does_replace(pkg,&pkg->available,fixbyrm)) &&
          (!fixbyrm->installed.essential || fc_removeessential)))) {
      assert(fixbyrm->clientdata->istobe == itb_normal);
      fixbyrm->clientdata->istobe= itb_remove;
      fprintf(stderr, _("dpkg: considering removing %s in favour of %s ...\n"),
              fixbyrm->name, pkg->name);
      if (fixbyrm->status != stat_installed) {
        fprintf(stderr,
                _("%s is not properly installed - ignoring any dependencies on it.\n"),
                fixbyrm->name);
        pdep= 0;
      } else {
        for (pdep= fixbyrm->installed.depended;
             pdep;
             pdep= pdep->nextrev) {
          if (pdep->up->type != dep_depends && pdep->up->type != dep_predepends)
            continue;
          if (depisok(pdep->up, &removalwhy, 0,0)) continue;
          varbufaddc(&removalwhy,0);
          if (!try_remove_can(pdep,fixbyrm,removalwhy.buf))
            break;
        }
        if (!pdep) {
          /* If we haven't found a reason not to yet, let's look some more. */
          for (providecheck= fixbyrm->installed.depends;
               providecheck;
               providecheck= providecheck->next) {
            if (providecheck->type != dep_provides) continue;
            for (pdep= providecheck->list->ed->installed.depended;
                 pdep;
                 pdep= pdep->nextrev) {
              if (pdep->up->type != dep_depends && pdep->up->type != dep_predepends)
                continue;
              if (depisok(pdep->up, &removalwhy, 0,0)) continue;
              varbufaddc(&removalwhy,0);
              fprintf(stderr, _("dpkg"
                      ": may have trouble removing %s, as it provides %s ...\n"),
                      fixbyrm->name, providecheck->list->ed->name);
              if (!try_remove_can(pdep,fixbyrm,removalwhy.buf))
                goto break_from_both_loops_at_once;
            }
          }
        break_from_both_loops_at_once:;
        }
      }
      if (!pdep && skip_due_to_hold(fixbyrm)) {
        pdep= &flagdeppossi;
      }
      if (!pdep && (fixbyrm->eflag & eflagf_reinstreq)) {
        if (fc_removereinstreq) {
          fprintf(stderr, _("dpkg: package %s requires reinstallation, but will"
                  " remove anyway as you request.\n"), fixbyrm->name);
        } else {
          fprintf(stderr, _("dpkg: package %s requires reinstallation, "
                  "will not remove.\n"), fixbyrm->name);
          pdep= &flagdeppossi;
        }
      }
      if (!pdep) {
        /* This conflict is OK - we'll remove the conflictor. */
        *conflictorp= fixbyrm;
        varbuffree(&conflictwhy); varbuffree(&removalwhy);
        fprintf(stderr, _("dpkg: yes, will remove %s in favour of %s.\n"),
                fixbyrm->name, pkg->name);
        return;
      }
      fixbyrm->clientdata->istobe= itb_normal; /* put it back */
    }
  }
  varbufaddc(&conflictwhy,0);
  fprintf(stderr, _("dpkg: regarding %s containing %s:\n%s"),
          pfilename, pkg->name, conflictwhy.buf);
  if (!force_conflicts(dep->list))
    ohshit(_("conflicting packages - not installing %.250s"),pkg->name);
  fprintf(stderr, _("dpkg: warning - ignoring conflict, may proceed anyway !\n"));
  varbuffree(&conflictwhy);
  
  return;
}

void cu_cidir(int argc, void **argv) {
  char *cidir= (char*)argv[0];
  char *cidirrest= (char*)argv[1];
  cidirrest[-1]= 0;
  ensure_pathname_nonexisting(cidir);
}  

void cu_fileslist(int argc, void **argv) {
  struct fileinlist **headp= (struct fileinlist**)argv[0];
  struct fileinlist *current, *next;
  for (current= *headp; current; current= next) {
    next= current->next;
    free(current);
  }
}  

void archivefiles(const char *const *argv) {
  const char *volatile thisarg;
  const char *const *volatile argp;
  jmp_buf ejbuf;
  int pi[2], fc, nfiles, c, i;
  FILE *pf;
  static struct varbuf findoutput;
  const char **arglist;
  char *p;

  checkpath();
  
  if (f_recursive) {
    
    if (!*argv)
      badusage(_("--%s --recursive needs at least one path argument"),cipaction->olong);
    
    m_pipe(pi);
    if (!(fc= m_fork())) {
      const char *const *ap;
      int i;
      m_dup2(pi[1],1); close(pi[0]);
      for (i=0, ap=argv; *ap; ap++, i++);
      arglist= m_malloc(sizeof(char*)*(i+15));
      arglist[0]= FIND;
      for (i=1, ap=argv; *ap; ap++, i++) {
        if (strchr(FIND_EXPRSTARTCHARS,(*ap)[0])) {
          char *a;
          a= m_malloc(strlen(*ap)+10);
          strcpy(a,"./");
          strcat(a,*ap);
          arglist[i]= a;
        } else {
          arglist[i]= *ap;
        }
      }
      arglist[i++]= "-follow"; /*  When editing these, make sure that     */
      arglist[i++]= "-name";   /*  arglist is mallocd big enough, above.  */
      arglist[i++]= ARCHIVE_FILENAME_PATTERN;
      arglist[i++]= "-type";
      arglist[i++]= "f";
      arglist[i++]= "-print0";
      arglist[i++]= 0;
      execvp(FIND, (char* const*)arglist);
      ohshite(_("failed to exec find for --recursive"));
    }

    nfiles= 0;
    pf= fdopen(pi[0],"r");  if (!pf) ohshite(_("failed to fdopen find's pipe"));
    close(pi[1]);
    varbufreset(&findoutput);
    while ((c= fgetc(pf)) != EOF) {
      varbufaddc(&findoutput,c);
      if (!c) nfiles++;
    }
    if (ferror(pf)) ohshite(_("error reading find's pipe"));
    if (fclose(pf)) ohshite(_("error closing find's pipe"));
    waitsubproc(fc,"find",0);

    if (!nfiles) ohshit(_("searched, but found no packages (files matching "
                        "*.deb"));

    varbufaddc(&findoutput,0);
    varbufaddc(&findoutput,0);
    
    arglist= m_malloc(sizeof(char*)*(nfiles+1));
    p= findoutput.buf; i=0;
    while (*p) {
      arglist[i++]= p;
      while ((c= *p++) != 0);
    }
    arglist[i]= 0;
    argp= arglist;

  } else {

    if (!*argv) badusage(_("--%s needs at least one package archive file argument"),
                         cipaction->olong);
    argp= argv;
    
  }

  modstatdb_init(admindir,
                 f_noact ?                     msdbrw_readonly
               : cipaction->arg == act_avail ? msdbrw_write
               : fc_nonroot ?                  msdbrw_write
               :                               msdbrw_needsuperuser);

  currenttime= time(0);

  varbufaddstr(&fnamevb,instdir); varbufaddc(&fnamevb,'/');
  varbufaddstr(&fnametmpvb,instdir); varbufaddc(&fnametmpvb,'/');
  varbufaddstr(&fnamenewvb,instdir); varbufaddc(&fnamenewvb,'/');
  fnameidlu= fnamevb.used;

  ensure_diversions();
  
  while ((thisarg= *argp++) != 0) {
    if (setjmp(ejbuf)) {
      error_unwind(ehflag_bombout);
      if (onerr_abort > 0) break;
      continue;
    }
    push_error_handler(&ejbuf,print_error_perpackage,thisarg);
    process_archive(thisarg);
    onerr_abort++;
    if (ferror(stdout)) werr("stdout");
    if (ferror(stderr)) werr("stderr");
    onerr_abort--;
    set_error_display(0,0);
    error_unwind(ehflag_normaltidy);
  }

  switch (cipaction->arg) {
  case act_install:
  case act_configure:
  case act_remove:
  case act_purge:
    process_queue();
  case act_unpack:
  case act_avail:
    break;
  default:
    internerr(_("unknown action"));
  }

  modstatdb_shutdown();
}
