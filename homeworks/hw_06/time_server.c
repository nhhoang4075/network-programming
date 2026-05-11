/**
 * @file time_server.c
 * @author Nguyen Huy Hoang (nhh4075@gmail.com)
 * @brief TCP time server using multiprocessing
 * @date 2026-05-11
 *
 * Usage: ./time_server <port>
 *
 * Protocol: client sends one line per request.
 *   Request:  GET_TIME <format>
 *   Formats:  dd/mm/yyyy | dd/mm/yy | mm/dd/yyyy | mm/dd/yy
 *   Reply:    formatted current date on success, or "ERROR: <reason>"
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

#define RESET   "\033[0m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"

#define exf exit(EXIT_FAILURE)
#define BACKLOG  32
#define BUF_SIZE 512

static const struct {
    const char *name;
    const char *strftime_fmt;
} FORMATS[] = {
    { "dd/mm/yyyy", "%d/%m/%Y" },
    { "dd/mm/yy",   "%d/%m/%y" },
    { "mm/dd/yyyy", "%m/%d/%Y" },
    { "mm/dd/yy",   "%m/%d/%y" },
};
#define NUM_FORMATS (sizeof(FORMATS) / sizeof(FORMATS[0]))

static const char *resolve_format(const char *name) {
    for (size_t i = 0; i < NUM_FORMATS; i++) {
        if (strcmp(name, FORMATS[i].name) == 0) {
            return FORMATS[i].strftime_fmt;
        }
    }
    return NULL;
}

static void handle_request(int client, const char *line) {
    char reply[BUF_SIZE];

    char cmd[16] = {0};
    char fmt[32] = {0};
    char extra[2] = {0};
    int n = sscanf(line, "%15s %31s %1s", cmd, fmt, extra);

    if (n < 1 || strcmp(cmd, "GET_TIME") != 0) {
        snprintf(
            reply, sizeof(reply),
            RED "ERROR: unknown command. Usage: GET_TIME <format>" RESET "\n"
        );
        write(client, reply, strlen(reply));
        return;
    }

    if (n != 2) {
        snprintf(
            reply, sizeof(reply),
            RED "ERROR: GET_TIME requires exactly one format argument." RESET "\n"
        );
        write(client, reply, strlen(reply));
        return;
    }

    const char *strf = resolve_format(fmt);
    if (!strf) {
        snprintf(
            reply, sizeof(reply),
            RED "ERROR: unsupported format '%s'. "
            "Supported: dd/mm/yyyy, dd/mm/yy, mm/dd/yyyy, mm/dd/yy" RESET "\n",
            fmt
        );
        write(client, reply, strlen(reply));
        return;
    }

    time_t now = time(NULL);
    struct tm tm_now;
    localtime_r(&now, &tm_now);

    char formatted[128];
    strftime(formatted, sizeof(formatted), strf, &tm_now);
    snprintf(reply, sizeof(reply), GREEN "%s" RESET "\n", formatted);
    write(client, reply, strlen(reply));
}

static void handle_client(int client, struct sockaddr_in cl_addr) {
    FILE *in = fdopen(client, "r");
    if (!in) {
        perror("fdopen() failed");
        return;
    }

    printf("[pid=%d] Client connected from %s:%d\n",
           getpid(),
           inet_ntoa(cl_addr.sin_addr),
           ntohs(cl_addr.sin_port));

    char buf[BUF_SIZE];
    while (fgets(buf, sizeof(buf), in)) {
        buf[strcspn(buf, "\r\n")] = '\0';
        if (buf[0] == '\0') continue;
        printf(GREEN "[pid=%d] Request: %s" RESET "\n", getpid(), buf);
        handle_request(client, buf);
    }

    printf("[pid=%d] Client %s:%d disconnected\n",
           getpid(),
           inet_ntoa(cl_addr.sin_addr),
           ntohs(cl_addr.sin_port));

    fclose(in);
}

static void sigchld_handler(int signo) {
    (void)signo;
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

int main(int argc, char *argv[]) {
    setvbuf(stdout, NULL, _IOLBF, 0);

    if (argc != 2) {
        printf("Usage: %s <port>\n", argv[0]);
        exf;
    }

    signal(SIGCHLD, sigchld_handler);

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

    printf("Time server listening on port %s\n", argv[1]);

    while (1) {
        struct sockaddr_in cl_addr;
        socklen_t cl_addr_len = sizeof(cl_addr);

        int client = accept(listener, (struct sockaddr *)&cl_addr, &cl_addr_len);
        if (client < 0) {
            if (errno == EINTR) continue;
            perror("accept() failed");
            continue;
        }

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork() failed");
            close(client);
        } else if (pid == 0) {
            close(listener);
            handle_client(client, cl_addr);
            exit(EXIT_SUCCESS);
        } else {
            close(client);
        }
    }

    close(listener);
    return 0;
}
