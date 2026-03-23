/**
 * @file server-3.c
 * @author Nguyen Huy Hoang (nhh4075@gmail.com)
 * @brief Server receive data from browser
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
        perror("setsockopt() failed");
        exit(EXIT_FAILURE);
    }

    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind() failed");
        exit(EXIT_FAILURE);
    }

    if (listen(listener, 5) < 0) {
        perror("listen() failed");
        exit(EXIT_FAILURE);
    }

    printf("Waiting for client\n");

    char buf[2048];
    char msg[] = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<html><body><h1>Hello World</h1><p>Welcome to my <strong>server<strong><p></body></html>";
    struct sockaddr_in client_addr;
    unsigned int client_addr_len = sizeof(client_addr);

    while(1) {
        int client = accept(listener, (struct sockaddr *)&client_addr, &client_addr_len);

        if (client < 0) {
            perror("accept() failed");
            continue;
        }

        printf("Client connected: %d\n", client);
        printf("IP: %s\n", inet_ntoa(client_addr.sin_addr));
        printf("Port: %d\n", ntohs(client_addr.sin_port));

        int len = recv(client, buf, sizeof(buf), 0);
        if (len <= 0) {
            continue;
        }

        buf[len] = 0;
        printf("Received: %s\n", buf);

        send(client, msg, strlen(msg), 0);

        close(client);
    }

    close(listener);

    return 0;
}