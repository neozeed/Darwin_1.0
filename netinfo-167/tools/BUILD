#! /bin/sh
# This script will build netinfo/tools.
#

TARGET=tools

echo
echo "=================================================="
echo Building $TARGET
echo "=================================================="

BLD=$TARGET.build
ROOT_DIR=$BLD/root

CLEAN=0
MKBLD=1

if [ ${1}x = cleanx ]; then
  CLEAN=1
  MKBLD=0
fi
if [ ${1}x = freshx ]; then
  CLEAN=1
fi

if [ $CLEAN = 1 ]; then
  echo "Cleaning $TARGET"
  rm -rf $BLD
fi

if [ $MKBLD = 1 ]; then
  if [ ! -d $BLD ]; then
    mkdir $BLD
  fi
  if [ ! -d $ROOT_DIR ]; then
    mkdir $ROOT_DIR
  fi
fi

COMPONENTS="nidomain nicl nidump nifind nigrep niload nireport niutil"

for i in $COMPONENTS
do
  cd $i
  sh BUILD $1
  if [ -d $i.build/root ]; then
    cd $i.build/root
	cp -r . ../../../$ROOT_DIR
	cd ../..
  fi
  cd ..
done
