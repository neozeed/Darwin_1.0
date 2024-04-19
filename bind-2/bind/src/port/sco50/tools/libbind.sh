#!/bin/sh

# Author: Eduard Vopicka <eda@vse.cz> 01-May-97
#
# This script takes lib/libbind.a and creates port/sco50/libbind.a
# and port/sco50/libbind.so. Depending on the compilation options,
# only one of the resulting libraries is useful. This is why we need
# the extra "tricky" compilation pass.

TMPDIR=/tmp/tmpdir.$$
LIBBIND=../../lib/libbind.a
LIBPORT=../libport.a
NEWBIND=libbind.a				

### *** Don't change anything below this point *** ###

case "$NEWBIND" in
  /*) continue;;
  *)  NEWBIND="`pwd`/$NEWBIND"
esac
NEWBINDSO=`/bin/dirname $NEWBIND`/`/bin/basename $NEWBIND .a`.so

err() {
  #rm -rf $TMPDIR
  echo ""
  echo "ERROR: Something went wrong, sorry..."
  echo ""
  exit 1
}

echo "Current directory: `pwd`"

case "$LIBBIND" in
  /*) continue;;
  *)  LIBBIND="`pwd`/$LIBBIND"
esac

[ -f "$LIBBIND" ] || {
  echo 'Unable to find libbind.a. Sorry...'
  exit 1
}
echo "Found libbind.a: $LIBBIND"

case "$LIBPORT" in
  /*) continue;;
  *)  LIBPORT="`pwd`/$LIBPORT"
esac

[ -f "$LIBPORT" ] || {
  echo 'Unable to find libport.a. Sorry...'
  exit 1
}
echo "Found libport.a: $LIBPORT"

[ -f /tmp/tmp.$$ ] && {
  echo "File/directory $TMPDIR already exists. I am not"
  echo "clever enough to continue. Sorry, please try again."
  exit 1
}

mkdir "$TMPDIR" || {
  echo "Unable to create temporary directory $TMPDIR"
  exit 1
}

( cd $TMPDIR || {
    echo "Unable to chdir to $TMPDIR"
    exit 1
  }
  #set -x
  /bin/ar x "$LIBBIND" || exit 1
  /bin/rm ftruncate.o gettimeofday.o mktemp.o putenv.o || exit 1
  /bin/rm readv.o setenv.o setitimer.o strcasecmp.o strdup.o || exit 1
  /bin/rm strerror.o strpbrk.o strtoul.o utimes.o writev.o || exit 1
  ###/bin/ar x "$LIBPORT" getrusage.o || exit 1
  /bin/rm -f $NEWBIND || exit 1
  /bin/rm -f $NEWBINDSO || exit 1
  /bin/ar r $NEWBIND *.o || exit 1
  /bin/cc -b elf -G -o $NEWBINDSO *.o || exit 1
  exit 0
) || err

rm -rf $TMPDIR || err

cat <<EOF

libbind.sh -- OK,
          $NEWBIND
      and
          $NEWBINDSO
      have been created.

EOF
