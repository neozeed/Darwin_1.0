#! /bin/sh

# eats a config file and creates named configuration files
#
# config file lines are:
#  comments - start with a '#'
#  blank - don't put anything in them
#  domain - define a domain
#    domain <name> <server> <who> [private]
#       <name> is domain name
#       <server> is the host name of the server - presumably a member of the
#                 domain
#       <who> gets mail from named [user name - NOT a fully qualified e-mail
#             address]
#       [private] marks zone as private
#  network - define a network
#    network <address> <domain>
#       <address> is network address in CIDER format: X.X.X.X/BITS
#       <domain> is the domain the network is in
#  <address> <host> [aliases] [# comment] - define a host address
#       <address> is the host address in CIDER format
#       <host> is the host name - w/o domain, that is figured out
#       [aliases] are literal aliases for the host - CNAME records
#       [# comment] is an inline comment - signaled by the leading '#'
#  allowed-hosts - defines list of hosts allowed to query private networks
#    allowed-hosts <address> [more addresses]
#        <address> a host address in CIDER format
#        [more addresses] 0 or more blank separted host addresses
#
# NOTES:
# 1. host addresses in CIDER format are not X.Y.Z.W/32, but X.Y.Z.W/BITS,
#    where BITS is the number of bits which are the network part of the
#    address in the subnet to which the host belongs

AWK=gawk
#AWK=awk

case $# in
  1) case $1 in
      -*) echo "$0 [config-file]" ; exit ;;
     esac
     CONFIG_FILE=$1 ;;
  0) CONFIG_FILE=hosts.cider ;;
  *) echo "$0 [config-file]" ; exit 1 ;;
esac

${AWK} --re-interval -f cider2named.awk ${CONFIG_FILE}

sed -e 's/^domain/# domain/' -e 's/^network/# name_server/' \
    -e 'sx/[0-9][0-9]*\([ 	]\)x\1x' ${CONFIG_FILE} >hosts

sed -e 's/$/ hosts/' make-clean.sh >make-clean
chmod +x make-clean

