

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>



void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}
struct player
{
    int p;
    int gm;
    char card[1];
    int rslt;
    char b[10];
    int cnt;
};

int main(int argc, char *argv[])
{
    fd_set master;    
    fd_set read_fds;  
    int max;        
    char Decf[60];
    int listener;     
    int newfd;        
    struct sockaddr_storage remoteaddr; 
    socklen_t addrlen;
    char buf[256];    
    char send_data[256];
    int n=0;
    char remoteIP[INET6_ADDRSTRLEN];
    struct player user[200];
    int yes=1;        
    int i, j, r,a;
    struct addrinfo hints, *ai, *p;
    FD_ZERO(&master);    
    FD_ZERO(&read_fds);
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET6;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if ((r = getaddrinfo(NULL, argv[1], &hints, &ai)) != 0) {
        printf("Error in getting address\n");
        exit(1);
    }
    for(p = ai; p != NULL; p = p->ai_next) {
        listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (listener < 0) {
            continue;
        }
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
        if (bind(listener, p->ai_addr, p->ai_addrlen) < 0) {
            close(listener);
            continue;
        }
        break;
    }
    int f=0;
    for (i=0; i<4; i++)
    {
        for (j=0;j<13;j++)
        {
            Decf[f] = j+2;
            f=f+1;
        }
    }
    if (p == NULL) {
        printf("Server failed to bind\n");
        exit(1);
    }
    freeaddrinfo(ai); 
    if (listen(listener, 300) == -1) {
        printf("Error in listening\n");
        exit(1);
    }
    max = listener;
    FD_SET(listener, &master);
    int g=1;
    int usercnt=0;
    for(;;) {
       int s=0;
        read_fds = master;
        

        if (select(max+1, &read_fds, NULL, NULL, NULL) == -1) {
            printf("Error in select\n");
            exit(1);
        }
        
        for(i = 0; i <= max ; i++) {
            
            if (FD_ISSET(i, &read_fds))
            {
                
                if (i == listener)
                {
                    addrlen = sizeof remoteaddr;
                    newfd = accept(listener,
                    (struct sockaddr *)&remoteaddr,
                    &addrlen);
                    if (newfd == -1) {
                        printf("Cannot accept new connection\n");
                    }
                    else {
                        FD_SET(newfd, &master);
                        if (newfd > max) {
                            max = newfd;
                        }
                        printf("Welcome new player: %d\n",newfd);
                        user[i].cnt=0;
                        user[i].gm=0;
                        user[i].rslt=0;

                        usercnt++;
                        
                    }
                }
                else
                {
                    memset(&buf,0,sizeof(buf));

                    n = recv(i, buf, 256, 0);
                    
                    strcpy(user[i].b,buf);
                    if ( n<= 0)
                    {
                        if (n == 0) {
                            printf("Player %d left the game\n", i);
                            close(i);
                            FD_CLR(i, &master);
                            FD_CLR(i, &read_fds);
                            for(f=0;f<=max;f++)
                            {
                                if(user[i].gm== user[f].gm && i!=f && user[i].gm!=0 && user[f].gm!=f)
                                {
                                    FD_CLR(f,&master);
                                    FD_CLR(f,&read_fds);
                                    close(f);
                                    printf("Exiting the partner %d of the game \n",f);
                                }
                            }
                        }
                        else {
                            printf("Error\n");
                        }
                    }
                    else
                    {
                        user[i].p=i;
                        int cnt=0;
                        for (cnt = 0;cnt<n;cnt++)
                        {
                            user[i].b[user[i].cnt]=buf[cnt];
                        }
                        if(user[i].cnt>=2)
                        {
                            user[i].cnt=0;  
                            user[i].cnt=n;  
                            
                        }
                        else
                        {
                            user[i].cnt=user[i].cnt+n;
                        }
                   if(user[i].cnt==2)
                   {
                    
                        if(user[i].b[0]==0 && user[i].b[1]==0)
                        {
                            if(usercnt%2==1)
                            {
                                user[i].gm=g;
                                int o=26;
                                for(f=1;f<=27;f++)
                                {
                                    send_data[f]=Decf[o];
                                    o++;
                                }
                                send_data[0]=1;
                                send(i,send_data,27,0);
                            }
                            else
                            {
                                user[i].gm=g;
                                g++;
                                for (a = 0; a < 52; a++)
                                {
                                    int x = rand() % 52;
                                    int temp = (int)Decf[x];
                                    Decf[x] = Decf[a];
                                    Decf[a] = temp;
                                }
                                memset(&send_data, 0, sizeof send_data);
                                int o=0;
                                for(f=1;f<=27;f++)
                                {
                                    send_data[f]=Decf[o];
                                    o++;
                                }
                                send_data[0]=1;
                                send(i,send_data,27,0);
                            }
                        }
                        else if(user[i].b[0]==2)
                        {
                            user[i].card[0]=(int)user[i].b[1];
                            int f;
                            for (f=1;f<=max;f++)
                            {
                                if (user[f].gm == user[i].gm && user[i].card[0]!=0 && user[f].card[0]!=0 && user[i].p!=user[f].p)
                                {
                                    
                                    if (user[i].card[0]>user[f].card[0])
                                    {
                                        user[i].rslt = 1;
                                        user[f].rslt = 2;
                                      
                                    }
                                    else if (user[i].card[0]<user[f].card[0])
                                    {
                                        user[i].rslt = 2;
                                        user[f].rslt = 1;
                                        
                                    }
                                    else if (user[i].card[0]==user[f].card[0])
                                    {
                                       
                                        user[i].rslt = 3;
                                        user[f].rslt = 3;
                                    }
                                    user[i].card[0]=0;
                                    user[f].card[0]=0;
                                }
                            }
                            for (f = 0; f <=max; f++)
                            {
                                if (user[f].rslt == 1 || user[f].rslt == 2 || user[f].rslt == 3)
                                {
                                    send_data[0]=3;
                                    send_data[1]=user[f].rslt-1;
                                    send(f,send_data,2,0);
                                    user[f].rslt=0;
                                    memset(&user[i].b,0,2);
                                    memset(&user[f].b,0,2);
                                }
                            }

                        }
                    }
                    }
                }
            }
        }
    FD_ZERO(&read_fds);    
    }

    return 0;
}

