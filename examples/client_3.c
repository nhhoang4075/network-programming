/**
 * @file client_3.c
 * @author Nguyen Huy Hoang (nhh4075@gmail.com)
 * @brief Client that receives data from httpbin.org
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
#include <netdb.h>

int main() {
    int client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (client < 0) {
        perror("socket() failed");
        exit(EXIT_FAILURE);
    }

    struct addrinfo *res;
    if (getaddrinfo("httpbin.org", "http", NULL, &res)) {
        perror("getaddrinfo() failed");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in addr;

    if (connect(client, res->ai_addr, res->ai_addrlen)) {
        perror("connect() failed");
        exit(EXIT_FAILURE);
    }

    char buf[2048];
    char msg[] = "GET /get HTTP/1.1\r\nHost: httpbin.org\r\n\r\n";
    write(client, msg, strlen(msg));

    int len = read(client, buf, sizeof(buf));

    buf[len] = '\0';
    printf("%s\n", buf);

    close(client);

    return 0;
}