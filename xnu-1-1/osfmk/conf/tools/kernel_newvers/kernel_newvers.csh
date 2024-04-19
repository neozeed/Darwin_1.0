#!/bin/sh -
#
# Mach Operating System
# Copyright (c) 1990 Carnegie-Mellon University
# Copyright (c) 1989 Carnegie-Mellon University
# All rights reserved.  The CMU software License Agreement specifies
# the terms and conditions for use and redistribution.
#

#
# kernel_newvers.sh	copyright major minor variant
#

major="$1"; minor="$2"; variant="$3"
v="${major}.${minor}" d="${OBJROOT}/${KERNEL_CONFIG}_${ARCH_CONFIG}" s="${SRCROOT}" h=`hostname` t=`date` w=`whoami`
if [ -z "$d" -o -z "$h" -o -z "$t" ]; then
    exit 1
fi
CONFIG=`expr "$d" : '.*/\([^/]*\)$'`
d=`expr "$d" : '.*/\([^/]*/[^/]*/[^/]*\)$'`
s=`expr "$s" : '.*/\([^/]*/[^/]*\)$'`
(
  /bin/echo "int  version_major      = ${major};" ;
  /bin/echo "int  version_minor      = ${minor};" ;
  /bin/echo "char version_variant[]  = \"${variant}\";" ;
  /bin/echo "char version[] = \"Darwin Kernel Version ${v}:\\n${t}; $w:$d\\n\\n\";" ;
  /bin/echo "char osrelease[] = \"${major}.${minor}\";" ;
  /bin/echo "char ostype[] = \"Darwin\";" ;
) > kernel_vers.c
if [ -s vers.suffix -o ! -f vers.suffix ]; then
    rm -f vers.suffix
    echo ".${variant}.${CONFIG}" > vers.suffix
fi
exit 0
