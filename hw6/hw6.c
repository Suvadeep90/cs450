#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include "hw6.h"

int sequence_number;
int expected_packet;
unsigned int RTT;
unsigned DEVIATION;
struct timeval timeout1;
int lengthpacket;
int timeval_to_msec(struct timeval *t) { 
  return t->tv_sec*1000+t->tv_usec/1000;
}

void msec_to_timeval(int millis, struct timeval *out_timeval) {
  out_timeval->tv_sec = millis/1000;
  out_timeval->tv_usec = (millis%1000)*1000;
}

int current_msec() {
  struct timeval t;
  gettimeofday(&t,0);
  return timeval_to_msec(&t);
}

int rel_connect(int socket,struct sockaddr_in *toaddr,int addrsize) {
     connect(socket,(struct sockaddr*)toaddr,addrsize);
}

int rel_rtt(int socket) {
     return RTT;
}

void rel_send(int sock, void *buf, int len)
{
  // make the packet = header + buf
 char packet[1400];
  char rec_packet[MAX_PACKET];
  struct hw6_hdr *hdr = (struct hw6_hdr*)packet;
  hdr->sequence_number = htonl(sequence_number);
  memcpy(hdr+1,buf,len); //hdr+1 is where the payload starts
  fprintf(stderr, "Sending packet: %d\n", sequence_number);
  if(len!=0){
  
  
  while(1)
  {
   send(sock, packet, sizeof(struct hw6_hdr)+len, 0);
  int sendTime=current_msec();
  struct sockaddr_in addr;
  unsigned int addrleng=sizeof(addr);
  
  setsockopt(sock,SOL_SOCKET,SO_RCVTIMEO,&timeout1,sizeof(timeout1));
  int rCount=recvfrom(sock,rec_packet,MAX_PACKET,0,(struct sockaddr*)&addr, &addrleng);
  struct hw6_hdr* rec1_hdr= (struct hw6_hdr*)rec_packet;
  fprintf(stderr, "%d\n",ntohl(rec1_hdr->ack_number));
  if(rCount>0)
  {
    if(ntohl(rec1_hdr->ack_number)==sequence_number+1)
    {
      int currentTime=current_msec();
    DEVIATION=0.75*DEVIATION + 0.25*(abs((currentTime-sendTime)-RTT));
    RTT=0.75 * RTT + 0.25 * (currentTime-sendTime);
    msec_to_timeval(RTT+4*DEVIATION,&timeout1);
    fprintf(stderr, "Recieved Ack %d\n   and RTT time : %d",ntohl(rec1_hdr->ack_number),RTT);
    sequence_number++;
    break;
    }
  }
    else
    {
        msec_to_timeval((2*timeval_to_msec(&timeout1)),&timeout1);
    }

}
}else 
{
send(sock, packet, sizeof(struct hw6_hdr)+len, 0);
}
}

int rel_socket(int domain, int type, int protocol) {
  sequence_number = 0;
  expected_packet=0;
  RTT=600;
  DEVIATION=30;
  
  timeout1.tv_usec=(RTT+4*DEVIATION)*1000;
  return socket(domain, type, protocol);
}

int rel_recv(int sock, void * buffer, size_t length) {
  char packet[MAX_PACKET];
  char ack_packet[MAX_PACKET];
  memset(&packet,0,sizeof(packet));
  struct hw6_hdr* hdr=(struct hw6_hdr*)packet;  
int recv_count;
  struct sockaddr_in fromaddr;
  unsigned int addrlen=sizeof(fromaddr);  
 if(length!=0){
 while(1)
 { 
  recv_count = recvfrom(sock, packet, MAX_PACKET, 0, (struct sockaddr*)&fromaddr, &addrlen);    

  struct hw6_hdr* ack_hdr=(struct hw6_hdr*)ack_packet;
  int seq=ntohl(hdr->sequence_number);
  fprintf(stderr, "Recieved Seq: %d   and expected Seq %d\n",seq,expected_packet );
  if(seq == expected_packet)
  {
   
    ack_hdr->ack_number = htonl(seq+1);
    expected_packet=ntohl(ack_hdr->ack_number);
    
    fprintf(stderr, "Recieved packet %d sending ack %d\n",ntohl(hdr->sequence_number), ntohl(ack_hdr->ack_number));
    //fprintf(stderr, "expected_packet %d\n\n",expected_packet ); 
    send(sock,ack_packet,sizeof(struct hw6_hdr),0);
    break;
    
  }
  else
  {
    
    ack_hdr->ack_number = htonl(expected_packet);
    fprintf(stderr, "Recieved incorrect packet %d re-sending last ack %d\n",ntohl(hdr->sequence_number), ntohl(ack_hdr->ack_number));
    fprintf(stderr, "expected_packet %d\n\n",expected_packet );
    send(sock,ack_packet,sizeof(struct hw6_hdr),0);

  }
}
  if(connect(sock, (struct sockaddr*)&fromaddr, addrlen)) {
    perror("couldn't connect socket");
  }

  fprintf(stderr, "Got packet %d\n", ntohl(hdr->sequence_number));

  memcpy(buffer, packet+sizeof(struct hw6_hdr), recv_count-sizeof(struct hw6_hdr));
}
else
 { 
fprintf(stderr, "Closing Reciever\n");
close(sock);

  }
  return recv_count - sizeof(struct hw6_hdr);
}

int rel_close(int sock) {
  
  rel_send(sock, 0, 0); // send an empty packet to signify end of file
  fprintf(stderr, "Sent EOF. Now closing the connection\n");
  
    
  close(sock);
}

