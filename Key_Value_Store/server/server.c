#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <pthread.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <netdb.h>
#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <semaphore.h>

#include "types.h"
#include "sock.h"

sem_t x;
pthread_t dealthreads[100];
char *server_port = 0;

void* serverthread(void* voidpointer_forClientSockfd)
{
    int forClientSockfd = (*(int *)(voidpointer_forClientSockfd));
    char inputBuffer[256] = {};
    char file_name[256] = {};
    char message[256] = {};
    char token[256] = {};
    char token_key[256] = {};
    char token_num[256] = {};
    char first_info[256]= {};
    FILE* fptr;
    pid_t threadId = syscall(__NR_gettid);

    /* Receive the initial infomation from Client, and print it out.*/
    recv(forClientSockfd,inputBuffer,sizeof(inputBuffer),0);
    sprintf(first_info,"[CLIENT CONNECTED] Connect to the client %s",inputBuffer);
    printf("%s\n",first_info);
    sprintf(first_info,"[INFO] Listen to the port %s ...",server_port);
    printf("%s\n",first_info);
    sprintf(first_info,"[THREAD INFO] Thread (id = %d) is created serving connection fd %d",threadId,forClientSockfd);
    printf("%s\n",first_info);

    while(1)
    {
        /* Receive the infomation from Client step by step */
        recv(forClientSockfd,inputBuffer,sizeof(inputBuffer),0);
        strcpy(token,strtok(inputBuffer," "));

        /* SET [Key] [Value] command */
        if (strcmp(token,"SET") == 0)
        {
            /* Lock the database */
            sem_wait(&x);
            /* get key and value string , and check key.txt file exist or not */
            strcpy(token_key,strtok(NULL," "));
            strcpy(token_num,strtok(NULL," "));
            sprintf(file_name,"%s.txt",token_key);
            if( access( file_name, F_OK ) == 0 )
            {
                /* If the file has existed, send ERROR msg to Client. */
                sprintf(message,"[ERROR] Key \"%s\" has existed",token_key);
                send(forClientSockfd,message,sizeof(message),0);
            }
            else
            {
                /* If not exist, created the "key.txt" and print "value" in the file, send OK msg to Client. */
                fptr = fopen(file_name, "w");
                fputs(token_num,fptr);
                fclose(fptr);
                sprintf(message,"[OK] Key value pair (%s, %s) is stored",token_key,token_num);
                send(forClientSockfd,message,sizeof(message),0);
            }
            /* Unlocked the database */
            sem_post(&x);
        }

        /* GET [Key] command */
        else if (strcmp(token,"GET") == 0)
        {
            /* Lock the database */
            sem_wait(&x);
            /* Get key string and check "key.txt" file exist or not */
            strcpy(token_key,strtok(NULL," "));
            sprintf(file_name,"%s.txt",token_key);
            if( access( file_name, F_OK ) == 0 )
            {
                /* If the file exists, read the value from "key.txt" and send it back to Client. */
                fptr= fopen(file_name, "r");
                fscanf(fptr,"%s",token_num);
                sprintf(message,"[OK] The value of \"%s\" is %s",token_key,token_num);
                send(forClientSockfd,message,sizeof(message),0);
                fclose(fptr);
            }
            else
            {
                /* If the file doesn't exist, sends ERROR msg to Client. */
                sprintf(message,"[ERROR] Key \"%s\" not exist",token_key);
                send(forClientSockfd,message,sizeof(message),0);
            }
            /* Unlocked the database */
            sem_post(&x);
        }

        /* DELETE [Key] command */
        else if (strcmp(token,"DELETE") == 0)
        {
            /* Lock the database */
            sem_wait(&x);
            /* Get key string and check "key.txt" file exist or not */
            strcpy(token_key,strtok(NULL," "));
            sprintf(file_name,"%s.txt",token_key);
            if( access( file_name, F_OK ) == 0 )
            {
                /* If the file exists, remove "key.txt" and send OK msg to Client. */
                remove(file_name);
                sprintf(message,"[OK] Key \"%s\" is removed",token_key);
                send(forClientSockfd,message,sizeof(message),0);
            }
            else
            {
                /* If the file doesn't exist, sends ERROR msg to Client. */
                sprintf(message,"[ERROR] Key \"%s\" not exist",token_key);
                send(forClientSockfd,message,sizeof(message),0);
            }
            /* Unlock the database */
            sem_post(&x);
        }

        /* HELP command */
        else if (strcmp(token,"HELP") == 0)
        {
            /* Send HELP msg to Client, and Client print it by itself. */
            sprintf(message,"HELP");
            send(forClientSockfd,message,sizeof(message),0);
        }

        /* EXIT command */
        else if (strcmp(token,"EXIT") == 0)
        {
            /* Send OK msg to Client and close the thread. */
            sprintf(message,"[OK] Close the client");
            send(forClientSockfd,message,sizeof(message),0);
            pthread_exit(NULL);
        }

        /* Others unknow msg */
        else
        {
            /* Send ERROR msg to Client. */
            sprintf(message,"[ERROR] Wrong input!");
            send(forClientSockfd,message,sizeof(message),0);
        }

    }
}


int main(int argc, char **argv)
{
    int opt = 0;
    /* Parsing args */
    while ((opt = getopt(argc, argv, "p:")) != -1)
    {
        switch (opt)
        {
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

    if (!server_port)
    {
        fprintf(stderr, "Error! No port number provided!\n");
        exit(1);
    }

    /* Clean all "key.txt" files */
    DIR *directory = NULL;
    directory = opendir ("./");
    struct dirent *ent;
    while ((ent = readdir(directory)) != NULL)
    {
        int length = strlen(ent->d_name);
        if(strncmp(ent->d_name+length-4,".txt",4) == 0)
            remove(ent->d_name);
    }

    /* Initailze the sem to lock database */
    sem_init(&x, 0,1);

    char First_Info[256] = "[INFO] Start with a clean database... \n[INFO] Initializing the server...\n[INFO] Server initialized\n[INFO] Listening on the port ";
    printf("%s",First_Info);
    printf("%s\n",server_port);

    /*
    Open a listen socket fd, and
    Create threads when
    Server accept Client.
    */
    int listenfd __attribute__((unused)) = open_listenfd(server_port);
    int *forClientSockfd;
    struct sockaddr_in clientInfo;
    socklen_t addrlen = sizeof(clientInfo);
    int i = 0;
    while(1)
    {
        forClientSockfd = malloc(sizeof(int));

        /* Accept the Client and create the thread */
        *forClientSockfd = accept(listenfd,(struct sockaddr*) &clientInfo, &addrlen);
        pthread_create(&dealthreads[i++],NULL,serverthread,forClientSockfd);

        /* If thread count is bigger than 50, Join other thread by their enter order. */
        if (i > 50)
        {
            i = 0;
            while(i < 50)
            {
                pthread_join(dealthreads[i++],NULL);
            }
            i = 0;
        }
    }
    return 0;
}

