/* Assorted BFD support routines, only used internally.
   Copyright 1990, 91, 92, 93, 94, 95, 96, 1997 Free Software Foundation, Inc.
   Written by Cygnus Support.

This file is part of BFD, the Binary File Descriptor library.

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

#include "sysdep.h"

#include "bfd.h"
#include "libbfd.h"

#undef HAVE_MPROTECT

#if HAVE_MMAP || HAVE_MPROTECT || HAVE_MADVISE
#include <sys/types.h>
#include <sys/mman.h>
#endif

#undef MAP_SHARED
#define MAP_SHARED MAP_PRIVATE

#ifndef MAP_FILE
#define MAP_FILE 0
#endif

#ifndef HAVE_GETPAGESIZE
#define getpagesize() 2048
#endif

/* The window support stuff should probably be broken out into
   another file....  */
/* The idea behind the next and refcount fields is that one mapped
   region can suffice for multiple read-only windows or multiple
   non-overlapping read-write windows.  It's not implemented yet
   though.  */
struct _bfd_window_internal {
  struct _bfd_window_internal *next;
  PTR data;
  bfd_size_type size;
  int refcount : 30;		/* should be enough... */
  unsigned mapped : 2;		/* 2 = in-memory, 1 = mmap, 0 = malloc */
};

void
bfd_init_window (windowp)
     bfd_window *windowp;
{
  windowp->data = 0;
  windowp->i = 0;
  windowp->size = 0;
}

/* Currently, if USE_MMAP is undefined, none if the window stuff is
   used.  Okay, so it's mis-named.  At least the command-line option
   "--without-mmap" is more obvious than "--without-windows" or some
   such.  */

static int debug_windows;

void
bfd_free_window (windowp)
     bfd_window *windowp;
{
  bfd_window_internal *i = windowp->i;

  windowp->i = 0;
  windowp->data = 0;

  if (i == 0)
    return;

  i->refcount--;
  if (debug_windows)
    fprintf (stderr, "freeing window @%p<%p,%lx,%p>\n",
	     windowp, windowp->data, windowp->size, windowp->i);
  if (i->refcount > 0)
    return;

  switch (i->mapped) {

  case 2:
      i->data = NULL;
      break;

  case 1:
#if HAVE_MMAP
    munmap (i->data, i->size);
    i->data = NULL;
#else
    abort ();
#endif

  case 0:
#if HAVE_MPROTECT
    mprotect (i->data, i->size, PROT_READ | PROT_WRITE);
#endif
    free (i->data);
    i->data = NULL;
    break;

  default:
    BFD_ASSERT ((i->mapped == 0) || (i->mapped == 1) || (i->mapped == 2));
  }

  /* There should be no more references to i at this point.  */
  free (i);
}

#if HAVE_MMAP
static boolean
_bfd_get_file_window_mmap (abfd, offset, size, windowp, i, writable)
     bfd *abfd;
     file_ptr offset;
     bfd_size_type size;
     bfd_window *windowp;
     bfd_window_internal *i;
     boolean writable;
{
  static size_t pagesize;

  /* Make sure we know the page size, so we can be friendly to mmap.  */
  if (pagesize == 0)
    pagesize = getpagesize ();
  if (pagesize == 0)
    abort ();

  if (i->data == 0 || i->mapped == 1)
    {
      file_ptr file_offset, offset2;
      size_t real_size;
      int fd;
      FILE *f;

      /* Find the real file and the real offset into it.  */
      while (abfd->my_archive != NULL)
	{
	  offset += abfd->origin;
	  abfd = abfd->my_archive;
	}
      f = bfd_cache_lookup (abfd);
      fd = fileno (f);

      /* Compute offsets and size for mmap and for the user's data.  */
      offset2 = offset % pagesize;
      if (offset2 < 0)
	abort ();
      file_offset = offset - offset2;
      real_size = offset + size - file_offset;
      real_size = real_size + pagesize - 1;
      real_size -= real_size % pagesize;

      /* If we're re-using a memory region, make sure it's big enough.  */
      if (i->data && i->size < size)
	{
	  munmap (i->data, i->size);
	  i->data = 0;
	}
      i->data = mmap (i->data, real_size,
		      writable ? PROT_WRITE | PROT_READ : PROT_READ,
		      (writable
		       ? MAP_FILE | MAP_PRIVATE
		       : MAP_FILE | MAP_SHARED),
		      fd, file_offset);
      if (i->data == (PTR) -1)
	{
	  /* An error happened.  Report it, or try using malloc, or
	     something.  */
	  bfd_set_error (bfd_error_system_call);
	  i->data = 0;
	  windowp->data = 0;
	  if (debug_windows)
	    fprintf (stderr, "\t\tmmap failed!\n");
	  return false;
	}
      if (debug_windows)
	fprintf (stderr, "\n\tmapped %ld at %p, offset is %ld\n",
		 (long) real_size, i->data, (long) offset2);
      i->size = real_size;
      windowp->data = (PTR) ((bfd_byte *) i->data + offset2);
      windowp->size = size;
      i->mapped = 1;
      return true;
    }
}
#endif /* HAVE_MMAP */

static boolean
_bfd_get_file_window_malloc (abfd, offset, size, windowp, i, writable)
     bfd *abfd;
     file_ptr offset;
     bfd_size_type size;
     bfd_window *windowp;
     bfd_window_internal *i;
     boolean writable;
{
  static size_t pagesize;
  size_t size_to_alloc = size;

  /* Make sure we know the page size, so we can be friendly to mmap.  */
  if (pagesize == 0)
    pagesize = getpagesize ();
  if (pagesize == 0)
    abort ();

#if HAVE_MPROTECT
  if (!writable)
    {
      size_to_alloc += pagesize - 1;
      size_to_alloc -= size_to_alloc % pagesize;
    }
#endif

  if (debug_windows)
    fprintf (stderr, "\n\t%s(%6ld)",
	     i->data ? "realloc" : " malloc", (long) size_to_alloc);
  i->data = (PTR) bfd_realloc (i->data, size_to_alloc);
  if (debug_windows)
    fprintf (stderr, "\t-> %p\n", i->data);
  i->refcount = 1;
  if (i->data == NULL)
    {
      if (size_to_alloc == 0)
	return true;
      bfd_set_error (bfd_error_no_memory);
      return false;
    }
  if (bfd_seek (abfd, offset, SEEK_SET) != 0)
    return false;
  i->size = bfd_read (i->data, size, 1, abfd);
  if (i->size != size)
    return false;
  i->mapped = 0;

#if HAVE_MPROTECT
  if (!writable)
    {
      if (debug_windows)
	fprintf (stderr, "\tmprotect (%p, %ld, PROT_READ)\n", i->data,
		 (long) i->size);
      mprotect (i->data, i->size, PROT_READ);
    }
#endif

  windowp->data = i->data;
  windowp->size = i->size;
}

boolean
bfd_get_file_window (abfd, offset, size, windowp, writable)
     bfd *abfd;
     file_ptr offset;
     bfd_size_type size;
     bfd_window *windowp;
     boolean writable;
{
  boolean ret = false;
  bfd_window_internal *i = windowp->i;

  if (debug_windows)
    fprintf (stderr, "bfd_get_file_window (%p, %6ld, %6ld, %p<%p,%lx,%p>, %d)",
	     abfd, (long) offset, (long) size,
	     windowp, windowp->data, (unsigned long) windowp->size,
	     windowp->i, writable);

  BFD_ASSERT (i == NULL);

  if (i == NULL)
    {
      windowp->i = i = (bfd_window_internal *) bfd_zmalloc (sizeof (bfd_window_internal));
      if (i == 0)
	return false;
      i->data = 0;
    }

  if ((abfd->flags & BFD_IO_FUNCS) != 0)
    {
      if (! _bfd_get_file_window_malloc (abfd, offset, size, windowp, i, writable))
	return false;
    }
  else if ((abfd->flags & BFD_IN_MEMORY) != 0) 
    {
      struct bfd_in_memory *bim = (struct bfd_in_memory *) abfd->iostream;
      BFD_ASSERT (bim != NULL);

      if ((offset > bim->size) || ((bim->size - offset) < size))
	{
	  bfd_set_error (bfd_error_file_truncated);
	  return false;
	}
      
      i->next = NULL;
      i->data = bim->buffer + offset;
      i->size = size;
      i->refcount = 1;
      i->mapped = 2;
    }
  else
    {
#if HAVE_MMAP
      if (! _bfd_get_file_window_mmap (abfd, offset, size, windowp, i, writable))
	return false;
#else
      if (! _bfd_get_file_window_malloc (abfd, offset, size, windowp, i, writable))
	return false;
#endif
    }

  return true;
}

boolean
_bfd_generic_get_section_contents_in_window (abfd, section, w, offset, count)
     bfd *abfd;
     sec_ptr section;
     bfd_window *w;
     file_ptr offset;
     bfd_size_type count;
{
  if (count == 0)
    return true;

  if (abfd->xvec->_bfd_get_section_contents != _bfd_generic_get_section_contents)
    {
      /* We don't know what changes the bfd's get_section_contents
	 method may have to make.  So punt trying to map the file
	 window, and let get_section_contents do its thing.  */
      /* @@ FIXME : If the internal window has a refcount of 1 and was
	 allocated with malloc instead of mmap, just reuse it.  */
      bfd_free_window (w);
      w->i = (bfd_window_internal *) bfd_zmalloc (sizeof (bfd_window_internal));
      if (w->i == NULL)
	return false;
      w->i->data = (PTR) bfd_malloc ((size_t) count);
      if (w->i->data == NULL)
	{
	  free (w->i);
	  w->i = NULL;
	  return false;
	}
      w->i->mapped = 0;
      w->i->refcount = 1;
      w->size = w->i->size = count;
      w->data = w->i->data;
      return bfd_get_section_contents (abfd, section, w->data, offset, count);
    }

  if ((bfd_size_type) (offset+count) > section->_raw_size
      || (bfd_get_file_window (abfd, section->filepos + offset, count, w, true)
	  == false))
    return false;

  return true;
}
