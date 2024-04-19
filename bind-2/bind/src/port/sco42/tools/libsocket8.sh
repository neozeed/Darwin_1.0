#!/bin/sh

# Author: Eduard Vopicka <eda@vse.cz> 03-Feb-97

LIBSOCKET=/usr/lib/libsocket.a
TMPDIR=/tmp/tmpdir.$$

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

### *** Don't change anything below this point *** ###

err() {
  rm -rf $TMPDIR
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

[ -f $LIBSOCKET.orig ] || {
  echo "$LIBSOCKET.orig does not exist. Plase make a backup"
  echo "copy of $LIBSOCKET to $LIBSOCKET.orig"
  exit 1
}

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
  ar t $LIBSOCKET.orig >/dev/null || exit 1
  cp $LIBSOCKET.orig $LIBSOCKET || exit 1
  ar d $LIBSOCKET hostent.o || exit 1
  ar x "$LIBBIND" || exit 1
  rm strcasecmp.o strdup.o strerror.o strpbrk.o strtoul.o || exit 1
  rm gettimeofday.o || exit 1
  ar x "$LIBPORT" ansi_realloc.o sco_gettime.o || exit 1
  ar r $LIBSOCKET *.o || exit 1
) || err

rm -rf $TMPDIR || exit 1

echo ""
echo "OK - all done"
echo ""
