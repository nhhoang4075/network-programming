/**
 * @file info_server.c
 * @author Nguyen Huy Hoang (nhh4075@gmail.com)
 * @brief Server that unpacks and displays directory info from client
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
    // Create TCP socket for server
    int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener < 0) {
        perror("socket() failed");
        exf;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(9999);
    
    // Allow port reuse after restart
    int opt = 1;
    if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt() failed");
        exf;
    }

    // Bind to given port
    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind() failed");
        exf;
    }

    // Start listening, max 5 pending connections
    if (listen(listener, 5) < 0) {
        perror("listen() failed");
        exf;
    }

    printf("Waiting for client at port %d\n", ntohs(addr.sin_port));

    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    // Wait for a client to connect
    int client = accept(listener, (struct sockaddr *)&client_addr, &client_addr_len);

    if (client < 0) {
        perror("accept() failed");
        exf;
    }

    printf("Client connected: %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

    char buf[4096];
    int offset = 0;

    recv(client, buf, sizeof(buf), 0);

    char *dir = buf + offset;
    offset += strlen(dir) + 1;
    printf("%s\n", dir);

    int f_count;
    memcpy(&f_count, buf + offset, sizeof(f_count));
    offset += sizeof(f_count);

    for (int i = 0; i < f_count; ++i) {
        char *f_name = buf + offset;
        offset += strlen(f_name) + 1;

        off_t f_size;
        memcpy(&f_size, buf + offset, sizeof(f_size));
        offset += sizeof(off_t);

        printf("%s - %lld bytes\n", f_name, f_size);
    }

    close(client);
    close(listener);

    return 0;
}