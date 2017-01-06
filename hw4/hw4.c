#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "dns.h"
#include <time.h>



int resolve_name(int sock, uint8_t * request, int packet_size, uint8_t * response, struct sockaddr_storage * nameservers, int nameserver_count);
struct Cache_server
{
  char url[100];
  uint8_t response_cache[UDP_RECV_SIZE];
  int packetsiz;
  long ttl;

};
int r_flag,counter_timer=0;
char URL[100];
typedef struct addrinfo saddrinfo;
typedef struct sockaddr_storage sss,new_str;
int p=0;
new_str backup_nameserver_ns[255];
struct Cache_server cs[1000];
int root_server_count;
sss root_servers[255];
int cc=0,backup_nameserver_count=0,backup_chosen=0;
static int debug=0;
void usage() {
  printf("Usage: hw4 [-d] [-p port]\n\t-d: debug\n\t-p: port\n");
  exit(1);
}

/* returns: true if answer found, false if not.
 * side effect: on answer found, populate result with ip address.
 */



// wrapper for inet_ntop that takes a sockaddr_storage as argument
const char * ss_ntop(struct sockaddr_storage * ss, char * dst, int dstlen)
{     
  void * addr;
  if (ss->ss_family == AF_INET)
    addr = &(((struct sockaddr_in*)ss)->sin_addr);
  else if (ss->ss_family == AF_INET6)
    addr = &(((struct sockaddr_in6*)ss)->sin6_addr);
  else
  {
    if (debug)
      printf("error parsing ip address\n");
    return NULL;
  }
  return inet_ntop(ss->ss_family, addr, dst, dstlen);
}

/*
 * wrapper for inet_pton that detects a valid ipv4/ipv6 string and returns it in pointer to
 * sockaddr_storage dst
 *
 * return value is consistent with inet_pton
 */
int ss_pton(const char * src, void * dst){
  // try ipv4
  unsigned char buf[sizeof(struct in6_addr)];
  int r;
  r = inet_pton(AF_INET,src,buf);
  if (r == 1){
    char printbuf[INET6_ADDRSTRLEN];
    struct sockaddr_in6 * out = (struct sockaddr_in6*)dst;
    // for socket purposes, we need a v4-mapped ipv6 address
    unsigned char * mapped_dst = (void*)&out->sin6_addr;
    // take the first 4 bytes of buf and put them in the last 4
    // of the return value
    memcpy(mapped_dst+12,buf,4);
    // set the first 10 bytes to 0
    memset(mapped_dst,0,10);
    // set the next 2 bytes to 0xff
    memset(mapped_dst+10,0xff,2);
    out->sin6_family = AF_INET6;
    return 1;
  }
  r = inet_pton(AF_INET6,src,buf);
  if (r == 1){
    struct sockaddr_in6 * out = (struct sockaddr_in6*)dst;
    out->sin6_family = AF_INET6;
    out->sin6_addr = *((struct in6_addr*)buf);
    return 1;
  }
  return r;
}


void read_server_file() {
  root_server_count=0;
  char addr[25];

  FILE *f = fopen("root-servers.txt","r");
  while(fscanf(f," %s ",addr) > 0){
    ss_pton(addr,&root_servers[root_server_count++]);
  }
}



/* constructs a DNS query message for the provided hostname */
int construct_query(uint8_t* query, int max_query, char* hostname,int qtype) {
if(debug)
printf("\nWe are creating a new Query\n");
  memset(query,0,max_query);
  // does the hostname actually look like an IP address? If so, make
  // it a reverse lookup. 
  in_addr_t rev_addr=inet_addr(hostname);
  if(rev_addr!=INADDR_NONE) {
    static char reverse_name[255];    
    sprintf(reverse_name,"%d.%d.%d.%d.in-addr.arpa",
        (rev_addr&0xff000000)>>24,
        (rev_addr&0xff0000)>>16,
        (rev_addr&0xff00)>>8,
        (rev_addr&0xff));
    hostname=reverse_name;
  }
  // first part of the query is a fixed size header
  struct dns_hdr *hdr = (struct dns_hdr*)query;
  // generate a random 16-bit number for session
  uint16_t query_id = (uint16_t) (random() & 0xffff);
 // uint16_t query_id = header_id;
  hdr->id = htons(query_id);
  //hdr->id = query_id;
  
  // set header flags to request recursive query
  hdr->flags = htons(0x0000); 
  // 1 question, no answers or other records
  hdr->q_count=htons(1);
  // add the name
  int query_len = sizeof(struct dns_hdr); 
  int name_len=to_dns_style(hostname,query+query_len);
  query_len += name_len; 
  // now the query type: A/AAAA or PTR. 
  uint16_t *type = (uint16_t*)(query+query_len);
  if(rev_addr!=INADDR_NONE)
  {
    *type = htons(12);
  }
  else
  {
    *type = htons(qtype);
  }
  query_len+=2;
  //finally the class: INET
  uint16_t *class = (uint16_t*)(query+query_len);
  *class = htons(1);
  query_len += 2;
  return query_len; 
}



//Change the query ID of the request packet 
void setQueryID(uint8_t* response, uint16_t id)
{
 
  struct dns_hdr *res= (struct dns_hdr*)response;
  if(debug)
  printf("Changing req header id from %u to %u\n",res->id, id);
  res->id=id;

}

//decrease the cache time 
void chngtime(uint8_t * response,long ttlnew)
{
  struct dns_hdr * res = (struct dns_hdr *)response;
  if(debug)
  printf("Time decreasing:%ld\n",(ttlnew));
  // parse the response to get our answer
  
  uint8_t * answer_ptr = response + sizeof(struct dns_hdr);

  // now answer_ptr points at the first question.
  int question_count = ntohs(res->q_count);
  int answer_count = ntohs(res->a_count);
  int auth_count = ntohs(res->auth_count);
  int other_count = ntohs(res->other_count);
  int id=ntohs(res->id);
  

char string_name[255];
  // skip questions
int q = 0;
  for(q=0; q<question_count; q++){
    
    memset(string_name,0,255);
    int size=from_dns_style(response, answer_ptr,string_name);
    answer_ptr+=size;
    answer_ptr+=4;
  }

  

  /*
   * iterate through answer, authoritative, and additional records
   */
   int a = 0;
  for(a=0; a<answer_count;a++)
  {
    int dnsnamelen=from_dns_style(response,answer_ptr,string_name);
    answer_ptr += dnsnamelen;
    struct dns_rr* rr = (struct dns_rr*)answer_ptr;
    answer_ptr+=sizeof(struct dns_rr);
    rr->ttl=htonl(ttlnew);
    answer_ptr+=htons(rr->datalen);

  }
    
}

 void getRequestURL(uint8_t * request)
{
  struct dns_hdr * req= (struct dns_hdr *)request;
  uint8_t * req_ptr= request+ sizeof(struct dns_hdr);
  int req_qs=ntohs(req->q_count);
  
  int z = 0;
  for(z=0;z<req_qs;z++)
  {
    memset(&URL,0,100);
    int xx=from_dns_style(request,req_ptr,URL);
    req_ptr+=xx;
    req_ptr+=4;
  }
  if(debug)
  printf("URL from request   %s\n", URL);

}


int checkCache(uint8_t *request, uint8_t * response)
{

  struct dns_hdr * req= (struct dns_hdr *)request;
  uint16_t reqid= req->id;
   int response_length=0,z=0,a=0;
   time_t now;
   now=time(NULL);
   if(debug)
    printf("Time now : %ld\n", now);
   getRequestURL(request);
  if(debug)
    printf("Request ID: %u\n",reqid);
  for(z=0;z<cc;z++)
  {
    if(strcmp(cs[z].url,URL)==0)
    {
      
      if(debug)
      {  printf("ttl in cache: %ld \n",cs[z].ttl);
        printf("%ld\n",(cs[z].ttl-now));
        }
      if(cs[z].ttl>now)
      {
      if(debug)
        printf("Cache found at cache count: %d \n",z);
      for(a=0;a<cs[z].packetsiz;a++)
      {
        response[a]=cs[z].response_cache[a];
      }
      chngtime(response,(cs[z].ttl-now));
      setQueryID(response,reqid);
      response_length=cs[z].packetsiz;
      break;
    }
    }

  }
return response_length;
  

}



int resolve_name(int sock, uint8_t * request, int packet_size, uint8_t * response, struct sockaddr_storage * nameservers, int nameserver_count)
{
  //Assume that we're getting no more than 20 NS responses
  char recd_ns_name[20][255];
  struct sockaddr_storage recd_ns_ips[20];
  int recd_ns_count = 0;
  int recd_ip_count = 0; // additional records
  int response_size = 0;
  struct timeval timeout1;
  uint32_t ttl=0;
  // if an entry in recd_ns_ips is 0.0.0.0, we treat it as unassigned
  int a_cnt=0;
  new_str a_srvers[255],not_chosen_ns[255];
  memset(recd_ns_ips,0,sizeof(recd_ns_ips));

  memset(recd_ns_name,0,20*255);
  int retries = 5;
  
  if(debug)
    printf("resolve name called with packet size %d\n",packet_size);
if(nameserver_count!=0)
{
  int chosen = random()%nameserver_count;
  struct sockaddr_storage * chosen_ns = &nameservers[chosen];
  
  if(debug)
  {
    printf("\nAsking for record using server %d out of %d\n",chosen, nameserver_count);
  }

  /* using sockaddr to actually send a packet, so make sure the 
   * port is set
   */
  if(debug)
    printf("ss family: %d\n",chosen_ns->ss_family);
  if(chosen_ns->ss_family == AF_INET)
    ((struct sockaddr_in *)chosen_ns)->sin_port = htons(53);
  else if(chosen_ns->ss_family==AF_INET6)
    ((struct sockaddr_in6 *)chosen_ns)->sin6_port = htons(53);
  else
  {
    // this can happen during recursion if a NS w/o a glue record
    // doesn't resolve properly
    if (debug)
      printf("ss_family not set\n");
  }
 


//parse the request


  int send_count = sendto(sock, request, packet_size, 0, 
      (struct sockaddr *)chosen_ns, sizeof(struct sockaddr_in6));
  if(send_count<0){
    perror("Send failed");
    exit(1);
  }


  timeout1.tv_sec = 5;
  timeout1.tv_usec = 0;
  setsockopt(sock,SOL_SOCKET,SO_RCVTIMEO,&timeout1,sizeof(timeout1));
  // await the response - not calling recvfrom, don't care who is responding
  response_size = recv(sock, response, UDP_RECV_SIZE, 0);
  
  // discard anything that comes in as a query instead of a response
  if (((response_size > 0) && ((ntohs(((struct dns_hdr *)response)->flags) & 0x8000) == 0))||response_size<=0)
  {
    if(debug){
      printf("flags: 0x%x\n",ntohs(((struct dns_hdr *)response)->flags) & 0x8000);
      printf("received a query while expecting a response\n");
    }
  /*}
  else if(response_size<0)
  {*/
    perror("Timed out");
    
    if(counter_timer==0)
    {
      int a;
  
      for(a=0;a<nameserver_count;a++)
      {
        if(debug)
          printf("Copying %d of  nameservers in to %d of backup_nameserver_ns\n",a,a);
        backup_nameserver_ns[a]=nameservers[a];
      }
        backup_chosen=chosen; 
        backup_nameserver_count=nameserver_count; 
        if(debug)
         printf("Tries:  %d\n",counter_timer+2);
         counter_timer++;  
        return resolve_name(sock,request,packet_size,response,chosen_ns,1);
       
    }

    else if(counter_timer==1)
    {
      if(debug)
      printf("Tries:  %d\n",counter_timer+2);
      counter_timer++; 
      return resolve_name(sock,request,packet_size,response,chosen_ns,1);
       
    }
    else
    {
      int a,b=0;
      if(debug){
      printf("backup_chosen   %d\n", backup_chosen);
      printf("backup_nameserver_count %d\n",backup_nameserver_count );
      }
      for(a=0;a<backup_nameserver_count;a++)
      {
        if(a!=backup_chosen)
        {
    if(debug)        
        printf("Copying %d of  nameservers in to %d of not_chosen_ns\n",a,b );
        not_chosen_ns[b]=backup_nameserver_ns[a];
        b++;
      }
    }

      counter_timer=0;
      if(debug)
      printf("Size of new array %d\n",b);
      //printf("now 1st element: %s\n", not_chosen_ns[0]);
      return resolve_name(sock,request,packet_size,response,not_chosen_ns,b);
    }
    
  }
  if(debug) 
  printf("response size: %d\n",response_size);

  // parse the response to get our answer
  struct dns_hdr * header = (struct dns_hdr *) response;
  uint8_t * answer_ptr = response + sizeof(struct dns_hdr);

  // now answer_ptr points at the first question.
  int question_count = ntohs(header->q_count);
  int answer_count = ntohs(header->a_count);
  int auth_count = ntohs(header->auth_count);
  int other_count = ntohs(header->other_count);
  uint16_t id=header->id;
  
  // skip questions
  int q = 0;
  for(q=0; q<question_count; q++){
    char string_name[255];
    memset(string_name,0,255);
    int size=from_dns_style(response, answer_ptr,string_name);
    answer_ptr+=size;
    answer_ptr+=4;
  }

  if(debug)
    printf("Got %d+%d+%d=%d resource records total.\n",answer_count,auth_count,other_count,answer_count+auth_count+other_count);
  if(answer_count+auth_count+other_count>50){
    printf("ERROR: got a corrupt packet\n");
    return -1;
  }

  /*
   * iterate through answer, authoritative, and additional records
   */
   uint8_t cn_req[100];
   memset(&a_srvers,0,255);
  int a =0;
  for(a=0; a<answer_count+auth_count+other_count;a++)
  {
    // first the name this answer is referring to
    char string_name[255];
    int dnsnamelen=from_dns_style(response,answer_ptr,string_name);
    answer_ptr += dnsnamelen;

    // then fixed part of the RR record
    struct dns_rr* rr = (struct dns_rr*)answer_ptr;
    answer_ptr+=sizeof(struct dns_rr);
   
    
    //A record
    if(htons(rr->type)==RECTYPE_A)
    {
       ttl=ntohl(rr->ttl);
      if(debug)
        printf("The name %s resolves to IP addr: %s\n",
            string_name,
            inet_ntoa(*((struct in_addr *)answer_ptr)));
            ss_pton(inet_ntoa(*((struct in_addr *)answer_ptr)),&a_srvers[a_cnt]);
            a_cnt++;
    }
    //NS record
    else if(htons(rr->type)==RECTYPE_NS) 
    {
      from_dns_style(response,answer_ptr,recd_ns_name[recd_ns_count]);
      if(debug)
        printf("The name %s can be resolved by NS: %s\n",
            string_name, recd_ns_name[recd_ns_count]);
      recd_ns_count++;
    }
    //CNAME record
    else if(htons(rr->type)==RECTYPE_CNAME)
    {
       ttl=ntohl(rr->ttl);
      char ns_string[255];
      int ns_len=from_dns_style(response,answer_ptr,ns_string);
      if(debug)
        printf("The name %s is also known as %s.\n",        
            string_name, ns_string);
      
    }
    // SOA record
    else if(htons(rr->type)==RECTYPE_SOA)
    {
      char printbuf[INET6_ADDRSTRLEN];  
      if(debug) 
     
        printf("Ignoring SOA record\n");
      
     
    }
    
    // AAAA record
    else if(htons(rr->type)==RECTYPE_AAAA)  
    {
       ttl=ntohl(rr->ttl);
        char printbuf[INET6_ADDRSTRLEN];  
      if(debug)
    printf("The name %s resolves to IP addr: %s\n",
            string_name,
            inet_ntop(AF_INET6, answer_ptr, printbuf,INET6_ADDRSTRLEN));
        ss_pton(inet_ntop(AF_INET6, answer_ptr, printbuf,INET6_ADDRSTRLEN),&a_srvers[a_cnt]);
            a_cnt++;
      
    }
    else
    {
      if(debug)
        printf("got unknown record type %hu\n", htons(rr->type));
    }
    answer_ptr+=htons(rr->datalen);
  }
  
  if(other_count<1 && auth_count !=0)
  {
    uint8_t gluedServer[100];
    int x=0;
    for(x=0;x<recd_ns_count;x++)
    {
      int gluedLength=construct_query(gluedServer,100,recd_ns_name[x],1);
      if(debug)
        printf("Calling from Glued\n");
      setQueryID(gluedServer,header->id);
      return resolve_name(sock,gluedServer,gluedLength,response,root_servers,root_server_count);
    }
  }
  else if(answer_count<=0)
  {
    if(debug)
  printf("Calling from Else\n");
    return resolve_name(sock,request,packet_size,response,a_srvers,a_cnt);
  }

  if (answer_count >0)
  {
  int a=0;
  time_t currenttime;
  getRequestURL(request);
  currenttime=time(NULL);
  strcpy(cs[cc].url,URL);
  cs[cc].ttl=currenttime+(int)ttl;
  if(debug)
    printf("TTL in cache %ld\n", cs[cc].ttl);
  cs[cc].packetsiz=response_size;
  for(a=0;a<response_size;a++)
    cs[cc].response_cache[a]=response[a];
  if(debug)
    printf("Added to cache at index: %d\n",cc );
cc++;
}
}
  return response_size;

}

int main(int argc, char ** argv){
  int port_num=53;
  int sockfd;
  struct sockaddr_in6 server_address;
  struct dns_hdr * header=NULL;
  struct dns_hdr * req=NULL;
  char * question_domain=NULL;
  char client_ip[INET6_ADDRSTRLEN];
  char *optString = "dp";
  struct timeval timeout;
  
  int opt = getopt(argc, argv, optString);

  while( opt != -1){
    switch(opt) {
      case 'd':
        debug = 1;
        printf("Debug mode\n");
        break;
      case 'p':
        port_num=atoi(argv[optind]);
        break;
      case '?':
        usage();
        break;
    }
    opt = getopt(argc, argv, optString);
  }

  read_server_file();

  //Create socket as DNS Server
  printf("Creating socket on port: %d\n", port_num);
  sockfd=socket(AF_INET6, SOCK_DGRAM, 0);
  if(sockfd<0){
    perror("Unable to screate socket");
    return -1;
  }
  timeout.tv_sec = 8;
  timeout.tv_usec = 0;
  setsockopt(sockfd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));


  memset(&server_address, 0, sizeof(server_address));
  server_address.sin6_family=AF_INET6;
  server_address.sin6_addr = in6addr_any;
  server_address.sin6_port=htons(port_num);
  if(bind(sockfd, (struct sockaddr *)&server_address, sizeof(server_address))<0){
    perror("Uable to bind");
    return -1;
  }
  if (debug)
    printf("Bind successful\n");
  
  socklen_t addrlen = sizeof(struct sockaddr_in6);
  struct sockaddr_in6 client_address;
  uint8_t request[UDP_RECV_SIZE];
  uint8_t response[UDP_RECV_SIZE];
  int packet_size;
  if(debug)
    printf("Waiting for query...\n");

  while(1){
    if((packet_size = recvfrom(sockfd, request, UDP_RECV_SIZE, 0, (struct sockaddr *)&client_address, &addrlen))<0){
      perror("recvfrom error");
      printf("timed out... %d\n",packet_size);
      continue;
    }
    if(debug)
      printf("received request of size %d\n",packet_size);
    
    if(packet_size<(int)(sizeof(struct dns_hdr)+sizeof(struct dns_query_section))){
      perror("Receive invalid DNS request");
      continue;
    }
    counter_timer=0;
    header = (struct dns_hdr *)response;
    struct dns_hdr * reqq=(struct dns_hdr *)request;
    reqq->flags=htons(0x0000);


    int packetsiz=0;
    packetsiz=checkCache(request,response);
    if(packetsiz==0)
    {
     if(debug)
   printf("Calling resolve\n");
      packet_size = resolve_name(sockfd, request, packet_size, response, root_servers, root_server_count);
    }
    else
    {
      packet_size=packetsiz;
    }
    if(debug)
    printf("packet size recieved%d\n",packet_size );
    if (packet_size <= 0)
    {
      perror("failed to receive any answer (unexpected!)");
      continue;
    }
    if(debug)
      printf("outgoing packet size: %d\n",packet_size);

    //send the response to client
    req= (struct dns_hdr *)response;
   
    int sent_count = sendto(sockfd,response, packet_size, 0, (struct sockaddr*)&client_address, addrlen);
    if(debug)
      printf("Waiting for query...\n");

  }

  return 0;
}


