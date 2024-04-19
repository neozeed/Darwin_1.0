if ("`/usr/bin/uname`" != "Mac OS" ) then
setenv NEXT_ROOT /Local/Public/MacOSX
endif
setenv SRCROOT `pwd`
setenv OBJROOT $SRCROOT/BUILD/obj
setenv DSTROOT $SRCROOT/BUILD/dst
setenv SYMROOT $SRCROOT/BUILD/sym
