/******client code*******/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

#define PORT "3490"
#define BuffSize 256


char mydeck[27];
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}






int main(int argc, char *argv[])
{
	int sockfd, numbytes;  
    char buf[BuffSize];
    struct addrinfo hints, *servinfo, *p;
    int rv, n;
    char s[INET6_ADDRSTRLEN];
    int choice = 0;
    

    if (argc < 3)
    {
    	printf("error in input\n");
        exit(1);
    }

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET6;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(argv[1], argv[2], &hints, &servinfo)) != 0) 
    {
        printf("error getting server\n");
        exit(1);
    }
    for(p = servinfo; p != NULL; p = p->ai_next) 
    {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) 
        {
            perror("client: socket");
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) 
        {
            close(sockfd);
            perror("client: connect");
            continue;
        }

        break;
    }
    char frst[2];
    if (p == NULL)
    {
    	printf("error connecting to host\n");
        exit(1);
    }
    else
    {
    memset(&buf, 0, 255);
    frst[0]=0;
    frst[1]=0;
    

    n = send(sockfd, frst, 2, 0);
    if (n < 0){
    printf("error writing to socket\n");
    exit(1);
    }
    memset(&mydeck, 0, 27);
    char card[2];
    char result[2];
    
    int counter=0;
RECIEVECARD:
    n = recv(sockfd, mydeck, 27, 0);
    if(n<=27)
        goto RECIEVECARD;
    if (mydeck[0] == 1)
    {
        int i=0;    
        memset(&card, 0, 2);
        memset(&result, 0, 2);
        memset(&buf, 0, 255);
        card[0] = 2;
        for (i=1;i<=26;i++)
        {
            card[1]=mydeck[i];
            
            n=send(sockfd,card,2,0);
RECIEVERESULT:               
            n=recv(sockfd,result,2,0);
            if(n<=2)
                goto RECIEVERESULT;
            if (result[0] == 3)
            {           
                if (result[1] == 0)
                {    
                    printf("You win\n");
                    counter++;
                }
                else if (result[1] == 1)
                    printf("You lose\n");
                else if (result[1] == 2)
                    printf("Draw\n");
            }
            else if(result[0]!=3)
            {
                printf("Wrong message sent from server\n");
                exit(1);
            }
        }
        if(counter>=13)
            printf("****************GAME WON***************\n");
        else
            printf("****************GAME LOST***************\n");
    }
    else if(mydeck[0]!=1)
    {
        printf("Wrong control message from server\n");
        exit(1);
    }

    

    } 

	return 0;
}
