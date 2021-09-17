#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include "mfs.h"
#include "udp.h"

#define CLIENT_PORT (20000)

struct sockaddr_in server_sockaddr;
struct sockaddr_in receipt_sockaddr;
int GLOBAL_SD; // global fd for communication through specified client port 

// ***---------------- Helper functions for debugging  ----------------*** //




// ***---------------- Functions for sending and receiving message to and from server ----------------*** //

int MFS_Send_Message(MFS_Message_t* message) {
    char* message_buffer = malloc(sizeof(MFS_Message_t));
    bzero(message_buffer, sizeof(MFS_Message_t));
    memcpy(message_buffer, message, sizeof(MFS_Message_t));
    int write_status = UDP_Write(GLOBAL_SD, &server_sockaddr, message_buffer, sizeof(MFS_Message_t));
    free(message_buffer);
    return write_status;
}

int MFS_Receive_Reply(MFS_Reply_t* reply) {
    fd_set readset;
    struct timeval timer;
    
    // timeout 
    char* reply_buffer = malloc(sizeof(MFS_Reply_t));
    bzero(reply_buffer, sizeof(MFS_Reply_t));
    bzero(&receipt_sockaddr, sizeof(struct sockaddr_in));
    timer.tv_sec = 5;
    timer.tv_usec = 0;
    FD_ZERO(&readset);
    FD_SET(GLOBAL_SD, &readset);
    int select_status = select(GLOBAL_SD+1, &readset, NULL, NULL, &timer);
    if (select_status < 0) {
        perror("select failed \n");
        exit(1);
    }else if (FD_ISSET(GLOBAL_SD, &readset)) {
        // read succeeded, push reply message into reply
        // exit with success status
        int reply_status = UDP_Read(GLOBAL_SD, &receipt_sockaddr, reply_buffer, sizeof(MFS_Reply_t));
        memcpy(reply, reply_buffer,sizeof(MFS_Reply_t));
        free(reply_buffer);
        return reply_status;
    }
    free(reply_buffer);
    return -2; //-2 represents time out!
}

int MFS_Send_and_Receive(MFS_Message_t* message, MFS_Reply_t* reply){
    int write_status = MFS_Send_Message(message);
    assert(write_status > 0);
    int read_status = MFS_Receive_Reply(reply);
    while (read_status == -2) {
        write_status = MFS_Send_Message(message);
        assert(write_status > 0);
        read_status = MFS_Receive_Reply(reply);
    }
    free(message);
    return read_status;
}

// ***---------------- Interface functions  ----------------*** //

int MFS_Init(char *hostname, int port){
    // bind to port for listening to server response
    GLOBAL_SD = UDP_Open(CLIENT_PORT); //communicate through specified client port
    assert(GLOBAL_SD > -1); 

    // find and contact the server exporting the file system
    int server_sockaddr_init_status = UDP_FillSockAddr(&server_sockaddr, hostname, port);
    assert(server_sockaddr_init_status == 0);

    // Craft message and prep to get reply
    MFS_Message_t* message = malloc(sizeof(MFS_Message_t));
    bzero(message, sizeof(MFS_Message_t));
    message->message_type = 'I';
    MFS_Reply_t* reply = malloc(sizeof(MFS_Reply_t));
    bzero(reply, sizeof(MFS_Reply_t));

    // Send and wait for reply
    int read_status = MFS_Send_and_Receive(message, reply);
    assert(read_status > 0);
    int result = reply->success;
    free(reply);
    return result;
}

int MFS_Lookup(int pinum, char *name){

    // craft message 
    MFS_Message_t* message = malloc(sizeof(MFS_Message_t));
    bzero(message, sizeof(MFS_Message_t));
    message->message_type = 'L';
    message->inum = pinum;
    strcpy(message->name, name);

    // send message and wait for reply
    MFS_Reply_t* reply = malloc(sizeof(MFS_Reply_t));
    bzero(reply, sizeof(MFS_Reply_t));
    int read_status = MFS_Send_and_Receive(message, reply);
    assert(read_status > 0); // check that read succeeded
    int result = reply->success;    
    free(reply);   
    /* 
    if (result >= 0) {
        printf("CLIENT:: Lookup was a success! gonna return the inum : %d \n", result);
    }
    */
    return result;
}

int MFS_Stat(int inum, MFS_Stat_t *m){
    // craft message
    MFS_Message_t* message = malloc(sizeof(MFS_Message_t));
    bzero(message, sizeof(MFS_Message_t));
    message->message_type = 'S';
    message->inum = inum;
    MFS_Reply_t* reply = malloc(sizeof(MFS_Reply_t));
    bzero(reply, sizeof(MFS_Reply_t));

    // send and wait for reply
    //printf("CLIENT:: Sending Stat request now \n");
    int read_status = MFS_Send_and_Receive(message, reply);
    assert(read_status > 0); // check that read succeeded

    //process the reply
    int result = reply->success;
    if (!result) {
        //printf("CLIENT:: Stat request was successful \n");
        // result = 0, so valid inum read 
        m->type = reply->stats.type;
        m->size = reply->stats.size;
        m->blocks = reply->stats.blocks;
    }
    free(reply);
    return result;
}

int MFS_Write(int inum, char *buffer, int block){
    //craft message and prep for reply
    MFS_Message_t* message = malloc(sizeof(MFS_Message_t));
    bzero(message, sizeof(MFS_Message_t));
    message->message_type = 'W';
    message->inum = inum;
    message->block = block;
    memcpy(message->buffer, buffer, BUFFER_SIZE);
    int prelim = memcmp(message->buffer, buffer, BUFFER_SIZE);
    printf("CLIENT:: For write, crafting message now, the memcmp result is: %d \n", prelim);
    MFS_Reply_t* reply = malloc(sizeof(MFS_Reply_t));
    bzero(reply, sizeof(MFS_Reply_t));

    //send message and wait for reply
    int read_status = MFS_Send_and_Receive(message, reply);
    assert(read_status > 0); // check that read succeeded
    
    //process the reply
    int result = reply->success;
    free(reply);
    return result;
}

int MFS_Read(int inum, char *buffer, int block){
    //craft message and prep for reply
    MFS_Message_t* message = malloc(sizeof(MFS_Message_t));
    bzero(message, sizeof(MFS_Message_t));
    message->message_type = 'R';
    message->inum = inum;
    message->block = block;
    MFS_Reply_t* reply = malloc(sizeof(MFS_Reply_t));
    bzero(reply, sizeof(MFS_Reply_t));

    // send message and wait for reply
    int read_status = MFS_Send_and_Receive(message, reply);
    assert(read_status > 0);

    // process the reply
    int result = reply->success;
    if (!result) {
        memcpy(buffer, reply->buffer, BUFFER_SIZE);
    }
    free(reply);
    return result;
}

int MFS_Creat(int pinum, int type, char *name){
    // craft message and prep for reply
    MFS_Message_t* message = malloc(sizeof(MFS_Message_t));
    bzero(message, sizeof(MFS_Message_t));
    message->message_type = 'C';
    message->inum = pinum;
    message->type = type;
    strcpy(message->name, name);
    MFS_Reply_t* reply = malloc(sizeof(MFS_Reply_t));
    bzero(reply, sizeof(MFS_Reply_t));

    // send message and wait for reply
    int read_status = MFS_Send_and_Receive(message, reply);
    assert(read_status > 0); // check that read succeeded

    // process the reply
    int result = reply->success;
    free(reply);
    return result;
}

int MFS_Unlink(int pinum, char *name){
    // craft message and prep for reply
    MFS_Message_t* message = malloc(sizeof(MFS_Message_t));
    bzero(message, sizeof(MFS_Message_t));
    message->message_type = 'U';
    message->inum = pinum;
    strcpy(message->name, name);
    MFS_Reply_t* reply = malloc(sizeof(MFS_Reply_t));
    bzero(reply, sizeof(MFS_Reply_t));

    // send message and wait for reply
    int read_status = MFS_Send_and_Receive(message, reply);
    assert(read_status > 0);

    // process the reply
    assert(read_status > 0); // check that read succeeded
    int result = reply->success;
    free(reply);
    return result;
}