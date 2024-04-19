if [ "`/usr/bin/uname`" != "Mac OS" ]  
then
export NEXT_ROOT=/Local/Public/MacOSX
fi
export SRCROOT=$(pwd)
export OBJROOT=$SRCROOT/BUILD/obj
export DSTROOT=$SRCROOT/BUILD/dst
export SYMROOT=$SRCROOT/BUILD/sym
