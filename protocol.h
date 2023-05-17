#ifndef _PROTOCOL_H
#define _PROTOCOL_H

#define BUFLEN 264
#define MAX_NAME 100

typedef enum {PLAY, WAIT, BEGN, MOVE, MOVD, RSGN, DRAW, OVER, INVL} type;

struct msg{
    int len;
    char* str;
    int field_len;
    int pos;
}typedef msg_t;

struct handle{
    int fd;
    char *buf;
    int len;
}typedef handle_t;

int p_recv(handle_t *h, msg_t *msg);
//char* get_name(handle_t *h, active *p);
int valid_move(msg_t *msg, char* gameboard, char symbol);
int valid_draw(msg_t *msg);
int check_board(char*, char);

#endif