#! /bin/sh
# Use this script to build nigrep if "make" is too broken to do it.
#

TARGET=nigrep
TARGET_DIR=/usr/bin

echo
echo "=================================================="
echo $TARGET
echo "=================================================="

CLEAN=0
BUILD=1

if [ ${1}x = cleanx ]; then
  CLEAN=1
  BUILD=0
fi

if [ ${1}x = freshx ]; then
  CLEAN=1
  BUILD=1
fi

BLD=$TARGET.build
OBJECT_DIR=$BLD/objects-optimized
ROOT_DIR=$BLD/root
DST_DIR=${ROOT_DIR}${TARGET_DIR}

CFLAGS="-g -Wall"
INCLUDE="-I. -I../../common"
LIBS=../../common/common.build/root/usr/lib/libcommon.a

if [ $CLEAN = 1 ]; then
  echo "Cleaning $TARGET"
  rm -f $TARGET
  rm -rf $BLD
fi

if [ $BUILD = 0 ]; then
  echo "Done"
  exit 0
fi

if [ ! -d $BLD ]; then
  mkdir $BLD
fi
if [ ! -d $OBJECT_DIR ]; then
  mkdir $OBJECT_DIR
fi
if [ ! -d $ROOT_DIR ]; then
  mkdir $ROOT_DIR
fi
if [ ! -d $DST_DIR ]; then
  if [ -f /bin/mkdirs ]; then
    mkdirs $DST_DIR
  else
    mkdir -p $DST_DIR
  fi
fi

MAKE_TARGET=0
if [ ! -f $DST_DIR/$TARGET ]; then
  MAKE_TARGET=1
fi

for c in *.[mc]
do
  OBJ=`echo $c | sed 's/..$/.o/'`
  SRC=$c
  DST=$OBJECT_DIR/$OBJ

  MAKEIT=1
  if [ -f $DST ]; then
    RECENT=`/bin/ls -1t $DST $SRC | head -1`
    if [ $RECENT = $DST ]; then
      MAKEIT=0
    fi
  fi

  if [ $MAKEIT = 1 ]; then
    MAKE_TARGET=1
    echo cc -c $CFLAGS $INCLUDE -o $DST $SRC
    cc -c $CFLAGS $INCLUDE -o $DST $SRC
  fi
done

if [ $MAKE_TARGET = 1 ]; then
  echo cc $CFLAGS -o $DST_DIR/$TARGET $OBJECT_DIR/* $LIBS
  cc $CFLAGS -o $DST_DIR/$TARGET $OBJECT_DIR/* $LIBS
fi

echo "Finished building $TARGET"
exit 0
