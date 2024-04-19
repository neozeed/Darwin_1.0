#! @PERL@
#
# @(#) gendhcp.pl 1.18 99/01/04 @(#)

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

# generate DHCP configuration files
# files used
#	<dir>/main
#	<dir>/subnet
#

require "getopts.pl";

$dir = "db";
&Getopts("d:v:k");
$dir = $opt_d if $opt_d;
$verbose=$opt_v;

# only boot known hosts
$known=$opt_k;

# sanity checking
-d "$dir" || die("directory $dir does not exist\n");
-f "$dir/main" || die("file $dir/main does not exist\n");
-f "$dir/subnet" || die("file $dir/subnet does not exist\n");

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

open(IN , "readinfo subnet mask gateway dns domain time hardware dhcprange maxdhcplease dynamicbootp <$dir/subnet|");
@subnet = <IN>;
close(IN);
chop(@subnet);
printf STDERR "subnet " if $verbose;

# open the dhcpd.conf file
open(DHCPD, ">dhcpd.conf") || die("cannot open dhcpd.conf\n");

# generate a bit of global information
$hostname = `hostname`;
chop($hostname);
print DHCPD "server-identifier $hostname;\n";

# do we allow unknown cients to boot?
print DHCPD "boot-unknown-clients false;\n" if $known;

# for each entry in the subnet table, generate an entry with the
# global information for each subnet

for ($i=0; $i<=$#subnet; $i++) {
	($subnet, $sm, $gw, $ds, $dn, $ts, $ht, $dhcprange, $maxdhcplease,
	 $dynamicbootp) = split('\t', $subnet[$i]);
	$ht = "ethernet" if ! $ht;
	$ds =~ s/ /,/g;
	$ts =~ s/ /,/g;
	print DHCPD "subnet $subnet netmask $sm {\n";
	print DHCPD "	max-lease-time $maxdhcplease;\n" if $maxdhcplease;
	print DHCPD "	default-lease-time $maxdhcplease;\n" if $maxdhcplease;
	print DHCPD "	dynamic-bootp-lease-length $maxdhcplease;\n"
		if $maxdhcplease;
	print DHCPD "	option routers $gw;\n" if $gw;
	print DHCPD "	option domain-name-servers $ds;\n" if $ds;
	print DHCPD "	option ntp-servers $ts;\n" if $ts;
	print DHCPD "	option domain-name \"$dn\";\n" if $dn;
	if ($dhcprange) {
		# ranges are separated with commas
		@range = split(/,/, $dhcprange);
		for $range (@range) {
			@dhcprange = &getiprange($subnet, $range);
			if (@dhcprange == ()) {
				printf STDERR "dhcp range is bad - $dhcprange\n";
			}
			else {
				$dynbootp = "dynamic-bootp"
				    if $dynamicbootp eq "yes";
				print DHCPD "	range $dynbootp $dhcprange[0] $dhcprange[1];\n";
			}
		}
	}
	print DHCPD "}\n\n";
}

# for each host entry in @main get the name, ip, and hardware address
for $main (@main) {
	($host, $ip, $hard) = split('\t', $main);

	# skip if no hardware address
	next if ! $hard;
	
	# put the hardware address in the correct format
	$hardaddr = &hardware($hard);

	if (!$hardaddr) {
		printf STDERR "$hard is invalid - skipping\n";
		next;
	}

	# produce the dhcpd.conf entry for this host
	print DHCPD "host $host {\n";
	$ht = "ethernet"; # just ethernet is supported
	print DHCPD "	hardware $ht $hardaddr;\n";
	print DHCPD "	fixed-address $ip;\n" if $ip !~ /dynamic/;
	print DHCPD "}\n\n";
}
close(DHCPD);

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

# put a hexadeciaml address into the xx:xx:xx:xx:xx:xx:xx:xx format

sub hardware {
	local($address) = shift;

	# lowercase the address
	$address = "\L$address\E";

	# if the address does not have colons then split it up by bytes
	if ($address !~ /:/) {
		# must be 12 hex characters long
		return "" if length($address) != 12;

		# add match only [0-9a-f] in string
		# must match exactly 12 times
		return "" if $address !~ /^[0-9a-f]{12}$/;

		# add in the colons
		$address =~ s/(..)(..)(..)(..)(..)(..)/\1:\2:\3:\4:\5:\6/;
	}
	# address has colons already
	# so we make sure each field is legit
	else {
		local(@b) = split(":", $address);

		# must be just 6 fields
		return "" if $#b != 6;

		# each must be 1 or 2 characters and hex
		local($b);
		for $b (@b) {
			return "" if $b !~ /^[0-9a-f]{1,2}$/;
		}
	}
	return $address;
}

# split an IP range
# range numbers are separarted with a '-'
# they are currently just a single number

sub getiprange {
	local($subnet) = shift;	# subnet number
	local($range) = shift;	# ip range

	return () if $range !~ /[0-9]-[0-9]/;

	local($start, $end) = split(/-/, $range, 2);

	return () if $start > $end || $start < 1 || $end > 255;

	local(@subnet) = split(/\./, $subnet, 4);
	pop(@subnet);

	$start = join('.', (@subnet, $start));
	$end = join('.', (@subnet, $end));

	return ($start, $end);
}
