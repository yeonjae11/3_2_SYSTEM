/*---------------------------------------------------------------------------*/
/* client.c                                                                  */
/* Author: Junghan Yoon, KyoungSoo Park                                      */
/* Modified by: Yeonjae Kim                                                  */
/*---------------------------------------------------------------------------*/
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <getopt.h>
#include <errno.h>
#include "common.h"
/*---------------------------------------------------------------------------*/
int main(int argc, char *argv[])
{
    char *ip = DEFAULT_LOOPBACK_IP;
    int port = DEFAULT_PORT;
    int interactive = 0; /* Default is non-interactive mode */
    int opt;

/*---------------------------------------------------------------------------*/
    /* free to declare any variables */
    struct addrinfo hints, *ai, *ai_it;
    int res, s, len;
    char buffer[BUFFER_SIZE];
    char port_str[6];
    
/*---------------------------------------------------------------------------*/

    /* parse command line options */
    while ((opt = getopt(argc, argv, "i:p:th")) != -1)
    {
        switch (opt)
        {
        case 'i':
            ip = optarg;
            break;
        case 'p':
            port = atoi(optarg);
            if (port <= 1024 || port >= 65536)
            {
                perror("Invalid port number");
                exit(EXIT_FAILURE);
            }
            break;
        case 't':
            interactive = 1;
            break;
        case 'h':
        default:
            printf("Usage: %s [-i server_ip_or_domain (%s)] "
                   "[-p port (%d)] [-t]\n",
                   argv[0],
                   DEFAULT_LOOPBACK_IP, 
                   DEFAULT_PORT);
            exit(EXIT_FAILURE);
        }
    }

/*---------------------------------------------------------------------------*/
    /* edit here */
    if((s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0){
        perror(strerror(errno));
        exit(EXIT_FAILURE);
    }
    memset(&hints,0,sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_ADDRCONFIG;
    hints.ai_protocol = 0;

    snprintf(port_str, sizeof(port_str), "%d", port);

    if(interactive){
        //null
    }

    res = getaddrinfo(ip, port_str, &hints, &ai);
    if (res != 0)
    {
        fprintf(stderr, "%s\n", gai_strerror(res));
        exit(EXIT_FAILURE);
    }


    if(res != 0) {
        fprintf(stderr,"%s\n",gai_strerror(res));
        exit(EXIT_FAILURE);
    }

    ai_it = ai;
    while(ai_it){
        if(connect(s, ai_it->ai_addr, ai_it->ai_addrlen) == 0)
            break;
        ai_it = ai_it->ai_next;
    }
    freeaddrinfo(ai);

    while(fgets(buffer,sizeof(buffer),stdin) != NULL){
        len = strlen(buffer);
        if((res = write(s, buffer, len)) <= 0){
            perror("write");
            exit(EXIT_FAILURE);
        }
        if(res == 1 && buffer[0] == '\n'){
            break;
        }
        if((res = read(s, buffer, sizeof(buffer)-1)) <= 0){
            fprintf(stderr,"Connection closed by server\n");
            exit(EXIT_FAILURE);
        }
        buffer[res] = 0;
        printf("%s\n",buffer);
    }
    close(s);

/*---------------------------------------------------------------------------*/

    return 0;
}
