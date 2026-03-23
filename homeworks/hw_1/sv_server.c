/**
 * @file sv_server.c
 * @author Nguyen Huy Hoang (nhh4075@gmail.com)
 * @brief Server that receives struct data of student
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
#include <time.h>

#define exf exit(EXIT_FAILURE)

struct student {
    int id;
    char name[64];
    char birth[64];
    double avg_score;
};

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s <port> <file>\n", argv[0]);
        exf;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(atoi(argv[1]));

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

    printf("Waiting for client\n");

    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    // Wait for a client to connect
    int client = accept(listener, (struct sockaddr *)&client_addr, &client_addr_len);

    if (client < 0) {
        perror("accept() failed");
        exf;
    }

    printf("Client connected: %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

    struct tm *t;
    struct student stud;
    char datetime[36];
    FILE *f = fopen(argv[2], "wb");

    // Receive student data, log to file with IP, timestamp and data
    while(1) {
        int len = recv(client, &stud, sizeof(stud), 0);

        if (len <= 0) {
            printf("Disconnected\n");
            break;
        }

        time_t now = time(NULL);
        t = localtime(&now);

        strftime(datetime, sizeof(datetime), "%Y-%m-%d %H:%M:%S", t);


        fprintf(
            f,
            "%s %s %d %s %s %.2lf\n",
            inet_ntoa(client_addr.sin_addr),
            datetime,
            stud.id,
            stud.name,
            stud.birth,
            stud.avg_score
        );
        fflush(f);

        printf("Wrote %d bytes to %s\n", len, argv[2]);
    }

    fclose(f);
    close(listener);
    close(client);

    return 0;
}