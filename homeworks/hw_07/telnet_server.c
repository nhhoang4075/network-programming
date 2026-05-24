/**
 * @file telnet_server.c
 * @author Nguyen Huy Hoang (nhh4075@gmail.com)
 * @brief Telnet-like TCP server with authentication and command execution using multithreading (pthread)
 * @date 2026-05-25
 *
 * Usage: ./telnet_server <port>
 *
 * Mỗi client được xử lý trong 1 thread riêng (pthread_create + pthread_detach).
 * Credentials file: users.txt  (mỗi dòng 1 cặp "username password")
 * Lệnh được thực thi qua system() và kết quả đọc từ out_<tid>.txt
 *
 * Build: gcc -o telnet_server telnet_server.c -pthread
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>

#define RESET   "\033[0m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"

#define exf         exit(EXIT_FAILURE)
#define BACKLOG     32
#define BUF_SIZE    2048
#define USER_SIZE   64

#define USERS_FILE  "users.txt"

typedef struct {
    int                  fd;
    struct sockaddr_in   addr;
} client_arg_t;

static int check_credentials(const char *user, const char *pass) {
    FILE *f = fopen(USERS_FILE, "r");
    if (!f) {
        perror("fopen() failed");
        return 0;
    }

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        char u[USER_SIZE], p[USER_SIZE];
        if (sscanf(line, "%63s %63s", u, p) == 2) {
            if (strcmp(u, user) == 0 && strcmp(p, pass) == 0) {
                fclose(f);
                return 1;
            }
        }
    }

    fclose(f);
    return 0;
}

static void send_file(int fd, const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) {
        const char *err = RED "Error: could not read command output\n" RESET;
        write(fd, err, strlen(err));
        return;
    }

    char buf[BUF_SIZE];
    int n;
    write(fd, GREEN, sizeof(GREEN));
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        write(fd, buf, n);
    }
    write(fd, RESET, sizeof(RESET));

    fclose(f);
}

static int read_line(FILE *in, char *buf, int size) {
    if (!fgets(buf, size, in)) return 0;
    buf[strcspn(buf, "\r\n")] = '\0';
    return 1;
}

static void *client_proc(void *arg) {
    client_arg_t *c = (client_arg_t *)arg;
    int client = c->fd;
    struct sockaddr_in cl_addr = c->addr;
    free(c);

    /* tid dùng làm tên file output để tránh đụng giữa các thread */
    unsigned long tid = (unsigned long)pthread_self();

    FILE *in = fdopen(client, "r");
    if (!in) {
        perror("fdopen() failed");
        close(client);
        return NULL;
    }

    printf(
        "[tid=%lu] New connection from %s:%d\n",
        tid,
        inet_ntoa(cl_addr.sin_addr),
        ntohs(cl_addr.sin_port)
    );

    char user[USER_SIZE];
    char buf[BUF_SIZE];

    while (1) {
        const char *uprompt = "Username: ";
        write(client, uprompt, strlen(uprompt));
        if (!read_line(in, buf, sizeof(buf))) {
            fclose(in);
            return NULL;
        }

        strncpy(user, buf, USER_SIZE - 1);
        user[USER_SIZE - 1] = '\0';

        const char *pprompt = "Password: ";
        write(client, pprompt, strlen(pprompt));
        if (!read_line(in, buf, sizeof(buf))) {
            fclose(in);
            return NULL;
        }

        if (check_credentials(user, buf)) {
            char welcome[128];
            snprintf(
                welcome, sizeof(welcome),
                "Login successful. Welcome, %s!\n> ",
                user
            );
            write(client, welcome, strlen(welcome));
            printf("[tid=%lu] User '%s' authenticated\n", tid, user);
            break;
        }

        const char *fail = YELLOW "Invalid credentials\n" RESET;
        write(client, fail, strlen(fail));
    }

    char out_path[64];
    snprintf(out_path, sizeof(out_path), "out_%lu.txt", tid);

    while (read_line(in, buf, sizeof(buf))) {
        if (strcmp(buf, "exit") == 0) {
            const char *bye = "Goodbye!\n";
            write(client, bye, strlen(bye));
            printf("[tid=%lu] User '%s' logged out\n", tid, user);
            break;
        }

        char cmd[BUF_SIZE + 64];
        snprintf(
            cmd, sizeof(cmd),
            "%s > %s 2>&1",
            buf, out_path
        );
        system(cmd);

        send_file(client, out_path);

        const char *prompt = "> ";
        write(client, prompt, strlen(prompt));

        printf(
            GREEN "[tid=%lu] User '%s' executed: %s" RESET "\n",
            tid, user, buf
        );
    }

    fclose(in);
    unlink(out_path);
    return NULL;
}

int main(int argc, char *argv[]) {
    setvbuf(stdout, NULL, _IOLBF, 0);

    if (argc != 2) {
        printf("Usage: %s <port>\n", argv[0]);
        exf;
    }

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
        "Telnet server listening on port %s (users: %s)\n",
        argv[1], USERS_FILE
    );

    while (1) {
        struct sockaddr_in cl_addr;
        socklen_t cl_addr_len = sizeof(cl_addr);

        int client = accept(listener, (struct sockaddr *)&cl_addr, &cl_addr_len);
        if (client < 0) {
            if (errno == EINTR) continue;
            perror("accept() failed");
            continue;
        }

        client_arg_t *arg = malloc(sizeof(client_arg_t));
        if (!arg) {
            perror("malloc() failed");
            close(client);
            continue;
        }
        arg->fd   = client;
        arg->addr = cl_addr;

        pthread_t tid;
        if (pthread_create(&tid, NULL, client_proc, arg) != 0) {
            perror("pthread_create() failed");
            free(arg);
            close(client);
            continue;
        }
        pthread_detach(tid);
    }

    close(listener);
    return 0;
}
