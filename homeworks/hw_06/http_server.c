/**
 * @file http_server.c
 * @author Nguyen Huy Hoang (nhh4075@gmail.com)
 * @brief Iterative HTTP server using the preforking multiprocessing technique
 * @date 2026-05-11
 *
 * Usage: ./http_server <port> [num_workers]
 *
 * A fixed-size pool of worker processes is created at startup. Each worker
 * independently calls accept() on the shared listening socket; the kernel
 * load-balances incoming connections across the pool.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>

#define RESET   "\033[0m"
#define GREEN   "\033[32m"

#define exf exit(EXIT_FAILURE)
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
    "<p>Served by preforking HTTP server.</p></body></html>"
    RESET;

static void worker_loop(int listener) {
    pid_t pid = getpid();
    char buf[BUF_SIZE];

    printf("[worker pid=%d] ready\n", pid);

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
                GREEN "[worker pid=%d] %s %s from %s:%d" RESET "\n",
                pid, method, path,
                inet_ntoa(cl_addr.sin_addr),
                ntohs(cl_addr.sin_port)
            );
        }

        send(client, HTTP_RESPONSE, sizeof(HTTP_RESPONSE) - 1, 0);
        close(client);
    }
}

static volatile sig_atomic_t shutting_down = 0;

static void sigterm_handler(int signo) {
    (void)signo;
    shutting_down = 1;
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

    printf("HTTP server listening on port %s with %d workers\n",
           argv[1], num_workers);

    pid_t *workers = malloc(sizeof(pid_t) * num_workers);
    if (!workers) {
        perror("malloc() failed");
        exf;
    }

    for (int i = 0; i < num_workers; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork() failed");
            exf;
        } else if (pid == 0) {
            free(workers);
            worker_loop(listener);
            exit(EXIT_SUCCESS);
        }
        workers[i] = pid;
    }

    close(listener);

    signal(SIGINT, sigterm_handler);
    signal(SIGTERM, sigterm_handler);

    while (!shutting_down) {
        int status;
        pid_t pid = wait(&status);
        if (pid < 0) {
            if (shutting_down) break;
            continue;
        }
        printf("[parent] worker %d exited with status %d\n", pid, status);
    }

    for (int i = 0; i < num_workers; i++) {
        kill(workers[i], SIGTERM);
    }
    while (wait(NULL) > 0);

    free(workers);
    printf("[parent] shutdown complete\n");
    return 0;
}
