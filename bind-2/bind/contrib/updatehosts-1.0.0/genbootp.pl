#! @PERL@
#
# @(#) genbootp.pl 1.16 98/12/21 @(#)

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

# generate bootp files
# files used
#	<dir>/main
#	<dir>/subnet
#

require "getopts.pl";

$dir = "db";
&Getopts("d:D:v");
$dir = $opt_d if $opt_d;

# sanity checking
-d "$dir" || die("directory $dir does not exist\n");
-f "$dir/main" || die("file $dir/main does not exist\n");
-f "$dir/subnet" || die("file $dir/subnet does not exist\n");

$verbose=$opt_v;

($sec, $min, $hour, $mday, $mon, $year, $wday, $yday, $isdst) = localtime;
$mon++;
$dateserial = sprintf("%2.2d%2.2d%2.2d%2.2d", $year, $mon, $mday, $hour);

printf STDERR "preloading the database files..." if $verbose;

# preload all the database files
open(IN , "readinfo host ip ether <$dir/main|");
@main = <IN>;
close(IN);
chop(@main);
printf STDERR "main " if $verbose;

open(IN , "readinfo subnet mask gateway dns domain time hardware<$dir/subnet|");
@subnet = <IN>;
close(IN);
chop(@subnet);
printf STDERR "subnet " if $verbose;

# open the bootptab file
open(BOOTP, ">bootptab") || die("cannot open bootptab\n");

# for each entry in the subnet table, generate a dummy entry with the
# global information for each subnet
# each subnet is just numbered per its index in the table
# e.g. N<index>
# hardware type is ether

for ($i=0; $i<=$#subnet; $i++) {
	($subnet, $sm, $gw, $ds, $dn, $ts, $ht) = split('\t', $subnet[$i]);
	$ht = "ether" if ! $ht || $ht eq "ethernet";
	print BOOTP "#Subnet:$subnet\nN.$subnet:";
	print BOOTP "sm=$sm:" if $sm;
	print BOOTP "gw=$gw:" if $gw;
	print BOOTP "ds=$ds:" if $ds;
	print BOOTP "ts=$ts:" if $ts;
	print BOOTP "dn=$dn:ht=$ht:\n\n";
}

# for each host entry in @main get the name, ip, and hardware address
for ($i=0; $i<=$#main; $i++) {
	($host, $ip, $hard) = split('\t', $main[$i]);

	# skip if no hardware address
	next if ! $hard;
	# determine the subnet this host is a part of
	# this is an index into the @subnet array
	$subnet = &getsubnet($ip);
	
	printf STDERR "subnet for $ip is $subnet\n" if $verbose;

	# sanity checking
	if ($subnet == -1) {
		print STDERR "no subnet for $ip\n";
		next;
	}

	# produce the bootptab entry
	($net) = split('\t', $subnet[$subnet]);
	printf BOOTP "%s:ip=%s:hn:ha=%s:tc=N.%s:\n\n",
		$host, $ip, $hard, $net;
}
close(BOOTPTAB);

# determine the subnet entry this host belongs to
# assumes the masks are all the same length per network or in the case of
# variable masks, the subnets are sorted from most specific to least specific
sub getsubnet {
	local($ip) = @_;

	local($i);
	local($subnet);
	local($mask);
	local($res);
	for ($i=0; $i<=$#subnet; $i++) {
		($subnet, $mask) = split('\t', $subnet[$i]);

		# apply the mask to the address
		# return in dotted decimal notation
		# see if the subnet matches

		$res = &subnet($ip, $mask);
#print STDERR "ip - $ip subnet - \"$subnet\" mask - $mask res - \"$res\"\n";
		return $i if $subnet eq $res
	}
	return -1;
}

# return the subnet number in dotted decimal notation given a mask

sub subnet {
	local($ip) = shift;
	local($mask) = shift; 

	local(@ip) = split(/\./, $ip);
	local(@mask) = split(/\./, $mask);

	return "0.0.0.0" if $#ip != 3 || $#mask != 3;

	# do a bit-wise and with each byte of the address 
	# and convert to alphanumeric

	local($res) = "";
	local(@res);
	local($i);
	for ($i=0; $i<4; $i++) {
		$res[$i] = sprintf("%d", int($ip[$i]) & int($mask[$i]));
	}
	$res = join(".", @res);
	return $res
}
