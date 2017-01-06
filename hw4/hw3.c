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

static int debug=0, nameserver_flag=0;
in_addr_t root_servers[255];
int root_server_count=0;

void usage() {
	printf("Usage: hw2 [-d] -n nameserver -i domain/ip_address\n\t-d: debug\n");
	exit(1);
}

// fills in the root_servers array from the file "root-servers.txt"
void read_root_file() {
	root_server_count=0;
	char addr[25];

	FILE *f = fopen("root-servers.txt","r");
	while(fscanf(f," %s ",addr) > 0) 
		root_servers[root_server_count++]=inet_addr(addr);
}

/* constructs a DNS query message for the provided hostname */
int construct_query(uint8_t* query, int max_query, char* hostname) {
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
	hdr->id = htons(query_id);
	// set header flags to request recursive query
	hdr->flags = htons(0x0100);	
	// 1 question, no answers or other records
	hdr->q_count=htons(1);

	// add the name
	int query_len = sizeof(struct dns_hdr); 
	int name_len=to_dns_style(hostname,query+query_len);
	query_len += name_len; 
	
	// now the query type: A or PTR. 
	uint16_t *type = (uint16_t*)(query+query_len);
	if(rev_addr!=INADDR_NONE)
		*type = htons(12);
	else
		*type = htons(1);
	query_len+=2;

	// finally the class: INET
	uint16_t *class = (uint16_t*)(query+query_len);
	*class = htons(1);
	query_len += 2;
 
	return query_len;	
}

in_addr_t resolve_name(int sock, in_addr_t* nameservers, int nameserver_count, char* name, int persist)
{
	uint8_t answerbuf[1500];
	int rec_count = 0;
	// figure we're getting no more than 20 NS responses
	char recd_ns_names[20][255]; 	
	in_addr_t recd_ns_ips[20];	
	int recd_ns_count = 0;

	// if an entry in recd_ns_ips is 0.0.0.0, we treat it as unassigned
	memset(recd_ns_ips,0,sizeof(recd_ns_ips));

	/* error recovery procedure. if we don't get a response, try again.
		 this only works for broken root servers, as required by hw3 spec. */
 	do {
		// construct the query message. Need a new query every time to avoid duplicating xids.
		uint8_t query[1500];
		int query_len=construct_query(query,1500,name); 

		in_addr_t chosen_ns = nameservers[random() % nameserver_count];
		
		if(debug)
			printf("\nResolving %s using server %s out of %d\n",name, inet_ntoa(*(struct in_addr*)&chosen_ns),nameserver_count);

		struct sockaddr_in addr; 	// internet socket address data structure
		addr.sin_family = AF_INET;
		addr.sin_port = htons(53); // port 53 for DNS
		addr.sin_addr.s_addr = chosen_ns; // destination address (any local for now)	

		int send_count = sendto(sock, query, query_len, 0,
														(struct sockaddr*)&addr,sizeof(addr));		
		if(send_count<0) { perror("Send failed");	exit(1); }	
		
		// await the response 
		rec_count = recv(sock,answerbuf,1500,0);
	} while(persist && rec_count <= 0);
	
	// parse the response to get our answer
	struct dns_hdr *ans_hdr=(struct dns_hdr*)answerbuf;
	uint8_t *answer_ptr = answerbuf + sizeof(struct dns_hdr);
	
	// now answer_ptr points at the first question. 
	int question_count = ntohs(ans_hdr->q_count);
	int answer_count = ntohs(ans_hdr->a_count);
	int auth_count = ntohs(ans_hdr->auth_count);
	int other_count = ntohs(ans_hdr->other_count);

	// skip past all questions
	int q;
	for(q=0;q<question_count;q++) {
		char string_name[255];
		memset(string_name,0,255);
		int size=from_dns_style(answerbuf,answer_ptr,string_name);
		answer_ptr+=size;
		answer_ptr+=4; // 1 for the null terminator, 2 for type, 2 for class
	}

	int a;

	// last_auth will contain the last authoritative name server after parsing a message
	char last_auth_name[255]; 
	last_auth_name[0]=0; // set first byte to zero to indicate empty string

	if(debug) 
		printf("Got %d+%d+%d=%d resource records total.\n",answer_count,auth_count,other_count,answer_count+auth_count+other_count);
	if(answer_count+auth_count+other_count>50) {
		printf("ERROR: got a corrupt packet. What's going on?\n");
		return 0;
	}

	// now answer_ptr points at the first answer. loop through
	// all answers in all sections
	
	for(a=0;a<answer_count+auth_count+other_count;a++) {


		// first the name this answer is referring to 
		char string_name[255];
		int dnsnamelen=from_dns_style(answerbuf,answer_ptr,string_name);
		answer_ptr += dnsnamelen;

		// then fixed part of the RR record
		struct dns_rr* rr = (struct dns_rr*)answer_ptr;
		answer_ptr+=sizeof(struct dns_rr);


		if(htons(rr->type)==RECTYPE_A) {
			if(debug)
				printf("The name %s resolves to IP addr: %s\n",
						 string_name,
						 inet_ntoa(*((struct in_addr *)answer_ptr)));
			// if it's in the answer section, we found our answer
			if(a<answer_count) 
				return *((in_addr_t*)answer_ptr);

			// otherwise, we probably got pointed at another name server 
			else if(a>=answer_count+auth_count) {
				// find a matching name, and store the address for later
				for(int i=0;i<recd_ns_count;i++) 
					if(strcmp(string_name,recd_ns_names[i])) 
						recd_ns_ips[i]=*((in_addr_t*)answer_ptr);
			}
		}

		// NS record
		else if(htons(rr->type)==RECTYPE_NS) {
			int ns_len=from_dns_style(answerbuf,answer_ptr,recd_ns_names[recd_ns_count]);			
			if(debug) printf("The name %s can be resolved by NS: %s\n",
											 string_name, recd_ns_names[recd_ns_count]);					
			recd_ns_count++;
		}
		// CNAME record
		else if(htons(rr->type)==RECTYPE_CNAME) {
			char ns_string[255];
			int ns_len=from_dns_style(answerbuf,answer_ptr,ns_string);
			if(debug)
				printf("The name %s is also known as %s.\n",
						 string_name, ns_string);							

			// if it's in the answer section, this means we need to look up a new name,
			// starting all over again from the root
			if(a<answer_count)
				return resolve_name(sock,root_servers,root_server_count,ns_string,persist);	
		}
		// PTR record
		// this is a very non-functional solution: we return 0 as if nothing was found, but 
		// print out the result on the console! A better solution would accept a void* as argument,
		// and write the PTR result there. 
		else if(htons(rr->type)==RECTYPE_PTR) {
			char ns_string[255];
			int ns_len=from_dns_style(answerbuf,answer_ptr,ns_string);
			printf("%s resolves to %s\n",
						 name, ns_string);								
			return 0;
		}
		// SOA record
		else if(htons(rr->type)==RECTYPE_SOA) {
			if(debug)
				printf("Ignoring SOA record\n");
		}
		// AAAA record
		else if(htons(rr->type)==RECTYPE_AAAA) {
			if(debug)
				printf("Ignoring IPv6 record\n");
		}
		else {
			if(debug) 
				printf("got unknown record type %hu\n",htons(rr->type));
 		}
		answer_ptr+=htons(rr->datalen);
	}
	
	// if we dropped through all the way to here, that means we either got
	// nothing, or perhaps we got another name server to ask.
	// if we got a name server, go fetch any missing IP addresses, and try again!

	if(recd_ns_count>0) {

		// resolve the IP's of any name servers we don't have the address for
		for(int i=0;i<recd_ns_count;i++) 
			if(recd_ns_ips[i]==0) {
				if(debug) printf("No A record for server %s, asking root servers.\n",recd_ns_names[i]);
				recd_ns_ips[i] = resolve_name(sock,root_servers,root_server_count,recd_ns_names[i],0);
			}

		// this gets the ip of our host
		return resolve_name(sock,recd_ns_ips,recd_ns_count,name,persist);
	}
	
	return 0;
}
	
int main(int argc, char** argv)
{
	if(argc<2) usage();
	
	char *hostname;
	char *nameserver;
	
	char *optString = "dp";
 	int opt = getopt( argc, argv, optString );
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
	read_root_file();

	int sock = socket(AF_INET, SOCK_DGRAM, 0);
	if(sock < 0) {
		perror("Creating socket failed: ");
		exit(1);
	}
	
	struct timeval timeout;
	timeout.tv_sec = 1;
	timeout.tv_usec = 0;
  	setsockopt(sock,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));
	
	in_addr_t result;
	if(nameserver_flag) { //name server is supplied in command line
		in_addr_t ns_addr=inet_addr(nameserver);
		result=resolve_name(sock,&ns_addr,1,hostname,1);
	}
	else { // use root servers
		result=resolve_name(sock,root_servers,root_server_count,hostname,1);
	}
	
	// only print if the 'hostname' isn't actually an IP address
	in_addr_t rev_addr=inet_addr(hostname);
	if(rev_addr==INADDR_NONE) {
		if(!result) printf("Host %s not found.\n",hostname);
		else {
			printf("%s resolves to %s\n",
						 hostname,
						 inet_ntoa(*(struct in_addr*)&result));
		}
	}
	
	shutdown(sock,SHUT_RDWR);
	close(sock);
}