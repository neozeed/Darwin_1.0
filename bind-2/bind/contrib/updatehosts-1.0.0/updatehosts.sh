#!/bin/sh
# @(#) updatehosts.sh 1.53 99/01/05 @(#)

# updatehosts DNS maintenance package
# Copyright (C) 1998  Smoot Carl-Mitchell
# 
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
# 
# smoot@tic.com

# update the host tables and DNS files
#
# arguments are the files to edit

CONFIG_FILE=@bindir@/updatehosts.env
DB_DIR=/var/named/db
EDITOR=${EDITOR-vi}
JUST_D=
NAMED_DIR=/var/named
POSTPROCESS=
STATICHOSTS=static
USERCS=
USESCCS=1
VERSION=8

update=1
verbose=
while getopts D:d:f:nsv option; do
	case $option in
	D) $JUST_D=$OPTARG ;;
	d) DB_DIR=$OPTARG ;;
	f) CONFIG_FILE=$OPTARG ;;
	n) update= ;;
	r) USERCS=1 ;;
	s) USESCCS=1 ;;
	v) verbose=-v ;;
	esac
done
shift `expr $OPTIND - 1`

# look for the startup file which sets global environment variables
# this file must be present
if [ ! -f $CONFIG_FILE ]; then
	echo $CONFIG_FILE not found
	exit 1
fi
. $CONFIG_FILE

list=
# check the filename arguments
for i do
	found=
	for j in $DB_FILES; do
		if [ $i = $j ]; then
			found=1
			break
		fi
	done
	if [ ! "$found" ]; then
		echo $i is not a valid filename
		list=1
	fi
done
if [ "$list" ]; then
	echo legal filenames are:
	echo $DB_FILES
	exit 1
fi

cd $NAMED_DIR || ( echo no $NAMED_DIR directory; exit 1 )

# set umask to group read and write
umask 2
# check if the files exist and are under SCCS control and are readable
#
# files are in a subdirectory - so we cd there
cd $DB_DIR || ( echo no $DB_DIR present; exit 1 )

okay=1
if [ "$USESCCS" ]; then for i do
	if [ ! -f SCCS/s.$i ]; then
		echo "file $i is not under SCCS control"
		okay=0
	elif [ ! -r SCCS/s.$i ]; then
		echo "file $i is not readable by you"
		okay=0
	elif [ -f SCCS/p.$i ]; then
		echo "file $i is already checked out under SCCS"
		okay=0
	elif [ -f $i ]; then
		rm -f $i
	fi
done
elif [ "$USERCS" ]; then for i do
	if [ ! -f RCS/$i,v ]; then
		echo "file $i is not under RCS control"
		okay=0
	elif [ ! -r RCS/$i,v ]; then
		echo "file $i is not readable by you"
		okay=0
	elif [ -f $i ]; then
		rm -f $i
	fi
done fi

if [ $okay -eq 0 ]; then
	exit 1
fi

# checkout and edit the files
for i do
	if [ "$USESCCS" ]; then
		sccs edit $i
	elif [ "$USERCS" ]; then
		co -l $i
	fi
	$EDITOR $i
	if [ "$USESCCS" ]; then
		sccs delget $i
	elif [ "$USERCS" ]; then
		ci $i
	fi
done

cd $NAMED_DIR

# generate the static tables
if [ $STATICHOSTS ]; then
	echo "generating static hosts table..."
	HOSTS=$STATICHOSTS genstatic
fi

# generate the dns zone files
echo "generating the DNS database..."
if [ "$JUST_D" ]; then
	gendns $verbose -D "$JUST_D" -d $DB_DIR
else
	gendns $verbose -d $DB_DIR
fi

# generate the bootptab file 
# but only if we are instructed to
if [ $GENBOOTP ]; then
	echo "generating the bootptab file..."
	genbootp

	# copy it to alternate machines
	for m in $SECBOOTP; do
		cp -p bootptab /net/$SECBOOTP/etc
	done
fi

# generate the dhcptab file 
# but only if we are instructed to
if [ $GENDHCP ]; then
	echo "generating the dhcpd.conf file..."
	if [ $verbose ]; then
		verbose=-v
	fi
	if [ $KNOWNHOSTS ]; then
		known=-k
	fi
	gendhcp $verbose $known

	# copy it to alternate machines
	for m in $SECDHCP; do
		cp -p dhcpd.conf /net/$SECDHCP/etc/dhcpd.conf
	done

	# restart the server
	poke_dhcp >/dev/null 2>&1
fi

# run postprocessor command
if [ "$POSTPROCESS" ]; then
	echo running $POSTPROCESS
	eval $POSTPROCESS
fi

if [ $update ]; then
	poke_ns restart
fi
