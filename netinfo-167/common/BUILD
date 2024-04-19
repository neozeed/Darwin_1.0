#! /bin/sh
# This script will build libcommon.a.
#

TARGET=common
TARGET_LIB=lib$TARGET.a
TARGET_DIR=/usr/lib

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
DERIVED_SRC_DIR=$BLD/derived_src
OBJECT_DIR=$BLD/objects-optimized
ROOT_DIR=$BLD/root
DST_DIR=${ROOT_DIR}${TARGET_DIR}

CFLAGS="-g -Wall"
INCLUDE="-I. -I$DERIVED_SRC_DIR"
LIBS=

if [ $CLEAN = 1 ]; then
  echo "Cleaning $TARGET"
  rm -rf $BLD
fi

if [ $BUILD = 0 ]; then
  echo "Done"
  exit 0
fi

if [ ! -d $BLD ]; then
  mkdir $BLD
fi
if [ ! -d $DERIVED_SRC_DIR ]; then
  mkdir $DERIVED_SRC_DIR
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

SRC=nibind_prot.x
DST=$DERIVED_SRC_DIR/nibind_prot.h
MAKEIT=1
if [ -f $DST ]; then
  RECENT=`/bin/ls -1t $DST $SRC | head -1`
  if [ $RECENT = $DST ]; then
    MAKEIT=0
  fi
fi

if [ $MAKEIT = 1 ]; then
  echo rpcgen -h -o $DST $SRC
  rpcgen -h -o $DST $SRC
fi

SRC=ni_prot.x
DST=$DERIVED_SRC_DIR/ni_prot.h
MAKEIT=1
if [ -f $DST ]; then
  RECENT=`/bin/ls -1t $DST $SRC | head -1`
  if [ $RECENT = $DST ]; then
    MAKEIT=0
  fi
fi

if [ $MAKEIT = 1 ]; then
  echo rpcgen -h -o $DST $SRC
  rpcgen -h -o $DST $SRC
fi

SRC=_lu_types.x
DST=$DERIVED_SRC_DIR/_lu_types.h
MAKEIT=1
if [ -f $DST ]; then
  RECENT=`/bin/ls -1t $DST $SRC | head -1`
  if [ $RECENT = $DST ]; then
    MAKEIT=0
  fi
fi

if [ $MAKEIT = 1 ]; then
  echo rpcgen -h -o $DST $SRC
  rpcgen -h -o $DST $SRC
fi

cd $DERIVED_SRC_DIR

SRC=../../lookup.defs
DST=lookup.h
MAKEIT=1
if [ -f $DST ]; then
  RECENT=`/bin/ls -1t $DST $SRC | head -1`
  if [ $RECENT = $DST ]; then
    MAKEIT=0
  fi
fi

if [ $MAKEIT = 1 ]; then
  echo mig lookup.defs
  mig -I/System/Library/Frameworks/System.framework/PrivateHeaders $SRC
  echo rm lookupServer.c lookupUser.c
  rm lookupServer.c lookupUser.c
fi

cd ../..

MAKE_TARGET=0
if [ ! -f $DST_DIR/$TARGET_LIB ]; then
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
  echo ar r $DST_DIR/$TARGET_LIB $OBJECT_DIR/*.o
  ar r $DST_DIR/$TARGET_LIB $OBJECT_DIR/*.o

  echo ranlib $DST_DIR/$TARGET_LIB
  ranlib $DST_DIR/$TARGET_LIB
fi

echo "Finished building $TARGET"
exit 0
