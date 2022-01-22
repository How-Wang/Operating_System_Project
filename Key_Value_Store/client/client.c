#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <ctype.h>
#include <stdlib.h>

#include <pthread.h>
#include "sock.h"

struct Args
{
    char* server_host;
    char* server_port;
};

void* clienthread(void *server_info)
{
    /* Open a client socket fd */
    char* server_host_name = ((struct Args*)server_info)->server_host;
    char* server_port = ((struct Args*)server_info)->server_port;
    int clientfd __attribute__((unused)) = open_clientfd(server_host_name, server_port);
    char first_info[256] = {};

    /* Send first msg to Server */
    sprintf(first_info,"%s",server_host_name);
    send(clientfd,first_info,sizeof(first_info),0);
    sprintf(first_info,"[INFO] Connect to %s:%s\n[INFO] Welcome! Please type HELP for avalable commands",server_host_name,server_port);
    printf("%s\n",first_info);

    while(1)
    {
        /* Send msg to Server step by step*/
        char input[256] = {};
        scanf("%[^\n]%*c",input);
        char receiveMessage[256] = {};
        send(clientfd,input,sizeof(input),0);

        /* Receive the msg from Server */
        recv(clientfd,receiveMessage,sizeof(receiveMessage),0);
        if(strcmp(receiveMessage,"[OK] Close the client") == 0)
        {
            /*
            If receiveMessage is Close
            break the while loop to close client and exit thread.
            */
            printf("%s\n",receiveMessage);
            break;
        }
        else if(strcmp(receiveMessage,"HELP") ==0)
        {
            printf("Commands                Descrption\nSET [key] [value]     Store the key value pair ([key], [value]) into the database.\nGET [key]             Get the value of key from database.\nDELETE [key]          Delete [key] and it's associated value from the database\n.\nEXIT                  Exit the connection.\n");
        }
        else
        {
            printf("%s\n",receiveMessage);
        }
    }
    close(clientfd);
    pthread_exit(NULL);
    return 0;
}

int main(int argc, char **argv)
{
    int opt;
    char *server_host_name = NULL, *server_port = NULL;
    pthread_t tid;

    /* Parsing args */
    while ((opt = getopt(argc, argv, "h:p:")) != -1)
    {
        switch (opt)
        {
        case 'h':
            server_host_name = malloc(strlen(optarg) + 1);
            strncpy(server_host_name, optarg, strlen(optarg));
            break;
        case 'p':
            server_port = malloc(strlen(optarg) + 1);
            strncpy(server_port, optarg, strlen(optarg));
            break;
        case '?':
            fprintf(stderr, "Unknown option \"-%c\"\n", isprint(optopt) ?
                    optopt : '#');
            return 0;
        }
    }

    if (!server_host_name)
    {
        fprintf(stderr, "Error!, No host name provided!\n");
        exit(1);
    }

    if (!server_port)
    {
        fprintf(stderr, "Error!, No port number provided!\n");
        exit(1);
    }

    /* create a thread and prepare the Server info in order to connect */
    struct Args *args = (struct Args *)malloc(sizeof(struct Args));
    args->server_host = server_host_name;
    args->server_port = server_port;
    pthread_create(&tid,NULL,clienthread,(void *)args);
    pthread_join(tid,NULL);
    return 0;
}
