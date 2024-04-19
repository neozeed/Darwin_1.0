/*
		>>> ADM z0ne.c 1.1 <<<

	this program makes a list of domains from the arguments given, and
	does axfr transfer for these domains and their subdomains. when its
	done it print out a sorted list of ips to stdout.

	options:
		-c	zero out last byte of ips and print out all 256
			possible instead.
		-l	also print 127.* ips. these are by default ignored.
		-o	print ips as they are found.

	- crazy-b
*/

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/wait.h>
#include <signal.h>

#define VER_STR "1.1"
#define DNS_PORT 53

#define CONNECT_TIMEOUT_VAL 15
#define READ_TIMEOUT_VAL 50

char *progname;
int eoz = 0;

u_char *nslist_ptr, *nslist_cur;
u_long nslist_size = 0;
u_char *domlist_ptr, *domlist_cur;
u_long domlist_size = 0;
char *domain = NULL;
char *nameserv = NULL;
u_char *iplist_ptr;
u_long iplist_count = 0;
u_long *iplistt;

int sorted = 1, localips = 0, allclassc = 0;

int sock;
void connect_and_read_timeout_handler() {	/* nice name, huh? :) */
	close(sock);
}

void usage() {
	fprintf(stderr,
		"\nADM\tz0ne %s - crazy-b\n\n"
		"usage:\t%s [-clo] [domain...]\n\n",
		VER_STR, progname
	);
	exit(0);
}

typedef union {
	HEADER qb1;
	u_char qb2[PACKETSZ];
} querybuf;

int addIP(u_char *ip) {
	u_long *ip1, *ip2;

	ip2 = (u_long *)ip;
	*ip2 = htonl(*ip2);

	if (allclassc) *ip = 0;

	if ((!localips) && (*(ip + 3) == 127)) return(0);

	if (iplist_count == 0) {
		iplist_count++;
		iplist_ptr = (char *)malloc(iplist_count * 4);
		ip1 = (u_long *)iplist_ptr;
		*ip1 = *ip2;
		*ip2 = htonl(*ip2);
		return(1);
	}

	ip1 = (u_long *)iplist_ptr;
	while (ip1 < (u_long *)(iplist_ptr + (iplist_count * 4))) {
		if (*ip1 == *ip2) return(0);
		ip1++;
	}

	iplist_count++;
	iplist_ptr = (char *)realloc(iplist_ptr, iplist_count * 4);
	ip1 = (u_long *)(iplist_ptr + ((iplist_count *4) - 4));
	*ip1 = *ip2;

	*ip2 = htonl(*ip2);
	return(1);
}

int addDomain(char *name) {
	u_char *tmp;
	u_long x;

	if (domlist_size == 0) {
		domlist_size = strlen(name) + 1;
		domlist_ptr = (char *)malloc(domlist_size);
		domlist_cur = domlist_ptr;
		memcpy(domlist_ptr, name, strlen(name) + 1);
		return(1);
	}

	tmp = domlist_ptr;
	while (tmp < (domlist_ptr + domlist_size)) {
		if ((strlen(name) == strlen(tmp))
		&& (!strncasecmp(name, tmp, strlen(tmp))))
			return(0);
		tmp += strlen(tmp) + 1;
	}

	x = domlist_cur - domlist_ptr;
	domlist_size += strlen(name) + 1;
	domlist_ptr = (char *)realloc(domlist_ptr, domlist_size);
	domlist_cur = domlist_ptr + x;
	memcpy((domlist_ptr + domlist_size) - (strlen(name) + 1), name, strlen(name) + 1);

	return(1);
}

int nextDomain() {
	if ((domlist_cur - domlist_ptr) >= domlist_size)
		return(0);

	if (domain != NULL) free(domain);
	domain = (u_char *)malloc(strlen(domlist_cur) + 1);
	memcpy(domain, domlist_cur, strlen(domlist_cur) + 1);

	domlist_cur += strlen(domlist_cur) + 1;
	return(1);
}

int parsepaq (char *paq, char *eom, char *zone) {
	register u_char *cp = paq + HFIXEDSZ;
	HEADER *headerPtr = (HEADER *)paq;
	char name[512];
	int nameLen, type, class, dlen, x;
	u_int32_t ttl;
	struct in_addr in;

	if (headerPtr->rcode != 0) return(0);
	if (headerPtr->ancount <= 0) return(0);

	if (ntohs(headerPtr->qdcount) > 0) {
		nameLen = dn_skipname(cp, eom);
		if (nameLen < 0) return(0);
		cp += nameLen + QFIXEDSZ;
	}
	nameLen = dn_expand(paq, eom, cp, name, sizeof(name));
	if (nameLen < 0) return(0);
	cp += nameLen;

	type = _getshort((u_char *)cp);
	cp += 2;
	class = _getshort((u_char *)cp);
	cp += 2;
	ttl = _getlong((u_char *)cp);
	cp += 4;
	dlen = _getshort((u_char *)cp);
	cp += 2;

	if (class != C_IN) return(1);

	if (type == T_A) {
		memcpy((char *)&in, cp, 4);
		if (dlen != 4 && dlen != 7) return(1);
		if (
			(strlen(name) >= strlen(zone)) &&
			!strncasecmp(zone, name + (strlen(name) - strlen(zone)), strlen(zone))
		) {
			if (addIP(cp) && !sorted) {
				if (allclassc) {
					in.s_addr &= 0xffffff;
					for (x = 0; x <= 255; x++) {
						printf("%s\n", inet_ntoa(in));
						in.s_addr = htonl(htonl(in.s_addr) + 1);
					}
				} else printf("%s\n", inet_ntoa(in));
				fflush(stdout);
			}
		}
	} else
	if (type == T_NS) {
		if (
			(strlen(name) >= strlen(zone)) &&
			!strncasecmp(zone, name + (strlen(name) - strlen(zone)), strlen(zone))
		) {
			addDomain(name);
		}
	} else
	if (type == T_SOA) {
		eoz++;
	}

	return(1);
}

int getAXFR(char *zone, char *serv) {
	struct hostent *hp;
	struct sockaddr_in sa;
	long addr;
	int stat;

	querybuf buf;
	int msglen;
	u_short len;
	u_char *cp;
	int amtToRead;
	int numRead;
	static int answerLen = 0;
	static u_char *answer = NULL;
	HEADER *headerPtr;

	msglen = res_mkquery(QUERY, zone, C_IN, T_AXFR, NULL,
			     0, 0, buf.qb2, sizeof(buf));
	if (msglen == -1) return(0);

	memset(&sa, 0, sizeof(sa));

	if ((addr = inet_addr(serv)) != -1) {
		if (addr == 0) return(0);
		memcpy(&sa.sin_addr, &addr, sizeof(addr));
		sa.sin_family = AF_INET;
	} else {
		if ((hp = gethostbyname(serv)) == NULL) return(0);
		memcpy(&sa.sin_addr, hp->h_addr, hp->h_length);
		sa.sin_family = hp->h_addrtype;
	}
	sa.sin_port = htons((u_short) DNS_PORT);

	if ((sock = socket(sa.sin_family, SOCK_STREAM, 0)) < 0) return(0);

	signal(SIGALRM, (void *)connect_and_read_timeout_handler);
	alarm(CONNECT_TIMEOUT_VAL);
	if (connect(sock, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
		alarm(0);
		return(0);
	}
	alarm(0);

	__putshort(msglen, (u_char *)&len);

	if (write(sock, (char *)&len, 2) != 2 ||
	    write(sock, (char *)&buf, msglen) != msglen) {
		close(sock);
		return(0);
	}

	while(1) {
		cp = (u_char *)&len;
		amtToRead = 2;

		signal(SIGALRM, (void *)connect_and_read_timeout_handler);
		alarm(READ_TIMEOUT_VAL);
		while ((numRead = read(sock, cp, amtToRead)) > 0) {
			alarm(READ_TIMEOUT_VAL);
			cp += numRead;
			if ((amtToRead -= numRead) <= 0)
				break;
		}
		alarm(0);

		if (numRead <= 0) break;
		if ((len = htons(len)) == 0) break;

		if (len > (u_int)answerLen) {
			if (answerLen != 0) free(answer);
			answerLen = len;
			answer = (u_char *)malloc(answerLen);
		}

		amtToRead = len;
		cp = answer;
		signal(SIGALRM, (void *)connect_and_read_timeout_handler);
		alarm(READ_TIMEOUT_VAL);
		while (amtToRead > 0 && (numRead = read(sock, cp, amtToRead)) > 0) {
			alarm(READ_TIMEOUT_VAL);
			cp += numRead;
			amtToRead -= numRead;
		}
		alarm(0);
		if (numRead <= 0) break;

		if (!parsepaq(answer, cp, zone)) {
			close(sock);
			return(0);
		}
		if (eoz == 2) {
			close(sock);
			eoz = 0;
			return(1);
		}
	}

	close(sock);
	return(1);
}

int getNS(char *zone) {
	querybuf buf, answer;
	int msglen, nscount, numns, numsaddr, type, dlen;
	char dom[512], name[512];
	u_char *cp;

	msglen = res_mkquery(QUERY, zone, C_IN, T_NS, NULL,
			     0, 0, buf.qb2, sizeof(buf));
	if (msglen < 0) return(0);

	msglen = res_send(buf.qb2, msglen, answer.qb2, sizeof(answer));
	if (msglen < 0) return(0);

	nscount = ntohs(answer.qb1.ancount) + ntohs(answer.qb1.nscount) +
		  ntohs(answer.qb1.arcount);

	if (answer.qb1.rcode != 0 || nscount == 0) return(0);

	cp = (u_char *)answer.qb2 + HFIXEDSZ;
	if (ntohs(answer.qb1.qdcount) > 0)
		cp += dn_skipname(cp, answer.qb2 + msglen) + QFIXEDSZ;

	numns = numsaddr = 0;

	if (nslist_size != 0) {
		free(nslist_ptr);
		nslist_size = 0;
	}

	for (;nscount; nscount--) {
		cp += dn_expand(answer.qb2, answer.qb2 + msglen, cp,
				dom, sizeof(dom));
		type = _getshort(cp);
		cp += 8;
		dlen = _getshort(cp);
		cp += 2;

		if (type == T_NS) {
			if (dn_expand(answer.qb2, answer.qb2 + msglen, cp,
			    name, sizeof(name)) >= 0) {
				if (nslist_size == 0) {
					nslist_size = strlen(name) + 1;
					nslist_ptr = (char *)malloc(nslist_size);
					nslist_cur = nslist_ptr;
				} else {
					nslist_size += strlen(name) + 1;
					nslist_ptr = (char *)realloc(nslist_ptr, nslist_size);
					nslist_cur = (nslist_ptr + nslist_size) - (strlen(name) + 1);
				}
				memcpy(nslist_cur, name, strlen(name) + 1);
			}
		}
		cp += dlen;
	}

	nslist_cur = nslist_ptr;
	return(1);
}

int nextNS() {
	if ((nslist_cur - nslist_ptr) >= nslist_size)
		return(0);

	if (nameserv != NULL) free(nameserv);
	nameserv = (u_char *)malloc(strlen(nslist_cur) + 1);
	memcpy(nameserv, nslist_cur, strlen(nslist_cur) + 1);

	nslist_cur += strlen(nslist_cur) + 1;
	return(1);
}

void ip_sort(u_long *list, u_long count, u_long *temp) {
	u_long nlo, nhi;
	u_long *lo, *hi, *t;

	if (count == 2) {
		if (*list > *(list + 1))
			*temp = *list, *list = *(list + 1), *(list + 1) = *temp;
		return;
	}

	nlo = count / 2;
	lo = list;
	nhi = count - nlo;
	hi = list + nlo;

	if (nlo > 1)
		ip_sort(lo, nlo, temp);
	if (nhi > 1)
		ip_sort(hi, nhi, temp);

	t = temp;

	while (nlo && nhi)
		if (*lo <= *hi)
			*t++ = *lo++, --nlo;
		else
			*t++ = *hi++, --nhi;
	while (nlo--)
		*t++ = *lo++;

	for (lo = list, nlo = count - nhi, t = temp; nlo; --nlo)
		*lo++ = *t++;
}

void ip_print() {
	struct in_addr in;
	u_long n;
	int x;

	for (n = 0; n < iplist_count; n++) {
		in.s_addr = htonl(*(u_long *)(iplist_ptr + (n * 4)));
		if (allclassc) {
			for (x = 0; x <= 255; x++) {
				printf("%s\n", inet_ntoa(in));
				in.s_addr = htonl(htonl(in.s_addr) + 1);
			}
		} else printf("%s\n", inet_ntoa(in));
	}
	fflush(stdout);
}

int main(int argc, char **argv) {
	int x, n;

	progname = argv[0];
	for (n = strlen(argv[0]); n >= 0; n--)
		if (argv[0][n] == '/') {
			progname += n + 1;
			break;
	}

	if (argc < 2) usage();

	for (n = 1; n < argc; n++) {
		if (argv[n][0] == '-') {
			if (strlen(argv[n]) <= 1) usage();
			for (x = 1; x < strlen(argv[n]); x++)
			switch(argv[n][x]) {
				case 'o':
					sorted = 0;
					break;
				case 'c':
					allclassc = 1;
					break;
				case 'l':
					localips = 1;
					break;
				default:
					usage();
			}
		} else addDomain(argv[n]);
	}

	while (nextDomain()) if (getNS(domain))
		while (nextNS() && !getAXFR(domain, nameserv));

	if (sorted) {
		iplistt = (u_long *)malloc(iplist_count * 4);
		ip_sort((u_long *)iplist_ptr, iplist_count, iplistt);
		ip_print();
	}
}

