/**
 * @file http_server.c
 * @author Nguyen Huy Hoang (nhh4075@gmail.com)
 * @brief Iterative HTTP server using the prethreading technique
 * @date 2026-05-25
 *
 * Usage: ./http_server <port> [num_workers]
 *
 * Một pool cố định các thread được tạo lúc khởi động. Mỗi thread tự gọi
 * accept() trên cùng 1 listening socket; kernel tự cân bằng kết nối đến
 * giữa các thread (giống preforking nhưng dùng pthread thay vì fork).
 *
 * Build: gcc -o http_server http_server.c -pthread
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#define RESET   "\033[0m"
#define GREEN   "\033[32m"

#define exf             exit(EXIT_FAILURE)
#define BACKLOG         64
#define BUF_SIZE        4096
#define DEFAULT_WORKERS 4

static const char HTTP_RESPONSE[] =
    GREEN
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/html; charset=utf-8\r\n"
    "Connection: close\r\n"
    "\r\n"
    "<html><body><h1>Hello world</h1>"
    "<p>Served by prethreading HTTP server.</p></body></html>"
    RESET;

static void *worker_loop(void *arg) {
    int listener = *(int *)arg;
    unsigned long tid = (unsigned long)pthread_self();
    char buf[BUF_SIZE];

    printf("[worker tid=%lu] ready\n", tid);

    while (1) {
        struct sockaddr_in cl_addr;
        socklen_t cl_addr_len = sizeof(cl_addr);

        int client = accept(listener, (struct sockaddr *)&cl_addr, &cl_addr_len);
        if (client < 0) {
            perror("accept() failed");
            continue;
        }

        int len = read(client, buf, sizeof(buf) - 1);
        if (len > 0) {
            buf[len] = '\0';
            char method[8] = {0}, path[256] = {0};
            sscanf(buf, "%7s %255s", method, path);
            printf(
                GREEN "[worker tid=%lu] %s %s from %s:%d" RESET "\n",
                tid, method, path,
                inet_ntoa(cl_addr.sin_addr),
                ntohs(cl_addr.sin_port)
            );
        }

        send(client, HTTP_RESPONSE, sizeof(HTTP_RESPONSE) - 1, 0);
        close(client);
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    setvbuf(stdout, NULL, _IOLBF, 0);

    if (argc < 2 || argc > 3) {
        printf("Usage: %s <port> [num_workers]\n", argv[0]);
        exf;
    }

    int num_workers = (argc == 3) ? atoi(argv[2]) : DEFAULT_WORKERS;
    if (num_workers <= 0) num_workers = DEFAULT_WORKERS;

    int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener < 0) {
        perror("socket() failed");
        exf;
    }

    int opt = 1;
    if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt() failed");
        exf;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(atoi(argv[1]));

    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind() failed");
        exf;
    }

    if (listen(listener, BACKLOG) < 0) {
        perror("listen() failed");
        exf;
    }

    printf(
        "HTTP server listening on port %s with %d workers\n",
        argv[1], num_workers
    );

    pthread_t *workers = malloc(sizeof(pthread_t) * num_workers);
    if (!workers) {
        perror("malloc() failed");
        exf;
    }

    for (int i = 0; i < num_workers; i++) {
        if (
            pthread_create(
                &workers[i], NULL,
                worker_loop, &listener
            ) != 0
        ) {
            perror("pthread_create() failed");
            exf;
        }
    }

    // main thread chờ vô hạn — các worker đã join với listener
    for (int i = 0; i < num_workers; i++) {
        pthread_join(workers[i], NULL);
    }

    free(workers);
    close(listener);
    return 0;
}
