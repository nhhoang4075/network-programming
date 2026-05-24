/**
 * @file file_server.c
 * @author Nguyen Huy Hoang (nhh4075@gmail.com)
 * @brief Simple file download server using multiprocessing (fork-per-client)
 * @date 2026-05-25
 *
 * Usage: ./file_server <port> <directory>
 *
 * Protocol:
 *   - Server -> Client (on connect):
 *       "OK N\r\n<name1>\r\n<name2>\r\n...\r\n"   (list N regular files in <directory>)
 *       hoặc "ERROR No files to download\r\n" rồi đóng kết nối nếu thư mục rỗng.
 *   - Client -> Server: tên file muốn tải (kết thúc bằng \r\n hoặc \n).
 *   - Server -> Client:
 *       "OK <size>\r\n" + nội dung file (nhị phân) rồi đóng kết nối.
 *       hoặc "ERROR File not found\r\n" nếu file không tồn tại — chờ tên khác.
 *
 * Mỗi client được xử lý trong một tiến trình con (fork). Tiến trình cha
 * dùng SIGCHLD handler + waitpid(WNOHANG) để dọn tiến trình zombie.
 *
 * Build: gcc -o file_server file_server.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

#define RESET   "\033[0m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"

#define exf         exit(EXIT_FAILURE)
#define BACKLOG     64
#define BUF_SIZE    2048
#define CHUNK_SIZE  4096

static int send_all(int fd, const void *buf, size_t len) {
    const char *p = (const char *)buf;
    size_t remaining = len;

    while(remaining > 0) {
        ssize_t sent = send(fd, p, remaining, 0);

        if (sent > 0) {
            p += sent;
            remaining -= sent;
        } else if (sent < 0) {
            if (errno == EINTR) continue;

            perror("send() failed");
            return -1;
        } else {
            return -1;
        }
    }

    return 0;
}

static int send_file_list(int client, const char *dir) {
    struct dirent **list;
    int total = scandir(dir, &list, NULL, alphasort);

    if (total < 0) {
        perror("scandir() failed");
        return -1;
    }

    const char *names[total > 0 ? total : 1];
    int n = 0;
    for (int i = 0; i < total; ++i) {
        if (
            (strcmp(list[i]->d_name, ".") == 0) ||
            (strcmp(list[i]->d_name, "..") == 0)
        ) {
            continue;
        }

        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", dir, list[i]->d_name);

        struct stat st;
        if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) {
            names[n] = list[i]->d_name;
            n++;
        }
    }

    char buf[BUF_SIZE];
    if (n == 0) {
        const char *msg = "ERROR No files to download\r\n";
        send_all(client, msg, strlen(msg));
    } else {
        int len = snprintf(buf, sizeof(buf), "OK %d\r\n", n);
        send_all(client, buf, len);

        for (int i = 0; i < n; ++i) {
            len = snprintf(buf, sizeof(buf), "%s\r\n", names[i]);
            send_all(client, buf, len);
        }
        send_all(client, "\r\n", 2);
    }
    
    for (int i = 0; i < total; ++i) {
        free(list[i]);
    }
    free(list);

    return n;
}

static int send_file(int client, const char *dir, const char *name) {
    if (
        name[0] == '\0' ||
        strchr(name, '/') ||
        strstr(name, "..")
    ) {
        const char *msg = "ERROR File not found\r\n";
        send_all(client, msg, strlen(msg));
        return 0;
    }

    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", dir, name);

    struct stat st;
    if (stat(path, &st) < 0 || !S_ISREG(st.st_mode)) {
        const char *msg = "ERROR File not found\r\n";
        send_all(client, msg, strlen(msg));
        return 0;
    }

    FILE *fp = fopen(path, "rb");
    if (!fp) {
        const char *msg = "ERROR File not found\r\n";
        send_all(client, msg, strlen(msg));
        return 0;
    }

    char header[64];
    int hlen = snprintf(
        header, sizeof(header),
        "OK %lld\r\n", (long long)st.st_size
    );

    if (send_all(client, header, hlen) < 0) {
        fclose(fp);
        return 1;
    }

    char chunk[CHUNK_SIZE];
    size_t nread;
    while ((nread = fread(chunk, 1, sizeof(chunk), fp)) > 0) {
        if (send_all(client, chunk, nread) < 0) {
            fclose(fp);
            return 1;
        }
    }

    fclose(fp);
    printf(
        GREEN "[pid=%d] Sent file: %s (%lld bytes)" RESET "\n",
        getpid(), name, (long long)st.st_size
    );
    return 1;
}

static void handle_client(int client, struct sockaddr_in addr, const char *dir) {
    printf(
        GREEN "[pid=%d] Client connected from %s:%d" RESET "\n",
        getpid(),
        inet_ntoa(addr.sin_addr),
        ntohs(addr.sin_port)
    );

    int n = send_file_list(client, dir);
    if (n <= 0) {
        close(client);
        return;
    }

    char buf[BUF_SIZE];
    while (1) {
        ssize_t len = recv(client, buf, sizeof(buf) - 1, 0);
        if (len <= 0) break;

        buf[len] = '\0';
        buf[strcspn(buf, "\r\n")] = '\0';
        if (buf[0] == '\0') continue;

        if (send_file(client, dir, buf) == 1) break;
    }

    printf(YELLOW "[pid=%d] Client disconnected" RESET "\n", getpid());
    close(client);
}

static void sigchld_handler(int signo) {
    (void)signo;
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s <port> <directory>\n", argv[0]);
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

    printf("File server listening on port %s\n", argv[1]);
    printf("File directory path: %s\n", argv[2]);

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
            handle_client(client, cl_addr, argv[2]);
            exit(EXIT_SUCCESS);
        } else {
            close(client);
        }
    }

    close(listener);
    return 0;
}


