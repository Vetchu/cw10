#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <netinet/in.h>
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
int epoll_fd;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

struct client_node {
    char name[100];
    int busy;
    enum connectType connection_type;
    int conn;
    struct sockaddr *addr;
    socklen_t addr_size;
    struct client_node *next;
    double last_appeared;
    double last_calculated;
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

void register_client(char *name, int connection, struct sockaddr *client_addr, int size) {
    fflush(stdout);

    printf("asked to register %s\n", name);
    if (is_used(name)) {
        sendto(connection, PLACETAKEN, sizeof(PLACETAKEN), 0, (struct sockaddr *) &client_addr, sizeof(client_addr));
        printf("access denied, already exists\n");
    } else {
        struct client_node *new_client = calloc(1, sizeof(struct client_node));
        strcpy(new_client->name, name);
        new_client->addr = calloc(1, sizeof(struct sockaddr));
        memcpy(new_client->addr, client_addr, sizeof(struct sockaddr));
        new_client->addr_size = size;
        new_client->conn = connection;
        new_client->next = clients->next;
        new_client->last_calculated = 0;
        new_client->last_appeared = pobierz_sekundy();
        clients->next = new_client;
        sleep(1);
        int sent = 0;
        printf("%lu size %d\n", sizeof(*client_addr),size);
//        while (recvfrom(connection,NULL,1,0,client_addr,size)!=);
        if (CONN_MODE == SOCK_STREAM) {
            sent = send(connection, "O", 1, 0);
        } else {
            sent = sendto(connection, "O", 1, 0, client_addr,  sizeof(*client_addr));
        }
        if (sent < 0)
            perror("sent register");
        printf("registered %s \n", name);
    }
}

int flag = 1;

void *keep_alive(void *pVoid) {
    while (flag) {
        sleep(10);
        double currentTime = pobierz_sekundy();
        printf("\nPINGING CLIENTS\n");
        struct client_node *tmp = clients;

        while (tmp->next != NULL) {
            struct client_node *tmp2 = tmp->next;
            int res;
            pthread_mutex_lock(&mutex);
            if (CONN_MODE == SOCK_DGRAM) {
                res = sendto(tmp2->conn, "PING", 4, 0, tmp2->addr, tmp2->addr_size);
            } else {
                res = send(tmp2->conn, "PING", 4, MSG_NOSIGNAL);
            }
            if ((res < 0) || tmp2->last_appeared < currentTime - 20) {
                perror("dead");
                printf("\n%s IS KILL, UNREGISTERING\n", tmp2->name);
                if (CONN_MODE == SOCK_STREAM)
                    close(tmp2->conn);
                unregister(tmp2->name);
            } else {
                printf("%s IS OK\n", tmp2->name);
                tmp = tmp->next;
            }
            pthread_mutex_unlock(&mutex);
        }
    }
    return 0;
}

void handle_client(int connection, struct sockaddr *client, int socket) {
    char buf[bufferSize];
    int size;
    memset(buf, 0, sizeof(buf));

    socklen_t len = sizeof(*client);
//    if(connection==3){
//        len=strlen(((struct sockaddr_un*)client)->sun_path);
//    }
    if (CONN_MODE == SOCK_DGRAM) {
        size = recvfrom(connection, buf, sizeof(buf), 0, client, &len);
    } else {
        size = recv(connection, buf, sizeof(buf), 0);
    }
    if (size <= 0) {
        perror("host closed connection\n");
        if (CONN_MODE == SOCK_STREAM)
            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, connection, 0);
        return;
    }
//    printf("rozmiar paczki: %d\n"
//           "treść:\n"
//           "%s\n",
//           size, buf);

    char *command = strtok(buf, "|");
    char *from = strtok(NULL, "|");
    if (from == NULL) {
        printf("INCORRECT COMMAND ARRIVED:\n");
        if (command != NULL)
            printf("%s\n", command);
        return;
    } else {
        if (strncmp(command, "PONG", 4) == 0) {
            struct client_node *tmp = find_client(from);
            if (tmp != NULL)
                tmp->last_appeared = pobierz_sekundy();
        } else if (strcmp(command, "INIT") == 0) {
            register_client(from, connection, client, len);
        } else if (strcmp(command, "UNREGISTER") == 0) {
            unregister(from);
        } else if (strcmp(command, "RESULTS") == 0) {
            char *which = strtok(NULL, "|");
            char *result = strtok(NULL, "|");
            if (result != NULL) {
                printf("\nCOMMAND: %s\nFROM: %s\nRESULTS:\n%s\n", which, from, result);
            }
            find_client(from)->busy = 0;
        }
    }

    //close(connection);
}

void add_to_epoll(int epoll_fd, int fd) {
    struct epoll_event event;
    event.events = EPOLLIN | EPOLLPRI;
    event.data.fd = fd;
    int res = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event);
    if (res < 0)
        die("cannot add client to epoll");
}

void *monitor_multiple(void *arg) {
    epoll_fd = epoll_create(2);
    int res;
    int unix_fd = unix_sock;
    int inet_fd = inet_sock;

    struct epoll_event inet_event, events[MAX_EVENTS];

    add_to_epoll(epoll_fd, unix_fd);
    add_to_epoll(epoll_fd, inet_fd);

    struct sockaddr_in inet_client;
    struct sockaddr_un unix_client;

    int conn_fd = 0;

    while (flag) {
        int event_count = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
//        printf("events: %d\n", events[0].data.fd);

        printf("%d ready events\n", event_count);

        for (int i = 0; i < event_count; i++) {
            int fd = events[i].data.fd;
            printf("Reading file descriptor '%d'\n", fd);

            if (CONN_MODE == SOCK_DGRAM) {
                if (events[i].data.fd == unix_sock) {
                    printf("awaiting unix\n");
                    handle_client(unix_sock, (struct sockaddr *) &unix_client, fd);
                } else if (events[i].data.fd == inet_sock) {
                    printf("awaiting inet\n");
                    handle_client(inet_sock, (struct sockaddr *) &inet_client, fd);
                } else {
                    perror("error, wrong fd");
                }
            } else {
                if (fd == unix_sock) {
                    socklen_t csize = sizeof(unix_client);
                    if ((conn_fd = accept(fd, (struct sockaddr *) &unix_client, &csize)) == -1)
                        die("UNIX CONNECTION BROKE");
                    handle_client(conn_fd, (struct sockaddr *) &unix_client, fd);

                    add_to_epoll(epoll_fd, conn_fd);
                } else if (fd == inet_sock) {

                    socklen_t csize = sizeof(inet_client);
                    if ((conn_fd = accept(fd, (struct sockaddr *) &inet_client, &csize)) == -1)
                        die(" INET CONNECTION BROKE");
                    handle_client(conn_fd, (struct sockaddr *) &inet_client, fd);

                    add_to_epoll(epoll_fd, conn_fd);
                } else {
                    handle_client(fd, (struct sockaddr *) NULL, fd);
                }
            }
        }
    }
    return NULL;
}

void send_request(char *content) {
    char buf[bufferSize];
    memset(buf, 0, sizeof(buf));

    if (strlen(content) > bufferSize) {
        fprintf(stderr, "OUTPUT WILL BE CUT OFF\n");
    }
    snprintf(buf, bufferSize - 1, "%d %s", global_counter++, content);

    struct client_node *tmp = clients;
    struct client_node *found = NULL;
    double earliest_time = pobierz_sekundy();
    while (tmp->next != NULL) {
        struct client_node *tmp2 = tmp->next;
        if (tmp2->last_calculated == 0) {
            found = tmp2;
        } else if (tmp2->last_calculated < earliest_time) {
            earliest_time = tmp2->last_calculated;
            found = tmp2;
        }
        tmp = tmp->next;
    }

    if (found == NULL) {
        printf("no one to send request to\n");
    } else {
        if (found->busy == 1) {
            printf("overflow, sending %d to %d\n", global_counter, tmp->conn);
            sendto(tmp->conn, buf, sizeof(buf), 0, tmp->addr, tmp->addr_size);
        }
        found->busy = 1;
        found->last_calculated = pobierz_sekundy();
        printf("sending request %d to %d\n", global_counter, found->conn);
        int res;
        if (CONN_MODE == SOCK_DGRAM) {
            res = sendto(found->conn, buf, sizeof(buf), 0, found->addr, found->addr_size);
        } else {
            res = send(found->conn, buf, sizeof(buf), 0);
        }
        if (res < 0)
            die("unable to send request");
        return;
    }

}

void *input(void *pid) {
    system("ls");
    while (flag) {
        char value[100];
        memset(value, 0, sizeof(value));
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

    if (pthread_create(&input_thread, NULL, input, NULL) != 0)
        die("Cannot create input_thread thread");

    if (pthread_create(&pinger_thread, NULL, keep_alive, NULL) != 0)
        die("Cannot create keep_alive thread");

    if (pthread_create(&handler_thread, NULL, monitor_multiple, NULL) != 0)
        die("Cannot create handler thread");

    pthread_join(input_thread, NULL);
    pthread_join(pinger_thread, NULL);
    pthread_join(handler_thread, NULL);
}

int inet_socket(int port) {
    struct sockaddr_in addr;
    int sock = socket(AF_INET, CONN_MODE, 0);

    int reuseaddr = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(reuseaddr));
    if (sock < 0)
        die("UNABLE TO CREATE INET SOCKET");

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sock, (struct sockaddr *) &addr, sizeof(addr)) < 0)
        die("UNABLE TO BIND");

    if (CONN_MODE == SOCK_STREAM)
        if (listen(sock, 20) == -1)
            die("UNABLE TO LISTEN");

    printf("registered inet conn on %u port %hu\n", addr.sin_addr.s_addr, addr.sin_port);
    return sock;
}

int unix_socket(char *path) {
    struct sockaddr_un addr;
    int sock = socket(AF_UNIX, CONN_MODE | O_NONBLOCK, 0);

    int reuseaddr = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(reuseaddr));
    if (sock < 0)
        die("UNABLE TO CREATE UNIX SOCKET");

    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, path);

    if (bind(sock, (struct sockaddr *) &addr, sizeof(addr)) < 0)
        die("UNABLE TO BIND");

    if (CONN_MODE == SOCK_STREAM)
        if (listen(sock, 20) == -1)
            die("UNABLE TO LISTEN");

    printf("registered unix conn on %s\n", path);
    return sock;
}

void killf() {
    close(unix_sock);
    close(inet_sock);
}

int main(int args, char *argv[]) {
    if (args == 3) {
        atexit(killf);

        int port = parse_port(argv[1]);
        clients = calloc(1, sizeof(struct client_node));
        strcpy(clients->name, "");
        clients->next = NULL;

        char *unixpath = argv[2];
        unlink(unixpath);
        inet_sock = inet_socket(port);
        unix_sock = unix_socket(unixpath);
        struct client_node *tmp = clients;
        init_threads();
    }

    return 0;
}