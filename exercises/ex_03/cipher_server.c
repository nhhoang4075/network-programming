/**
 * @file cipher_server.c
 * @author Nguyen Huy Hoang (nhh4075@gmail.com)
 * @brief TCP server using poll() – greet clients and encrypt strings
 * @date 2026-04-28
 *
 * Encryption rules:
 *   - Letter  → next letter (Z→A, z→a)
 *   - Digit d → '0' + (9 - d)
 *   - Other   → unchanged
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ctype.h>

#define exf exit(EXIT_FAILURE)
#define MAX_CLIENTS 32
#define BUF_SIZE 2048

static int clients[MAX_CLIENTS];
static int num_clients = 0;
static struct pollfd fds[MAX_CLIENTS + 1];

static char encrypt_char(char c) {
    if (isalpha((unsigned char)c)) {
        if (c == 'Z') return 'A';
        if (c == 'z') return 'a';
        return c + 1;
    }
    
    if (isdigit((unsigned char)c)) {
        return '0' + (9 - (c - '0'));
    }

    return c;
}

static void cipher(char *s) {
    for (; *s; s++) {
        *s = encrypt_char(*s);
    }
}

static void remove_client(int fd) {
    for (int i = 0; i < num_clients; ++i) {
        if (clients[i] == fd) {
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

    printf("Server listening on port %s\n", argv[1]);

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
                const char *full = "Server full.\n";

                write(new_fd, full, strlen(full));
                close(new_fd);
            } else {
                clients[num_clients] = new_fd;

                fds[num_clients + 1].fd = new_fd;
                fds[num_clients + 1].events = POLLIN;
                num_clients++;

                char welcome[128];
                snprintf(
                    welcome, sizeof(welcome),
                    "Hello, %d clients are connected to server.\n",
                    num_clients
                );

                write(new_fd, welcome, strlen(welcome));

                printf(
                    "New client fd=%d from %s:%d (total: %d)\n",
                    new_fd,
                    inet_ntoa(cl_addr.sin_addr),
                    ntohs(cl_addr.sin_port),
                    num_clients
                );
            }
        }

        for (int i = 0; i < num_clients; ++i) {
            if (!(fds[i + 1].revents & (POLLIN | POLLERR))) continue;

            char buf[BUF_SIZE];
            int len = read(clients[i], buf, sizeof(buf) - 1);

            if (len <= 0) {
                printf("Client fd=%d disconnected\n", clients[i]);
                remove_client(clients[i]);
                i--;
                continue;
            }

            buf[len] = '\0';
            buf[strcspn(buf, "\r\n")] = '\0';

            if (strcmp(buf, "exit") == 0) {
                const char *bye = "Good bye!\n";
                write(clients[i], bye, strlen(bye));
                printf(
                    "Client fd=%d exited (total: %d)\n",
                    clients[i],
                    num_clients - 1
                );
                remove_client(clients[i]);
                i--;
                continue;
            }

            cipher(buf);
            char resp[BUF_SIZE + 2];
            snprintf(resp, sizeof(resp), "%s\n", buf);

            write(clients[i], resp, strlen(resp));
        }
    }

    close(listener);
    return 0;
}