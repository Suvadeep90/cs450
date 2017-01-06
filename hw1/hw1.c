
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <err.h>
#include <math.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <stdarg.h>
#include <stdlib.h>
#define BSIZE 0x1000


static void get_page (int s, const char * host, const char * page,char spage[100])
{
    
    
    
    FILE *f;
    const char * format ="GET / HTTP/1.0\r\nHost:%s\r\n\r\n\r\n";
    int status;
    char *search;
    char *ptr;
    search = spage;
    char *path=spage;
    
    while(search != NULL)
    {
        ptr = search;
        search = strchr(path++, '/');
    }
    if (strlen(ptr) > 1)
    {
        ptr = ptr+1;
        f = fopen(ptr, "a");
    }
else
{
    f = fopen("index.html", "a");
}


    char msg[BSIZE];
    
    
    if (strlen(spage) > 1)
        sprintf(msg,"GET %s HTTP/1.0\r\n\r\n",spage);
    else
        {
            char *tempaddr = "GET / HTTP/1.0\n\n";
            strcpy(msg, tempaddr);
            
        }

   
    
    status = send (s, msg, strlen (msg), 0);
    


   
    
    char buf[BSIZE+10];
    int recv_count;
    int bytes;
    int size=0;
    char *sss;
    int indx;
    char *chars;
    int flag=1;
    do {
        recv_count = recv(s, buf, BSIZE, 0);
       if (flag) 
      {  
        if(recv_count<0) 
        { 
            break; 
        }
        
        sss = strstr(buf, "\r\n\r\n");
        if(sss!=NULL)
        {
        sss=sss+4;
        fwrite(sss,1,recv_count-(sss-buf),f);
        }
        flag=0;
        continue;
       }
      
       fwrite(buf, 1, recv_count, f);

    }while(recv_count!=0);

    
    //free (msg);
}










int main (int argc, char** argv)
{
    struct addrinfo hints, *res, *res0;
    int error;
    int s;
    char args[500],p[500],*c;
    char *url="/";
    char page[500];
    strcpy(args,argv[1]);

    c=args;
    
    
    do
    {
        if (strchr(url, *c))
       {
       
          
          sscanf(args, "http://%99[^/]%99[^\n]",p,page);
          
          break;
       }
       else
       {
            strcpy(p,args);
       }

       c++;
    }while(*c);
    
    
    
    



    const char * host = p;
    
    memset (&hints, 0, sizeof(hints));
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    error = getaddrinfo (host, "http", & hints, & res0);
    
    s = -1;
    for (res = res0; res; res = res->ai_next)
    {
        s = socket (res->ai_family, res->ai_socktype, res->ai_protocol);
        
        if (connect(s, res->ai_addr, res->ai_addrlen) < 0) {
            fprintf (stderr, "connect: %s\n", strerror (errno));
            close(s);
            exit (EXIT_FAILURE);
        }
        break;
    }
    if (s != -1) {
        get_page (s, host,host,page);
    }
    freeaddrinfo (res0);
    return 0;
}

