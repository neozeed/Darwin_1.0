/*
 * libdpkg - Debian packaging suite library routines
 * showcright.c - show copyright file routine
 *
 * Copyright (C) 1994,1995 Ian Jackson <iwj10@cus.cam.ac.uk>
 * Copyright (C) 1997 Klee Dienes <klee@mit.edu>
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

#include <unistd.h>
#include <fcntl.h>

#include "dpkg.h"
#include "dpkg-db.h"
#include "dpkg-var.h"
#include "libdpkg-int.h"
#include "config.h"

void showcopyright (const struct cmdinfo *c, const char *v) 
{
  int fd;
  fd = open (COPYINGFILE, O_RDONLY);
  if (fd < 0) {
    ohshite (_("unable to open GPL file %s"), COPYINGFILE);
  }
  m_dup2 (fd, 0);
  execlp (CAT, CAT, "-", (char*) 0);
  ohshite (_("unable to exec \"%s\" to display GPL file"), CAT);
}
