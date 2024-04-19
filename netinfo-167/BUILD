#! /bin/sh
# This script will build the netinfo project.
#

#
# Determine OS version on this host
#

_OS_VERSION_=MACOS_X

if [ -d /NextLibrary ]; then
	if [ -d /NextLibrary/Frameworks ]; then
		_OS_VERSION_=OPENSTEP
	else
		_OS_VERSION_=NEXTSTEP
	fi
elif [ -d /System ]; then
	if [ -f /System/Library/CoreServices/software_version ]; then
		grep Server /System/Library/CoreServices/software_version > /dev/null
		if [ $? -eq 0 ]; then
			_OS_VERSION_=MACOS_X_SERVER
		fi
	else
		_OS_VERSION_=DARWIN
	fi
fi

echo ""
echo "Building for OS version $_OS_VERSION_"
rm -f common/os.h
echo "#define _OS_VERSION_${_OS_VERSION_}_" > common/os.h
export _OS_VERSION_

#
# Set Product Version
#

TS=`date -u | awk '{printf "%d.%s.%02d %s\n", $6,$2,$3,$4}'`
TS=`echo $TS | sed 's/Jan/01/'`
TS=`echo $TS | sed 's/Feb/02/'`
TS=`echo $TS | sed 's/Mar/03/'`
TS=`echo $TS | sed 's/Apr/04/'`
TS=`echo $TS | sed 's/May/05/'`
TS=`echo $TS | sed 's/Jun/06/'`
TS=`echo $TS | sed 's/Jul/07/'`
TS=`echo $TS | sed 's/Aug/08/'`
TS=`echo $TS | sed 's/Sep/09/'`
TS=`echo $TS | sed 's/Oct/10/'`
TS=`echo $TS | sed 's/Nov/11/'`
TS=`echo $TS | sed 's/Dec/12/'`

BASE=`pwd | sed 's:.*/::'`
VERS=`echo $BASE | sed 's:.*-::'`
DEV=`echo $VERS | sed 's:.dev::'`
if [ $BASE = $VERS ]; then
  echo "#define _PROJECT_VERSION_ \"0\"" > common/project_version.h
else
  if [ $VERS = $DEV ]; then
    echo "#define _PROJECT_VERSION_ \"$VERS\"" > common/project_version.h
  else
    echo "#define _PROJECT_VERSION_ \"$DEV (`whoami` $TS UTC)\"" > common/project_version.h
  fi
fi

TARGET=netinfo
echo
echo "=================================================="
echo $TARGET
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
  echo Done
fi

if [ $MKBLD = 1 ]; then
  if [ ! -d $BLD ]; then
    mkdir $BLD
  fi
  if [ ! -d $ROOT_DIR ]; then
    mkdir $ROOT_DIR
  fi
fi

COMPONENTS="common servers tools"

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

rm -rf $ROOT_DIR/usr/lib
