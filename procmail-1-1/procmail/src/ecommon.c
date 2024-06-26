/************************************************************************
 *	Some common routines to all programs but procmail		*
 *									*
 *	Copyright (c) 1993-1997, S.R. van den Berg, The Netherlands	*
 *	#include "../README"						*
 ************************************************************************/
#ifdef RCS
static /*const*/char rcsid[]=
 "$Id: ecommon.c,v 1.1.1.1 1999/09/23 17:30:07 wsanchez Exp $";
#endif
#include "includes.h"
#include "ecommon.h"
#include "common.h"

void
 nlog P((const char*const a));

static const char outofmem[]="Out of memory\n";

void*tmalloc(len)const size_t len;
{ void*p;
  if(p=malloc(len))
     return p;
  nlog(outofmem);exit(EX_OSERR);
}

void*trealloc(old,len)void*old;const size_t len;
{ if(old=realloc(old,len))
     return old;
  nlog(outofmem);exit(EX_OSERR);
}

void tfree(a)void*a;
{ free(a);
}

#include "shell.h"
