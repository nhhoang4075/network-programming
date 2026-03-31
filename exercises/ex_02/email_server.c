/**
 * @file email_server.c
 * @author Nguyen Huy Hoang (nhh4075@gmail.com)
 * @brief Non-blocking server that returns HUST student email
 * @date 2026-03-31
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <ctype.h>

#define exf exit(EXIT_FAILURE)
#define MAX_CLIENTS 64

// Client states
#define STATE_NEW      0
#define STATE_GOT_NAME 1

struct client_info {
    int fd;
    char name[128];
    char stud_id[16];
    int state;
};

// Generate HUST email from name and student ID
// "Nguyen Huy Hoang" + "20235336" -> "hoang.nh235336@sis.hust.edu.vn"
void generate_email(const char *name, const char *stud_id, char *email, int size) {
    char lower[128];
    for (int i = 0; name[i]; i++)
        lower[i] = tolower(name[i]);
    lower[strlen(name)] = '\0';

    // Split into words
    char *words[16];
    int wc = 0;
    char *tok = strtok(lower, " ");
    while (tok) {
        words[wc++] = tok;
        tok = strtok(NULL, " ");
    }

    // Last word = first name, rest = initials
    char initials[16] = {0};
    for (int i = 0; i < wc - 1; i++)
        initials[i] = words[i][0];

    // Last 6 digits of student ID
    int len = strlen(stud_id);
    const char *digits = len > 6 ? stud_id + len - 6 : stud_id;

    snprintf(email, size, "%s.%s%s@sis.hust.edu.vn", words[wc - 1], initials, digits);
}

int main() {
    setbuf(stdout, NULL);

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
    addr.sin_port = htons(9999);

    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind() failed");
        exf;
    }

    if (listen(listener, 5) < 0) {
        perror("listen() failed");
        exf;
    }

    // Set listener to non-blocking
    fcntl(listener, F_SETFL, O_NONBLOCK);

    printf("Email server listening on port %d\n", ntohs(addr.sin_port));

    struct client_info clients[MAX_CLIENTS];
    int num_clients = 0;

    while (1) {
        // Try to accept new connections
        struct sockaddr_in cl_addr;
        socklen_t cl_len = sizeof(cl_addr);
        int client = accept(listener, (struct sockaddr *)&cl_addr, &cl_len);

        if (client >= 0) {
            // Set new client to non-blocking
            fcntl(client, F_SETFL, O_NONBLOCK);

            clients[num_clients].fd = client;
            clients[num_clients].state = STATE_NEW;
            num_clients++;

            printf("Client connected: %s:%d\n",
                inet_ntoa(cl_addr.sin_addr), ntohs(cl_addr.sin_port));

            // Ask for name
            char *prompt = "Enter name: ";
            send(client, prompt, strlen(prompt), 0);

        } else if (errno != EWOULDBLOCK && errno != EAGAIN) {
            perror("accept() failed");
            exf;
        }

        // Check each client for data
        for (int i = 0; i < num_clients; i++) {
            char buf[256];
            int len = recv(clients[i].fd, buf, sizeof(buf) - 1, 0);

            if (len > 0) {
                buf[len] = '\0';
                // Strip newline
                buf[strcspn(buf, "\r\n")] = '\0';

                if (clients[i].state == STATE_NEW) {
                    // Got name, ask for student ID
                    strcpy(clients[i].name, buf);
                    clients[i].state = STATE_GOT_NAME;

                    char *prompt = "Enter student ID: ";
                    send(clients[i].fd, prompt, strlen(prompt), 0);

                } else if (clients[i].state == STATE_GOT_NAME) {
                    // Got student ID, generate email
                    strcpy(clients[i].stud_id, buf);

                    char email[128];
                    generate_email(clients[i].name, clients[i].stud_id, email, sizeof(email));

                    char reply[256];
                    int reply_len = snprintf(reply, sizeof(reply), "Email: %s\n", email);
                    send(clients[i].fd, reply, reply_len, 0);

                    printf("Generated: %s - %s: %s\n", clients[i].name, clients[i].stud_id, email);

                    // Reset state for next query
                    clients[i].state = STATE_NEW;
                    char *prompt = "Enter name: ";
                    send(clients[i].fd, prompt, strlen(prompt), 0);
                }

            } else if (len == 0) {
                // Client disconnected
                printf("Client %d disconnected\n", clients[i].fd);
                close(clients[i].fd);

                // Remove from array
                clients[i] = clients[num_clients - 1];
                num_clients--;
                i--;
            }
            // len == -1 && EWOULDBLOCK: no data yet, skip
        }

        usleep(10000); // 10ms to avoid busy loop
    }

    close(listener);
    return 0;
}
