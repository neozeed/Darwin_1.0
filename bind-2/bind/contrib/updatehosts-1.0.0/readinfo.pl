#! @PERL@
# @(#) readinfo.pl 1.10 98/12/21 @(#)

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

# reads a filed oriented text file
#
# input from stdin
# output to stdout
#
# Arguments are names of fields in the file.
#
# fields in a file are defined by a comment line of the form
# #FIELDS <field_description> ...
#
# where <field_description> is the name of the field followed by optional
# keyword parameters of the form parameter=<value>
# 4 paramaters are supported in this script
#	prefix - prefix added to value of the field
#	suffix - suffix added to value of field
#	no - character to prevent adding a prefix or suffix to a field
#	null - null field value (default is X)
#
# It is also possible to define global values for the parameters with the
# GLOBAL field value followed by the usual keywords
#
# Fields in the file are separated with white space.  Embedded blanks in fields
# are either escaped with a '\' or the entire field is quoted with double quotes
# The prefix or suffix can be overridden by prepending or following the value
# with the ``no='' character.

# Fields may be continued over more than 1 line by ending the line with a \

# line with double quotes or backslashes
sub double_back {
	@chars = split(//);
	$chars[$#chars+1] = " ";
	$l = $#chars;
	$quoted = 0;
	$escaped = 0;
	$space = 0;
	$out = "";
	$nfields = 0;
	for ($i=0; $i<=$l; $i++) {
		if ($chars[$i] eq "\"") {
			if ($quoted) {
				$quoted = 0;
			}
			else {
				$quoted = 1;
			}
			$space = 0;
		}
		elsif ($chars[$i] eq "\\") {
			$escaped = 1;
			$space = 0;
		}
		elsif ($quoted) {
			$out .= $chars[$i];
			$space = 0;
		}
		elsif ($escaped) {
			$out .= $chars[$i];
			$escaped = 0;
			$space = 0;
		}
		elsif ($chars[$i] eq " " || $chars[$i] eq "	") {
			if ($space == 0) {
				$out[$nfields++] = $out;
				$out = "";
			}
			$space = 1;
		}
		else {
			$out .= $chars[$i];
			$space = 0;
		}
	}
}

sub field_def {
	local($nfields) = 0;
	local($global) = -1;
	local($i);
	for ($i=1; $i<=$#out; $i++) {
		# field is a parameter
		if ($out[$i] =~ /\=/) {
			($keyword, $value) = split("=", $out[$i]);
			# prefix
			if ($keyword eq "prefix") {
				$prefix[$nfields-1] = $value;
			}
			# suffix
			elsif ($keyword eq "suffix") {
				$suffix[$nfields-1] = $value;
			}
			elsif ($keyword eq "no") {
				$no[$nfields-1] = $value;
			}
			elsif ($keyword eq "null") {
				$null[$nfields-1] = $value;
			}
		}
		#field name
		else {
			$fields[$nfields] = $out[$i];
			$prefix[$nfields] = "";
			$suffix[$nfields] = "";
			$no[$nfields] = "";
			$null[$nfields] = "";
			
			# check for GLOBAL special field
			$global = $nfields if $fields[$nfields] eq "GLOBAL";
			$nfields++;
		}
	}

	# copy in GLOBAL parameters if set
	if ($global != -1) {
		for ($i=0; $i<$nfields; $i++) {
			$prefix[$i] = $prefix[$global] if !$prefix[$i];
			$suffix[$i] = $suffix[$global] if !$suffix[$i];
			$no[$i] = $no[$global] if !$no[$i];
			$null[$i] = $null[$global] if !$null[$i];
		}

		# now shift out the GLOBAL pseudo field
		for ($i=$global+1; $i<$nfields; $i++) {
			$fields[$i-1] = $fields[$i];
			$prefix[$i-1] = $prefix[$i];
			$suffix[$i-1] = $suffix[$i];
			$no[$i-1] = $no[$i];
			$null[$i-1] = $null[$i];
		}
		$nfields--;
	}

	# quote all special characters in a "no" value
	# this allows any characters in the string
	
	for ($i=0; $i<$nfields; $i++) {
		$no[$i] =~ s/(\W)/\\$1/g;
	}

	# check to see all the extract list of fields are in the fields list
	$error = 0;
	for ($j=0; $j<=$#extract; $j++) {
		$match = 0;
		for ($i=0; $i<$nfields; $i++) {
			if ($fields[$i] eq $extract[$j]) {
				$match = 1;
				last;
			}
		}
		if (! $match) {
			printf STDERR "ERROR - field %s not found\n", $extract[$j];
			$error = 1;
		}
	}
	exit if $error;
}

sub process_line {
	for ($i=0; $i<=$#extract; $i++) {
		for ($j=0; $j<=$#fields; $j++) {
			last if $fields[$j] eq $extract[$i];
		}

		if ($i > 0) {
			printf OUT "\t";
		}
		# check for null value
		$out[$j] = "" if $out[$j] eq $null[$j];
		# no overide character for this field
		if ($no[$j] eq "") {
			printf OUT "%s%s%s", $prefix[$j], $out[$j], $suffix[$j];
			next;
		}
		# overide character exist - see if it is in the field
		if ($out[$j] =~ /^$no[$j]/) {
			$out[$j] = substr($out[$j], 1);
			printf OUT "%s", $out[$j];
		}
		elsif ($out[$j] =~ /$no[$j]$/) {
			chop($out[$j]);
			printf OUT "%s", $out[$j];
		}
		else {
			printf OUT "%s%s%s", $prefix[$j], $out[$j], $suffix[$j];
		}
	}
	printf OUT "\n";
}

# get the fields from the command line

# command line arguments
require "getopts.pl";
&Getopts("i:o:");
$input = $opt_i;
$output = $opt_o;

@extract=@ARGV;

# check for i/o from/to files
if ($input) {
	open(IN, $input) || die "cannot read $input\n";
}
else {
	open(IN, "<&STDIN") || die "cannot open STDIN\n";
}

if ($output) {
	open(OUT, ">$output") || die "cannot open $output\n";
}
else {
	open(OUT, ">&STDOUT") || die "cannot open STDOUT\n";
}

# change whitespace to tabs and handle quoted fields and escaped blanks
# also remove comment lines, except the #FIELDS line
$saved_line = "";
while (<IN>) {
	chop;

	if ($saved_line) {
		$_ = "$saved_line$_" if $saved_line;
		$saved_line = "";
	}

	# line continues over next line?
	if (/\\$/) {
		$saved_line = $_;
		next;
	}

	# just do special stuff on non-comments
	next if /^#/ && (! /#FIELDS/ && ! /#INCLUDE/);

	# also skip blank lines
	next if /^[ \t]*$/;

	@out = ();

	# line is just a bunch of normal fields
	if (! /"/ && ! /\\/) {
		@out = split;
	}
	# line has double quotes or backslashes
	else {
		&double_back;
	}

	# extract field definition info from file
	if ($out[0] eq "#FIELDS") {
		&field_def;
		next;
	}

	# #INCLUDE reads another file
	if ($out[0] eq "#INCLUDE") {
		$readinfoargs = join(" ", @extract);
		$filename = $out[1];
		# just execute readinfo with the same argument list
		if ($filename !~ /^|/) {
			system("readinfo $readinfoargs <$filename");
		}
		# if the filename starts with an | then just execute the command
		# with any arguments following and pipe into readinfo
		else {
			$filename =~ s/.//; 
			$args = join(" ", @out[2..$#out]);
			system("$filename $args | readinfo $readinfoargs");
		}
		next;
	}
	
	# process a record
	# scan the list of fields to extract and compare them with the
	# list in the file and print as they are encountered
	&process_line;
}
