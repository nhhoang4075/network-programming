/**
 * @file stream_server.c
 * @author Nguyen Huy Hoang (nhh4075@gmail.com)
 * @brief Server that counts "0123456789" occurrences in streamed data
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
#define PATTERN "0123456789"
#define PAT_LEN 10

int main() {
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

    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind() failed");
        exf;
    }

    if (listen(listener, 5) < 0) {
        perror("listen() failed");
        exf;
    }

    printf("Waiting for client at port %d\n", ntohs(addr.sin_port));

    struct sockaddr_in cl_addr;
    socklen_t cl_addr_len = sizeof(cl_addr);

    int client = accept(listener, (struct sockaddr *)&cl_addr, &cl_addr_len);
    if (client < 0) {
        perror("accept() failed");
        exf;
    }

    printf("Client connected: %s:%d\n", inet_ntoa(cl_addr.sin_addr), ntohs(cl_addr.sin_port));

    // Keep tail of previous recv to handle pattern spanning two recv() calls
    char tail[PAT_LEN - 1];
    int tail_len = 0;
    char buf[2048];
    char combined[2048 + PAT_LEN];
    int count = 0;

    while (1) {
        int len = recv(client, buf, sizeof(buf), 0);
        if (len <= 0)
            break;

        // Combine tail from previous recv with current data
        memcpy(combined, tail, tail_len);
        memcpy(combined + tail_len, buf, len);
        int total = tail_len + len;

        // Search for pattern in combined buffer
        for (int i = 0; i <= total - PAT_LEN; i++) {
            if (memcmp(combined + i, PATTERN, PAT_LEN) == 0)
                count++;
        }

        // Save tail for next iteration
        if (total >= PAT_LEN - 1) {
            memcpy(tail, combined + total - (PAT_LEN - 1), PAT_LEN - 1);
            tail_len = PAT_LEN - 1;
        } else {
            memcpy(tail, combined, total);
            tail_len = total;
        }

        printf("Count: %d\n", count);
    }

    printf("Final count: %d\n", count);

    close(client);
    close(listener);

    return 0;
}
