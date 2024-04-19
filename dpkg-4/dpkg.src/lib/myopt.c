/*
 * libdpkg - Debian packaging suite library routines
 * myopt.c - my very own option parsing
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
 * License along with this file; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <string.h>

#include "dpkg.h"
#include "dpkg-db.h"
#include "myopt.h"
#include "libdpkg-int.h"
#include "config.h"

void myopt(const char *const **argvp, const struct cmdinfo *cmdinfos) {
  const struct cmdinfo *cip;
  const char *p, *value;
  int l;

  ++(*argvp);
  while ((p= **argvp) && *p == '-') {
    ++(*argvp);
    if (!strcmp(p,"--")) break;
    if (*++p == '-') {
      ++p; value=0;
      for (cip= cmdinfos;
           cip->olong || cip->oshort;
           cip++) {
        if (!cip->olong) continue;
        if (!strcmp(p,cip->olong)) break;
        l= strlen(cip->olong);
        if (!strncmp(p,cip->olong,l) &&
            (p[l]== ((cip->takesvalue==2) ? '-' : '='))) { value=p+l+1; break; }
      }
      if (!cip->olong) badusage(_("unknown option --%s"),p);
      if (cip->takesvalue) {
        if (!value) {
          value= *(*argvp)++;
          if (!value) badusage(_("--%s option takes a value"),cip->olong);
        }
        if (cip->call) cip->call(cip,value);
        else *cip->sassignto= value;
      } else {
        if (value) badusage(_("--%s option does not take a value"),cip->olong);
        if (cip->call) cip->call(cip,0);
        else *cip->iassignto= cip->arg;
      }
    } else {
      while (*p) {
        for (cip= cmdinfos; (cip->olong || cip->oshort) && *p != cip->oshort; cip++);
        if (!cip->oshort) badusage(_("unknown option -%c"),*p);
        p++;
        if (cip->takesvalue) {
          if (!*p) {
            value= *(*argvp)++;
            if (!value) badusage(_("-%c option takes a value"),cip->oshort);
          } else {
            value= p; p="";
            if (*value == '=') value++;
          }
          if (cip->call) cip->call(cip,value);
          else *cip->sassignto= value;
        } else {
          if (*p == '=') badusage(_("-%c option does not take a value"),cip->oshort);
          if (cip->call) cip->call(cip,0);
          else *cip->iassignto= cip->arg;
        }
      }
    }
  }
}
