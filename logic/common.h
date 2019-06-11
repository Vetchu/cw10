//
// Created by vetch on 02.06.19.
//

#ifndef CW10_COMMON_H
#define CW10_COMMON_H

#include "../utils/hashmap.h"

#define MSG_SIZE 500
#define PORT 5000
#define PLACETAKEN ""
int bufferSize = 60000;
//#ifdef DGRAM
#define CONN_MODE SOCK_DGRAM
//#else
//#define CONN_MODE SOCK_STREAM
//#endif
enum connectType {
    NETWORK,
    UNIX
};

double pobierz_sekundy() {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return ts.tv_sec + 1e-9 * ts.tv_nsec;
}

void die(char *message) {
    perror(message);
    exit(EXIT_FAILURE);
}

int parse_port(char *portstring) {
    if (portstring == NULL)
        die("BAD PORT");

    int port = strtol(portstring, NULL, 10);

    if (port == 0)
        die("BAD PORT NUM");

    return port;
}

struct q_node {
    char *content;
    struct q_node *next;
};


struct q_node* init_queue() {
    struct q_node *q = calloc(1, sizeof(struct q_node));
    q->content = NULL;
    q->next = NULL;
    return q;
}

char *dequeue(struct q_node *q) {
    struct q_node *tmp = q->next;
    q->next = tmp->next;
    char *content = tmp->content;
    free(tmp);
    return content;
}

void enqueue(struct q_node *q, char *content) {
    struct q_node *tmp = q;
    while (tmp->next != NULL) {
        tmp = tmp->next;
    }
    struct q_node *newNode = calloc(1, sizeof(struct q_node));
    newNode->next = NULL;
    newNode->content = content;
    tmp->next = newNode;
}


#endif //CW10_COMMON_H
