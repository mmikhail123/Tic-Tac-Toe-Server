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

#include "protocol.h"

#define QUEUE_SIZE 128

struct connection_data {
    struct sockaddr_storage addr;
    char* client_name;
    socklen_t addr_len;
    handle_t *h;
};

struct gameboard{
    struct connection_data *p1;
    struct connection_data *p2;
    char* board;
}typedef game;

struct player_list{
    char** players;
    int counter;
    pthread_mutex_t lock;
}typedef active;

volatile int temp = 1;

active active_players;

void broken_connection(handle_t *h, char* name){
    printf("%s has disconnected from the server\n", name);
    char* brcon_msg = malloc(sizeof(char) * BUFLEN);
    sprintf(brcon_msg, "OVER|%ld|W|%s disconnected from the game.|", 31 + strlen(name), name);
    write(h->fd, brcon_msg, strlen(brcon_msg));
    free(brcon_msg);
}

void free_game(game *g){
    close(g->p1->h->fd);
    close(g->p2->h->fd);
    free(g->p1->client_name);
    free(g->p2->client_name);
    free(g->p1->h->buf);
    free(g->p2->h->buf);
    free(g->p1->h);
    free(g->p2->h);
    free(g->p1);
    free(g->p2);
    free(g->board);
    free(g);
}

void print_list(active *list){
    for(int i = 0; i < list->counter; i++){
        printf("%s\n", list->players[i]);
    }
}

int check_dup_name(active *list, char* name){
    for(int i = 0; i < list->counter; i++){
        if(strcmp(list->players[i], name) == 0){
            return i;
        }
    }
    return -1;
}

int add_player(active *list, char* name){
    pthread_mutex_lock(&list->lock);
    int check = 1;
    if(check_dup_name(list, name) >= 0){
        check = 0;
    }
    else{
        list->players = realloc(list->players, sizeof(char *) * (list->counter + 1));
        list->players[list->counter] = malloc(sizeof(char) * (strlen(name) + 1));
        memcpy(list->players[list->counter], name, (strlen(name) + 1));
        list->counter++;
    }
    pthread_mutex_unlock(&list->lock);
    return check;
}

void remove_player(active *list, char* name){
    pthread_mutex_lock(&list->lock);
    int pos = check_dup_name(list, name);
    if(pos >= 0){
        free(list->players[pos]);
        for(int i = 0; i < list->counter - pos - 1; i++){
            list->players[pos + i] = list->players[pos + i + 1];
        }
        list->counter--;
        list->players[list->counter] = NULL;
        list->players = realloc(list->players, sizeof(char *) * (list->counter));
    }
    pthread_mutex_unlock(&list->lock);
}

char* get_name(handle_t *h, active *p){
    char message1[] = "INVL|39|Bad Input: First message must be PLAY.|";
    char message2[] = "INVL|59|Bad Input: Name must not be more than 100 characters long.|";
    char message3[] = "WAIT|0|";

    char* name = malloc(sizeof(char) * MAX_NAME + 1);

    int error;
    while(1){
        msg_t *msg = malloc(sizeof(msg_t));
        msg->len = 0;
        msg->str = malloc(sizeof(char) * BUFLEN);
        error = p_recv(h, msg);
        if(error == -1){
            free(msg->str);
            free(msg);
            char* inv_msg = malloc(sizeof(char) * BUFLEN);
            sprintf(inv_msg, "INVL|79|!Invalid message: Message format does not conform with required specifications|");
            write(h->fd, inv_msg, strlen(inv_msg));
            free(inv_msg);
            free(name);
            return NULL;
        }
        else if(error == -2){
            puts("Connection broken");
            free(msg->str);
            free(msg);
            free(name);
            return NULL;
        }
        else{
            if(strncmp(msg->str, "PLAY", 4) != 0){
                //printf("%s\n", message1);
                if(write(h->fd, message1, strlen(message1)) < 0){
                    free(name);
                    return NULL;
                }
            }
            else if(msg->field_len-1 > MAX_NAME){
                //printf("%s\n", message2);
                if(write(h->fd, message2, strlen(message2)) < 0){
                    free(name);
                    return NULL;
                }
            }
            else{
                memcpy(name, &msg->str[msg->pos + 1], msg->field_len-1);
                name[msg->field_len-1] = '\0';
                if(add_player(p, name)){
                    //printf("%s\n", message3);
                    free(msg->str);
                    free(msg);
                    if(write(h->fd, message3, strlen(message3)) < 0){
                        free(name);
                        return NULL;
                    }
                    break;
                }
                else{
                    char* invalid = malloc(sizeof(char) * BUFLEN);
                    sprintf(invalid, "INVL|%ld|Error: %s is already used by another player!|", strlen(name) + 43, name);
                    if(write(h->fd, invalid, strlen(invalid)) < 0){
                        free(name);
                        return NULL;
                    }
                    free(invalid);
                }
            }
        }
        free(msg->str);
        free(msg);
    }
    return name;
}

void free_playerlist(active * list){
    for(int i = 0; i < list->counter; i++){
        free(list->players[i]);
    }
    free(list->players);
    //free(list);
}

// void * handle_connection(void* p_client_socket);

void handler(int signum)
{
    temp = 0;
}

// set up signal handlers for primary thread
// return a mask blocking those signals for worker threads
// FIXME should check whether any of these actually succeeded
void install_handlers(sigset_t *mask)
{
    struct sigaction act;
    act.sa_handler = handler;
    act.sa_flags = 0;
    signal(SIGPIPE, SIG_IGN);
    sigemptyset(&act.sa_mask);
    sigaction(SIGINT, &act, NULL);
    sigaction(SIGTERM, &act, NULL);
    sigemptyset(mask);
    sigaddset(mask, SIGINT);
    sigaddset(mask, SIGTERM);
}

int open_listener(char *service, int queue_size)
{
    struct addrinfo hint, *info_list, *info;
    int error, sock;
    // initialize hints
    memset(&hint, 0, sizeof(struct addrinfo));
    hint.ai_family = AF_UNSPEC;
    hint.ai_socktype = SOCK_STREAM;
    hint.ai_flags = AI_PASSIVE;

    error = getaddrinfo(NULL, service, &hint, &info_list);
    if (error) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(error));
        return -1;
    }

    for (info = info_list; info != NULL; info = info->ai_next) {
        sock = socket(info->ai_family, info->ai_socktype, info->ai_protocol);

        if (sock == -1) continue;

        error = bind(sock, info->ai_addr, info->ai_addrlen);
        if (error) {
            close(sock);
            continue;
        }

        error = listen(sock, queue_size);
        if (error) {
            close(sock);
            continue;
        }

        break;
    }
    freeaddrinfo(info_list);

    if (info == NULL) {
        fprintf(stderr, "Error: Could not bind\n");
        return -1;
    }
    return sock;
}

void bad_input(handle_t *curr_p, handle_t *waiting_p){
    char* inv_msg = malloc(sizeof(char) * BUFLEN);
    sprintf(inv_msg, "INVL|79|!Invalid message: Message format does not conform with required specifications|");
    write(curr_p->fd, inv_msg, strlen(inv_msg));
    free(inv_msg);
    
    char* over_msg = malloc(sizeof(char) * BUFLEN);
    sprintf(over_msg, "OVER|64|W|Opponent sent a invalid message! Therefore you win by default|");
    write(waiting_p->fd, over_msg, strlen(over_msg));
    free(over_msg);
}

#define HOSTSIZE 100
#define PORTSIZE 10
void *start_game(void *arg){
    game *test = arg;

    char host[HOSTSIZE], port[PORTSIZE];
    int error = getnameinfo((struct sockaddr *)&test->p1->addr, test->p1->addr_len, host, HOSTSIZE, port, PORTSIZE, NI_NUMERICSERV);
    if (error) {
        fprintf(stderr, "getnameinfo: %s\n", gai_strerror(error));
        strcpy(host, "??");
        strcpy(port, "??");
    }
    printf("%s Connection from %s:%s\n", test->p1->client_name, host, port);

    error = getnameinfo((struct sockaddr *)&test->p2->addr, test->p2->addr_len, host, HOSTSIZE, port, PORTSIZE, NI_NUMERICSERV);

    if (error) {
        fprintf(stderr, "getnameinfo: %s\n", gai_strerror(error));
        strcpy(host, "??");
        strcpy(port, "??");
    }
     printf("%s Connection from %s:%s\n", test->p2->client_name, host, port);   

    test->board = malloc(sizeof(char) * 10);
    for(int i = 0; i < 9; i++){
        test->board[i] = '.';
    }
    test->board[9] = '\0';

    active *p = &active_players;
    int disconnected = 0;
    char msg[BUFLEN];
    sprintf(msg, "BEGN|%ld|X|%s|", strlen(test->p2->client_name) + 3, test->p2->client_name);
    if(write(test->p1->h->fd, msg, strlen(msg)) < 0){
        broken_connection(test->p2->h, test->p1->client_name);
        disconnected = 1;
    }
    
    sprintf(msg, "BEGN|%ld|O|%s|", strlen(test->p1->client_name) + 3, test->p1->client_name);
    if(write(test->p2->h->fd, msg, strlen(msg)) < 0){
        broken_connection(test->p1->h, test->p1->client_name);
        disconnected = 1;
    }
    

    handle_t *curr_p = test->p1->h;
    handle_t *waiting_p = test->p2->h;
    char *cp_name = test->p1->client_name;
    char *wp_name = test->p2->client_name;
    //int broke = 0;
    int turn = 0;
    char symbol = 'X';
    int drawed = 0;
    while(turn < 9 && disconnected != 1){
        msg_t *msg = malloc(sizeof(msg_t));
        msg->len = 0;
        msg->str = malloc(sizeof(char) * BUFLEN);

        error = p_recv(curr_p, msg);
        if(error == -1){
            free(msg->str);
            free(msg);
            printf("Broken message received from %s. Closing connections and ending game.\n", cp_name);
            bad_input(curr_p, waiting_p);
            //broke = 1;
            break;
        }
        else if(error == -2){
            broken_connection(waiting_p, cp_name);
            disconnected = 1;
            free(msg->str);
            free(msg);
            continue;
        }
        else if(error == 0){
            char* inv_msg = malloc(sizeof(char) * BUFLEN);
            sprintf(inv_msg, "INVL|38|Bad input: Please send a valid input.|");
            //printf("Invalid response recieved\n");
            if(write(curr_p->fd, inv_msg, strlen(inv_msg)) < 0){
                broken_connection(waiting_p, cp_name);
                disconnected = 1;
            }
            free(inv_msg);
            free(msg->str);
            free(msg);
            continue;
        }
        msg->str[msg->len] = '\0';
        printf("Message from %s: %s\n", cp_name, msg->str);
        if(strncmp(msg->str, "MOVE", 4) == 0){
            int check = valid_move(msg, test->board, symbol);
            if(check == 1){
                turn++;
                char* movd_msg = malloc(sizeof(char) * BUFLEN);
                char *temp = malloc(sizeof(char) * 10);
                memcpy(temp, &msg->str[msg->pos] + 1, msg->field_len);
                temp[5] = '\0';
                sprintf(movd_msg, "MOVD|16|%s|%s|", temp, test->board);
                free(temp);
                if(write(curr_p->fd, movd_msg, strlen(movd_msg)) < 0){
                    broken_connection(waiting_p, cp_name);
                    disconnected = 1;
                    free(msg->str);
                    free(msg);
                    free(movd_msg);
                    continue;
                }

                if(check_board(test->board, symbol)){
                    printf("Game Completed: %s has won the game!\n", cp_name);
                    char *winner = malloc(sizeof(char) * BUFLEN);
                    sprintf(winner, "OVER|%ld|W|%s has won the game!|", 19 + strlen(cp_name), cp_name);
                    write(curr_p->fd, winner, strlen(winner));
                    free(winner);
                    char *loser = malloc(sizeof(char) * BUFLEN);
                    sprintf(loser, "OVER|%ld|L|%s has won the game!|", 19 + strlen(cp_name), cp_name);
                    write(waiting_p->fd, loser, strlen(loser));
                    free(loser);
                    free(movd_msg);
                    free(msg->str);
                    free(msg);
                    break;
                }
                else{
                    if(write(waiting_p->fd, movd_msg, strlen(movd_msg)) < 0){
                        broken_connection(curr_p, wp_name);
                        disconnected = 1;
                    }
                    if(curr_p == test->p1->h){
                        cp_name = test->p2->client_name;
                        wp_name = test->p1->client_name;
                        curr_p = test->p2->h;
                        waiting_p = test->p1->h;
                        symbol = 'O';
                    }
                    else{
                        cp_name = test->p1->client_name;
                        wp_name = test->p2->client_name;
                        curr_p = test->p1->h;
                        waiting_p = test->p2->h;
                        symbol = 'X';
                    }
                    free(movd_msg);
                }
            }
            else if(check == 0){
                char *inv_move = malloc(sizeof(char) * BUFLEN);
                sprintf(inv_move,"INVL|38|Bad Input: Inputted move was invalid.|");
                if(write(curr_p->fd, inv_move, strlen(inv_move)) < 0){
                    broken_connection(waiting_p, cp_name);
                    disconnected = 1;
                }
                free(inv_move);
            }
            else{
                char *inv_move = malloc(sizeof(char) * BUFLEN);
                sprintf(inv_move,"INVL|24|That space is occupied.|");
                if(write(curr_p->fd, inv_move, strlen(inv_move)) < 0){
                    broken_connection(waiting_p, cp_name);
                    disconnected = 1;
                }
                free(inv_move);
            }
        }
        else if(strncmp(msg->str, "RSGN", 4) == 0){
            if(msg->str[5] != '0'){
                printf("Broken message received from %s. Closing connections and ending game.\n", cp_name);
                bad_input(curr_p, waiting_p);
            }
            else{
                char *rsg_msg = malloc(sizeof(char) * BUFLEN);
                sprintf(rsg_msg, "OVER|%ld|L|%s has resigned from the game!|", 31 + strlen(cp_name), cp_name);
                write(curr_p->fd, rsg_msg, strlen(rsg_msg));
                free(rsg_msg);
                rsg_msg = malloc(sizeof(char) * BUFLEN);
                sprintf(rsg_msg, "OVER|%ld|W|%s has resigned from the game!|", 31 + strlen(cp_name), cp_name);
                write(waiting_p->fd, rsg_msg, strlen(rsg_msg));
                free(rsg_msg);
                free(msg->str);
                free(msg);
                break;
            }
        }
        else if(strncmp(msg->str, "DRAW", 4) == 0){
            if(valid_draw(msg)){
                if(msg->str[7] == 'S'){
                    if(write(waiting_p->fd, msg->str, strlen(msg->str)) < 0){
                        broken_connection(curr_p, wp_name);
                        disconnected = 1;
                    }
                    else{
                        while(1){
                            msg_t *draw = malloc(sizeof(msg_t));
                            draw->len = 0;
                            draw->str = malloc(sizeof(char) * BUFLEN);
                            error = p_recv(waiting_p, draw);
                            if(error == -1){
                                printf("Broken message received from %s. Closing connections and ending game.\n", wp_name);
                                bad_input(waiting_p, curr_p);
                                //broke = 1;
                                free(draw->str);
                                free(draw);
                                break;
                            }
                            else if(error == -2){
                                free(draw->str);
                                free(draw);
                                broken_connection(curr_p, wp_name);
                                disconnected = 1;
                                break;
                            }
                            draw->str[draw->len] = '\0';
                            if(valid_draw(draw)){
                                printf("Message from %s (Draw Response): %s\n", wp_name, draw->str);
                                if(draw->str[7] == 'A'){
                                    printf("Game Completed: %s and %s have drawn the game!\n", cp_name, wp_name);
                                    char *draw_msg = malloc(sizeof(char) * BUFLEN);
                                    sprintf(draw_msg, "OVER|37|D|Both players have accepted a draw!|");
                                    write(curr_p->fd, draw_msg, strlen(draw_msg));
                                    write(waiting_p->fd, draw_msg, strlen(draw_msg));
                                    free(draw_msg);
                                    drawed = 1;
                                    free(draw->str);
                                    free(draw);
                                    break;
                                }
                                else if(draw->str[7] == 'R'){
                                    if(write(curr_p->fd, draw->str, strlen(draw->str)) < 0){
                                        broken_connection(waiting_p, cp_name);
                                        disconnected = 1;
                                    }
                                    free(draw->str);
                                    free(draw);
                                    break;
                                }
                                else{
                                    char *inv_draw = malloc(sizeof(char) * BUFLEN);
                                    sprintf(inv_draw, "INVL|41|Bad Input: Must either accept or reject.|");
                                    if(write(waiting_p->fd, inv_draw, strlen(inv_draw)) < 0){
                                        broken_connection(curr_p, wp_name);
                                        disconnected = 1;
                                    }
                                    free(inv_draw);
                                }
                            }
                            else{
                                char *inv_draw = malloc(sizeof(char) * BUFLEN);
                                sprintf(inv_draw, "INVL|43|Bad Input: Improper draw message was sent.|");
                                if(write(waiting_p->fd, inv_draw, strlen(inv_draw)) < 0){
                                    broken_connection(curr_p, wp_name);
                                    disconnected = 1;
                                    break;
                                }
                                free(inv_draw);
                            }
                            free(draw->str);
                            free(draw);
                        }
                    }
                }
                else{
                    char *inv_draw = malloc(sizeof(char) * BUFLEN);
                    sprintf(inv_draw, "INVL|83|Bad Input: Cannot accept or reject draw without first being suggested by opponent.|");
                    if(write(curr_p->fd, inv_draw, strlen(inv_draw)) < 0){
                        broken_connection(curr_p, wp_name);
                        disconnected = 1;
                    }
                    free(inv_draw);
                }
            }
            else{
                char *inv_draw = malloc(sizeof(char) * BUFLEN);
                sprintf(inv_draw, "INVL|43|Bad Input: Improper draw message was sent.|");
                if(write(curr_p->fd, inv_draw, strlen(inv_draw)) < 0){
                    broken_connection(waiting_p, cp_name);
                    disconnected = 1;
                }
                free(inv_draw);
            }
        }
        else if(strncmp(msg->str, "PLAY", 4) == 0){
            char *inv_play = malloc(sizeof(char) * BUFLEN);
            sprintf(inv_play, "INVL|55|PLAY is not a valid command after names have been set.|");
            if(write(curr_p->fd, inv_play, strlen(inv_play)) < 0){
                broken_connection(waiting_p, cp_name);
                disconnected = 1;
            }
            free(inv_play);
        }
        else{
            char* inv_com = malloc(sizeof(char) * BUFLEN);
            sprintf(inv_com, "INVL|71|Bad Input: Client must either send a PLAY, RSGN, DRAW, or MOVE command|");
            if(write(curr_p->fd, inv_com, strlen(inv_com)) < 0){
                broken_connection(waiting_p, cp_name);
                disconnected = 1;
            }
            free(inv_com);
        }
        free(msg->str);
        free(msg);
        if(drawed){
            break;
        }
    }
    if(turn == 9){
        printf("Game Completed: %s and %s have filled the board, drawn game!\n", cp_name, wp_name);
        char *draw_msg = malloc(sizeof(char) * BUFLEN);
        sprintf(draw_msg, "OVER|48|D|Board is filled with no winners. Drawed Game.|");
        write(curr_p->fd, draw_msg, strlen(draw_msg));
        write(waiting_p->fd, draw_msg, strlen(draw_msg));
        free(draw_msg);
    }
    remove_player(p, test->p1->client_name);
    remove_player(p, test->p2->client_name);
    free_game(test);
    return NULL;
}

int make_connection(struct connection_data *con, int listener){
    //p1 = (struct connection_data *) malloc(sizeof(struct connection_data));
    con->addr_len = sizeof(struct sockaddr_storage);
    con->h = malloc(sizeof(handle_t));
    con->h->fd = accept(listener, (struct sockaddr *)&con->addr, &con->addr_len);
    if (con->h->fd < 0) {
        perror("accept");
        return 0;
    }
    return 1;
}

int main(int argc, char const* argv[]){
    sigset_t mask;
    int error;
    pthread_t tid;

    char *service = argc == 2 ? argv[1] : "15000";

    install_handlers(&mask);

    int listener = open_listener(service, QUEUE_SIZE);
    if(listener < 0){
        exit(EXIT_FAILURE);
    }

    active *p = &active_players;
    p->counter = 0;
    p->players = malloc(sizeof(char*));

    printf("Listening for incoming connections on %s\n", service);

    //playercount = 0;
    while(temp){
        struct connection_data *p1 = (struct connection_data *) malloc(sizeof(struct connection_data));
        if(make_connection(p1, listener) == 0){
            free(p1->h);
            free(p1);
            continue;
        }
        puts("First person connected: Attempting to get player name.");
        p1->h->buf = malloc(sizeof(char) * BUFLEN);
        p1->h->len = 0;
        char* player_1_name = get_name(p1->h, p);
        if(player_1_name == NULL){
            puts("Broken Response received or connection lost. Closing connection.");
            free(player_1_name);
            close(p1->h->fd);
            free(p1->h->buf);
            free(p1->h);
            free(p1);
            continue;
        }
        printf("%s successfully connected, now waiting for a opponent.\n", player_1_name);
        p1->client_name = player_1_name;
        char* player_2_name;
        struct connection_data *p2;
        while(temp){
            p2 = (struct connection_data *) malloc(sizeof(struct connection_data));
            if(make_connection(p2, listener) == 0){
                free(p2->h);
                free(p2);
                continue;
            }
            puts("Second person connected: Attempting to get player name.");
            p2->h->buf = malloc(sizeof(char) * BUFLEN);
            p2->h->len = 0;
            player_2_name = get_name(p2->h, p);
            if(player_2_name == NULL){
                puts("Broken Response received or connection lost. Closing connection.");
                close(p2->h->fd);
                free(p2->h->buf);
                free(p2->h);
                free(p2);
                continue;
            }
            printf("%s successfully connected, pair formed and game can begin.\n", player_2_name);
            p2->client_name = player_2_name;
            break;
        }

        game *test = (game *) malloc(sizeof(game));
        test->p1 = p1;
        test->p2 = p2;
        error = pthread_sigmask(SIG_BLOCK, &mask, NULL);
        if (error != 0) {
        	fprintf(stderr, "sigmask: %s\n", strerror(error));
        	exit(EXIT_FAILURE);
        }

        error = pthread_create(&tid, NULL, start_game, test);
        if (error != 0) {
        	fprintf(stderr, "pthread_create: %s\n", strerror(error));
        	free(test);
        	continue;
        }
        pthread_detach(tid);

        error = pthread_sigmask(SIG_UNBLOCK, &mask, NULL);
        if (error != 0) {
        	fprintf(stderr, "sigmask: %s\n", strerror(error));
        	exit(EXIT_FAILURE);
        }
    }
}