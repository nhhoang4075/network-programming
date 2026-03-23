/**
 * @file udp-receiver.c
 * @author Nguyen Huy Hoang (nhh4075@gmail.com)
 * @brief Simple UDP receiver
 * @date 2026-03-23
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

int main() {
    int receiver = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(8000);

    if (bind(receiver, (struct sockaddr *)&addr, sizeof(addr))) {
        perror("bind() failed");
        exit(EXIT_FAILURE);
    }

    char buf[16];
    struct sockaddr_in s_addr;
    socklen_t s_addr_len = sizeof(s_addr);

    while(1) {
        int ret = recvfrom(receiver, buf, sizeof(buf), 0, (struct sockaddr *)&s_addr, &s_addr_len);
        
        if (ret <= 0) {
            break;
        }

        buf[ret] = '\0';
        printf(
            "Received %d bytes from %s:%d: %s\n",
            ret, inet_ntoa(s_addr.sin_addr),
            ntohs(s_addr.sin_port),
            buf
        );
    }

    close(receiver);

    return 0;
}