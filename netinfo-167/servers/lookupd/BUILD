#! /bin/sh
# Use this script to build lookupd if "make" is too broken to do it.
#

TARGET=lookupd

echo
echo "=================================================="
echo $TARGET
echo "=================================================="

OS=`cat ../../common/os.h | awk '{print $2}'`

TARGET_DIR=/usr/sbin
LIBS="-lobjc"

if [ ${OS}x = _OS_VERSION_NEXTSTEP_x ]; then
  TARGET_DIR=/usr/etc
  LIBS=
fi

if [ ${OS}x = _OS_VERSION_OPENSTEP_x ]; then
  TARGET_DIR=/usr/etc
  LIBS=
fi

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
INCLUDE="-I. -I$DERIVED_SRC_DIR -I../../common"

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

SRC=_lu_types.x
DST=$DERIVED_SRC_DIR/_lu_types_xdr.c
MAKEIT=1
if [ -f $DST ]; then
  RECENT=`/bin/ls -1t $DST $SRC | head -1`
  if [ $RECENT = $DST ]; then
    MAKEIT=0
  fi
fi

if [ $MAKEIT = 1 ]; then
  echo rpcgen -c -o $DST $SRC
  rpcgen -c -o $DST $SRC
fi

cd $DERIVED_SRC_DIR

SRC=../../lookup.defs
DST=lookupServer.c
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
  echo rm lookupUser.c
  rm lookupUser.c
fi

cd ../..

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

for c in $DERIVED_SRC_DIR/*.[mc]
do
  SRCFILE=`echo $c | sed 's:.*/::'`
  OBJ=`echo $SRCFILE | sed 's/..$/.o/'`
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
