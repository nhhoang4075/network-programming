/**
 * @file server-1.c
 * @author Nguyen Huy Hoang (nhh4075@gmail.com)
 * @brief Simple server
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
    int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(9000);

    int opt = 1;
    if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt() failed\n");
        exit(EXIT_FAILURE);
    }

    int ret;
    ret = bind(listener, (struct sockaddr *)&addr, sizeof(addr));
    if (ret < 0) {
        perror("bind() failed\n");
        exit(EXIT_FAILURE);
    }

    ret = listen(listener, 5);
    if (ret < 0) {
        perror("listen() failed\n");
        exit(EXIT_FAILURE);
    }

    printf("Waiting for client\n");

    struct sockaddr_in client_addr;
    unsigned int client_addr_len = sizeof(client_addr);

    int client = accept(listener, (struct sockaddr *)&client_addr, &client_addr_len);
    if (client < 0) {
        perror("accept() failed\n");
        exit(EXIT_FAILURE);
    }

    printf("Client connected: %d\n", client);
    printf("IP: %s\n", inet_ntoa(client_addr.sin_addr));
    printf("Port: %d\n", ntohs(client_addr.sin_port));

    char msg[] = "Hello client\n";
    send(client, msg, strlen(msg), 0);

    char buf[256];
    while(1) {
        int n = recv(client, buf, sizeof(buf), 0);
        printf("%d bytes received\n", n);

        if (n <= 0) {
            printf("Disconnected\n");
            break;
        }

        buf[n] = '\0';
        printf("%d bytes received: %s\n", n, buf);

        send(client, buf, n, 0);
    }

    close(client);
    close(listener);
}