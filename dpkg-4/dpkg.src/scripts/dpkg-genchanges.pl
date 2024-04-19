#! /usr/bin/perl

my $dpkglibdir, $version;

BEGIN { 
    $dpkglibdir = ".";
    $version = "unknown"; # This line modified by Makefile
    push (@INC, $dpkglibdir); 
}

require 'controllib.pl';

use POSIX;
use POSIX qw(:errno_h :signal_h);

$controlfile= 'debian/control';
$changelogfile= 'debian/changelog';
$fileslistfile= 'debian/files';
$varlistfile= 'debian/substvars';
$uploadfilesdir= '..';
$sourcestyle= 'i';
$quiet= 0;

sub usageversion {
    print STDERR
"Debian GNU/Linux dpkg-genchanges $version.  Copyright (C) 1996
Ian Jackson.  This is free software; see the GNU General Public Licence
version 2 or later for copying conditions.  There is NO warranty.

Usage: dpkg-genchanges [options ...]

Options:  -b                     binary-only build - no source files
          -B                     arch-specific - no source or arch-indep files
          -c<controlfile>        get control info from this file
          -l<changelogfile>      get per-version info from this file
          -f<fileslistfile>      get .deb files list from this file
          -v<sinceversion>       include all changes later than version
          -C<changesdescription> use change description from this file
          -m<maintainer>         override changelog's maintainer value
          -u<uploadfilesdir>     directory with files (default is \`..')
          -si (default)          src includes orig for debian-revision 0 or 1
          -sa                    source includes orig src
          -sd                    source is diff and .dsc only
          -q                     quiet - no informational messages on stderr
          -F<changelogformat>    force change log format
          -V<name>=<value>       set a substitution variable
          -T<varlistfile>        read variables here, not debian/substvars
          -D<field>=<value>      override or add a field and value
          -U<field>              remove a field
          -h                     print this message
";
}

$i=100;grep($fieldimps{$_}=$i--,
          qw(Format Date Source Binary Architecture Version
             Distribution Urgency Maintainer Description Changes Files));

while (@ARGV) {
    $_=shift(@ARGV);
    if (m/^-b$/) {
        $binaryonly= 1;
    } elsif (m/^-B$/) {
	$archspecific=1;
	$binaryonly= 1;
	print STDERR "$progname: arch-specific upload - not including arch-independent packages\n";
    } elsif (m/^-s([iad])$/) {
        $sourcestyle= $1;
    } elsif (m/^-q$/) {
        $quiet= 1;
    } elsif (m/^-c/) {
        $controlfile= $';
    } elsif (m/^-l/) {
        $changelogfile= $';
    } elsif (m/^-C/) {
        $changesdescription= $';
    } elsif (m/^-f/) {
        $fileslistfile= $';
    } elsif (m/^-v/) {
        $since= $';
    } elsif (m/^-T/) {
        $varlistfile= $';
    } elsif (m/^-m/) {
        $forcemaint= $';
    } elsif (m/^-F([0-9a-z]+)$/) {
        $changelogformat=$1;
    } elsif (m/^-D([^\=:]+)[=:]/) {
        $override{$1}= $';
    } elsif (m/^-u/) {
        $uploadfilesdir= $';
    } elsif (m/^-U([^\=:]+)$/) {
        $remove{$1}= 1;
    } elsif (m/^-V(\w[-:0-9A-Za-z]*)[=:]/) {
        $substvar{$1}= $';
    } elsif (m/^-h$/) {
        &usageversion; exit(0);
    } else {
        &usageerr("unknown option \`$_'");
    }
}

&findarch;
&parsechangelog;
&parsecontrolfile;

$fileslistfile="./$fileslistfile" if $fileslistfile =~ m/^\s/;
open(FL,"< $fileslistfile") || &syserr("cannot read files list file");
while(<FL>) {
    if (m/^(([-+.0-9a-z]+)_([^_]+)_(\w+)\.deb) (\S+) (\S+)$/) {
        defined($p2f{"$2 $4"}) &&
            &warn("duplicate files list entry for package $2 (line $.)");
	$f2p{$1}= $2;
        $p2f{"$2 $4"}= $1;
        $p2f{$2}= $1;
        $p2ver{$2}= $3;
        defined($f2sec{$1}) &&
            &warn("duplicate files list entry for file $1 (line $.)");
        $f2sec{$1}= $5;
        $f2pri{$1}= $6;
        push(@fileslistfiles,$1);
    } elsif (m/^([-+.,_0-9a-zA-Z]+) (\S+) (\S+)$/) {
	defined($f2sec{$1}) &&
            &warn("duplicate files list entry for file $1 (line $.)");
        $f2sec{$1}= $2;
        $f2pri{$1}= $3;
        push(@fileslistfiles,$1);
    } else {
        &error("badly formed line in files list file, line $.");
    }
}
close(FL);

for $_ (keys %fi) {
    $v= $fi{$_};
    if (s/^C //) {
#print STDERR "G key >$_< value >$v<\n";
	if (m/^Source$/) { &setsourcepackage; }
	elsif (m/^Section$|^Priority$/) { $sourcedefault{$_}= $v; }
	elsif (s/^X[BS]*C[BS]*-//i) { $f{$_}= $v; }
	elsif (m/|^X[BS]+-|^Standards-Version$|^Maintainer$/i) { }
	else { &unknown('general section of control info file'); }
    } elsif (s/^C(\d+) //) {
#print STDERR "P key >$_< value >$v<\n";
	$i=$1; $p=$fi{"C$i Package"}; $a=$fi{"C$i Architecture"};
	if (!defined($p2f{$p})) {
	    if (!$archspecific || $a eq $substvar{'Arch'}) {
		&error("package $p in control file but not in files list");
	    }
	} else {
	    $p2arch{$p}=$a;
	    $f=$p2f{$p};
	    if (m/^Description$/) {
		$v=$` if $v =~ m/\n/;
		push(@descriptions,sprintf("%-10s - %-.65s",$p,$v));
	    } elsif (m/^Section$/) {
		$f2seccf{$f}= $v;
	    } elsif (m/^Priority$/) {
		$f2pricf{$f}= $v;
	    } elsif (s/^X[BS]*C[BS]*-//i) {
		$f{$_}= $v;
	    } elsif (m/^Architecture$/) {
		$v= $arch if $v eq 'any';
		push(@archvalues,$v) unless $archadded{$v}++;
	    } elsif (m/^(Package|Essential|Pre-Depends|Depends|Provides)$/ ||
		     m/^(Recommends|Suggests|Optional|Conflicts|Replaces)$/ ||
		     m/^X[CS]+-/i) {
	    } else {
		&unknown("package's section of control info file");
	    }
	}
    } elsif (s/^L //) {
#print STDERR "L key >$_< value >$v<\n";
        if (m/^Source$/) {
            &setsourcepackage;
        } elsif (m/^(Version|Maintainer|Changes|Urgency|Distribution|Date)$/) {
            $f{$_}= $v;
        } elsif (s/^X[BS]*C[BS]*-//i) {
            $f{$_}= $v;
        } elsif (!m/^X[BS]+-/i) {
            &unknown("parsed version of changelog");
        }
    } else {
        &internerr("value from nowhere, with key >$_< and value >$v<");
    }
}

if ($changesdescription) {
    $changesdescription="./$changesdescription" if $changesdescription =~ m/^\s/;
    $f{'Changes'}= '';
    open(X,"< $changesdescription") || &syserr("read changesdescription");
    while(<X>) {
        s/\s*\n$//;
        $_= '.' unless m/\S/;
        $f{'Changes'}.= "\n $_";
    }
}

for $p (keys %p2f) {
    my ($pp, $aa) = (split / /, $p);
    defined($p2i{"C $pp"}) ||
        &warn("package $pp listed in files list but not in control info");
}

for $p (keys %p2f) {
    $f= $p2f{$p};
    $sec= $f2seccf{$f}; $sec= $sourcedefault{'Section'} if !length($sec);
    if (!length($sec)) { $sec='-'; &warn("missing Section for binary package $p; using '-'"); }
    $sec eq $f2sec{$f} || &error("package $p has section $sec in control file".
                                 " but $f2sec{$f} in files list");
    $pri= $f2pricf{$f}; $pri= $sourcedefault{'Priority'} if !length($pri);
    if (!length($pri)) { $pri='-'; &warn("missing Priority for binary package $p; using '-'"); }
    $pri eq $f2pri{$f} || &error("package $p has priority $pri in control".
                                 " file but $f2pri{$f} in files list");
}

if (!$binaryonly) {
    $version= $f{'Version'};
    $origversion= $version; $origversion =~ s/-[^-]+$//;
    $sec= $sourcedefault{'Section'};
    if (!length($sec)) { $sec='-'; &warn("missing Section for source files"); }
    $pri= $sourcedefault{'Priority'};
    if (!length($pri)) { $pri='-'; &warn("missing Priority for source files"); }

    ($sversion = $version) =~ s/^\d+://;
    $dsc= "$uploadfilesdir/${sourcepackage}_${sversion}.dsc";
    open(CDATA,"< $dsc") || &error("cannot open .dsc file $dsc: $!");
    push(@sourcefiles,"${sourcepackage}_${sversion}.dsc");
    
    &parsecdata('S',-1,"source control file $dsc");
    $files= $fi{'S Files'};
    for $file (split(/\n /,$files)) {
        next if $file eq '';
        $file =~ m/^([0-9a-f]{32})[ \t]+\d+[ \t]+([0-9a-zA-Z][-+:.,=0-9a-zA-Z_]+)$/
            || &error("Files field contains bad line \`$file'");
        ($md5sum{$2},$file) = ($1,$2);
        push(@sourcefiles,$file);
    }
    for $f (@sourcefiles) { $f2sec{$f}= $sec; $f2pri{$f}= $pri; }
    
    if (($sourcestyle =~ m/i/ && $version !~ m/-[01]$/ ||
         $sourcestyle =~ m/d/) &&
        grep(m/\.diff\.gz$/,@sourcefiles)) {
        $origsrcmsg= "not including original source code in upload";
        @sourcefiles= grep(!m/\.orig\.tar\.gz$/,@sourcefiles);
    } else {
        $origsrcmsg= "including full source code in upload";
    }
} else {
    $origsrcmsg= "binary-only upload - not including any source code";
}

print(STDERR "$progname: $origsrcmsg\n") ||
    &syserr("write original source message") unless $quiet;

$f{'Format'}= $substvar{'Format'};

if (!length($f{'Date'})) {
    chop($date822=`822-date`); $? && subprocerr("822-date");
    $f{'Date'}= $date822;
}

$f{'Binary'}= join(' ',grep(s/C //,keys %p2i));

unshift(@archvalues,'source') unless $binaryonly;
$f{'Architecture'}= join(' ',@archvalues);

$f{'Description'}= "\n ".join("\n ",sort @descriptions);

$f{'Files'}= '';
for $f (@sourcefiles,@fileslistfiles) {
    next if ($archspecific && ($p2arch{$f2p{$f}} eq 'all'));
    next if $filedone{$f}++;
    $uf= "$uploadfilesdir/$f";
    open(STDIN,"< $uf") || &syserr("cannot open upload file $uf for reading");
    (@s=stat(STDIN)) || &syserr("cannot fstat upload file $uf");
    $size= $s[7]; $size || &warn("upload file $uf is empty");
    $md5sum=`md5sum`; $? && subprocerr("md5sum upload file $uf");
    $md5sum =~ m/^([0-9a-f]{32})\s*$/i ||
        &failure("md5sum upload file $uf gave strange output \`$md5sum'");
    $md5sum= $1;
    defined($md5sum{$f}) && $md5sum{$f} ne $md5sum &&
        &error("md5sum of source file $uf ($md5sum) is different from md5sum in $dsc".
               " ($md5sum{$f})");
    $f{'Files'}.= "\n $md5sum $size $f2sec{$f} $f2pri{$f} $f";
}    

$f{'Source'}= $sourcepackage;

$f{'Maintainer'}= $forcemaint if length($forcemaint);

for $f (qw(Version Distribution Maintainer Changes)) {
    defined($f{$f}) || &error("missing information for critical output field $f");
}

for $f (qw(Urgency)) {
    defined($f{$f}) || &warn("missing information for output field $f");
}

for $f (keys %override) { $f{&capit($f)}= $override{$f}; }
for $f (keys %remove) { delete $f{&capit($f)}; }

&outputclose(0);
