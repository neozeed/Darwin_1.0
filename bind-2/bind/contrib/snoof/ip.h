
/* adapted from tcpdump */

#ifndef IPVERSION
  #define IPVERSION 4
#endif  /* IPVERISON */

struct iphdr {
  u_char  ihl:4,	/* header length */
          version:4;	/* version */
  u_char  tos;		/* type of service */
  short   tot_len;	/* total length */
  u_short id;		/* identification */
  short   off;		/* fragment offset field */
#define IP_DF   0x4000	/* dont fragment flag */
#define IP_MF   0x2000	/* more fragments flag */
  u_char  ttl;		/* time to live */
  u_char  protocol;	/* protocol */
  u_short check;	/* checksum */
  struct  in_addr saddr, daddr;  /* source and dest address */
};

#ifndef IP_MAXPACKET
  #define IP_MAXPACKET 65535
#endif  /* IP_MAXPACKET */

