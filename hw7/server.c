/****** server program*******/

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

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

struct PlayerInfo
{
    char player_deck[27], buffer_player[2],result[2];
    int bytescount;
};

struct PlayerInfo p1,p2;
char cards_deck[52];
 int sck1,sck2,sck;
 int cb, msg, n1, n2,flagss=1;



void assigncard()
{
    if(flagss!=0)
    { 
    int k=0;
    for (k = 0; k < 52; k++)
    {      
        int a = rand() % 52;   
        int tmp = (int)cards_deck[a];
        cards_deck[a] = cards_deck[k];
        cards_deck[k] = tmp;       
    }
    k=0;
    int i=1;
    for(k=0;k<26;k++)
    {
        p1.player_deck[i]=cards_deck[k];
        i++;
    }
    int j=1;
    memset(&p1.player_deck,27,0);
    memset(&p2.player_deck,27,0);
    for(j=1;j<=26;j++)
    {
        p2.player_deck[j]=cards_deck[k];
        k++;
    }

      
    p1.player_deck[0]=1;
    p2.player_deck[0]=1;
    }
}

int checkcard(int l,int cardvalue)
{
    
    int calcard=cardvalue%13;
    if(cardvalue<=14)
    {
        if(calcard==12)
        {
            printf("Player %d plays : ACE of CLUBS\n",l);
        }
        else if(calcard==11)
        {
            printf("Player %d plays : KING of SPADES\n",l);
        }
        else if(calcard==9)
        {
            printf("Player %d plays : JACK of SPADES\n",l);
        }
        else if(calcard==10)
        {
            printf("Player %d plays : QUEEN of SPADES\n",l);
        }
        else
        {
            printf("Player %d plays : %d of SPADES\n",l,calcard);
        }
    } 
    else if(cardvalue>14 && cardvalue <=28)
    {
        if(calcard==12)
        {
            printf("Player %d plays : ACE of HEARTS\n",l);
        }
        else if(calcard==11)
        {
            printf("Player %d plays : KING of HEARTS\n",l);
        }
        else if(calcard==9)
        {
            printf("Player %d plays : JACK of HEARTS\n",l);
        }
        else if(calcard==10)
        {
            printf("Player %d plays : QUEEN of HEARTS\n",l);
        }
        else
        {
            printf("Player %d plays : %d of HEARTS\n",l,calcard);
        }
    }
    else if(cardvalue>28 && cardvalue<=41)
    {
        if(calcard==12)
        {
            printf("Player %d plays : ACE of DIAMOND\n",l);
        }
        else if(calcard==11)
        {
            printf("Player %d plays : KING of DIAMOND\n",l);
        }
        else if(calcard==9)
        {
            printf("Player %d plays : JACK of DIAMOND\n",l);
        }
        else if(calcard==10)
        {
            printf("Player %d plays : QUEEN of DIAMOND\n",l);
        }
        else
        {
            printf("Player %d plays : %d of DIAMOND\n",l,calcard);
        }
    }
    else
    {
         if(calcard==12)
        {
            printf("Player %d plays : ACE of SPADES\n",l);
        }
        else if(calcard==11)
        {
            printf("Player %d plays : KING of SPADES\n",l);
        }
        else if(calcard==9)
        {
            printf("Player %d plays : JACK of SPADES\n",l);
        }
        else if(calcard==10)
        {
            printf("Player %d plays : QUEEN of SPADES\n",l);
        }
        else
        {
            printf("Player %d plays : %d of SPADES\n",l,calcard);
        }
    }
    return 1;
}

void calculatewin()
{
    int player1=p1.buffer_player[1];
    int player2=p2.buffer_player[1];
    int player1cal=(player1)%12;
    int player2cal=(player2)%12;

    p1.result[0]=3;
    p2.result[0]=3;
    int a=checkcard(1,player1);
    a=checkcard(2,player2);
    if(player2cal==player1cal)
    {
        printf("DRAW\n");
        p1.result[1]=2;
        p2.result[1]=2;
    }
    else if(player2cal>player1cal)
    {   
        printf("Player 2 wins\n");
        p2.result[1]=0;
        p1.result[1]=1;
    }
    else
    {
        printf("Player 1 wins\n");
        p2.result[1]=1;
        p1.result[1]=0;
    }
}

int checkcheating()
{
    int cheat=0;
    int i=0,j=0;


    for(i=1;i<=26;i++)
    {
        if(p2.buffer_player[1]==p2.player_deck[i])
         { 
            cheat=1;
        }
    }
    printf("\n\n");
    for(j=1;j<=26;j++)
    {
        if(p1.buffer_player[1]==p1.player_deck[j])
        {
            cheat=1;
        }
    }
    return cheat;
}


int main(int argc, char *argv[])
{
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr;
    socklen_t sin_size;
    int var, yes=1;
    int game_counter=1;
    char s[INET6_ADDRSTRLEN];

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET6;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if ((var = getaddrinfo(NULL, argv[1], &hints, &servinfo)) != 0) {
        
        printf("error in connection\n");
        exit(1);;
    }

    for(p = servinfo; p != NULL; p = p->ai_next) 
        {
        sck = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol);
        if (sck == -1) {
            perror("server: socket");
            continue;
        }

        if (setsockopt(sck, SOL_SOCKET, SO_REUSEADDR, &yes,
                sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }

        cb = bind(sck, p->ai_addr, p->ai_addrlen);
        if (cb == -1) {
            close(sck);
            perror("server: bind");
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo); 

    if (p == NULL)  {
        printf("server: failed to bind\n");
        exit(1);
    }

    if (listen(sck, 10) == -1) {
        perror("listen");
        exit(1);
    }


    printf("server: waiting for connections...\n");
    sin_size = sizeof their_addr;

    int a=0,i=0;
    for(a=0;a<=51;a++)
    {
        cards_deck[i]=a;
        i++;
    }
    sck1= accept(sck, (struct sockaddr *)&their_addr, &sin_size);
    sck2= accept(sck, (struct sockaddr *)&their_addr, &sin_size);
    int flag=0,fl=0;
RECIEVE:
    memset(&p1.buffer_player,2,0);
    memset(&p2.buffer_player,2,0);
    n1=recv(sck1, p1.buffer_player, 2,0);
    n2=recv(sck2, p2.buffer_player, 2,0);
    p1.bytescount=n1;
    p2.bytescount=n2;
PLAY:  
    if(game_counter>26)
    {
        printf("GAME OVER...closing connection\n");
        close(sck);
        exit(0);
    }  
    if(p1.bytescount==2 && p2.bytescount==2)
    {

        assigncard();        
        if(p1.buffer_player[0]==0 && p1.buffer_player[1]==0)
        {

            if(flag==1)
             {
                close(sck1);
                exit(1);
             }
             else
             {
                fl=1;
                printf("sending player 1 deck\n");

                int n = send(sck1, p1.player_deck, 27, 0);
                flagss=0;
             }

        }
        if(p2.buffer_player[0]==0 && p2.buffer_player[1]==0)
        {

            if(flag==1)
             {
                close(sck2);
                exit(1);
             }
             else
             {
                fl=1;
                printf("sending player 2 deck\n");
                int n = send(sck2, p2.player_deck, 27, 0);
                flagss=0;
            }
        }
        if(p2.buffer_player[0]==2 && p1.buffer_player[0]==2)
        {   
            if(checkcheating()!=0)
            {  
                flag=1;
                fl=1;
                calculatewin();

                int n3= send(sck1,p1.result,2,0);
                int n4= send(sck2,p2.result,2,0);
                memset(&p1.result,2,0);
                memset(&p2.result,2,0);
                
            }
            else
            {
                printf("Found cheating\n");
                exit(1);
            }
           game_counter++; 
        }
        if(fl==0)
        {
            printf("1st player1 : %d   player2: %d \n", p1.buffer_player[0],p2.buffer_player[0]);
            printf("Invalid payload data recieved...closing connection but 2bytes\n");
            close(sck);
            exit(1);
        }
        goto RECIEVE;
    }
    else
    {
        char buf[1];
        if((p2.buffer_player[0]!=2) && (p1.buffer_player[0]!=2) && (p1.buffer_player[0]!=0) && (p2.buffer_player[0]!=0))
        {
             printf("Invalid payload data recieved...closing connection\n");
            close(sck);
            exit(1);   
        }
        if(p1.bytescount==1)
        {
            if(p1.buffer_player[0]!=0 && p1.buffer_player[0]!=2)
                exit(1);
            else
            {
                int r= recv(sck1,buf,1,0);
                if(r>0)
                {
                    fl=1;
                    p1.buffer_player[1]=buf[0];
                    p1.bytescount++;
                }
            }
        }
        memset(&buf,1,0);
        if(p2.bytescount==1)
        {
            if(p2.buffer_player[0]!=0 && p2.buffer_player[0]!=2)
                exit(1);
            else{
            int r=recv(sck2,buf,1,0);
            if(r>0)
            {
                p1.buffer_player[1]=buf[0];
                p2.bytescount++;
            }}
        }
        goto PLAY;
    }
return 0;
}






