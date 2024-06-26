#! /usr/bin/perl
#
# Class name: Var
# Synopsis: Holds class and instance data members parsed by headerDoc
#
# Author: Matt Morse (matt@apple.com)
# Last Updated: 12/9/99
# 
# Copyright (c) 1999 Apple Computer, Inc.  All Rights Reserved.
# The contents of this file constitute Original Code as defined in and are
# subject to the Apple Public Source License Version 1.1 (the "License").
# You may not use this file except in compliance with the License.  Please
# obtain a copy of the License at http://www.apple.com/publicsource and
# read it before using this file.
#
# This Original Code and all software distributed under the License are
# distributed on an TAS ISU basis, WITHOUT WARRANTY OF ANY KIND, EITHER
# EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
# INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS
# FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the License for
# the specific language governing rights and limitations under the
# License.
#
######################################################################
package Var;

use Utilities;
use HeaderElement;

# making it a subclass of Struct, so that it has the "fields" ivar.
@ISA = qw( Struct );
use strict;
use vars qw($VERSION @ISA);
$VERSION = '1.20';


sub new {
    my($param) = shift;
    my($class) = ref($param) || $param;
    my $self = {};
    
    bless($self, $class);
    $self->SUPER::_initialize();
    $self->_initialize();
    return($self);
}

sub _initialize {
    my($self) = shift;
}


sub processVarComment {
    my($self) = shift;
    my $fieldArrayRef = shift;
    my @fields = @$fieldArrayRef;
	foreach my $field (@fields) {
		SWITCH: {
            ($field =~ /^\/\*\!/)&& do {last SWITCH;}; # ignore opening /*!
            ($field =~ s/^var\s+//) && 
            do {
                my ($name, $disc);
                ($name, $disc) = &Utilities::getAPINameAndDisc($field); 
                $self->name($name);
                if (length($disc)) {$self->discussion($disc);};
                last SWITCH;
            };
            ($field =~ s/^abstract\s+//) && do {$self->abstract($field); last SWITCH;};
            ($field =~ s/^discussion\s+//) && do {$self->discussion($field); last SWITCH;};
            print "Unknown field: $field\n";
		}
	}
}


sub setVarDeclaration {
    my($self) = shift;
    my ($dec) = @_;
    my $localDebug = 0;
    
    print "============================================================================\n" if ($localDebug);
    print "Raw var declaration is: $dec\n" if ($localDebug);
    
    $dec =~ s/^extern\s+//;
    $dec =~ s/[ \t]+/ /g;
    if (length ($dec)) {$dec = "<pre>\n$dec</pre>\n";};
    print "Var: returning declaration:\n\t|$dec|\n" if ($localDebug);
    print "============================================================================\n" if ($localDebug);
    $self->declarationInHTML($dec);
    return $dec;
}

sub printObject {
    my $self = shift;
 
    print "Var\n";
    $self->SUPER::printObject();
    print "\n";
}

1;

