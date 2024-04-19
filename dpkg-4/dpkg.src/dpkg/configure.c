/*
 * dpkg - main program for package management
 * configure.c - configure packages
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

#undef _POSIX_SOURCE

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <ctype.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <sys/wait.h>

#include <dpkg.h>
#include <dpkg-db.h>
#include <dpkg-var.h>

#include "filesdb.h"
#include "main.h"
#include "dpkg-int.h"
#include "config.h"

int conffoptcells[2][2]= { 
  /* int conffoptcells[2] {* 1= user edited *}
     [2] {* 1= distributor edited *} = */
  /* dist not */     /* dist edited */
  /* user did not edit */ 
  {     cfo_keep,           cfo_install    },
  /* user did edit     */
  {     cfo_keep,         cfo_prompt_keep  }
};

static void md5hash(struct pkginfo *pkg, char hashbuf[MD5HASHLEN+1], const char *fn);

void deferred_configure(struct pkginfo *pkg) {
  /* The algorithm for deciding what to configure first is as follows:
   * Loop through all packages doing a `try 1' until we've been round
   * and nothing has been done, then do `try 2' and `try 3' likewise.
   * The incrementing of `dependtry' is done by process_queue().
   * Try 1:
   *  Are all dependencies of this package done ?  If so, do it.
   *  Are any of the dependencies missing or the wrong version ?
   *   If so, abort (unless --force-depends, in which case defer)
   *  Will we need to configure a package we weren't given as an
   *   argument ?  If so, abort - except if --force-configure-any,
   *   in which case we add the package to the argument list.
   *  If none of the above, defer the package.
   * Try 2:
   *  Find a cycle and break it (see above).
   *  Do as for try 1.
   * Try 3 (only if --force-depends-version).
   *  Same as for try 2, but don't mind version number in dependencies.
   * Try 4 (only if --force-depends).
   *  Do anyway.
   */
  struct varbuf aemsgs, cdr, cdr2;
  char *cdr2rest;
  int ok, r, useredited, distedited, c, cc, status, c1;
  struct conffile *conff;
  char currenthash[MD5HASHLEN+1], newdisthash[MD5HASHLEN+1];
  struct stat stab;
  enum conffopt what;
  const char *s;
  char* possiblekeys;

  /* y=yes i=install n=no o=?? */
  possiblekeys = _("yino");
#define key_yes possiblekeys[0]
#define key_install possiblekeys[1]
#define key_no possiblekeys[2]
#define key_o possiblekeys[3]
#define key_suspend possiblekeys[4]
  
  if (pkg->status == stat_notinstalled)
    ohshit(_("no package named `%s' is installed, cannot configure"),pkg->name);
  if (pkg->status != stat_unpacked && pkg->status != stat_halfconfigured)
    ohshit(_("package %.250s is not ready for configuration\n"
           " cannot configure (current status `%.250s')"),
           pkg->name, statusinfos[pkg->status].name);
  
  if (dependtry > 1) { if (findbreakcycle(pkg,0)) sincenothing= 0; }

  varbufinit(&aemsgs);
  ok= dependencies_ok(pkg,0,&aemsgs);
  if (ok == 1) {
    varbuffree(&aemsgs);
    pkg->clientdata->istobe= itb_installnew;
    add_to_queue(pkg);
    return;
  } else if (ok == 0) {
    sincenothing= 0;
    varbufaddc(&aemsgs,0);
    fprintf(stderr,
            _("dpkg: dependency problems prevent configuration of %s:\n%s"),
            pkg->name, aemsgs.buf);
    varbuffree(&aemsgs);
    ohshit(_("dependency problems - leaving unconfigured"));
  } else if (aemsgs.used) {
    varbufaddc(&aemsgs,0);
    fprintf(stderr,
            _("dpkg: %s: dependency problems, but configuring anyway as you request:\n%s"),
            pkg->name, aemsgs.buf);
  }
  varbuffree(&aemsgs);
  sincenothing= 0;

  if (pkg->eflag & eflagf_reinstreq)
    forcibleerr(fc_removereinstreq,
                _("Package is in a very bad inconsistent state - you should\n"
                " reinstall it before attempting configuration."));

  printf(_("Setting up %s (%s) ...\n"),pkg->name,
         versiondescribe(&pkg->installed.version,vdew_never));

  if (f_noact) {
    pkg->status= stat_installed;
    pkg->clientdata->istobe= itb_normal;
    return;
  }
  
  if (pkg->status == stat_unpacked) {
    debug(dbg_general,"deferred_configure updating conffiles");

    /* This will not do at all the right thing with overridden conffiles
     * or conffiles that are the `target' of an override; all the references
     * here would be to the `contested' filename, and in any case there'd
     * only be one hash for both `versions' of the conffile.
     *
     * Overriding conffiles is a silly thing to do anyway :-).
     */

    modstatdb_note(pkg);

    /* On entry, the `new' version of each conffile has been
     * unpacked as *.dpkg-new, and the `installed' version is
     * as-yet untouched in `*'.  The hash of the `old distributed'
     * version is in the conffiles data for the package.
     * If `*.dpkg-new' no longer exists we assume that we've already
     * processed this one.
     */
    varbufinit(&cdr);
    varbufinit(&cdr2);
    for (conff= pkg->installed.conffiles; conff; conff= conff->next) {
      r= conffderef(pkg, &cdr, conff->name);
      if (r == -1) {
        conff->hash= nfstrsave("-");
        continue;
      }
      md5hash(pkg,currenthash,cdr.buf);

      varbufreset(&cdr2);
      varbufaddstr(&cdr2,cdr.buf);
      cdr2.used+=50; varbufaddc(&cdr2,0); cdr2rest= cdr2.buf+strlen(cdr.buf);
      /* From now on we can just strcpy(cdr2rest,extension); */

      strcpy(cdr2rest,DPKGNEWEXT);
      /* If the .dpkg-new file is no longer there, ignore this one. */
      if (lstat(cdr2.buf,&stab)) {
        if (errno == ENOENT) continue;
        ohshite(_("unable to stat new dist conffile `%.250s'"),cdr2.buf);
      }
      md5hash(pkg,newdisthash,cdr2.buf);

      /* Copy the permissions from the installed version to the new
       * distributed version.
       */
      if (!stat(cdr.buf,&stab)) {
        if (chown(cdr2.buf,stab.st_uid,stab.st_gid))
          ohshite(_("unable to change ownership of new dist conffile `%.250s'"),cdr2.buf);
        if (chmod(cdr2.buf,stab.st_mode & 07777))
          if (errno != ENOENT)
            ohshite(_("unable to set mode of new dist conffile `%.250s'"),cdr2.buf);
      } else {
        if (errno != ENOENT)
          ohshite(_("unable to stat current installed conffile `%.250s'"),cdr.buf);
      }

      if (!strcmp(currenthash,newdisthash)) {
        /* They're both the same so there's no point asking silly questions. */
        useredited= -1;
        distedited= -1;
        what= cfo_identical;
      } else if (!strcmp(conff->hash,NEWCONFFILEFLAG)) {
        if (!strcmp(currenthash,NONEXISTENTFLAG)) {
          what= cfo_newconff;
          useredited= -1;
          distedited= -1;
        } else {
          useredited= 1;
          distedited= 1;
          what= conffoptcells[useredited][distedited] | cfof_isnew;
        }
      } else {
        useredited= strcmp(conff->hash,currenthash) != 0;
        distedited= strcmp(conff->hash,newdisthash) != 0;
        what= conffoptcells[useredited][distedited];
      }

      debug(dbg_conff,
            "deferred_configure `%s' (= `%s') useredited=%d distedited=%d what=%o",
            conff->name, cdr.buf, useredited, distedited, what);
      
      if (what & cfof_prompt) {

        do {
          
          fprintf(stderr, _("\nConfiguration file `%s'"), conff->name);
          if (strcmp(conff->name,cdr.buf))
            fprintf(stderr,_(" (actually `%s')"),cdr.buf);

          if (cfof_isnew) {

            fprintf(stderr,
                    _("\n"
                    " ==> File on system created by you or by a script.\n"
                    " ==> File also in package provided by package maintainer.\n"));

          } else {
            
            fprintf(stderr, useredited ?
                    _("\n ==> Modified (by you or by a script) since installation.\n") :
                    _("\n     Not modified since installation.\n"));

            fprintf(stderr, distedited ?
                    _(" ==> Package distributor has shipped an updated version.\n") :
                    _("     Version in package is the same as at last installation.\n"));

          }
          
          fprintf(stderr,
                  _("   What would you like to do about it ?  Your options are:\n"
		    "    %c or %c  : install the package maintainer's version\n"
		    "    %c or %c  : keep your currently-installed version\n"
		    "      Z     : background this process to examine the situation\n"),
		  toupper(key_yes), toupper(key_install), toupper(key_no), toupper(key_o));

          if (what & cfof_keep)
            fprintf(stderr, _(" The default action is to keep your current version.\n"));
          else if (what & cfof_install)
            fprintf(stderr, _(" The default action is to install the new version.\n"));

          s= strrchr(conff->name,'/');
          if (!s || !*++s) s= conff->name;
          fprintf(stderr, "*** %s (%c/%c/%c/%c/Z) %s ? ",
                  s,
		  toupper(key_yes), toupper(key_install), toupper(key_no), toupper(key_o),
		  /* L10n: keep consistent with "yino" msgid */
                  (what & cfof_keep) ? _("[default=N]") :
		  /* L10n: !keep consistent with "yino" msgid */
                  (what & cfof_install) ? _("[default=Y]") : _("[no default]"));

          if (ferror(stderr))
            ohshite(_("error writing to stderr, discovered before conffile prompt"));

          cc= 0;
          while ((c= getchar()) != EOF && c != '\n')
            if (!isspace(c) && !cc) cc= tolower(c);

          if (c == EOF) {
            if (ferror(stdin)) ohshite(_("read error on stdin at conffile prompt"));
            ohshit(_("EOF on stdin at conffile prompt"));
          }

          if (!cc) {
            if (what & cfof_keep) { cc= key_no; break; }
            else if (what & cfof_install) { cc= key_yes; break; }
          }
          
          /* fixme: say something if silently not install */

          if (cc == 'z') {

            strcpy(cdr2rest, DPKGNEWEXT);
            
            fprintf(stderr,
          _("Your currently installed version of the file is in:\n"
          " %s\n"
          "The version contained in the new version of the package is in:\n"
          " %s\n"
          "If you decide to take care of the update yourself, perhaps by editing\n"
          " the installed version, you should choose `%c' when you return, so that\n"
          " I do not mess up your careful work.\n"),
                    cdr.buf, cdr2.buf, toupper(key_no));

            s= getenv(NOJOBCTRLSTOPENV);
            if (s && *s) {
              fputs(_("Type `exit' when you're done.\n"),stderr);
              if (!(c1= m_fork())) {
                s= getenv(SHELLENV);
                if (!s || !*s) s= DEFAULTSHELL;
                execlp(s,s,"-i",(char*)0);
                ohshite(_("failed to exec shell (%.250s)"),s);
              }
              while ((r= waitpid(c1,&status,0)) == -1 && errno == EINTR);
              if (r != c1) { onerr_abort++; ohshite(_("wait for shell failed")); }
            } else {
              fputs(_("Don't forget to foreground (`fg') this "
                    "process when you're done !\n"),stderr);
              kill(-getpgid(0),SIGTSTP);
            }
          }

        } while (!strchr(possiblekeys,cc));

        if ((cc = key_yes) || (cc = key_install))
	    what= cfof_install | cfof_backup;
        else if ((cc = key_no) || (cc = key_o))
	    what= cfof_keep    | cfof_backup;
	else 
	    internerr("unknown response");

      }

      switch (what & ~cfof_isnew) {
      case cfo_keep | cfof_backup:
        strcpy(cdr2rest,DPKGOLDEXT);
        if (unlink(cdr2.buf) && errno != ENOENT)
          fprintf(stderr,
                  _("dpkg: %s: warning - failed to remove old backup `%.250s': %s\n"),
                  pkg->name, cdr2.buf, strerror(errno));
        cdr.used--;
        varbufaddstr(&cdr,DPKGDISTEXT);
        varbufaddc(&cdr,0);
        strcpy(cdr2rest,DPKGNEWEXT);
        if (rename(cdr2.buf,cdr.buf))
          fprintf(stderr,
                  _("dpkg: %s: warning - failed to rename `%.250s' to `%.250s': %s\n"),
                  pkg->name, cdr2.buf, cdr.buf, strerror(errno));
        break;

      case cfo_keep:
        strcpy(cdr2rest,DPKGNEWEXT);
        if (unlink(cdr2.buf))
          fprintf(stderr,
                  _("dpkg: %s: warning - failed to remove `%.250s': %s\n"),
                  pkg->name, cdr2.buf, strerror(errno));
        break;

      case cfo_install | cfof_backup:
        strcpy(cdr2rest,DPKGDISTEXT);
        if (unlink(cdr2.buf) && errno != ENOENT)
          fprintf(stderr,
                  _("dpkg: %s: warning - failed to remove old distrib version `%.250s': %s\n"),
                  pkg->name, cdr2.buf, strerror(errno));
        strcpy(cdr2rest,DPKGOLDEXT);
        if (unlink(cdr2.buf) && errno != ENOENT)
          fprintf(stderr,
                  _("dpkg: %s: warning - failed to remove `%.250s' (before overwrite): %s\n"),
                  pkg->name, cdr2.buf, strerror(errno));
        if (link(cdr.buf,cdr2.buf))
          fprintf(stderr,
                  _("dpkg: %s: warning - failed to link `%.250s' to `%.250s': %s\n"),
                  pkg->name, cdr.buf, cdr2.buf, strerror(errno));
        /* fall through */
      case cfo_install:
        printf(_("Installing new version of config file %s ...\n"),conff->name);
      case cfo_newconff:
        strcpy(cdr2rest,DPKGNEWEXT);
        if (rename(cdr2.buf,cdr.buf))
          ohshite(_("unable to install `%.250s' as `%.250s'"),cdr2.buf,cdr.buf);
        break;

      default:
        internerr("unknown what");
      }

      conff->hash= nfstrsave(newdisthash);
      modstatdb_note(pkg);
      
    } /* for (conff= ... */
    varbuffree(&cdr);
    varbuffree(&cdr2);

    pkg->status= stat_halfconfigured;
  }

  assert(pkg->status == stat_halfconfigured);

  modstatdb_note(pkg);

  /* submsg: maintainer_script_installed */
  if (maintainer_script_installed(pkg, POSTINSTFILE, _("post-installation"),
                                  "configure",
                                  informativeversion(&pkg->configversion)
                                  ? versiondescribe(&pkg->configversion,
                                                  vdew_nonambig)
                                  : "",
                                  (char*)0))
    putchar('\n');

  pkg->status= stat_installed;
  pkg->eflag= eflagv_ok;
  modstatdb_note(pkg);

}

int conffderef(struct pkginfo *pkg, struct varbuf *result, const char *in) {
  /* returns 0 if all OK, -1 if some kind of error. */
  static char *linkreadbuf= 0;
  static int linkreadbufsize= 0;
  struct stat stab;
  int r, need;
  int loopprotect;

  varbufreset(result);
  varbufaddstr(result,instdir);
  if (*in != '/') varbufaddc(result,'/');
  varbufaddstr(result,in);
  varbufaddc(result,0);

  loopprotect= 0;
  
  for (;;) {
    debug(dbg_conffdetail,"conffderef in=`%s' current working=`%s'", in, result->buf);
    if (lstat(result->buf,&stab)) {
      if (errno != ENOENT)
        fprintf(stderr, _("dpkg: %s: warning - unable to stat config file `%s'\n"
                " (= `%s'): %s\n"),
                pkg->name, in, result->buf, strerror(errno));
      debug(dbg_conffdetail,"conffderef nonexistent");
      return 0;
    } else if (S_ISREG(stab.st_mode)) {
      debug(dbg_conff,"conffderef in=`%s' result=`%s'", in, result->buf);
      return 0;
    } else if (S_ISLNK(stab.st_mode)) {
      debug(dbg_conffdetail,"conffderef symlink loopprotect=%d",loopprotect);
      if (loopprotect++ >= 25) {
        fprintf(stderr, _("dpkg: %s: warning - config file `%s' is a circular link\n"
                " (= `%s')\n"), pkg->name, in, result->buf);
        return -1;
      }
      need= 255;
      for (;;) {
        if (need > linkreadbufsize) {
          linkreadbuf= m_realloc(linkreadbuf,need);
          linkreadbufsize= need;
          debug(dbg_conffdetail,"conffderef readlink realloc(%d)=%p",need,linkreadbuf);
        }
        r= readlink(result->buf,linkreadbuf,linkreadbufsize-1);
        if (r < 0) {
          fprintf(stderr, _("dpkg: %s: warning - unable to readlink conffile `%s'\n"
                  " (= `%s'): %s\n"),
                  pkg->name, in, result->buf, strerror(errno));
          return -1;
        }
        debug(dbg_conffdetail,"conffderef readlink gave %d, `%.*s'",
              r, r>0 ? r : 0, linkreadbuf);
        if (r < linkreadbufsize-1) break;
        need= r<<2;
      }
      linkreadbuf[r]= 0;
      if (linkreadbuf[0] == '/') {
        varbufreset(result);
        varbufaddstr(result,instdir);
        debug(dbg_conffdetail,"conffderef readlink absolute");
      } else {
        for (r=result->used-2; r>0 && result->buf[r] != '/'; r--);
        if (r < 0) {
          fprintf(stderr,
                  _("dpkg: %s: warning - conffile `%.250s' resolves to degenerate filename\n"
                  " (`%s' is a symlink to `%s')\n"),
                  pkg->name, in, result->buf, linkreadbuf);
          return -1;
        }
        if (result->buf[r] == '/') r++;
        result->used= r;
        debug(dbg_conffdetail,"conffderef readlink relative to `%.*s'",
              result->used, result->buf);
      }
      varbufaddstr(result,linkreadbuf);
      varbufaddc(result,0);
    } else {
      fprintf(stderr, _("dpkg: %s: warning - conffile `%.250s' is not a plain"
              " file or symlink (= `%s')\n"),
              pkg->name, in, result->buf);
      return -1;
    }
  }
}
    
static void md5hash(struct pkginfo *pkg, char hashbuf[33], const char *fn) {
  static int fd, p1[2];
  FILE *file;
  pid_t c1;
  int ok;
  
  fd= open(fn,O_RDONLY);
  if (fd >= 0) {
    push_cleanup(cu_closefd,ehflag_bombout, 0,0, 1,(void*)&fd);
    m_pipe(p1);
    push_cleanup(cu_closepipe,ehflag_bombout, 0,0, 1,(void*)&p1[0]);
    if (!(c1= m_fork())) {
      m_dup2(fd,0); m_dup2(p1[1],1); close(p1[0]);
      execlp(MD5SUM,MD5SUM,(char*)0);
      ohshite(_("failed to exec md5sum"));
    }
    close(p1[1]); close(fd);
    file= fdopen(p1[0],"r");
    push_cleanup(cu_closefile,ehflag_bombout, 0,0, 1,(void*)file);
    if (!file) ohshite(_("unable to fdopen for md5sum of `%.250s'"),fn);
    ok= 1;
    memset(hashbuf,0,33);
    if (fread(hashbuf,1,32,file) != 32) ok=0;
    for (;;) {
      int c = getc (file);
      if (c == EOF) { break; }
      if ((c != '\n') && (c != ' ') && (c != '-')) { ok = 0; }
    }
    waitsubproc(c1,"md5sum",0);
    if (strspn(hashbuf,"0123456789abcdef") != 32) ok=0;
    if (ferror(file)) ohshite(_("error reading pipe from md5sum"));
    if (fclose(file)) ohshite(_("error closing pipe from md5sum"));
    pop_cleanup(ehflag_normaltidy); /* file= fdopen(p1[0]) */
    pop_cleanup(ehflag_normaltidy); /* m_pipe() */
    pop_cleanup(ehflag_normaltidy); /* fd= open(cdr.buf) */
    if (!ok) ohshit(_("md5sum gave malformatted output `%.250s'"),hashbuf);
  } else if (errno == ENOENT) {
    strcpy(hashbuf,NONEXISTENTFLAG);
  } else {
    fprintf(stderr, _("dpkg: %s: warning - unable to open conffile %s for hash: %s\n"),
            pkg->name, fn, strerror(errno));
    strcpy(hashbuf,"-");
  }
}      
