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

struct sockaddr_in inet_server_addr;
struct sockaddr_un unix_server_addr;

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
    int sock = socket(AF_INET, CONN_MODE, 0);
    inet_server_addr.sin_family = AF_INET;
    inet_server_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, address, &inet_server_addr.sin_addr) == -1) {
        perror("BAD ADDRESS GIVEN - PARSE ERROR");
        exit(-1);
    }

    int res = connect(sock, (struct sockaddr *) &inet_server_addr, sizeof(inet_server_addr));

    if (res < 0) {
        perror("inet connect");
        exit(-1);
    }
    return sock;
}

int unix_connect_socket_client(char *path) {
    int sock = socket(AF_UNIX, CONN_MODE, 0);
    unix_server_addr.sun_family = AF_UNIX;
    strcpy(unix_server_addr.sun_path, path);

    int res = connect(sock, (struct sockaddr *) &unix_server_addr, sizeof(unix_server_addr));

    if (res < 0) {
        perror("unix connect");
        exit(-1);
    }
    return sock;
}

void *pong(void *s) {
//    while (flag) {
//        char buf[bufferSize];
//        socklen_t size = sizeof(inet_server_addr);
//        memset(buf, 0, sizeof(buf));
//
//        int size2 = recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr *) &inet_server_addr, &size);
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
    socklen_t size = sizeof(inet_server_addr);

    int ret;
    ret = sendto(sock, buf, strlen(buf), 0, (struct sockaddr *) &inet_server_addr, size);
    printf("Unregistered\n");
//    memset(buf,0, sizeof(buf));
//    if(ret<0){
//        perror("unregister sendto");
//    }
//    ret=recvfrom(sock, &buf, strlen(buf), 0,(struct sockaddr*) &inet_server_addr, &size);
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

    printf("Trying to register as %s\n",name);
    sendto(sock, buf, strlen(buf)+1, 0, NULL, 0);
    socklen_t size = sizeof(inet_server_addr);
    memset(buf, 0, sizeof(buf));
    recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr *) &inet_server_addr, &size);
    printf("received to register as %s\n",buf);

    if (strcmp(buf, PLACETAKEN) == 0) {
        printf("MY NAME IS TAKEN\n COULD NOT CONNECT - DYING\n");
        exit(-1);
    } else {
        printf("Registered %s\n", buf);
    }
}

int main(int args, char *argv[]) {
    char *addr_string;
    socklen_t addr_size;
    struct sockaddr *addr;

    if (args == 4) {
        atexit(unregister_me);
        my_name = argv[1];
        char *name = argv[3];
        enum connectType connectionType = strcmp(argv[2], "unix") == 0 ? UNIX : NETWORK;
        if (connectionType == UNIX) {
            char *unix_path = argv[3];
            sock = unix_connect_socket_client(unix_path);
            addr = (struct sockaddr *) &unix_server_addr;
        } else {
            addr_string = parse_address(strtok(argv[3], ":"));
            int port = parse_port(strtok(NULL, ":"));
            sock = network_connect_socket_client(addr_string, port);
            addr = (struct sockaddr *) &inet_server_addr;
        }
        addr_size = sizeof(addr);

        register_me(sock, my_name);

        pthread_t ping_thread;

        if (pthread_create(&ping_thread, NULL, pong, NULL) != 0) {
            perror("Cannot create ping thread");
            exit(1);
        }
        fflush(stdout);

        char buf[bufferSize];
        memset(buf, 0, sizeof(buf));
        while (flag) {
            int packet_size = recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr *) addr, &addr_size);
            if(packet_size==0){
                perror("Host closed connection");
                exit(-1);
            }
            printf("received package of size %d\n", packet_size);
            if (packet_size > 0 && strncmp(buf,"PING",4)!=0) {
                char *result = parseText(buf);
                sendto(sock, result, strlen(result), 0, (struct sockaddr *) addr, addr_size);
            }
        }
    } else {
        fprintf(stderr, "BAD ARGS\n");
    }

    printf("Hello, World!\n");
    return 0;
}