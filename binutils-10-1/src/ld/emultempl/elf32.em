# This shell script emits a C file. -*- C -*-
# It does some substitutions.
# This file is now misnamed, because it supports both 32 bit and 64 bit
# ELF emulations.
test -z "${ELFSIZE}" && ELFSIZE=32
cat >e${EMULATION_NAME}.c <<EOF
/* This file is is generated by a shell script.  DO NOT EDIT! */

/* ${ELFSIZE} bit ELF emulation code for ${EMULATION_NAME}
   Copyright (C) 1991, 93, 94, 95, 96, 97, 98, 1999
   Free Software Foundation, Inc.
   Written by Steve Chamberlain <sac@cygnus.com>
   ELF support by Ian Lance Taylor <ian@cygnus.com>

This file is part of GLD, the Gnu Linker.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#define TARGET_IS_${EMULATION_NAME}

#include "bfd.h"
#include "sysdep.h"

#include <ctype.h>

#include "bfdlink.h"

#include "ld.h"
#include "ldmain.h"
#include "ldemul.h"
#include "ldfile.h"
#include "ldmisc.h"
#include "ldexp.h"
#include "ldlang.h"
#include "ldgram.h"

static void gld${EMULATION_NAME}_before_parse PARAMS ((void));
static boolean gld${EMULATION_NAME}_open_dynamic_archive
  PARAMS ((const char *, search_dirs_type *, lang_input_statement_type *));
static void gld${EMULATION_NAME}_after_open PARAMS ((void));
static void gld${EMULATION_NAME}_check_needed
  PARAMS ((lang_input_statement_type *));
static void gld${EMULATION_NAME}_stat_needed
  PARAMS ((lang_input_statement_type *));
static boolean gld${EMULATION_NAME}_search_needed
  PARAMS ((const char *, const char *, int));
static boolean gld${EMULATION_NAME}_try_needed PARAMS ((const char *, int));
static void gld${EMULATION_NAME}_vercheck
  PARAMS ((lang_input_statement_type *));
static void gld${EMULATION_NAME}_before_allocation PARAMS ((void));
static void gld${EMULATION_NAME}_find_statement_assignment
  PARAMS ((lang_statement_union_type *));
static void gld${EMULATION_NAME}_find_exp_assignment PARAMS ((etree_type *));
static boolean gld${EMULATION_NAME}_place_orphan
  PARAMS ((lang_input_statement_type *, asection *));
static void gld${EMULATION_NAME}_place_section
  PARAMS ((lang_statement_union_type *));
static char *gld${EMULATION_NAME}_get_script PARAMS ((int *isfile));

static void
gld${EMULATION_NAME}_before_parse()
{
  ldfile_output_architecture = bfd_arch_`echo ${ARCH} | sed -e 's/:.*//'`;
  config.dynamic_link = ${DYNAMIC_LINK-true};
  config.has_shared = `if test -n "$GENERATE_SHLIB_SCRIPT" ; then echo true ; else echo false ; fi`;
}

/* Try to open a dynamic archive.  This is where we know that ELF
   dynamic libraries have an extension of .so (or .sl on oddball systems
   like hpux).  */

static boolean
gld${EMULATION_NAME}_open_dynamic_archive (arch, search, entry)
     const char *arch;
     search_dirs_type *search;
     lang_input_statement_type *entry;
{
  const char *filename;
  char *string;

  if (! entry->is_archive)
    return false;

  filename = entry->filename;

  /* This allocates a few bytes too many when EXTRA_SHLIB_EXTENSION
     is defined, but it does not seem worth the headache to optimize
     away those two bytes of space.  */
  string = (char *) xmalloc (strlen (search->name)
			     + strlen (filename)
			     + strlen (arch)
#ifdef EXTRA_SHLIB_EXTENSION
			     + strlen (EXTRA_SHLIB_EXTENSION)
#endif
			     + sizeof "/lib.so");

  sprintf (string, "%s/lib%s%s.so", search->name, filename, arch);

#ifdef EXTRA_SHLIB_EXTENSION
  /* Try the .so extension first.  If that fails build a new filename
     using EXTRA_SHLIB_EXTENSION.  */
  if (! ldfile_try_open_bfd (string, entry))
    sprintf (string, "%s/lib%s%s%s", search->name,
	     filename, arch, EXTRA_SHLIB_EXTENSION);
#endif

  if (! ldfile_try_open_bfd (string, entry))
    {
      free (string);
      return false;
    }

  entry->filename = string;

  /* We have found a dynamic object to include in the link.  The ELF
     backend linker will create a DT_NEEDED entry in the .dynamic
     section naming this file.  If this file includes a DT_SONAME
     entry, it will be used.  Otherwise, the ELF linker will just use
     the name of the file.  For an archive found by searching, like
     this one, the DT_NEEDED entry should consist of just the name of
     the file, without the path information used to find it.  Note
     that we only need to do this if we have a dynamic object; an
     archive will never be referenced by a DT_NEEDED entry.

     FIXME: This approach--using bfd_elf_set_dt_needed_name--is not
     very pretty.  I haven't been able to think of anything that is
     pretty, though.  */
  if (bfd_check_format (entry->the_bfd, bfd_object)
      && (entry->the_bfd->flags & DYNAMIC) != 0)
    {
      char *needed_name;

      ASSERT (entry->is_archive && entry->search_dirs_flag);

      /* Rather than duplicating the logic above.  Just use the
	 filename we recorded earlier.

	 First strip off everything before the last '/'.  */
      filename = strrchr (entry->filename, '/');
      filename++;

      needed_name = (char *) xmalloc (strlen (filename) + 1);
      strcpy (needed_name, filename);
      bfd_elf_set_dt_needed_name (entry->the_bfd, needed_name);
    }

  return true;
}

EOF
if [ "x${host}" = "x${target}" ] ; then
  case " ${EMULATION_LIBPATH} " in
  *" ${EMULATION_NAME} "*)
cat >>e${EMULATION_NAME}.c <<EOF

/* For a native linker, check the file /etc/ld.so.conf for directories
   in which we may find shared libraries.  /etc/ld.so.conf is really
   only meaningful on Linux, but we check it on other systems anyhow.  */

static boolean gld${EMULATION_NAME}_check_ld_so_conf
  PARAMS ((const char *, int));

static boolean
gld${EMULATION_NAME}_check_ld_so_conf (name, force)
     const char *name;
     int force;
{
  static boolean initialized;
  static char *ld_so_conf;

  if (! initialized)
    {
      FILE *f;

      f = fopen ("/etc/ld.so.conf", FOPEN_RT);
      if (f != NULL)
	{
	  char *b;
	  size_t len, alloc;
	  int c;

	  len = 0;
	  alloc = 100;
	  b = (char *) xmalloc (alloc);

	  while ((c = getc (f)) != EOF)
	    {
	      if (len + 1 >= alloc)
		{
		  alloc *= 2;
		  b = (char *) xrealloc (b, alloc);
		}
	      if (c != ':'
		  && c != ' '
		  && c != '\t'
		  && c != '\n'
		  && c != ',')
		{
		  b[len] = c;
		  ++len;
		}
	      else
		{
		  if (len > 0 && b[len - 1] != ':')
		    {
		      b[len] = ':';
		      ++len;
		    }
		}
	    }

	  if (len > 0 && b[len - 1] == ':')
	    --len;

	  if (len > 0)
	    b[len] = '\0';
	  else
	    {
	      free (b);
	      b = NULL;
	    }

	  fclose (f);

	  ld_so_conf = b;
	}

      initialized = true;
    }

  if (ld_so_conf == NULL)
    return false;

  return gld${EMULATION_NAME}_search_needed (ld_so_conf, name, force);
}

EOF
  ;;
  esac
fi
cat >>e${EMULATION_NAME}.c <<EOF

/* These variables are required to pass information back and forth
   between after_open and check_needed and stat_needed and vercheck.  */

static struct bfd_link_needed_list *global_needed;
static struct stat global_stat;
static boolean global_found;
static struct bfd_link_needed_list *global_vercheck_needed;
static boolean global_vercheck_failed;

/* This is called after all the input files have been opened.  */

static void
gld${EMULATION_NAME}_after_open ()
{
  struct bfd_link_needed_list *needed, *l;

  /* We only need to worry about this when doing a final link.  */
  if (link_info.relocateable || link_info.shared)
    return;

  /* Get the list of files which appear in DT_NEEDED entries in
     dynamic objects included in the link (often there will be none).
     For each such file, we want to track down the corresponding
     library, and include the symbol table in the link.  This is what
     the runtime dynamic linker will do.  Tracking the files down here
     permits one dynamic object to include another without requiring
     special action by the person doing the link.  Note that the
     needed list can actually grow while we are stepping through this
     loop.  */
  needed = bfd_elf_get_needed_list (output_bfd, &link_info);
  for (l = needed; l != NULL; l = l->next)
    {
      struct bfd_link_needed_list *ll;
      int force;

      /* If we've already seen this file, skip it.  */
      for (ll = needed; ll != l; ll = ll->next)
	if (strcmp (ll->name, l->name) == 0)
	  break;
      if (ll != l)
	continue;

      /* See if this file was included in the link explicitly.  */
      global_needed = l;
      global_found = false;
      lang_for_each_input_file (gld${EMULATION_NAME}_check_needed);
      if (global_found)
	continue;

      /* We need to find this file and include the symbol table.  We
	 want to search for the file in the same way that the dynamic
	 linker will search.  That means that we want to use
	 rpath_link, rpath, then the environment variable
	 LD_LIBRARY_PATH (native only), then the linker script
	 LIB_SEARCH_DIRS.  We do not search using the -L arguments.

	 We search twice.  The first time, we skip objects which may
	 introduce version mismatches.  The second time, we force
	 their use.  See gld${EMULATION_NAME}_vercheck comment.  */
      for (force = 0; force < 2; force++)
	{
	  const char *lib_path;
	  size_t len;
	  search_dirs_type *search;

	  if (gld${EMULATION_NAME}_search_needed (command_line.rpath_link,
						  l->name, force))
	    break;
	  if (gld${EMULATION_NAME}_search_needed (command_line.rpath,
						  l->name, force))
	    break;
	  if (command_line.rpath_link == NULL
	      && command_line.rpath == NULL)
	    {
	      lib_path = (const char *) getenv ("LD_RUN_PATH");
	      if (gld${EMULATION_NAME}_search_needed (lib_path, l->name,
						      force))
		break;
	    }
EOF
if [ "x${host}" = "x${target}" ] ; then
  case " ${EMULATION_LIBPATH} " in
  *" ${EMULATION_NAME} "*)
cat >>e${EMULATION_NAME}.c <<EOF
	  lib_path = (const char *) getenv ("LD_LIBRARY_PATH");
	  if (gld${EMULATION_NAME}_search_needed (lib_path, l->name, force))
	    break;
EOF
  ;;
  esac
fi
cat >>e${EMULATION_NAME}.c <<EOF
	  len = strlen (l->name);
	  for (search = search_head; search != NULL; search = search->next)
	    {
	      char *filename;

	      if (search->cmdline)
		continue;
	      filename = (char *) xmalloc (strlen (search->name) + len + 2);
	      sprintf (filename, "%s/%s", search->name, l->name);
	      if (gld${EMULATION_NAME}_try_needed (filename, force))
		break;
	      free (filename);
	    }
	  if (search != NULL)
	    break;
EOF
if [ "x${host}" = "x${target}" ] ; then
  case " ${EMULATION_LIBPATH} " in
  *" ${EMULATION_NAME} "*)
cat >>e${EMULATION_NAME}.c <<EOF
	  if (gld${EMULATION_NAME}_check_ld_so_conf (l->name, force))
	    break;
EOF
  ;;
  esac
fi
cat >>e${EMULATION_NAME}.c <<EOF
	}

      if (force < 2)
	continue;

      einfo ("%P: warning: %s, needed by %B, not found (try using --rpath)\n",
	     l->name, l->by);
    }
}

/* Search for a needed file in a path.  */

static boolean
gld${EMULATION_NAME}_search_needed (path, name, force)
     const char *path;
     const char *name;
     int force;
{
  const char *s;
  size_t len;

  if (path == NULL || *path == '\0')
    return false;
  len = strlen (name);
  while (1)
    {
      char *filename, *sset;

      s = strchr (path, ':');
      if (s == NULL)
	s = path + strlen (path);

      filename = (char *) xmalloc (s - path + len + 2);
      if (s == path)
	sset = filename;
      else
	{
	  memcpy (filename, path, s - path);
	  filename[s - path] = '/';
	  sset = filename + (s - path) + 1;
	}
      strcpy (sset, name);

      if (gld${EMULATION_NAME}_try_needed (filename, force))
	return true;

      free (filename);

      if (*s == '\0')
	break;
      path = s + 1;
    }

  return false;	  
}

/* This function is called for each possible name for a dynamic object
   named by a DT_NEEDED entry.  The FORCE parameter indicates whether
   to skip the check for a conflicting version.  */

static boolean
gld${EMULATION_NAME}_try_needed (name, force)
     const char *name;
     int force;
{
  bfd *abfd;

  abfd = bfd_openr (name, bfd_get_target (output_bfd));
  if (abfd == NULL)
    return false;
  if (! bfd_check_format (abfd, bfd_object))
    {
      (void) bfd_close (abfd);
      return false;
    }
  if ((bfd_get_file_flags (abfd) & DYNAMIC) == 0)
    {
      (void) bfd_close (abfd);
      return false;
    }

  /* Check whether this object would include any conflicting library
     versions.  If FORCE is set, then we skip this check; we use this
     the second time around, if we couldn't find any compatible
     instance of the shared library.  */

  if (! force)
    {
      struct bfd_link_needed_list *needed;

      if (! bfd_elf_get_bfd_needed_list (abfd, &needed))
	einfo ("%F%P:%B: bfd_elf_get_bfd_needed_list failed: %E\n", abfd);

      if (needed != NULL)
	{
	  global_vercheck_needed = needed;
	  global_vercheck_failed = false;
	  lang_for_each_input_file (gld${EMULATION_NAME}_vercheck);
	  if (global_vercheck_failed)
	    {
	      (void) bfd_close (abfd);
	      /* Return false to force the caller to move on to try
                 another file on the search path.  */
	      return false;
	    }

	  /* But wait!  It gets much worse.  On Linux, if a shared
             library does not use libc at all, we are supposed to skip
             it the first time around in case we encounter a shared
             library later on with the same name which does use the
             version of libc that we want.  This is much too horrible
             to use on any system other than Linux.  */

EOF
case ${target} in
  *-*-linux-gnu*)
    cat >>e${EMULATION_NAME}.c <<EOF
	  {
	    struct bfd_link_needed_list *l;

	    for (l = needed; l != NULL; l = l->next)
	      if (strncmp (l->name, "libc.so", 7) == 0)
		break;
	    if (l == NULL)
	      {
		(void) bfd_close (abfd);
		return false;
	      }
	  }

EOF
    ;;
esac
cat >>e${EMULATION_NAME}.c <<EOF
	}
    }

  /* We've found a dynamic object matching the DT_NEEDED entry.  */

  /* We have already checked that there is no other input file of the
     same name.  We must now check again that we are not including the
     same file twice.  We need to do this because on many systems
     libc.so is a symlink to, e.g., libc.so.1.  The SONAME entry will
     reference libc.so.1.  If we have already included libc.so, we
     don't want to include libc.so.1 if they are the same file, and we
     can only check that using stat.  */

  if (bfd_stat (abfd, &global_stat) != 0)
    einfo ("%F%P:%B: bfd_stat failed: %E\n", abfd);
  global_found = false;
  lang_for_each_input_file (gld${EMULATION_NAME}_stat_needed);
  if (global_found)
    {
      /* Return true to indicate that we found the file, even though
         we aren't going to do anything with it.  */
      return true;
    }

  /* Tell the ELF backend that don't want the output file to have a
     DT_NEEDED entry for this file.  */
  bfd_elf_set_dt_needed_name (abfd, "");

  /* Add this file into the symbol table.  */
  if (! bfd_link_add_symbols (abfd, &link_info))
    einfo ("%F%B: could not read symbols: %E\n", abfd);

  return true;
}

/* See if an input file matches a DT_NEEDED entry by name.  */

static void
gld${EMULATION_NAME}_check_needed (s)
     lang_input_statement_type *s;
{
  if (global_found)
    return;

  if (s->filename != NULL
      && strcmp (s->filename, global_needed->name) == 0)
    {
      global_found = true;
      return;
    }

  if (s->the_bfd != NULL)
    {
      const char *soname;

      soname = bfd_elf_get_dt_soname (s->the_bfd);
      if (soname != NULL
	  && strcmp (soname, global_needed->name) == 0)
	{
	  global_found = true;
	  return;
	}
    }
	  
  if (s->search_dirs_flag
      && s->filename != NULL
      && strchr (global_needed->name, '/') == NULL)
    {
      const char *f;

      f = strrchr (s->filename, '/');
      if (f != NULL
	  && strcmp (f + 1, global_needed->name) == 0)
	{
	  global_found = true;
	  return;
	}
    }
}

/* See if an input file matches a DT_NEEDED entry by running stat on
   the file.  */

static void
gld${EMULATION_NAME}_stat_needed (s)
     lang_input_statement_type *s;
{
  struct stat st;
  const char *suffix;
  const char *soname;
  const char *f;

  if (global_found)
    return;
  if (s->the_bfd == NULL)
    return;

  if (bfd_stat (s->the_bfd, &st) != 0)
    {
      einfo ("%P:%B: bfd_stat failed: %E\n", s->the_bfd);
      return;
    }

  if (st.st_dev == global_stat.st_dev
      && st.st_ino == global_stat.st_ino)
    {
      global_found = true;
      return;
    }

  /* We issue a warning if it looks like we are including two
     different versions of the same shared library.  For example,
     there may be a problem if -lc picks up libc.so.6 but some other
     shared library has a DT_NEEDED entry of libc.so.5.  This is a
     hueristic test, and it will only work if the name looks like
     NAME.so.VERSION.  FIXME: Depending on file names is error-prone.
     If we really want to issue warnings about mixing version numbers
     of shared libraries, we need to find a better way.  */

  if (strchr (global_needed->name, '/') != NULL)
    return;
  suffix = strstr (global_needed->name, ".so.");
  if (suffix == NULL)
    return;
  suffix += sizeof ".so." - 1;

  soname = bfd_elf_get_dt_soname (s->the_bfd);
  if (soname == NULL)
    soname = s->filename;

  f = strrchr (soname, '/');
  if (f != NULL)
    ++f;
  else
    f = soname;

  if (strncmp (f, global_needed->name, suffix - global_needed->name) == 0)
    einfo ("%P: warning: %s, needed by %B, may conflict with %s\n",
	   global_needed->name, global_needed->by, f);
}

/* On Linux, it's possible to have different versions of the same
   shared library linked against different versions of libc.  The
   dynamic linker somehow tags which libc version to use in
   /etc/ld.so.cache, and, based on the libc that it sees in the
   executable, chooses which version of the shared library to use.

   We try to do a similar check here by checking whether this shared
   library needs any other shared libraries which may conflict with
   libraries we have already included in the link.  If it does, we
   skip it, and try to find another shared library farther on down the
   link path.

   This is called via lang_for_each_input_file.
   GLOBAL_VERCHECK_NEEDED is the list of objects needed by the object
   which we ar checking.  This sets GLOBAL_VERCHECK_FAILED if we find
   a conflicting version.  */

static void
gld${EMULATION_NAME}_vercheck (s)
     lang_input_statement_type *s;
{
  const char *soname, *f;
  struct bfd_link_needed_list *l;

  if (global_vercheck_failed)
    return;
  if (s->the_bfd == NULL
      || (bfd_get_file_flags (s->the_bfd) & DYNAMIC) == 0)
    return;

  soname = bfd_elf_get_dt_soname (s->the_bfd);
  if (soname == NULL)
    soname = bfd_get_filename (s->the_bfd);

  f = strrchr (soname, '/');
  if (f != NULL)
    ++f;
  else
    f = soname;

  for (l = global_vercheck_needed; l != NULL; l = l->next)
    {
      const char *suffix;

      if (strcmp (f, l->name) == 0)
	{
	  /* Probably can't happen, but it's an easy check.  */
	  continue;
	}

      if (strchr (l->name, '/') != NULL)
	continue;

      suffix = strstr (l->name, ".so.");
      if (suffix == NULL)
	continue;

      suffix += sizeof ".so." - 1;

      if (strncmp (f, l->name, suffix - l->name) == 0)
	{
	  /* Here we know that S is a dynamic object FOO.SO.VER1, and
             the object we are considering needs a dynamic object
             FOO.SO.VER2, and VER1 and VER2 are different.  This
             appears to be a version mismatch, so we tell the caller
             to try a different version of this library.  */
	  global_vercheck_failed = true;
	  return;
	}
    }
}

/* This is called after the sections have been attached to output
   sections, but before any sizes or addresses have been set.  */

static void
gld${EMULATION_NAME}_before_allocation ()
{
  const char *rpath;
  asection *sinterp;

  /* If we are going to make any variable assignments, we need to let
     the ELF backend know about them in case the variables are
     referred to by dynamic objects.  */
  lang_for_each_statement (gld${EMULATION_NAME}_find_statement_assignment);

  /* Let the ELF backend work out the sizes of any sections required
     by dynamic linking.  */
  rpath = command_line.rpath;
  if (rpath == NULL)
    rpath = (const char *) getenv ("LD_RUN_PATH");
  if (! (bfd_elf${ELFSIZE}_size_dynamic_sections
         (output_bfd, command_line.soname, rpath,
	  command_line.export_dynamic, command_line.filter_shlib,
	  (const char * const *) command_line.auxiliary_filters,
	  &link_info, &sinterp, lang_elf_version_info)))
    einfo ("%P%F: failed to set dynamic section sizes: %E\n");

  /* Let the user override the dynamic linker we are using.  */
  if (command_line.interpreter != NULL
      && sinterp != NULL)
    {
      sinterp->contents = (bfd_byte *) command_line.interpreter;
      sinterp->_raw_size = strlen (command_line.interpreter) + 1;
    }

  /* Look for any sections named .gnu.warning.  As a GNU extensions,
     we treat such sections as containing warning messages.  We print
     out the warning message, and then zero out the section size so
     that it does not get copied into the output file.  */

  {
    LANG_FOR_EACH_INPUT_STATEMENT (is)
      {
	asection *s;
	bfd_size_type sz;
	char *msg;
	boolean ret;

	if (is->just_syms_flag)
	  continue;

	s = bfd_get_section_by_name (is->the_bfd, ".gnu.warning");
	if (s == NULL)
	  continue;

	sz = bfd_section_size (is->the_bfd, s);
	msg = xmalloc ((size_t) sz + 1);
	if (! bfd_get_section_contents (is->the_bfd, s, msg, (file_ptr) 0, sz))
	  einfo ("%F%B: Can't read contents of section .gnu.warning: %E\n",
		 is->the_bfd);
	msg[sz] = '\0';
	ret = link_info.callbacks->warning (&link_info, msg,
					    (const char *) NULL,
					    is->the_bfd, (asection *) NULL,
					    (bfd_vma) 0);
	ASSERT (ret);
	free (msg);

	/* Clobber the section size, so that we don't waste copying the
	   warning into the output file.  */
	s->_raw_size = 0;
      }
  }
}

/* This is called by the before_allocation routine via
   lang_for_each_statement.  It locates any assignment statements, and
   tells the ELF backend about them, in case they are assignments to
   symbols which are referred to by dynamic objects.  */

static void
gld${EMULATION_NAME}_find_statement_assignment (s)
     lang_statement_union_type *s;
{
  if (s->header.type == lang_assignment_statement_enum)
    gld${EMULATION_NAME}_find_exp_assignment (s->assignment_statement.exp);
}

/* Look through an expression for an assignment statement.  */

static void
gld${EMULATION_NAME}_find_exp_assignment (exp)
     etree_type *exp;
{
  struct bfd_link_hash_entry *h;

  switch (exp->type.node_class)
    {
    case etree_provide:
      h = bfd_link_hash_lookup (link_info.hash, exp->assign.dst,
				false, false, false);
      if (h == NULL)
	break;

      /* We call record_link_assignment even if the symbol is defined.
	 This is because if it is defined by a dynamic object, we
	 actually want to use the value defined by the linker script,
	 not the value from the dynamic object (because we are setting
	 symbols like etext).  If the symbol is defined by a regular
	 object, then, as it happens, calling record_link_assignment
	 will do no harm.  */

      /* Fall through.  */
    case etree_assign:
      if (strcmp (exp->assign.dst, ".") != 0)
	{
	  if (! (bfd_elf${ELFSIZE}_record_link_assignment
		 (output_bfd, &link_info, exp->assign.dst,
		  exp->type.node_class == etree_provide ? true : false)))
	    einfo ("%P%F: failed to record assignment to %s: %E\n",
		   exp->assign.dst);
	}
      gld${EMULATION_NAME}_find_exp_assignment (exp->assign.src);
      break;

    case etree_binary:
      gld${EMULATION_NAME}_find_exp_assignment (exp->binary.lhs);
      gld${EMULATION_NAME}_find_exp_assignment (exp->binary.rhs);
      break;

    case etree_trinary:
      gld${EMULATION_NAME}_find_exp_assignment (exp->trinary.cond);
      gld${EMULATION_NAME}_find_exp_assignment (exp->trinary.lhs);
      gld${EMULATION_NAME}_find_exp_assignment (exp->trinary.rhs);
      break;

    case etree_unary:
      gld${EMULATION_NAME}_find_exp_assignment (exp->unary.child);
      break;

    default:
      break;
    }
}

/* Place an orphan section.  We use this to put random SHF_ALLOC
   sections in the right segment.  */

static asection *hold_section;
static lang_output_section_statement_type *hold_use;
static lang_output_section_statement_type *hold_text;
static lang_output_section_statement_type *hold_rodata;
static lang_output_section_statement_type *hold_data;
static lang_output_section_statement_type *hold_bss;
static lang_output_section_statement_type *hold_rel;
static lang_output_section_statement_type *hold_interp;

/*ARGSUSED*/
static boolean
gld${EMULATION_NAME}_place_orphan (file, s)
     lang_input_statement_type *file;
     asection *s;
{
  lang_output_section_statement_type *place;
  asection *snew, **pps;
  lang_statement_list_type *old;
  lang_statement_list_type add;
  etree_type *address;
  const char *secname, *ps;
  const char *outsecname;
  lang_output_section_statement_type *os;

  if ((s->flags & SEC_ALLOC) == 0)
    return false;

  /* Look through the script to see where to place this section.  */
  hold_section = s;
  hold_use = NULL;
  lang_for_each_statement (gld${EMULATION_NAME}_place_section);

  if (hold_use != NULL)
    {
      /* We have already placed a section with this name.  */
      wild_doit (&hold_use->children, s, hold_use, file);
      return true;
    }

  secname = bfd_get_section_name (s->owner, s);

  /* If this is a final link, then always put .gnu.warning.SYMBOL
     sections into the .text section to get them out of the way.  */
  if (! link_info.shared
      && ! link_info.relocateable
      && strncmp (secname, ".gnu.warning.", sizeof ".gnu.warning." - 1) == 0
      && hold_text != NULL)
    {
      wild_doit (&hold_text->children, s, hold_text, file);
      return true;
    }

  /* Decide which segment the section should go in based on the
     section name and section flags.  We put loadable .note sections
     right after the .interp section, so that the PT_NOTE segment is
     stored right after the program headers where the OS can read it
     in the first page.  */
  place = NULL;
  if (s->flags & SEC_EXCLUDE)
    return false;
  else if ((s->flags & SEC_LOAD) != 0
      && strncmp (secname, ".note", 4) == 0
      && hold_interp != NULL)
    place = hold_interp;
  else if ((s->flags & SEC_HAS_CONTENTS) == 0
	   && hold_bss != NULL)
    place = hold_bss;
  else if ((s->flags & SEC_READONLY) == 0
	   && hold_data != NULL)
    place = hold_data;
  else if (strncmp (secname, ".rel", 4) == 0
	   && hold_rel != NULL)
    place = hold_rel;
  else if ((s->flags & SEC_CODE) == 0
	   && (s->flags & SEC_READONLY) != 0
	   && hold_rodata != NULL)
    place = hold_rodata;
  else if ((s->flags & SEC_READONLY) != 0
	   && hold_text != NULL)
    place = hold_text;
  if (place == NULL)
    return false;

  /* Choose a unique name for the section.  This will be needed if the
     same section name appears in the input file with different
     loadable or allocateable characteristics.  */
  outsecname = secname;
  if (bfd_get_section_by_name (output_bfd, outsecname) != NULL)
    {
      unsigned int len;
      char *newname;
      unsigned int i;

      len = strlen (outsecname);
      newname = xmalloc (len + 5);
      strcpy (newname, outsecname);
      i = 0;
      do
	{
	  sprintf (newname + len, "%d", i);
	  ++i;
	}
      while (bfd_get_section_by_name (output_bfd, newname) != NULL);

      outsecname = newname;
    }

  /* Create the section in the output file, and put it in the right
     place.  This shuffling is to make the output file look neater.  */
  snew = bfd_make_section (output_bfd, outsecname);
  if (snew == NULL)
      einfo ("%P%F: output format %s cannot represent section called %s\n",
	     output_bfd->xvec->name, outsecname);
  if (place->bfd_section != NULL)
    {
      for (pps = &output_bfd->sections; *pps != snew; pps = &(*pps)->next)
	;
      *pps = snew->next;
      snew->next = place->bfd_section->next;
      place->bfd_section->next = snew;
    }

  /* Start building a list of statements for this section.  */
  old = stat_ptr;
  stat_ptr = &add;
  lang_list_init (stat_ptr);

  /* If the name of the section is representable in C, then create
     symbols to mark the start and the end of the section.  */
  for (ps = outsecname; *ps != '\0'; ps++)
    if (! isalnum ((unsigned char) *ps) && *ps != '_')
      break;
  if (*ps == '\0' && config.build_constructors)
    {
      char *symname;

      symname = (char *) xmalloc (ps - outsecname + sizeof "__start_");
      sprintf (symname, "__start_%s", outsecname);
      lang_add_assignment (exp_assop ('=', symname,
				      exp_unop (ALIGN_K,
						exp_intop ((bfd_vma) 1
							   << s->alignment_power))));
    }

  if (! link_info.relocateable)
    address = NULL;
  else
    address = exp_intop ((bfd_vma) 0);

  lang_enter_output_section_statement (outsecname, address, 0,
				       (bfd_vma) 0,
				       (etree_type *) NULL,
				       (etree_type *) NULL,
				       (etree_type *) NULL);

  os = lang_output_section_statement_lookup (outsecname);
  wild_doit (&os->children, s, os, file);

  lang_leave_output_section_statement
    ((bfd_vma) 0, "*default*", (struct lang_output_section_phdr_list *) NULL,
     "*default*");
  stat_ptr = &add;

  if (*ps == '\0' && config.build_constructors)
    {
      char *symname;

      symname = (char *) xmalloc (ps - outsecname + sizeof "__stop_");
      sprintf (symname, "__stop_%s", outsecname);
      lang_add_assignment (exp_assop ('=', symname,
				      exp_nameop (NAME, ".")));
    }

  /* Now stick the new statement list right after PLACE.  */
  *add.tail = place->header.next;
  place->header.next = add.head;

  stat_ptr = old;

  return true;
}

static void
gld${EMULATION_NAME}_place_section (s)
     lang_statement_union_type *s;
{
  lang_output_section_statement_type *os;

  if (s->header.type != lang_output_section_statement_enum)
    return;

  os = &s->output_section_statement;

  if (strcmp (os->name, hold_section->name) == 0
      && os->bfd_section != NULL
      && ((hold_section->flags & (SEC_LOAD | SEC_ALLOC))
	  == (os->bfd_section->flags & (SEC_LOAD | SEC_ALLOC))))
    hold_use = os;

  if (strcmp (os->name, ".text") == 0)
    hold_text = os;
  else if (strcmp (os->name, ".rodata") == 0)
    hold_rodata = os;
  else if (strcmp (os->name, ".data") == 0)
    hold_data = os;
  else if (strcmp (os->name, ".bss") == 0)
    hold_bss = os;
  else if (hold_rel == NULL
	   && os->bfd_section != NULL
	   && (os->bfd_section->flags & SEC_ALLOC) != 0
	   && strncmp (os->name, ".rel", 4) == 0)
    hold_rel = os;
  else if (strcmp (os->name, ".interp") == 0)
    hold_interp = os;
}

static char *
gld${EMULATION_NAME}_get_script(isfile)
     int *isfile;
EOF

if test -n "$COMPILE_IN"
then
# Scripts compiled in.

# sed commands to quote an ld script as a C string.
sc="-f stringify.sed"

cat >>e${EMULATION_NAME}.c <<EOF
{			     
  *isfile = 0;

  if (link_info.relocateable == true && config.build_constructors == true)
    return
EOF
sed $sc ldscripts/${EMULATION_NAME}.xu                     >> e${EMULATION_NAME}.c
echo '  ; else if (link_info.relocateable == true) return' >> e${EMULATION_NAME}.c
sed $sc ldscripts/${EMULATION_NAME}.xr                     >> e${EMULATION_NAME}.c
echo '  ; else if (!config.text_read_only) return'         >> e${EMULATION_NAME}.c
sed $sc ldscripts/${EMULATION_NAME}.xbn                    >> e${EMULATION_NAME}.c
echo '  ; else if (!config.magic_demand_paged) return'     >> e${EMULATION_NAME}.c
sed $sc ldscripts/${EMULATION_NAME}.xn                     >> e${EMULATION_NAME}.c

if test -n "$GENERATE_SHLIB_SCRIPT" ; then
echo '  ; else if (link_info.shared) return'		   >> e${EMULATION_NAME}.c
sed $sc ldscripts/${EMULATION_NAME}.xs                     >> e${EMULATION_NAME}.c
fi

echo '  ; else return'                                     >> e${EMULATION_NAME}.c
sed $sc ldscripts/${EMULATION_NAME}.x                      >> e${EMULATION_NAME}.c
echo '; }'                                                 >> e${EMULATION_NAME}.c

else
# Scripts read from the filesystem.

cat >>e${EMULATION_NAME}.c <<EOF
{			     
  *isfile = 1;

  if (link_info.relocateable == true && config.build_constructors == true)
    return "ldscripts/${EMULATION_NAME}.xu";
  else if (link_info.relocateable == true)
    return "ldscripts/${EMULATION_NAME}.xr";
  else if (!config.text_read_only)
    return "ldscripts/${EMULATION_NAME}.xbn";
  else if (!config.magic_demand_paged)
    return "ldscripts/${EMULATION_NAME}.xn";
  else if (link_info.shared)
    return "ldscripts/${EMULATION_NAME}.xs";
  else
    return "ldscripts/${EMULATION_NAME}.x";
}
EOF

fi

if test -n "$PARSE_AND_LIST_ARGS" ; then
cat >>e${EMULATION_NAME}.c <<EOF
static int  gld_${EMULATION_NAME}_parse_args PARAMS ((int, char **));
static void gld_${EMULATION_NAME}_list_options PARAMS ((FILE * file));

 $PARSE_AND_LIST_ARGS
EOF
else

cat >>e${EMULATION_NAME}.c <<EOF
#define gld_${EMULATION_NAME}_parse_args   NULL
#define gld_${EMULATION_NAME}_list_options NULL
EOF

fi

cat >>e${EMULATION_NAME}.c <<EOF

struct ld_emulation_xfer_struct ld_${EMULATION_NAME}_emulation = 
{
  gld${EMULATION_NAME}_before_parse,
  syslib_default,
  hll_default,
  after_parse_default,
  gld${EMULATION_NAME}_after_open,
  after_allocation_default,
  set_output_arch_default,
  ldemul_default_target,
  gld${EMULATION_NAME}_before_allocation,
  gld${EMULATION_NAME}_get_script,
  "${EMULATION_NAME}",
  "${OUTPUT_FORMAT}",
  NULL, 	/* finish */
  NULL, 	/* create output section statements */
  gld${EMULATION_NAME}_open_dynamic_archive,
  gld${EMULATION_NAME}_place_orphan,
  NULL,		/* set_symbols */
  gld_${EMULATION_NAME}_parse_args,
  NULL,		/* unrecognized_file */
  gld_${EMULATION_NAME}_list_options,
  NULL,		/* recognized_file */
  NULL		/* find_potential_libraries */
};
EOF
