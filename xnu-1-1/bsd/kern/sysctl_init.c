/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#include <sys/param.h>
#include <sys/buf.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>

extern struct sysctl_oid sysctl__debug_bpf_bufsize;

#if TUN
extern struct sysctl_oid sysctl__debug_if_tun_debug;
#endif

#if COMPAT_43
#ifndef NeXT
extern struct sysctl_oid sysctl__debug_ttydebug;
#endif
#endif

extern struct sysctl_oid sysctl__kern_dummy;
extern struct sysctl_oid sysctl__kern_ipc_maxsockbuf;
extern struct sysctl_oid sysctl__kern_ipc_nmbclusters;
extern struct sysctl_oid sysctl__kern_ipc_sockbuf_waste_factor;
extern struct sysctl_oid sysctl__kern_ipc_somaxconn;
extern struct sysctl_oid sysctl__kern_ipc_maxsockets;
extern struct sysctl_oid sysctl__net_inet_icmp_icmplim;
extern struct sysctl_oid sysctl__net_inet_icmp_maskrepl;
extern struct sysctl_oid sysctl__net_inet_icmp_bmcastecho;
extern struct sysctl_oid sysctl__net_inet_ip_accept_sourceroute;

#if IPCTL_DEFMTU
extern struct sysctl_oid sysctl__net_inet_ip_mtu;
#endif

extern struct sysctl_oid sysctl__net_inet_ip_ttl;
extern struct sysctl_oid sysctl__net_inet_ip_fastforwarding;
extern struct sysctl_oid sysctl__net_inet_ip_forwarding;
extern struct sysctl_oid sysctl__net_inet_ip_intr_queue_drops;
extern struct sysctl_oid sysctl__net_inet_ip_intr_queue_maxlen;
extern struct sysctl_oid sysctl__net_inet_ip_rtexpire;
extern struct sysctl_oid sysctl__net_inet_ip_rtmaxcache;
extern struct sysctl_oid sysctl__net_inet_ip_rtminexpire;
extern struct sysctl_oid sysctl__net_inet_ip_redirect;
extern struct sysctl_oid sysctl__net_inet_ip_sourceroute;
extern struct sysctl_oid sysctl__net_inet_ip_subnets_are_local;

#if DUMMYNET
extern struct sysctl_oid sysctl__net_inet_ip_dummynet_calls;
extern struct sysctl_oid sysctl__net_inet_ip_dummynet_debug;
extern struct sysctl_oid sysctl__net_inet_ip_dummynet_idle;
extern struct sysctl_oid sysctl__net_inet_ip_dummynet;
#endif

#if IPFIREWALL
extern struct sysctl_oid sysctl__net_inet_ip_fw_debug;
extern struct sysctl_oid sysctl__net_inet_ip_fw_verbose;
extern struct sysctl_oid sysctl__net_inet_ip_fw_verbose_limit;
extern struct sysctl_oid sysctl__net_inet_ip_fw_one_pass;
extern struct sysctl_oid sysctl__net_inet_ip_fw;
#endif

extern struct sysctl_oid sysctl__net_inet_raw_maxdgram;
extern struct sysctl_oid sysctl__net_inet_raw_recvspace;
extern struct sysctl_oid sysctl__net_inet_tcp_always_keepalive;
extern struct sysctl_oid sysctl__net_inet_tcp_delayed_ack;
extern struct sysctl_oid sysctl__net_inet_tcp_log_in_vain;
extern struct sysctl_oid sysctl__net_inet_tcp_pcbcount;
extern struct sysctl_oid sysctl__net_inet_tcp_rfc1323;
extern struct sysctl_oid sysctl__net_inet_tcp_rfc1644;
extern struct sysctl_oid sysctl__net_inet_tcp_keepidle;
extern struct sysctl_oid sysctl__net_inet_tcp_keepinit;
extern struct sysctl_oid sysctl__net_inet_tcp_keepintvl;
extern struct sysctl_oid sysctl__net_inet_tcp_mssdflt;
extern struct sysctl_oid sysctl__net_inet_tcp_recvspace;
extern struct sysctl_oid sysctl__net_inet_tcp_rttdflt;
extern struct sysctl_oid sysctl__net_inet_tcp_sendspace;
extern struct sysctl_oid sysctl__net_inet_udp_log_in_vain;
extern struct sysctl_oid sysctl__net_inet_udp_checksum;
extern struct sysctl_oid sysctl__net_inet_udp_maxdgram;
extern struct sysctl_oid sysctl__net_inet_udp_recvspace;

#if NETAT
extern struct sysctl_oid sysctl__net_appletalk_debug;
extern struct sysctl_oid sysctl__net_appletalk_routermix;
#endif /* NETAT */

#if BRIDGE
extern struct sysctl_oid sysctl__net_link_ether_bdgfwc;
extern struct sysctl_oid sysctl__net_link_ether_bdgfwt;
extern struct sysctl_oid sysctl__net_link_ether_bdginc;
extern struct sysctl_oid sysctl__net_link_ether_bdgint;
extern struct sysctl_oid sysctl__net_link_ether_bridge_ipfw;
extern struct sysctl_oid sysctl__net_link_ethe_bdgstats;
#endif

extern struct sysctl_oid sysctl__net_link_ether_inet_host_down_time;
extern struct sysctl_oid sysctl__net_link_ether_inet_max_age;
extern struct sysctl_oid sysctl__net_link_ether_inet_maxtries;
extern struct sysctl_oid sysctl__net_link_ether_inet_proxyall;
extern struct sysctl_oid sysctl__net_link_ether_inet_prune_intvl;
extern struct sysctl_oid sysctl__net_link_ether_inet_useloopback;

#if NETMIBS
extern struct sysctl_oid sysctl__net_link_generic_system_ifcount;
extern struct sysctl_oid sysctl__net_link_generic;
extern struct sysctl_oid sysctl__net_link_generic_ifdata;
extern struct sysctl_oid sysctl__net_link_generic_system;
#endif

#if VLAN
extern struct sysctl_oid sysctl__net_link_vlan_link_proto;
extern struct sysctl_oid sysctl__net_link_vlan;
extern struct sysctl_oid sysctl__net_link_vlan_link;
#endif

extern struct sysctl_oid sysctl__net_local_inflight;
extern struct sysctl_oid sysctl__net_local_dgram_maxdgram;
extern struct sysctl_oid sysctl__net_local_dgram_recvspace;
extern struct sysctl_oid sysctl__net_local_stream_recvspace;
extern struct sysctl_oid sysctl__net_local_stream_sendspace;

#if 0
extern struct sysctl_oid sysctl__vfs_nfs_nfs_privport;
extern struct sysctl_oid sysctl__vfs_nfs_async;
extern struct sysctl_oid sysctl__vfs_nfs_debug;
extern struct sysctl_oid sysctl__vfs_nfs_defect;
extern struct sysctl_oid sysctl__vfs_nfs_diskless_valid;
extern struct sysctl_oid sysctl__vfs_nfs_gatherdelay;
extern struct sysctl_oid sysctl__vfs_nfs_gatherdelay_v3;
extern struct sysctl_oid sysctl__vfs_nfs;
extern struct sysctl_oid sysctl__vfs_nfs_diskless_rootaddr;
extern struct sysctl_oid sysctl__vfs_nfs_diskless_swapaddr;
extern struct sysctl_oid sysctl__vfs_nfs_diskless_rootpath;
extern struct sysctl_oid sysctl__vfs_nfs_diskless_swappath;
extern struct sysctl_oid sysctl__vfs_nfs_nfsstats;
#endif

extern struct sysctl_oid sysctl__kern_ipc;
extern struct sysctl_oid sysctl__net_inet;

#if NETAT
extern struct sysctl_oid sysctl__net_appletalk;
#endif /* NETAT */

extern struct sysctl_oid sysctl__net_link;
extern struct sysctl_oid sysctl__net_local;
extern struct sysctl_oid sysctl__net_routetable;

#if IPDIVERT
extern struct sysctl_oid sysctl__net_inet_div;
#endif

extern struct sysctl_oid sysctl__net_inet_icmp;
extern struct sysctl_oid sysctl__net_inet_igmp;
extern struct sysctl_oid sysctl__net_inet_ip;
extern struct sysctl_oid sysctl__net_inet_raw;
extern struct sysctl_oid sysctl__net_inet_tcp;
extern struct sysctl_oid sysctl__net_inet_udp;
extern struct sysctl_oid sysctl__net_inet_ip_portrange;

extern struct sysctl_oid sysctl__net_link_ether;
extern struct sysctl_oid sysctl__net_link_ether_inet;

extern struct sysctl_oid sysctl__net_local_dgram;
extern struct sysctl_oid sysctl__net_local_stream;
extern struct sysctl_oid sysctl__sysctl_name;
extern struct sysctl_oid sysctl__sysctl_next;
extern struct sysctl_oid sysctl__sysctl_oidfmt;
extern struct sysctl_oid sysctl__net_inet_ip_portrange_first;
extern struct sysctl_oid sysctl__net_inet_ip_portrange_hifirst;
extern struct sysctl_oid sysctl__net_inet_ip_portrange_hilast;
extern struct sysctl_oid sysctl__net_inet_ip_portrange_last;
extern struct sysctl_oid sysctl__net_inet_ip_portrange_lowfirst;
extern struct sysctl_oid sysctl__net_inet_ip_portrange_lowlast;
extern struct sysctl_oid sysctl__net_inet_raw_pcblist;
extern struct sysctl_oid sysctl__net_inet_tcp_pcblist;
extern struct sysctl_oid sysctl__net_inet_udp_pcblist;
extern struct sysctl_oid sysctl__net_link_ether_bridge;
extern struct sysctl_oid sysctl__net_local_dgram_pcblist;
extern struct sysctl_oid sysctl__net_local_stream_pcblist;
extern struct sysctl_oid sysctl__sysctl_debug;
extern struct sysctl_oid sysctl__sysctl_name2oid;
extern struct sysctl_oid sysctl__net_inet_icmp_stats;
extern struct sysctl_oid sysctl__net_inet_igmp_stats;
extern struct sysctl_oid sysctl__net_inet_ip_stats;
extern struct sysctl_oid sysctl__net_inet_tcp_stats;
extern struct sysctl_oid sysctl__net_inet_udp_stats;
extern struct sysctl_oid sysctl__kern;
extern struct sysctl_oid sysctl__hw;
extern struct sysctl_oid sysctl__net;
extern struct sysctl_oid sysctl__debug;
extern struct sysctl_oid sysctl__vfs;
extern struct sysctl_oid sysctl__sysctl;





struct sysctl_oid *newsysctl_list[] =
{
    &sysctl__kern,
    &sysctl__hw,
    &sysctl__net,
    &sysctl__debug,
    &sysctl__vfs,
    &sysctl__sysctl,
    &sysctl__debug_bpf_bufsize
#if TUN
    ,&sysctl__debug_if_tun_debug
#endif

#if COMPAT_43
#ifndef NeXT
    ,&sysctl__debug_ttydebug
#endif
#endif

    ,&sysctl__kern_dummy
    ,&sysctl__kern_ipc_maxsockbuf
    ,&sysctl__kern_ipc_nmbclusters
    ,&sysctl__kern_ipc_sockbuf_waste_factor
    ,&sysctl__kern_ipc_somaxconn
    ,&sysctl__kern_ipc_maxsockets
    ,&sysctl__net_inet_icmp_icmplim
    ,&sysctl__net_inet_icmp_maskrepl
    ,&sysctl__net_inet_icmp_bmcastecho
    ,&sysctl__net_inet_ip_accept_sourceroute
#if IPCTL_DEFMTU
    ,&sysctl__net_inet_ip_mtu
#endif
    ,&sysctl__net_inet_ip_ttl
    ,&sysctl__net_inet_ip_fastforwarding
    ,&sysctl__net_inet_ip_forwarding
    ,&sysctl__net_inet_ip_intr_queue_drops
    ,&sysctl__net_inet_ip_intr_queue_maxlen
    ,&sysctl__net_inet_ip_rtexpire
    ,&sysctl__net_inet_ip_rtmaxcache
    ,&sysctl__net_inet_ip_rtminexpire
    ,&sysctl__net_inet_ip_redirect
    ,&sysctl__net_inet_ip_sourceroute
    ,&sysctl__net_inet_ip_subnets_are_local
#if DUMMYNET
    ,&sysctl__net_inet_ip_dummynet_calls
    ,&sysctl__net_inet_ip_dummynet_debug
    ,&sysctl__net_inet_ip_dummynet_idle
    ,&sysctl__net_inet_ip_dummynet
#endif

#if IPFIREWALL
    ,&sysctl__net_inet_ip_fw_debug
    ,&sysctl__net_inet_ip_fw_verbose
    ,&sysctl__net_inet_ip_fw_verbose_limit
    ,&sysctl__net_inet_ip_fw_one_pass
    ,&sysctl__net_inet_ip_fw
#endif
    ,&sysctl__net_inet_raw_maxdgram
    ,&sysctl__net_inet_raw_recvspace
    ,&sysctl__net_inet_tcp_always_keepalive
    ,&sysctl__net_inet_tcp_delayed_ack
    ,&sysctl__net_inet_tcp_log_in_vain
    ,&sysctl__net_inet_tcp_pcbcount
    ,&sysctl__net_inet_tcp_rfc1323
    ,&sysctl__net_inet_tcp_rfc1644
    ,&sysctl__net_inet_tcp_keepidle
    ,&sysctl__net_inet_tcp_keepinit
    ,&sysctl__net_inet_tcp_keepintvl
    ,&sysctl__net_inet_tcp_mssdflt
    ,&sysctl__net_inet_tcp_recvspace
    ,&sysctl__net_inet_tcp_rttdflt
    ,&sysctl__net_inet_tcp_sendspace
    ,&sysctl__net_inet_udp_log_in_vain 
    ,&sysctl__net_inet_udp_checksum
    ,&sysctl__net_inet_udp_maxdgram
    ,&sysctl__net_inet_udp_recvspace

#if NETAT
    ,&sysctl__net_appletalk_debug
    ,&sysctl__net_appletalk_routermix
#endif /* NETAT */

#if BRIDGE
    ,&sysctl__net_link_ether_bdgfwc
    ,&sysctl__net_link_ether_bdgfwt
    ,&sysctl__net_link_ether_bdginc
    ,&sysctl__net_link_ether_bdgint
    ,&sysctl__net_link_ether_bridge_ipfw
    ,&sysctl__net_link_ethe_bdgstats
    ,&sysctl__net_link_ether_bridge
#endif

    ,&sysctl__net_link_ether_inet_host_down_time
    ,&sysctl__net_link_ether_inet_max_age
    ,&sysctl__net_link_ether_inet_maxtries
    ,&sysctl__net_link_ether_inet_proxyall
    ,&sysctl__net_link_ether_inet_prune_intvl
    ,&sysctl__net_link_ether_inet_useloopback
#if NETMIBS
    ,&sysctl__net_link_generic_system_ifcount
    ,&sysctl__net_link_generic
    ,&sysctl__net_link_generic_ifdata
    ,&sysctl__net_link_generic_system
#endif

#if VLAN
    ,&sysctl__net_link_vlan_link_proto
    ,&sysctl__net_link_vlan
    ,&sysctl__net_link_vlan_link
#endif

    ,&sysctl__net_local_inflight
    ,&sysctl__net_local_dgram_maxdgram
    ,&sysctl__net_local_dgram_recvspace
    ,&sysctl__net_local_stream_recvspace
    ,&sysctl__net_local_stream_sendspace
#if 0
    ,&sysctl__vfs_nfs_nfs_privport
    ,&sysctl__vfs_nfs_async
    ,&sysctl__vfs_nfs_debug
    ,&sysctl__vfs_nfs_defect
    ,&sysctl__vfs_nfs_diskless_valid
    ,&sysctl__vfs_nfs_gatherdelay
    ,&sysctl__vfs_nfs_gatherdelay_v3
    ,&sysctl__vfs_nfs
    ,&sysctl__vfs_nfs_diskless_rootaddr
    ,&sysctl__vfs_nfs_diskless_swapaddr
    ,&sysctl__vfs_nfs_diskless_rootpath
    ,&sysctl__vfs_nfs_diskless_swappath
    ,&sysctl__vfs_nfs_nfsstats
#endif
    ,&sysctl__kern_ipc
    ,&sysctl__net_inet
#if NETAT
    ,&sysctl__net_appletalk
#endif /* NETAT */
    ,&sysctl__net_link
    ,&sysctl__net_local
    ,&sysctl__net_routetable
#if IPDIVERT
    ,&sysctl__net_inet_div
#endif
    ,&sysctl__net_inet_icmp
    ,&sysctl__net_inet_igmp
    ,&sysctl__net_inet_ip
    ,&sysctl__net_inet_raw
    ,&sysctl__net_inet_tcp
    ,&sysctl__net_inet_udp
    ,&sysctl__net_inet_ip_portrange
    ,&sysctl__net_link_ether
    ,&sysctl__net_link_ether_inet
    ,&sysctl__net_local_dgram
    ,&sysctl__net_local_stream
    ,&sysctl__sysctl_name
    ,&sysctl__sysctl_next
    ,&sysctl__sysctl_oidfmt
    ,&sysctl__net_inet_ip_portrange_first
    ,&sysctl__net_inet_ip_portrange_hifirst
    ,&sysctl__net_inet_ip_portrange_hilast
    ,&sysctl__net_inet_ip_portrange_last
    ,&sysctl__net_inet_ip_portrange_lowfirst
    ,&sysctl__net_inet_ip_portrange_lowlast
    ,&sysctl__net_inet_raw_pcblist
    ,&sysctl__net_inet_tcp_pcblist
    ,&sysctl__net_inet_udp_pcblist
    ,&sysctl__net_local_dgram_pcblist
    ,&sysctl__net_local_stream_pcblist
    ,&sysctl__sysctl_debug
    ,&sysctl__sysctl_name2oid
    ,&sysctl__net_inet_icmp_stats
    ,&sysctl__net_inet_igmp_stats
    ,&sysctl__net_inet_ip_stats
    ,&sysctl__net_inet_tcp_stats
    ,&sysctl__net_inet_udp_stats
    ,(struct sysctl_oid *) 0
};

