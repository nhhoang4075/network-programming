/**
 * @file udp-sender.c
 * @author Nguyen Huy Hoang (nhh4075@gmail.com)
 * @brief Simple UDP sender
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
    int sender = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = htons(8000);

    char buf[256];
    while(1) {
        printf("Enter string: ");
        fgets(buf, sizeof(buf), stdin);

        int ret = sendto(sender, buf, strlen(buf), 0, (struct sockaddr *)&addr, sizeof(addr));

        printf("Return value: %d\n", ret);
    }

    close(sender);

    return 0;
}