#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netdb.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <hostname>\n", argv[0]);
        exit(1);
    }

    struct addrinfo *res, *p;

    int ret = getaddrinfo(argv[1], "http", NULL, &res);
    if (ret != 0) {
        printf("getaddrinfo() failed\n");
        exit(1);
    }

    p = res;
    while (p != NULL) {
        if (p->ai_family == AF_INET) {
            printf("IPv4\n");
            printf("Sock type: %d\n", p->ai_socktype);
            printf("Protocol: %d\n", p->ai_protocol);
            struct sockaddr_in addr;
            memcpy(&addr, p->ai_addr, p->ai_addrlen);
            printf("IP: %s\n", inet_ntoa(addr.sin_addr));
        } else if (p->ai_family == AF_INET6) {
            printf("IPv6\n");
            printf("Sock type: %d\n", p->ai_socktype);
            printf("Protocol: %d\n", p->ai_protocol);
            struct sockaddr_in6 addr;
            char buf[INET6_ADDRSTRLEN];
            memcpy(&addr, p->ai_addr, p->ai_addrlen);
            inet_ntop(AF_INET6, &addr.sin6_addr, buf, sizeof(buf));
            printf("IP: %s\n", buf);
        }
        p = p->ai_next;
    }

    freeaddrinfo(res);

    return 0;
}
