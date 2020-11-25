#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h> // for sleep()
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <stdbool.h>

#define PORT 8080
#define HOST "127.0.0.1"
#define SSDP "239.0.0.0"
#define MAX_CLIENTS 100

const int delay_secs = 1;
char message[100] = "127.0.0.1 8080";
char *welcomeMsg;
int sockid_udp, sockid_tcp;
struct sockaddr_in dest_addr;
char* client_username;
char* client_port;
char* client_ip;

struct clientInfo{
    char* username;
    char* contestentUsername;
    char* port;
    char* ip;
    bool valid;
    bool has_contestent;
    int id;
    int isWaiting;
};

void int2string(int num, char out[])
{
    int count=0;
    int temp;
    temp = num;
    if (num == 0)
    {
        out[0] = '0';
        out[1] = '\0';
        return;
    }
    while(temp > 0)
    {
        temp /= 10;
        count++;
    }
    int i = count-1;
    while(num > 0)
    {   
        out[i] = (char)((num % 10) + '0');
        num /= 10;
        i--;
    }
}

char* concatination(char* s1, char* s2)
{
    char* out = malloc(strlen(s1) + strlen(s2));
    strcpy(out, s1);
    strcat(out, " ");
    strcat(out, s2);
    strcat(out, "\0");
    return out;
}

void heartbeat() {
    int portsend = sendto(sockid_udp, message, strlen(message), 0, 
        (struct sockaddr*) &dest_addr, sizeof(dest_addr) );
    if (portsend < 0)
    {
        perror("sendto");
        exit(1);
    }
    signal(SIGALRM, heartbeat); // Install handler first,
    alarm(1);
}

int main(int argc, char *argv[])
{
    //constructing clients and initializing them
    struct clientInfo clients[MAX_CLIENTS];
    for(int i=0; i<MAX_CLIENTS; i++)
    {
        clients[i].valid = 0;
        clients[i].port = NULL;
        clients[i].ip = NULL;
        clients[i].id = -1;
        clients[i].has_contestent = 0;
        clients[i].isWaiting = 0;
    }

    // argument error
    if (argc != 9) {
        char* ArgError = "Command line args should be multicast group and port\n";
        write(1, ArgError, strlen(ArgError));
        exit(1);
    }


    char* group = SSDP; // e.g. 239.255.255.250 for SSDP
    char* port = argv[4]; // 0 if error, which is an invalid port

    // Port X for broadcasting
    sockid_udp = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockid_udp < 0) {
        perror("socket");
        exit(1);
    }

    // set up destination address
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_addr.s_addr = inet_addr(group);
    dest_addr.sin_port = htons(atoi(port));

    // Port N for listening
    sockid_tcp = socket(AF_INET, SOCK_STREAM, 0);
    if (sockid_tcp < 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    struct sockaddr_in receive_addr;
    memset(&receive_addr, 0, sizeof(receive_addr));
    receive_addr.sin_family = AF_INET;
    receive_addr.sin_addr.s_addr = inet_addr(HOST);     // 
    receive_addr.sin_port = htons(PORT);    // unsigned short


    int pid = fork();
    if(pid == 0)
    {
        char* heartbeatMsg = "Heartbeat Message: \n";
        write(1, heartbeatMsg, strlen(heartbeatMsg));
        heartbeat();
        while(1){}
    }
    else
    {
        int new_socket, client_socket[30], max_clients = 30, activity, i, valread, sd;
        int max_sd;
        char buffer[1025];  //data buffer of 1K
       
        //set of socket descriptors
        fd_set serverSel;

        for (i = 0; i < max_clients; i++)
            client_socket[i] = 0;

        int opt = 1;
        if( setsockopt(sockid_tcp, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0 )
        {
            perror("setsockopt");
            exit(EXIT_FAILURE);
        }

        //bind the socket to localhost port 8888
        if (bind(sockid_tcp, (struct sockaddr *)&receive_addr, sizeof(receive_addr))<0)
        {
            perror("bind failed");
            exit(EXIT_FAILURE);
        }

        if (listen(sockid_tcp, 20) < 0)        //////////////////////////////
        {
            perror("listen");
            exit(EXIT_FAILURE);
        }
        char* WaitForConn = "Waiting for connections ...\n";
        write(1, WaitForConn, strlen(WaitForConn));
        while (1)
        {
            FD_ZERO(&serverSel);
            FD_SET(sockid_tcp, &serverSel);
            max_sd = sockid_tcp;

            for ( i = 0 ; i < max_clients ; i++)
            {
                //socket descriptor
                sd = client_socket[i];
                //if valid socket descriptor then add to read list
                if(sd > 0)
                    FD_SET( sd , &serverSel);
                //highest file descriptor number, need it for the select function
                if(sd > max_sd)
                    max_sd = sd;
            }

            activity = select( max_sd + 1 , &serverSel , NULL , NULL , NULL);

            if( (activity < 0) && (errno!=EINTR) )
                perror("select error");
            //If something happened on the master socket ,
            //then its an incoming connection
            int addrLen = sizeof(receive_addr);
            if (FD_ISSET(sockid_tcp, &serverSel))
            {
                if ( (new_socket = accept(sockid_tcp, (struct sockaddr *)&receive_addr,
                    &addrLen)) < 0 )
                {
                    perror("accept");
                    exit(EXIT_FAILURE);
                }

                for(int i=0; i<MAX_CLIENTS; i++)
                    if(clients[i].valid == 0)
                    {
                        clients[i].id = new_socket;
                        clients[i].valid = 1;
                        break;
                    }
                //inform user of socket number - used in send and receive commands
                
                //send new connection greeting message
                welcomeMsg = "Server Accepted your request. Welcome!\n";
                if( send(new_socket, welcomeMsg, strlen(welcomeMsg), 0) != strlen(welcomeMsg) )
                    perror("send");
                char* WelcomeMessage = "Welcome message sent successfully\n";
                write(1, WelcomeMessage, strlen(WelcomeMessage));

                
                for (i = 0; i < max_clients; i++)
                {
                    if( client_socket[i] == 0 )
                    {
                        client_socket[i] = new_socket;
                        break;
                    }
                }
            }
            for (i = 0; i < max_clients; i++)
            {
                sd = client_socket[i];
                if (FD_ISSET( sd , &serverSel))
                {
                    //add new socket to array of sockets
                    if ((valread = read( sd , buffer, 1024)) <= 0)
                    // if( (valread = recv(sd, buffer, sizeof(buffer), 0)) < 0)
                    {
                    //Somebody disconnected , get his details and print
                        getpeername(sd, (struct sockaddr*)&receive_addr, 
                            (socklen_t *)sizeof(receive_addr));
                        char* disconnectMsg = "Host disconnected, ip %s, port %d \n";
                        write(1, disconnectMsg, strlen(disconnectMsg));
                        write(1, inet_ntoa(receive_addr.sin_addr), sizeof(inet_ntoa(receive_addr.sin_addr)));
                        write(1, ntohs(receive_addr.sin_port), sizeof(ntohs(receive_addr.sin_port)));

                        // printf("Host disconnected, ip %s, port %d \n", 
                        //     inet_ntoa(receive_addr.sin_addr), ntohs(receive_addr.sin_port));

                        //Close the socket and mark as 0 in list for reuse
                        for(int i=0; i<MAX_CLIENTS; i++)
                        {
                            
                            if( clients[i].id == sd ) 
                            {
                                clients[i].valid = 0;
                                clients[i].port = NULL;
                                clients[i].ip = NULL;
                                clients[i].id = -1;
                                clients[i].has_contestent = 0;
                                clients[i].isWaiting = 0;
                                break;
                            }
                        }
                        close( sd );
                        client_socket[i] = 0;
                    }
                    //Echo back the message that came in
                    else
                    {
                        // int finding_contestent_for;
                        buffer[valread] = '\0';
                        puts(buffer);

                    //set the string terminating NULL byte on the end of the data read
                        for (int i=0; i<MAX_CLIENTS; i++)
                        {
                            if( clients[i].id == sd )
                            {
                                char* temp;

                                // puts("message from client");
                                // puts(buffer);

                                temp = strtok(buffer, " ");
                                int tempSize = strlen(temp);
                                clients[i].username = malloc(tempSize);
                                for(int j=0; j<tempSize; j++)
                                    clients[i].username[j] = temp[j];

                                temp = strtok(NULL, " ");
                                tempSize = strlen(temp);
                                clients[i].port = malloc(tempSize);
                                for(int j=0; j<tempSize; j++)
                                    clients[i].port[j] = temp[j];
                                
                                temp = strtok(NULL, " ");
                                tempSize = strlen(temp);
                                clients[i].ip = malloc(tempSize);
                                for(int j=0; j<tempSize; j++)
                                    clients[i].ip[j] = temp[j];

                                temp = strtok(NULL, " ");
                                tempSize = strlen(temp);

                                clients[i].contestentUsername = malloc(tempSize);
                                if(strncmp(temp, "N", 1) != 0)
                                {
                                    for(int j=0; j<tempSize; j++)
                                        clients[i].contestentUsername[j] = temp[j];
                                    clients[i].isWaiting = 1;
                                }

                                clients[i].has_contestent = 0;

                                break;
                            }
                        }

                        int secondClient;
                        int checked = 0;
                        for (int i=0; i<MAX_CLIENTS; i++)
                            if(clients[i].id == sd)
                            {
                                secondClient = i;
                                break;
                            }

                        for(int i=0; i<MAX_CLIENTS; i++)
                        {   
                            if(clients[i].id == sd && clients[i].isWaiting == 1 && clients[i].valid == 1)
                            {
                                checked = -1;
                            }
                            else if( clients[i].valid ==1 && clients[i].isWaiting == 1 && clients[i].id != sd 
                                && clients[i].has_contestent == 0)
                            {
                                int count = 0;
                                for(int j = 0; j< sizeof(clients[i].contestentUsername); j++)
                                {

                                    if(((int)clients[i].contestentUsername[j]>=65 && (int)clients[i].contestentUsername[j]<=90) 
                                        || ((int)clients[i].contestentUsername[j]>=97 && (int)clients[i].contestentUsername[j]<=172))
                                        {
                                            count++;
                                            continue;
                                        }
                                    else
                                        break;
                                }

                                if(strncmp(clients[i].contestentUsername, clients[secondClient].username, 
                                    count) == 0)
                                {
                                    puts("second if");
                                    char* forSecondClientMsg = concatination(clients[i].ip, clients[i].port);
                                    close(clients[i].id);
                                
                                    if((send(clients[secondClient].id, forSecondClientMsg, strlen(forSecondClientMsg)
                                        ,0)) < 0)
                                    {
                                        perror("contestent sending error");
                                        return(1);
                                    }
                                    char* sendsucc = "Message Sent successfullyyyyy";
                                    write(1, sendsucc, sizeof(sendsucc));
                                    clients[secondClient].has_contestent = 1;

                                    clients[i].has_contestent = 1;


                                    for (int j = 0; j < max_clients; j++)
                                    {
                                        if(clients[i].id == client_socket[j])
                                            client_socket[j] = 0;
                                        if(clients[secondClient].id == client_socket[j])
                                            client_socket[j] = 0;
                                    }
                                    close(clients[secondClient].id);

                                    clients[i].valid = 0;
                                    clients[i].port = "\0";
                                    clients[i].ip = "\0";
                                    clients[i].id = -1;
                                    clients[i].has_contestent = 0;
                                    clients[i].isWaiting = 0;                                    
                                    checked = 1;
                                    break;
                                }
                            }
                        }

                        if(checked == 0)
                        {
                            for(int i=0; i<MAX_CLIENTS; i++)
                            {
                                if( (clients[i].valid == 1) && (clients[i].has_contestent == 0) 
                                    && (clients[i].id != sd) && (clients[i].id != -1) && (clients[i].isWaiting ==0))
                                {
                                    char* forSecondClientMsg;
                                    forSecondClientMsg = concatination(clients[i].ip, 
                                        clients[i].port);

                            
                                    close(clients[i].id);
                                    
                                    if((send(clients[secondClient].id, forSecondClientMsg, strlen(forSecondClientMsg)
                                        ,0)) < 0)
                                    {
                                        perror("contestent sending error");
                                        return(1);
                                    }

                                    char* sendsucc = "Message Sent successfullyyyyy";
                                    write(1, sendsucc, sizeof(sendsucc));

                                    clients[secondClient].has_contestent = 1;

                                    clients[i].has_contestent = 1;


                                    for (int j = 0; j < max_clients; j++)
                                    {
                                        if(clients[i].id == client_socket[j])
                                            client_socket[j] = 0;
                                        if(clients[secondClient].id == client_socket[j])
                                            client_socket[j] = 0;
                                    }
                                    close(clients[secondClient].id);

                                    clients[i].valid = 0;
                                    clients[i].port = "\0";
                                    clients[i].ip = "\0";
                                    clients[i].id = -1;
                                    clients[i].has_contestent = 0;
                                    clients[i].isWaiting = 0;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    
    int closeStatus = close(sockid_udp);
    closeStatus = close(sockid_tcp);
    exit(1);
}
