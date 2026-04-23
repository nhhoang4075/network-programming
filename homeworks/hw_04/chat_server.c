/**
 * @file chat_server.c
 * @author Nguyen Huy Hoang (nhh4075@gmail.com)
 * @brief Multi-client TCP chat server using poll()
 * @date 2026-04-23
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
#include <sys/poll.h>
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
static struct pollfd fds[MAX_CLIENTS + 1];

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
            int last = --num_clients;
            clients[i] = clients[last];
            fds[i + 1] = fds[last + 1];
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

    fds[0].fd = listener;
    fds[0].events = POLLIN;

    printf("Chat server listening on port %s\n", argv[1]);

    while(1) {
        if (poll(fds, num_clients + 1, -1) < 0) {
            perror("poll() failed");
            exf;
        }

        if (fds[0].revents & POLLIN) {
            struct sockaddr_in cl_addr;
            socklen_t cl_addr_len = sizeof(cl_addr);
            
            int new_fd = accept(listener, (struct sockaddr *)&cl_addr, &cl_addr_len);

            if (new_fd < 0) {
                perror("accept() failed");
            } else if (num_clients >= MAX_CLIENTS) {
                const char *full = "Server full. Try again later.\n";
                write(new_fd, full, strlen(full));
                close(new_fd);
            } else {
                clients[num_clients].fd = new_fd;
                clients[num_clients].state = REGISTERING;
                clients[num_clients].id[0] = '\0';
                clients[num_clients].name[0] = '\0';
                fds[num_clients + 1].fd = new_fd;
                fds[num_clients + 1].events = POLLIN;
                num_clients++;

                const char *prompt = "Enter your ID and name (format: client_id: client_name):\n";
                write(new_fd, prompt, strlen(prompt));

                printf(
                    "New connection from %s:%d (fd=%d)\n",
                    inet_ntoa(cl_addr.sin_addr),
                    ntohs(cl_addr.sin_port),
                    new_fd
                );
            }
        }

        for (int i = 0; i < num_clients; ++i) {
            if (!(fds[i + 1].revents & (POLLIN | POLLERR))) continue;

            char buf[BUF_SIZE];
            int len = read(clients[i].fd, buf, sizeof(buf) - 1);

            if (len <= 0) {
                if (clients[i].state == ACTIVE) {
                    char notice[BUF_SIZE];

                    snprintf(
                        notice, sizeof(notice),
                        "*** %s has left the chat ***\n",
                        clients[i].id
                    );
                    broadcast(clients[i].fd, "Client fd=%s disconnected\n");
                }
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
                        write(clients[i].fd, welcome, strlen(welcome));

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
                write(clients[i].fd, retry, strlen(retry));
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
