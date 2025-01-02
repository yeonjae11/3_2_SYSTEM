/*---------------------------------------------------------------------------*/
/* server.c                                                                  */
/* Author: Junghan Yoon, KyoungSoo Park                                      */
/* Modified by: Yeonjae Kim                                                  */
/*---------------------------------------------------------------------------*/
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <sys/time.h>
#include <netdb.h>
#include <fcntl.h>
#include "common.h"
#include "skvslib.h"
/*---------------------------------------------------------------------------*/
struct thread_args
{
    int listenfd;
    int idx;
    struct skvs_ctx *ctx;

/*---------------------------------------------------------------------------*/
    /* free to use */

/*---------------------------------------------------------------------------*/
};
/*---------------------------------------------------------------------------*/
volatile static sig_atomic_t g_shutdown = 0;
/*---------------------------------------------------------------------------*/
void *handle_client(void *arg)
{
    TRACE_PRINT();
    struct thread_args *args = (struct thread_args *)arg;
    struct skvs_ctx *ctx = args->ctx;
    int idx = args->idx;
    int listenfd = args->listenfd;
/*---------------------------------------------------------------------------*/
    /* free to declare any variables */
    int clientfd;
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];
    const char *response;
    ssize_t bytes_received;
    size_t total_sent;
    size_t response_len;
    ssize_t bytes_sent;
    int isFree;
    int flags;
    

/*---------------------------------------------------------------------------*/

    free(args);
    printf("%dth worker ready\n", idx);

/*---------------------------------------------------------------------------*/
    /* edit here */
    while (!g_shutdown)
    {
        clientfd = accept(listenfd, (struct sockaddr *)&client_addr, &client_len);
        if (clientfd < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                usleep(100);
                continue;
            }
            if (errno == EINTR && g_shutdown)
            {
                printf("shutdown\n");
                break;
            }
            perror("accept");
            continue;
        }

        flags = fcntl(clientfd, F_GETFL, 0);
        if (flags < 0) {
            perror("fcntl(F_GETFL)");
        } else if (fcntl(clientfd, F_SETFL, flags | O_NONBLOCK) < 0) {
            perror("fcntl(F_SETFL)");
        }

        while (!g_shutdown) {
            isFree = 0;
            bytes_received = read(clientfd, buffer, sizeof(buffer) - 1);
            if (bytes_received < 0) {
                if (errno == EINTR) {
                    if (g_shutdown) {
                        close(clientfd);
                        break;
                    }
                    continue;
                }
                if(errno == EAGAIN || errno == EWOULDBLOCK){
                    usleep(100);
                    continue;
                }
                perror("read");
                break;
            }
            if (bytes_received == 0) {
                printf("Connection closed by client\n");
                break;
            }

            if (bytes_received == 1 && buffer[0] == '\n') {
                printf("Connection closed by client\n");
                break;
            }

            buffer[bytes_received] = '\0';

            response = skvs_serve(ctx, buffer, bytes_received, &isFree);
            
            if (response != NULL) {
                total_sent = 0;
                response_len = strlen(response);

                while (total_sent < response_len && !g_shutdown) {
                    bytes_sent = write(clientfd, response + total_sent, 
                                     response_len - total_sent);
                    if (bytes_sent < 0) {
                        if (errno == EINTR) {
                            if (g_shutdown) break;
                            continue;
                        }
                        perror("write");
                        break;
                    }
                    total_sent += bytes_sent;
                }

                if(isFree == 1){
                    free((void*)response);
                }
            }
        }
        close(clientfd);
    }
    
/*---------------------------------------------------------------------------*/

    return NULL;
}
/*---------------------------------------------------------------------------*/
/* Signal handler for SIGINT */
void handle_sigint(int sig)
{
    TRACE_PRINT();
    printf("\nReceived SIGINT, initiating shutdown...\n");
    g_shutdown = 1;
}
/*---------------------------------------------------------------------------*/
int main(int argc, char *argv[])
{
    size_t hash_size = DEFAULT_HASH_SIZE;
    char *ip = DEFAULT_ANY_IP;
    int port = DEFAULT_PORT, opt;
    int num_threads = NUM_THREADS;
    int delay = RWLOCK_DELAY;
/*---------------------------------------------------------------------------*/
    /* free to declare any variables */
    int s, res;
    struct addrinfo hints, *ai, *ai_it;
    struct thread_args* args;
    pthread_t tid[num_threads];
    struct skvs_ctx *global_ctx;
    char port_str[6];
    int flags, opt_val;
/*---------------------------------------------------------------------------*/

    /* parse command line options */
    while ((opt = getopt(argc, argv, "p:t:s:d:h")) != -1)
    {
        switch (opt)
        {
        case 'p':
            port = atoi(optarg);
            break;
        case 't':
            num_threads = atoi(optarg);
            break;
        case 's':
            hash_size = atoi(optarg);
            if (hash_size <= 0)
            {
                perror("Invalid hash size");
                exit(EXIT_FAILURE);
            }
            break;
        case 'd':
            delay = atoi(optarg);
            break;
        case 'h':
        default:
            printf("Usage: %s [-p port (%d)] "
                   "[-t num_threads (%d)] "
                   "[-d rwlock_delay (%d)] "
                   "[-s hash_size (%d)]\n",
                   argv[0],
                   DEFAULT_PORT,
                   NUM_THREADS,
                   RWLOCK_DELAY,
                   DEFAULT_HASH_SIZE);
            exit(EXIT_FAILURE);
        }
    }

/*---------------------------------------------------------------------------*/
    /* edit here */
    global_ctx = skvs_init(hash_size, delay);
    if(signal(SIGINT,handle_sigint) == SIG_ERR){
        fprintf(stderr,"sig error\n");
        exit(EXIT_FAILURE);
    }
    memset(&hints,0,sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_flags = AI_PASSIVE | AI_ADDRCONFIG;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = 0;

    snprintf(port_str, sizeof(port_str), "%d", port);

    res = getaddrinfo(ip, port_str, &hints, &ai);
    if (res != 0) {
        perror("getaddrinfo");
        exit(EXIT_FAILURE);
    }
    
    s = -1;
    ai_it = ai;
    while(ai_it){
        s = socket(ai_it->ai_family, ai_it->ai_socktype, ai_it->ai_protocol);
        if (s < 0) {
            ai_it = ai_it->ai_next;
            continue;
        }

        flags = fcntl(s, F_GETFL, 0);
        if (flags < 0) {
            close(s);
            ai_it = ai_it->ai_next;
            continue;
        }

        if (fcntl(s, F_SETFL, flags | O_NONBLOCK) < 0) {
            perror("fcntl(F_SETFL)");
            exit(EXIT_FAILURE);
        }

        opt_val = 1;
        if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt_val, sizeof(opt_val)) < 0) {
            close(s);
            ai_it = ai_it->ai_next;
            continue;
        }

        if (bind(s, ai_it->ai_addr, ai_it->ai_addrlen) < 0) {
            close(s);
            ai_it = ai_it->ai_next;
            continue;
        }
        break;
    }

    if (ai_it == NULL) {
        fprintf(stderr, "Could not bind to any address\n");
        freeaddrinfo(ai);
        exit(EXIT_FAILURE);
    }

    if (listen(s, NUM_BACKLOG) < 0) {
        perror("listen");
        close(s);
        freeaddrinfo(ai);
        exit(EXIT_FAILURE);
    }
    freeaddrinfo(ai);
    
    for(int i = 0; i < num_threads; i++){
        args = (struct thread_args *)malloc(sizeof(struct thread_args));
        args->listenfd = s;
        args->idx = i;
        args->ctx = global_ctx;
        if (pthread_create(&tid[i], NULL, handle_client, args) != 0) {
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
    }
    while(g_shutdown==0) sleep(1);
    printf("Shutting down server...\n");
    for(int i = 0 ;i<num_threads; i++){
        pthread_join(tid[i],NULL);
    }
    close(s);
    skvs_destroy(global_ctx,1);

    
/*---------------------------------------------------------------------------*/

    return 0;
}
