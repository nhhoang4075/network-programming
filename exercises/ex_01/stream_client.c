/**
 * @file stream_client.c
 * @author Nguyen Huy Hoang (nhh4075@gmail.com)
 * @brief Client that continuously sends text data to server
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
    int client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (client < 0) {
        perror("socket() failed");
        exf;
    }

    struct sockaddr_in sv_addr;
    sv_addr.sin_family = AF_INET;
    sv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    sv_addr.sin_port = htons(9999);

    if (connect(client, (struct sockaddr *)&sv_addr, sizeof(sv_addr)) < 0) {
        perror("connect() failed");
        exf;
    }

    printf("Connected to server\n");

    char buf[2048];

    // Continuously send text data
    while (1) {
        printf("Input: ");
        fgets(buf, sizeof(buf), stdin);
        buf[strcspn(buf, "\n")] = '\0';

        if (strcmp(buf, "exit") == 0)
            break;

        send(client, buf, strlen(buf), 0);
    }

    close(client);

    return 0;
}
