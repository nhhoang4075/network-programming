/**
 * @file tcp_client.c
 * @author Nguyen Huy Hoang (nhh4075@gmail.com)
 * @brief TCP client with dynamic IP and port
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
    if (argc != 3) {
        printf("Usage: %s <IP address> <port>\n", argv[0]);
        exf;
    }

    // Create TCP socket
    int client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (client < 0) {
        perror("socket() failed");
        exf;
    }

    struct sockaddr_in sv_addr;
    sv_addr.sin_family = AF_INET;
    sv_addr.sin_addr.s_addr = inet_addr(argv[1]);
    sv_addr.sin_port = htons(atoi(argv[2]));

    // Connect to server at given IP and port
    if (connect(client, (struct sockaddr *)&sv_addr, sizeof(sv_addr)) < 0) {
        perror("connect() failed");
        exf;
    }

    char buf[2048];
    int len;
    long size, progress = 0;

    // Read file size, then receive file content
    read(client, &size, sizeof(size));

    while (progress < size) {
        len = read(client, buf, sizeof(buf));

        if (len <= 0) {
            break;
        }

        progress += len;

        buf[len] = '\0';
        printf("%s", buf);
    }

    printf("\n");

    // Send user input to server until "exit"
    while(1) {
        printf("Input: ");
        fgets(buf, sizeof(buf), stdin);

        if (strncmp(buf, "exit", 4) == 0) {
            break;
        }

        len = write(client, buf, strlen(buf));
        
        printf("Sent %d bytes to server\n", len);
    }

    close(client);

    return 0;
}