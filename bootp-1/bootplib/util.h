/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
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

#import <unistd.h>
#import <stdlib.h>
#import <stdio.h>
#import <netinet/in.h>
#import <sys/types.h>
#import <sys/stat.h>
#import <sys/param.h>
#import <errno.h>
#import <mach/boolean.h>
#import <string.h>
#import <ctype.h>

#define IP_FORMAT	"%d.%d.%d.%d"
#define IP_CH(ip)	((u_char *)ip)
#define IP_LIST(ip)	IP_CH(ip)[0],IP_CH(ip)[1],IP_CH(ip)[2],IP_CH(ip)[3]

typedef struct {
	struct in_addr	start;
	struct in_addr	end;
} ip_range_t;

static __inline__
u_long iptohl(struct in_addr ip)
{
    return (ntohl(ip.s_addr));
}

static __inline__
struct in_addr hltoip(u_long l)
{
    struct in_addr ip;

    ip.s_addr = htonl(l);
    return (ip);
}

static __inline__ boolean_t
in_subnet(struct in_addr netaddr, struct in_addr netmask, struct in_addr ip)
{
    if ((iptohl(ip) & iptohl(netmask)) != iptohl(netaddr)) {
	return (FALSE);
    }
    return (TRUE);
}

static __inline__
int ipRangeCmp(ip_range_t * a_p, ip_range_t * b_p, boolean_t * overlap)
{
    u_long		b_start = iptohl(b_p->start);
    u_long		b_end = iptohl(b_p->end);
    u_long		a_start = iptohl(a_p->start);
    u_long		a_end = iptohl(a_p->end);
    int			result = 0;
    
    *overlap = TRUE;
    if (a_start == b_start) {
	result = 0;
    }
    else if (a_start < b_start) {
	result = -1;
	if (a_end < b_start)
	    *overlap = FALSE;
    }
    else {
	result = 1;
	if (b_end < a_start)
	    *overlap = FALSE;
    }
    return (result);
}

/* nbits_host: number of bits of host address */
static __inline__ int
nbits_host(struct in_addr mask)
{
    u_long l = iptohl(mask);
    int i;

    for (i = 0; i < 32; i++) {
	if (l & (1 << i))
	    return (32 - i);
    }
    return (32);
}

static __inline__ u_char *
inet_nettoa(struct in_addr addr, struct in_addr mask)
{
    u_char *		addr_p;
    int 		nbits = nbits_host(mask);
    int 		nbytes;
    static u_char 	sbuf[32];
    u_char 		tmp[8];

#define NBITS_PER_BYTE	8    
    sbuf[0] = '\0';
    nbytes = (nbits + NBITS_PER_BYTE - 1) / NBITS_PER_BYTE;
//    printf("-- nbits %d, nbytes %d--", nbits, nbytes);
    for (addr_p = (u_char *)&addr.s_addr; nbytes > 0; addr_p++) {

	sprintf(tmp, "%d%s", *addr_p, nbytes > 1 ? "." : "");
	strcat(sbuf, tmp);
	nbytes--;
    }
    if (nbits % NBITS_PER_BYTE) {
	sprintf(tmp, "/%d", nbits);
	strcat(sbuf, tmp);
    }
    return (sbuf);
}

static __inline__ long
random_range(long bottom, long top)
{
    long ret;
    long number = top - bottom + 1;
    long range_size = LONG_MAX / number;
    ret = (random() / range_size) + bottom;
    return (ret);
}

/*
 * Function: timeval_subtract
 *
 * Purpose:
 *   Computes result = tv1 - tv2.
 */
static __inline__ void
timeval_subtract(struct timeval tv1, struct timeval tv2, 
		 struct timeval * result)
{
#define USECS_PER_SEC	1000000
    result->tv_sec = tv1.tv_sec - tv2.tv_sec;
    result->tv_usec = tv1.tv_usec - tv2.tv_usec;
    if (result->tv_sec > 0 && result->tv_usec < 0) {
	result->tv_usec += USECS_PER_SEC;
	result->tv_sec--;
    }
    return;
}

/*
 * Function: timeval_subtract
 *
 * Purpose:
 *   Computes result = tv1 + tv2.
 */
static __inline__ void
timeval_add(struct timeval tv1, struct timeval tv2,
	    struct timeval * result)
{
    result->tv_sec = tv1.tv_sec + tv2.tv_sec;
    result->tv_usec = tv1.tv_usec + tv2.tv_usec;
    if (result->tv_usec > USECS_PER_SEC) {
	result->tv_usec -= USECS_PER_SEC;
	result->tv_sec++;
    }
    return;
}

static __inline__ void
printData(u_char * data_p, int n_bytes)
{
#define CHARS_PER_LINE 	16
    char		line_buf[CHARS_PER_LINE + 1];
    int			line_pos;
    int			offset;

    for (line_pos = 0, offset = 0; offset < n_bytes; offset++, data_p++) {
	if (line_pos == 0)
	    printf("%04x ", offset);

	line_buf[line_pos] = isprint(*data_p) ? *data_p : '.';
	printf(" %02x", *data_p);
	line_pos++;
	if (line_pos == CHARS_PER_LINE) {
	    line_buf[CHARS_PER_LINE] = '\0';
	    printf("  %s\n", line_buf);
	    line_pos = 0;
	}
	else if (line_pos == (CHARS_PER_LINE / 2))
	    printf(" ");
    }
    if (line_pos) { /* need to finish up the line */
	for (; line_pos < CHARS_PER_LINE; line_pos++) {
	    printf("   ");
	    line_buf[line_pos] = ' ';
	}
	line_buf[CHARS_PER_LINE] = '\0';
	printf("  %s\n", line_buf);
    }
}

/*
 * Function: create_path
 *
 * Purpose:
 *   Create the given directory hierarchy.  Return -1 if anything
 *   went wrong, 0 if successful.
 */
static __inline__ int
create_path(u_char * dirname, mode_t mode)
{
    boolean_t	done = FALSE;
    u_char *	scan;

    if (mkdir(dirname, mode) == 0 || errno == EEXIST)
	return (0);

    if (errno != ENOENT)
	return (-1);

    {
	u_char	path[PATH_MAX];

	for (path[0] = '\0', scan = dirname; done == FALSE;) {
	    u_char * 	next_sep;
	    
	    if (scan == NULL || *scan != '/')
		return (FALSE);
	    scan++;
	    next_sep = strchr(scan, '/');
	    if (next_sep == 0) {
		done = TRUE;
		next_sep = dirname + strlen(dirname);
	    }
	    strncpy(path, dirname , next_sep - dirname);
	    path[next_sep - dirname] = '\0';
	    if (mkdir(path, mode) == 0 || errno == EEXIST)
		;
	    else
		return (-1);
	    scan = next_sep;
	}
    }
    return (0);
}
