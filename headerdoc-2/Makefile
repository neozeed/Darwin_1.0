##
# Makefile for headerdoc
# Wilfredo Sanchez | wsanchez@apple.com
##

DSTROOT = /tmp/headerdoc/Release

bindir  = /usr/bin
program = headerdoc2html

perl_libdir := $(shell perl -e 'require Config; print "$$Config::Config{'privlib'}\n";')
startperl   := $(shell perl -e 'require Config; print "$$Config::Config{'startperl'}\n";')

all:

installhdrs:

build:

install:
	umask 022 && install -d $(DSTROOT)$(perl_libdir)
	install -c -m 444 Modules/*.pm $(DSTROOT)$(perl_libdir)
	umask 022 && install -d $(DSTROOT)$(bindir)
	install -c -m 755 headerDoc2HTML.pl $(DSTROOT)$(bindir)/$(program)
	perl -i -pe 's|^#!/usr/bin/perl.*$$|$(startperl)|;' $(DSTROOT)$(bindir)/$(program)
	chmod 555 $(DSTROOT)$(bindir)/$(program)
