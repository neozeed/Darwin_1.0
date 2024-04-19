#! @PERL@
#
# @(#) cvtstatic.pl 1.3 98/12/21 @(#)

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

# convert static host table to updathost file format
#

require "getopts.pl";

&Getopts("ad:D:I:");

$append = $opt_a;
$app = ">";
$app = ">>" if $append;
$dbdir = $opt_d;
$domainsuf = $opt_D;
$ippre = $opt_I;

# open up the database files
open(MAIN, "$app$dbdir/main") || die "cannot open $dbdir/main";
open(CNAME, "$app$dbdir/cname") || die "cannot open $dbdir/cname";

if ($domainsuf || $ippre) {
	print MAIN "#FIELDS GLOBAL null=X host";
	print MAIN " suffix=$domainsuf" if $domainsuf;
	print MAIN " no=. ip";
	print MAIN " prefix=$ippre" if $ippre;
	print MAIN " no=. ether hard os contact norev\n";
	print CNAME "#FIELDS GLOBAL null=X host no=.";
	print CNAME " suffix=$domainsuf" if $domainsuf;
	print CNAME " alias no=.";
	print CNAME " suffix=$domainsuf" if $domainsuf;
	print CNAME "\n";
}
$ippat = $ippre;
$domainpat = $domainsuf;
$ippat =~ s/(\W)/\\$1/g;
$domainpat =~ s/(\W)/\\$1/g;
while (<>) {
	chop;
	next if /^#/;
	($ip, $canonical, @cname) = split;
	
	if ($ippat && $ip =~ /^$ippat/) {
		$ip =~ s/^$ippat//;
	}
	else {
		$ip = ".$ip";
	}
	if ($canonical =~ /\./) {
		if ($domainpat && $canonical =~ /$domainpat$/) {
			$canonical =~ s/$domainpat$//;
		}
		else {
			$canonical = "$canonical.";
		}
	}
	print MAIN "$canonical\t$ip\n";
	foreach $cname (@cname) {
		if ($canonical =~ /\./) {
			if ($domainpat && $cname =~ /$domainpat$/) {
				$cname =~ s/$domainpat$//;
			}
			else {
				$cname = "$cname.";
			}
		}
		print CNAME "$canonical\t$cname\n";
	}
}
