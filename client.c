//
// Simple listener.c program for UDP multicast
//
// Adapted from:
// http://ntrg.cs.tcd.ie/undergrad/4ba2/multicast/antony/example.html
//
// Changes:
// * Compiles for Windows as well as Linux
// * Takes the port and group on the command line
//

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <poll.h>
#include <signal.h>


#define MSGBUFSIZE 256
#define PORT 9999
#define SSDP "239.0.0.0"
#define SSDPII "239.1.1.1"
#define LOCALHOST "127.0.0.1"

char msgbuf [MSGBUFSIZE];
struct sockaddr_in sockid_PortY_dest_addr;
int sockid_portY = 100;
char* msg;



struct clientInfo{
    int sockid_portX;
    int sockid_portN;
    int sockid_portM_connector;
    int sockid_portT;
    int sockid_portM_accepter;

    char* ipX;
    int portX;

    char* ipN;
    char* portN;

    char* selfusername;
    char* selfipM;
    char selfportM[20];

    char* ipY;
    char* portY;

    char* contestentUsername;
    char* contestentPortM;
    char* contestentIpM;

    int contestentId;

    int connectOrAccept; // connect = 0; accept = 1;

    int hasContestent;
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

void infoBroadcast() {
    write(1, msg, strlen(msg));
    int portsend = sendto(sockid_portY, msg, strlen(msg), 0, 
        (struct sockaddr*) &sockid_PortY_dest_addr, sizeof(sockid_PortY_dest_addr) );

    if (portsend < 0)
    {
        perror("BroadCast sendto");
        exit(1);
    }
    signal(SIGALRM, infoBroadcast); // Install handler first,
    alarm(1);
}

int main(int argc, char *argv[])
{
    if (argc != 6) {
        char* ArgError = "Command line args should be multicast group and port\n";
        write(1, ArgError, strlen(ArgError));
        exit(1);
    }

    struct clientInfo client;
    client.ipX = SSDP; // e.g. 239.255.255.250 for SSDP
    client.portX = atoi(argv[2]); // 0 if error, which is an invalid port
    client.contestentId = 0;
    client.hasContestent = 0;

    // PortX Creation
    client.sockid_portX = socket(AF_INET, SOCK_DGRAM, 0);
    if (client.sockid_portX < 0) {
        perror("socketX");
        exit(1);
    }

    // allow multiple sockets to use the same PORT number
    u_int yes = 1;
    if (setsockopt(client.sockid_portX, SOL_SOCKET, SO_REUSEADDR, (char*) &yes, sizeof(yes)) < 0){
       perror("Reusing ADDR failed");
       exit(1);
    }

    // use setsockopt() to request that the kernel join a multicast group
    struct ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = inet_addr(client.ipX);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);

    //Option for Port X
    if(setsockopt(client.sockid_portX, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*) &mreq, sizeof(mreq))<0){
        perror("setsockoptX");
        exit(1);
    }


    //creation of Port N
    client.sockid_portN = socket(AF_INET, SOCK_STREAM, 0);    
    if (client.sockid_portN < 0)
    {
        perror("socketN");
        exit(1);
    }

    // Creation of Port M
    client.sockid_portM_accepter = socket(AF_INET, SOCK_STREAM, 0);    
    if (client.sockid_portM_accepter < 0)
    {
        perror("socketM");
        exit(1);
    }
    // Structs Of Sockets

    struct sockaddr_in portX_address;
    memset(&portX_address, 0, sizeof(portX_address));
    portX_address.sin_family = AF_INET;
    portX_address.sin_addr.s_addr = htonl(INADDR_ANY); // differs from sender
    portX_address.sin_port = htons(client.portX);  

    struct sockaddr_in portN_address;
    memset(&portN_address, 0, sizeof(portN_address));
    portN_address.sin_family = AF_INET;
    portN_address.sin_addr.s_addr = inet_addr(LOCALHOST); // differs from sender
    portN_address.sin_port = htons(client.portX);

    struct sockaddr_in portM_address;
    memset(&portM_address, 0, sizeof(portM_address));
    portM_address.sin_family = AF_INET;
    portM_address.sin_addr.s_addr = inet_addr(LOCALHOST); // differs from sender
    portM_address.sin_port = 0;


    if (bind(client.sockid_portM_accepter, (struct sockaddr*) &portM_address, sizeof(portM_address)) < 0)
    {
        perror("bindM");
        exit(1);
    }

    int portM_address_len = sizeof(portM_address);
    if(getsockname(client.sockid_portM_accepter, (struct sockaddr*) &portM_address, &portM_address_len) < 0)
    {
        perror("getsockname error");
        exit(1);
    }


        // bind to receive address
    if (bind(client.sockid_portX, (struct sockaddr*) &portX_address, sizeof(portX_address)) < 0)
    {
        perror("bind");
        exit(1);
    }



    // filling and concating port and Ip of PortM for sending to server
    // char* getting_username = "What is your username?";
    // write(1, getting_username, strlen(getting_username));
    // scanf("%s", client.selfusername);
    // printf("heeeereeeee\n");

    char* SpecificClient = "Do you want to play with a friend? Type the username\n";
    write(1, SpecificClient, strlen(SpecificClient));
    char buf[30];
    read(0, buf, 30);

    char* temp1;
    temp1 = strtok(buf, " ");
    int tempSize = strlen(temp1);
    client.contestentUsername = malloc(tempSize);
    for(int j=0; j<tempSize; j++)
        client.contestentUsername[j] = temp1[j];

    client.selfusername = argv[3];         
    client.selfipM = inet_ntoa(portM_address.sin_addr);
    int2string(ntohs(portM_address.sin_port), client.selfportM);


    msg = concatination(client.selfusername, client.selfportM);
    msg = concatination(msg, client.selfipM);
    msg = concatination(msg, client.contestentUsername);

    char* UserPortIp = "username, port, ip, contestent username \n";
    write(1, UserPortIp, strlen(UserPortIp));
    write(1, msg, strlen(msg));


    //Time interval
    struct pollfd fd;
    int res;

    fd.fd = client.sockid_portX;
    fd.events = POLLIN;
    res = poll(&fd, 1, 2000); // 1000 ms timeout

    char buffer[1025];  //data buffer of 1K
    char contestent_info[100];
    int recvbytes;
    int temp_portM;
    struct sockaddr_in contestent_address;
    int c = -1;

    pid_t childPID;
    int result;

    if (res == 0)
    {
        char* BroadCastMessage;
        client.ipY = SSDPII;
        client.portY = argv[5];

        write(1, client.ipY, sizeof(client.ipY));

        //server 
        char* serverOfflineMsg = "Server is Offline \n";
        write(1, serverOfflineMsg, strlen(serverOfflineMsg));
        sockid_portY = socket(AF_INET, SOCK_DGRAM, 0);
        if (sockid_portY < 0) {
            perror("socketY");
            exit(1);
        }

        memset(&sockid_PortY_dest_addr, 0, sizeof(sockid_PortY_dest_addr));
        sockid_PortY_dest_addr.sin_family = AF_INET;
        sockid_PortY_dest_addr.sin_addr.s_addr = inet_addr(client.ipY);
        sockid_PortY_dest_addr.sin_port = htons(atoi(client.portY));


        // use setsockopt() to request that the kernel join a multicast group
        struct ip_mreq mreq2;
        mreq2.imr_multiaddr.s_addr = inet_addr(client.ipY);
        mreq2.imr_interface.s_addr = htonl(INADDR_ANY);
        
        //Option for Port Y
        if (setsockopt(sockid_portY, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*) &mreq2, sizeof(mreq2)) < 0){
            perror("setsockoptY");
            exit(1);
        }

        //client
        u_int yes = 1;
        if (setsockopt(sockid_portY, SOL_SOCKET, SO_REUSEADDR, (char*) &yes, sizeof(yes)) < 0){
           perror("Reusing ADDR failed Y");
           exit(1);
        }

        if (bind(sockid_portY, (struct sockaddr*) &sockid_PortY_dest_addr, 
            sizeof(sockid_PortY_dest_addr)) < 0)
        {
            perror("bind");
            exit(1);
        }

        struct pollfd filedis;

        filedis.fd = sockid_portY;
        filedis.events = POLLIN;
        result = poll(&filedis, 1, 5000); // 1000 ms timeout

        if(result == 0)
        {
            pid_t childPID = fork();
            if(childPID == 0)
            {
                char* BroadCast = "BroadCastMessage: \n";
                write(1, BroadCast, strlen(BroadCast));
                BroadCastMessage = concatination(client.selfusername, client.selfportM);
                BroadCastMessage = concatination(BroadCastMessage, client.selfipM);
                msg = BroadCastMessage;
                infoBroadcast();
                while(1){}
            }
            else
                c = 0;
        }
        else if(result <= -1)
        {
            perror("result error");
            exit(1);
        }
        else
        {
            char udpYmessage[1000];
            int udp_addrlen = sizeof(sockid_PortY_dest_addr);
            int udprecvbytes = recvfrom(sockid_portY, udpYmessage, MSGBUFSIZE, 0, 
                (struct sockaddr *) &sockid_PortY_dest_addr, &udp_addrlen);
            udpYmessage[udprecvbytes] = '\0';
            if (udprecvbytes < 0)
            {
                perror("recvfrom");
                exit(1);
            }

            char* temp1;
            temp1 = strtok(udpYmessage, " ");
            int tempSize = strlen(temp1);
            client.contestentUsername = malloc(tempSize);
            for(int j=0; j<tempSize; j++)
                client.contestentUsername[j] = temp1[j];          

            temp1 = strtok(NULL, " ");
            tempSize = strlen(temp1);
            client.contestentPortM = malloc(tempSize);
            for(int j=0; j<tempSize; j++)
                client.contestentPortM[j] = temp1[j];

            temp1 = strtok(NULL, " ");
            tempSize = strlen(temp1);
            client.contestentIpM = malloc(tempSize);
            for(int j=0; j<tempSize; j++)
                client.contestentIpM[j] = temp1[j];
            c = 1;
        }
    }
    else if (res == -1)
    {
        perror("res error");
        exit(1);
    }
    else
    {
        int udp_addrlen = sizeof(portX_address);
        int udprecvbytes = recvfrom(client.sockid_portX, msgbuf, MSGBUFSIZE, 0, 
            (struct sockaddr *) &portX_address, &udp_addrlen);
        if (udprecvbytes < 0)
        {
            perror("recvfrom");
            exit(1);
        }

        char* temp1;

        temp1 = strtok(msgbuf, " ");
        int tempSize = strlen(temp1);
        client.ipN = malloc(tempSize);
        for(int j=0; j<tempSize; j++)
            client.ipN[j] = temp1[j];

        temp1 = strtok(NULL, " ");
        tempSize = strlen(temp1);
        client.portN = malloc(tempSize);
        for(int j=0; j<tempSize; j++)
            client.portN[j] = temp1[j];
        

        // struct of server address
        struct sockaddr_in server_address;
        memset(&server_address, 0, sizeof(server_address));
        server_address.sin_family = AF_INET;
        server_address.sin_addr.s_addr = inet_addr(LOCALHOST); // differs from sender
        server_address.sin_port = htons(atoi(client.portN));

        //connecting to server
        if( (connect(client.sockid_portN, (struct sockaddr*) &server_address, 
            sizeof(server_address))) < 0)
        {
            perror("connect failed");
            exit(1);
        }

        // accepted message receive
        char accepted_msg[100];
        int accepted_msg_bytes;
        if( (accepted_msg_bytes = recv(client.sockid_portN, accepted_msg, sizeof(accepted_msg), 0)) < 0)
        {
            perror("Accepted message error");
            exit(1);
        }
        accepted_msg[accepted_msg_bytes] = '\0';
        char* accepted_msgMsg = "accepted confirmation message";
        write(1, accepted_msgMsg, strlen(accepted_msgMsg));
        write(1, accepted_msg, strlen(accepted_msg));


        // sending port and Ip of sockM to server to find a rival
        if( send(client.sockid_portN, msg, strlen(msg), 0) != strlen(msg) )
        {
            perror("send");
            exit(1);
        }
        char* ContestentInfoSendMsg = "Info (username, Ip, Port, Contestent Username) message sent successfully\n";
        write(1, ContestentInfoSendMsg, strlen(ContestentInfoSendMsg));
        
        
        //contestent info receive
        memset(&contestent_address, 0, sizeof(contestent_address));
        contestent_address.sin_family = AF_INET;
        contestent_address.sin_addr.s_addr = inet_addr(LOCALHOST); // differs from sender
        contestent_address.sin_port;
        int contestent_address_len;

        recvbytes = recv(client.sockid_portN, contestent_info, sizeof(contestent_info), 0);
        contestent_info[recvbytes] = '\0';
        char* ContestentInfoRecvMsg = "Rival Information received successfully\n";
        write(1, ContestentInfoRecvMsg, strlen(ContestentInfoRecvMsg));
        write(1, contestent_info, strlen(contestent_info));

        if( recvbytes < 0 )
        {
            perror("receive info error");
            exit(1);
        }
        else if(recvbytes != 0)
        {
            // tokenizing and filling contestent info
            char* temp2;

            temp2 = strtok(contestent_info, " ");
            int tempSize2 = strlen(temp2);
            client.contestentIpM = malloc(tempSize2);
            for(int j=0; j<tempSize2; j++)
                client.contestentIpM[j] = temp2[j];

            temp2 = strtok(NULL, " ");
            tempSize2 = strlen(temp2);
            client.contestentPortM = malloc(tempSize2);
            for(int j=0; j<tempSize2; j++)
                client.contestentPortM[j] = temp2[j];
            c = 1;
        }
        else
        {
            puts("receive info:");
            printf("%d\n", recvbytes);
            puts("recv");
            c = 0;
        }
    }


    if(c == 0) // accept
    {
        int new_socket, valread; 
        struct sockaddr_in address; 
        int opt = 1; 
        int addrlen = sizeof(portM_address); 
        char buffer[1025] = {0};
        char* hello; 
           
        // Forcefully attaching socket to the port 8080 
        if (setsockopt(client.sockid_portM_accepter, SOL_SOCKET, 
            SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) 
        { 
            perror("setsockopt");
            exit(EXIT_FAILURE); 
        } 
        
        char* acceptMsg = "Waiting for new connection to accept\n";
        write(1, acceptMsg, strlen(acceptMsg));

        if (listen(client.sockid_portM_accepter, 3) < 0) 
        { 
            perror("listen"); 
            exit(EXIT_FAILURE); 
        } 
        if ((new_socket = accept(client.sockid_portM_accepter, (
            struct sockaddr *)&portM_address, (socklen_t*)&addrlen))<0) 
        { 
            perror("accept"); 
            exit(EXIT_FAILURE); 
        } 

        hello = "hello!!";
        char buff[1024];
        valread = read( new_socket , buff, 1024); 
        char* contestentRecvMsg = "new message from your rival\n";
        write(1, contestentRecvMsg, strlen(contestentRecvMsg));
        write(1, buff, strlen(buff));
        send(new_socket , hello , strlen(hello) , 0 ); 
        char* contestentSendMsg = "Message to your rival sent successfully\n";
        write(1, contestentSendMsg, strlen(contestentSendMsg));
    }

    else //connect
    {
        char* connectingMsg = "Connecting to rival\n";
        write(1, connectingMsg, strlen(connectingMsg));
        int valread; 
        struct sockaddr_in serv_addr; 
        char *hello = "Hello from client"; 
        char buffer[1025] = {0}; 

        if ((client.sockid_portM_connector = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
        { 
            perror("\n Socket creation error \n"); 
            exit(1);
        } 
       
        memset(&serv_addr, '0', sizeof(serv_addr)); 
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_addr.s_addr = inet_addr(LOCALHOST); // differs from sender
        serv_addr.sin_port = htons(atoi(client.contestentPortM));

           
        // Convert IPv4 and IPv6 addresses from text to binary form 
        if(inet_pton(AF_INET, LOCALHOST, &serv_addr.sin_addr) <= 0)  
        { 
            perror("\nInvalid address/ Address not supported \n"); 
            exit(1);
        } 
       
        if (connect(client.sockid_portM_connector, (struct sockaddr *)&serv_addr, 
            sizeof(serv_addr)) < 0) 
        { 
            perror("\nConnection Failed \n"); 
            exit(1);
        }

        send(client.sockid_portM_connector , hello , strlen(hello) , 0 ); 
        char* contestentSendMsg = "Message to your rival sent successfully\n";
        write(1, contestentSendMsg, strlen(contestentSendMsg));
        valread = read( client.sockid_portM_connector , buffer, 1024); 
        char* contestentRecvMsg = "new message from your rival\n";
        write(1, contestentRecvMsg, strlen(contestentRecvMsg));
        write(1, buffer, strlen(buffer));        
    }

    if(result == 0)
    {
        kill(childPID, SIGTERM);
    }

    int close_PortX = close(client.sockid_portX);
    int close_PortN = close(client.sockid_portN);
    int close_PortY = close(sockid_portY);
    return 0;
}