#!/bin/sh
#lamers.sh,v 1.1.1.1 1995/12/08 03:45:02 kim Exp
#
# From: Kimmo Suominen <kim@tac.nyc.ny.us>
#
# Lame delegation notifier
#
# -----------------------------------------------------------------------
# COPYRIGHT FOR THE ORIGINAL SCRIPT
# -----------------------------------------------------------------------
#  Copyright (c) 1991, 1994 Regents of the University of Michigan.
#  All rights reserved.
#
#  Redistribution and use is permitted provided that this notice 
#  is preserved and that due credit is given to the University of 
#  Michigan. The name of the University may not be used to endorse 
#  or promote products derived from this software without specific 
#  prior written permission. This software is provided "as is" 
#  without express or implied warranty.
#
#  Original author:  Bryan Beecher
# -----------------------------------------------------------------------

# -----------------------------------------------------------------------
# To use this software, you will need to have built BIND 4.9.3 or a later
# version) with LAME_DELEGATION defined in options.h.  You will also need
# to be sure that the 'dig' program resides in the current PATH.
# -----------------------------------------------------------------------

# -------------------------------------------------------------
#  tailor these to your system
# -------------------------------------------------------------

##
##  Where are lame delegations recorded?
##
LOGFILE="@LOGFILE@"

##
##  Where should scratch files be kept?
##
TMP="@TEMPDIR@"

##
##  Who should get the main "lame delegations" report?
##
ADMIN="@ADMIN@"

##
##  Mailer to use
##
MAILER="@MAILER@"

##
##  Mail should show up as having come from this address
##
SENDER="@SENDER@"

# -------------------------------------------------------------
#  you shouldn't need to change any of these
# -------------------------------------------------------------
LAMERS=${TMP}/lamers$$
LAMEREPORT=${TMP}/.lamereport$$
REPORT=${TMP}/.report.$$

# -------------------------------------------------------------
#  handle arguments
# -------------------------------------------------------------
#	-f <logfile>
#	Change the LOGFILE.
#
#	-v
#	Be verbose.
#
#	-e
#	Echo messages that would have been sent as e-mail.
#
#	-t
#	Test mode.  Do not send mail to the lame delegation
#	hostmasters.
#
#	-a <admin>
#	Specify a different admin.
# -------------------------------------------------------------
VERBOSE=0
TESTMODE=0
while [ $# != 0 ] ; do
	case "$1" in

		-e)
		MAILER=/bin/cat
		;;

		-f)
		LOGFILE=$2
		shift
		;;

		-v)
		VERBOSE=1
		;;

		-t)
		TESTMODE=1
		;;

		-a)
		ADMIN=$2
		shift
		;;
	esac
	shift
done

#--------------------------------------------------------------------------
#  Clean up and exit on a HUP, INT or QUIT
#--------------------------------------------------------------------------
trap "rm -f ${LAMERS} ${LAMEREPORT} ; exit" 1 2 3

if [ ${TESTMODE} -eq 1 ] ; then
	echo "Operating in test mode."
	echo "Notifications sent via: ${MAILER}"
fi

#--------------------------------------------------------------------------
#  See if there are any lamers
#
# Nov 30 04:20:17 grendel named[758]:
# Lame server on 'ifcsun1.ifisiol.unam.mx' (in 'ifisiol.UNAM.MX'?):
# [132.248.1.3].53 'danzon.astroscu.unam.mx':
# learnt (A=192.33.4.12,NS=132.248.1.3)
#
#--------------------------------------------------------------------------
grep "Lame server on" ${LOGFILE} | grep -v "*" | tr '[A-Z]' '[a-z]' |
    sed -e "s/[^']*'\([^']*\)'[^']*'\([^']*\)'[^[]*\[\([^]]*\).*/\2 \3/" |
    sort | uniq > ${LAMERS}

if [ ! -s ${LAMERS} ] ; then
	exit 0
fi

if [ ${VERBOSE} -eq 1 ] ; then
	echo "Found" `awk 'END { print NR }' ${LAMERS}` "lame delegations"
fi

#--------------------------------------------------------------------------
#  There were lamers; send them mail
#--------------------------------------------------------------------------
touch ${LAMEREPORT}
NAME=""
while read DOMAIN IPADDR ; do
	#-----------------------------------------------------------
	# Echo args if verbose
	#-----------------------------------------------------------
	if [ ${VERBOSE} -eq 1 ] ; then
		echo ""
		echo "-> ${IPADDR} may be a lame delegation for ${DOMAIN}"
	fi
	#-----------------------------------------------------------
	# Lookup the SOA record form ${DOMAIN}.  A really broken name
	# server may have more than one SOA for a domain, so exit
	# after finding the first one.  Send it to the local hostmaster
	# if we cannot find the proper one.
	#-----------------------------------------------------------
	if [ ${VERBOSE} -eq 1 ] ; then
		echo "   Looking up the hostmaster for ${DOMAIN}"
	fi
	HOSTMASTER=`dig ${DOMAIN} SOA 2> /dev/null |
	    awk '$3 == "SOA" { print substr($5, 1, length($5) - 1) ; exit }' |
	    sed -e 's/@/./' | sed -e 's/\./@/'`
	NAME=`dig -x ${IPADDR} 2> /dev/null |
	    awk '$3 == "PTR" { print substr($4, 1, length($4) - 1) ; exit }'`
	if [ -z "${HOSTMASTER}" ] ; then
	    if [ ! -z ""${NAME} ] ; then
		HOSTMASTER="postmaster@${NAME}"
	    else
		HOSTMASTER=""
	    fi
	fi
	HOSTMASTER="`echo ${HOSTMASTER} | tr '[A-Z]' '[a-z]'`"
	if [ ${VERBOSE} -eq 1 -a -z "${HOSTMASTER}" ] ; then
		echo "   Could not locate an appropriate e-mail address"
	elif [ ${VERBOSE} -eq 1 ] ; then
		echo "   "Hostmaster is "${HOSTMASTER}"
	fi
	#-----------------------------------------------------------
	# Find the name associated with IP address ${IPADDR}.  Query
	# the nameserver at that address:  If it responds listing
	# itself as a domain namserver, then it is lame; if it isn't
	# in the list, then perhaps the lame delegation alert was
	# spurious.
	#-----------------------------------------------------------
	if [ ${VERBOSE} -eq 1 ] ; then
		echo -n "   Is ${IPADDR} listed as a NS for ${DOMAIN}?  "
	fi
	dig @${IPADDR} ${DOMAIN} NS 2>&1 | grep "A	${IPADDR}" > /dev/null
	if [ $? -eq 1 ] ; then
		if [ ${VERBOSE} -eq 1 ] ; then
			echo "No, skipping."
		fi
		continue
	else
		if [ ${VERBOSE} -eq 1 ] ; then
			echo "Yes."
		fi
	fi
	#-----------------------------------------------------------
	# If the delegation is no longer lame, don't send mail.
	# We do the query twice; the first answer could be authori-
	# tative even if the nameserver is not performing service
	# for the domain.  If this is the case, then the second
	# query will come from cached data, and will be exposed
	# on the second query.  If the resolver returns trash, the
	# entire set of flags will be set.  In this case, don't
	# count the answer as authoritative.
	#-----------------------------------------------------------
	if [ ${VERBOSE} -eq 1 ] ; then
		echo -n "   Data returned from ${IPADDR} is from the "
	fi
	dig @${IPADDR} ${DOMAIN} > /dev/null
	dig @${IPADDR} ${DOMAIN} | grep flags | grep aa | grep -v tc > /dev/null
	if [ $? -eq 0 ] ; then
		if [ ${VERBOSE} -eq 1 ] ; then
			echo "hash table (authoritative)."
		fi
		continue
	fi
	if [ ${VERBOSE} -eq 1 ] ; then
		echo "cache (non-authoritative)."
	fi
	#-----------------------------------------------------------
	# Notify the owner of the lame delegation, and also notify
	# the local hostmaster.
	#-----------------------------------------------------------
	if [ -z "${HOSTMASTER}" ] ; then
		continue
	fi
	if [ ${VERBOSE} -eq 1 ] ; then
		echo "  "
	fi
	echo "${IPADDR}:${DOMAIN}:${NAME}:${HOSTMASTER}" >> ${LAMEREPORT}
done < ${LAMERS}

#--------------------------------------------------------------------------
# Message leader and trailer
#--------------------------------------------------------------------------
doleader() {
	rm -f ${REPORT}
	cat <<!EOF_LEADER >> ${REPORT}
From: ${SENDER}
To: $1
Subject: Lame delegation report

This message was machine generated.  It is intended to alert you
to a possible problem with the nameservers for the domains listed
below.  If you have questions about this message, please contact
${SENDER}

This nameserver  was found to be a lame delegation for this domain
---------------  -------------------------------------------------
!EOF_LEADER
}
AWKFMT="awk -F: '{ printf("'"%-16s %s\n", $1, $2) }'"'"

dotrailer() {
	if [ -s ${REPORT} ] ; then
		cat <<!EOF_TRAILER >> ${REPORT}

The following paragraphs describe what a lame delegation is and suggests
some things you might do to eliminate the lame delegation.  If you are
an experienced hostmaster, you probably do not to read the rest of this
message; if you don't know what a lame delegation is, you probably want
to keep reading.

A lame delegation is an instance when a nameserver has been listed as
authoritative for a domain, but does not seem to be performing
nameservice for that domain.  A lame delegation can usually be easily
remedied by either fixing some small error in a configuration file, or
by contacting the appropriate hostmaster and supplying him/her with
up-to-date information.

Two pieces of information are listed above.  The first one is the name
of the domain that was flagged, and the second one is the lame
delegation -- that is, it is a purported nameserver for the domain.

If the nameserver should be performing nameservice for the listed
domain, it may be useful to inspect it.  Is the zone for that domain
expiring?  If the nameserver is performing secondary service, does it
have the correct IP address of the master server(s)?  Again, if the
nameserver is performing secondary service, is its copy of the SOA
record serial number greater than the one held by the master server? Is
there a typographical error or omission in the boot file?

If the nameserver should not be listed as a nameserver for the domain,
contact the hostmaster for that domain and ask that the NS record
pointing to your nameserver be deleted.  If your machine is not a
nameserver for the domain, it should not be listed as such.

Thank you for your time and your patience.
!EOF_TRAILER
		${MAILER} < ${REPORT}
	fi
	rm -f ${REPORT}
}
#--------------------------------------------------------------------------
# Tell hostmasters
#--------------------------------------------------------------------------
OHM="nobody"
if [ ${TESTMODE} -eq 0 ] ; then
	sort -t: +3 -4 ${LAMEREPORT} | while read LAMELINE ; do
		OIFS="${IFS}"
		IFS=:
		set x ${LAMELINE}
		IFS="${OIFS}"
		if [ "$5" != "${OHM}" ] ; then
			dotrailer
			doleader "$5"
			OHM="$5"
		fi
		echo "${LAMELINE}" | eval ${AWKFMT} >> ${REPORT}
	done
	dotrailer
fi

#--------------------------------------------------------------------------
# No news is good news
#--------------------------------------------------------------------------
if [ -s ${LAMEREPORT} ] ; then
	doleader "${ADMIN}"
	sort -t. -n +0 -1 +1 -2 +2 -3 +3 -4 ${LAMEREPORT} |
	    eval ${AWKFMT} >> ${REPORT}
	dotrailer
fi

#--------------------------------------------------------------------------
# Tidy up
#--------------------------------------------------------------------------
rm -f ${LAMERS} ${LAMEREPORT} ${REPORT}
