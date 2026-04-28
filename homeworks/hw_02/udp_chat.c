/**
 * @file udp_chat.c
 * @author Nguyen Huy Hoang (nhh4075@gmail.com)
 * @brief Non-blocking UDP chat application
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

#define exf exit(EXIT_FAILURE)

int main(int argc, char *argv[]) {
    if (argc != 4) {
        printf("Usage: %s <port_s> <ip_d> <port_d>\n", argv[0]);
        exf;
    }

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        perror("socket() failed");
        exf;
    }

    int opt = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt() failed");
        exf;
    }

    // Bind to local port for receiving
    struct sockaddr_in local_addr;
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    local_addr.sin_port = htons(atoi(argv[1]));

    if (bind(sock, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0) {
        perror("bind() failed");
        exf;
    }

    // Destination address for sending
    struct sockaddr_in dest_addr;
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_addr.s_addr = inet_addr(argv[2]);
    dest_addr.sin_port = htons(atoi(argv[3]));

    // Set socket and stdin to non-blocking
    fcntl(sock, F_SETFL, O_NONBLOCK);
    fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);

    printf("UDP chat on port %s -> %s:%s\n", argv[1], argv[2], argv[3]);
    printf("Type a message and press Enter to send.\n\n> ");

    fflush(stdout);

    char buf[2048];

    while (1) {
        // Check for incoming messages
        struct sockaddr_in sender_addr;
        socklen_t sender_len = sizeof(sender_addr);
        int len = recvfrom(sock, buf, sizeof(buf) - 1, 0,
            (struct sockaddr *)&sender_addr, &sender_len);

        if (len > 0) {
            buf[len] = '\0';
            printf("\r[%s:%d] %s\n> ",
                inet_ntoa(sender_addr.sin_addr),
                ntohs(sender_addr.sin_port), buf);
            fflush(stdout);
        }

        // Check for keyboard input
        len = read(STDIN_FILENO, buf, sizeof(buf) - 1);

        if (len > 0) {
            buf[len] = '\0';
            buf[strcspn(buf, "\n")] = '\0';

            if (strcmp(buf, "exit") == 0)
                break;

            sendto(sock, buf, strlen(buf), 0,
                (struct sockaddr *)&dest_addr, sizeof(dest_addr));

            printf("> ");
            fflush(stdout);
        }

        usleep(10000);
    }

    close(sock);
    return 0;
}
