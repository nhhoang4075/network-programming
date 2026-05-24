/**
 * @file multi_chat_server.c
 * @author Nguyen Huy Hoang (nhh4075@gmail.com)
 * @brief Multi-client TCP chat server using multithreading (pthread)
 * @date 2026-05-25
 *
 * Usage: ./multi_chat_server <port>
 *
 * Registration: client phải gửi "client_id: client_name" (cả 2 không có khoảng trắng).
 * Sau khi đăng ký, mọi tin nhắn được broadcast cho các client khác dưới dạng:
 *   "YYYY/MM/DD HH:MM:SSam/pm <id>: <message>"
 *
 * Build: gcc -o multi_chat_server multi_chat_server.c -pthread
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
#include <time.h>

#define RESET   "\033[0m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define CYAN    "\033[36m"

#define exf         exit(EXIT_FAILURE)
#define MAX_CLIENTS 64
#define BUF_SIZE    2048
#define ID_SIZE     64
#define NAME_SIZE   128

typedef enum { REGISTERING, ACTIVE } ClientState;

typedef struct {
    int          fd;
    ClientState  state;
    char         id[ID_SIZE];
    char         name[NAME_SIZE];
} Client;

/* State chia sẻ giữa các thread — mọi truy nhập phải lock clients_mutex */
static Client            clients[MAX_CLIENTS];
static int               num_clients = 0;
static pthread_mutex_t   clients_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Các hàm _locked: caller PHẢI giữ clients_mutex trước khi gọi */

static void broadcast_locked(int sender_fd, const char *msg) {
    size_t len = strlen(msg);
    for (int i = 0; i < num_clients; ++i) {
        if (clients[i].fd != sender_fd && clients[i].state == ACTIVE) {
            send(clients[i].fd, msg, len, 0);
        }
    }
}

static void remove_client_locked(int fd) {
    for (int i = 0; i < num_clients; ++i) {
        if (clients[i].fd == fd) {
            clients[i] = clients[--num_clients];
            return;
        }
    }
}

static int find_client_locked(int fd, char *id_out, ClientState *state_out) {
    for (int i = 0; i < num_clients; ++i) {
        if (clients[i].fd == fd) {
            if (id_out) {
                strncpy(id_out, clients[i].id, ID_SIZE);
            }
            if (state_out) {
                *state_out = clients[i].state;
            }
            return 1;
        }
    }
    return 0;
}

/* Parse và áp dụng đăng ký "id: name". Trả về 1 nếu hợp lệ. */
static int try_register(int fd, const char *buf) {
    const char *sep = strstr(buf, ": ");
    if (!sep || sep == buf) return 0;

    int id_len = sep - buf;
    const char *name_start = sep + 2;

    if (id_len <= 0 || id_len >= ID_SIZE) return 0;
    if (strchr(buf, ' ') && strchr(buf, ' ') < sep) return 0;
    int name_len = strlen(name_start);
    if (name_len == 0 || strchr(name_start, ' ')) return 0;

    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < num_clients; ++i) {
        if (clients[i].fd == fd) {
            strncpy(clients[i].id, buf, id_len);
            clients[i].id[id_len] = '\0';
            strncpy(clients[i].name, name_start, NAME_SIZE - 1);
            clients[i].name[NAME_SIZE - 1] = '\0';
            clients[i].state = ACTIVE;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
    return 1;
}

static void make_timestamp(char *out, size_t out_size) {
    time_t now = time(NULL);
    struct tm tm_now;
    localtime_r(&now, &tm_now);
    strftime(out, out_size, "%Y/%m/%d %I:%M:%S%p", &tm_now);
}

static void *client_proc(void *arg) {
    int fd = *(int *)arg;
    free(arg);

    const char *prompt = "Enter your ID and name (format: client_id: client_name):\n";
    send(fd, prompt, strlen(prompt), 0);

    char buf[BUF_SIZE];

    while (1) {
        ssize_t len = recv(fd, buf, sizeof(buf) - 1, 0);
        if (len <= 0) break;
        buf[len] = '\0';
        buf[strcspn(buf, "\r\n")] = '\0';
        if (buf[0] == '\0') continue;

        ClientState state;
        char my_id[ID_SIZE] = {0};
        pthread_mutex_lock(&clients_mutex);
        find_client_locked(fd, my_id, &state);
        pthread_mutex_unlock(&clients_mutex);

        if (state == REGISTERING) {
            if (try_register(fd, buf)) {
                pthread_mutex_lock(&clients_mutex);
                find_client_locked(fd, my_id, NULL);

                char welcome[BUF_SIZE];
                snprintf(
                    welcome, sizeof(welcome),
                    "Welcome, %s! You can start chatting now.\n",
                    my_id
                );
                send(fd, welcome, strlen(welcome), 0);

                char notice[BUF_SIZE];
                snprintf(
                    notice, sizeof(notice),
                    GREEN "%s has joined the chat" RESET "\n", 
                    my_id
                );
                broadcast_locked(fd, notice);
                pthread_mutex_unlock(&clients_mutex);

                printf(
                    GREEN "[+] Registered: id=%s (fd=%d)" RESET "\n",
                    my_id, fd
                );
            } else {
                const char *retry = RED "Invalid format. Use: client_id: client_name" RESET "\n";
                send(fd, retry, strlen(retry), 0);
            }
        } else {
            char ts[32];
            make_timestamp(ts, sizeof(ts));

            char msg[BUF_SIZE + 256];
            snprintf(
                msg, sizeof(msg),
                "%s %s: %s\n",
                ts, my_id, buf
            );

            pthread_mutex_lock(&clients_mutex);
            broadcast_locked(fd, msg);
            pthread_mutex_unlock(&clients_mutex);

            printf(CYAN "%s" RESET, msg);
        }
    }

    char my_id[ID_SIZE] = {0};
    ClientState state = REGISTERING;
    pthread_mutex_lock(&clients_mutex);
    find_client_locked(fd, my_id, &state);
    if (state == ACTIVE) {
        char notice[BUF_SIZE];
        snprintf(notice, sizeof(notice),
                 "*** %s has left the chat ***\n", my_id);
        broadcast_locked(fd, notice);
    }
    remove_client_locked(fd);
    pthread_mutex_unlock(&clients_mutex);

    printf(YELLOW "[-] Client fd=%d disconnected (id=%s)" RESET "\n",
           fd, my_id[0] ? my_id : "<unregistered>");
    close(fd);
    return NULL;
}

int main(int argc, char *argv[]) {
    setvbuf(stdout, NULL, _IOLBF, 0);

    if (argc != 2) {
        printf("Usage: %s <port>\n", argv[0]);
        exf;
    }

    int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener < 0) { perror("socket() failed"); exf; }

    int opt = 1;
    if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt() failed"); exf;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(atoi(argv[1]));

    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind() failed"); exf;
    }

    if (listen(listener, MAX_CLIENTS) < 0) {
        perror("listen() failed"); exf;
    }

    printf("Multi-chat server listening on port %s\n", argv[1]);

    while (1) {
        struct sockaddr_in cl_addr;
        socklen_t cl_addr_len = sizeof(cl_addr);

        int client = accept(listener, (struct sockaddr *)&cl_addr, &cl_addr_len);
        if (client < 0) {
            if (errno == EINTR) continue;
            perror("accept() failed");
            continue;
        }

        pthread_mutex_lock(&clients_mutex);
        if (num_clients >= MAX_CLIENTS) {
            pthread_mutex_unlock(&clients_mutex);
            const char *full = "Server full. Try again later.\n";
            send(client, full, strlen(full), 0);
            close(client);
            continue;
        }
        clients[num_clients].fd    = client;
        clients[num_clients].state = REGISTERING;
        clients[num_clients].id[0]   = '\0';
        clients[num_clients].name[0] = '\0';
        num_clients++;
        pthread_mutex_unlock(&clients_mutex);

        printf(GREEN "[+] New connection from %s:%d (fd=%d)" RESET "\n",
               inet_ntoa(cl_addr.sin_addr),
               ntohs(cl_addr.sin_port),
               client);

        int *arg = malloc(sizeof(int));
        if (!arg) { 
            perror("malloc() failed");
            close(client);
            continue;
        }
        *arg = client;

        pthread_t tid;
        if (pthread_create(&tid, NULL, client_proc, arg) != 0) {
            perror("pthread_create() failed");
            free(arg);
            pthread_mutex_lock(&clients_mutex);
            remove_client_locked(client);
            pthread_mutex_unlock(&clients_mutex);
            close(client);
            continue;
        }
        pthread_detach(tid);
    }

    close(listener);
    return 0;
}
