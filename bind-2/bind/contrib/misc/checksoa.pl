#!/usr/bin/perl -w

# checksoa.pl - check SOA serial numbers given a local(?) named.boot file
# vix 19sep95 [original]
#
# $Id: checksoa.pl,v 1.1.1.1 1999/10/04 22:25:54 wsanchez Exp $

$Dig = 'dig';
$, = ' ';
$| = 1;

sub usage {
	print STDERR <<":EOF:";
usage: $0 [-d] -i priaddr {-a aliasaddr}... {-n servname}... bootfile...
(i.e.: $0 -i 127.0.0.1 -a 192.5.5.1 -a 192.5.5.2 -n gw.home.vix.com named.boot
:EOF:
	exit(1);
}

$something = 0;
$Debug = 0;
while ($_ = shift) {
	if (/\-/) {
		$_ = $';
		if ($_ eq 'i') {
			($_ = shift) =~ tr/A-Z/a-z/;
			delete $MyServerA{$MyServer};
			$MyServer = $_;
		} elsif ($_ eq 'a') {
			($_ = shift) =~ tr/A-Z/a-z/;
			$MyServerA{$_} = '';
		} elsif ($_ eq 'n') {
			($_ = shift) =~ tr/A-Z/a-z/;
			$MyServerN{$_} = '';
		} elsif ($_ eq 'd') {
			$Debug++;
		} else {
			&usage();
		}
	} else {
		&usage() unless $MyServer;
		print "<myserv $MyServer,",
			"A(",keys(%MyServerA),"),",
			"N(",keys(%MyServerN),")\n"	if $Debug;
		&bootfile($_);
		$something++;
	}
}
&usage() unless $something;
exit(0);

sub bootfile {
	local($bootfile) = @_;
	local(*f, $_, $zone, $server, @servers);

	open(f, "<$bootfile") || die "$bootfile: $!";
	while (<f>) {
		chop; split;
		$_ = shift(@_);
		$Output = '';
		if ($_ eq 'primary') {
			$zone = shift(@_);
			@servers = &getnsaddrs($zone, $MyServer);
		} elsif ($_ eq 'secondary') {
			$zone = shift(@_);
			@servers = ();
			while (defined($_ = shift(@_)) && /^(\d+\.){3}\d+$/) {
				push(@servers, $_);
			}
		} else {
			next;
		}
		next unless length($zone) && $#servers >= $[;
		$okservers = 0;
		$serial = &getserial($zone, $MyServer);
		print "<servers @servers>\n" if $Debug;
		foreach $server (@servers) {
			$_ = &getserial($zone, $server);
			if ($_ != $serial) {
				$Output .= sprintf("\t%-16s %s\n", $server,
						   "has wrong serial ($_)");
			} else {
				$okservers++;
			}
		}
		if ($Output || $Debug) {
			print "[$zone]: serial $serial," .
				" $okservers good server(s)\n" .
				$Output;
		}
	}
	close(f);
	return;
}

sub getserial {
	local($zone, $server) = @_;
	local(*dig, $serial, $_, $isauth);

	$isauth = 0;
	$serial = 0;
	print "<$Dig @$server $zone soa>\n" if $Debug;
	open(dig, "$Dig @$server $zone soa 2>&1 |") || die "dig: $!";
	while (<dig>) {
		chop;
		$Output .= sprintf("\t%-16s %s\n", $server, $_) if /res_send/;
		$isauth = 't' if /\bflags\:.*\Waa\b/io;
		next unless $isauth;
		$serial = $1 if !$serial && /^\s+(\d+)\s*\;\s*serial\s*/io;
	}
	close(dig);
	return ($serial);
}

sub getnsaddrs {
	local($zone, $server) = @_;
	local(*dig, @ns, %a, @nsaddrs, $_, $isauth);

# vix.com.        3600    NS      ns.uu.net.
# ns.uu.net.      52647   A       137.39.1.3

	$isauth = 0;
	@ns = ();
	%a = ();
	print "<$Dig @$server $zone soa>\n" if $Debug;
	open(dig, "$Dig @$server $zone ns 2>&1 |") || die "dig: $!";
	while (<dig>) {
		chop; tr/A-Z/a-z/;
		$Output .= sprintf("\t%-16s %s\n", $server, $_) if /res_send/;
		$isauth = 't' if /\bflags\:.*\Waa\b/io;
		next unless $isauth;
		if (/^[\w\_\.\-]+\s+\d+\s+NS\s+([\w\_\-\.]+)$/io) {
			next if defined $MyServerN{$1};
			push(@ns, $1);
		}
		if (/^([\w\_\.\-]+)\s+\d+\s+A\s+((\d+\.){3}\d+)$/io) {
			next if $2 eq $MyServer;
			next if defined $MyServerA{$2};
			$a{$1} = $2;
		}
	}
	close(dig);
	@nsaddrs = ();
	foreach (@ns) {
		push(@nsaddrs, $a{$_}) if defined $a{$_};
	}
	print "<getnsaddrs($zone, $server) -> (@nsaddrs)>\n" if $Debug;
	return (@nsaddrs);
}
