#!/bin/sh

# Author: Eduard Vopicka <eda@vse.cz> 03-Feb-97
# 10-Apr-97	Modified search/substitute pattern.
#		Thanks to Bela Lubkin for his suggestions.

SCO=sco42
TFILE=/tmp/temp$#sed

DIR=`pwd`

BASE=""
case "$DIR" in
	*/src/port/$SCO/tools)			BASE=../../..	;;
	*/src/port/$SCO)			BASE=../..	;;
	*/src/port)	[ -d $SCO ] &&		BASE=..		;;
	*/src)		[ -d port/$SCO ] &&	BASE=.		;;
esac

[ "$BASE" -a -f "$BASE/Version" -a -d "$BASE/port/$SCO/tools" ] || {
	echo "ERROR: Unable to find src directory of BIND distribution)"
	exit 1
}

find "$BASE" -name '*.c' -print | (
	while read f
		do
			grep '".*%p.*"' $f >/dev/null || continue
			echo "$f"
			[ -f "$f.dist" ] || cp "$f" "$f.dist"
			[ "$?" != 0 ] && exit 1
			sed  -e  '/".*%p.*"/s/%p/0x%08x%/g' <$f >$TFILE
			[ "$?" != 0 ] && exit 1
			cp $TFILE $f
			[ "$?" != 0 ] && exit 1
		done
	exit 0
)
X="$?"
[ "$X" = 0 ] && echo "HOPE it is OK"
[ "$X" != 0 ] && echo "ERROR !!! ERROR !!! ERROR !!!"
exit "$X"
