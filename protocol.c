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


int getfieldcount(handle_t*);
int validate_msg(msg_t*, int);
void append(handle_t*, msg_t*, int);
int p_recv(handle_t*, msg_t*);
int check_msg(msg_t*, char*,  int);
int valid_move(msg_t*, char*, char);

void remove_whitespace(handle_t *h){
    int count = 0;
    int i = 0;
    while(i < h->len){
        if(h->buf[i] == '|'){
            count++;
        }
        if(count == 2){
            break;
        }
        if(h->buf[i] == ' '){
            memmove(h->buf + i, h->buf + i + 1, h->len - i);
            h->len--;
            continue;
        }
        i++;
    }
}

int p_recv(handle_t *h, msg_t *msg){
    int bytes;
    int fieldcount = 0;
    int i = 0;
    
    while(h->len < BUFLEN){
        remove_whitespace(h);
        if(h->len >= 4){
            if(fieldcount == 0){
                fieldcount = getfieldcount(h);
                if(fieldcount == -1){
                    return -1;
                }
            }
            int count = 0;
            for(int i = 0; i < h->len; i++){
                if(h->buf[i] == '|'){
                    count++;
                }
            }
            if(count >= fieldcount){
                //incomplete = 0;
                break;
            }
            if(count >= 2){
                char* temp = malloc(sizeof(char));
                int ptr = 5;
                int length = 0;
                while(h->buf[ptr] != '|'){
                    temp = realloc(temp, sizeof(char) * (ptr - 4));
                    memcpy(temp+(ptr-5), &h->buf[ptr], 1);
                    ptr++;
                    length++;
                }
                temp = realloc(temp, sizeof(char) * (ptr - 3));
                temp[length] = '\0';
                if(strlen(temp) <= 3 && strlen(temp) > 0){
                    for(int i = 0; i < length; i++){
                        if(isdigit(temp[i])){
                            continue;
                        }
                        free(temp);
                        return -1;
                    }
                }
                else{
                    free(temp);
                    return -1;
                }
                int num = atoi(temp);
                if(num <= 255){
                    if(h->len-strlen(temp) - 6 > num){
                        free(temp);
                        return -1;
                    }
                }
                else{
                    free(temp);
                    return -1;
                }
                free(temp);
            }
        }
        bytes = read(h->fd, h->buf+h->len, BUFLEN-h->len);
        if(bytes == 0){
            return -2;
        }
        h->len = h->len + bytes;
    }
    //gets rid of new line characters
    i = 0;
    while(i < h->len){
        if(h->buf[i] == '\n'){
            memmove(h->buf + i, h->buf + i + 1, h->len - i);
            h->len--;
            continue;
        }
        i++;
    }
    //puts("Reach here? Protocol - after loop");
    append(h, msg, fieldcount);
    return validate_msg(msg, fieldcount);
}

int validate_msg(msg_t *msg, int fields){
    int valid = 1;
    if(msg->str[4] == '|' && msg->str[5] != '|'){
        int ptr = 5;
        int length = 0;
        char* temp = malloc(sizeof(char));
        while(msg->str[ptr] != '|'){
            temp = realloc(temp, sizeof(char) * (ptr - 4));
            memcpy(temp+(ptr-5), &msg->str[ptr], 1);
            ptr++;
            length++;
        }
        msg->pos = ptr;
        temp = realloc(temp, sizeof(char) * (ptr - 3));
        temp[length] = '\0';
        if(length <= 3 && length > 0){
            for(int i = 0; i < length; i++){
                if(isdigit(temp[i])){
                    continue;
                }
                free(temp);
                valid = -1;
                break;
            }
        }
        else{
            free(temp);
            valid = -1;
        }
        if(valid == 1){
            int num = atoi(temp);
            msg->field_len = num;
            free(temp);
            if(num <= 255){
                if(msg->str[ptr + num] != '|' || msg->len != ptr+num+1){
                    //printf("%c, %d, %d\n", msg->str[ptr+num], msg->len, ptr+num+1);
                    //printf("Msg length does not match specified!\n");
                    valid = -1;
                }
            }
            else{
                //printf("Msg length is too long. Max 255\n");
                valid = -1;
            }
        }
    
    }
    else{
        valid = -1;
    }
    return valid;
}


//1 is X, 2 is O
int check_msg(msg_t* msg, char* gameboard, int symbol){
    if((strncmp(msg->str, "PLAY", 4) == 0)){
        if(msg->field_len-1 > MAX_NAME){
            return -1;
        }
        return 1;
    }
    else if(strncmp(msg->str, "MOVE", 4) == 0){
        return valid_move(msg, gameboard, symbol);
    }
    else if((strncmp(msg->str, "DRAW", 4) == 0)){
        return 1;
    }
    return -1;
}

int valid_move(msg_t *msg, char* gameboard, char symbol){
    if(msg->str[5] != '6'){
        return 0;
    }
    if(symbol == 'X' && msg->str[7] != 'X'){
        return 0;
    }
    else if(symbol == 'O' && msg->str[7] != 'O'){
        return 0;
    }
    if((msg->str[9] < '1' || msg->str[9] > '3')){
        return 0;
    }
    if(msg->str[10] != ','){
        return 0;
    }
    if((msg->str[11] < '1' || msg->str[11] > '3')){
        return 0;
    }
    int first = msg->str[9] - '0';
    int second = msg->str[11] - '0';
    int pos = ((first - 1) * 3) + (second - 1);
    if(gameboard[pos] == 'O' || gameboard[pos] == 'X'){
        return -1;
    }
    else{
        gameboard[pos] = symbol;
    }
    return 1;
}

int valid_draw(msg_t *msg){
    if(msg->str[5] != '2'){
        return 0;
    }
    if(msg->str[7] == 'A' || msg->str[7] == 'R' || msg->str[7] == 'S'){
        return 1;
    }
    return 0;
}

int check_board(char* gameboard, char symbol){
    if(gameboard[0] == symbol && gameboard[1] == symbol && gameboard[2] == symbol){
        return 1;
    }
    else if(gameboard[3] == symbol && gameboard[4] == symbol && gameboard[5] == symbol){
        return 1;
    }
    else if(gameboard[6] == symbol && gameboard[7] == symbol && gameboard[8] == symbol){
        return 1;
    }
    else if(gameboard[0] == symbol && gameboard[3] == symbol && gameboard[6] == symbol){
        return 1;
    }
    else if(gameboard[1] == symbol && gameboard[4] == symbol && gameboard[7] == symbol){
        return 1;
    }
    else if(gameboard[2] == symbol && gameboard[5] == symbol && gameboard[8] == symbol){
        return 1;
    }
    else if(gameboard[0] == symbol && gameboard[4] == symbol && gameboard[8] == symbol){
        return 1;
    }
    else if(gameboard[2] == symbol && gameboard[4] == symbol && gameboard[6] == symbol){
        return 1;
    }
    return 0;
}

void append(handle_t *h, msg_t *msg, int fields){
    int fieldCount = 0;
    int leftover = h->len;
    for(int i = 0; i < h->len; i++){
        leftover--;
        if(h->buf[i] == '\n'){
            continue;
        }
        if (h->buf[i] == '|'){
            fieldCount++;
        }
        memcpy(msg->str + msg->len, h->buf + i, 1);
        msg->len++;
        if(fieldCount == fields){
            break;
        }
    }
    memmove(h->buf, h->buf + h->len - leftover, leftover);
    h->len = leftover;
}

int getfieldcount(handle_t *h){
    if((strncmp(h->buf, "WAIT", 4) == 0) || (strncmp(h->buf, "RSGN", 4) == 0)){
        return 2;
    }
    else if ((strncmp(h->buf, "PLAY", 4) == 0) || (strncmp(h->buf, "INVL", 4) == 0) || (strncmp(h->buf, "DRAW", 4) == 0)){
        return 3;
    }
    else if((strncmp(h->buf, "MOVE", 4) == 0) || (strncmp(h->buf, "OVER", 4) == 0) || strncmp(h->buf, "BEGN", 4) == 0){
        return 4;
    }
    else if((strncmp(h->buf, "MOVD", 4) == 0)){
        return 5;
    }
    else{
        return -1;
    }
}