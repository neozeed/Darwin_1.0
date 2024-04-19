#!/bin/sh

# Author: Eduard Vopicka <eda@vse.cz> 03-Feb-97
# SCO OSE5 version 01-Apr-97
# 
# NOTE WELL: This file is currently unused during BIND 8.1 / SCO OSE5 build.

TMPDIR=/tmp/tmpdir.$$
LIBSOCKET=/usr/lib/libsocket.a
LIBSOCKETNEW=$TMPDIR/libsocket.a
LIBBINDNEW=/usr/local/lib/libbind.a

### *** Don't change anything below this point *** ###

LIBBIND=libbind.a
[ -f "$LIBBIND" ] || LIBBIND=lib/libbind.a
[ -f "$LIBBIND" ] || LIBBIND=src/lib/libbind.a
[ -f "$LIBBIND" ] || LIBBIND=../../lib/libbind.a
[ -f "$LIBBIND" ] || LIBBIND=../../../lib/libbind.a
LIBPORT=libport.a
[ -f "$LIBPORT" ] || LIBPORT=port/libport.a
[ -f "$LIBPORT" ] || LIBPORT=src/port/libport.a.a
[ -f "$LIBPORT" ] || LIBPORT=../../port/libport.a
[ -f "$LIBPORT" ] || LIBPORT=../../../port/libport.a

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
  echo "File/directory $TMPDIR already exists. I am not clever enough"
  echo "to continue. Sorry, please try again."
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
  set -x
  ar t $LIBSOCKET >/dev/null || exit 1
  cp $LIBSOCKET $LIBSOCKETNEW || exit 1
  chmod 644 $LIBSOCKETNEW
  ar d $LIBSOCKETNEW hostent.o || exit 1
  ar x "$LIBBIND" || exit 1
  rm ftruncate.o gettimeofday.o mktemp.o putenv.o || exit 1
  rm readv.o setenv.o setitimer.o strcasecmp.o strdup.o || exit 1
  rm strerror.o strpbrk.o strtoul.o utimes.o writev.o || exit 1
  #ar x "$LIBPORT" getrusage.o || exit 1
  ar r $LIBSOCKETNEW *.o || exit 1
  rm -f $LIBBINDNEW || exit 1
  ar r $LIBBINDNEW *.o || exit 1
  exit 0
) || err

# rm -rf $TMPDIR || err

cat <<EOF

OK - all done, $LIBBINDNEW is updated.

     Some .o files and updated libsocket.a are left in $TMPDIR.
     For the time being, these files are probably of no practical use.

EOF
