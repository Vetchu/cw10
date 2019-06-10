#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <semaphore.h>
#include <fcntl.h>
#include <signal.h>
#include "common.h"
#include "../utils/hashmap.h"

struct sockaddr_in inet_server_addr;
struct sockaddr_un unix_server_addr;
struct q_node *queue;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
sem_t *process_mutex;

char *addr_string;
socklen_t addr_size;
struct sockaddr *addr;

char *my_name;
int sock;
int flag = 1;
int flag2 =1;
char *parse_address(char *addr) {
    if (addr == NULL)
        die("BAD ADDRESS");
    return addr;
}

pthread_t pinger_thread;

int inet_connect_socket_client(char *address, int port) {
    int sock = socket(AF_INET, CONN_MODE, 0);
    inet_server_addr.sin_family = AF_INET;
    inet_server_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, address, &inet_server_addr.sin_addr) == -1)
        die("BAD ADDRESS GIVEN - PARSE ERROR");

    int res = connect(sock, (struct sockaddr *) &inet_server_addr, sizeof(inet_server_addr));

    if (res < 0)
        die("inet connect");

    return sock;
}

int unix_connect_socket_client(char *path) {
    int sock = socket(AF_UNIX, CONN_MODE, 0);
    unix_server_addr.sun_family = AF_UNIX;
    strcpy(unix_server_addr.sun_path, path);

    int res = connect(sock, (struct sockaddr *) &unix_server_addr, sizeof(unix_server_addr));

    if (res < 0)
        die("unix connect");
    unix_server_addr.sun_family = AF_UNIX;
    strcpy(unix_server_addr.sun_path, my_name);
    unlink(my_name);
    res = bind(sock, (struct sockaddr *) &unix_server_addr, sizeof(unix_server_addr));
    if (res < 0)
        die("unix bind");
    return sock;
}

void unregister_me() {
    char buf[bufferSize];
    strcpy(buf, "UNREGISTER|");
    strcat(buf, my_name);
    socklen_t size = sizeof(inet_server_addr);

    int ret;
    ret = sendto(sock, buf, sizeof(buf), 0, (struct sockaddr *) &inet_server_addr, size);
    printf("Unregistered\n");

    pthread_cancel(pinger_thread);
    pthread_join(pinger_thread, NULL);
    if (CONN_MODE == SOCK_DGRAM)
        close(sock);
    sem_close(process_mutex);
}

void *process() {
    while (flag) {
        sem_wait(process_mutex);
        char *buf = dequeue(queue);
        char *counter = strtok(buf, " ");
        char *result = parseText(strtok(NULL, "|"));
        if(result==NULL) continue;
        char buff[strlen(result) + strlen(my_name) + 10];
        memset(buff, 0, sizeof(buff));
        strcpy(buff, "RESULTS|");
        strcat(buff, my_name);
        strcat(buff, "|");
        strcat(buff, counter);
        strcat(buff, "|");
        strcat(buff, result);
        int sent = 0;
        if(flag2)
        pthread_mutex_lock(&mutex);
        if (CONN_MODE == SOCK_DGRAM) {
            sent = sendto(sock, buff, sizeof(buff), 0, NULL,0);
        } else {
            sent = send(sock, buff, sizeof(buff), 0);
        }
        if(flag2)
        pthread_mutex_unlock(&mutex);
        if (sent < 0)
            perror("send?");
        printf("sent %d\n %s\n", sent, buff);
        free(result);
        free(buf);
    }
    return NULL;
}

void register_me(int sock, struct sockaddr *server_addr, char *name, socklen_t *size) {
    char buf[bufferSize];
    strcpy(buf, "INIT|");
    strcat(buf, name);

    printf("Trying to register as %s\n", name);
    int sent = 0;

    if (CONN_MODE == SOCK_DGRAM) {
        sent = sendto(sock, buf, sizeof(buf), 0, NULL, 0);
    } else {
        sent = send(sock, buf, sizeof(buf), 0);
    }
    if (sent < 0)
        die("sent reg");
    memset(buf, 0, sizeof(buf));
    strcpy(buf, "PING");
    while (strncmp(buf, "O", 1) != 0) {
        if (CONN_MODE == SOCK_DGRAM) {
            recvfrom(sock, buf, 1, 0, server_addr, size);
        } else {
            recv(sock, buf, sizeof(buf), 0);
        }
    }
    printf("received to register as %s\n", buf);

    if (strcmp(buf, PLACETAKEN) == 0)
        die("MY NAME IS TAKEN\n COULD NOT CONNECT - DYING\n");
    else
        printf("Registered\n", buf);
}

int main(int args, char *argv[]) {
    if (args == 4) {
//        sighandler_t set;
        signal(SIGINT, exit);
        queue = init_queue();
        my_name = argv[1];
        char *bufname = calloc(strlen(my_name) + 5, sizeof(char));
        strcpy(bufname, "/");
        strcat(bufname, my_name);
//        printf("%s", bufname);
        sem_unlink(bufname);
        process_mutex = sem_open(bufname, O_CREAT, 0777, 0);
        if (process_mutex == SEM_FAILED)
            die("process");

        enum connectType connectionType = strcmp(argv[2], "unix") == 0 ? UNIX : NETWORK;
        if (connectionType == UNIX) {
            char *unix_path = argv[3];
            sock = unix_connect_socket_client(unix_path);
            addr = (struct sockaddr *) &unix_server_addr;
        } else {
            addr_string = parse_address(strtok(argv[3], ":"));
            int port = parse_port(strtok(NULL, ":"));
            sock = inet_connect_socket_client(addr_string, port);
            addr = (struct sockaddr *) &inet_server_addr;

            printf("connect on %s %d %d\n", addr_string, port, htons(port));
        }

        if (pthread_create(&pinger_thread, NULL, process, NULL) != 0)
            die("Cannot create keep_alive thread");

        socklen_t size = sizeof(*addr);
        register_me(sock, addr, my_name, &size);
        fflush(stdout);

        char buf[bufferSize];
        memset(buf, 0, sizeof(buf));
        atexit(unregister_me);

        while (flag) {
            int packet_size;
            flag2=1;
            if (CONN_MODE == SOCK_DGRAM) {
                packet_size = recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr *) addr, &size);
            } else {
                packet_size = recv(sock, buf, sizeof(buf), 0);
            }
            flag2=0;

            if (packet_size <= 0)
                die("Host closed connection");

            int sent = 0;
            if (strncmp(buf, "PING", 4) == 0) {
                strcpy(buf, "PONG|");
                strcat(buf, my_name);
                pthread_mutex_lock(&mutex);
                if (CONN_MODE == SOCK_DGRAM) {
                    sent = sendto(sock, buf, sizeof(buf), 0, (struct sockaddr *) addr, size);
                } else {
                    sent = sendto(sock, buf, sizeof(buf), 0, NULL, 0);
                }
                pthread_mutex_unlock(&mutex);

                if (sent < 0)
                    perror("send?");
            } else if (packet_size > 0) {
                printf("received package of size %d\n", packet_size);
                char *content = calloc(sizeof(buf), sizeof(char));
                strcpy(content, buf);
                enqueue(queue, content);
                sem_post(process_mutex);
            }
        }
        pthread_join(pinger_thread, NULL);

    } else {
        fprintf(stderr, "BAD ARGS\n");
    }

    printf("Hello, World!\n");
    return 0;
}