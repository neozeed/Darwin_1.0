#!/usr/bin/perl

# ; BIND version named 4.9.3-BETA28 Thu Dec  7 17:07:25 PST 1995
# ; BIND version vixie@drop148.internetmci.ietf.org:/var/users/vixie/cur/named
# ; zone 'root-servers.net'   last serial 95092201
# ; from 198.41.0.5   at Sat Dec 16 23:31:23 1995
# $ORIGIN net.
# root-servers	IN	SOA	rs0.internic.net. markk.internic.net. (
# 		95121500 14400 7200 432000 3600000 )
# 		IN	NS	rs0.internic.net.
#		IN	NS	ns.ripe.net.
#		IN	NS	gw.home.vix.com.
#		IN	MX	10 rs0.internic.net.
# $ORIGIN root-servers.net.
# a		IN	TXT	"formerly ns.internic.net"
#		IN	MX	10 ops.internic.net.

($Origin, $Class) = @ARGV;
$Domain = '';
$Partial = '';
$Ttl = -1;
$NumSOA = 0;
@SOA = ();
$SOAclass = '';

while (<STDIN>) {
	chop;

	# Handle directives like $ORIGIN.
	if (/^\$/o) {
		($_, @_) = split(/\s+/, $');
		if ($_ eq 'ORIGIN') {
			$_ = $_[0];
			$Origin = '' if (/\.$/);
			$Origin = $_.'.'.$Origin;
		} else {
			print STDERR "unsupported directive: \$".$_."\n";
			exit(1);
		}
		next;
	}

	# Trim comments and skip blank lines.
	# XXX: Does not handle ``"foo\"bar"''.
	$x = '';
	while (/\"[^\"]+\"/) {
		$x .= $`.$_;
		$_ = $';
	}
	s/\;.*$//o;
	s/\s+$//o;
	$_ = $x.$_;
	next if !length;

	# Handle continuations (lines ending with ``('').
	if (!length($Partial) && /\($/o) {
		$Partial = $_;
		next;
	} elsif (length($Partial)) {
		$Partial .= $_;
		next unless /\)$/o;
		$_ = $Partial;
		$Partial = '';
	}

	# Parse an RR, handling default domain case (leading blanks).
	if (/^\s+/) {
		$domain = $Domain;
		s/^\s+//o;
	} else {
		($domain, $_) = split(/\s+/o, $_, 2);
	}
	$domain = &normalize_domain($domain);
	$Domain = $domain;
	# Collect TTL if there is one.
	if (/^\d/) {
		($Ttl, $_) = split(/\s+/o, $_, 2);
	}
	# Collect class if there is one.
	if (/^(IN|HS|CHAOS)\s+/io) {
		$Class = $1;
		$_ = $';
	}
	# Type and arguments are nonoptional
	($type, $_) = split(/\s+/o, $_, 2);
	$type =~ tr/a-z/A-Z/;

	# First RR must be an SOA.  Last might also be but must match.
	if ($NumSOA == 0) {
		if ($type ne 'SOA') {
			print STDERR "first RR must be an SOA, not $type\n";
			exit(1);
		}
		$NumSOA++;
		@SOA = &normalize_soa($_);
		$Ttl = $SOA[6] if ($Ttl == -1);		# minimum == default
		$SOAclass = $Class;
		if ($SOAclass eq '') {
			print STDERR "can't default CLASS on first SOA RR\n";
			exit(1);
		}
	} elsif ($type eq 'SOA') {
		if ($NumSOA > 2) {
			print STDERR "too many SOA RR's\n";
			exit(1);
		}
		$NumSOA++;
		@_ = &normalize_soa($_);
		for ($x = 0; $x < 5; $x++) {
			if ($SOA[$x] != $_[$x]) {
				print STDERR
				     "SOA[$x] mismatch ($SOA[$x] != $_[$x])\n";
				exit(1);
			}
		}
		next;
	} elsif ($NumSOA == 2) {
		print STDERR "second SOA wasn't end of zone\n";
		exit(1);
	}
	if ($Class ne $SOAclass) {
		print STDERR "all RRs in zone must have same class as SOA\n";
		exit(1);
	}

	# Normalize arguments.  Last arg is a domain name in some RR types.
	if ($type =~ /^(NS|CNAME|PTR|MX)$/) {
		@_ = split(/\s+/o, $_);
		$_[$#_] = &normalize_domain($_[$#_]);
		$_ = join(' ', @_);
	} elsif ($type eq 'SOA') {
		$_ = join(' ', &normalize_soa($_));
	}

	# Done.  Print it out.
	print "$domain $Ttl $Class $type $_\n";
}
exit(0);

sub normalize_domain {
	local($_) = @_;

	$_ .= '.'.$Origin unless $_ =~ /\.$/o;
	$_ .= '.' unless /\.$/o;
	$_ = "$`." while $_ =~ /\.\.$/o;
	y/A-Z/a-z/;
	return ($_);
}

sub normalize_soa {
	local($_) = @_;

	# BIND puts ( and ) on SOA args even when they aren't needed.
	while (/\(([^\)]+)\)/o) {
		$_ = $`.$1.$';
		s/^\s+//o;
		s/\s+$//o;
	}
	split;
	return (&normalize_domain($_[0]), &normalize_domain($_[1]), @_[2..6]);
}
