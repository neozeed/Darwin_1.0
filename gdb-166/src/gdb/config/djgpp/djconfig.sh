#!/bin/sh
# 
# This shell script is a wrapper to the main configure script when
# configuring GDB for DJGPP.  99% of it can also be used when
# configuring other GNU programs for DJGPP.
#
# Originally written by Robert Hoehne, revised by Eli Zaretskii.
#
# Call it like the main configure script with one exception.  If you
# want to pass parameters to configure, you have to pass as the first
# argument the srcdir, even when it is `.' !!!!!

# Where are the sources? If you are used to having the sources
# in a separate directory and the objects in another, then set
# here the full path to the source directory and run this script
# in the directory where you want to build gdb!!
# You might give the source directory on commandline, but use
# then only forward slashes (/) in the directories. It should be
# an absolute path.

if [ x$1 = x ]; then
  srcdir=`pwd`
else
  srcdir=`cd $1 && pwd`
  shift
fi

# Make sure they don't have some file names mangled by untarring.
echo -n "Checking the unpacked distribution..."
if ( ! test -f ${srcdir}/intl/po2tblsed.in || \
     ! test -d ${srcdir}/gdb/testsuite/gdb.cxx || \
     ! test -f ${srcdir}/readline/config.h-in ) ; then
  echo " FAILED."
  echo ""
  echo "You MUST unpack the sources with the DJTAR command, like this:"
  echo ""
  echo "         djtar -x -n fnchange.lst gdb-X.YZ.tar.gz"
  echo ""
  echo "where X.YZ is the GDB version, and fnchange.lst can be found"
  echo "in the gdb/config/djgpp/ directory in the GDB distribution."
  echo "configure FAILED!"
  exit 1
else
  echo " ok."
fi

# Where is the directory with DJGPP-specific scripts?
DJGPPDIR=${srcdir}/gdb/config/djgpp

echo "Editing configure scripts for DJGPP..."
TMPFILE="${TMPDIR-.}/cfg.tmp"

# We need to skip the build directory if it is a subdirectory of $srcdir,
# otherwise we will have an infinite recursion on our hands...
if test "`pwd`" == "${srcdir}" ; then
  SKIPDIR=""
  SKIPFILES=""
else
  SKIPDIR=`pwd | sed -e "s|${srcdir}|.|"`
  SKIPFILES="${SKIPDIR}/*"
fi
for fix_dir in \
  `cd $srcdir && find . -type d ! -ipath "${SKIPDIR}" ! -ipath "${SKIPFILES}"`
do
  if test ! -f ${fix_dir}/configure.orig ; then
    if test -f ${srcdir}/${fix_dir}/configure ; then
      mkdir -p ${fix_dir}
      cp -p ${srcdir}/${fix_dir}/configure ${fix_dir}/configure.orig
    fi
  fi
  if test -f ${fix_dir}/configure.orig ; then
    sed -f ${DJGPPDIR}/config.sed ${fix_dir}/configure.orig > $TMPFILE
    update $TMPFILE ${fix_dir}/configure
    touch ./${fix_dir}/configure -r ${fix_dir}/configure.orig
    rm -f $TMPFILE
  fi
  if test -f ${fix_dir}/INSTALL ; then
    mv ${fix_dir}/INSTALL ${fix_dir}/INSTALL.txt
  fi
done

# Now set the config shell. It is really needed, that the shell
# points to a shell with full path and also it must conatain the
# .exe suffix. I assume here, that bash is installed. If not,
# install it. Additionally, the pathname must not contain a
# drive letter, so use the /dev/x/foo format supported by versions
# of Bash 2.03 and later, and by all DJGPP programs compiled with
# v2.03 (or later) library.
export CONFIG_SHELL=/dev/env/DJDIR/bin/sh.exe

# force to have the ltmain.sh script to be in DOS text format,
# otherwise the resulting ltconfig script will have mixed
# (UNIX/DOS) format and is unusable with Bash ports before v2.03.
utod $srcdir/ltmain.sh

# Give the configure script some hints:
export LD=ld
export CC=gcc
export RANLIB=ranlib
export DEFAULT_YACC="bison -y"
export YACC="bison -y"
export DEFAULT_LEX=flex
# Define explicitly the .exe extension because on W95 with LFN=y
# the check might fail
export am_cv_exeext=.exe

# The configure script needs to see the `install-sh' script, otherwise
# it decides the source installation is broken.  But "make install" will
# fail on 8+3 filesystems if it finds a file `install-', since there
# are numerous "install-foo" targets in Makefile's.  So we rename the
# offending file after the configure step is done.
if test ! -f ${srcdir}/install-sh ; then
  if test -f ${srcdir}/install-.sh ; then
    mv ${srcdir}/install-.sh ${srcdir}/install-sh
  fi
fi

# Now run the configure script while disabling some things like the NLS
# support, which is nearly impossible to be supported in the current way,
# since it relies on file names which will never work on DOS.
echo "Running the configure script..."
$srcdir/configure --srcdir="$srcdir" --prefix='${DJDIR}' \
  --disable-shared --disable-nls --verbose $*

if test -f ${srcdir}/install- ; then
  mv ${srcdir}/install- ${srcdir}/install-.sh
fi
