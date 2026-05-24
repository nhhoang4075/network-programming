/**
 * @file chat_server.c
 * @author Nguyen Huy Hoang (nhh4075@gmail.com)
 * @brief 2-person pair chat server using multithreading (pthread)
 * @date 2026-05-25
 *
 * Usage: ./chat_server <port>
 *
 * Mỗi client kết nối tới sẽ được đưa vào hàng đợi. Khi đủ 2 client, server
 * ghép cặp 2 client đó và spawn 2 thread chuyển tiếp tin nhắn (mỗi thread
 * forward 1 chiều). Khi 1 trong 2 client ngắt kết nối, thread tương ứng
 *
 * Build: gcc -o chat_server chat_server.c -pthread
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
#define CYAN    "\033[36m"

#define exf         exit(EXIT_FAILURE)
#define BACKLOG     64
#define BUF_SIZE    2048

/*
 * Tại mỗi thời điểm tối đa có 1 client đứng chờ partner.
 * Biến toàn cục waiting_client lưu socket của client đó,
 * được bảo vệ bởi queue_mutex.
 */
static int waiting_client = -1;
static pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;

// self = socket mình đọc data từ. peer = socket gửi data sang.
typedef struct {
    int self;
    int peer;
} pair_t;

/* 
 * Thread chuyển tiếp 1 chiều: đọc từ self, gửi sang peer.
 * Khi self ngắt kết nối (recv <= 0), shutdown peer
 * để thread chiều ngược lại cũng nhận recv = 0
 * và thoát, cả 2 client được đóng.
 */
static void *forward_proc(void *arg) {
    pair_t *p = (pair_t *)arg;
    int self = p->self;
    int peer = p->peer;
    free(p); // giải phóng ngay, không cần nữa

    char buf[BUF_SIZE];
    while (1) {
        ssize_t len = recv(self, buf, sizeof(buf), 0);
        if (len <= 0) break;
        if (send(peer, buf, len, 0) <= 0) break;
    }

    shutdown(peer, SHUT_RDWR);
    close(self);

    printf(
        YELLOW "[-] Forwarder fd=%d -> fd=%d closed" RESET "\n",
        self, peer
    );
    return NULL;
}

/* 
 * Spawn 1 thread chuyển tiếp đã detach.
 * Trả về 0 nếu OK, -1 nếu lỗi.
 */
static int spawn_forwarder(int self, int peer) {
    pair_t *p = malloc(sizeof(pair_t));
    if (!p) return -1;
    p->self = self;
    p->peer = peer;

    pthread_t tid;
    if (pthread_create(&tid, NULL, forward_proc, p) != 0) {
        free(p);
        return -1;
    }
    pthread_detach(tid); // Tự dọn khi xong
    return 0;
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

    printf("Chat server listening on port %s\n", argv[1]);

    while (1) {
        struct sockaddr_in cl_addr;
        socklen_t cl_addr_len = sizeof(cl_addr);

        int client = accept(listener, (struct sockaddr *)&cl_addr, &cl_addr_len);
        if (client < 0) {
            if (errno == EINTR) continue;
            perror("accept() failed");
            continue;
        }

        printf(
            GREEN "[+] Client connected from %s:%d (fd=%d)" RESET "\n",
            inet_ntoa(cl_addr.sin_addr),
            ntohs(cl_addr.sin_port),
            client
        );

        /* Truy nhập biến toàn cục waiting_client, cần lock mutex */
        pthread_mutex_lock(&queue_mutex);

        if (waiting_client == -1) {
            // Chưa có ai đợi, mình vào hàng đợi, chờ partner đến.
            waiting_client = client;
            pthread_mutex_unlock(&queue_mutex);

            printf(
                YELLOW "[?] Client fd=%d is waiting for a partner..." RESET "\n",
                client
            );
        } else {
            // Đã có người chờ, ghép cặp ngay
            int peer = waiting_client;
            waiting_client = -1;
            pthread_mutex_unlock(&queue_mutex);

            printf(
                CYAN "[=] Paired: fd=%d <-> fd=%d" RESET "\n",
                peer, client
            );

            // Spawn 2 thread chuyển tiếp
            if (
                spawn_forwarder(peer, client) < 0 ||
                spawn_forwarder(client, peer) < 0
            ) {
                perror("pthread_create() failed");
                close(peer);
                close(client);
            }
        }
    }

    close(listener);
    return 0;
}
