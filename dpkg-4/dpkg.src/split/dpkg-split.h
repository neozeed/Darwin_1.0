/*
 * dpkg-split - splitting and joining of multipart *.deb archives
 * dpkg-split.h - external definitions for this program
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

#ifndef DPKG_SPLIT_H
#define DPKG_SPLIT_H

#include "config.h"

#if HAVE_LOCALE_H
# include <locale.h>
#endif
#if !HAVE_SETLOCALE
# define setlocale(category, locale)
#endif

#if ENABLE_NLS
# include <libintl.h>
# define _(text) (gettext (text))
# define G_(text) (gettext (text))
# define N_(text) (text)
#else
# define bindtextdomain(domain, directory)
# define textdomain(domain)
# define _(text) (text)
# define G_(text) (text)
# define N_(text) (text)
#endif

#define internerr(s) do_internerr (s,__LINE__,__FILE__)

typedef void dofunction(const char *const *argv);
dofunction do_split, do_join, do_info, do_auto, do_queue, do_discard;

struct partinfo {
  const char *filename;
  const char *fmtversion;
  const char *package;
  const char *version;
  const char *md5sum;
  unsigned long orglength;
  int thispartn, maxpartn;
  unsigned long maxpartlen;
  unsigned long thispartoffset;
  unsigned long thispartlen;
  unsigned long headerlen; /* size of header in part file */
  unsigned long filesize;
};

struct partqueue {
  struct partqueue *nextinqueue;
  struct partinfo info;
  /* only fields filename, md5sum, maxpartlen, thispartn, maxpartn
   * are valid; the rest are null.  If the file is not named correctly
   * to be a part file md5sum is null too and the numbers are zero.
   */
};

extern dofunction *action;
extern const struct cmdinfo *cipaction;
extern long maxpartsize;
extern const char *depotdir, *outputfile;
extern struct partqueue *queue;
extern int npquiet, msdos;

void rerr(const char *fn) NONRETURNING;
void rerreof(FILE *f, const char *fn) NONRETURNING;
void print_info(const struct partinfo *pi);
struct partinfo *read_info(FILE *partfile, const char *fn, struct partinfo *ir);

void scandepot(void);
void reassemble(struct partinfo **partlist, const char *outputfile);
void mustgetpartinfo(const char *filename, struct partinfo *ri);
void addtopartlist(struct partinfo**, struct partinfo*, struct partinfo *refi);

#define PARTMAGIC         "!<arch>\ndebian-split    "
#define HEADERALLOWANCE    1024

#endif /* DPKG_SPLIT_H */
