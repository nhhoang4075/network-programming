/**
 * @file client_1.c
 * @author Nguyen Huy Hoang (nhh4075@gmail.com)
 * @brief Simple client
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
    int client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = htons(9000);

    int ret = connect(client, (struct sockaddr *)&addr, sizeof(addr));
    if (ret < 0) {
        perror("connect() failed");
        exit(EXIT_FAILURE);
    }

    char msg[256];
    char buf[256];

    strcpy(msg, "Hello server!\n");
    send(client, msg, strlen(msg), 0);

    int n = recv(client, buf, sizeof(buf), 0);
    if (n > 0) {
        buf[n] = '\0';
        printf("%d bytes received: %s\n", n, buf);
    }

    while(1) {
        fgets(msg, 256, stdin);
        send(client, msg, strlen(msg), 0);

        n = recv(client, buf, sizeof(buf), 0);
        if (n <= 0) {
            printf("Disconnected\n");
            break;
        }

        buf[n] = '\0';
        printf("Echo: %s\n", buf);
    }

    close(client);
}