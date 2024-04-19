#! @PERL@
# @(#) gendns.pl 1.56 99/01/05 @(#)

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

# generate the DNS database files from generic database files

# files used
#	<dir>/soa
#	<dir>/main
#	<dir>/cname
#	<dir>/mx
#	<dir>/ns
#	<dir>/secondary
#	<dir>/global
#
# this script assumes that a subdomain is always delegated to
# another nameserver and hosts for the subdomain, except for the
# glue records, do not exist in the base host database files

require "getopts.pl";

# fields required in each file read with readinfo
%FIELDS = (
"main", "host ip ether hard os ptr ttl",
"cname", "alias host ttl",
"mx", "domain priority host ttl",
"ns", "domain server ttl",
"soa", "domain server contact refresh retry expire min checknames notify also_notify",
"secondary", "domain ip checknames",
"global", "directory cache forwarders checknames slave"
);

# create the list reference structures for each file
# this is used to name each field correctly as a perl variable
for $file (keys(%FIELDS)) {
	$fields_list = $FIELDS{$file};
	$fields_list =~ s/ /, \$/g;
	$fields_list =~ s/^/\$/;
	$FIELDS_LIST{$file} = $fields_list;
}

# get a zone filename
sub zonefile {
	local($domain) = shift;

	if ($domain !~ /.*\.in-addr\.arpa/i) {
		$Rev = 0;
		return $domain;
	}
	else {
		$Rev = 1;
		$Unreverse = unrev($domain);
		# change subnet "/" to a "-" if present
		$Unreverse =~ s/\//-/;
		return "f.$Unreverse";
	}
}

# check for CNAME records at populated nodes and remove them
sub check_cname {
	# build a list of all the domain names in the database
	local(%d) = ();
	local($i, $d, $e, @rest);
	for ($i=0; $i<=$#Soa; $i++) {
		($d, @rest) = split("\t", $Soa[$i]);
		$d{"\L$d\E"} = 1;
	}
	for ($i=0; $i<=$#Main; $i++) {
		($d, @rest) = split("\t", $Main[$i]);
		$d{"\L$d\E"} = 1;
	}
	for ($i=0; $i<=$#Ns; $i++) {
		($d, @rest) = split("\t", $Ns[$i]);
		$d{"\L$d\E"} = 1;
	}
	for ($i=0; $i<=$#Mx; $i++) {
		($d, @rest) = split("\t", $Mx[$i]);
		$d{"\L$d\E"} = 1;
	}
	# see if the domain of the CNAME records match
	local($host);
	local($invalidcname) = 0;
	for ($i=0; $i<=$#Cname; $i++) {
		($d, $host) = split("\t", $Cname[$i]);
		$e = "\L$d\E";
		if ($d{$e}) {
			$invalidcname++;
			printf(STDERR "ERROR - %s CNAME %s - record invalid - deleted\n", $d, $host);
			$Cname[$i] = "";
		}
	}
	# close the holes in the cname list
	if ($invalidcname > 0) {
		local($n) = $#Cname;
		for ($i=0; $i<$n; $i++) {
			if ($Cname[$i] eq "") {
				$Cname[$i] = $Cname[$i+1];
				$Cname[$i+1] = "";
			}
		}
	}
	return;
}

# check for bad ip addresses
sub check_ip {

	local(@ip) = ();
	local(%ip) = ();
	local($main) = "";
	local($invalida) = 0;
	local($err) = 0;
	for $main (@Main) {
		eval "($FIELDS_LIST{\"main\"}) = split(/\\t/, \$main);";
#		($host, $ip, $ether, $hard, $os, $ptr, $ttl) = split(/\t/, $main);

		# skip dynamic addresses
		next if $ip =~ /dynamic/;

		# build the ip address list
		$ip{$ip}++;
		@ip = split(/\./, $ip);
		if ($#ip != 3) {
			$err++;
		}
		else {
			local($ip);
			for $ip (@ip) { 
				$err++ if $ip > 255 || $ip < 0;
			}
		}
		if ($err) {
			$invalida += $err;
			printf STDERR "ERROR %s A %s - bad IP address - record deleted\n",
		       		$host, $ip if $err;
			$main = "";
		}
	}

	# warn about duplicate IP addresses
	local($key, $value);
	while ((($key, $value) = each(%ip))) {
		printf STDERR "WARNING - duplicate IP address - $key\n" if $value > 1;
	}

	# close the holes in the main list if there are invalid A records
	if ($invalida > 0) {
		local($n) = $#Main;
		local($i);
		for ($i=0; $i<$n; $i++) {
			if ($Main[$i] eq "") {
				$Main[$i] = $Main[$i+1];
				$Main[$i+1] = "";
			}
		}
	}
}

# check for duplicate hostnames
sub check_duphosts {

	local($err);
	local(@ip);
	local($main);
	local(%host) = ();
	for $main (@Main) {
		eval "($FIELDS_LIST{\"main\"}) = split(/\\t/, \$main);";
#		local($host, $ip, $ether, $hard, $os, $ptr, $ttl) = split(/\t/, $main);

		# skip dynamic addresses
		next if $ip =~ /dynamic/;
		
		# build the host list
		$host{$host}++;
	}

	local($key, $value);
	while ((($key, $value) = each(%host))) { 
		printf STDERR "WARNING - duplicate hostname - $key\n" if $value > 1;
	}
}

# check for non-numeric ttl value
sub check_ttl {
	local($domain) = shift;
	local($file) = shift;
	local($ttl) = shift;

	# must be a number if it exists at all
	if ($ttl && $ttl !~ /^\d\d*$/) {
		printf STDERR "ERROR - ttl field $ttl for $domain in file $file non-numeric - using default value\n";
		return 0;
	}
	return 1;
}

# see if an IP address is in a reverse domain

sub reverse_domain {
	local($ip) = shift;

	local($i, $ip1);
	local($domain);
	local($value);

	# exact match
	if ($Unreverse{$ip}) {
		$partsindomain = 4;
		return $domain[$Unreverse{$ip}];
	}

	@ipparts = split(/\./, $ip);
	# one down match
	pop(@ipparts);
	$ip1 = join(".", @ipparts);
	if ($Unreverse{$ip1}) {
		$partsindomain = 3;
		return $domain[$Unreverse{$ip1}];
	}

	# two down match
	pop(@ipparts);
	$ip1 = join(".", @ipparts);
	if ($Unreverse{$ip1}) {
		$partsindomain = 2;
		return $domain[$Unreverse{$ip1}];
	}

	# three down match
	pop(@ipparts);
	$ip1 = join(".", @ipparts);
	if ($Unreverse{$ip1}) {
		$partsindomain = 1;
		return $domain[$Unreverse{$ip1}];
	}

	# must not be a match
	return "";
}

# see if a domain name is a part of a domain in the SOA list
sub forward_domain {
	local($host) = shift;

	local($domain);
	local($rest);
	local(@domainparts);

	# make the name all lowercase
	$host = "\L$host\E";

	# first an exact match
	return $host if $domain{$host};

	# return null if the domain is in the secondary list
	return "" if $Secondary{$host}; 

	@domainparts = split(/\./, $host);

	# now up the tree
	for ($i=0; $i<=$#domainparts; $i++) {
		shift(@domainparts);
		$domain = join(".", @domainparts);
		return $domain if $domain{$domain};
	}

	return "";
}

# make the secondary zone bootstrap entries
sub boot_sec {
	local($filename);
	for (local($i)=0; $i<=$#Secondary; $i++) {
		local($domain, $ip, $checknames) = split("\t", $Secondary[$i]);

		# skip if domain in soa file
		next if $Secondary{"\L$domain\E"} == 0;

		if ($domain !~ /.*\.in-addr\.arpa/i) {
			$filename = $domain;
		}
		else {
			$filename = unrev($domain);
			$filename = "f.$filename";
		}
		# version 8
		printf BOOT8 "zone \"$domain\" {\n";
		printf BOOT8 "\ttype slave;\n";
		printf BOOT8 "\tmasters { $ip; };\n";
		printf BOOT8 "\tfile \"$filename\";\n";
		printf BOOT8 "\tcheck-names $checknames;\n" if $checknames;
		printf BOOT8 "};\n";

		# version 4
		printf BOOT4 "secondary %s %s %s\n", $domain, $ip, $filename;
	}
}

sub lock {
	if ( -f "$Dir/LOCK") {
		open(LCK, "$Dir/LOCK") || die "cannot open $Dir/LOCK\n";
		$pid = <LCK>;
		close(LCK);
		chop($pid);
		return 0 if kill(0, $pid);
	}
	open(LCK, ">$Dir/LOCK") || die "cannot open $Dir/LOCK\n";
	print LCK "$$\n";
	close(LCK);
	return 1;
}

sub unlock {
	unlink("$Dir/LOCK");
}

# generate unreverse IP address from in-addr.arpa domain
sub unrev {
	local($d) = shift;

	# unreverse the domain name for matching IP addresses
	local(@p) = split(/\./, $d);
	# delete the in-addr.arpa
	$#p -= 2;

	return join(".", reverse(@p));
}

# get a dynamic IP address from the DHCP lease file
%Ether = ();

sub getdynamicip {
	local($Ether)= shift;

	local(*IN);
	local($dum);
	local($ip);
	local($ether1);
	# read in the leases if we have not already done so
	if (! %Ether) {
		print STDERR "reading the $Lease file -" if $Verbose;
		print STDERR " found dynamic address for " if $Verbose;
		open(IN, "<$Leasefile") || return "";
		while (<IN>) {
			chop;
			if (/^lease/) {
				($dum, $ip, $dum) = split;
				next;
			}
			if (/\thardware/) {
				local($dum, $dum, $ether1) = split;
				chop($ether1);

				# convert hardware address to canonical format
				$ether1 = cvttohex($ether1);
				$ether{$ether1} = $ip;
				print STDERR "$ether1 " if $Verbose;
			}
		}
		close(IN);
		printf STDERR "\n" if $Verbose;
	}

	# see if there is a mapping
	return $ether{$ether};
}

# convert an Ethernet address in xx:xx:xx:xx:xx:xx format to hex
sub cvttohex {
	local($hard) = shift;

	local(@byte) = split(/:/, $hard);

	local($byte);
	local($out) = "";
	for $byte (@byte) {
		$out .= sprintf("%2.2X", hex($byte));
	}
	return $out;
}

# check the FIELDS line for the correct fields names in each database files
sub check_db_file_syntax {
	local($dir) = shift;

	local($file);
	local($fields);
	local(@fields);
	local($ret) = 1;

	for $file (keys(%FIELDS)) {

		# first check if file is readable
		if (! -r "$dir/$file") {
			$ret = 0;
			printf STDERR "ERROR - $dir/$file is not readable\n";
		}

		$fields = $FIELDS{$file};

		# just do a readinfo and check the STDERR output for errors
		open(IN, "readinfo $fields <$dir/$file 2>&1 1>/dev/null |");
		local($line) = <IN>;
		if ($line =~ /\*\*\*ERROR\*\*\*/) {
			chop($line);
			local($fieldname) = $line;
			$fieldname =~ s/.* //;
			printf STDERR "ERROR - field $fieldname not in file $file\n";
			$ret = 0;
		}
		close(IN);
	}
	return $ret;
}

# get the location of the database files
$Dir = "db";
Getopts("d:D:l:v");
$Dir = $opt_d if $opt_d;
$Just_these_domains = $opt_D;
$Leasefile = "/etc/dhcpd.leases";
$Leasefile = $opt_l if $opt_l;

# sanity checking
if (! -d "$Dir") {
	print STDERR "directory $Dir does not exist\n";
	exit 1;
}

# check for a running gendns
if (! lock) {
	print STDERR "another gendns is running\n";
	exit;
}

$Verbose=$opt_v;

# if the syntax of the FIELDS line in each file is wrong just abort
check_db_file_syntax($Dir) || exit 1;

# serial number generation
($sec, $min, $hour, $mday, $mon, $year, $wday, $yday, $isdst) = localtime;
$mon++;
# last two digits are 0-99 which divides the day into about 15 min intervals
$hour = ($hour*60+$min)/15;
# the year gets returned as yy instead of yyyy
$year += 1900;
$Dateserial = sprintf("%4.4d%2.2d%2.2d%2.2d", $year, $mon, $mday, $hour);

# preload all the database files
printf STDERR "preloading the database files..." if $Verbose;
for $file (keys(%FIELDS)) {
	$filevar = $file;
	$filevar =~ s/(.)(.*)/\U$1\E$2/;
	open(IN , "readinfo $FIELDS{$file} <$Dir/$file|");
	eval "\@$filevar = <IN>;chop(\@$filevar);";
	close(IN);
	printf STDERR "$file " if $Verbose;
}
printf STDERR "\n" if $Verbose;

# build  an associative array for the secondary domains
for $secondary (@Secondary) {
	eval "($FIELDS_LIST{\"secondary\"}) = split(/\\t/, \$secondary);";
#	($domain, $ip, $checknames) = split(/\t/, $secondary);
	$domain = "\L$domain\E";
	$Secondary{$domain} = 1;
}

# split out the global file
# check to be sure it is exactly one line long 
if ($#Global != 0) {
	printf STDERR "global is not one line long\n";
	exit 1;
}
eval "($FIELDS_LIST{\"global\"}) = split(/\\t/, \$Global[\$i]);";
#($directory, $cache, $forwarders, $checknames, $slave) = split(/\t/, $Global[0]);

# open up the bootstrap files and recreate them
unlink("boot4");
unlink("boot8");
open(BOOT8, ">named.conf");
open(BOOT4, ">named.boot");
# 
# for version 8 these are in the options statement
printf BOOT8 "options {\n";
printf BOOT8 "\tdirectory \"$directory\";\n" if $directory;
printf BOOT8 "\tcheck-names master $checknames;\n" if $checknames;
if ($forwarders) {
	# change spaces to semicolons
	$forwarders .= " ";
	$forwarders =~ s/ .*/;/g;
	printf BOOT8 "\tforwarders {$forwarders};\n" if $forwarders;
	# add the forward line to only do forwarding
	printf BOOT8 "\tforward only;\n" if $forwarders;
}
printf BOOT8 "};\n";

# version 4 bootstrap file
printf BOOT4 "directory $directory\n" if $directory;
printf BOOT4 "check-names primary $checknames $semicolon\n" if $checknames;
printf BOOT4 "forwarders $forwarders $semicolon\n" if $forwarders;

# check for invalid IP addresses
printf STDERR "checking for invalid IP addresses...\n" if $Verbose;
check_ip;

# check for duplicate hostnames
printf STDERR "checking for duplicate hostnames...\n" if $Verbose;
check_duphosts;

# check for CNAME records at populated nodes and remove them
printf STDERR "checking for invalid CNAME records...\n" if $Verbose;
check_cname;

$n_zones = $#Soa+1;
# if we have a list of domains to update, just use them
if ($Just_these_domains) {
	@Just_these_domains = split(" ", $Just_these_domains);
	$n_zones = $#Just_these_domains+1;
}

printf STDERR "generating files for %d zones\n", $n_zones if $Verbose;

printf STDERR "reading SOA information\n" if $Verbose;
# read SOA info for each domain
for ($i=0; $i<=$#Soa; $i++) {
	eval "($FIELDS_LIST{\"soa\"}) = split(/\\t/, \$Soa[\$i]);";
#	($domain, $server, $contact, $Refresh, $retry, $expire, $min,
#	 $checknames, $notify, $also_notify) = split("\t", $Soa[$i]);

	# lowercase the domain
	$domain = "\L$domain\E";


	# check if it is in the secondary list
	if ($Secondary{$domain}) {
		printf STDERR "WARNING domain $domain is also in the soa file - deleted from secondary domain list\n";
		$Secondary{$domain} = 0;
	}

	# get the filename for this zone
	$filename{$domain} = zonefile($domain);
	$Unreverse[$i] = "";
	if ($Rev == 1) {
		# if there is a "-" in the name then it is a Class C on non-byte subnet boundaries
		# set the unreverse to the Class C network address
		if ($Unreverse =~ /-/) {
			$Unreverse =~ s/([0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*)(\..*)/$1/;
		}
		$Unreverse[$i] = $Unreverse;
		$Unreverse{$Unreverse} = $i;
	}

	# write the bootstrap file entry for the domain
	printf BOOT8 "zone \"$domain\" {\n";
	printf BOOT8 "\ttype master;\n";
	printf BOOT8 "\tfile \"$filename{$domain}\";\n";
	printf BOOT8 "\tcheck-names $checknames;\n" if $checknames;
	printf BOOT8 "\tnotify $notify;\n" if $notify;
	# add IP addresses of other secondary servers
	if ($notify eq "yes" && $also_notify) {
		# IP addresses are separated with commas
		local(@ipaddr) = split(/,/, $also_notify);
		printf BOOT8 "\talso-notify {";
		local($ipaddr);
		for $ipaddr (@ipaddr) {
			printf BOOT8 "$ipaddr;";
		}
		printf BOOT8 "};\n";
	}
	printf BOOT8 "};\n";

	# version 4 bootstrap
	printf BOOT4 "primary %s %s\n", $domain, $filename{$domain};

	$domain[$i] = $domain;
	$domain{$domain} = $i;

	# save the header and SOA record
	$domain{$domain} = "\$ORIGIN $domain.\n";
	$domain{$domain} .= "@ SOA $server. $contact. ( $Dateserial $refresh $retry $expire $min )\n";
}

# get servers for this domain
# also gets servers for any delegated subdomains

printf STDERR "reading NS information\n" if $Verbose;
for $ns (@Ns) {
	eval "($FIELDS_LIST{\"ns\"}) = split(/\\t/, \$ns);";
#	($domain, $server, $ttl) = split("\t", $ns);
	if (($pdomain = forward_domain($domain))) {
		# check ttl value
		$ttl = "" if !check_ttl($domain, "ns", $ttl);
		$domain{$pdomain} .= "$domain. $ttl IN NS $server.\n";
	}
}
# scan the input file and extract info depending on whether this
# is a forward or reverse domain file

printf STDERR "scanning input file main\n" if $Verbose;
for $main (@Main) {
	eval "($FIELDS_LIST{\"main\"}) = split(/\t/, \$main);";
#	($host, $ip, $ether, $hard, $os, $ptr, $ttl) = split(/\t/, $main);

	# forward domain
	if ((! $ptr || $ptr ne "only") && ($domain = forward_domain($host))) {

		# dynamic address
		# get IP address from DHCP lease file, if available
		next if $ip =~ /dynamic/ && ! ($ip = getdynamicip($ether));

		# double quote $hard and $os if blanks in value
		$hard = "\"$hard\"" if ($hard =~ / /);
		$os = "\"$os\"" if ($os =~ / /);

		# check ttl value
		$ttl = "" if !check_ttl($host, "main", $ttl);

 		$domain{$domain} .= "$host. $ttl IN A $ip\n";
		$domain{$domain} .= "$host. $ttl IN HINFO $hard $os\n"
		    if $hard && $os;
	}

	# reverse domain
	if ($ptr ne "no" && ($domain = reverse_domain($ip))) {
		$out = "";
		@ipparts = split(/\./, $ip);
		for ($n=3; $n>=$partsindomain; $n--) {
			$out .= "$ipparts[$n].";
		}

		# check ttl value
		$ttl = "" if !check_ttl($domain, "main", $ttl);

		$out .= "$domain. $ttl IN PTR $host.\n";
		$domain{$domain} .= $out;
	}
}

printf STDERR "scanning input file cname\n" if $Verbose;
for $cname (@Cname) {
	eval "($FIELDS_LIST{\"cname\"}) = split(/\\t/, \$cname);";
#	($alias, $host, $ttl) = split("\t", $cname);
	# skip if we do not serve the domain
	if (($domain = forward_domain($alias))) {
		# check ttl value
		$ttl = "" if !check_ttl($alias, "alias", $ttl);
		$domain{$domain} .= "$alias. $ttl IN CNAME $host.\n";
	}
}

printf STDERR "scanning input file mx\n" if $Verbose;
for $mx (@Mx) {
	eval "($FIELDS_LIST{\"mx\"}) = split(/\\t/, \$mx);";
#	($domain, $priority, $host, $ttl) = split("\t", $mx);
	# skip if we do not serve the domain
	if (($pdomain = forward_domain($domain))) {
		# check ttl value
		$ttl = "" if !check_ttl($domain, "mx", $ttl);
		$domain{$pdomain} .= "$domain. $ttl IN MX $priority $host.\n";
	}
}

printf STDERR "generating output files...\n" if $Verbose;
for ($i=0; $i<=$#Soa; $i++) {
	$domain = $domain[$i];

	# make the output look nice
	# treat string as multiline
	$* = 1;
	$domain{$domain} =~ s/(.*)(\.$domain\.)/$1/gi;
	$domain{$domain} =~ s/(.*)(\.$domain\. )/$1 /gi;
     	$domain{$domain} =~ s/^$domain\./@/g;
	$* = 0;

	# unlink the output file in case of permission problems
	$filename = $filename{$domain};
	unlink("$filename{$domain}");
	open(OUT, ">$filename");
	print OUT $domain{$domain};
	close(OUT);
	printf STDERR "$domain " if $Verbose;
}
printf STDERR "\n" if $Verbose;
close(IN);

# slave server
# XXX is there a version 8 command for this
printf BOOT4 "slave\n" if $slave eq "yes";

# create secondary zone directives
boot_sec;

# and the cache directive
printf BOOT8 "zone \".\" {\n";
printf BOOT8 "\ttype hint;\n";
printf BOOT8 "\tfile \"$cache\";\n";
printf BOOT8 "};\n";

printf BOOT4 "cache . %s\n", $cache;

close(BOOT4);
close(BOOT8);

#finally unlock the database files
unlock;
