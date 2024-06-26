/* Copyright (C) RSA Data Security, Inc. created 1986, 1996.  This is an
   unpublished work protected as such under copyright law.  This work
   contains proprietary, confidential, and trade secret information of
   RSA Data Security, Inc.  Use, disclosure or reproduction without the
   express written authorization of RSA Data Security, Inc. is
   prohibited.
 */

#include "global.h"
#include "bigmath.h"

/* Comparison operator.
   BigCmp (a, b, n) -- returns sign of a-b.
 */
int BigCmp (a, b, n)
UINT2 *a, *b;
unsigned int n;
{
  register int i;
  int aSign = BigSign (a, n), bSign = BigSign (b, n);

  if (aSign > bSign)
    return (1);
  if (aSign < bSign)
    return (-1);
  
  for (i = n-1; i >= 0 && a[i] == b[i]; i--);
  
  if (i == -1)
    return (0);
  if (a[i] > b[i])
    return (1);
  return (-1);
}
