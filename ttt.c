#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>
#include <ctype.h> 


#include "protocol.h"

int connect_inet(char *host, char *service)
{
    struct addrinfo hints, *info_list, *info;
    int sock, error;

    // look up remote host
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_UNSPEC;  // in practice, this means give us IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // indicate we want a streaming socket

    error = getaddrinfo(host, service, &hints, &info_list);
    if (error) {
        fprintf(stderr, "error looking up %s:%s: %s\n", host, service, gai_strerror(error));
        return -1;
    }

    for (info = info_list; info != NULL; info = info->ai_next) {
        sock = socket(info->ai_family, info->ai_socktype, info->ai_protocol);
        if (sock < 0) continue;

        error = connect(sock, info->ai_addr, info->ai_addrlen);
        if (error) {
            close(sock);
            continue;
        }

        break;
    }
    freeaddrinfo(info_list);

    if (info == NULL) {
        fprintf(stderr, "Unable to connect to %s:%s\n", host, service);
        return -1;
    }

    return sock;
}

int main(int argc, char **argv)
{
    if (argc != 3) {
        printf("Specify host and service\n");
        exit(EXIT_FAILURE);
    }
    char buf[BUFLEN];

    handle_t *input = malloc(sizeof(handle_t));
    input->fd = STDIN_FILENO;
    input->buf = malloc(sizeof(char) * BUFLEN);
    input->len = 0;

    handle_t *con = malloc(sizeof(handle_t));
    con->fd = connect_inet(argv[1], argv[2]);
    con->buf = malloc(sizeof(char) * BUFLEN);
    con->len = 0;
    if (con->fd < 0){ 
        free(con);
        exit(EXIT_FAILURE);
    }
    int check;
    while(1){
        msg_t *msg = malloc(sizeof(msg_t));
        msg->len = 0;
        msg->str = malloc(sizeof(char) * BUFLEN);
        
        int bytes = read(STDIN_FILENO, buf, BUFLEN);    
        bytes = write(con->fd, buf, bytes);
        if(bytes <= 0){
            free(msg->str);
            free(msg);
            printf("Write error\n");
            break;
        }
        free(msg->str);
        free(msg);

        // check = p_recv(input, msg);
        // if(check == -1){
        //     free(msg->str);
        //     free(msg);
        //     printf("Very bad input!\n");
        //     con->len = 0;
        //     break;
        //     // exit(EXIT_FAILURE);
        // }

        //int bytes = write(con->fd, msg->str, msg->len);
        if(bytes <= 0){
            free(msg->str);
            free(msg);
            printf("Write error\n");
            exit(EXIT_FAILURE);
        }

        msg = malloc(sizeof(msg_t));
        msg->len = 0;
        msg->str = malloc(sizeof(char) * BUFLEN);

        if(p_recv(con, msg) == -1){
            free(msg->str);
            free(msg);
            break;
        }
        msg->str[msg->len] = '\0';
        printf("Message from server: %s\n", msg->str);
        if(strncmp(msg->str, "WAIT", 4) == 0){
            free(msg->str);
            free(msg);
            break;
        }
        else if(strncmp(msg->str, "INVL", 4) == 0 && msg->str[msg->pos + 1] == '!'){
            free(msg->str);
            free(msg);
            exit(EXIT_FAILURE);
        }
        free(msg->str);
        free(msg);
    }
    msg_t *msg = malloc(sizeof(msg_t));
    msg->len = 0;
    msg->str = malloc(sizeof(char) * BUFLEN);
    check = p_recv(con, msg);

    if(check == -1){
        printf("Received malformed message!\n");
        free(msg->str);
        free(msg);
        close(con->fd);
        exit(EXIT_FAILURE);
    }

    msg->str[msg->len] = '\0';
    printf("Message from server (BEGIN): %s\n", msg->str);
    
    int wait = 0;
    int counter = 0;
    for(int i = 0; i < msg->len; i++){
        if(msg->str[i] == '|'){
            counter++;
        }
        if(counter == 2){
            if(msg->str[i+1] == 'O'){
                wait = 1;
                break;
            }
            else{
                break;
            }
        }
    }
    free(msg->str);
    free(msg);


    while (1) {        
        msg_t *msg = malloc(sizeof(msg_t));
        msg->len = 0;
        msg->str = malloc(sizeof(char) * BUFLEN);

        if(wait == 0){
            int bytes = read(STDIN_FILENO, buf, BUFLEN);
            // if(p_recv(input, msg) == -1){
            //     free(msg->str);
            //     free(msg);
            //     printf("Very bad input!");
            //     break;
            // };


            // buf[bytes] = '\0';
            //printf("Input: %s | Bytes: %d\n", buf, bytes);
        
            bytes = write(con->fd, buf, bytes);
            if(bytes <= 0){
                free(msg->str);
                free(msg);
                printf("Write error\n");
                break;
            }
            free(msg->str);
            free(msg);

            msg = malloc(sizeof(msg_t));
            msg->len = 0;
            msg->str = malloc(sizeof(char) * BUFLEN);

            if(p_recv(con, msg) == -1){
                free(msg->str);
                free(msg);
                break;
            }
            
            msg->str[msg->len] = '\0';
            printf("Message from server (Active): %s\n", msg->str);

            if(strncmp(msg->str, "OVER", 4) == 0){
                free(msg->str);
                free(msg);
                break;
            }
            else if(strncmp(msg->str, "INVL", 4) == 0){
                if(msg->str[msg->pos + 1] != '!'){
                    free(msg->str);
                    free(msg);
                    continue;
                }
                else{
                    free(msg->str);
                    free(msg);
                    break;
                }
            }
            else if(strncmp(msg->str, "DRAW", 4) == 0){
                free(msg->str);
                free(msg);
                continue;
            }
        }
        else{
            wait = 0;
        }
        
        free(msg->str);
        free(msg);

        msg = malloc(sizeof(msg_t));
        msg->len = 0;
        msg->str = malloc(sizeof(char) * BUFLEN);

        if(p_recv(con, msg) == -1){
            free(msg->str);
            free(msg);
            break;
        }
        
        msg->str[msg->len] = '\0';
        printf("Message from server (Waiting): %s\n", msg->str);
        if(strncmp(msg->str, "OVER", 4) == 0){
            free(msg->str);
            free(msg);
            break;
        }
        // else if(strncmp(msg->str, "DRAW", 4) == 0){
        //     // wait = 1;
        // }
        free(msg->str);
        free(msg);
        // FIXME: should check whether the write succeeded!
    }
    close(con->fd);
    free(con->buf);
    free(con);

    free(input->buf);
    free(input);

    return EXIT_SUCCESS;
}