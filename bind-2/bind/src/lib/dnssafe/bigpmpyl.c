/* Copyright (C) RSA Data Security, Inc. created 1987, 1996.  This is an
   unpublished work protected as such under copyright law.  This work
   contains proprietary, confidential, and trade secret information of
   RSA Data Security, Inc.  Use, disclosure or reproduction without the
   express written authorization of RSA Data Security, Inc. is
   prohibited.
 */

#include "global.h"
#include "bigmath.h"

/* Single precision multiply, a is same len as b and c.
   Returns low order n bytes of result.
 */
void BigPmpyl (a, b, c, n)
UINT2 *a, *b, *c;
unsigned int  n;
{
  register unsigned int i;
  unsigned int cLen;

  BigZero (a, n);
  cLen = BigLenw (c, n);
  for (i = 0; i < n; i++) {
    if (cLen < n-i)
      a[cLen+i] = BigAcc (&a[i], (unsigned int)b[i], c, cLen);
    else
      BigAcc (&a[i], (unsigned int)b[i], c, n-i);
  }
}
