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
#include "common.h"
#include "../utils/hashmap.h"

struct sockaddr_in server_addr;
char *my_name;
int sock;
int flag = 1;

char *parse_address(char *addr) {
    if (addr == NULL) {
        perror("BAD ADDRESS");
        exit(-1);
    }
    return addr;
}

int parse_port(char *portstring) {
    if (portstring == NULL) {
        perror("BAD PORT");
        exit(-1);
    }
    int port = strtol(portstring, NULL, 10);
    if (port == 0) {
        perror("BAD PORT NUM");
        exit(-1);
    }
    return port;
}

int network_connect_socket_client(char *address, int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, address, &server_addr.sin_addr) == -1) {
        perror("BAD ADDRESS GIVEN - PARSE ERROR");
        exit(-1);
    }

    connect(sock, (struct sockaddr *) &server_addr, sizeof(server_addr));
    return sock;
}

void *pong(void *s) {
//    while (flag) {
//        char buf[bufferSize];
//        socklen_t size = sizeof(server_addr);
//        memset(buf, 0, sizeof(buf));
//
//        int size2 = recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr *) &server_addr, &size);
//        if (size2 > 0) {
//            printf("size of msg %d", size2);
//            puts(buf);
//            puts("\n");
//        }
//    }
//    return NULL;
}

void unregister_me() {
    char buf[bufferSize];
    strcpy(buf, "UNREGISTER|");
    strcat(buf, my_name);
    socklen_t size = sizeof(server_addr);

    int ret;
    ret = sendto(sock, buf, strlen(buf), 0, (struct sockaddr *) &server_addr, size);
    printf("Unregistered\n");
//    memset(buf,0, sizeof(buf));
//    if(ret<0){
//        perror("unregister sendto");
//    }
//    ret=recvfrom(sock, &buf, strlen(buf), 0,(struct sockaddr*) &server_addr, &size);
//
//    if(ret<0){
//        perror("unregister recv");
//    }
//    if (strcmp(my_name, buf) == 0) {
//        printf("successfully unregistered\n");
//    } else {
//        printf("%s\nCONNECT ERROR", buf);
//    }
    close(sock);
}

void register_me(int sock, char *name) {
    char buf[bufferSize];
    strcpy(buf, "INIT|");
    strcat(buf, name);

    sendto(sock, buf, strlen(buf), 0, NULL, 0);
    socklen_t size = sizeof(server_addr);
    memset(buf,0, sizeof(buf));
    recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr *) &server_addr, &size);

    if (strcmp(buf, PLACETAKEN) == 0) {
        printf("MY NAME IS TAKEN\n COULD NOT CONNECT - DYING\n");
        exit(-1);
    } else {
        printf("Registered %s\n", buf);
    }
}

int main(int args, char *argv[]) {
    if (args == 4) {
        atexit(unregister_me);
        my_name = argv[1];
        enum connectType connectionType = strcmp(argv[2], "unix") == 0 ? UNIX : NETWORK;
        if (connectionType == UNIX) {
            char *unix_path = argv[3];
        } else {
            char *addr;
            addr = parse_address(strtok(argv[3], ":"));
            socklen_t addr_size = sizeof(addr);
            int port = parse_port(strtok(NULL, ":"));
            sock = network_connect_socket_client(addr, port);
            register_me(sock, my_name);


            pthread_t ping_thread;

            if (pthread_create(&ping_thread, NULL, pong, NULL) != 0) {
                perror("Cannot create input thread");
                exit(1);
            }
            fflush(stdout);

            char buf[bufferSize];
            memset(buf, 0, sizeof(buf));
            while (flag) {
                int packet_size = recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr *) &addr, &addr_size);
                if (packet_size > 0) {
                    char *result = parseText(buf);
                    sendto(sock, result, strlen(result), 0, (struct sockaddr *) &addr, addr_size);
                }
            }
//
        }

    } else {
        fprintf(stderr, "BAD ARGS\n");
    }

    printf("Hello, World!\n");
    return 0;
}