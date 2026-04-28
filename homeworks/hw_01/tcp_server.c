/**
 * @file tcp_server.c
 * @author Nguyen Huy Hoang (nhh4075@gmail.com)
 * @brief TCP server with dynamic port
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

#define exf exit(EXIT_FAILURE)

int main(int argc, char *argv[]) {
    if (argc != 4) {
        printf("Usage: %s <port> <file> <file>\n", argv[0]);
        exf;
    }

    // Create TCP socket
    int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener < 0) {
        perror("socket() failed");
        exf;
    }

    // Allow port reuse after restart
    int opt = 1;
    if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt() failed");
        exf;
    }

    // Bind to all interfaces on given port
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(atoi(argv[1]));

    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind() failed");
        exf;
    }

    // Start listening, max 5 pending connections
    if (listen(listener, 5) < 0) {
        perror("listen() failed");
        exf;
    }

    printf("Waiting for client\n");

    struct sockaddr_in cl_addr;
    socklen_t cl_addr_len = sizeof(cl_addr);

    // Wait for a client to connect
    int client = accept(listener, (struct sockaddr *)&cl_addr, &cl_addr_len);
    if (client < 0) {
        perror("accept() failed");
        exf;
    }

    printf("Client connected: %s:%d\n", inet_ntoa(cl_addr.sin_addr), ntohs(cl_addr.sin_port));

    char buf[2048];

    FILE *rf = fopen(argv[2], "rb"); // file to send
    FILE *wf = fopen(argv[3], "wb"); // file to write received data

    // Get file size and send it to client
    fseek(rf, 0, SEEK_END);
    long size = ftell(rf);
    fseek(rf, 0, SEEK_SET);

    write(client, &size, sizeof(size));

    int len;

    // Send file content to client
    while(1) {
        len = fread(buf, 1, sizeof(buf), rf);

        if (len <= 0) {
            break;
        }

        write(client, buf, len);
    }

    // Receive data from client and write to file
    while(1) {
        len = read(client, buf, sizeof(buf));

        if (len <= 0) {
            printf("Disconnected\n");
            break;
        }

        fwrite(buf, 1, len, wf);
        fflush(wf);

        printf("Wrote %d bytes to %s\n", len, argv[3]);
    }

    fclose(rf);
    fclose(wf);
    close(client);
    close(listener);

    return 0;
}