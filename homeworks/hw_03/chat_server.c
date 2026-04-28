/**
 * @file chat_server.c
 * @author Nguyen Huy Hoang (nhh4075@gmail.com)
 * @brief Multi-client TCP chat server using select()
 * @date 2026-04-14
 *
 * Usage: ./chat_server <port>
 *
 * Registration: client must send "client_id: client_name" (no spaces in either field)
 * After registration: messages are broadcast as "YYYY/MM/DD HH:MM:SSam/pm id: message"
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <time.h>

#define exf exit(EXIT_FAILURE)
#define MAX_CLIENTS 64
#define BUF_SIZE    2048
#define ID_SIZE     64
#define NAME_SIZE   128

typedef enum { REGISTERING, ACTIVE } ClientState;

typedef struct {
    int fd;
    ClientState state;
    char id[ID_SIZE];
    char name[NAME_SIZE];
} Client;

static Client clients[MAX_CLIENTS];
static int num_clients = 0;

static void broadcast(int sender_fd, const char *msg) {
    for (int i = 0; i < num_clients; ++i) {
        if (clients[i].fd != sender_fd && clients[i].state == ACTIVE) {
            send(clients[i].fd, msg, strlen(msg), 0);
        }
    }
}

static void remove_client(int fd) {
    for (int i = 0; i < num_clients; ++i) {
        if (clients[i].fd == fd) {
            close(fd);
            clients[i] = clients[--num_clients];
            return;
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <port>\n", argv[0]);
        exf;
    }

    int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener < 0) {
        perror("socket() failed");
        exf;
    }

    int opt = 1;
    if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt() failed");
        exf;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(atoi(argv[1]));

    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind() failed");
        exf;
    }

    if (listen(listener, MAX_CLIENTS) < 0) {
        perror("listen() failed");
        exf;
    }

    printf("Chat server listening on port %s\n", argv[1]);

    while (1) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(listener, &read_fds);
        int max_fd = listener;

        for (int i = 0; i < num_clients; ++i) {
            FD_SET(clients[i].fd, &read_fds);
            if (clients[i].fd > max_fd) max_fd = clients[i].fd;
        }

        if (select(max_fd + 1, &read_fds, NULL, NULL, NULL) < 0) {
            perror("select() failed");
            exf;
        }

        if (FD_ISSET(listener, &read_fds)) {
            struct sockaddr_in cl_addr;
            socklen_t cl_addr_len = sizeof(cl_addr);

            int new_fd = accept(listener, (struct sockaddr *)&cl_addr, &cl_addr_len);

            if (new_fd < 0) {
                perror("accept() failed");
            } else if (num_clients >= MAX_CLIENTS) {
                const char *full = "Server full. Try again later.\n";
                send(new_fd, full, strlen(full), 0);
                close(new_fd);
            } else {
                clients[num_clients].fd = new_fd;
                clients[num_clients].state = REGISTERING;
                clients[num_clients].id[0] = '\0';
                clients[num_clients].name[0] = '\0';
                num_clients++;

                const char *prompt = "Enter your ID and name (format: client_id: client_name):\n";
                send(new_fd, prompt, strlen(prompt), 0);

                printf(
                    "New connection from %s:%d (fd=%d)\n",
                    inet_ntoa(cl_addr.sin_addr),
                    ntohs(cl_addr.sin_port),
                    new_fd
                );
            }
        }

        for (int i = 0; i < num_clients; ++i) {
            if (!FD_ISSET(clients[i].fd, &read_fds)) continue;

            char buf[BUF_SIZE];
            int len = recv(clients[i].fd, buf, sizeof(buf) - 1, 0);

            if (len <= 0) {
                if (clients[i].state == ACTIVE) {
                    char notice[BUF_SIZE];
                    snprintf(
                        notice, sizeof(notice),
                        "*** %s has left the chat ***\n",
                        clients[i].id
                    );
                    broadcast(clients[i].fd, notice);
                }
                printf("Client fd=%d disconnected\n", clients[i].fd);
                remove_client(clients[i].fd);
                --i;
                continue;
            }

            buf[len] = '\0';
            buf[strcspn(buf, "\r\n")] = '\0';

            if (clients[i].state == REGISTERING) {
                char *sep = strstr(buf, ": ");

                if (sep != NULL && sep != buf) {
                    int id_len = sep - buf;
                    char *name_start = sep + 2;

                    int valid = (strchr(buf, ' ') == NULL || strchr(buf, ' ') >= sep);

                    int name_len = strlen(name_start);
                    if (name_len == 0 || strchr(name_start, ' ') != NULL) valid = 0;

                    if (valid) {
                        strncpy(clients[i].id, buf, id_len);
                        clients[i].id[id_len] = '\0';
                        strncpy(clients[i].name, name_start, NAME_SIZE - 1);
                        clients[i].state = ACTIVE;

                        char welcome[BUF_SIZE];
                        snprintf(
                            welcome, sizeof(welcome),
                            "Welcome, %s (%s)! You can start chatting now.\n",
                            clients[i].name, clients[i].id
                        );
                        send(clients[i].fd, welcome, strlen(welcome), 0);

                        char notice[BUF_SIZE];
                        snprintf(
                            notice, sizeof(notice),
                            "*** %s (%s) has joined the chat ***\n",
                            clients[i].name, clients[i].id
                        );
                        broadcast(clients[i].fd, notice);

                        printf("Registered: id=%s name=%s\n", clients[i].id, clients[i].name);
                        continue;
                    }
                }

                const char *retry = "Invalid format. Use: client_id: client_name\n";
                send(clients[i].fd, retry, strlen(retry), 0);
            } else {
                time_t now = time(NULL);
                struct tm *t = localtime(&now);
                char ts[32];
                strftime(ts, sizeof(ts), "%Y/%m/%d %I:%M:%S%p", t);

                char msg[BUF_SIZE + 256];
                snprintf(msg, sizeof(msg), "%s %s: %s\n", ts, clients[i].id, buf);
                broadcast(clients[i].fd, msg);
                printf("%s", msg);
            }
        }
    }

    close(listener);
    return 0;
}
