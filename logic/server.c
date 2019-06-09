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

#include <sys/epoll.h>
#include <sys/un.h>

#define MAX_EVENTS 1000

//udp size
//65,507
int global_counter = 0;
int inet_sock;
int unix_sock;

struct client_node {
    char name[100];
    int busy;
    enum connectType connection_type;
    int sock;
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

int is_used(char *name) {
    struct client_node *tmp = clients;
    while (tmp != NULL) {
        if (strcmp(tmp->name, name) == 0)
            return 1;
        tmp = tmp->next;
    }
    return 0;
}

struct client_node *find_client(char *name) {
    struct client_node *tmp = clients;

    while (tmp != NULL) {
        if (strcmp(tmp->name, name) == 0)
            return tmp;
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
    struct sockaddr *address;
    address = (struct sockaddr *) &find_client(name)->addr;
    printf("asked to register %s\n", name);
    if (is_used(name)) {
        sendto(connection, PLACETAKEN, sizeof(PLACETAKEN), 0, (struct sockaddr *) address, sizeof(address));
    } else {
        struct client_node *new_client = calloc(1, sizeof(struct client_node));
        strcpy(new_client->name, name);
        new_client->addr = *client;
        new_client->sock = connection;
        new_client->next = clients->next;
        clients->next = new_client;

        printf("registered %s with addr %u\n", name, client->sin_addr.s_addr);
        sendto(connection, name, sizeof(name), 0, NULL, 0);
    }
}

int flag = 1;

void *keep_alive(void *pVoid) {
    while (flag) {
        sleep(10);
        printf("PINGING CLIENTS\n");
        struct client_node *tmp = clients;

        while (tmp->next != NULL) {
            struct client_node *tmp2 = tmp->next;
            if (sendto(tmp2->sock, "PING", 4, MSG_NOSIGNAL, (struct sockaddr *) &tmp2->addr, sizeof(tmp2->addr)) ==
                -1) {
                perror("dead");
                printf("%s IS KILL, UNREGISTERING\n", tmp2->name);
                unregister(tmp2->name);
            } else {
                printf("%s IS OK\n", tmp2->name);
                tmp = tmp->next;
            }
        }
    }
    return 0;
}

void *handle_client(int connection, struct sockaddr_in *client, int socket) {
    char buf[bufferSize];
    memset(buf, 0, sizeof(buf));
    int size;

    size = recvfrom(connection, buf, sizeof(buf), 0, NULL, NULL);

    if (size <= 0) {
        return NULL;
    }

    printf("rozmiar paczki: %d\n"
           "treść:\n"
           "%s\n",
           size, buf);

    char *command = strtok(buf, "|");
    char *rest = strtok(NULL, "|");
    if (rest == NULL) {
        printf("INCORRECT COMMAND ARRIVED:\n");
        if (command != NULL)
            printf("%s\n", command);
    } else {
        if (strcmp(command, "INIT") == 0) {
            register_client(rest, connection, client, socket);
        } else if (strcmp(command, "UNREGISTER") == 0) {
            unregister(rest);
        } else if (strcmp(command, "RESULTS") == 0) {
            printf("RESULTS:\n %s", rest);
        }
    }

    //close(connection);
    return NULL;
}

void *monitor_multiple(void *arg) {
    int epoll_fd = epoll_create(2);
    int res;
    //char read_buffer[MSG_SIZE];
    int unix_fd = unix_sock;
    int inet_fd = inet_sock;

    struct epoll_event inet_event, unix_event, events[MAX_EVENTS];

    inet_event.events = EPOLLIN;
    inet_event.data.fd = inet_fd;
    res = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, inet_fd, &inet_event);

    if (res) {
        perror("inet_fd");
        exit(-1);
    }

    unix_event.events = EPOLLIN;
    unix_event.data.fd = unix_fd;
    res = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, unix_fd, &unix_event);

    if (res) {
        perror("unix_fd");
        exit(-1);
    }
    struct sockaddr_in inet_client;
    struct sockaddr_un unix_client;

    int conn_fd;

    while (flag) {
        int event_count = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (event_count < 0) {
            perror("events");
            exit(-1);
        }
        printf("%d ready events\n", event_count);

        for (int i = 0; i < event_count; i++) {
            int fd = events[i].data.fd;
            printf("Reading file descriptor '%d'\n ", fd);

            if (events[i].data.fd == unix_sock) {
                socklen_t clientsize = sizeof(unix_client);
                if ((conn_fd = accept(fd, (struct sockaddr *) &unix_client, &clientsize)) == -1) {
                    perror("CONNECTION BROKE");
                    exit(-1);
                }
                handle_client(conn_fd, (struct sockaddr_in *) &unix_client, fd);
            } else if (events[i].data.fd == inet_sock) {
                socklen_t clientsize = sizeof(inet_client);
                if ((conn_fd = accept(fd, (struct sockaddr *) &inet_client, &clientsize)) == -1) {
                    perror("CONNECTION BROKE");
                    exit(-1);
                }

                handle_client(conn_fd, &inet_client, fd);

                struct epoll_event conn_event;
                conn_event.events = EPOLLIN;
                conn_event.data.fd = conn_fd;
                res = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, conn_fd, &conn_event);
                if (res) {
                    perror("conn_fd");
                    exit(-1);
                }
            } else {
                handle_client(conn_fd, NULL, fd);
            }


//            bytes_read = read(events[i].data.fd, read_buffer, MSG_SIZE);
//            if (bytes_read < 0) perror("Error in epoll_wait!");
//            printf("%d bytes read.\n", bytes_read);
//            read_buffer[bytes_read] = '\0';
//            printf("Read '%s'\n", read_buffer);
        }
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
            printf("sending to %d", tmp2->sock);
            sendto(tmp2->sock, buf, strlen(buf), 0, (struct sockaddr *) &tmp2->addr, sizeof(tmp2->addr));
            return;
        } else {
            tmp = tmp->next;
        }
    }

    if (tmp->next != NULL) {
        tmp = clients->next;
        sendto(tmp->sock, buf, strlen(buf), 0, (struct sockaddr *) &tmp->addr, sizeof(tmp->addr));
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
    if (pthread_create(&handler_thread, NULL, monitor_multiple, NULL) != 0) {
        perror("Cannot create handler thread");
        exit(1);
    }
    pthread_join(input_thread, NULL);
    pthread_join(pinger_thread, NULL);
    pthread_join(handler_thread, NULL);
}

int inet_socket(int port, int CONN_MODE) {
    struct sockaddr_in addr;
    int sock = socket(AF_INET, CONN_MODE, 0);

    int reuseaddr = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(reuseaddr));
    if (sock < 0) {
        perror("UNABLE TO CREATE SOCKET");
        exit(-1);
    };

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sock, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        perror("UNABLE TO BIND");
        exit(-1);
    }
    if (listen(sock, 20) == -1) {
        perror("UNABLE TO LISTEN");
        exit(-1);
    };

    printf("registered inet sock on %u port %hu\n", addr.sin_addr.s_addr, addr.sin_port);
    return sock;
}

int unix_socket(char *path, int CONN_MODE) {
    struct sockaddr_un addr;
    int sock = socket(AF_UNIX, CONN_MODE, 0);

    int reuseaddr = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(reuseaddr));
    if (sock < 0) {
        perror("UNABLE TO CREATE SOCKET");
        exit(-1);
    };

    /* Give the sock a name. */
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, path);

    if (bind(sock, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        perror("UNABLE TO BIND");
        exit(-1);
    }
    if (listen(sock, 20) == -1) {
        perror("UNABLE TO LISTEN");
        exit(-1);
    }

    printf("registered unix sock on %s\n", path);

    return sock;
}

void killf() {
    close(unix_sock);
    close(inet_sock);
}

int main(int args, char *argv[]) {
    if (args == 3) {
        atexit(killf);

        int port = atoi(argv[1]);
        clients = calloc(1, sizeof(struct client_node));
        strcpy(clients->name, "");
        clients->next = NULL;

        char *unixpath = argv[2];
        unlink(unixpath);
        inet_sock = inet_socket(port, CONN_MODE);
        unix_sock = unix_socket(unixpath, CONN_MODE);
        init_threads();

//    struct client_node *tmp = clients;
    }

    return 0;
}