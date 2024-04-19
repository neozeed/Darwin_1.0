# Generated automatically from Makefile.in by configure.
#### Start of system configuration section. ####

srcdir = .

INSTALL = /usr/bin/ginstall -c
INSTALL_PROGRAM = ${INSTALL}
M4 = @M4@
AWK = gawk
PERL = /usr/local/bin/perl
CC = gcc

prefix = /usr/local
exec_prefix = ${prefix}

# Directory in which to install scripts.
bindir = ${exec_prefix}/bin

# man directory
mandir = ${prefix}/man

#### End of system configuration section. ####

SHELL = /bin/sh

PROGS= poke_ns poke_dhcp gendns readinfo updatehosts genstatic

SHELL = /bin/sh

MANPAGES = gendhcp.1 gendns.1 poke_dhcp.1 poke_ns.1 readinfo.1 updatehosts.1

man: 
	for f in ${MANPAGES}; do ${INSTALL_PROGRAM} -m 444 $$f ${mandir}/man1; done
