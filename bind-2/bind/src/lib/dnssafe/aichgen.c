/* Copyright (C) RSA Data Security, Inc. created 1993, 1996.  This is an
   unpublished work protected as such under copyright law.  This work
   contains proprietary, confidential, and trade secret information of
   RSA Data Security, Inc.  Use, disclosure or reproduction without the
   express written authorization of RSA Data Security, Inc. is
   prohibited.
 */

#include "global.h"
#include "bsafe2.h"
#include "bkey.h"
#include "balg.h"
#include "ahchgen.h"
#include "aichgen.h"

B_TypeCheck *AITChooseGenerateNewHandler (infoType, algorithm)
B_AlgorithmInfoType *infoType;
B_Algorithm *algorithm;
{
  POINTER info;

  if (B_InfoCacheFindInfo (&algorithm->infoCache, &info, (POINTER)infoType)
      != 0)
    /* This really shouldn't happen since the info was just added. */
    return ((B_TypeCheck *)NULL_PTR);

  /* Pass in NULL_PTR so that constructor will allocate. */
  return ((B_TypeCheck *)AHChooseGenerateConstructor2
          ((AHChooseGenerate *)NULL_PTR, infoType, info));
}

