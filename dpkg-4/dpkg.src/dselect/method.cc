/*
 * dselect - Debian GNU/Linux package maintenance user interface
 * method.cc - access method handling
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
#include <unistd.h>
#include <fcntl.h>
#include <sys/file.h>

#include <curses.h>

extern "C" {
#include <dpkg.h>
#include <dpkg-db.h>
#include <dpkg-var.h>
}

#include "dselect.h"
#include "method.h"
#include "config.h"

static char *methodlockfile= 0;
static int methlockfd= -1;

static void cu_unlockmethod(int, void**) {
  assert(methodlockfile);
  assert(methlockfd);
  if (flock(methlockfd,LOCK_UN))
    ohshite(_("unable to unlock access method area"));
}

static enum urqresult ensureoptions (void) 
{
  const char *const *ccpp;
  const char *lang;
  char *ccpp_nls;
  option *newoptions;
  int nread;
  
  const char *const methoddirectories[]= {
    LIBDIR,
    LOCALLIBDIR,
    0
  };    

#if defined (HAVE_SETLOCALE) && defined (HAVE_LC_MESSAGES) && defined (HAVE_SETLOCALE_NULL)
  lang = setlocale (LC_MESSAGES, NULL);
#else
  lang = getenv ("LC_ALL");
  if (lang == NULL || lang[0] == '\0') {
    lang = getenv ("LC_MESSAGES");
  }
  if (lang == NULL || lang[0] == '\0') {
    lang = getenv ("LANG");
  }
  if (lang == NULL || lang[0] == '\0') {
    lang = "en";
  }
#endif

  if (options == NULL) {
    newoptions = 0;
    nread = 0;
    for (ccpp = methoddirectories; *ccpp != NULL; ccpp++) {
      if ((lang != NULL) && (lang[0] != '\0')) {
        ccpp_nls = (char *) m_malloc (strlen (*ccpp) + strlen (METHODSDIR) + strlen (lang) + 3);
	sprintf (ccpp_nls, "%s/%s/%s", *ccpp, METHODSDIR, lang);
	readmethods (ccpp_nls, &newoptions, &nread);
	free (ccpp_nls);
      }
      ccpp_nls = (char *) m_malloc (strlen (*ccpp) + strlen (METHODSDIR) + 2);
      sprintf (ccpp_nls, "%s/%s", *ccpp, METHODSDIR);
      readmethods (ccpp_nls, &newoptions, &nread);
      free (ccpp_nls);
    }

    if (newoptions == NULL) {
      curseson();
      addstr (_("No access methods are available.\n\n"
		"Press RETURN to continue."));
      refresh(); getch();
      return urqr_fail;
    }
    options= newoptions;
    noptions= nread;
  }

  return urqr_normal;
}

static void lockmethod(void) {
  if (!methodlockfile) {
    int l;
    l= strlen(admindir);
    methodlockfile= new char[l+strlen(METHLOCKFILE)+2];
    strcpy(methodlockfile,admindir);
    strcpy(methodlockfile+l, "/");
    strcpy(methodlockfile+l+1,METHLOCKFILE);
  }
  if (methlockfd == -1) {
    methlockfd= open(methodlockfile, O_RDWR|O_CREAT|O_TRUNC, 0660);
    if (methlockfd == -1) {
      if (errno == EPERM)
        ohshit(_("you do not have permission to change the access method"));
      ohshite(_("unable to open/create access method lockfile"));
    }
  }
  if (flock(methlockfd,LOCK_EX|LOCK_NB)) {
    if (errno == EWOULDBLOCK || errno == EAGAIN)
      ohshit(_("the access method area is already locked"));
    ohshite(_("unable to lock access method area"));
  }
  push_cleanup(cu_unlockmethod,~0, 0,0, 0);
}

static int catchsignals[]= { SIGQUIT, SIGINT, 0 };
#define NCATCHSIGNALS ((signed)(sizeof(catchsignals)/sizeof(int))-1)
static struct sigaction uncatchsignal[NCATCHSIGNALS];

void cu_restoresignals(int, void**) {
  int i;
  for (i=0; i<NCATCHSIGNALS; i++)
    if (sigaction(catchsignals[i],&uncatchsignal[i],0))
      fprintf(stderr,_("error un-catching signal %d: %s\n"),
              catchsignals[i],strerror(errno));
}

urqresult falliblesubprocess(const char *exepath, const char *name,
                             const char *const *args) {
  pid_t c1, cr;
  int status, i, c;
  struct sigaction catchsig;
  
  cursesoff();

  memset(&catchsig,0,sizeof(catchsig));
  catchsig.sa_handler= SIG_IGN;
  sigemptyset(&catchsig.sa_mask);
  catchsig.sa_flags= 0;
  for (i=0; i<NCATCHSIGNALS; i++)
    if (sigaction(catchsignals[i],&catchsig,&uncatchsignal[i]))
      /* supermsg: falliblesubprocess -- may also be a command line */
      ohshite(_("unable to ignore signal %d before running %.250s"),
              catchsignals[i], name);
  push_cleanup(cu_restoresignals,~0, 0,0, 0);

  if (!(c1= m_fork())) {
    cu_restoresignals(0,0);
    execvp(exepath,(char* const*) args);
    /* supermsg: falliblesubprocess -- may also be a command line */
    ohshite(_("unable to run %.250s process `%.250s'"),name,exepath);
  }

  while ((cr= waitpid(c1,&status,0)) == -1)
    /* supermsg: falliblesubprocess -- may also be a command line */
    if (errno != EINTR) ohshite(_("unable to wait for %.250s"),name);
  if (cr != c1)
    ohshit(_("got wrong child's status - asked for %ld, got %ld"),(long)c1,(long)cr);

  pop_cleanup(ehflag_normaltidy);

  if (WIFEXITED(status) && !WEXITSTATUS(status)) {
    sleep(1);
    return urqr_normal;
  }
  if (WIFEXITED(status)) {
    i= WEXITSTATUS(status);
    /* supermsg: falliblesubprocess -- may also be a command line */
    fprintf(stderr,_("\n%s returned error exit status %d.\n"),name, i);
  } else if (WIFSIGNALED(status)) {
    i= WTERMSIG(status);
    if (i == SIGINT) {
      /* supermsg: falliblesubprocess -- may also be a command line */
      fprintf(stderr,_("\n%s was interrupted.\n"),name);
    } else {
      /* supermsg: falliblesubprocess -- may also be a command line */
      fprintf(stderr,_("\n%s was terminated by a signal: %s.\n"),name, strsignal(i));
    }
    if (WCOREDUMP(status))
      fprintf(stderr,_("(It left a coredump.)\n"));
  } else {
    /* supermsg: falliblesubprocess -- may also be a command line */
    fprintf(stderr,_("\n%s failed with an unknown wait return code %d.\n"),name, status);
  }
  fprintf(stderr,_("Press RETURN to continue.\n"));
  if (ferror(stderr))
    ohshite(_("write error on standard error"));
  do { c= fgetc(stdin); } while (c != EOF && c != '\n');
  if (c == EOF)
    ohshite(_("error reading acknowledgement of program failure message"));
  return urqr_fail;
}

static urqresult runscript(const char *exepath, const char *name) {
  urqresult ur;  

  ur= ensureoptions();  if (ur != urqr_normal) return ur;
  lockmethod();
  getcurrentopt();

  if (coption) {
    strcpy(coption->meth->pathinmeth,exepath);
    const char *fallibleargs[] = {
      exepath,
      admindir,
      coption->meth->name,
      coption->name,
      0
      };
    ur= falliblesubprocess(coption->meth->path,name,fallibleargs);
  } else {
    curseson();
    addstr(_("No access method is selected/configured.\n\n"
	     "Press RETURN to continue."));
    refresh(); getch();
    ur= urqr_fail;
  }
  pop_cleanup(ehflag_normaltidy);

  return ur;
}

urqresult urq_update(void) {
  /* submsg: falliblesubprocess */
  return runscript(METHODUPDATESCRIPT,_("update available list script"));
}

urqresult urq_install(void) {
  return runscript(METHODINSTALLSCRIPT,_("installation script"));
}

static urqresult rundpkgauto(const char *name, const char *dpkgmode) {
  const char *fallibleargs[] = {
    DPKG,
    "--pending",
    dpkgmode,
    0
  };
  cursesoff();
  /* L10n: args are a command line */
  printf(_("running %s %s ...\n"), "dpkg --pending", dpkgmode);
  fflush(stdout);
  return falliblesubprocess(DPKG,name,fallibleargs);
}

urqresult urq_remove(void) {
  return rundpkgauto("dpkg --remove","--remove");
}

urqresult urq_config(void) {
  return rundpkgauto("dpkg --configure","--configure");
}

urqresult urq_setup(void) {
  quitaction qa;
  urqresult ur;

  ur= ensureoptions();  if (ur != urqr_normal) return ur;
  lockmethod();  
  getcurrentopt();

  curseson();
  methodlist *l= new methodlist();
  qa= l->display();
  delete l;

  if (qa == qa_quitchecksave) {
    strcpy(coption->meth->pathinmeth,METHODSETUPSCRIPT);
    const char *fallibleargs[] = {
      METHODSETUPSCRIPT,
      admindir,
      coption->meth->name,
      coption->name,
      0
    };
    /* submsg: falliblesubprocess */
    ur= falliblesubprocess(coption->meth->path,_("query/setup script"),fallibleargs);
    if (ur == urqr_normal) writecurrentopt();
  } else {
    ur= urqr_fail;
  }
  
  pop_cleanup(ehflag_normaltidy);
  return ur;
}
