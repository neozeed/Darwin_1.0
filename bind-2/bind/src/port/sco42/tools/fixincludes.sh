#!/bin/sh

INCDIR="/usr/include"
INCFILES="netdb.h resolv.h arpa/inet.h arpa/nameser.h"
BITYPES=/usr/include/sys/bitypes.h
CDEFS=/usr/include/sys/cdefs.h


[ "`/usr/bin/id`" = "uid=0(root) gid=1(other)" ] || {
  echo "ERROR: You must do this as root."
  exit 1
}

[ -f "$BITYPES" ] || {
   echo "ERROR: File $BITYPES does not exist."
   exit 1
}

[ -f "$CDEFS" ] || {
   echo "ERROR: File $CDEFS does not exist."
   exit 1
}

cp $BITYPES `dirname $BITYPES`/BIND`basename $BITYPES` || exit 1
cp $CDEFS `dirname $CDEFS`/BIND`basename $CDEFS` || exit 1

for f in $INCFILES
  do
    	FILE="$INCDIR/$f"
	echo "Fixing $FILE"
	sed  -e 's|<sys/bitypes.h>|<sys/BINDbitypes.h>|g' \
	     -e 's|<sys/cdefs.h>|<sys/BINDcdefs.h>|g' \
		<$FILE >/tmp/temp$#sed

	[ "$?" = 0 ] && mv /tmp/temp$#sed $FILE
	[ "$?" = 0 ] || exit 1
  done

rm $BITYPES || exit 1
rm $CDEFS || exit 1

echo "OK - All done."
