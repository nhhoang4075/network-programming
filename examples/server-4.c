/**
 * @file server-4.c
 * @author Nguyen Huy Hoang (nhh4075@gmail.com)
 * @brief Server receive struct data
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

struct sinhvien {
    int mssv;
    char hoten[64];
    int tuoi;
};

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

    int client = accept(listener, NULL, NULL);

    if (client < 0) {
        perror("accept() failed");
        exit(EXIT_FAILURE);
    }

    printf("Client connected\n");

    struct sinhvien sv;

    while (1) {
        int ret = recv(client, &sv, sizeof(sv), 0);

        if (ret <= 0) {
            break;
        }

        printf("Received:\n%d\n%s\n%d\n", sv.mssv, sv.hoten, sv.tuoi);
    }

    close(client);
    close(listener);

    return 0;
}
