/**
 * @file client-4.c
 * @author Nguyen Huy Hoang (nhh4075@gmail.com)
 * @brief Client send struct data
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

struct student {
    int id;
    char name[64];
    int age;
};

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

    struct student stud;

    while (1) {
        printf("Enter id: ");
        scanf(" %d", &stud.id);

        printf("Enter name: ");
        while (getchar() != '\n');
        fgets(stud.name, sizeof(stud.name), stdin);
        stud.name[strcspn(stud.name, "\n")] = '\0';

        printf("Enter age: ");
        scanf(" %d", &stud.age);

        send(client, &stud, sizeof(stud), 0);

        if (stud.age == 0) {
            break;
        }
    }

    close(client);

    return 0;
}
