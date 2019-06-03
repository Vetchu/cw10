#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <netinet/in.h>
#include <zconf.h>
#include <wait.h>
#include <pthread.h>
#include <fcntl.h>
#include <asm/errno.h>
#include <arpa/inet.h>
#include "common.h"

//udp size
//65,507
int sock;
int global_counter = 0;

struct client_node {
    char name[100];
    int busy;
    enum connectType connection_type;
    int socket;
    int connection;
    struct sockaddr_in addr;
    struct client_node *next;
};
struct client_node *clients;

char *read_file_content(char *filepath) {
    FILE *f = fopen(filepath, "rb");
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *string = malloc(fsize + 1);
    fread(string, fsize, 1, f);

    fclose(f);

    string[fsize] = 0;

    return string;
}

int is_used(const char *name) {
    struct client_node *tmp = clients;

    while (tmp != NULL) {
        if (strcmp(tmp->name, name) == 0)
            return 1;
        tmp = tmp->next;
    }
    return 0;
}

struct sockaddr_in *find_client_addr(char *name) {
    struct client_node *tmp = clients;

    while (tmp != NULL) {
        if (strcmp(tmp->name, name) == 0)
            return &tmp->addr;
        tmp = tmp->next;
    }
    return NULL;
}

void unregister(char *name) {
    struct client_node *tmp = clients;

    while (tmp->next != NULL) {
        struct client_node *tmp2 = tmp->next;
        if (strcmp(name, tmp2->name) == 0) {
            tmp->next = tmp2->next;
            free(tmp2);
            return;
        } else {
            tmp = tmp->next;
        }
    }
    printf("no such client man");
}

void register_client(char *name, int connection, struct sockaddr_in *client, int socket) {

    fflush(stdout);
    struct sockaddr_in *address = find_client_addr(name);

    if (is_used(name)) {
        if (address != client) exit(-1);
        sendto(connection, PLACETAKEN, sizeof(PLACETAKEN), 0, (struct sockaddr *) address, sizeof(address));
    } else {
        struct client_node *new_client = calloc(1, sizeof(struct client_node));
        strcpy(new_client->name, name);
        new_client->addr = *client;
        new_client->socket = socket;
        new_client->connection=connection;
        new_client->next = clients->next;
        clients->next = new_client;

        printf("registered %s with addr %d\n", name, client->sin_addr.s_addr);

        sendto(connection, name, sizeof(name), 0, (struct sockaddr *) address, sizeof(address));
    }
}

int flag = 1;

void *keep_alive(void *pVoid) {
//    while (flag) {
//        sleep(10);
//        printf("PINGING CLIENTS\n");
//        struct client_node *tmp = clients;
//
//        while (tmp->next != NULL) {
//            struct client_node *tmp2 = tmp->next;
//            if (sendto(tmp2->socket, "PING", 4, MSG_NOSIGNAL, (struct sockaddr *) &tmp2->addr, sizeof(tmp2->addr)) ==
//                -1) {
//                perror("dead");
//                printf("%s IS KILL, UNREGISTERING\n", tmp2->name);
//                unregister(tmp2->name);
//            } else {
//                printf("%s IS OK\n", tmp2->name);
//                tmp = tmp->next;
//            }
//        }
//    }
    return 0;
}

void *handle_client(int connection, struct sockaddr_in *client, int socket) {
    char buf[bufferSize];
    int size;

    while (1) {
        size = recvfrom(connection, buf, sizeof(buf), 0, NULL, NULL);

        if (size <= 0) {
            return NULL;
        }
        printf("rozmiar paczki: %d\n"
               "treść:%s\n",
               size, buf);

        char *command = strtok(buf, "|");
        char *rest = strtok(NULL, "|");
        if (rest == NULL) {
            printf("INCORRECT COMMAND ARRIVED:\n");
            if(command!=NULL)
                printf("%s\n", command);
//            continue;
        }

        if (strcmp(command, "INIT") == 0) {
            register_client(rest, connection, client, socket);
        } else if (strcmp(command, "UNREGISTER") == 0) {
            unregister(rest);
        } else if (strcmp(command, "RESULTS") == 0) {
            printf("%s", rest);
        }

    }
    return NULL;
}

void *handler(void *arg) {
    struct sockaddr_in client;
    int connection;
    while (flag) {
        socklen_t clientsize = sizeof(client);
        if ((connection = accept(sock, (struct sockaddr *) &client, &clientsize)) == -1) {
            perror("RIP CONNECTION");
            exit(-1);
        }
        handle_client(connection, &client, sock);
        close(connection);
    }
    return NULL;
}

void send_request(char *content) {
    char buf[bufferSize];
    global_counter++;
    buf[0] = global_counter;
    strcat(buf, content);

    struct client_node *tmp = clients;

    while (tmp->next != NULL) {
        struct client_node *tmp2 = tmp->next;
        if (tmp2->busy == 0) {
            tmp2->busy = 1;
            sendto(tmp2->connection, buf, strlen(buf), 0, (struct sockaddr *) &tmp2->addr, sizeof(tmp2->addr));
            return;
        } else {
            tmp = tmp->next;
        }
    }

    if (tmp->next != NULL) {
        tmp = clients->next;
        sendto(tmp->connection, buf, strlen(buf), 0, (struct sockaddr *) &tmp->addr, sizeof(tmp->addr));
    } else {
        printf("no one to send request to\n");
    }
}

void *input(void *pid) {
    system("ls");
    while (flag) {
        char value[100];
        fgets(value, 100, stdin);
        value[strlen(value) - 1] = 0;

        if (access(value, F_OK) != -1) {
            printf("Correct path\n");
            char *content = read_file_content(value);
            send_request(content);
            free(content);
        } else {
            perror("Wrong File given!");
            continue;
        }
    }
    return NULL;
}

void init_threads() {
    pthread_t input_thread;
    pthread_t pinger_thread;
    pthread_t handler_thread;

    if (pthread_create(&input_thread, NULL, input, NULL) != 0) {
        perror("Cannot create input thread");
        exit(1);
    }
    if (pthread_create(&pinger_thread, NULL, keep_alive, NULL) != 0) {
        perror("Cannot create keepalive thread");
        exit(1);
    }
    if (pthread_create(&handler_thread, NULL, handler, NULL) != 0) {
        perror("Cannot create handler thread");
        exit(1);
    }
    pthread_join(input_thread, NULL);
    pthread_join(pinger_thread, NULL);
    pthread_join(handler_thread, NULL);
}

struct sockaddr_in inet_TCP_socket() {
    /* Create the socket. */
    struct sockaddr_in addr;
    sock = socket(AF_INET, SOCK_STREAM, 0);

    int reuseaddr = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(reuseaddr));
    if (sock < 0) {
        perror("UNABLE TO CREATE SOCKET");
        exit(-1);
    };

    /* Give the socket a name. */
    addr.sin_family = AF_INET;
    addr.sin_port = htons(40000);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(sock, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        perror("UNABLE TO BIND");
        exit(-1);
    }
    if (listen(sock, 20) == -1) {
        perror("UNABLE TO LISTEN");
        exit(-1);
    };

    printf("registered TCP inet socket on %u port %hu\n", addr.sin_addr.s_addr, addr.sin_port);

    return addr;
}

int main() {
    char *address = "127.0.0.1";

    clients = calloc(1, sizeof(struct client_node));
    strcpy(clients->name, "");
    clients->next = NULL;

    struct sockaddr_in addr = inet_TCP_socket();
    struct client_node *tmp = clients;

    init_threads();

    return 0;
}