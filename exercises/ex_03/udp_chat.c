/**
 * @file udp_chat.c
 * @author Nguyen Huy Hoang (nhh4075@gmail.com)
 * @brief Peer-to-peer UDP chat application using poll()
 * @date 2026-04-28
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
#define BUF_SIZE 2048

int main(int argc, char *argv[]) {
    if (argc < 4) {
        printf("Usage: %s <port> <remote_ip> <remote_port>\n", argv[0]);
        exf;
    }

    int listener = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (listener < 0) {
        perror("socket() failed");
        exf;
    }
    
    struct pollfd fds[2];
    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN;
    fds[1].fd = listener;
    fds[1].events = POLLIN;

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

    struct sockaddr_in dest_addr;
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_addr.s_addr = inet_addr(argv[2]);
    dest_addr.sin_port = htons(atoi(argv[3]));

    printf("UDP chat on port %s to %s:%s\n", argv[1], argv[2], argv[3]);
    printf("Type a message and press Enter to send\n\n> ");

    fflush(stdout);

    char buf[BUF_SIZE];

    while (1) {
        if (poll(fds, 2, -1) < 0) {
            perror("poll() failed"); 
            exf; 
        }

        int len;

        if (fds[0].revents & POLLIN) {
            len = read(STDIN_FILENO, buf, sizeof(buf) - 1);

            if (len > 0) {
                buf[len] = '\0';
                buf[strcspn(buf, "\r\n")] = '\0';

                if (strcmp(buf, "exit") == 0)
                    break;

                sendto(listener, buf, strlen(buf), 0,
                    (struct sockaddr *)&dest_addr, sizeof(dest_addr));

                printf("> ");
                fflush(stdout);
            }
        }

        if (fds[1].revents & POLLIN) {
            struct sockaddr_in sender_addr;
            socklen_t sender_len = sizeof(sender_addr);

            len = recvfrom(
                listener, buf, sizeof(buf) - 1, 0, (struct sockaddr *)&sender_addr, &sender_len
            );

                if (len > 0) {
                buf[len] = '\0';
                buf[strcspn(buf, "\r\n")] = '\0';
                printf("\r[%s:%d] %s\n> ",
                    inet_ntoa(sender_addr.sin_addr),
                    ntohs(sender_addr.sin_port), buf);
                fflush(stdout);
            }
        }
    }
    
    close(listener);
    return 0;
}