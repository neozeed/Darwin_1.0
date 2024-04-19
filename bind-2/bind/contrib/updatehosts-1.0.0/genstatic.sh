#!/bin/sh
# %Z genstatic.sh 1.12 98/12/21 %%Z genstatic.sh 1.12 98/12/21 @(#)

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

# generate the static hosttable from database files
# called from updatehosts
#
# files used
#	db/main
#	db/cname

# this cannot be /etc/hosts
HOSTS=${HOSTS:-static}
PATH=.:$PATH

# extract host name and IP address from db/main
readinfo ip host <db/main  | @AWK@ '
{
	# split up the domain name
	n = split($2, domain, ".")
	printf("%s %s %s\n", $1, $2, domain[1])
}' >$HOSTS

# build edit script for aliases
readinfo host alias <db/cname | @AWK@ '
{
	# extract simple alias name from domain name
	n = split($2, alias, ".")
	# split up the domain name
	n = split($1, domain, ".")
	# build the edit command for this line
	# will look like /<domain>/s/$/<alias> <alias.domain>/
	edcmd = "/" domain[1]
	# replace "." with "\."
	for (i=2; i<=n; i++) {
		edcmd = edcmd "\\." domain[i]
	}
	edcmd = edcmd "/s/$/ " $2 " " alias[1] "/"
	print edcmd
}
END {
	print "w"
	print "q"
}' | ed >/dev/null 2>&1 $HOSTS
