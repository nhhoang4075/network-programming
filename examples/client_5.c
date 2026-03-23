/**
 * @file client_5.c
 * @author Nguyen Huy Hoang (nhh4075@gmail.com)
 * @brief Client that sends file data
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

long fsize(FILE *f) {
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    return size;
}

int main() {
    int client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (client < 0) {
        perror("socket() failed");
        exit(EXIT_FAILURE);
    }


    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = htons(9000);

    if (connect(client, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("connect() failed");
        exit(EXIT_FAILURE);
    }

    FILE *f = fopen("test.pdf", "rb");
    long size = fsize(f);

    write(client, &size, sizeof(size));

    char buf[256];    
    long progress = 0;

    while(1) {
        int len = fread(buf, 1, sizeof(buf), f);

        if (len <= 0) {
            break;
        }

        write(client, buf, len);
        progress += len;

        printf("\rProgress: %ld/%ld\n", progress, size);
    }

    fclose(f);
    close(client);

    return 0;
}