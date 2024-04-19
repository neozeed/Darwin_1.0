Return-Path: bind-workers-request
Received: by gw.home.vix.com id AA15120; Tue, 15 Feb 94 16:23:37 -0800
Received: by gw.home.vix.com id AA15116; Tue, 15 Feb 94 16:23:32 -0800
Received: by hot.ee.lbl.gov for bind-workers@vix.com (5.65/1.43r)
	id AA03216; Tue, 15 Feb 94 16:23:16 -0800
Message-Id: <9402160023.AA03216@hot.ee.lbl.gov>
To: bind-workers@vix.com
Subject: Re: Suggested HACK to allow recovery after SOA typo ...
Date: Tue, 15 Feb 94 16:23:15 PST
From: Craig Leres <leres@ee.lbl.gov>

People have mentioned making typos when updating the SOA serial; I
wrote the appended script and we don't have this problem anymore.
It does require that the file be writable (we keep our dns
files in RCS) and that the serial number on a line by itself
and directly follow the soa, e.g.:

@               in      soa     ns1.lbl.gov. hostmaster.ns1.lbl.gov. (
                                        94021509
                                        14400   ; refresh
                                        1800    ; retry
                                        3600000 ; expire
                                        86400 ) ; minimum


		Craig
------
#!/bin/csh -f
# @(#) $Header: /cvs/Darwin/Services/bind/bind/contrib/misc/update-soa.csh,v 1.1.1.1 1999/10/04 22:25:54 wsanchez Exp $ (LBL)
#
# updateserial - Update the SOA serial in the specified files.
#

set prog=$0
set prog=$prog:t

# Construct a serial of the form YYMMDDHH
set m="-e s/Jan/01/ -e s/Feb/02/ -e s/Mar/03/ -e s/Apr/04/"
set m="$m -e s/May/05/ -e s/Jun/06/ -e s/Jul/07/ -e s/Aug/08/"
set m="$m -e s/Sep/09/ -e s/Oct/10/ -e s/Nov/11/ -e s/Dec/12/"

set serial=`date | sed $m -e 's/^... \(..\) \(..\) \(..\):..:.. ... ..\(..\)/\4\1\2\3/' -e 's/ /0/g'`

# If wrong number of arguments, give usage.
if ( $#argv < 1 ) then
    echo "usage: $prog dns-file ..."
    echo "(Current serial is $serial)"
    exit 1
endif

# Loop through arguments and process.
while ($#argv > 0)
    # Don't munge file unless writable
    if (! -w $1) then
	echo "Warning: $1 not writable, skipping..."
	shift
	continue
    endif
    # Extract old serial
    set temp=$1.$$
    mv $1 $temp
    set oldserial=`head -50 $temp | sed -n -e "/[iI][nN][ 	][ 	]*[sS][oO][aA]/{n" -e "s/[^0-9]*\([0-9][0-9]*\).*/\1/p" -e "}"`

    # Insure new serial is at least one higher than old serial
    set newserial=$serial
    if ("$oldserial" != "") set newserial=`echo $oldserial $newserial | awk -e '{if ($1 >= $2) $2 = $1+1; print $2}'`

    # Munge the file
    sed -e "/[iI][nN][ 	][ 	]*[sS][oO][aA]/{n" -e "s/[0-9][.0-9]*/$newserial/" -e "}" \
	$temp > $1
#    diff $1 $temp >&/dev/null
#    if ( $status == 0 ) then
#	echo "Warning: serial didn't change, restoring $1"
#	mv -f $temp $1
#    else
	rm -f $temp
#    endif
    shift
end

exit
