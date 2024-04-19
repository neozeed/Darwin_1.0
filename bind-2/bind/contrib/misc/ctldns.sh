Received: by gw.home.vix.com id AA01363; Wed, 27 Jul 94 21:00:45 -0700
Received: from roam.psg.com by rip.psg.com with smtp
	(Smail3.1.28.1 #7) id m0qTMdt-0002zyC; Wed, 27 Jul 94 21:00 PDT
Received: by roam.psg.com (Smail3.1.28.1 #6)
	id m0qTMje-0000RGC; Wed, 27 Jul 94 21:06 PDT
Message-Id: <m0qTMje-0000RGC@roam.psg.com>
Date: Wed, 27 Jul 94 21:06 PDT
From: randy@psg.com (Randy Bush)
To: Paul A Vixie <paul@vix.com>
Subject: ctldns

#!/bin/sh
#
# named signal commander
#

PID=`cat /etc/named.pid`

if [ -z "$1" ]; then
   echo usage: `basename $0` {reload,dump,stats,on,off,kill,start}
   fi

while [ -n "$1" ]; do
    case $1 in
        reload) kill -HUP $PID
		echo "named cache reload issued"
		;;
        dump)   kill -INT $PID
		echo "named cache dumped to /usr/tmp/named_dump.db"
		;;
        stats)  kill -6 $PID
		echo "named statistics dumped to /usr/tmp/named.stat"
		;;
        on)	kill -USR1 $PID
		echo "named debug incremented"
		;;
        off)	kill -USR2 $PID
		echo "named debug off"
		;;
        kill)   kill -9 $PID
		echo "named killed"
		;;
	start)	in.named
		echo  "named restarted"
		;;
        *)	echo "useage is $0 {reload,dump,stats,kill,start}"
		exit 1
		;;
        esac
    shift
    done

exit 0

# end
