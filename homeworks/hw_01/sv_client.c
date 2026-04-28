/**
 * @file sv_client.c
 * @author Nguyen Huy Hoang (nhh4075@gmail.com)
 * @brief Client that sends struct data of student
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

struct student {
    int id;
    char name[64];
    char birth[64];
    double avg_score;
};

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

    // Connect to server at given IP and port
    struct sockaddr_in sv_addr;
    sv_addr.sin_family = AF_INET;
    sv_addr.sin_addr.s_addr = inet_addr(argv[1]);
    sv_addr.sin_port = htons(atoi(argv[2]));

    if (connect(client, (struct sockaddr *)&sv_addr, sizeof(sv_addr)) < 0) {
        perror("connect() failed");
        exf;
    }

    struct student stud;

    // Input student data and send to server, enter id=0 to exit
    while(1) {
        printf("Enter id: ");
        scanf(" %d", &stud.id);

        if (stud.id == 0) {
            break;
        }

        printf("Enter name: ");
        while (getchar() != '\n');
        fgets(stud.name, sizeof(stud.name), stdin);
        stud.name[strcspn(stud.name, "\n")] = '\0';

        printf("Enter birthday: ");
        scanf(" %s", stud.birth);

        printf("Enter average score: ");
        scanf(" %lf", &stud.avg_score);

        int len = send(client, &stud, sizeof(stud), 0);

        printf("Sent %d bytes to server\n", len);
    }

    close(client);

    return 0;
}