/**
 * @file telnet_server.c
 * @author Nguyen Huy Hoang (nhh4075@gmail.com)
 * @brief Telnet-like TCP server with authentication and command execution using poll()
 * @date 2026-04-23
 *
 * Usage: ./telnet_server <port>
 *
 * Credentials file: users.txt  (one "username password" pair per line)
 * Commands are executed via system() and output is read from out.txt
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <arpa/inet.h>
#include <unistd.h>

#define exf exit(EXIT_FAILURE)
#define MAX_CLIENTS 32
#define BUF_SIZE    2048
#define USER_SIZE   64

#define USERS_FILE  "users.txt"
#define CMD_OUT     "out.txt"

typedef enum { WAIT_USER, WAIT_PASS, AUTHENTICATED } ClientSate;

typedef struct {
    int fd;
    ClientSate state;
    char user[USER_SIZE];
} Client;

static Client clients[MAX_CLIENTS];
static int num_clients = 0;
static struct pollfd fds[MAX_CLIENTS + 1];

static int check_credentials(const char *user, const char *pass) {
    FILE *f = fopen(USERS_FILE, "r");
    if (!f) {
        perror("fopen(users.txt) failed");
        return 0;
    }

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        char u[USER_SIZE], p[USER_SIZE];
        if (sscanf(line, "%63s %63s", u, p) == 2) {
            if (strcmp(u, user) == 0 && strcmp(p, pass) == 0) {
                fclose(f);
                return 1;
            }
        }
    }

    fclose(f);
    return 0;
}

static void send_file(int fd, const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) {
        const char *err = "Error: could not read command output.\n";
        write(fd, err, strlen(err));
        return;
    }

    char buf[BUF_SIZE];
    int n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        write(fd, buf, n);
    }

    fclose(f);
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

    printf("Telnet server listening on port %s (users: %s)\n", argv[1], USERS_FILE);

    while (1) {
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
                clients[num_clients].state = WAIT_USER;
                clients[num_clients].user[0] = '\0';
                fds[num_clients + 1].fd = new_fd;
                fds[num_clients + 1].events = POLLIN;
                num_clients++;

                const char *prompt = "Username: ";
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
                printf("Client fd=%d disconnected\n", clients[i].fd);
                remove_client(clients[i].fd);
                --i;
                continue;
            }

            buf[len] = '\0';
            buf[strcspn(buf, "\r\n")] = '\0';

            if (clients[i].state == WAIT_USER) {
                strncpy(clients[i].user, buf, USER_SIZE - 1);
                clients[i].state = WAIT_PASS;
                const char *prompt = "Password: ";
                write(clients[i].fd, prompt, strlen(prompt));
            } else if (clients[i].state == WAIT_PASS) {
                if (check_credentials(clients[i].user, buf)) {
                    clients[i].state = AUTHENTICATED;
                    
                    char welcome[128];
                    snprintf(welcome, sizeof(welcome), "Login successful. Welcome, %s!\n> ", clients[i].user);

                    write(clients[i].fd, welcome, strlen(welcome));
                    printf("User '%s' authenticated (fd=%d)\n", clients[i].user, clients[i].fd);
                } else {
                    const char *fail = "Login failed.\nUsername: ";
                    write(clients[i].fd, fail, strlen(fail));

                    clients[i].state = WAIT_USER;
                    clients[i].user[0] = '\0';
                }
            } else {
                if (strcmp(buf,"exit") == 0) {
                    const char *bye = "Goodbye!\n";
                    write(clients[i].fd, bye, strlen(bye));

                    printf(
                        "User '%s' logged out (fd=%d)\n",
                        clients[i].user, clients[i].fd
                    );

                    remove_client(clients[i].fd);
                    --i;
                    continue;
                }

                char cmd[BUF_SIZE + 32];
                snprintf(cmd, sizeof(cmd), "%s > %s 2>&1", buf, CMD_OUT);
                system(cmd);

                send_file(clients[i].fd, CMD_OUT);

                const char *prompt = "> ";
                write(clients[i].fd, prompt, strlen(prompt));

                printf("User '%s' executed: %s\n", clients[i].user, buf);
            }
        }
    }

    close(listener);
    return 0;
}