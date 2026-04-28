/**
 * @file broker.c
 * @author Nguyen Huy Hoang (nhh4075@gmail.com)
 * @brief TCP pub/sub message broker using poll()
 * @date 2026-04-28
 *
 * Commands:
 *   SUB <topic>        -- subscribe to a topic
 *   UNSUB <topic>      -- unsubscribe from a topic
 *   PUB <topic> <msg>  -- publish a message to all subscribers of a topic
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <arpa/inet.h>
#include <unistd.h>

#define RESET   "\033[0m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"

#define exf             exit(EXIT_FAILURE)
#define MAX_CLIENTS     64
#define MAX_TOPICS      16
#define MAX_TOPIC_LEN   64
#define BUF_SIZE        2048
#define BROKER_PORT     9000

typedef struct {
    int fd;
    struct sockaddr_in addr;
    char topics[MAX_TOPICS][MAX_TOPIC_LEN];
    int num_topics;
} Client;

static Client clients[MAX_CLIENTS];
static int num_clients = 0;
static struct pollfd fds[MAX_CLIENTS + 1];

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
    addr.sin_port = htons(BROKER_PORT);

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

    printf("Server listening on port %d\n", BROKER_PORT);

    while (1) {
        if (poll(fds, num_clients + 1, -1) < 0) {
            perror("poll() failed");
            exf;
        }

        char fwd[BUF_SIZE];

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
                clients[num_clients].fd = new_fd;
                clients[num_clients].num_topics = 0;
                clients[num_clients].addr = cl_addr;

                fds[num_clients + 1].fd = new_fd;
                fds[num_clients + 1].events = POLLIN;
                num_clients++;

                snprintf(fwd, sizeof(fwd), "Connected!\n");
                write(new_fd, fwd, strlen(fwd));

                printf(
                    "Client %s:%d connected\n",
                    inet_ntoa(cl_addr.sin_addr),
                    ntohs(cl_addr.sin_port)
                );
            }
        }

        for (int i = 0; i < num_clients; ++i) {
            if (!(fds[i + 1].revents & (POLLIN | POLLERR))) continue;

            char buf[BUF_SIZE];

            int len = read(clients[i].fd, buf, sizeof(buf) - 1);

            if (len <= 0) {
                printf(
                    RED "Client %s:%d disconnected\n" RESET,
                    inet_ntoa(clients[i].addr.sin_addr),
                    ntohs(clients[i].addr.sin_port)
                );
                remove_client(clients[i].fd);
                i--;
                continue;
            }

            buf[len] = '\0';
            buf[strcspn(buf, "\r\n")] = '\0';

            char topic[MAX_TOPIC_LEN];
            char msg[BUF_SIZE];

            if (strncmp(buf, "SUB ", 4) == 0) {
                sscanf(buf, "SUB %63s", topic);
                
                int already = 0;
                for (int j = 0; j < clients[i].num_topics; ++j) {
                    if (strcmp(clients[i].topics[j], topic) == 0) {
                        already = 1;
                        break;
                    }
                }
                
                if (already) {
                    snprintf(
                        fwd, sizeof(fwd),
                        "You have already subscribed to %s\n",
                        topic
                    );
                    
                    write(clients[i].fd, fwd, strlen(fwd));
                } else if (clients[i].num_topics < MAX_TOPICS) {
                    strncpy(
                        clients[i].topics[clients[i].num_topics++],
                        topic, MAX_TOPIC_LEN - 1
                    );

                    snprintf(fwd, sizeof(fwd), GREEN "Subscribed to %s\n" RESET, topic);
                    write(clients[i].fd, fwd, strlen(fwd));

                    printf(
                        GREEN "Client %s:%d subscribed to %s\n" RESET,
                        inet_ntoa(clients[i].addr.sin_addr),
                        ntohs(clients[i].addr.sin_port),
                        topic
                    );
                }
            } else if (strncmp(buf, "UNSUB ", 6) == 0) {
                sscanf(buf, "UNSUB %63s", topic);

                int found = 0;
                for (int j = 0; j < clients[i].num_topics; ++j) {
                    if (strcmp(clients[i].topics[j], topic) == 0) {
                        found = 1;
                        int last_topic = --clients[i].num_topics;
                        if (j != last_topic) {
                            strncpy(
                                clients[i].topics[j],
                                clients[i].topics[last_topic],
                                MAX_TOPIC_LEN - 1
                            );
                        }
                        break;
                    }
                }

                if (found) {
                    snprintf(fwd, sizeof(fwd), YELLOW "Unsubscribed from %s\n" RESET, topic);
                    write(clients[i].fd, fwd, strlen(fwd));

                    printf(
                        YELLOW "Client %s:%d unsubscribed to %s\n" RESET,
                        inet_ntoa(clients[i].addr.sin_addr),
                        ntohs(clients[i].addr.sin_port),
                        topic
                    );
                } else {
                    snprintf(fwd, sizeof(fwd), "Not subscribed to %s\n", topic);
                    write(clients[i].fd, fwd, strlen(fwd));
                }
            } else if (strncmp(buf, "PUB ", 4) == 0) {
                sscanf(buf, "PUB %63s %[^\n]", topic, msg);

                int found;
                for (int j = 0; j < num_clients; ++j) {
                    for (int k = 0; k < clients[j].num_topics; ++k) {
                        if (strcmp(clients[j].topics[k], topic) == 0) {
                            found = 1;
                            snprintf(
                                fwd, sizeof(fwd),
                                "[%s] %s\n",
                                topic, msg
                            );
                            write(clients[j].fd, fwd, strlen(fwd));

                            break;
                        }
                    }
                }

                if (found) {
                    printf(
                        "Client %s:%d published message to %s\n",
                        inet_ntoa(clients[i].addr.sin_addr),
                        ntohs(clients[i].addr.sin_port),
                        topic
                    );
                }
            } else {
                const char *err = RED "Unknown command. Use:\n- SUB <topic>\n- UNSUB <topic>\n- PUB <topic> <msg>\n" RESET;
                write(clients[i].fd, err, strlen(err));
            }
        }
    }

    return 0;
}
