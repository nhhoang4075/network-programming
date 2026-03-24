/**
 * @file udp_echo.c
 * @author Nguyen Huy Hoang (nhh4075@gmail.com)
 * @brief UDP echo server that echoes received data back to sender
 * @date 2026-03-24
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#define exf exit(EXIT_FAILURE)

int main() {
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        perror("socket() failed");
        exf;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(9999);

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind() failed");
        exf;
    }

    printf("UDP echo server listening on port %d\n", ntohs(addr.sin_port));

    char buf[2048];
    struct sockaddr_in sender_addr;
    socklen_t sender_len;

    while (1) {
        // Receive data from any sender
        sender_len = sizeof(sender_addr);
        int len = recvfrom(
            sock, buf, sizeof(buf), 0,
            (struct sockaddr *)&sender_addr, &sender_len
        );

        if (len <= 0)
            break;

        buf[len] = '\0';
        printf(
            "From %s:%d - %s",
            inet_ntoa(sender_addr.sin_addr),
            ntohs(sender_addr.sin_port),
            buf
        );

        // Echo data back to sender
        sendto(
            sock, buf, len, 0,
            (struct sockaddr *)&sender_addr, sender_len
        );
    }

    close(sock);

    return 0;
}
