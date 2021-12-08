#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/time.h>
#include <string.h>
#include <math.h>

#define BUFFERSIZE 1494
#define ACKSIZE 7
#define MAXIMUM_SEGMENT_SIZE 1500 //BUFFERSIZE + ACKSIZE
#define MSG_CONTROL_SIZE 20
#define FILENAME_SIZE 255

#define PORTDATA 8080

int sizeFile(const char * filename);
int max (int a, int b);



int main(int argc, char *argv[]){

    printf("\n========================= Server Scenario 1 =========================\n");

    int socket_control;
    int socket_data;
    struct sockaddr_in server, client;
    socklen_t server_size = sizeof(server);
    socklen_t client_size = sizeof(client);
    int port;
    int valid = 1;
    int msgSize;
    char *syn_ack_msg = "SYN-ACK8080";
    char *end_msg = "FIN";
    char rcv_msg_control[MSG_CONTROL_SIZE];
    char rcv_msg_requested_file[FILENAME_SIZE];

    char buffer[BUFFERSIZE];
    char ack[ACKSIZE];
    char segment[MAXIMUM_SEGMENT_SIZE];

    if (argc <2){
        printf("Execution : ./server1 <port_number> \n");
        exit(-1);
    }
    port =  atoi(argv[1]);

    // Creating the socket control

    socket_control = socket(AF_INET, SOCK_DGRAM, 0);
    if(socket_control  < 0){
        printf("FAILURE CREATING SOCKET CONTROL \n");
        exit(-1);
    }

    setsockopt(socket_control, SOL_SOCKET, SO_REUSEADDR, &valid, sizeof(int));

    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    
    if (bind(socket_control, (struct sockaddr*) &server, sizeof(server)) == -1) {
		printf("FAILURE BINDING SOCKET CONTROL \n");
        exit(-1);
	}

    // 3way handshake processus

    printf("\nSTARTING THE 3WAY HANDSHAKE PROCESSUS ... \n\n");
    
    msgSize = recvfrom(socket_control,rcv_msg_control,MSG_CONTROL_SIZE, 0, (struct sockaddr *) &client, &client_size);
    rcv_msg_control[msgSize] = '\0';
    printf("\n <==> SERVER RECEIVE : %s FROM %s:%d\n", rcv_msg_control, inet_ntoa(client.sin_addr),ntohs(client.sin_port));

    sendto(socket_control,(const char *) syn_ack_msg, strlen(syn_ack_msg), 0, (const struct sockaddr *)&client, client_size);
    printf("\n <==> SERVER SENT : %s\n",syn_ack_msg);

    msgSize = recvfrom(socket_control,rcv_msg_control,MSG_CONTROL_SIZE, 0, (struct sockaddr *) &client, &client_size);
    rcv_msg_control[msgSize] = '\0';
    printf("\n <==> SERVER RECEIVE : %s FROM %s:%d\n", rcv_msg_control, inet_ntoa(client.sin_addr),ntohs(client.sin_port));

    printf("\nEND OF THE 3WAY HANDSHAKE PROCESSUS \n\n");

    // Creating the socket data

    socket_data = socket(AF_INET, SOCK_DGRAM, 0);
    if(socket_data <0){
        printf("FAILURE CREATING SOCKET DATA \n");
        exit(-1);
    }

    setsockopt(socket_data, SOL_SOCKET, SO_REUSEADDR, &valid, sizeof(int));
    server.sin_port = htons(PORTDATA);
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    

    if(bind(socket_data, (struct sockaddr*) &server, sizeof(server)) < 0){
        printf("FAILURE BINDING SOCKET DATA \n");
        exit(-1);
    }

    // Reception of the requested file

    msgSize = recvfrom(socket_data,rcv_msg_requested_file,FILENAME_SIZE, 0, (struct sockaddr *) &client, &client_size);
    rcv_msg_requested_file[msgSize] = '\0';
    char *filename = rcv_msg_requested_file;

    //Lecture du fichier demandé par le client

    printf("THE ASKED FILE IS %s\n", filename);
    FILE *fp = fopen(filename, "rb");
    if(fp == NULL){
        printf("FAILURE OPENING THE REQUESTED FILE \n");
        exit(-1);
    }

    // Variables utilisees pour la fenetre
    
    int seg_number = 0;
    int window_size = 50;
    int ack_received;
    int b_fread;
    int number_of_fragment = sizeFile(filename)/BUFFERSIZE +1;
    int size_of_last_segment;
    printf("NUMBER OF FRAGMENT %d\n", number_of_fragment);
    fd_set desc;
    
    

    struct timeval debut , fin,timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    int duplicates_ack;
    
    //construction de packet

    typedef struct pacquet{

        char data[BUFFERSIZE];

    } pkt;
    
    pkt* sequence=malloc(sizeof(struct pacquet)*number_of_fragment);
    
    for (int i=1; i<= number_of_fragment; i++){
        if((b_fread = fread(buffer, sizeof(char), BUFFERSIZE, fp))> 0){
            memcpy(sequence[i].data,buffer,BUFFERSIZE);
            size_of_last_segment = b_fread;
        }

    }
    
    gettimeofday(&debut, NULL);
    while (seg_number <= number_of_fragment){

        
        for (int i = seg_number+1; i <=window_size+seg_number && i<=number_of_fragment; i++){
            
            
            sprintf(ack, "%06d" ,i);
            memcpy(&segment[0], ack, ACKSIZE);
            
            if(i == number_of_fragment){
                memcpy(&segment[6], sequence[i].data, size_of_last_segment+6);
                sendto(socket_data, segment, size_of_last_segment+6, 0, (struct sockaddr *)&client, client_size);
            }
            else{
                memcpy(&segment[6], sequence[i].data, MAXIMUM_SEGMENT_SIZE);
                sendto(socket_data, segment, MAXIMUM_SEGMENT_SIZE, 0, (struct sockaddr *)&client, client_size);
            }
        }
        
        
        FD_SET(socket_data, &desc);
        select(socket_data+1, &desc, NULL, NULL,&timeout);
        if(FD_ISSET(socket_data, &desc)!=0){
    
            msgSize = recvfrom(socket_data,rcv_msg_control,MSG_CONTROL_SIZE,MSG_DONTWAIT, (struct sockaddr *)&client, &client_size);
            rcv_msg_control[msgSize] = '\0';
            ack_received = atoi(&rcv_msg_control[3]);
            printf("ACK %d RECEIVED \n", ack_received);
            
            if (ack_received > seg_number){
                seg_number = ack_received+1;
                
            }else if (ack_received == seg_number+window_size){
                seg_number = ack_received +1;
                window_size=window_size*2;
                
            }
            else if(ack_received == seg_number){
                
                duplicates_ack++;

                if (duplicates_ack == 3){//FASTRETRANSMIT
                    printf("FAST RETRANSMIT FOR THE PACKET %d\n", ack_received+1);
                    sprintf(ack, "%06d" ,ack_received+1);
                    memcpy(&segment[0], ack, ACKSIZE);
                
                    if(ack_received+1 == number_of_fragment){
                        memcpy(&segment[6], sequence[ack_received+1].data, size_of_last_segment+6);
                        sendto(socket_data, segment, size_of_last_segment+6, 0, (struct sockaddr *)&client, client_size);
                    }
                    else{
                        memcpy(&segment[6], sequence[ack_received+1].data, MAXIMUM_SEGMENT_SIZE);
                        sendto(socket_data, segment, MAXIMUM_SEGMENT_SIZE, 0, (struct sockaddr *)&client, client_size);
                    }

                }
            }
        
        }else{//timeout on retransmet le paquet et on diminue la taille de la fenêtre

            sprintf(ack, "%06d" ,seg_number);
            memcpy(&segment[0], ack, ACKSIZE);
            
            if(seg_number == number_of_fragment){
                memcpy(&segment[6], sequence[seg_number].data, size_of_last_segment+6);
                sendto(socket_data, segment, size_of_last_segment+6, 0, (struct sockaddr *)&client, client_size);
            }
            else{
                memcpy(&segment[6], sequence[seg_number].data, MAXIMUM_SEGMENT_SIZE);
                sendto(socket_data, segment, MAXIMUM_SEGMENT_SIZE, 0, (struct sockaddr *)&client, client_size);
            }
            
            //window_size=max(window_size/2, 2);
            
        }
        
        
    }
    gettimeofday(&fin, NULL);
    long double elapsed_time = fin.tv_sec - debut.tv_sec + 0.000001* (fin.tv_usec - debut.tv_usec) ;
    printf("SIZE FILE %d\n",sizeFile(filename));
    double debit = sizeFile(filename)/elapsed_time;
    //printf("Le debit est %.3f ko/s \n", debit/1024);
    //printf("Le debit est %.3f Mo/s \n", debit/1048576);
    printf("Le debit est %.3f Mo/s \n", debit/1000000);
    free(sequence);
    sendto(socket_control,(const char *) end_msg, strlen(end_msg), 0, (const struct sockaddr *)&client, client_size);
    printf("\n <==> SERVER SENT : %s\n",end_msg);
    
    close(socket_data);
    close(socket_control);
    


    


    


}

int max (int a, int b){
    if (a> b){
        return a;   
    }
    return b;
}
int sizeFile(const char * filename){
    
    FILE * f;
    long int size = 0;
    f = fopen(filename, "r");
    fseek(f, 0, SEEK_END);

    size = ftell(f);

    if (size != -1)
        return(size);
    return 0;
 }
